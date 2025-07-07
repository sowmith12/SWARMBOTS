#include <WiFi.h>
#include <WebServer.h>

// WiFi credentials
const char* ssid = "apple";
const char* password = "9966809332";
// Motor pin definitions
#define MOTOR_A_IN1 14
#define MOTOR_A_IN2 27
#define MOTOR_A_ENA 33

#define MOTOR_B_IN1 26
#define MOTOR_B_IN2 25
#define MOTOR_B_ENB 32

WebServer server(80);

// ==== Setup ====

void setup() {
 Serial.begin(115200);

 // Setup motor pins
 pinMode(MOTOR_A_IN1, OUTPUT);
 pinMode(MOTOR_A_IN2, OUTPUT);
 pinMode(MOTOR_A_ENA, OUTPUT);
 pinMode(MOTOR_B_IN1, OUTPUT);
 pinMode(MOTOR_B_IN2, OUTPUT);
 pinMode(MOTOR_B_ENB, OUTPUT);
 
 stopMotors();

 Serial.println("Connecting to WiFi...");
 WiFi.begin(ssid, password);
 while (WiFi.status() != WL_CONNECTED) {
  delay(500);
  Serial.print(".");
 }

 Serial.println("\nWiFi Connected");
 Serial.println("IP Address: " + WiFi.localIP().toString());

 // Web routes
 server.on("/", handleRoot);
 server.on("/control", handleControl);

 server.begin();
 Serial.println("HTTP server started");
}

void loop() {
 server.handleClient();
}

// ==== Motor Control ====

void moveForward() {
 stopMotors();
 delay(50);
 digitalWrite(MOTOR_A_IN1, HIGH); digitalWrite(MOTOR_A_IN2, LOW);
 digitalWrite(MOTOR_B_IN1, HIGH); digitalWrite(MOTOR_B_IN2, LOW);
 analogWrite(MOTOR_A_ENA, 255);
 analogWrite(MOTOR_B_ENB, 255);
 Serial.println("Moving Forward");
}

void moveBackward() {
 stopMotors();
 delay(50);
 digitalWrite(MOTOR_A_IN1, LOW); digitalWrite(MOTOR_A_IN2, HIGH);
 digitalWrite(MOTOR_B_IN1, LOW); digitalWrite(MOTOR_B_IN2, HIGH);
 analogWrite(MOTOR_A_ENA, 255);
 analogWrite(MOTOR_B_ENB, 255);
 Serial.println("Moving Backward");
}

void turnLeft() {
 digitalWrite(MOTOR_A_IN1, LOW); digitalWrite(MOTOR_A_IN2, HIGH);
 digitalWrite(MOTOR_B_IN1, HIGH); digitalWrite(MOTOR_B_IN2, LOW);
 analogWrite(MOTOR_A_ENA, 150);
 analogWrite(MOTOR_B_ENB, 150);
 Serial.println("Turning Left");
}

void turnRight() {
 digitalWrite(MOTOR_A_IN1, HIGH); digitalWrite(MOTOR_A_IN2, LOW);
 digitalWrite(MOTOR_B_IN1, LOW); digitalWrite(MOTOR_B_IN2, HIGH);
 analogWrite(MOTOR_A_ENA, 150);
 analogWrite(MOTOR_B_ENB, 150);
 Serial.println("Turning Right");
}

void stopMotors() {
 digitalWrite(MOTOR_A_IN1, LOW); digitalWrite(MOTOR_A_IN2, LOW); digitalWrite(MOTOR_A_ENA, LOW);
 digitalWrite(MOTOR_B_IN1, LOW); digitalWrite(MOTOR_B_IN2, LOW); digitalWrite(MOTOR_B_ENB, LOW);
 Serial.println("Motors Stopped");
}

// ==== Web Handlers ====
void handleRoot() {
 String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
 <meta charset="UTF-8">
 <title>🚗 UGV Control Panel</title>
 <style>
  body {
   background: #121212;
   color: white;
   font-family: 'Segoe UI', sans-serif;
   text-align: center;
   padding: 20px;
  }

  h1 {
   font-size: 2em;
   color: #00ffcc;
   margin-bottom: 40px;
  }

  .control-grid {
   display: grid;
   grid-template-columns: 80px 80px 80px;
   grid-template-rows: 80px 80px 80px;
   gap: 20px;
   justify-content: center;
   margin: auto;
  }

  .control-btn {
   background: #1e1e1e;
   border: 2px solid #00ffcc;
   border-radius: 12px;
   color: #00ffcc;
   font-size: 20px;
   font-weight: bold;
   cursor: pointer;
   box-shadow: 0 0 10px #00ffcc55;
   transition: 0.2s;
  }

  .control-btn:hover {
   background: #00ffcc;
   color: black;
   transform: scale(1.05);
   box-shadow: 0 0 20px #00ffccaa;
  }

  .stop {
   background: #cc0000;
   border-color: #ff4444;
   color: white;
  }

  .stop:hover {
   background: #ff4444;
  }

  .status {
   margin-top: 40px;
   font-size: 1.2em;
   color: #00ffcc;
  }
 </style>
</head>
<body>
 <h1>🚗 UGV Control Panel</h1>
 <div class="control-grid">
  <button class="control-btn" onmousedown="send('forward')" onmouseup="send('stop')">⬆</button>
  <button class="control-btn" onmousedown="send('left')" onmouseup="send('stop')">⬅</button>
  <button class="control-btn stop" onclick="send('stop')">⏹</button>
  <button class="control-btn" onmousedown="send('right')" onmouseup="send('stop')">➡</button>
  <button class="control-btn" onmousedown="send('backward')" onmouseup="send('stop')">⬇</button>
 </div>
 <div class="status" id="status">Status: Ready</div>

 <script>
  function send(cmd) {
   fetch('/control?cmd=' + cmd)
    .then(res => res.json())
    .then(data => {
     document.getElementById('status').innerText = 'Status: ' + data.status + ' (' + data.command + ')';
    })
    .catch(() => {
     document.getElementById('status').innerText = 'Status: Error';
    });
  }

  document.addEventListener('keydown', function(e) {
   if (e.repeat) return;
   switch (e.key.toLowerCase()) {
    case 'w': send('forward'); break;
    case 's': send('backward'); break;
    case 'a': send('left'); break;
    case 'd': send('right'); break;
    case ' ': send('stop'); e.preventDefault(); break;
   }
  });

  document.addEventListener('keyup', function(e) {
   if (['w', 'a', 's', 'd'].includes(e.key.toLowerCase())) send('stop');
  });
 </script>
</body>
</html>
 )rawliteral";

 server.send(200, "text/html", html);
}

void handleControl() {
 String cmd = server.arg("cmd");
 String status = "OK";

 if (cmd == "forward") moveForward();
 else if (cmd == "backward") moveBackward();
 else if (cmd == "left") turnLeft();
 else if (cmd == "right") turnRight();
 else if (cmd == "stop") stopMotors();
 else status = "Unknown Command";

 String json = "{\"command\":\"" + cmd + "\",\"status\":\"" + status + "\"}";
 server.send(200, "application/json", json);
}
