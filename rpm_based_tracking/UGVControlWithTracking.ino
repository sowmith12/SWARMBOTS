#include <WiFi.h>
#include <WebServer.h>

// Wi-Fi credentials
const char* ssid = "apple";
const char* password = "9966809332";

// Motor pins (L293D)
#define MOTOR_A_IN1 14
#define MOTOR_A_IN2 27
#define MOTOR_A_ENA 33
#define MOTOR_B_IN1 26
#define MOTOR_B_IN2 25
#define MOTOR_B_ENB 32

WebServer server(80);

// Calibration values
float rpm = 0;
float wheel_circumference = 0;
float wheel_base_cm = 14.0;  // Distance between wheels (in cm)

// Position tracking variables
float current_x = 0.0;
float current_y = 0.0;
float current_angle = 0.0;  // Current heading in degrees (0 = facing +X direction)

// Motor state tracking
unsigned long motor_start_time = 0;
String current_motor_state = "stop";
bool is_moving = false;

void setup() {
  Serial.begin(115200);
  
  pinMode(MOTOR_A_IN1, OUTPUT);
  pinMode(MOTOR_A_IN2, OUTPUT);
  pinMode(MOTOR_A_ENA, OUTPUT);
  pinMode(MOTOR_B_IN1, OUTPUT);
  pinMode(MOTOR_B_IN2, OUTPUT);
  pinMode(MOTOR_B_ENB, OUTPUT);
  stopMotors();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  server.on("/", handleRoot);
  server.on("/control", handleControl);
  server.on("/position", handlePosition);
  server.on("/reset_position", handleResetPosition);

  server.on("/test", []() {
    moveForward(); delay(2000); stopMotors();
    server.send(200, "text/plain", "Test complete.");
  });

  server.on("/calculate", HTTP_POST, []() {
    if (server.hasArg("distance") && server.hasArg("circumference")) {
      float d = server.arg("distance").toFloat();
      wheel_circumference = server.arg("circumference").toFloat();
      rpm = (d / wheel_circumference) * 30.0;
      Serial.printf("Calculated RPM = %.2f\n", rpm);
      server.send(200, "text/plain", "RPM = " + String(rpm));
    } else {
      server.send(400, "text/plain", "Missing distance or circumference.");
    }
  });

  server.on("/goto", HTTP_POST, []() {
    if (!server.hasArg("x") || !server.hasArg("y")) {
      server.send(400, "text/plain", "Missing x or y");
      return;
    }
    if (rpm <= 0 || wheel_circumference <= 0) {
      server.send(400, "text/plain", "Please calculate RPM first.");
      return;
    }

    float target_x = server.arg("x").toFloat();
    float target_y = server.arg("y").toFloat();

    // Calculate relative movement needed
    float dx = target_x - current_x;
    float dy = target_y - current_y;

    // Move in X direction first
    if (abs(dx) > 0.5) {  // Only move if significant distance
      float time_x_sec = (abs(dx) / wheel_circumference) * 60.0 / rpm;
      int time_x_ms = (int)(time_x_sec * 1000);
      
      if (dx > 0) {
        moveForward(); delay(time_x_ms); stopMotors(); delay(150);
      } else {
        moveBackward(); delay(time_x_ms); stopMotors(); delay(150);
      }
    }

    // Turn 90 degrees
    float turn_arc_cm = 3.1416 * wheel_base_cm / 4.0;
    float turn_time_sec = (turn_arc_cm / wheel_circumference) * 60.0 / rpm;
    float correction_factor = 1.5;
    int turn_time_ms = (int)(turn_time_sec * correction_factor * 1000);

    turnRight(); delay(turn_time_ms); stopMotors(); delay(150);

    // Move in Y direction
    if (abs(dy) > 0.5) {  // Only move if significant distance
      float time_y_sec = (abs(dy) / wheel_circumference) * 60.0 / rpm;
      int time_y_ms = (int)(time_y_sec * 1000);
      
      if (dy > 0) {
        moveForward(); delay(time_y_ms); stopMotors(); delay(150);
      } else {
        moveBackward(); delay(time_y_ms); stopMotors(); delay(150);
      }
    }

    // Update position to target (approximate)
    current_x = target_x;
    current_y = target_y;
    current_angle = 90.0;  // Facing +Y after the sequence

    server.send(200, "text/plain", "UGV moved to approx (x,y)");
  });

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
  updatePosition();  // Continuously update position based on motor state
}

