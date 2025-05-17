#include <HardwareSerial.h>

// Define the serial port connected to your NEO-6M
HardwareSerial& gpsSerial = Serial1; // Example: Use Serial1 on ESP32
const long currentBaud = 9600;     // Current baud rate of NEO-6M
const long newBaud = 9600;       // Desired new baud rate
const long newFrequency = 200;      // Measurement period in milliseconds for 5Hz (1000 / 5)

// UBX-CFG-PRT message to change the baud rate of UART1 to 115200
const unsigned char UBLOX_INIT[] PROGMEM = {
  // Rate (pick one)
//  0xB5,0x62,0x06,0x08,0x06,0x00,0x64,0x00,0x01,0x00,0x01,0x00,0x7A,0x12, //(10Hz)
  //0xB5,0x62,0x06,0x08,0x06,0x00,0xC8,0x00,0x01,0x00,0x01,0x00,0xDE,0x6A, //(5Hz)
  0xB5,0x62,0x06,0x08,0x06,0x00,0xE8,0x03,0x01,0x00,0x01,0x00,0x01,0x39 //(1Hz)
};
unsigned char setBaud115200[] = {
    0xB5, 0x62, 0x06, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0xD0, 0x08, 0x00, 0x00, 0x00, 0xC2,
    0x01, 0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC4, 0x96 // Checksum
};

// UBX-CFG-RATE message for 5Hz (200ms), 1 nav update, UTC time
unsigned char setRate5Hz[] = {0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xC8, 0x00, 0x01, 0x00, 0x01, 0x00, 0xDE, 0x6A};
// Note: 0xC8 00 in little-endian is 200 (milliseconds)

// UBX-CFG-CFG message to save the configuration
// This saves all current settings to the default device (BBR/EEPROM)
byte saveConfig[] = {0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x17, 0x31};

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 NEO-6M Configuration: 115200 bps, 5Hz");

  gpsSerial.begin(currentBaud);
  Serial.print("Connecting to NEO-6M at ");
  Serial.print(currentBaud);
  Serial.println(" bps...");
  delay(1000);

  // Send command to change baud rate
  Serial.println("Sending UBX-CFG-PRT command to change baud rate to 115200...");
  gpsSerial.write(setBaud115200, sizeof(setBaud115200));
  Serial.println("Command sent. Waiting a moment...");
  delay(500);

  // Close the serial port at the old baud rate
  gpsSerial.end();

  // Begin the serial port at the new baud rate
  gpsSerial.begin(newBaud);
  Serial.print("Reconnecting to NEO-6M at ");
  Serial.print(newBaud);
  Serial.println(" bps...");
  delay(1000);

  // Send command to set update rate to 5Hz
  Serial.println("Sending UBX-CFG-RATE command for 5Hz...");
  gpsSerial.write(setRate5Hz, sizeof(setRate5Hz));
  Serial.println("5Hz command sent.");
  delay(500);

  // Optionally, send command to save the configuration
  Serial.println("Sending UBX-CFG-CFG command to save configuration...");
  gpsSerial.write(saveConfig, sizeof(saveConfig));
  Serial.println("Save command sent. This might take a few seconds.");
  Serial.println("Now listening for GPS data at 115200 bps and 5Hz.");
}

void loop() {
  while (gpsSerial.available()) {
    Serial.write(gpsSerial.read()); // Forward GPS data to the main serial monitor
  }
}

// --- Checksum Calculation (for reference) ---
// void calculateChecksum(byte *payload, int len, byte *ck_a, byte *ck_b) {
//   *ck_a = 0;
//   *ck_b = 0;
//   for (int i = 0; i < len; i++) {
//     *ck_a = *ck_a + payload[i];
//     *ck_b = *ck_b + *ck_a;
//   }
// }