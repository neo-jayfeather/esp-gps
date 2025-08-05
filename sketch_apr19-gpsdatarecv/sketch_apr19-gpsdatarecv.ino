#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <FS.h> 

const char* ssid = "google wifi 1";
const char* password = "oseldiam";

WebServer server(80);

struct miniGPSdata{//incoming data
  float averageSpeed = 0.0; //4 by
  float totalDist = 0.0; //4 by
  uint32_t totalTime = 0; //4 by? 
  uint64_t mac = 0x000000000000; //8 bytes - only 6 are used
};
miniGPSdata allGPSdata[10]; 
struct GPSdata {
  float latitude = 0.0;
  float longitude = 0.0;
  float altitude = 0.0;
  float speed = 0.0;
  int satellites = 0;
  String timestamp = "N/A";
  String status_message = "Waiting for GPS fix...";
};

GPSdata currentGpsData; // Global variable to hold GPS data
void initGPSdata(){
  allGPSdata[1].mac = 0xDC0675A981B8;
  allGPSdata[2].mac = 0xDC0675F9829C;
  allGPSdata[3].mac = 0x28372F559210;
  allGPSdata[4].mac = 0xDC0675ABE504;
  allGPSdata[5].mac = 0x94A99097BB9C;
  allGPSdata[6].mac = 0x94A99095F8A8;
}

void handleGpsData() {
  String json = "{";
  json += "\"latitude\":" + String(currentGpsData.latitude, 6);
  json += ",\"longitude\":" + String(currentGpsData.longitude, 6);
  json += ",\"altitude\":" + String(currentGpsData.altitude, 2);
  json += ",\"speed\":" + String(currentGpsData.speed, 2);
  json += ",\"satellites\":" + String(currentGpsData.satellites);
  json += ",\"timestamp\":\"" + currentGpsData.timestamp + "\"";
  json += ",\"status_message\":\"" + currentGpsData.status_message + "\"";
  json += "}";
  server.send(200, "application/json", json);
}
void handleWifiStatus() {
  String status = "UNKNOWN";
  String ip_address = "N/A";
  String status_message = "Not connected";

  if (WiFi.status() == WL_CONNECTED) {
    status = "CONNECTED";
    ip_address = WiFi.localIP().toString();
    status_message = "Connected to " + WiFi.SSID();
  } else {
    status = "DISCONNECTED";
    status_message = "Wi-Fi disconnected. Status code: " + String(WiFi.status());
  }

  String json = "{";
  json += "\"status\":\"" + status + "\"";
  json += ",\"ip_address\":\"" + ip_address + "\"";
  json += ",\"status_message\":\"" + status_message + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  initGPSdata();
  Serial.begin(115200); //default debug baud rate

  if (!LittleFS.begin(true)) { //littlefs startup
    Serial.println("filesystem mount error");
    return;
  }

  Serial.printf("Connecting to WiFi: %s \n", ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("Connected - IP address: ");
  Serial.println(WiFi.localIP()); //prints ip
  //192.168 on home, is currently dynamic, 192.168.4.1 on own network
  // Route to serve the uncompressed HTML file from LittleFS
  server.on("/", HTTP_GET, []() {
    if(LittleFS.exists("/index.html")) {
      File file = LittleFS.open("/index.html", "r");
      server.streamFile(file, "text/html");
      file.close();
    }else{
      server.send(404, "text/plain", "File not found");
    }
  });

  //server.on("/api/gps-data", handleGpsData); - implementation later
  server.on("/api/wifi-status", handleWifiStatus);
  server.begin();
  Serial.println("server started");

  // --- Initialize GPS module here (example, replace with actual code) ---
  //currentGpsData.latitude = 34.0522;
  //currentGpsData.longitude = -118.2437;
  //currentGpsData.altitude = 100.0;
  //currentGpsData.speed = 10.5;
  //currentGpsData.satellites = 7;
  //currentGpsData.timestamp = "2025-07-16T02:30:00Z";
  //urrentGpsData.status_message = "GPS data initialized (dummy)";
}

void loop() {
  server.handleClient();
  // --- Update GPS data here (example, replace with actual code) ---
}
