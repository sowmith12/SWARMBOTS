#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

// Wi-Fi credentials
const char* ssid = "Infinix";
const char* password = "12345678";

// Motor driver pins (L293 connections)
#define MOTOR_A_IN1 14  // Motor A IN1
#define MOTOR_A_IN2 27  // Motor A IN2
#define MOTOR_A_ENA 33  // Motor A Speed (PWM) - EN1
#define MOTOR_B_IN1 26  // Motor B IN1
#define MOTOR_B_IN2 25  // Motor B IN2
#define MOTOR_B_ENB 32  // Motor B Speed (PWM) - EN2

// MPU6050 I2C pins
#define SDA_PIN 21
#define SCL_PIN 22

// Movement parameters (adjust based on your UGV)
const float LINEAR_SPEED = 0.5; // m/s (forward/backward speed)
const float ANGULAR_SPEED = 90.0; // deg/s (turning speed)

// Tracking variables
float x_pos = 0.0; // X position in meters
float y_pos = 0.0; // Y position in meters
float yaw_angle = 0.0; // Current yaw angle in degrees
float gyroZ_offset = 0.0; // Gyroscope Z-axis offset
unsigned long lastTime = 0; // Last update time
unsigned long lastCommandTime = 0; // Time of last command
String currentCommand = "stop"; // Current motor command

Adafruit_MPU6050 mpu;
WebServer server(80);

// Function prototypes
void handleRoot();
void handleControl();
void handleTrack();
void moveForward();
void moveBackward();
void turnLeft();
void turnRight();
void stopMotors();
void calibrateGyro();
void updatePosition();

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  // Initialize motor pins
  pinMode(MOTOR_A_IN1, OUTPUT);
  pinMode(MOTOR_A_IN2, OUTPUT);
  pinMode(MOTOR_A_ENA, OUTPUT);
  pinMode(MOTOR_B_IN1, OUTPUT);
  pinMode(MOTOR_B_IN2, OUTPUT);
  pinMode(MOTOR_B_ENB, OUTPUT);

  // Force all motor pins LOW initially
  digitalWrite(MOTOR_A_IN1, LOW);
  digitalWrite(MOTOR_A_IN2, LOW);
  digitalWrite(MOTOR_A_ENA, LOW);
  digitalWrite(MOTOR_B_IN1, LOW);
  digitalWrite(MOTOR_B_IN2, LOW);
  digitalWrite(MOTOR_B_ENB, LOW);

  delay(100);
  stopMotors();

  // Debug: Print pin states
  Serial.println("=== MOTOR PINS DEBUG ===");
  Serial.println("Motor A (Left): IN1=" + String(MOTOR_A_IN1) + ", IN2=" + String(MOTOR_A_IN2) + ", ENA=" + String(MOTOR_A_ENA));
  Serial.println("Motor B (Right): IN1=" + String(MOTOR_B_IN1) + ", IN2=" + String(MOTOR_B_IN2) + ", ENB=" + String(MOTOR_B_ENB));

  // Initialize I2C and MPU6050
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) delay(10);
  }
  Serial.println("MPU6050 Found!");
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Calibrate gyroscope
  calibrateGyro();
  lastTime = micros();
  lastCommandTime = micros();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());

  // Define server routes
  server.on("/", handleRoot);
  server.on("/control", handleControl);
  server.on("/track", handleTrack);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  updatePosition();
}

void calibrateGyro() {
  Serial.println("Calibrating gyroscope, keep UGV still...");
  float sumGyroZ = 0.0;
  int samples = 1000;
  for (int i = 0; i < samples; i++) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    sumGyroZ += g.gyro.z;
    delay(3);
  }
  gyroZ_offset = sumGyroZ / samples;
  Serial.print("Gyro Z offset: ");
  Serial.print(gyroZ_offset, 4);
  Serial.println(" rad/s");
}

