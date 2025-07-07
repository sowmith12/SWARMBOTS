#include <WiFi.h>
#include <WebServer.h>
#include <vector>
#include <cmath>
#include <algorithm>

// --- Wi-Fi credentials ---
const char* ssid = "apple";
const char* password = "9966809332";

// --- Motor pins (L293D) ---
#define MOTOR_A_IN1 14
#define MOTOR_A_IN2 27
#define MOTOR_A_ENA 33
#define MOTOR_B_IN1 26
#define MOTOR_B_IN2 25
#define MOTOR_B_ENB 32

WebServer server(80);

// --- UGV Allocation & Movement Globals ---
struct Point {
  float x;
  float y;
};

// Calibration values
float rpm = 0;
float wheel_circumference = 0;
float wheel_base_cm = 14.0; // Distance between wheels (in cm)

// Position tracking variables
float current_x = 0.0;
float current_y = 0.0;
float current_angle = 0.0; // Current heading in degrees (0 = facing +X direction, increasing Counter-Clockwise)

// Motor state tracking
unsigned long motor_start_time = 0;
String current_motor_state = "stop";
bool is_moving = false;

// Array to hold the target spots clicked on the map
std::vector<Point> ugv_spots_to_visit;

// --- Function Prototypes ---
void handleRoot();
void handleControl();
void handlePosition();
void handleResetPosition();
void moveForward();
void moveBackward();
void turnLeft();
void turnRight();
void stopMotors();
void updatePosition();
float distance(Point a, Point b);
void sortSpotsByNearestNeighbor(Point start, std::vector<Point>& spots);
void gotoPosition(float target_x, float target_y);
void handleAllocateAndGo();

// --- Core UGV Logic ---

/**
 * @brief Calculates the Euclidean distance between two points.
 * @param a The first point.
 * @param b The second point.
 * @return The distance between points a and b.
 */
float distance(Point a, Point b) {
  return sqrt(pow(a.x - b.x, 2) + pow(a.y - b.y, 2));
}

/**
 * @brief Sorts a vector of points using the Nearest Neighbor algorithm.
 * The algorithm starts from a given 'start' point and iteratively
 * selects the closest unvisited point. This function modifies the
 * input 'spots' vector in place.
 * @param start The starting point for the path planning (usually current UGV position).
 * @param spots A reference to the vector of points to be sorted.
 */
void sortSpotsByNearestNeighbor(Point start, std::vector<Point>& spots) {
  if (spots.empty()) {
    return;
  }
  
  std::vector<Point> sorted_spots;
  
  // Find the nearest spot to the start point
  float min_dist = 999999.0; // Initialize with a very large number
  int nearest_index = -1;
  
  for (int i = 0; i < spots.size(); i++) {
    float dist = distance(start, spots[i]);
    if (dist < min_dist) {
      min_dist = dist;
      nearest_index = i;
    }
  }
  
  // Add the first nearest spot to the sorted list and remove it from original list
  if (nearest_index != -1) {
    sorted_spots.push_back(spots[nearest_index]);
    spots.erase(spots.begin() + nearest_index);
  } else {
    // This case should not be reached if spots is not empty, but included for robustness.
    Serial.println("Error: No nearest spot found in sortSpotsByNearestNeighbor.");
    return; 
  }
  
  Point current_pos = sorted_spots[0]; // Set the first point in sorted_spots as current
  
  // Continue finding the nearest neighbor until all spots are sorted
  while (!spots.empty()) {
    min_dist = 999999.0;
    nearest_index = -1;
    
    for (int i = 0; i < spots.size(); i++) {
      float dist = distance(current_pos, spots[i]);
      if (dist < min_dist) {
        min_dist = dist;
        nearest_index = i;
      }
    }
    
    if (nearest_index != -1) {
      sorted_spots.push_back(spots[nearest_index]);
      current_pos = spots[nearest_index]; // Update current position for next iteration
      spots.erase(spots.begin() + nearest_index); // Remove the added spot
    } else {
      // Should not happen if spots is not empty
      Serial.println("Error: No nearest spot found during iteration in sortSpotsByNearestNeighbor.");
      break; 
    }
  }
  
  spots = sorted_spots; // Replace the original spots vector with the sorted one
}

/**
 * @brief Updates the UGV's estimated position and orientation based on its
 * current motor state and elapsed time. This implements a basic odometry model.
 */
void updatePosition() {
  // Only update if motors are currently moving and calibration values are set
  if (is_moving && rpm > 0 && wheel_circumference > 0) {
    unsigned long current_time = millis();
    float elapsed_time_sec = (current_time - motor_start_time) / 1000.0;
    
    // Distance traveled by a single wheel per second at current RPM
    float distance_per_second = (rpm * wheel_circumference) / 60.0; // cm/s
    float distance_moved = distance_per_second * elapsed_time_sec; // Distance since last update
    
    // Update position based on current motor state
    if (current_motor_state == "forward") {
      current_x += distance_moved * cos(current_angle * PI / 180.0);
      current_y += distance_moved * sin(current_angle * PI / 180.0);
    } else if (current_motor_state == "backward") {
      current_x -= distance_moved * cos(current_angle * PI / 180.0);
      current_y -= distance_moved * sin(current_angle * PI / 180.0);
    } else if (current_motor_state == "left") { // Physical Left turn (Counter-Clockwise)
      // Angle should INCREASE for CCW rotation
      float angle_change_rad = distance_moved / (wheel_base_cm / 2.0); 
      current_angle += angle_change_rad * 180.0 / PI; 
      if (current_angle >= 360.0) current_angle -= 360.0; // Keep angle between 0 and 360
    } else if (current_motor_state == "right") { // Physical Right turn (Clockwise)
      // Angle should DECREASE for CW rotation
      float angle_change_rad = distance_moved / (wheel_base_cm / 2.0);
      current_angle -= angle_change_rad * 180.0 / PI;
      if (current_angle < 0) current_angle += 360.0; // Keep angle between 0 and 360
    }
    
    motor_start_time = current_time; // Reset timer for next update
  }
}