// Update position based on current motor state and time
void updatePosition() {
  if (is_moving && rpm > 0 && wheel_circumference > 0) {
    unsigned long current_time = millis();
    float elapsed_time_sec = (current_time - motor_start_time) / 1000.0;
    
    // Calculate distance moved
    float distance_per_second = (rpm * wheel_circumference) / 60.0;  // cm/s
    float distance_moved = distance_per_second * elapsed_time_sec;
    
    // Update position based on current state and angle
    if (current_motor_state == "forward") {
      current_x += distance_moved * cos(current_angle * PI / 180.0);
      current_y += distance_moved * sin(current_angle * PI / 180.0);
    } else if (current_motor_state == "backward") {
      current_x -= distance_moved * cos(current_angle * PI / 180.0);
      current_y -= distance_moved * sin(current_angle * PI / 180.0);
    } else if (current_motor_state == "left") {
      // Calculate angle change for turning
      float turn_rate = distance_per_second / (wheel_base_cm / 2.0);  // rad/s
      current_angle -= turn_rate * elapsed_time_sec * 180.0 / PI;  // Convert to degrees
      if (current_angle < 0) current_angle += 360.0;
    } else if (current_motor_state == "right") {
      float turn_rate = distance_per_second / (wheel_base_cm / 2.0);  // rad/s
      current_angle += turn_rate * elapsed_time_sec * 180.0 / PI;
      if (current_angle >= 360.0) current_angle -= 360.0;
    }
    
    // Reset timer for next calculation
    motor_start_time = current_time;
  }
}

// === MOTOR FUNCTIONS (Modified to track state) ===

void moveForward() {
  updatePosition();  // Update position before changing state
  stopMotors(); delay(50);
  digitalWrite(MOTOR_A_IN1, HIGH); digitalWrite(MOTOR_A_IN2, LOW);
  digitalWrite(MOTOR_B_IN1, HIGH); digitalWrite(MOTOR_B_IN2, LOW);
  analogWrite(MOTOR_A_ENA, 255); analogWrite(MOTOR_B_ENB, 255);
  
  current_motor_state = "forward";
  is_moving = true;
  motor_start_time = millis();
}

void moveBackward() {
  updatePosition();
  stopMotors(); delay(50);
  digitalWrite(MOTOR_A_IN1, LOW); digitalWrite(MOTOR_A_IN2, HIGH);
  digitalWrite(MOTOR_B_IN1, LOW); digitalWrite(MOTOR_B_IN2, HIGH);
  analogWrite(MOTOR_A_ENA, 255); analogWrite(MOTOR_B_ENB, 255);
  
  current_motor_state = "backward";
  is_moving = true;
  motor_start_time = millis();
}

void turnRight() {
  updatePosition();
  digitalWrite(MOTOR_A_IN1, HIGH); digitalWrite(MOTOR_A_IN2, LOW);
  digitalWrite(MOTOR_B_IN1, LOW); digitalWrite(MOTOR_B_IN2, HIGH);
  analogWrite(MOTOR_A_ENA, 150); analogWrite(MOTOR_B_ENB, 250);
  
  current_motor_state = "right";
  is_moving = true;
  motor_start_time = millis();
}

