#include <WiFi.h>
#include <WebServer.h>

// Replace with your actual Wi-Fi network credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Web server listens on port 80 (standard HTTP)
WebServer server(80);

// The HTML content for your web page, stored in program memory (PROGMEM)
// This saves RAM and stores the large string in flash.
const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 GPS Tracker</title>
    <style>
        body {
            font-family: 'Inter', sans-serif;
            margin: 0;
            padding: 20px;
            background: linear-gradient(to bottom right, #3B82F6, #8B5CF6); /* Tailwind blue-500 to purple-600 */
            color: #333;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            box-sizing: border-box;
        }
        .container {
            background-color: #fff;
            border-radius: 12px; /* rounded-xl */
            box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.1), 0 8px 10px -6px rgba(0, 0, 0, 0.1); /* shadow-2xl */
            padding: 32px; /* p-8 */
            width: 100%;
            max-width: 960px; /* Increased max-width for PC */
            text-align: center;
            transition: transform 0.3s ease-in-out;
        }
        .container:hover {
            transform: scale(1.01); /* Slightly less hover effect for wider layout */
        }
        h1 {
            font-size: 36px; /* text-4xl */
            font-weight: 800; /* font-extrabold */
            color: #1F2937; /* gray-800 */
            margin-bottom: 24px; /* mb-6 */
        }
        h2 {
            font-size: 24px; /* text-2xl */
            font-weight: 700; /* font-bold */
            color: #374151; /* gray-700 */
            margin-bottom: 16px; /* mb-4 */
        }
        .section {
            margin-bottom: 24px; /* mb-6 */
            padding: 16px; /* p-4 */
            border-radius: 8px; /* rounded-lg */
            box-shadow: inset 0 2px 4px 0 rgba(0, 0, 0, 0.06); /* shadow-inner */
        }
        .gps-status {
            background-color: #DBEAFE; /* blue-100 */
        }
        .gps-status p:first-child {
            font-size: 20px; /* text-xl */
            font-weight: 600; /* font-semibold */
            color: #1E40AF; /* blue-800 */
            margin-bottom: 8px; /* mb-2 */
        }
        .gps-status p:last-child {
            font-size: 24px; /* text-2xl */
            color: #2563EB; /* blue-600 */
            font-weight: 700; /* font-bold */
        }
        .data-grid {
            display: grid;
            grid-template-columns: 1fr; /* Default to single column */
            gap: 16px; /* gap-4 */
            text-align: left;
        }
        @media (min-width: 640px) { /* md:grid-cols-2 */
            .data-grid {
                grid-template-columns: 1fr 1fr;
            }
            .col-span-2 {
                grid-column: span 2 / span 2;
            }
        }
        .data-item {
            background-color: #F9FAFB; /* gray-50 */
            padding: 16px; /* p-4 */
            border-radius: 8px; /* rounded-lg */
            box-shadow: 0 1px 2px 0 rgba(0, 0, 0, 0.05); /* shadow-sm */
        }
        .data-item p:first-child {
            font-size: 18px; /* text-lg */
            font-weight: 600; /* font-semibold */
            color: #374151; /* gray-700 */
        }
        .data-item p:last-child {
            font-size: 20px; /* text-xl */
            color: #1F2937; /* gray-900 */
            font-weight: 500; /* font-medium */
        }
        .wifi-status {
            margin-top: 32px; /* mt-8 */
            padding-top: 24px; /* pt-6 */
            border-top: 1px solid #E5E7EB; /* border-t border-gray-200 */
            text-align: left;
        }
        .wifi-status p {
            margin-bottom: 8px; /* mb-2 */
            color: #4B5563; /* gray-600 */
        }
        .wifi-status p span {
            font-weight: 600; /* font-semibold */
        }

        /* New styles for ESP32 List */
        .esp32-list-section {
            background-color: #F0F9FF; /* blue-50 */
            padding: 20px;
            border-radius: 12px;
            margin-bottom: 32px;
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -2px rgba(0, 0, 0, 0.1);
        }
        .esp32-list-section h2 {
            color: #1D4ED8; /* blue-700 */
            margin-bottom: 20px;
        }
        .esp32-table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 15px;
        }
        .esp32-table th, .esp32-table td {
            padding: 12px 8px;
            border-bottom: 1px solid #E0E7FF; /* blue-200 */
            text-align: left;
            font-size: 16px;
        }
        .esp32-table th {
            background-color: #BFDBFE; /* blue-200 */
            font-weight: 700;
            color: #1E40AF; /* blue-800 */
            text-transform: uppercase;
        }
        .esp32-table tr:last-child td {
            border-bottom: none;
        }
        .esp32-table tbody tr:nth-child(even) {
            background-color: #EFF6FF; /* blue-50 */
        }
        .mac-address {
            font-family: monospace;
            font-size: 15px;
            color: #334155; /* slate-700 */
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32 GPS Tracker</h1>

        <div class="esp32-list-section">
            <h2>Connected ESP32s</h2>
            <table class="esp32-table">
                <thead>
                    <tr>
                        <th>ID</th>
                        <th>MAC Address</th>
                        <th>Distance</th>
                        <th>Time</th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                        <td>01</td>
                        <td class="mac-address">N/A</td>
                        <td>N/A</td>
                        <td>N/A</td>
                    </tr>
                    <tr>
                        <td>02</td>
                        <td class="mac-address">dc:06:75:a9:81:b8</td>
                        <td>N/A</td>
                        <td>N/A</td>
                    </tr>
                    <tr>
                        <td>03</td>
                        <td class="mac-address">dc:06:75:f9:82:9c</td>
                        <td>N/A</td>
                        <td>N/A</td>
                    </tr>
                    <tr>
                        <td>04</td>
                        <td class="mac-address">28:37:2f:55:92:10</td>
                        <td>N/A</td>
                        <td>N/A</td>
                    </tr>
                    <tr>
                        <td>05</td>
                        <td class="mac-address">dc:06:75:ab:e5:04</td>
                        <td>N/A</td>
                        <td>N/A</td>
                    </tr>
                    <tr>
                        <td>06</td>
                        <td class="mac-address">94:a9:90:97:bb:9c</td>
                        <td>N/A</td>
                        <td>N/A</td>
                    </tr>
                    <tr>
                        <td>07</td>
                        <td class="mac-address">94:a9:90:95:f8:a8</td>
                        <td>N/A</td>
                        <td>N/A</td>
                    </tr>
                    <tr>
                        <td>08</td>
                        <td class="mac-address">N/A</td>
                        <td>N/A</td>
                        <td>N/A</td>
                    </tr>
                    <tr>
                        <td>09</td>
                        <td class="mac-address">N/A</td>
                        <td>N/A</td>
                        <td>N/A</td>
                    </tr>
                    <tr>
                        <td>10</td>
                        <td class="mac-address">N/A</td>
                        <td>N/A</td>
                        <td>N/A</td>
                    </tr>
                </tbody>
            </table>
        </div>

        <div class="section gps-status">
            <p>GPS Data Status:</p>
            <p id="gps-status-message">Waiting for GPS data...</p>
        </div>

        <div class="data-grid">
            <div class="data-item">
                <p>Latitude:</p>
                <p id="latitude">N/A</p>
            </div>
            <div class="data-item">
                <p>Longitude:</p>
                <p id="longitude">N/A</p>
            </div>
            <div class="data-item">
                <p>Altitude:</p>
                <p id="altitude">N/A</p>
            </div>
            <div class="data-item">
                <p>Speed:</p>
                <p id="speed">N/A</p>
            </div>
            <div class="data-item col-span-2">
                <p>Satellites:</p>
                <p id="satellites">N/A</p>
            </div>
            <div class="data-item col-span-2">
                <p>Timestamp (UTC):</p>
                <p id="timestamp">N/A</p>
            </div>
        </div>

        <div class="wifi-status">
            <h2>Wi-Fi Status</h2>
            <div>
                <p>
                    <span>Status:</span> <span id="wifi-status-text">UNKNOWN</span>
                </p>
                <p>
                    <span>IP Address:</span> <span id="wifi-ip-address">N/A</span>
                </p>
                <p>
                    <span>Details:</span> <span id="wifi-details">Attempting to connect to ESP32...</span>
                </p>
            </div>
        </div>
    </div>

    <script>
        // Function to fetch GPS data from the ESP32
        const fetchGpsData = async () => {
            try {
                const response = await fetch('/api/gps-data');
                if (!response.ok) {
                    throw new Error(`HTTP error! status: ${response.status}`);
                }
                const data = await response.json();

                document.getElementById('latitude').textContent = data.latitude !== undefined ? data.latitude.toFixed(6) : 'N/A';
                document.getElementById('longitude').textContent = data.longitude !== undefined ? data.longitude.toFixed(6) : 'N/A';
                document.getElementById('altitude').textContent = data.altitude !== undefined ? data.altitude.toFixed(2) + ' m' : 'N/A';
                document.getElementById('speed').textContent = data.speed !== undefined ? data.speed.toFixed(2) + ' km/h' : 'N/A';
                document.getElementById('satellites').textContent = data.satellites !== undefined ? data.satellites : 'N/A';
                document.getElementById('timestamp').textContent = data.timestamp || 'N/A';
                document.getElementById('gps-status-message').textContent = data.status_message || 'GPS data received.';

            } catch (error) {
                console.error("Could not fetch GPS data from ESP32:", error);
                document.getElementById('gps-status-message').textContent = 'Failed to load GPS data from ESP32.';
            }
        };

        // Function to fetch Wi-Fi status from the ESP32
        const fetchWifiStatus = async () => {
            try {
                const response = await fetch('/api/wifi-status');
                if (!response.ok) {
                    throw new Error(`HTTP error! status: ${response.status}`);
                }
                const data = await response.json();

                document.getElementById('wifi-status-text').textContent = data.status;
                document.getElementById('wifi-ip-address').textContent = data.ip_address;
                document.getElementById('wifi-details').textContent = data.status_message;

            } catch (error) {
                console.error("Could not fetch Wi-Fi status:", error);
                document.getElementById('wifi-details').textContent = 'Failed to retrieve Wi-Fi status from ESP32.';
            }
        };

        // Initial fetch and set up interval
        window.onload = function() {
            fetchGpsData();
            fetchWifiStatus();
            setInterval(() => {
                fetchGpsData();
                fetchWifiStatus();
            }, 5000); // Fetch every 5 seconds
        };
    </script>
</body>
</html>
)rawliteral";