/**
 * @brief Commands the UGV to navigate to a specific target (x, y) position.
 * This function calculates the necessary turns and forward movements.
 * It uses blocking 'delay' calls, suitable for simple demos.
 * @param target_x The target X coordinate in cm.
 * @param target_y The target Y coordinate in cm.
 */
void gotoPosition(float target_x, float target_y) {
  Serial.printf("UGV currently at (%.2f, %.2f) with angle %.1f. Target: (%.2f, %.2f)\n", 
               current_x, current_y, current_angle, target_x, target_y);
  
  float dx = target_x - current_x;
  float dy = target_y - current_y;
  float target_dist = distance({current_x, current_y}, {target_x, target_y});

  // Check if already at or very close to the target
  if (target_dist < 2.0) { // Threshold for "arrived" (2 cm)
    Serial.println("Already very close to target. Skipping movement.");
    return;
  }
  
  // Calculate the desired angle to the target point (0=X+, CCW positive)
  float target_angle_rad = atan2(dy, dx); // Angle in radians from -PI to PI
  float target_angle_deg = target_angle_rad * 180.0 / PI; // Convert to degrees
  if (target_angle_deg < 0) target_angle_deg += 360.0; // Normalize to 0-360 degrees
  
  // Calculate the minimum turn angle needed (-180 to 180)
  float turn_needed = target_angle_deg - current_angle;
  if (turn_needed > 180) turn_needed -= 360; 
  if (turn_needed < -180) turn_needed += 360; 
  
  // Perform turn if a significant angle adjustment is needed
  if (abs(turn_needed) > 5.0) { // Threshold for turning (5 degrees)
    // Estimate time needed for the turn. This is a simplified model.
    // Assuming turning speed is reduced (200/255 of max)
    float turn_speed_cm_s_at_reduced_power = (rpm * wheel_circumference) / 60.0 * (200.0/255.0);
    // Circumference of the circle the UGV's center traces during a 360-degree turn-in-place
    float circumference_of_rotation = PI * wheel_base_cm; 
    // Time to complete a 360-degree turn
    float time_for_360_turn_sec = circumference_of_rotation / turn_speed_cm_s_at_reduced_power;
    // Calculate duration for the required turn angle
    float turn_duration_ms = (abs(turn_needed) / 360.0) * time_for_360_turn_sec * 1000.0;

    Serial.printf("Turning %.1f degrees. Estimated Duration: %.0f ms\n", turn_needed, turn_duration_ms);

    // DETERMINE PHYSICAL TURN DIRECTION:
    if (turn_needed > 0) { // Target angle is CCW (left) relative to current heading
      turnLeft(); // Command physical left turn (which increases current_angle)
    } else { // Target angle is CW (right) relative to current heading
      turnRight(); // Command physical right turn (which decreases current_angle)
    }
    delay(turn_duration_ms); // Execute the turn
    stopMotors();
    delay(200); // Small pause after turning to settle
    current_angle = target_angle_deg; // Force update angle to target after turn completes (for accuracy)
  } else {
    Serial.println("Already aligned with target, no significant turn needed.");
  }
  
  // Move forward to the target if still a significant distance away
  if (target_dist > 2.0 && rpm > 0 && wheel_circumference > 0) { // Re-check distance and calibration
    float move_speed_cm_s = (rpm * wheel_circumference) / 60.0; // Use max speed for forward movement
    float move_duration_ms = (target_dist / move_speed_cm_s) * 1000.0;

    Serial.printf("Moving %.2f cm forward. Estimated Duration: %.0f ms\n", target_dist, move_duration_ms);
    moveForward();
    delay(move_duration_ms); // Execute forward movement
    stopMotors();
    delay(200); // Small pause after moving

    // For UI consistency and immediate feedback, force update UGV position to target after movement
    current_x = target_x;
    current_y = target_y;
  } else {
    Serial.println("Distance too small or RPM/Circumference not calibrated for forward movement.");
  }
}

// --- Motor Functions ---

/**
 * @brief Sets motors to move the UGV forward.
 */
void moveForward() {
  updatePosition(); // Update position based on previous state before starting new motion
  digitalWrite(MOTOR_A_IN1, LOW); digitalWrite(MOTOR_A_IN2, HIGH);
  digitalWrite(MOTOR_B_IN1, LOW); digitalWrite(MOTOR_B_IN2, HIGH);
  analogWrite(MOTOR_A_ENA, 255); analogWrite(MOTOR_B_ENB, 255); // Full speed
  current_motor_state = "forward";
  is_moving = true;
  motor_start_time = millis(); // Record start time of this movement
  Serial.println("Moving Forward");
}

