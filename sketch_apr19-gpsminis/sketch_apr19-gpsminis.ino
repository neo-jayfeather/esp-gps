// 4/17/2025

// ESP32 board: "ESP-WROOM-32 ESP32 ESP-32S Development Board"
// https://www.amazon.com/ESP-WROOM-32-Development-Microcontroller-Integrated-Compatible/dp/B08D5ZD528
// It's ESP-32S. Not ESP-32S3 or EPS32S2!
// select "ESP32 Dev Module"
// installed from https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

// GPS board: "GY-NEO6MV2 NEO-6M GPS Module"
// https://www.amazon.com/dp/B0B49LB18G

// GPS chip: https://content.u-blox.com/sites/default/files/products/documents/NEO-6_DataSheet_%28GPS.G6-HW-09005%29.pdf

// Connection:
// GPS VCC <=> ESP32 3V3
// GPS GND <=> ESP32 GND
// GPS RX  <=> ESP32 TX2
// GPS TX  <=> ESP32 RX2

#include <HardwareSerial.h>  //serial connections
#include <TinyGPS++.h>       //gps nmea sentence parsing
//#include <math.h>            //math
#include <arduino.h>         //math
#include <Preferences.h>     //for flash storage
#include <esp_now.h>         // to send data to a server
#include <WiFi.h>            //for espnow

TinyGPSPlus gps;                              //gps
Preferences preferences;                      //flash
HardwareSerial GPS_Serial(2);                 // Use UART2
int cycle = 0;                                //cycle for averaging (maybe use int if hz > 2)
double currAvgPos[6] = {0, 0, 0, 0, 0, 0};  // first two are for first pos, next two are for second pos, last two are for average including third pos
double pastAvgPos[2] = {0, 0};
double pastPos[2] = {0, 0};
double savedDist = 0;
bool reachLoop = false;
int gpsConnection = 0;
bool printLoc = false;
int gpsWaitTimes[2] = {500, 500};
int lastUpdateTime = 0;

uint8_t broadcastAddress[] = {0xC8, 0x2E, 0x18, 0xF8, 0x50, 0xE0};  //c8:2e:18:f8:50:e0
typedef struct struct_message {
  unsigned char a[6];
  double b;
} struct_message;
struct_message myData;
esp_now_peer_info_t peerInfo;

//BOTH INCLUDE CHECKSUM
const unsigned char changeHzMsg[] = {0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xC8, 0x00, 0x01, 0x00, 0x01, 0x00, 0xDE, 0x6A}; //14 bytes, to 5 hz
const unsigned char setBaud1152[] = {0xB5, 0x62, 0x06, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0xD0, 0x08, 0x00, 0x00, 0x00, 0xC2, 0x01, 0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC4, 0x96 };// to 115200 baud

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {//keep this around heheh!
  //Serial.print("\r\nLast Packet Send Status:\t");
  //Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail"); // switch to light indicator?
}
void setGPSsettings() {                                // sets gps
  Serial.print("wt0 - ");
  Serial.print(gpsWaitTimes[0]);
  Serial.print(" wt1 - ");
  Serial.println(gpsWaitTimes[1]);
  GPS_Serial.begin(9600, SERIAL_8N1, 16, 17);          // Initialize at the current baud rate - should always be 9600 at startup
  GPS_Serial.write(setBaud1152, sizeof(setBaud1152));  //sends message w/ checksum to change baud rate from 9600 -> 115200
  delay(gpsWaitTimes[0]);                              // Important: Give the GPS module time to change baud rates, DO NOT REMOVE
  GPS_Serial.end();                                    // closes serial at 9600
  GPS_Serial.begin(115200, SERIAL_8N1, 16, 17);        // opens serial at 115200
  GPS_Serial.write(changeHzMsg, sizeof(changeHzMsg));  //sends message w/ checksum to change refresh rate from 1hz --> 5hz
  delay(gpsWaitTimes[1]);                              // Small delay to allow processing, DO NOT REMOVE
}
void printData() {
  Serial.print("Latitude: ");
  Serial.print(gps.location.lat(), 6);
  Serial.print("  |  Longitude: ");
  Serial.print(gps.location.lng(), 6);
  Serial.print("  |  Satellites: ");
  Serial.print(gps.satellites.value());
  Serial.print("  |  Total Dist: ");
  Serial.println(savedDist, 5);
}
void sendData() {
  unsigned char tempMac[6] = { 0x30, 0xC9, 0x22, 0x31, 0xB8, 0xA4 };
  memcpy(myData.a, tempMac, sizeof(myData.a));
  myData.b = savedDist;
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));  // Send message via ESP-NOW
  //if (result == ESP_OK) {
  //  Serial.println("Sent with success");
  //}else{
  //  Serial.println("Error sending the data");
  //}
}
void setup() {
  Serial.begin(115200);  // between ESP32 and PC

  WiFi.mode(WIFI_STA);  //espnow
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  // Once ESPNow is successfully Init, we will register for Send CB to get the status of Trasnmitted packet
  //esp_now_register_send_cb(OnDataSent); //enable later

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);  //Register Peer
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {  //Add peer
    Serial.println("Failed to add peer");
    return;
  }

  preferences.begin("neo-gps", false);                                       //starts storage, flase is for RW mode
  savedDist = preferences.getDouble("savedDist", static_cast<double>(0.0));  // retreives distance from flash, otherwise sets it as a (float/double?)
  gpsWaitTimes[0] = preferences.getInt("waitTime0", static_cast<int>(500));
  gpsWaitTimes[1] = preferences.getInt("waitTime1", static_cast<int>(500));
  setGPSsettings();  //why am I saving distance as a double (gotta get that 1e-9km [nm] i guess?)
}

