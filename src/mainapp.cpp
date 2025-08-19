// Your main .ino file
#include <Arduino.h>
#include "homepage.h"
#include <esp_wifi.h>
#include "DataHandler.h"
#include <Preferences.h>
#include <PreferenceHandling.h>
#include <ESPAsyncWiFiManager.h>
#include <DNSServer.h>
// --- Wi-Fi Configuration ---

String mqtt_client ="";
String mqtt_server = "";
const int mqtt_port = 1883;
String mqtt_user = ""; // MQTT username, if required
String mqtt_pass = ""; // MQTT password, if required
String mqtt_topic = "";
String publishInfoTopic ="/info";


unsigned long mqttPublishInterval = 0; // 3 minutes
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long mqttReconnectInterval = 5000; // 5 seconds
unsigned long lastMqttPublishTime = 0;
const int MAX_WIFI_CONNECT_ATTEMPTS = 5;
int wifiConnectAttempts = 0;

// --- Optical Eye Serial Port Configuration ---
// IMPORTANT: Adjust these based on your Kamstrup Multical 21 and optical eye
const long BAUD_RATE_SERIAL = 1200; // Common baud rate for meters, verify yours!
// For ESP32-C3, Serial1 might default to GPIO_6 (RX) and GPIO_7 (TX) or other pins.
// Check your board's pinout for UART1 or UART2 pins if default Serial1 pins conflict
// or are not accessible.
#define KAMSTRUP_RX 16
#define KAMSTRUP_TX 17

// Define a HardwareSerial object for the optical eye communication.
// On ESP32-C3, UART1 is a good choice for a secondary serial port.

WiFiClient espClient;
PubSubClient mqttClient(espClient);
AsyncWebServer server(80);
DNSServer dns;
AsyncWebSocket ws("/ws");
HardwareSerial kamstrupSerial(1);
SerialSettings serialSettings = {KAMSTRUP_RX, KAMSTRUP_TX, BAUD_RATE_SERIAL};
AsyncWiFiManager wifiManager(&server, &dns);

// Instantiate the class with hardware configuration
DataHandler dataHandlerInstance(kamstrupSerial, ws, serialSettings);
RequestType requestType = CURRENT; // Default request type

// Check if 500ms have passed since the last transmission
const unsigned int TRANSMISSION_INTERVAL_MS = 500;

// Max buffer size before sending to WebSocket to avoid too many small packets
const int MAX_BUFFER_SIZE = 128; // Bytes
void setWiFiPower(int powerLevel)
{
  // Power level is in units of 0.25 dBm (e.g., 78 = 19.5 dBm)
  esp_wifi_set_max_tx_power(powerLevel);
}

float getPayloadNumber(byte* payload, unsigned int length){
  // Create a buffer that is one byte larger for the null terminator.
  // A stack buffer is fine since MQTT payloads are typically small.
  char buf[length + 1];
  
  // Copy the payload to the buffer.
  memcpy(buf, payload, length);
  
  // Add the null terminator to make it a valid C-string.
  buf[length] = '\0';
  
  // atof converts a C-string to a float.
  return atof(buf);
}

String getPublishTopic(RequestType& requestType) {
  String result ="";
  switch (requestType) {
    case RequestType::CURRENT:
      result ="Current";
      break;
    case RequestType::DAILY:
      result = "Daily";
      break;
    case RequestType::MONTHLY:
      result = "Monthly";
      break;
    case RequestType::YEARLY:
      result = "Yearly";
      break;
    default:
      result = "Unknown";
      break;
    }
    return mqtt_topic + "/" + result;
}

void callDataHandler(const String &requestType)
{
  // Convert the request type string to the enum
  if (requestType == "current")
  {
    dataHandlerInstance.currentState = DataHandler::DATAHANDLING;
    dataHandlerInstance.requestType = CURRENT;
  }
  else if (requestType == "daily")
  {
    dataHandlerInstance.currentState = DataHandler::DATAHANDLING;
    dataHandlerInstance.requestType = DAILY;
  }
  else if (requestType == "monthly")
  {
    dataHandlerInstance.currentState = DataHandler::DATAHANDLING;
    dataHandlerInstance.requestType = MONTHLY;
  }
  else if (requestType == "yearly")
  {
    dataHandlerInstance.currentState = DataHandler::DATAHANDLING;
    dataHandlerInstance.requestType = YEARLY;
  }
}

