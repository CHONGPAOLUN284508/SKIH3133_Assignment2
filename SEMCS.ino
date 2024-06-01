#include <ESP8266WiFi.h>  // Include the WiFi library for ESP8266
#include <EEPROM.h>       // Include the EEPROM library for storing configuration
#include <ESP8266WebServer.h>  // Include the WebServer library for creating a web server
#include <DHT.h>  // Include the DHT sensor library

#define DHTPIN D4          // Pin connected to DHT11 sensor
#define DHTTYPE DHT11      // DHT11 sensor type
#define RELAY_PIN D0       // Pin connected to Relay
#define TRIG_PIN D6        // Pin connected to Ultrasonic sensor Trig
#define ECHO_PIN D5        // Pin connected to Ultrasonic sensor Echo

DHT dht(DHTPIN, DHTTYPE);  // Initialize DHT sensor
ESP8266WebServer server(80);  // Initialize the web server on port 80

// Structure to store configuration
struct ConfigRecord {
  char deviceID[32];  // Device ID
  char ssid[32];  // WiFi SSID
  char password[32];  // WiFi password
  bool relayStatus;  // true means ON, false means OFF
  float temperature;  // Temperature value
  float humidity;  // Humidity value
  long distance;  // Distance value
};

String ssid = "", password = "", deviceID = "";  // Initialize empty strings for WiFi credentials and device ID
bool relayStatus = false;  // Initialize relay status as false (OFF)

void setup() {
  Serial.begin(115200);  // Initialize serial communication at 115200 baud
  delay(10);  // Small delay for stability

  dht.begin();  // Initialize DHT sensor
  pinMode(RELAY_PIN, OUTPUT);  // Set Relay pin as output
  pinMode(TRIG_PIN, OUTPUT);  // Set Trig pin as output
  pinMode(ECHO_PIN, INPUT);  // Set Echo pin as input
  Serial.println(" ");  // Print an empty line for readability

  // Initialize EEPROM with size of 512 bytes
  EEPROM.begin(512);
  loadConfig();  // Load configuration from EEPROM

  // If WiFi credentials are found, connect to WiFi
  if (!ssid.isEmpty() && !password.isEmpty()) {
    connectToWiFi();
  } else {
    startAPMode();  // Otherwise, start in Access Point mode
  }
}

void loop() {
  server.handleClient();  // Handle client requests
}

void startAPMode() {
  WiFi.mode(WIFI_AP);  // Set WiFi mode to Access Point
  WiFi.softAP("ESP8266-AP");  // Create a WiFi network with SSID "ESP8266-AP"
  Serial.print("Please connect to ESP8266-AP for setup your WiFi\nHere is the IP address: ");
  Serial.println(WiFi.softAPIP());  // Print the IP address of the AP
  startServer();  // Start the web server
}

void connectToWiFi() {
  WiFi.begin(ssid.c_str(), password.c_str());  // Connect to WiFi with provided credentials
  Serial.print("Connecting to WiFi");
  int attempts = 0;  // Initialize attempt counter
  // Try connecting to WiFi for 20 attempts
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");//Successful connecting
    Serial.print("Please connect to this IP Address: ");
    Serial.println(WiFi.localIP());  // Print the local IP address
    digitalWrite(RELAY_PIN, relayStatus ? HIGH : LOW);  // Set the relay status (HIGH = ON, LOW = OFF)
    startServer();  // Start the web server
  } else {
    Serial.println("\nFailed to connect to WiFi. Starting AP mode.");
    startAPMode();  // If connection fails, start in Access Point mode
  }
}