/**
 * @brief Sets motors to move the UGV backward.
 */
void moveBackward() {
  updatePosition();
  digitalWrite(MOTOR_A_IN1, HIGH); digitalWrite(MOTOR_A_IN2, LOW);
  digitalWrite(MOTOR_B_IN1, HIGH); digitalWrite(MOTOR_B_IN2, LOW);
  analogWrite(MOTOR_A_ENA, 255); analogWrite(MOTOR_B_ENB, 255);
  current_motor_state = "backward";
  is_moving = true;
  motor_start_time = millis();
  Serial.println("Moving Backward");
}

/**
 * @brief Sets motors to turn the UGV left (left wheel forward, right wheel backward).
 */
void turnLeft() {
  updatePosition();
  digitalWrite(MOTOR_A_IN1, LOW); digitalWrite(MOTOR_A_IN2, HIGH); // Left wheel forward
  digitalWrite(MOTOR_B_IN1, HIGH); digitalWrite(MOTOR_B_IN2, LOW); // Right wheel backward
  analogWrite(MOTOR_A_ENA, 200); analogWrite(MOTOR_B_ENB, 200); // Slightly slower for turns
  current_motor_state = "left";
  is_moving = true;
  motor_start_time = millis();
  Serial.println("Turning Left");
}

/**
 * @brief Sets motors to turn the UGV right (left wheel backward, right wheel forward).
 */
void turnRight() {
  updatePosition();
  digitalWrite(MOTOR_A_IN1, HIGH); digitalWrite(MOTOR_A_IN2, LOW); // Left wheel backward
  digitalWrite(MOTOR_B_IN1, LOW); digitalWrite(MOTOR_B_IN2, HIGH); // Right wheel forward
  analogWrite(MOTOR_A_ENA, 200); analogWrite(MOTOR_B_ENB, 200); // Slightly slower for turns
  current_motor_state = "right";
  is_moving = true;
  motor_start_time = millis();
  Serial.println("Turning Right");
}

/**
 * @brief Stops all motors.
 */
void stopMotors() {
  updatePosition(); // Final position update when motors stop
  digitalWrite(MOTOR_A_IN1, LOW); digitalWrite(MOTOR_A_IN2, LOW); analogWrite(MOTOR_A_ENA, 0);
  digitalWrite(MOTOR_B_IN1, LOW); digitalWrite(MOTOR_B_IN2, LOW); analogWrite(MOTOR_B_ENB, 0);
  current_motor_state = "stop";
  is_moving = false;
  Serial.println("Stopping Motors");
}

// --- Web Server Handlers ---

/**
 * @brief Handles requests to the root URL ("/"). Serves the main HTML page
 * for UGV control and interactive map.
 */
