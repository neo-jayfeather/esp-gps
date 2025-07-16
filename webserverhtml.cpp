#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h> // For Non-Volatile Storage (NVS)
#include <ArduinoJson.h> // For JSON parsing and serialization

// Replace with your actual initial Wi-Fi network credentials
// This will be used if no networks are stored in NVS.
const char* initial_ssid = "YOUR_WIFI_SSID";
const char* initial_password = "YOUR_WIFI_PASSWORD";

WebServer server(80);
Preferences preferences; // NVS preferences object

// Structure to hold network configuration
struct NetworkConfig {
  String ssid;
  String password;
  bool priority;
};

// Vector to store all configured networks
std::vector<NetworkConfig> configuredNetworks;

// --- GPS Data Placeholder ---
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
// --- End GPS Data Placeholder ---

// --- Wi-Fi Connection Management Variables ---
unsigned long lastConnectionAttemptTime = 0;
const long connectionAttemptInterval = 10000; // Try connecting every 10 seconds
int currentNetworkAttemptIndex = 0;
// --- End Wi-Fi Connection Management Variables ---

// --- HTML Content ---
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

        /* Network Management Form Styles */
        .network-management-section {
            background-color: #F8FAFC; /* gray-50 */
            padding: 20px;
            border-radius: 12px;
            margin-top: 32px;
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -2px rgba(0, 0, 0, 0.1);
            text-align: left;
        }
        .network-management-section h2 {
            color: #0F172A; /* slate-900 */
            margin-bottom: 20px;
        }
        .form-group {
            margin-bottom: 15px;
        }
        .form-group label {
            display: block;
            margin-bottom: 5px;
            font-weight: 600;
            color: #475569; /* slate-700 */
        }
        .form-group input[type="text"],
        .form-group input[type="password"] {
            width: calc(100% - 20px);
            padding: 10px;
            border: 1px solid #CBD5E1; /* slate-300 */
            border-radius: 6px;
            font-size: 16px;
            box-sizing: border-box;
        }
        .form-group input[type="checkbox"] {
            margin-right: 8px;
        }
        .button-group {
            display: flex;
            gap: 10px;
            margin-top: 20px;
            justify-content: flex-end; /* Align buttons to the right */
        }
        .button-group button {
            padding: 10px 20px;
            border-radius: 6px;
            font-weight: 600;
            cursor: pointer;
            transition: background-color 0.2s ease-in-out, transform 0.1s ease-in-out;
        }
        .button-group button:hover {
            transform: translateY(-1px);
        }
        .add-button {
            background-color: #22C55E; /* green-500 */
            color: white;
            border: none;
        }
        .add-button:hover {
            background-color: #16A34A; /* green-600 */
        }
        .clear-button {
            background-color: #EF4444; /* red-500 */
            color: white;
            border: none;
        }
        .clear-button:hover {
            background-color: #DC2626; /* red-600 */
        }
        .message-box {
            background-color: #FFFBEB; /* yellow-50 */
            color: #D97706; /* yellow-700 */
            border: 1px solid #FCD34D; /* yellow-300 */
            padding: 10px;
            border-radius: 6px;
            margin-top: 15px;
            text-align: center;
            font-size: 14px;
            display: none; /* Hidden by default */
        }
        .network-list-display {
            margin-top: 20px;
            border-top: 1px solid #E2E8F0; /* slate-200 */
            padding-top: 15px;
        }
        .network-list-display h3 {
            font-size: 18px;
            font-weight: 700;
            color: #475569;
            margin-bottom: 10px;
        }
        .network-list-display ul {
            list-style: none;
            padding: 0;
        }
        .network-list-display li {
            background-color: #F0F4F8; /* slate-100 */
            padding: 10px;
            border-radius: 6px;
            margin-bottom: 8px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 15px;
            color: #334155;
        }
        .network-list-display li strong {
            color: #1E40AF; /* blue-800 */
        }
        .network-list-display li .priority-tag {
            background-color: #BFDBFE; /* blue-200 */
            color: #1E40AF; /* blue-800 */
            padding: 4px 8px;
            border-radius: 4px;
            font-size: 12px;
            font-weight: 600;
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

        <div class="network-management-section">
            <h2>Manage Wi-Fi Networks</h2>
            <div class="message-box" id="messageBox"></div>

            <div class="form-group">
                <label for="ssid">SSID:</label>
                <input type="text" id="ssid" placeholder="Network Name" required>
            </div>
            <div class="form-group">
                <label for="password">Password:</label>
                <input type="password" id="password" placeholder="Network Password">
            </div>
            <div class="form-group">
                <input type="checkbox" id="priority">
                <label for="priority">Set as Priority Network</label>
            </div>
            <div class="button-group">
                <button class="add-button" onclick="addNetwork()">Add Network</button>
                <button class="clear-button" onclick="clearNetworks()">Clear All Networks</button>
            </div>

            <div class="network-list-display">
                <h3>Configured Networks:</h3>
                <ul id="configuredNetworksList">
                    <!-- Networks will be loaded here by JavaScript -->
                    <li>No networks configured yet.</li>
                </ul>
            </div>
        </div>
    </div>

    <script>
        function showMessage(message, type = 'info') {
            const msgBox = document.getElementById('messageBox');
            msgBox.textContent = message;
            msgBox.className = 'message-box'; // Reset classes
            if (type === 'error') {
                msgBox.style.backgroundColor = '#FEE2E2'; // red-100
                msgBox.style.color = '#B91C1C'; // red-700
                msgBox.style.borderColor = '#FCA5A5'; // red-300
            } else if (type === 'success') {
                msgBox.style.backgroundColor = '#D1FAE5'; // green-100
                msgBox.style.color = '#065F46'; // green-700
                msgBox.style.borderColor = '#A7F3D0'; // green-300
            } else { // info
                msgBox.style.backgroundColor = '#DBEAFE'; // blue-100
                msgBox.style.color = '#1E40AF'; // blue-800
                msgBox.style.borderColor = '#93C5FD'; // blue-300
            }
            msgBox.style.display = 'block';
            setTimeout(() => {
                msgBox.style.display = 'none';
            }, 5000); // Hide after 5 seconds
        }

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

        // Function to fetch and display configured networks
        const fetchConfiguredNetworks = async () => {
            try {
                const response = await fetch('/api/networks');
                if (!response.ok) {
                    throw new Error(`HTTP error! status: ${response.status}`);
                }
                const networks = await response.json();
                const listElement = document.getElementById('configuredNetworksList');
                listElement.innerHTML = ''; // Clear existing list

                if (networks.length === 0) {
                    listElement.innerHTML = '<li>No networks configured yet.</li>';
                } else {
                    networks.forEach(net => {
                        const listItem = document.createElement('li');
                        listItem.innerHTML = `
                            <span><strong>SSID:</strong> ${net.ssid}</span>
                            ${net.priority ? '<span class="priority-tag">Priority</span>' : ''}
                        `;
                        listElement.appendChild(listItem);
                    });
                }
            } catch (error) {
                console.error("Could not fetch configured networks:", error);
                document.getElementById('configuredNetworksList').innerHTML = '<li>Failed to load configured networks.</li>';
            }
        };

        // Function to add a new network
        const addNetwork = async () => {
            const ssidInput = document.getElementById('ssid');
            const passwordInput = document.getElementById('password');
            const priorityInput = document.getElementById('priority');

            const ssid = ssidInput.value.trim();
            const password = passwordInput.value;
            const priority = priorityInput.checked;

            if (!ssid) {
                showMessage('SSID cannot be empty.', 'error');
                return;
            }

            try {
                const response = await fetch('/api/add-network', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                    },
                    body: JSON.stringify({ ssid, password, priority }),
                });

                if (!response.ok) {
                    throw new Error(`HTTP error! status: ${response.status}`);
                }
                const result = await response.json();
                if (result.success) {
                    showMessage('Network added successfully!', 'success');
                    ssidInput.value = '';
                    passwordInput.value = '';
                    priorityInput.checked = false;
                    fetchConfiguredNetworks(); // Refresh the list
                } else {
                    showMessage(`Failed to add network: ${result.message}`, 'error');
                }
            } catch (error) {
                console.error("Error adding network:", error);
                showMessage('An error occurred while adding the network.', 'error');
            }
        };

        // Function to clear all networks
        const clearNetworks = async () => {
            if (!confirm('Are you sure you want to clear all configured networks?')) {
                return;
            }

            try {
                const response = await fetch('/api/clear-networks', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                    },
                    body: JSON.stringify({ confirm: true }), // Send confirmation
                });

                if (!response.ok) {
                    throw new Error(`HTTP error! status: ${response.status}`);
                }
                const result = await response.json();
                if (result.success) {
                    showMessage('All networks cleared successfully!', 'success');
                    fetchConfiguredNetworks(); // Refresh the list
                } else {
                    showMessage(`Failed to clear networks: ${result.message}`, 'error');
                }
            } catch (error) {
                console.error("Error clearing networks:", error);
                showMessage('An error occurred while clearing networks.', 'error');
            }
        };

        // Initial fetch and set up interval
        window.onload = function() {
            fetchGpsData();
            fetchWifiStatus();
            fetchConfiguredNetworks(); // Fetch networks on load

            setInterval(() => {
                fetchGpsData();
                fetchWifiStatus();
                fetchConfiguredNetworks(); // Periodically refresh networks
            }, 5000); // Fetch every 5 seconds
        };
    </script>