void turnLeft() {
  updatePosition();
  digitalWrite(MOTOR_A_IN1, LOW); digitalWrite(MOTOR_A_IN2, HIGH);
  digitalWrite(MOTOR_B_IN1, HIGH); digitalWrite(MOTOR_B_IN2, LOW);
  analogWrite(MOTOR_A_ENA, 150); analogWrite(MOTOR_B_ENB, 250);
  
  current_motor_state = "left";
  is_moving = true;
  motor_start_time = millis();
}

void stopMotors() {
  updatePosition();  // Final position update before stopping
  digitalWrite(MOTOR_A_IN1, LOW); digitalWrite(MOTOR_A_IN2, LOW); analogWrite(MOTOR_A_ENA, 0);
  digitalWrite(MOTOR_B_IN1, LOW); digitalWrite(MOTOR_B_IN2, LOW); analogWrite(MOTOR_B_ENB, 0);
  
  current_motor_state = "stop";
  is_moving = false;
}

// === NEW API ENDPOINTS ===

void handlePosition() {
  updatePosition();  // Ensure position is current
  String json = "{";
  json += "\"x\":" + String(current_x, 2) + ",";
  json += "\"y\":" + String(current_y, 2) + ",";
  json += "\"angle\":" + String(current_angle, 1) + ",";
  json += "\"state\":\"" + current_motor_state + "\",";
  json += "\"rpm\":" + String(rpm, 2) + ",";
  json += "\"circumference\":" + String(wheel_circumference, 2);
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleResetPosition() {
  current_x = 0.0;
  current_y = 0.0;
  current_angle = 0.0;
  server.send(200, "text/plain", "Position reset to (0,0)");
}

// === HTML WEB UI (Enhanced with Position Display) ===

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>UGV Control with Position Tracking</title>
  <style>
    body { background: black; color: white; font-family: sans-serif; margin: 20px; }
    .container { display: flex; gap: 20px; }
    .controls { flex: 1; text-align: center; }
    .position-box { 
      flex: 1; 
      border: 2px solid white; 
      padding: 20px; 
      border-radius: 10px;
      background: #333;
      min-height: 200px;
    }
    input, button { padding: 8px; margin: 5px; font-size: 16px; border-radius: 5px; border: none; }
    button { background: white; color: black; cursor: pointer; }
    button:hover { background: #ccc; }
    .position-display { font-size: 18px; margin: 10px 0; }
    .position-grid { 
      width: 200px; 
      height: 200px; 
      border: 1px solid white; 
      margin: 20px auto; 
      position: relative;
      background: linear-gradient(90deg, rgba(255,255,255,0.1) 1px, transparent 1px),
                  linear-gradient(rgba(255,255,255,0.1) 1px, transparent 1px);
      background-size: 20px 20px;
    }
    .ugv-dot { 
      width: 8px; 
      height: 8px; 
      background: #ff4444; 
      border-radius: 50%; 
      position: absolute;
      transform: translate(-50%, -50%);
      box-shadow: 0 0 10px #ff4444;
      animation: pulse 2s infinite;
    }
    @keyframes pulse {
      0% { box-shadow: 0 0 5px #ff4444; }
      50% { box-shadow: 0 0 15px #ff4444, 0 0 20px #ff4444; }
      100% { box-shadow: 0 0 5px #ff4444; }
    }
    .reset-btn { background: #ff6b6b !important; color: white !important; }
  </style>
</head>
<body>
  <h1>UGV Control with Real-time Position Tracking</h1>
  
  <div class="container">
    <div class="controls">
      <h2>Manual-covering Controls</h2>
      <button onmousedown="send('forward')" onmouseup="send('stop')">Forward</button><br>
      <button onmousedown="send('left')" onmouseup="send('stop')">Left</button>
      <button onclick="send('stop')">Stop</button>
      <button onmousedown="send('right')" onmouseup="send('stop')">Right</button><br>
      <button onmousedown="send('backward')" onmouseup="send('stop')">Backward</button>

      <h3>Test Run</h3>
      <button onclick="fetch('/test').then(r => r.text()).then(alert)">Run 2s Forward</button>

      <h3>Step 1: Calculate RPM</h3>
      <div>
        Distance in 2s (cm):<br><input id="distance" required><br>
        Wheel Circumference (cm):<br><input id="circumference" required><br>
        <button onclick="calculateRPM()">Calculate RPM</button>
      </div>

      <h3>Step 2: Go to (x, y)</h3>
      <div>
        X (cm):<br><input id="target-x" required><br>
        Y (cm):<br><input id="target-y" required><br>
        <button onclick="gotoPosition()">Move</button>
      </div>
    </div>

    <div class="position-box">
      <h2>Current Position</h2>
      <div class="position-display">
        <div>X: <span id="pos-x">0.00</span> cm</div>
        <div>Y: <span id="pos-y">0.00</span> cm</div>
        <div>Angle: <span id="pos-angle">0.0</span>°</div>
        <div>State: <span id="motor-state">stop</span></div>
        <div>RPM: <span id="rpm-value">0.00</span></div>
      </div>
      
      <div class="position-grid" id="position-grid">
        <svg id="path-svg" width="200" height="200" style="position: absolute; top: 0; left: 0; pointer-events: none;">
          <polyline id="path-line" fill="none" stroke="#00ff00" stroke-width="2" points=""></polyline>
        </svg>
        <div class="ugv-dot" id="ugv-dot"></div>
      </div>
      
      <button class="reset-btn" onclick="resetPosition()">Reset Position</button>
      <button onclick="clearPath()" style="background: #ffa500 !important; color: white !important;">Clear Path</button>
      <div style="color: #4CAF50; margin-top: 10px;">● Live Tracking Active</div>
      
      <!-- Status/Message Area -->
      <div id="status-area" style="margin-top: 20px; padding: 10px; background: #444; border-radius: 5px; min-height: 60px;">
        <h4 style="margin: 0 0 10px 0; color: #4CAF50;">System Status:</h4>
        <div id="status-messages" style="font-size: 14px; max-height: 100px; overflow-y: auto;">
          Ready for commands...
        </div>
      </div>
    </div>
  </div>

  <script>
    // Auto-start real-time tracking
    let trackingInterval = setInterval(updatePosition, 1000); // Update every 1 second
    let pathPoints = []; // Store path points for drawing
    let lastPosition = {x: 0, y: 0}; // Track last position for path drawing

    function send(cmd) {
      fetch('/control?cmd=' + cmd)
        .then(res => res.json())
        .then(data => console.log(data))
        .catch(() => alert("Command failed"));
    }

    function addStatusMessage(message, type = 'info') {
      const statusDiv = document.getElementById('status-messages');
      const timestamp = new Date().toLocaleTimeString();
      const color = type === 'success' ? '#4CAF50' : type === 'error' ? '#ff4444' : '#ffffff';
      
      statusDiv.innerHTML += `<div style="color: ${color}; margin: 2px 0;">[${timestamp}] ${message}</div>`;
      statusDiv.scrollTop = statusDiv.scrollHeight; // Auto-scroll to bottom
    }

    function calculateRPM() {
      const distance = document.getElementById('distance').value;
      const circumference = document.getElementById('circumference').value;
      
      if (!distance || !circumference) {
        addStatusMessage('Please enter both distance and circumference values', 'error');
        return;
      }

      const formData = new FormData();
      formData.append('distance', distance);
      formData.append('circumference', circumference);

      fetch('/calculate', {
        method: 'POST',
        body: formData
      })
      .then(res => res.text())
      .then(data => {
        addStatusMessage(`RPM Calculated: ${data}`, 'success');
        updatePosition(); // Refresh to show new RPM
      })
      .catch(err => addStatusMessage('RPM calculation failed', 'error'));
    }

    function gotoPosition() {
      const x = document.getElementById('target-x').value;
      const y = document.getElementById('target-y').value;
      
      if (!x || !y) {
        addStatusMessage('Please enter both X and Y coordinates', 'error');
        return;
      }

      addStatusMessage(`Moving to position (${x}, ${y})...`, 'info');

      const formData = new FormData();
      formData.append('x', x);
      formData.append('y', y);

      fetch('/goto', {
        method: 'POST',
        body: formData
      })
      .then(res => res.text())
      .then(data => {
        addStatusMessage(`Movement complete: ${data}`, 'success');
      })
      .catch(err => addStatusMessage('Movement failed', 'error'));
    }

    function updatePosition() {
      fetch('/position')
        .then(res => res.json())
        .then(data => {
          document.getElementById('pos-x').textContent = data.x.toFixed(2);
          document.getElementById('pos-y').textContent = data.y.toFixed(2);
          document.getElementById('pos-angle').textContent = data.angle.toFixed(1);
          document.getElementById('motor-state').textContent = data.state;
          document.getElementById('rpm-value').textContent = data.rpm.toFixed(2);
          
          // Update visual position (scale down for display)
          const scale = 2; // 1cm = 2px
          const centerX = 100; // Grid center
          const centerY = 100;
          
          const displayX = centerX + (data.x / scale);
          const displayY = centerY - (data.y / scale); // Invert Y for display
          
          // Check if position changed significantly (more than 0.5cm)
          const positionChanged = Math.abs(data.x - lastPosition.x) > 0.5 || 
                                 Math.abs(data.y - lastPosition.y) > 0.5;
          
          if (positionChanged || pathPoints.length === 0) {
            // Add point to path
            pathPoints.push({x: displayX, y: displayY});
            updatePathLine();
            lastPosition = {x: data.x, y: data.y};
            
            // Add movement status message when moving
            if (data.state !== 'stop' && positionChanged) {
              addStatusMessage(`Moving ${data.state} - Position: (${data.x.toFixed(1)}, ${data.y.toFixed(1)})`, 'info');
            }
          }
          
          // Update dot position
          const dot = document.getElementById('ugv-dot');
          dot.style.left = Math.max(0, Math.min(200, displayX)) + 'px';
          dot.style.top = Math.max(0, Math.min(200, displayY)) + 'px';
          
          // Rotate dot based on angle (make it arrow-like)
          dot.style.transform = `translate(-50%, -50%) rotate(${data.angle}deg)`;
          
          // Console logging
          const now = new Date().toLocaleTimeString();
          console.log(`[${now}] Position: (${data.x.toFixed(2)}, ${data.y.toFixed(2)}) Angle: ${data.angle.toFixed(1)}° State: ${data.state}`);
        })
        .catch(err => {
          console.log('Position update failed:', err);
          addStatusMessage('Position update failed - check connection', 'error');
        });
    }

    function updatePathLine() {
      const pathLine = document.getElementById('path-line');
      if (pathPoints.length > 1) {
        const pointsString = pathPoints.map(p => `${p.x},${p.y}`).join(' ');
        pathLine.setAttribute('points', pointsString);
      }
    }

    function resetPosition() {
      fetch('/reset_position')
        .then(res => res.text())
        .then(msg => {
          addStatusMessage(msg, 'success');
          pathPoints = []; // Clear path
          updatePathLine();
          updatePosition(); // Immediate update after reset
        });
    }

    function clearPath() {
      pathPoints = [];
      const pathLine = document.getElementById('path-line');
      pathLine.setAttribute('points', '');
      addStatusMessage('Path cleared', 'info');
    }

    // Start with immediate position update
    addStatusMessage('System initialized - Live tracking started', 'success');
    updatePosition();
  </script>
</body>
</html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleControl() {
  String cmd = server.arg("cmd");
  if (cmd == "forward") moveForward();
  else if (cmd == "backward") moveBackward();
  else if (cmd == "left") turnLeft();
  else if (cmd == "right") turnRight();
  else stopMotors();

  String json = "{\"command\":\"" + cmd + "\",\"status\":\"OK\"}";
  server.send(200, "application/json", json);
}
