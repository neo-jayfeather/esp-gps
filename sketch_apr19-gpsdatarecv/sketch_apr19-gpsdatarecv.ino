#include <esp_now.h>
#include <WiFi.h>
const char* ssid = "chad boothe's duck-fi";
const char* ssid = "java is a bad language";

const unsigned char acceptedMacs[8][6] = {
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // maybe just as one number? 
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // make sure the assigned numbers are correct!!!!
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ahahahahahahaa
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // this is gonna be shit
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};
// json parsing
//{
//   "distData" : [
//    {"mac" : 106E56F723A3, "dist" : 6.103, time "3256.4"},
//    {"mac" : 6D5732F3AE03, "dist" : 2.652, time "1752.5"}
//  ]
//}
// Structure example to receive data
// Must match the sender structure

typedef struct struct_message {
    unsigned char mac[6];
    float transDist;
    float totalTime;
    float timeGroups; //6 7 8 9 10 11 12

} struct_message;
// Create a struct_message called myData

struct_message myData;

WiFiServer server(80);
String header;


// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.print("mac address: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", myData.mac[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.print("  |  Dist: ");
  Serial.println(myData.transDist, 6);
}
 
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  Wifi.softAP(ssid, password);
  IPAddress IP = Wifi.softAPIP();
  Serial.println(IP);
  server.begin();

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}
 
void loop() {
  WiFiClient client = server.available();

  if(cilent){
    Serial.println("new client");
    String currentLine = "";
    while (client.connected()) {
      if(client.available()){
        char c = client.read();
        header += c;
        if(c=='\n'){
          if(currentline.length() == 0){
            client.println("HTTP/1.1 200 OK");
          }
        }
      }
    }
  }
}