</body>
</html>
)rawliteral";

// Function to save networks to NVS
void saveNetworks() {
  preferences.begin("wifi-creds", false); // Open preferences in read-write mode
  DynamicJsonDocument doc(4096); // Adjust size as needed for more networks
  JsonArray networksArray = doc.to<JsonArray>();

  for (const auto& net : configuredNetworks) {
    JsonObject obj = networksArray.add<JsonObject>();
    obj["ssid"] = net.ssid;
    obj["password"] = net.password;
    obj["priority"] = net.priority;
  }

  String jsonString;
  serializeJson(doc, jsonString);
  preferences.putString("networks", jsonString);
  preferences.end();
  Serial.println("Networks saved to NVS.");
}

// Function to load networks from NVS
void loadNetworks() {
  preferences.begin("wifi-creds", false); // Open preferences in read-write mode
  String jsonString = preferences.getString("networks", "[]"); // Default to empty array JSON
  preferences.end();

  configuredNetworks.clear(); // Clear current list before loading

  DynamicJsonDocument doc(4096); // Must be large enough for your JSON string
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  JsonArray networksArray = doc.as<JsonArray>();
  for (JsonObject obj : networksArray) {
    NetworkConfig net;
    net.ssid = obj["ssid"].as<String>();
    net.password = obj["password"].as<String>();
    net.priority = obj["priority"].as<bool>();
    configuredNetworks.push_back(net);
  }
  Serial.printf("Loaded %d networks from NVS.\n", configuredNetworks.size());
}

