// Ultrasonic sensor pins
const int trigPin = 5;
const int echoPin = 18;

// Motor driver pins (L293D)
const int motorA_IN1 = 14;  // Left motor forward
const int motorA_IN2 = 27;  // Left motor backward
const int motorB_IN3 = 26;  // Right motor forward
const int motorB_IN4 = 25;  // Right motor backward
const int enaPin = 33;      // ENA (left motor)
const int enbPin = 32;      // ENB (right motor)

// Distance threshold for obstacle detection (in cm)
const int obstacleDistance = 20;

// Speed for motors (0-255)
const int leftMotorSpeed = 200;  // Adjust if needed
const int rightMotorSpeed = 180; // Adjust to balance (lower if turning left)

// Function to measure distance using ultrasonic sensor
long getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  return duration * 0.034 / 2;  // Distance in cm
}

// Motor control functions
void moveForward() {
  digitalWrite(motorA_IN1, LOW);  // Reversed: LOW for forward
  digitalWrite(motorA_IN2, HIGH); // Reversed: HIGH for forward
  digitalWrite(motorB_IN3, LOW);  // Reversed: LOW for forward
  digitalWrite(motorB_IN4, HIGH); // Reversed: HIGH for forward
  analogWrite(enaPin, leftMotorSpeed);
  analogWrite(enbPin, rightMotorSpeed);
}

void moveBackward() {
  digitalWrite(motorA_IN1, HIGH); // Reversed: HIGH for backward
  digitalWrite(motorA_IN2, LOW);  // Reversed: LOW for backward
  digitalWrite(motorB_IN3, HIGH); // Reversed: HIGH for backward
  digitalWrite(motorB_IN4, LOW);  // Reversed: LOW for backward
  analogWrite(enaPin, leftMotorSpeed);
  analogWrite(enbPin, rightMotorSpeed);
}

void turnRight() {
  digitalWrite(motorA_IN1, LOW);  // Left motor forward
  digitalWrite(motorA_IN2, HIGH);
  digitalWrite(motorB_IN3, HIGH); // Right motor backward
  digitalWrite(motorB_IN4, LOW);
  analogWrite(enaPin, leftMotorSpeed);
  analogWrite(enbPin, rightMotorSpeed);
}

void stopMotors() {
  digitalWrite(motorA_IN1, LOW);
  digitalWrite(motorA_IN2, LOW);
  digitalWrite(motorB_IN3, LOW);
  digitalWrite(motorB_IN4, LOW);
  analogWrite(enaPin, 0);
  analogWrite(enbPin, 0);
}

void setup() {
  Serial.begin(115200);

  // Initialize pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(motorA_IN1, OUTPUT);
  pinMode(motorA_IN2, OUTPUT);
  pinMode(motorB_IN3, OUTPUT);
  pinMode(motorB_IN4, OUTPUT);
  pinMode(enaPin, OUTPUT);
  pinMode(enbPin, OUTPUT);
}

void loop() {
  long distance = getDistance();
  Serial.print("Distance: ");
  Serial.println(distance);

  if (distance < obstacleDistance && distance > 0) {
    stopMotors();
    delay(500);
    moveBackward();
    delay(1000);
    turnRight();
    delay(500);
  } else {
    moveForward();
  }

  delay(100);
}
