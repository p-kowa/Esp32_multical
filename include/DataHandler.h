#ifndef DATAHANDLER_H
#define DATAHANDLER_H
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <HardwareSerial.h>

// Define the enum
enum RequestType {
    CURRENT,
    DAILY,
    MONTHLY,
    YEARLY,
    UNKNOWN,
    };

struct SerialSettings {
    uint8_t rx_pin;
    uint8_t tx_pin;
    long baud_rate;
};

struct KamData {
    RequestType answerType = RequestType::CURRENT;
    String WaterConsumption = "0";
    String WaterFlow = "0";
    String WaterFlowMax = "0";
    String WaterFlowMin = "0";
    String WaterTempMin = "0";
    String WaterTempMax = "0";
    String WaterTempAvg = "0";
    String DeviceTempAvg = "0";
    String DeviceTempMax = "0";
    String DeviceTempMin = "0";
    String WaterTemp = "0";
    String DeviceTemp = "0";
    String SerialNumber = "0";

    String toJson() const {
        String typeStr;
        String json ="";
        switch(answerType) {
            case RequestType::CURRENT:  
            typeStr = "Current";
            json = "{";
            json += "\"AnswerType\":\"" + typeStr + "\",";
            json += "\"WaterConsumption\":" + String(WaterConsumption) + ",";
            json += "\"WaterFlow\":" + String(WaterFlow) + ",";
            json += "\"WaterTemp\":" + String(WaterTemp) + ",";
            json += "\"DeviceTemp\":" + String(DeviceTemp) + ",";
            json += "\"SerialNumber\":\"" + SerialNumber + "\"";
            json += "}"; 
            break;
            case RequestType::DAILY:
            typeStr = "Daily";
            json = "{";
            json += "\"AnswerType\":\"" + typeStr + "\",";
            json += "\"WaterConsumption\":" + String(WaterConsumption) + ",";
            json += "\"WaterFlowMax\":" + String(WaterFlowMax) + ",";
            json += "\"WaterTempAvg\":" + String(WaterTempAvg) + ",";
            json += "\"DeviceTempAvg\":" + String(DeviceTempAvg) + ",";
            json += "\"SerialNumber\":\"" + SerialNumber + "\"";
            json += "}";    
            break;
            case RequestType::MONTHLY:
            typeStr = "Monthly";
            json = "{";
            json += "\"AnswerType\":\"" + typeStr + "\",";
            json += "\"WaterConsumption\":" + String(WaterConsumption) + ",";
            json += "\"WaterFlowMax\":" + String(WaterFlowMax) + ",";
            json += "\"WaterTempAvg\":" + String(WaterTempAvg) + ",";
            json += "\"DeviceTempAvg\":" + String(DeviceTempAvg) + ",";
            json += "\"SerialNumber\":\"" + SerialNumber + "\"";
            json += "}";   
            break;
            case RequestType::YEARLY:   
            typeStr = "Yearly";
            json = "{";
            json += "\"AnswerType\":\"" + typeStr + "\",";
            json += "\"WaterConsumption\":" + String(WaterConsumption) + ",";
            json += "\"WaterFlowMax\":" + String(WaterFlowMax) + ",";
            json += "\"WaterTempMax\":" + String(WaterTempMax) + ",";
            json += "\"WaterTempMin\":" + String(WaterTempMin) + ",";
            json += "\"DeviceTempMax\":" + String(DeviceTempMax) + ",";
            json += "\"DeviceTempMin\":" + String(DeviceTempMin) + ",";
            json += "\"SerialNumber\":\"" + SerialNumber + "\"";
            json += "}";  
            break;
            default:
            typeStr = "Unknown";
            break;
        }
            
        return json;
    }
};

class DataHandler {
public:
    // The state machine enum now belongs to this class
    enum CommunicationState {
        SNIFFING,
        DATAHANDLING,
        ERROR_STATE
    };
    

    // Public members that need to be accessed from mainapp.cpp
    // WiFiClient espClient;
    // PubSubClient mqttClient;
    // AsyncWebServer server;
    // AsyncWebSocket ws;
    CommunicationState currentState;
    RequestType requestType;
    String dataHandlerResult;
    explicit DataHandler(HardwareSerial& serial, AsyncWebSocket& ws, SerialSettings& serSettings);

    // Constructor takes configuration to set up its internal components
    //DataHandler(uint8_t rx_pin, uint8_t tx_pin, long baud_rate);
    void sendToWebUI(const String& message);

    // Public methods (the main interface for the class)
    void sniffer();
    String dataHandler(RequestType requestType);


private:
    // Private helper methods (internal implementation)
    void processAndPublishBuffer(uint8_t *buffer, size_t length, const char *directionLabel);
    size_t sendRequestAndReadResponse(const uint8_t* request, size_t request_len, const char* request_name);
    void handleSerialBridge(Stream &source, Stream &dest, uint8_t *buffer, size_t &bufferLen, const char *directionLabel);
    uint16_t crc_1021(const uint8_t *data, size_t length);

    // Internal hardware and buffers are now private members
    HardwareSerial SerialPortOpticalEye;
    String getCurrentData();
    void bufferToString(size_t bytesRead, String &firstResponse);
    String getDailyData();
    String getMonthlyData();
    String getYearlyData();
    void getSerialNumber(size_t bytesRead, String &serialNumber);
    String getDecimalValue(size_t bytesRead, size_t start_index, size_t end_index);
    String getSingleDecimalValue(const uint8_t* buffer, int pos);
    AsyncWebSocket& ws;
    SerialSettings& serialSettings;
};

#endif // DATAHANDLER_H