// API endpoint to add a network
void handleAddNetwork() {
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Missing body\"}");
    return;
  }

  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    return;
  }

  String ssid = doc["ssid"].as<String>();
  String password = doc["password"].as<String>();
  bool priority = doc["priority"].as<bool>();

  if (ssid.isEmpty()) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"SSID cannot be empty\"}");
    return;
  }

  // If new network is priority, clear existing priorities
  if (priority) {
    for (auto& net : configuredNetworks) {
      net.priority = false;
    }
  }

  // Check if network already exists, update if it does
  bool updated = false;
  for (auto& net : configuredNetworks) {
    if (net.ssid == ssid) {
      net.password = password;
      net.priority = priority;
      updated = true;
      break;
    }
  }

  if (!updated) {
    NetworkConfig newNet = {ssid, password, priority};
    configuredNetworks.push_back(newNet);
  }

  saveNetworks();
  server.send(200, "application/json", "{\"success\":true,\"message\":\"Network added/updated\"}");
}

// API endpoint to clear all networks
void handleClearNetworks() {
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Missing body\"}");
    return;
  }

  DynamicJsonDocument doc(64);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error || !doc["confirm"].as<bool>()) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Confirmation missing\"}");
    return;
  }

  configuredNetworks.clear();
  saveNetworks(); // Save empty list
  server.send(200, "application/json", "{\"success\":true,\"message\":\"All networks cleared\"}");
}