void startServer() {
  server.on("/", HTTP_GET, []() {
    long distance = measureDistance();  // Measure distance using ultrasonic sensor
    float h = dht.readHumidity();  // Read humidity from DHT sensor
    float t = dht.readTemperature();  // Read temperature from DHT sensor

    // Check if any reads failed and set to 0 if true.
    if (isnan(h) || isnan(t)) {
      h = 0.0;
      t = 0.0;
    }

    // HTML content for the configuration page
    String content = "<html><head><title>Device Control</title><style>";
    content += "body {font-family: Arial, sans-serif; background: #f0f0f0; text-align: center; padding: 50px;}";
    content += "table {margin: auto; border-collapse: collapse; width: 80%;}";
    content += "input, button {margin: 10px; padding: 10px;}";
    content += "button {background-color: #4CAF50; color: white;}";
    content += "td, th {border: 1px solid #ddd; padding: 12px;}";
    content += "tr:nth-child(even) {background-color: #f2f2f2;}";
    content += "tr:hover {background-color: #ddd;}";
    content += "th {padding-top: 12px; padding-bottom: 12px; text-align: left; background-color: #4CAF50; color: white;}";
    content += "</style></head><body>";
    content += "<h1>Device Settings</h1>";
    content += "<form action='/save' method='post'>";
    content += "<table><tr><td>Device ID:</td><td><input type='text' name='deviceid' value=''></td></tr>";
    content += "<tr><td>WiFi SSID:</td><td><input type='text' name='ssid' value=''></td></tr>";
    content += "<tr><td>WiFi Password:</td><td><input type='password' name='password' value=''></td></tr>";
    content += "<tr><td>Temperature:</td><td>" + String(t) + "&deg;C</td></tr>";
    content += "<tr><td>Humidity:</td><td>" + String(h) + "%</td></tr>";
    content += "<tr><td>Distance:</td><td>" + String(distance) + " cm</td></tr>";
    content += "<tr><td>Output Status:</td><td><input type='radio' name='relay' value='1'>On <input type='radio' name='relay' value='0'>Off</td></tr>";
    content += "</table><button type='submit'>Save</button></form>";
    
    // History table displaying stored records
    content += "<h2>History</h2>";
    content += "<table border='1'>";
    content += "<tr><th>Device ID</th><th>SSID</th><th>Password</th><th>Temperature</th><th>Humidity</th><th>Distance</th><th>Relay Status</th></tr>";
    
    int recordSize = sizeof(ConfigRecord);  // Get the size of ConfigRecord
    int numRecords = EEPROM.read(0); // Get the number of records stored
    Serial.print("Number of records stored: ");
    Serial.println(numRecords);
    
    for (int i = 0; i < numRecords; i++) {
      ConfigRecord record;
      int address = 1 + (i * recordSize); // Offset by 1 to account for numRecords storage
      EEPROM.get(address, record);
    
      // Only display non-empty records
      if (String(record.deviceID) != "") {
        content += "<tr>";
        content += "<td>" + String(record.deviceID) + "</td>";
        content += "<td>" + String(record.ssid) + "</td>";
        content += "<td>" + String(record.password) + "</td>";
        content += "<td>" + String(record.temperature) + "&deg;C</td>";
        content += "<td>" + String(record.humidity) + "%</td>";
        content += "<td>" + String(record.distance) + " cm</td>";
        content += "<td>" + String(record.relayStatus ? "ON" : "OFF") + "</td>";
        content += "</tr>";
      }
    }
    
    content += "</table>";
    content += "</body></html>";
    server.send(200, "text/html", content);  // Send HTML response
  });

  server.on("/save", HTTP_POST, []() {
    ssid = server.arg("ssid");  // Get SSID from form
    password = server.arg("password");  // Get password from form
    deviceID = server.arg("deviceid");  // Get device ID from form
    relayStatus = server.arg("relay") == "1";  // Get relay status from form
    digitalWrite(RELAY_PIN, relayStatus ? HIGH : LOW);  // Set relay status (HIGH = ON, LOW = OFF)

    // If SSID or password is empty, show error message
    if (ssid.isEmpty() || password.isEmpty()) {
      server.send(200, "text/html", "<html><body><h1>SSID and Password cannot be empty. Please try again.</h1></body></html>");
    } else {
      saveConfig();  // Save configuration to EEPROM
      // HTML content for the redirection page
      String redirectPage = "<html><head>";
      redirectPage += "<meta http-equiv='refresh' content='5;url=http://" + WiFi.localIP().toString() + "' />";
      redirectPage += "<title>Configuration Saved</title>";
      redirectPage += "<style>body {font-family: Arial, sans-serif; background: #f0f0f0; text-align: center; padding: 50px;} ";
      redirectPage += "button {background-color: #4CAF50; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px;}</style>";
      redirectPage += "</head><body><h1>Configuration saved. Rebooting... Please wait for 5 seconds</h1>";
      redirectPage += "<button onclick='window.location.href=\"http://" + WiFi.localIP().toString() + "\";'>Go to Device Control Page</button>";
      redirectPage += "</body></html>";
      server.send(200, "text/html", redirectPage);  // Send HTML response
      delay(5000);  // Wait for 5 seconds
      ESP.restart();  // Restart the ESP8266
    }
  });

  server.begin();  // Start the web server
  Serial.println("HTTP server started");
}

void loadConfig() {
  int numRecords = EEPROM.read(0); // Get the number of records stored
  Serial.print("Loaded number of records stored: ");
  Serial.println(numRecords);

  if (numRecords > 0) {
    int recordSize = sizeof(ConfigRecord);
    ConfigRecord record;
    int address = 1 + ((numRecords - 1) * recordSize); // Offset by 1 to account for numRecords storage
    EEPROM.get(address, record);

    // Load the latest record into the current configuration
    if (String(record.deviceID) != "") {
      deviceID = String(record.deviceID);
      ssid = String(record.ssid);
      password = String(record.password);
      relayStatus = record.relayStatus;
    }
  }
}

void saveConfig() {
  ConfigRecord record;
  deviceID.toCharArray(record.deviceID, 32);
  ssid.toCharArray(record.ssid, 32);
  password.toCharArray(record.password, 32);
  record.relayStatus = relayStatus;
  record.temperature = dht.readTemperature();
  record.humidity = dht.readHumidity();
  record.distance = measureDistance();

  int recordSize = sizeof(ConfigRecord);
  int numRecords = EEPROM.read(0); // Get the number of records stored
  if (numRecords >= 255) numRecords = 0; // Reset if overflow (for simplicity)
  numRecords++;

  // Save the new record at the current position
  int startAddress = 1 + ((numRecords - 1) * recordSize); // Offset by 1 to account for numRecords storage
  EEPROM.put(startAddress, record);
  EEPROM.commit();

  // Increment the number of records
  EEPROM.write(0, numRecords);
  EEPROM.commit();
}

long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);  // Set Trig pin LOW to start
  delayMicroseconds(2);  // Wait for 2 microseconds
  digitalWrite(TRIG_PIN, HIGH);  // Set Trig pin HIGH to send pulse
  delayMicroseconds(10);  // Wait for 10 microseconds
  digitalWrite(TRIG_PIN, LOW);  // Set Trig pin LOW again
  long duration = pulseIn(ECHO_PIN, HIGH);  // Read the Echo pin for the pulse duration
  long distance = duration * 0.034 / 2; // Calculate distance (duration * speed of sound / 2)
  return distance;  // Return the distance value
}