void updatePosition() {
  unsigned long currentTime = micros();
  float dt = (currentTime - lastTime) / 1000000.0; // Time delta in seconds
  lastTime = currentTime;

  // Update yaw angle using MPU6050
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float gyroZ = g.gyro.z - gyroZ_offset;
  yaw_angle += gyroZ * dt * 180.0 / PI; // Update angle in degrees

  // Update position based on current command
  float dt_command = (currentTime - lastCommandTime) / 1000000.0;
  float yaw_rad = yaw_angle * PI / 180.0; // Convert to radians
  if (currentCommand == "forward") {
    x_pos += LINEAR_SPEED * dt_command * cos(yaw_rad);
    y_pos += LINEAR_SPEED * dt_command * sin(yaw_rad);
  } else if (currentCommand == "backward") {
    x_pos -= LINEAR_SPEED * dt_command * cos(yaw_rad);
    y_pos -= LINEAR_SPEED * dt_command * sin(yaw_rad);
  } else if (currentCommand == "left") {
    yaw_angle -= ANGULAR_SPEED * dt_command;
  } else if (currentCommand == "right") {
    yaw_angle += ANGULAR_SPEED * dt_command;
  }
  lastCommandTime = currentTime;

  // Normalize yaw angle to -180 to +180
  if (yaw_angle > 180.0) yaw_angle -= 360.0;
  if (yaw_angle <= -180.0) yaw_angle += 360.0;
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>UGV Control</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      background-color: #f0f0f0;
      color: #333;
      margin: 0;
      padding: 20px;
    }
    h1 {
      font-size: 24px;
      margin-bottom: 20px;
    }
    .control-panel {
      display: inline-block;
      background-color: #fff;
      padding: 20px;
      border-radius: 8px;
      box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
    }
    .control-grid {
      display: grid;
      grid-template-columns: 1fr 1fr 1fr;
      grid-template-rows: 1fr 1fr 1fr;
      gap: 10px;
      width: 210px;
    }
    .control-btn {
      background-color: #ddd;
      border: 1px solid #999;
      color: #333;
      font-size: 16px;
      padding: 10px;
      border-radius: 5px;
      cursor: pointer;
      transition: background-color 0.2s;
    }
    .control-btn:hover {
      background-color: #ccc;
    }
    .control-btn:active {
      background-color: #bbb;
    }
    .forward { grid-column: 2; grid-row: 1; }
    .left { grid-column: 1; grid-row: 2; }
    .stop { grid-column: 2; grid-row: 2; }
    .right { grid-column: 3; grid-row: 2; }
    .backward { grid-column: 2; grid-row: 3; }
    .track { margin-top: 20px; width: 100%; }
    .status {
      margin-top: 15px;
      font-size: 16px;
      color: #333;
    }
  </style>
</head>
<body>
  <h1>UGV Control</h1>
  <div class="control-panel">
    <div class="control-grid">
      <button class="control-btn forward" onmousedown="sendCommand('forward')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('forward')" ontouchend="sendCommand('stop')">Forward</button>
      <button class="control-btn left" onmousedown="sendCommand('left')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('left')" ontouchend="sendCommand('stop')">Left</button>
      <button class="control-btn stop" onclick="sendCommand('stop')">Stop</button>
      <button class="control-btn right" onmousedown="sendCommand('right')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('right')" ontouchend="sendCommand('stop')">Right</button>
      <button class="control-btn backward" onmousedown="sendCommand('backward')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('backward')" ontouchend="sendCommand('stop')">Backward</button>
    </div>
    <button class="control-btn track" onclick="getPosition()">Track Position</button>
    <div class="status" id="status">Ready to Control</div>
  </div>

  <script>
    function sendCommand(command) {
      fetch('/control?cmd=' + command)
        .then(response => response.json())
        .then(data => {
          document.getElementById('status').innerHTML = 'Command: ' + data.command + ' | Status: ' + data.status;
        })
        .catch(error => {
          console.log('Error:', error);
          document.getElementById('status').innerHTML = 'Connection Error';
        });
    }

    function getPosition() {
      fetch('/track')
        .then(response => response.json())
        .then(data => {
          document.getElementById('status').innerHTML = 
            'Position: X = ' + data.x.toFixed(2) + ' m, Y = ' + data.y.toFixed(2) + ' m';
        })
        .catch(error => {
          console.log('Error:', error);
          document.getElementById('status').innerHTML = 'Connection Error';
        });
    }

    document.addEventListener('keydown', function(event) {
      if (event.repeat) return;
      switch(event.key) {
        case 'ArrowUp': case 'w': case 'W':
          sendCommand('forward');
          break;
        case 'ArrowDown': case 's': case 'S':
          sendCommand('backward');
          break;
        case 'ArrowLeft': case 'a': case 'A':
          sendCommand('left');
          break;
        case 'ArrowRight': case 'd': case 'D':
          sendCommand('right');
          break;
        case ' ': case 't': case 'T':
          if (event.key === 't' || event.key === 'T') getPosition();
          else sendCommand('stop');
          event.preventDefault();
          break;
      }
    });

    document.addEventListener('keyup', function(event) {
      switch(event.key) {
        case 'ArrowUp': case 'ArrowDown': case 'ArrowLeft': case 'ArrowRight':
        case 'w': case 'W': case 's': case 'S': case 'a': case 'A': case 'd': case 'D':
          sendCommand('stop');
          break;
      }
    });
  </script>
