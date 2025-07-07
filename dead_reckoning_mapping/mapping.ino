#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <NewPing.h>

// Wi-Fi credentials
const char* ssid = "apple";
const char* password = "9966809332";

// Pin definitions
#define TRIG_PIN 5
#define ECHO_PIN 18
#define IN1 14
#define IN2 27
#define IN3 26
#define IN4 25
#define ENA 33
#define ENB 32

// Constants
#define MAX_DISTANCE 200
#define ROTATION_STEPS 36
#define ROTATION_TIME 4000
#define MOVE_FORWARD_TIME 5000
#define MOTOR_SPEED 150
#define WIFI_TIMEOUT 15000

// Initialize objects
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Function declarations
void moveForward();
void rotateInPlace();
void stopMotors();
void performMappingCycle();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);

// HTML webpage with p5.js
const char* webpage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>UGV Mapping</title>
</head>
<body>
  <h1>UGV Room Mapping</h1>
  <button onclick="startMapping()" id="startButton">Start Mapping</button>
  <div id="canvas"></div>
  <script>
    console.log("Main script loaded successfully");
    let ws;
    let points = [];
    
    function startMapping() {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send('S');
        console.log("Sent start mapping command");
      } else {
        console.log("WebSocket not connected, cannot start mapping");
      }
    }
    
    try {
      ws = new WebSocket('ws://' + window.location.hostname + ':81/');
      console.log("Attempting WebSocket connection to: ws://" + window.location.hostname + ":81/");
      ws.onopen = function() {
        console.log("WebSocket connected");
        document.getElementById('startButton').style.backgroundColor = 'green';
      };
      ws.onerror = function(error) {
        console.log("WebSocket error:", error);
      };
      ws.onclose = function(event) {
        console.log("WebSocket closed:", event);
      };
      ws.onmessage = function(event) {
        console.log("Received WebSocket message:", event.data);
        let data = event.data.split(',');
        if (data[0].startsWith('A')) {
          let angle = parseInt(data[0].substring(2));
          let dist = parseInt(data[1].substring(2));
          points.push({angle: angle, dist: dist});
          if (points.length > 360) points.shift();
        }
      };
    } catch (e) {
      console.log("WebSocket initialization failed:", e);
    }
  </script>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/p5.js/1.4.2/p5.min.js" onerror="console.log('Failed to load p5.js');"></script>
  <script>
    if (typeof p5 !== 'undefined') {
      console.log("p5.js loaded successfully, initializing canvas");
      function setup() {
        let canvas = createCanvas(600, 600);
        canvas.parent('canvas');
        background(255);
      }
      
      function draw() {
        background(255);
        translate(width/2, height/2);
        stroke(0);
        fill(0, 100);
        beginShape();
        for (let p of points) {
          let x = p.dist * cos(radians(p.angle)) * 2;
          let y = p.dist * sin(radians(p.angle)) * 2;
          vertex(x, -y);
        }
        endShape(CLOSE);
        fill(255, 0, 0);
        ellipse(0, 0, 20, 20);
      }
    } else {
      console.log("p5.js not loaded, skipping canvas initialization");
    }
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  stopMotors();
  
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();
  Serial.println("Attempting Wi-Fi connection...");
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT) {
    Serial.print("WiFi Status: ");
    Serial.println(WiFi.status());
    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL:
        Serial.println("Error: SSID not found");
        break;
      case WL_CONNECT_FAILED:
        Serial.println("Error: Connection failed (wrong password?)");
        break;
      case WL_DISCONNECTED:
        Serial.println("Disconnected, retrying...");
        break;
    }
    delay(1000);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi connection timed out. Restarting...");
    ESP.restart();
  }
  
  server.on("/", []() {
    server.send(200, "text/html", webpage);
  });
  server.begin();
  
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  server.handleClient();
  webSocket.loop();
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected. Reconnecting...");
    WiFi.reconnect();
    delay(5000);
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT && payload[0] == 'S') {
    performMappingCycle();
  }
}

void performMappingCycle() {
  while (true) {
    unsigned long startTime = millis();
    rotateInPlace();
    while (millis() - startTime < ROTATION_TIME) {
      int step = ((millis() - startTime) * ROTATION_STEPS) / ROTATION_TIME;
      if (step < ROTATION_STEPS) {
        unsigned int distance = sonar.ping_cm();
        if (distance == 0) distance = MAX_DISTANCE;
        int angle = (step * 360) / ROTATION_STEPS;
        String data = "A:" + String(angle) + ",D:" + String(distance);
        webSocket.broadcastTXT(data);
        delay(ROTATION_TIME / ROTATION_STEPS);
      }
    }
    stopMotors();
    
    moveForward();
    delay(MOVE_FORWARD_TIME);
    stopMotors();
  }
}

void moveForward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(ENA, MOTOR_SPEED);
  analogWrite(ENB, MOTOR_SPEED);
}

void rotateInPlace() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, MOTOR_SPEED);
  analogWrite(ENB, MOTOR_SPEED);
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}