// Placeholder for GPS data (you'll update this in your actual GPS integration)
struct GpsData {
  float latitude = 0.0;
  float longitude = 0.0;
  float altitude = 0.0;
  float speed = 0.0;
  int satellites = 0;
  String timestamp = "N/A";
  String status_message = "Waiting for GPS fix...";
};

GpsData currentGpsData; // Global variable to hold GPS data

// API endpoint for GPS data
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

// API endpoint for Wi-Fi status
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


// Setup function: runs once when the ESP32 starts
void setup() {
  Serial.begin(115200); // Initialize serial communication for debugging

  // Connect to Wi-Fi in Station (STA) mode
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // Wait for Wi-Fi connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); // Print the IP address assigned by your router

  // Set up web server routes
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML); // Serve the embedded HTML
  });
  server.on("/api/gps-data", handleGpsData);
  server.on("/api/wifi-status", handleWifiStatus);

  // Start the web server
  server.begin();
  Serial.println("HTTP server started");

  // --- Initialize GPS module here (example, replace with actual code) ---
  // For a real application, you would initialize your GPS module (e.g., Serial2.begin(9600);)
  // and start reading data.
  // For demonstration, we'll set some dummy data.
  currentGpsData.latitude = 34.0522;
  currentGpsData.longitude = -118.2437;
  currentGpsData.altitude = 100.0;
  currentGpsData.speed = 10.5;
  currentGpsData.satellites = 7;
  currentGpsData.timestamp = "2025-07-16T02:30:00Z";
  currentGpsData.status_message = "GPS data initialized (dummy)";
  // --- End GPS initialization example ---
}

// Loop function: runs repeatedly
void loop() {
  server.handleClient(); // Handle incoming client requests

  // --- Update GPS data here (example, replace with actual code) ---
  // In a real application, you would continuously read from your GPS module
  // and update currentGpsData.
  // For demonstration, we'll just update the timestamp every few seconds.
  static unsigned long lastGpsUpdateTime = 0;
  if (millis() - lastGpsUpdateTime > 10000) { // Update every 10 seconds
    lastGpsUpdateTime = millis();
    // Simulate new GPS data or update timestamp
    currentGpsData.timestamp = String(millis()); // Use millis for a changing timestamp
    Serial.println("Simulating GPS data update.");
  }
  // --- End GPS update example ---
}