RequestType getRequestTypeFromString(const String &type)
{
  if (type == "current")
    return CURRENT;
  else if (type == "daily")
    return DAILY;
  else if (type == "monthly")
    return MONTHLY;
  else if (type == "yearly")
    return YEARLY;
  else
    return UNKNOWN; // Handle unknown request types
}
void sendMqttInfo()
{
  // Publish device information to MQTT
  mqttClient.publish(publishInfoTopic.c_str(), mqtt_client.c_str());
  mqttClient.publish(publishInfoTopic.c_str(), WiFi.localIP().toString().c_str());
}

void reconnectMQTT()
{
  // Attempt to reconnect to the MQTT broker
  String publishSubscribeTopic = mqtt_topic + "/input";
  if (!mqttClient.connected())
  {
    // Attempt to connect
    if (mqtt_user.length() > 0)
    {
      if (mqttClient.connect(mqtt_client.c_str(), mqtt_user.c_str(), mqtt_pass.c_str()))
      {
        sendMqttInfo();
        mqttClient.subscribe(publishSubscribeTopic.c_str());
      }
    }
    else
    {
      if (mqttClient.connect(mqtt_client.c_str()))
      {
        sendMqttInfo();
        mqttClient.subscribe(publishSubscribeTopic.c_str());
      }
    }
  }else{
     mqttClient.publish(publishInfoTopic.c_str(), mqtt_client.c_str());
     mqttClient.publish(publishInfoTopic.c_str(), WiFi.localIP().toString().c_str());
  }
}
//run only once
void checkmqttClient()
{
  // Check if the MQTT client is set up correctly
  publishInfoTopic = mqtt_topic + "/info";
  if (mqtt_client == "Multical_")
  {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    mqtt_client = "Multical_" + mac; // Default client name
    setPreferences(mqttPublishInterval, mqtt_server, mqtt_topic, mqtt_user, mqtt_pass, mqtt_client); 
  }
}

void callback(char* topic, byte* payload, unsigned int length) {

  String payloadname = String((const char*)payload).substring(0, length);
  payloadname.trim();
  payloadname.toLowerCase();

  if (payloadname=="current")
  {
    dataHandlerInstance.requestType = CURRENT;
  }else if (payloadname=="daily")
  {
    dataHandlerInstance.requestType = DAILY;
  }else if (payloadname=="monthly")
  {
    dataHandlerInstance.requestType = MONTHLY;
  }else if (payloadname=="yearly")
  {
    dataHandlerInstance.requestType = YEARLY;
  }else{
    ws.textAll("Unknown request type");
    return;
  }
  String publishTopic = getPublishTopic(dataHandlerInstance.requestType);
  String data = dataHandlerInstance.dataHandler(dataHandlerInstance.requestType);
  if (data.length() > 0){
    mqttClient.publish(publishTopic.c_str(), data.c_str());
  }
  
}

