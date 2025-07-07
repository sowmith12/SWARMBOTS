#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>

// WiFi credentials (to connect to your router)
const char* ssid = "Infinix";
const char* password = "12345678";

// Slave MAC Address
uint8_t slaveMacAddress[] = {0xEC, 0x94, 0xCB, 0x52, 0x99, 0x78};

// L293D motor control pins
const int motor1Pin1 = 25; // IN1 on L293D
const int motor1Pin2 = 26; // IN2 on L293D
const int motor2Pin1 = 27; // IN3 on L293D
const int motor2Pin2 = 14; // IN4 on L293D

// Command to send via ESP-NOW
String command = "STOP";

// Web server on port 80
WebServer server(80);

// HTML for the control webpage
String webpage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>UGV Control</title>
  <style>
    body { text-align: center; font-family: Arial; }
    button { font-size: 20px; margin: 10px; padding: 15px 30px; }
  </style>
</head>
<body>
  <h1>UGV Control</h1>
  <div>
    <button onclick="sendCommand('FORWARD')">Forward</button>
  </div>
  <div>
    <button onclick="sendCommand('LEFT')">Left</button>
    <button onclick="sendCommand('STOP')">Stop</button>
    <button onclick="sendCommand('RIGHT')">Right</button>
  </div>
  <div>
    <button onclick="sendCommand('BACKWARD')">Backward</button>
  </div>
  <script>
    function sendCommand(cmd) {
      fetch('/command?dir=' + cmd)
        .then(response => response.text())
        .then(data => console.log(data));
    }
  </script>
</body>
</html>
)rawliteral";

// ESP-NOW send callback
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// Motor control functions
void moveForward() {
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, HIGH);
  digitalWrite(motor2Pin2, LOW);
}

void moveBackward() {
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, HIGH);
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, HIGH);
}

void moveLeft() {
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, HIGH);
  digitalWrite(motor2Pin1, HIGH);
  digitalWrite(motor2Pin2, LOW);
}

void moveRight() {
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, HIGH);
}

void stopMotors() {
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, LOW);
}

void handleCommand() {
  if (server.hasArg("dir")) {
    command = server.arg("dir");
    Serial.println("Command: " + command);

    // Control master's motors
    if (command == "FORWARD") {
      moveForward();
    } else if (command == "BACKWARD") {
      moveBackward();
    } else if (command == "LEFT") {
      moveLeft();
    } else if (command == "RIGHT") {
      moveRight();
    } else {
      stopMotors();
    }

    // Send command to slave via ESP-NOW
    esp_now_send(slaveMacAddress, (uint8_t *)command.c_str(), command.length());
    server.send(200, "text/plain", "Command received: " + command);
  } else {
    server.send(400, "text/plain", "No command provided");
  }
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  delay(1000); // Give Serial Monitor time to initialize
  Serial.println("Starting Master ESP32...");

  // Set motor pins as outputs
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT);
  pinMode(motor2Pin2, OUTPUT);
  stopMotors();
  Serial.println("Motor pins initialized.");

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi!");
    while (1); // Halt execution
  }

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    while (1); // Halt execution
  }
  Serial.println("ESP-NOW initialized.");

  esp_now_register_send_cb(onDataSent);
  Serial.println("ESP-NOW send callback registered.");

  // Register peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, slaveMacAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    while (1); // Halt execution
  }
  Serial.println("Peer added successfully.");

  // Set up web server routes
  server.on("/", []() {
    server.send(200, "text/html", webpage);
  });
  server.on("/command", handleCommand);
  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
}