// API endpoint to get configured networks (without passwords)
void handleGetNetworks() {
  DynamicJsonDocument doc(1024); // Adjust size based on number of networks
  JsonArray networksArray = doc.to<JsonArray>();

  for (const auto& net : configuredNetworks) {
    JsonObject obj = networksArray.add<JsonObject>();
    obj["ssid"] = net.ssid;
    obj["priority"] = net.priority;
    // Do NOT send password to frontend for security reasons
  }

  String jsonString;
  serializeJson(doc, jsonString);
  server.send(200, "application/json", jsonString);
}

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
    // Provide more specific error message if available
    switch (WiFi.status()) {
      case WL_NO_SHIELD: status_message = "No WiFi shield"; break;
      case WL_IDLE_STATUS: status_message = "Idle"; break;
      case WL_NO_SSID_AVAIL: status_message = "SSID not available"; break;
      case WL_SCAN_COMPLETED: status_message = "Scan completed"; break;
      case WL_CONNECT_FAILED: status_message = "Connection failed"; break;
      case WL_CONNECTION_LOST: status_message = "Connection lost"; break;
      case WL_DISCONNECTED: status_message = "Disconnected"; break;
      default: status_message = "Unknown status code: " + String(WiFi.status()); break;
    }
    if (configuredNetworks.empty()) {
      status_message += ". No networks configured.";
    } else {
      status_message += ". Attempting to connect to network " + String(currentNetworkAttemptIndex + 1) + "/" + String(configuredNetworks.size());
    }
  }

  String json = "{";
  json += "\"status\":\"" + status + "\"";
  json += ",\"ip_address\":\"" + ip_address + "\"";
  json += ",\"status_message\":\"" + status_message + "\"";
  json += "}";
  server.send(200, "application/json", json);
}


// Function to attempt Wi-Fi connection
void attemptWiFiConnection() {
  if (configuredNetworks.empty()) {
    Serial.println("No networks configured. Cannot connect to WiFi.");
    return;
  }

  // Find priority network first
  int priorityIndex = -1;
  for (int i = 0; i < configuredNetworks.size(); ++i) {
    if (configuredNetworks[i].priority) {
      priorityIndex = i;
      break;
    }
  }

  NetworkConfig targetNet;
  if (priorityIndex != -1) {
    // Try priority network first
    targetNet = configuredNetworks[priorityIndex];
    currentNetworkAttemptIndex = priorityIndex; // Update index for status message
  } else {
    // If no priority or priority failed, cycle through all networks
    targetNet = configuredNetworks[currentNetworkAttemptIndex];
  }

  Serial.printf("Attempting to connect to SSID: %s (Priority: %s)\n", targetNet.ssid.c_str(), targetNet.priority ? "Yes" : "No");
  WiFi.begin(targetNet.ssid.c_str(), targetNet.password.c_str());

  // Wait briefly for connection. Non-blocking approach is better for loop().
  // Here we just initiate connection, and check status in subsequent loops.
  // A longer blocking wait here would freeze the web server.
  unsigned long connectStartTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - connectStartTime < 5000) { // Wait up to 5 seconds
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected to WiFi: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    currentNetworkAttemptIndex = 0; // Reset index on successful connection
  } else {
    Serial.printf("Failed to connect to %s. Status: %d\n", targetNet.ssid.c_str(), WiFi.status());
    // Move to the next network in sequence for the next attempt
    currentNetworkAttemptIndex = (currentNetworkAttemptIndex + 1) % configuredNetworks.size();
  }
  lastConnectionAttemptTime = millis(); // Update last attempt time
}


// Setup function: runs once when the ESP32 starts
void setup() {
  Serial.begin(115200); // Initialize serial communication for debugging

  // Load networks from NVS
  loadNetworks();

  // If no networks are configured, add the initial one
  if (configuredNetworks.empty()) {
    Serial.println("No networks found in NVS. Adding initial network.");
    NetworkConfig initialNet = {String(initial_ssid), String(initial_password), true}; // Set initial as priority
    configuredNetworks.push_back(initialNet);
    saveNetworks();
  }

  // Set up web server routes
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML); // Serve the embedded HTML
  });
  server.on("/api/gps-data", handleGpsData);
  server.on("/api/wifi-status", handleWifiStatus);
  server.on("/api/add-network", HTTP_POST, handleAddNetwork);
  server.on("/api/clear-networks", HTTP_POST, handleClearNetworks);
  server.on("/api/networks", HTTP_GET, handleGetNetworks);

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

  // --- Wi-Fi Connection Management in loop ---
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastConnectionAttemptTime > connectionAttemptInterval) {
      attemptWiFiConnection();
    }
  }

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
