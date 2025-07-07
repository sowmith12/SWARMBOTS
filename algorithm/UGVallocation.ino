#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "apple";
const char* password = "9966809332";

WebServer server(80);

String assignmentResult = "";

struct Point {
  float x;
  float y;
};

float distance(Point a, Point b) {
  return sqrt(pow(a.x - b.x, 2) + pow(a.y - b.y, 2));
}

float totalDistance(Point start, Point spots[], int n) {
  if (n == 0) return 0;
  float dist = distance(start, spots[0]);
  for (int i = 1; i < n; i++) {
    dist += distance(spots[i - 1], spots[i]);
  }
  return dist;
}

void allocateUGVs(Point ugvs[], int ugv_count, Point spots[], int spot_count) {
  int bestAssignment[spot_count];
  float bestMaxTime = 999999;

  int totalComb = pow(ugv_count, spot_count);
  for (int mask = 0; mask < totalComb; mask++) {
    int assignment[spot_count];
    int tmp = mask;
    for (int i = 0; i < spot_count; i++) {
      assignment[i] = tmp % ugv_count;
      tmp /= ugv_count;
    }

    Point assigned[ugv_count][spot_count];
    int count[ugv_count] = {0};

    for (int i = 0; i < spot_count; i++) {
      int ugv = assignment[i];
      assigned[ugv][count[ugv]++] = spots[i];
    }

    float max_time = 0;
    for (int i = 0; i < ugv_count; i++) {
      float t = totalDistance(ugvs[i], assigned[i], count[i]);
      if (t > max_time) max_time = t;
    }

    if (max_time < bestMaxTime) {
      bestMaxTime = max_time;
      memcpy(bestAssignment, assignment, sizeof(bestAssignment));
    }
  }

  assignmentResult = "<h3>Spot Assignments:</h3>";
  for (int i = 0; i < ugv_count; i++) {
    assignmentResult += "<b>UGV " + String(i+1) + ":</b> ";
    for (int j = 0; j < spot_count; j++) {
      if (bestAssignment[j] == i) {
        assignmentResult += "(" + String(spots[j].x) + "," + String(spots[j].y) + ") ";
      }
    }
    assignmentResult += "<br>";
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><body><h2>UGV Allocation</h2>\
    <form action=\"/submit\" method=\"get\">\
    UGVs (comma-separated x y): <input name=\"ugvs\" value=\"0 0,10 10\"><br><br>\
    Spots (comma-separated x y): <input name=\"spots\" value=\"4 4,6 6,20 20\"><br><br>\
    <input type=\"submit\" value=\"Allocate\">\
    </form><hr>" + assignmentResult + "</body></html>";
  server.send(200, "text/html", html);
}

void handleSubmit() {
  if (server.hasArg("ugvs") && server.hasArg("spots")) {
    String ugv_str = server.arg("ugvs");
    String spot_str = server.arg("spots");

    std::vector<Point> ugvs;
    int start = 0;
    while (start < ugv_str.length()) {
      int end = ugv_str.indexOf(',', start);
      if (end == -1) end = ugv_str.length();
      String pair = ugv_str.substring(start, end);
      float x = pair.substring(0, pair.indexOf(' ')).toFloat();
      float y = pair.substring(pair.indexOf(' ') + 1).toFloat();
      ugvs.push_back({x, y});
      start = end + 1;
    }

    std::vector<Point> spots;
    start = 0;
    while (start < spot_str.length()) {
      int end = spot_str.indexOf(',', start);
      if (end == -1) end = spot_str.length();
      String pair = spot_str.substring(start, end);
      float x = pair.substring(0, pair.indexOf(' ')).toFloat();
      float y = pair.substring(pair.indexOf(' ') + 1).toFloat();
      spots.push_back({x, y});
      start = end + 1;
    }

    allocateUGVs(ugvs.data(), ugvs.size(), spots.data(), spots.size());
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected! ESP32 IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/submit", handleSubmit);

  server.begin();
}

void loop() {
  server.handleClient();
}
