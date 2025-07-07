#include <esp_now.h>
#include <WiFi.h>

// L293D motor control pins
const int motor1Pin1 = 25; // IN1 on L293D
const int motor1Pin2 = 26; // IN2 on L293D
const int motor2Pin1 = 27; // IN3 on L293D
const int motor2Pin2 = 14; // IN4 on L293D

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

// Callback function for receiving data
void onDataRecv(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len) {
  char data[20];
  memcpy(data, incomingData, len);
  data[len] = '\0'; // Null terminate the string
  String command = String(data);
  Serial.println("Received command: " + command);

  // Control slave's motors based on received command
  if (command == "FORWARD") moveForward();
  else if (command == "BACKWARD") moveBackward();
  else if (command == "LEFT") moveLeft();
  else if (command == "RIGHT") moveRight();
  else stopMotors();
}

void setup() {
  Serial.begin(115200);

  // Set motor pins as outputs
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT);
  pinMode(motor2Pin2, OUTPUT);
  stopMotors();

  // Set up ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register callback for receiving data
  esp_now_register_recv_cb(onDataRecv);

  Serial.println("Slave ready. MAC Address: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  // Nothing to do in loop, everything handled by ESP-NOW callback
}