double toRads(double degrees) {
  return degrees * PI / 180;  //pi is a (double?) defined by arduino
}
double haversine(double lat1, double lng1, double lat2, double lng2, double earthRadius) {
  double lat1Rad = toRads(lat1);  //latitude/longitude to radians
  double lng1Rad = toRads(lng1);
  double lat2Rad = toRads(lat2);
  double lng2Rad = toRads(lng2);

  double dLng = lng2Rad - lng1Rad;  //difference
  double dLat = lat2Rad - lat1Rad;

  double a = pow(sin(dLat / 2.0), 2) + cos(lat1Rad) * cos(lat2Rad) * pow(sin(dLng / 2.0), 2);  //sin(dLat/2)^2 + cos(lat1) + cos(lat2) + sin(dLng/2)^2
  double c = 2 * asin(sqrt(a));                                                                // arcsin(sqrt(a))*2
  return earthRadius * c;                                                                      //total distance (delta)
}

void updatePos() {  //updates position vars
  if (cycle == 0) {
    currAvgPos[0] = gps.location.lat();
    currAvgPos[1] = gps.location.lng();
  } else if (cycle == 1) {
    currAvgPos[2] = gps.location.lat();
    currAvgPos[3] = gps.location.lng();
  } else if (cycle == 2) {
    currAvgPos[4] = (currAvgPos[0] + currAvgPos[2] + gps.location.lat()) / 3;
    currAvgPos[5] = (currAvgPos[1] + currAvgPos[3] + gps.location.lng()) / 3;
    if (pastAvgPos[0] != 0 && pastAvgPos[1] != 0) {                                                    //so that you do not calculate distance from (0,0)
      double deltaDist = haversine(pastAvgPos[0], pastAvgPos[1], currAvgPos[4], currAvgPos[5], 6371);  //6371 is the thing used by gps so we use it too!
      if (deltaDist > 0.0016764) {
        savedDist += deltaDist;
      }
      preferences.putDouble("savedDist", savedDist);  //save this number into flash!
    }
    pastAvgPos[0] = currAvgPos[4];
    pastAvgPos[1] = currAvgPos[5];
  }
  ++cycle;
  if (gpsConnection <= 5) {
    --cycle;
  }
  cycle = cycle % 3;
}
void loop() {
  if (reachLoop == false) {
    Serial.println(millis());
    reachLoop = true;
  }
  while (GPS_Serial.available()) {
    ++gpsConnection;
    gps.encode(GPS_Serial.read());
    if(gps.location.isUpdated() && (millis()-lastUpdateTime) > 50){ //prevent double update/debouncing
      updatePos();
      if (printLoc == true) {
        printData();
      }
      sendData();
      lastUpdateTime = millis();
    }
  }
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.equalsIgnoreCase("updateLocation")) {
      printLoc = true;
      Serial.println("Received command: updateLocation. printLoc set to true.");
    } else if (strncmp(command.c_str(), "set waitTime", 12) == 0) {
      char serialBuffer[100];
      strcpy(serialBuffer, command.c_str());    // Copy the String to a buffer
      char *token = strtok(serialBuffer, " ");  // Use strtok
      token = strtok(NULL, " ");                // Get the variable name ("waitTime0")
      char *valueString = strtok(NULL, " ");    // Get the value string ("100")

      if (token != NULL && valueString != NULL) {
        int value = atoi(valueString);
        preferences.putInt(token, value);  // Use the token (variable name)=
        Serial.printf("Set %s to %d \n", token, value);
      } else {
        Serial.println("Error: Invalid set waitTime command format");
      }
    } else if (strncmp(command.c_str(), "print", 5) == 0) {
      // stop waitTime0
      char serialBuffer[100];
      strcpy(serialBuffer, command.c_str());
      char *token = strtok(serialBuffer, " ");  // Use strtok
      token = strtok(NULL, " ");                // Get the variable name

      if (token != NULL) {
        int storedValue = preferences.getInt(token);
        Serial.printf("Value of %s is %d", token, storedValue);
      }else{
        Serial.println("Error: Invalid print command.");
      }
    } else if (command.equalsIgnoreCase("stop")) {
      printLoc = false;
      Serial.println("stopping printing");
    } else {
      Serial.printf("Unknown command: %s", command);
    }
  }
  // Prevent watchdog reset if GPS data isn't coming in
  delay(10);  // or use yield(); i have no idea what this does hehe
}

// expected location:
// Latitude: 39.206317, Longitude: -76.931592