void handleRoot() {
  // Using a raw string literal (R"rawliteral(...)rawliteral") makes multi-line HTML cleaner
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>UGV Control with Interactive Map</title>
  <style>
    body { background: #1a1a1a; color: white; font-family: 'Segoe UI', sans-serif; margin: 0; padding: 20px; }
    .container { display: flex; flex-wrap: wrap; gap: 20px; max-width: 1400px; margin: 0 auto; }
    .controls { flex: 1 1 350px; min-width: 300px; }
    .map-section { flex: 2 1 600px; min-width: 300px; }
    
    h1 { text-align: center; color: #4CAF50; margin-bottom: 30px; }
    h2, h3 { color: #81C784; border-bottom: 1px solid #444; padding-bottom: 5px; }
    
    .control-group { 
      background: #2d2d2d; 
      border-radius: 10px; 
      padding: 15px; 
      margin-bottom: 15px; 
      border: 1px solid #444;
    }
    
    .direction-controls {
      display: grid;
      grid-template-columns: 1fr 1fr 1fr;
      grid-template-rows: 1fr 1fr 1fr;
      gap: 5px;
      max-width: 200px;
      margin: 0 auto;
    }
    
    .direction-controls button {
      padding: 15px;
      font-size: 18px;
      font-weight: bold;
    }
    
    /* Grid positioning for directional buttons */
    .direction-controls button:nth-child(1) { grid-column: 2; } /* Up */
    .direction-controls button:nth-child(2) { grid-column: 1; grid-row: 2; } /* Left */
    .direction-controls button:nth-child(3) { grid-column: 2; grid-row: 2; } /* Stop */
    .direction-controls button:nth-child(4) { grid-column: 3; grid-row: 2; } /* Right */
    .direction-controls button:nth-child(5) { grid-column: 2; grid-row: 3; } /* Down */
    
    input, button { 
      padding: 10px; 
      margin: 5px 0; 
      font-size: 14px; 
      border-radius: 8px; 
      border: 1px solid #555; 
      background: #3d3d3d;
      color: white;
    }
    
    button { 
      background: linear-gradient(135deg, #4CAF50, #45a049); 
      color: white; 
      cursor: pointer; 
      transition: all 0.3s ease;
      border: none;
      box-shadow: 0 2px 4px rgba(0,0,0,0.2);
    }
    
    button:hover { 
      background: linear-gradient(135deg, #45a049, #4CAF50); 
      transform: translateY(-2px);
      box-shadow: 0 4px 8px rgba(76, 175, 80, 0.3);
    }
    
    .stop-btn { background: linear-gradient(135deg, #f44336, #d32f2f) !important; }
    .test-btn { background: linear-gradient(135deg, #ff9800, #f57c00) !important; }
    .reset-btn { background: linear-gradient(135deg, #e91e63, #c2185b) !important; }
    
    .position-display { 
      background: #333; 
      padding: 15px; 
      border-radius: 8px; 
      margin: 10px 0;
      border-left: 4px solid #4CAF50;
    }
    
    .position-display div { 
      display: flex; 
      justify-content: space-between; 
      margin: 8px 0; 
      font-size: 16px;
    }
    
    .map-container {
      position: relative;
      width: 100%;
      max-width: 800px; /* Reference width for coordinate system */
      height: 600px; /* Reference height for coordinate system */
      margin: 20px auto;
      border: 2px solid #4CAF50;
      border-radius: 10px;
      overflow: hidden;
      box-shadow: 0 0 20px rgba(76,175,80,0.2);
    }
    
    .interactive-map { 
      width: 100%; 
      height: 100%; 
      position: relative;
      background: linear-gradient(90deg, rgba(76,175,80,0.1) 1px, transparent 1px),
                  linear-gradient(rgba(76,175,80,0.1) 1px, transparent 1px);
      background-size: var(--grid-size, 20px) var(--grid-size, 20px);
      background-position: var(--bg-x, 0px) var(--bg-y, 0px);
      cursor: crosshair;
      transform: scale(var(--zoom, 1)); /* Keep for potential zoom */
      transform-origin: center;
      transition: all 0.3s ease;
    }
    
    .map-controls {
      text-align: center;
      margin: 15px 0;
      display: flex;
      justify-content: center;
      align-items: center;
      gap: 15px;
      flex-wrap: wrap;
    }
    
    .ugv-dot { 
      width: 16px; 
      height: 16px; 
      background: #ff4444; 
      border-radius: 50%; 
      position: absolute;
      transform: translate(-50%, -50%); /* Center the dot on its coordinates */
      box-shadow: 0 0 15px #ff4444;
      animation: pulse 2s infinite;
      z-index: 10;
      border: 2px solid white;
    }
    
    .target-dot { /* Not directly used for spots, but good to keep for reference */
      width: 12px;
      height: 12px;
      background: #ffeb3b;
      border-radius: 50%;
      position: absolute;
      transform: translate(-50%, -50%);
      box-shadow: 0 0 10px #ffeb3b;
      z-index: 5;
      animation: glow 1.5s infinite alternate;
      border: 2px solid white;
    }
    
    .selected-spot {
      width: 10px;
      height: 10px;
      background: #2196F3;
      border-radius: 50%;
      position: absolute;
      transform: translate(-50%, -50%);
      box-shadow: 0 0 8px #2196F3;
      z-index: 4;
      border: 1px solid white;
    }

    .path-line {
        fill: none;
        stroke: #00ff00;
        stroke-width: 2;
        pointer-events: none; /* Allows clicks to pass through to the map */
    }
    
    @keyframes pulse {
      0% { box-shadow: 0 0 10px #ff4444; }
      50% { box-shadow: 0 0 25px #ff4444, 0 0 35px #ff4444; }
      100% { box-shadow: 0 0 10px #ff4444; }
    }
    
    @keyframes glow {
      0% { box-shadow: 0 0 5px #ffeb3b; }
      100% { box-shadow: 0 0 15px #ffeb3b, 0 0 20px #ffeb3b; }
    }
    
    .status-area {
      background: #2d2d2d;
      border-radius: 10px;
      padding: 15px;
      margin-top: 20px;
      border: 1px solid #444;
      max-height: 200px;
      overflow-y: auto; /* Enable scrolling for messages */
    }
    
    .status-messages {
      font-size: 13px;
      line-height: 1.4;
    }
    /* Style for manual input */
    .inline-controls {
        display: flex;
        align-items: center;
        gap: 10px;
        margin-bottom: 10px;
    }
    .inline-controls label {
        flex-shrink: 0;
        min-width: 80px;
        color: #B2FF59; /* Light green for labels */
    }
    .inline-controls input {
        flex-grow: 1;
        background: #333;
        border-color: #666;
    }
    /* Responsive adjustments */
    @media (max-width: 768px) {
        .container {
            flex-direction: column;
        }
        .controls, .map-section {
            min-width: unset;
            width: 100%;
        }
    }
  </style>
</head>
<body>
  <h1>🤖 UGV Control Center with Interactive Map</h1>
  
  <div class="container">
    <div class="controls">
      <div class="control-group">
        <h3>Manual Controls</h3>
        <div class="direction-controls">
          <button onmousedown="send('forward')" onmouseup="send('stop')" ontouchstart="send('forward')" ontouchend="send('stop')" title="Forward">↑</button>
          <button onmousedown="send('left')" onmouseup="send('stop')" ontouchstart="send('left')" ontouchend="send('stop')" title="Turn Left">←</button>
          <button onclick="send('stop')" class="stop-btn" title="Stop">⬛</button>
          <button onmousedown="send('right')" onmouseup="send('stop')" ontouchstart="send('right')" ontouchend="send('stop')" title="Turn Right">→</button>
          <button onmousedown="send('backward')" onmouseup="send('stop')" ontouchstart="send('backward')" ontouchend="send('stop')" title="Backward">↓</button>
        </div>
      </div>

      <div class="control-group">
        <h3>Position Info</h3>
        <div class="position-display">
          <div><span>X Position:</span><span id="pos-x">0.00 cm</span></div>
          <div><span>Y Position:</span><span id="pos-y">0.00 cm</span></div>
          <div><span>Angle:</span><span id="pos-angle">0.0°</span></div>
          <div><span>State:</span><span id="motor-state">stop</span></div>
          <div><span>RPM:</span><span id="rpm-value">0.00</span></div>
        </div>
      </div>

      <div class="control-group">
        <h3>Test & Calibration</h3>
        <button onclick="runTest()" class="test-btn">🔧 Run 2s Test</button>
        <div style="margin-top: 15px;">
          <div class="inline-controls">
            <label for="distance">Distance (cm):</label>
            <input type="number" id="distance" placeholder="20" step="0.1">
          </div>
          <div class="inline-controls">
            <label for="circumference">Circumference (cm):</label>
            <input type="number" id="circumference" placeholder="18.85" step="0.1">
          </div>
          <button onclick="calculateRPM()" style="width: 100%; margin-top: 10px;">📊 Calculate RPM</button>
        </div>
      </div>

      <div class="control-group">
        <h3>Route Planning</h3>
        <!-- Manual X,Y input -->
        <div class="inline-controls">
            <label for="manualX">Manual X:</label>
            <input type="number" id="manualX" placeholder="0.0" step="0.1">
        </div>
        <div class="inline-controls">
            <label for="manualY">Manual Y:</label>
            <input type="number" id="manualY" placeholder="0.0" step="0.1">
        </div>
        <button onclick="addManualSpot()" style="width: 100%; margin-top: 5px;">➕ Add Manual Spot</button>
        <!-- End Manual X,Y input -->
        <div id="spot-list"></div>
        <button onclick="clearSpots()" style="width: 100%; margin-top: 10px;">🧹 Clear All Spots</button>
        <button onclick="allocateAndGo()" style="width: 100%; margin-top: 10px;">🎯 Allocate & Go!</button>
      </div>

      <div class="control-group">
        <h3>System Controls</h3>
        <button onclick="resetPosition()" class="reset-btn" style="width: 100%;">🔄 Reset Position</button>
      </div>
    </div>

    <div class="map-section">
      <h2>🗺️ Interactive Map</h2>
      <div class="map-controls">
        <span style="color: #4CAF50;">Click on the map to add target spots.</span>
        <span id="cursor-coords" style="color: #e0e0e0; margin-left: 20px;">Cursor: (X: 0.00, Y: 0.00)</span>
      </div>
      
      <div class="map-container">
        <div class="interactive-map" id="interactive-map">
          <svg id="map-svg" width="100%" height="100%" style="position: absolute; top: 0; left: 0; pointer-events: none;">
              <polyline id="path-line" class="path-line"></polyline>
          </svg>
          <div class="ugv-dot" id="ugv-dot"></div>
        </div>
      </div>
      
      <div class="status-area">
        <h4 style="margin: 0 0 10px 0; color: #4CAF50;">📋 System Status:</h4>
        <div id="status-messages" class="status-messages">
          🚀 System initialized. Click on the map to set points.
        </div>
      </div>
    </div>
  </div>

  <script>
    let ugvSpots = [];
    // Update position every 100ms for smoother path drawing
    let trackingInterval = setInterval(updatePosition, 100); 
    let pathPoints = [];
    let lastPosition = {x: 0, y: 0}; // Track last known UGV position for path drawing
    
    const map = document.getElementById('interactive-map');
    const mapSvg = document.getElementById('map-svg');
    const pathLine = document.getElementById('path-line');
    const spotListDiv = document.getElementById('spot-list');
    const cursorCoordsSpan = document.getElementById('cursor-coords');

    const baseScale = 4; // 1cm = 4px for better visibility
    // These define the conceptual 0,0 center for CM to pixel conversion relative to map container
    const mapWidth = 800; 
    const mapHeight = 600; 
    const centerX = mapWidth / 2;
    const centerY = mapHeight / 2;

    /**
     * @brief Sends a control command to the UGV's web server.
     * @param cmd The command string (e.g., 'forward', 'stop', 'left').
     */
    function send(cmd) {
      fetch('/control?cmd=' + cmd)
        .then(res => res.json())
        .then(data => console.log(data))
        .catch(error => { // Add specific error handling for fetch
            console.error("Error sending command:", error);
            addStatusMessage("⚠️ Command failed: " + cmd, 'error');
        });
    }

    /**
     * @brief Adds a status message to the UI's status area.
     * @param message The message to display.
     * @param type The type of message ('info', 'success', 'warning', 'error').
     */
    function addStatusMessage(message, type = 'info') {
      const statusDiv = document.getElementById('status-messages');
      const timestamp = new Date().toLocaleTimeString();
      const icons = { success: '✅', error: '❌', info: 'ℹ️', warning: '⚠️' };
      const colors = { success: '#4CAF50', error: '#ff4444', info: '#2196F3', warning: '#ff9800' };
      
      statusDiv.innerHTML += `<div style="color: ${colors[type] || '#ffffff'}; margin: 3px 0;">${icons[type] || 'ℹ️'} [${timestamp}] ${message}</div>`;
      statusDiv.scrollTop = statusDiv.scrollHeight; // Auto-scroll to bottom
    }

    /**
     * @brief Sends a request to the UGV to run a 2-second forward test.
     */
    function runTest() {
      addStatusMessage("🔧 Running 2-second forward test...", 'info');
      fetch('/test')
        .then(r => r.text())
        .then(data => addStatusMessage('✅ Test completed: ' + data, 'success'))
        .catch(error => {
            console.error("Error running test:", error);
            addStatusMessage("❌ Test failed", 'error');
        });
    }

    /**
     * @brief Calculates the UGV's RPM based on a measured distance and wheel circumference.
     * Sends the calibration data to the ESP32.
     */
    function calculateRPM() {
      const distance = document.getElementById('distance').value;
      const circumference = document.getElementById('circumference').value;
      
      if (!distance || !circumference) {
        addStatusMessage('❌ Please enter both distance and circumference values', 'error');
        return;
      }

      const formData = new FormData();
      formData.append('distance', distance);
      formData.append('circumference', circumference);

      fetch('/calculate', { method: 'POST', body: formData })
      .then(res => res.text())
      .then(data => addStatusMessage('📊 ' + data, 'success'))
      .catch(error => {
          console.error("Error calculating RPM:", error);
          addStatusMessage('❌ RPM calculation failed', 'error');
      });
    }

    /**
     * @brief Resets the UGV's position and orientation on the ESP32 and clears the UI path.
     */
    function resetPosition() {
      fetch('/reset_position')
        .then(res => res.text())
        .then(data => {
          addStatusMessage('🔄 ' + data, 'success');
          pathPoints = []; // Clear path points on UI reset
          // Immediately get current position (should be 0,0) to start new path from there
          fetch('/position')
              .then(res => res.json())
              .then(data => {
                  lastPosition = {x: data.x, y: data.y};
                  pathPoints.push({x: data.x, y: data.y}); // Add current 0,0 as start of new path
                  updatePathLine();
                  updateUGVPosition(data.x, data.y);
              })
              .catch(error => console.error('Error fetching initial position after reset:', error));
        })
        .catch(error => {
            console.error("Error resetting position:", error);
            addStatusMessage('❌ Reset failed', 'error');
        });
    }

    /**
     * @brief Fetches the UGV's current position from the ESP32 and updates the UI.
     * Also adds points to the path line for visualization.
     */
    function updatePosition() {
      fetch('/position')
        .then(res => res.json())
        .then(data => {
          document.getElementById('pos-x').textContent = data.x.toFixed(2) + ' cm';
          document.getElementById('pos-y').textContent = data.y.toFixed(2) + ' cm';
          document.getElementById('pos-angle').textContent = data.angle.toFixed(1) + '°';
          document.getElementById('motor-state').textContent = data.state;
          document.getElementById('rpm-value').textContent = data.rpm.toFixed(2);
          
          updateUGVPosition(data.x, data.y);
          
          // Only add a new point to the path if the position has significantly changed
          // or if it's the very first point after a reset.
          if (pathPoints.length === 0 || Math.abs(data.x - lastPosition.x) > 0.5 || Math.abs(data.y - lastPosition.y) > 0.5) {
            pathPoints.push({x: data.x, y: data.y});
            updatePathLine();
            lastPosition = {x: data.x, y: data.y};
          }
        })
        .catch(error => {
            // Log the error but don't spam the status messages too much for continuous polling
            console.log('Position update failed:', error); 
        });
    }

    /**
     * @brief Updates the visual position of the UGV dot on the interactive map.
     * @param x The UGV's X coordinate in cm.
     * @param y The UGV's Y coordinate in cm.
     */
    function updateUGVPosition(x, y) {
      const ugvDot = document.getElementById('ugv-dot');
      // Convert world (cm) coordinates to screen (pixel) coordinates
      const screenX = centerX + (x * baseScale);
      const screenY = centerY - (y * baseScale); // Flip Y to match map's Cartesian plane (Y increases upwards)
      
      ugvDot.style.left = screenX + 'px';
      ugvDot.style.top = screenY + 'px';
    }
    
    /**
     * @brief Updates the SVG polyline element to draw the UGV's path.
     */
    function updatePathLine() {
      // Convert all path points from world (cm) to screen (pixel) coordinates
      const pointsStr = pathPoints.map(p => `${centerX + p.x * baseScale},${centerY - p.y * baseScale}`).join(' ');
      pathLine.setAttribute('points', pointsStr);
    }
    
    /**
     * @brief Clears existing target dots from the map and redraws all selected spots.
     */
    function updateSpotDots() {
        const existingDots = map.querySelectorAll('.selected-spot');
        existingDots.forEach(dot => dot.remove());

        ugvSpots.forEach(spot => {
            const screenX = centerX + (spot.x * baseScale);
            const screenY = centerY - (spot.y * baseScale);
            
            const dot = document.createElement('div');
            dot.className = 'selected-spot';
            dot.style.left = screenX + 'px';
            dot.style.top = screenY + 'px';
            
            map.appendChild(dot);
        });
    }

    /**
     * @brief Updates the list of selected spots displayed in the Route Planning section.
     */
    function updateSpotList() {
        spotListDiv.innerHTML = '<h4>Selected Spots:</h4>';
        if (ugvSpots.length === 0) {
            spotListDiv.innerHTML += '<p>No spots selected yet. Click on the map!</p>';
            return;
        }
        
        ugvSpots.forEach((spot, index) => {
            spotListDiv.innerHTML += `<div>Spot ${index + 1}: (${spot.x.toFixed(1)}, ${spot.y.toFixed(1)})</div>`;
        });
    }

    // Initialize path with current position on load
    document.addEventListener('DOMContentLoaded', () => {
        updatePosition(); // Initial fetch to set UGV dot and populate initial path point
    });

    /**
     * @brief Event listener for mouse movement over the map to display cursor coordinates.
     */
    map.addEventListener('mousemove', (event) => {
        const rect = map.getBoundingClientRect();
        // Calculate mouse position relative to the map container's top-left corner
        const x = event.clientX - rect.left;
        const y = event.clientY - rect.top;
        
        // Convert pixel coordinates to world (cm) coordinates
        const worldX = (x - centerX) / baseScale;
        const worldY = -(y - centerY) / baseScale; // Flip Y to match map's Cartesian plane (Y increases upwards)
        
        cursorCoordsSpan.textContent = `Cursor: (X: ${worldX.toFixed(2)}, Y: ${worldY.toFixed(2)})`;
    });

    /**
     * @brief Event listener for clicks on the map to add new target spots.
     */
    map.addEventListener('click', (event) => {
      const rect = map.getBoundingClientRect();
      const x = event.clientX - rect.left;
      const y = event.clientY - rect.top;
      
      const worldX = (x - centerX) / baseScale;
      const worldY = -(y - centerY) / baseScale; // Flip Y to match map
      
      ugvSpots.push({x: worldX, y: worldY});
      addStatusMessage(`📍 Added spot (click): (${worldX.toFixed(1)}, ${worldY.toFixed(1)})`);
      updateSpotDots();
      updateSpotList();
    });

    /**
     * @brief Adds a target spot using manually entered X and Y coordinates.
     */
    function addManualSpot() {
        const manualXInput = document.getElementById('manualX');
        const manualYInput = document.getElementById('manualY');
        
        const x = parseFloat(manualXInput.value);
        const y = parseFloat(manualYInput.value);

        if (isNaN(x) || isNaN(y)) {
            addStatusMessage('❌ Please enter valid numbers for Manual X and Y.', 'error');
            return;
        }

        ugvSpots.push({x: x, y: y});
        addStatusMessage(`📍 Added spot (manual): (${x.toFixed(1)}, ${y.toFixed(1)})`);
        updateSpotDots();
        updateSpotList();
        manualXInput.value = ''; // Clear inputs after adding
        manualYInput.value = '';
    }

    /**
     * @brief Clears all selected target spots from the UI list and map.
     */
    function clearSpots() {
      ugvSpots = [];
      updateSpotDots();
      updateSpotList();
      addStatusMessage('🧹 All spots cleared from list.', 'info');
    }

    /**
     * @brief Sends the list of selected target spots to the UGV for autonomous navigation.
     */
    function allocateAndGo() {
      if (ugvSpots.length === 0) {
        addStatusMessage('⚠️ No spots selected to allocate.', 'warning');
        return;
      }
      
      // Convert spots array to a comma-separated string for easy parsing on ESP32
      const spotsStr = ugvSpots.map(spot => `${spot.x.toFixed(2)} ${spot.y.toFixed(2)}`).join(',');
      
      const formData = new FormData();
      formData.append('spots', spotsStr);
      
      addStatusMessage(`🎯 Sending ${ugvSpots.length} spots to UGV...`, 'info');
      
      fetch('/allocate_and_go', {
        method: 'POST',
        body: formData
      })
      .then(res => res.text())
      .then(data => {
        addStatusMessage(`✅ UGV allocation and movement initiated: ${data}`, 'success');
        clearSpots(); // Clear the UI list after sending, as UGV is now processing them
      })
      .catch(error => {
          console.error("Error sending spots to UGV:", error);
          addStatusMessage('❌ Failed to send spots to UGV.', 'error');
      });
    }

  </script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

/**
 * @brief Handles /control requests from the web client to command motor movements.
 */
void handleControl() {
  if (server.hasArg("cmd")) {
    String cmd = server.arg("cmd");
    if (cmd == "forward") moveForward();
    else if (cmd == "backward") moveBackward();
    else if (cmd == "left") turnLeft();
    else if (cmd == "right") turnRight();
    else if (cmd == "stop") stopMotors();
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(400, "text/plain", "Missing command");
  }
}

/**
 * @brief Handles /position requests to send the UGV's current estimated position and state to the web client.
 */
void handlePosition() {
  updatePosition(); // Ensure the internal position is up-to-date before sending
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

/**
 * @brief Handles /reset_position requests to reset the UGV's internal position (x,y,angle) to zero.
 */
void handleResetPosition() {
  current_x = 0.0;
  current_y = 0.0;
  current_angle = 0.0;
  stopMotors(); // Ensure motors are stopped when position is reset
  server.send(200, "text/plain", "Position reset to (0,0,0deg).");
}

/**
 * @brief Handles /allocate_and_go POST requests. Receives a string of coordinates,
 * parses them, sorts them using Nearest Neighbor, and then commands the UGV
 * to visit each point in sequence. This is a blocking operation.
 */
void handleAllocateAndGo() {
  if (server.hasArg("spots")) {
    String spot_str = server.arg("spots");
    ugv_spots_to_visit.clear(); // Clear previous spots from the internal list

    int start = 0;
    while (start < spot_str.length()) {
      int end = spot_str.indexOf(',', start);
      if (end == -1) end = spot_str.length(); // Handle the last pair
      String pair = spot_str.substring(start, end);
      
      // Find the space separator within the "x y" pair
      int space_idx = pair.indexOf(' ');
      if (space_idx != -1) {
        float x = pair.substring(0, space_idx).toFloat();
        float y = pair.substring(space_idx + 1).toFloat();
        ugv_spots_to_visit.push_back({x, y});
      } else {
        Serial.println("Warning: Malformed spot pair received: " + pair);
      }
      start = end + 1;
    }
    
    // Send immediate response to the client so UI can update to show "journey initiated"
    server.send(200, "text/plain", "Allocation received. Starting journey.");

    // Create a copy for sorting, as sortSpotsByNearestNeighbor modifies the vector
    std::vector<Point> spots_to_sort = ugv_spots_to_visit;
    Point start_pos = {current_x, current_y}; // Use actual current position as start for sorting
    sortSpotsByNearestNeighbor(start_pos, spots_to_sort);

    // Now, iterate through the sorted spots and command the UGV to go to each one
    for (const auto& spot : spots_to_sort) {
      Serial.printf("Navigating to spot: (%.2f, %.2f)\n", spot.x, spot.y);
      gotoPosition(spot.x, spot.y);
      // The current_x and current_y globals are updated by gotoPosition.
    }
    
    Serial.println("Journey complete! All spots visited.");

  } else {
    server.send(400, "text/plain", "Missing spot coordinates in request.");
  }
}

/**
 * @brief Arduino setup function. Initializes serial, motor pins, WiFi, and web server routes.
 */
void setup() {
  Serial.begin(115200);
  
  // Configure motor driver pins as outputs
  pinMode(MOTOR_A_IN1, OUTPUT);
  pinMode(MOTOR_A_IN2, OUTPUT);
  pinMode(MOTOR_A_ENA, OUTPUT); // Enable pin for motor A (PWM)
  pinMode(MOTOR_B_IN1, OUTPUT);
  pinMode(MOTOR_B_IN2, OUTPUT);
  pinMode(MOTOR_B_ENB, OUTPUT); // Enable pin for motor B (PWM)
  stopMotors(); // Ensure motors are off at startup

  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { // Try for max 10 seconds
    delay(500); 
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nFailed to connect to WiFi. Please check credentials and try again.");
    Serial.println("Falling back to soft AP mode (if supported by ESP32 board).");
    // Optionally, you can enable soft AP here if WiFi connection fails
    // WiFi.softAP("ESP32_UGV", "12345678");
    // Serial.println("Soft AP IP: " + WiFi.softAPIP().toString());
  }

  // Define web server endpoints
  server.on("/", handleRoot);
  server.on("/control", handleControl);
  server.on("/position", handlePosition);
  server.on("/reset_position", handleResetPosition);
  
  // Custom endpoint for motor test (forward 2 seconds)
  server.on("/test", []() {
    Serial.println("Received /test command. Moving forward for 2 seconds.");
    moveForward(); 
    delay(2000); // Blocking delay for test
    stopMotors();
    server.send(200, "text/plain", "Test complete.");
  });

  // Custom endpoint to calculate and set RPM based on test distance/circumference
  server.on("/calculate", HTTP_POST, []() {
    if (server.hasArg("distance") && server.hasArg("circumference")) {
      float d = server.arg("distance").toFloat();
      wheel_circumference = server.arg("circumference").toFloat();
      // Calculate RPM assuming the 'test' moved 'd' distance in 2 seconds
      // RPM = (distance / circumference) / (time_in_seconds / 60)
      rpm = (d / wheel_circumference) * (60.0 / 2.0); // (rotations / second) * 60 = RPM
      Serial.printf("Calculated RPM = %.2f (distance=%.2f cm, circumference=%.2f cm)\n", rpm, d, wheel_circumference);
      server.send(200, "text/plain", "RPM = " + String(rpm, 2) + " (Circ: " + String(wheel_circumference, 2) + "cm)");
    } else {
      server.send(400, "text/plain", "Missing distance or circumference in /calculate request.");
    }
  });

  // Endpoint for multi-point allocation and autonomous navigation
  server.on("/allocate_and_go", HTTP_POST, handleAllocateAndGo);

  // Start the web server
  server.begin();
  Serial.println("HTTP server started.");
}

/**
 * @brief Arduino loop function. Continuously handles incoming web client requests.
 */
void loop() {
  server.handleClient();
  // The updatePosition() function is now primarily triggered by client requests
  // (e.g., when /position is polled) or directly before motor commands.
  // This avoids redundant updates in the main loop if no movement is commanded.
}