</body>
</html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void handleControl() {
  String command = server.arg("cmd");
  String status = "OK";

  // Update position before changing command
  updatePosition();
  currentCommand = command;
  lastCommandTime = micros();

  Serial.println("Command received: " + command);

  if (command == "forward") {
    moveForward();
  } else if (command == "backward") {
    moveBackward();
  } else if (command == "left") {
    turnLeft();
  } else if (command == "right") {
    turnRight();
  } else if (command == "stop") {
    stopMotors();
  } else {
    status = "Unknown Command";
  }

  String json = "{\"command\":\"" + command + "\",\"status\":\"" + status + "\"}";
  server.send(200, "application/json", json);
}

void handleTrack() {
  updatePosition();
  String json = "{\"x\":" + String(x_pos, 2) + ",\"y\":" + String(y_pos, 2) + "}";
  server.send(200, "application/json", json);
}

void moveForward() {
  stopMotors();
  delay(50);
  Serial.println("=== MOVING FORWARD ===");
  digitalWrite(MOTOR_A_IN1, HIGH);
  digitalWrite(MOTOR_A_IN2, LOW);
  analogWrite(MOTOR_A_ENA, 255);
  digitalWrite(MOTOR_B_IN1, HIGH);
  digitalWrite(MOTOR_B_IN2, LOW);
  analogWrite(MOTOR_B_ENB, 255);
  Serial.println("Left Motor: IN1=HIGH, IN2=LOW, ENA=255");
  Serial.println("Right Motor: IN1=HIGH, IN2=LOW, ENB=255");
}

void moveBackward() {
  stopMotors();
  delay(50);
  Serial.println("=== MOVING BACKWARD ===");
  digitalWrite(MOTOR_A_IN1, LOW);
  digitalWrite(MOTOR_A_IN2, HIGH);
  analogWrite(MOTOR_A_ENA, 255);
  digitalWrite(MOTOR_B_IN1, LOW);
  digitalWrite(MOTOR_B_IN2, HIGH);
  analogWrite(MOTOR_B_ENB, 255);
  Serial.println("Left Motor: IN1=LOW, IN2=HIGH, ENA=255");
  Serial.println("Right Motor: IN1=LOW, IN2=HIGH, ENB=255");
}

void turnLeft() {
  digitalWrite(MOTOR_A_IN1, LOW);
  digitalWrite(MOTOR_A_IN2, HIGH);
  analogWrite(MOTOR_A_ENA, 150);
  digitalWrite(MOTOR_B_IN1, HIGH);
  digitalWrite(MOTOR_B_IN2, LOW);
  analogWrite(MOTOR_B_ENB, 150);
  Serial.println("Turning Left");
}

void turnRight() {
  digitalWrite(MOTOR_A_IN1, HIGH);
  digitalWrite(MOTOR_A_IN2, LOW);
  analogWrite(MOTOR_A_ENA, 150);
  digitalWrite(MOTOR_B_IN1, LOW);
  digitalWrite(MOTOR_B_IN2, HIGH);
  analogWrite(MOTOR_B_ENB, 150);
  Serial.println("Turning Right");
}

void stopMotors() {
  digitalWrite(MOTOR_A_IN1, LOW);
  digitalWrite(MOTOR_A_IN2, LOW);
  digitalWrite(MOTOR_A_ENA, LOW);
  digitalWrite(MOTOR_B_IN1, LOW);
  digitalWrite(MOTOR_B_IN2, LOW);
  digitalWrite(MOTOR_B_ENB, LOW);
  Serial.println("Motors Stopped - All pins LOW");
  Serial.println("Debug - Pin States After Stop:");
  Serial.println("A_IN1: " + String(digitalRead(MOTOR_A_IN1)) + ", A_IN2: " + String(digitalRead(MOTOR_A_IN2)));
  Serial.println("B_IN1: " + String(digitalRead(MOTOR_B_IN1)) + ", B_IN2: " + String(digitalRead(MOTOR_B_IN2)));
}