// --- Setup Function ---
void setup()
{
  while (!wifiManager.autoConnect("Multical_AP")) {
      wifiConnectAttempts++;
      Serial.printf("Failed to connect to WiFi. Attempt %d of %d\n", 
                  wifiConnectAttempts, MAX_WIFI_CONNECT_ATTEMPTS);
      
      if (wifiConnectAttempts >= MAX_WIFI_CONNECT_ATTEMPTS) {
          Serial.println("Maximum WiFi connection attempts reached. Restarting device...");
          delay(1000);
          ESP.restart();
      }
      
      delay(3000); // Wait before next attempt
  }
  wifiConnectAttempts = 0;
  Serial.println("WiFi connected successfully!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  getPreferences(mqttPublishInterval, mqtt_server, mqtt_topic, mqtt_user, mqtt_pass, mqtt_client); // Load preferences
  checkmqttClient();

  setWiFiPower(68); // Set Wi-Fi power level (e.g., 68 = 17 dBm, adjust as needed)
  // --- Wi-Fi Setup ---
  dataHandlerInstance.requestType = RequestType::CURRENT; // Set the default request type

  // --- Web Server Setup ---
  server.addHandler(&ws);

  // Serve the HTML page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/html", index_html); });

  server.on("/callDataHandler", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (request->hasParam("type")) {
        String type = request->getParam("type")->value();
        String result = dataHandlerInstance.dataHandler(getRequestTypeFromString(type)); // This blocks until done
        request->send(200, "application/json; charset=utf-8", result);
    } else {
        request->send(400, "text/plain", "Missing type parameter");
    } });
  
  server.on("/resetWifi", HTTP_GET, [](AsyncWebServerRequest *request)
            { 
              wifiManager.resetSettings();
              request->send(200, "text/plain", "Settings reset");
             });
  
  
  
  server.on("/saveSettings", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String fupdate      = request->hasParam("fupdate")      ? request->getParam("fupdate")->value()      : "";
    mqtt_server  = request->hasParam("mqtt_server")  ? request->getParam("mqtt_server")->value()  : "";
    mqtt_topic   = request->hasParam("mqtt_topic")   ? request->getParam("mqtt_topic")->value()   : "";
    mqtt_user    = request->hasParam("mqtt_user")    ? request->getParam("mqtt_user")->value()    : "";
    mqtt_pass    = request->hasParam("mqtt_pass")    ? request->getParam("mqtt_pass")->value()    : "";
    if (fupdate.toInt() < 20) {
      request->send(400, "text/plain", "Invalid update interval value. Must be greater than or equal to 20 seconds.");
      return;
    }
    mqttPublishInterval = fupdate.toInt() * 1000; // Convert to milliseconds
    setPreferences(mqttPublishInterval, mqtt_server, mqtt_topic, mqtt_user, mqtt_pass, mqtt_client); // Save the settings to preferences
    request->send(200, "text/plain", "Settings saved"); });
    
  server.on("/getSettings", HTTP_GET, [](AsyncWebServerRequest *request){
    getPreferences(mqttPublishInterval, mqtt_server, mqtt_topic, mqtt_user, mqtt_pass, mqtt_client);
    String fupdate = String(mqttPublishInterval/1000);
    String json = "{";
    json += "\"fupdate\":\"" + fupdate + "\",";
    json += "\"mqtt_server\":\"" + mqtt_server + "\",";
    json += "\"mqtt_topic\":\"" + mqtt_topic + "\",";
    json += "\"mqtt_user\":\"" + mqtt_user + "\",";
    json += "\"mqtt_pass\":\"" + mqtt_pass + "\"";
    json += "}";
    request->send(200, "application/json", json);
});

  server.begin();
  // --- MQTT Setup ---
  while (mqtt_server.length() == 0)
  {
    delay(1000);
    getPreferences(mqttPublishInterval, mqtt_server, mqtt_topic, mqtt_user, mqtt_pass, mqtt_client);
    ws.textAll("MQTT server address is empty. Please configure it in the settings.");
     // Re-fetch preferences to ensure mqtt_server is set
  } 
  mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
  mqttClient.setCallback(callback);
  reconnectMQTT();
}

// --- Loop Function ---
void loop()
{
 
  if (!mqttClient.connected())
  {
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > mqttReconnectInterval)
    {
      lastMqttReconnectAttempt = now;
      reconnectMQTT();
    }
  }
  else
  {
    mqttClient.loop();
  }
  if (dataHandlerInstance.currentState == DataHandler::SNIFFING)
  {
    dataHandlerInstance.sniffer(); // Call the sniffer method
  }
  unsigned long currentMillis = millis();
  if (currentMillis - lastMqttPublishTime > mqttPublishInterval)
  {
    dataHandlerInstance.requestType = RequestType::CURRENT;
    lastMqttPublishTime = currentMillis;  
    String publishTopic = getPublishTopic(dataHandlerInstance.requestType);
    String data = dataHandlerInstance.dataHandler(dataHandlerInstance.requestType);
    if (data.length() > 0){
      mqttClient.publish(publishTopic.c_str(), data.c_str());
    }
    
  }
}
