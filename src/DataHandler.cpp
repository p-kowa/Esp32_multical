#include "DataHandler.h"
#include <Arduino.h> // For Serial object
#include "KamstrupRequests.h"
#include <string.h>

// --- Constants ---
static const int MAX_BUFFER_SIZE = 256;

// --- Buffers ---
// These are now static to this file, as they are passed by reference to the helper.
static uint8_t optical_eye_send_buffer[MAX_BUFFER_SIZE];
static size_t optical_eye_send_len = 0;
static uint8_t optical_eye_read_buffer[MAX_BUFFER_SIZE];
static size_t optical_eye_read_len = 0;
static boolean debugMode = true;

// --- Constructor ---
DataHandler::DataHandler(HardwareSerial &serial, AsyncWebSocket &ws, SerialSettings &serSettings)
    : SerialPortOpticalEye(serial),
      ws(ws),
      serialSettings(serSettings)
{
  SerialPortOpticalEye.begin(serialSettings.baud_rate, SERIAL_8N1, serialSettings.rx_pin, serialSettings.tx_pin);
  Serial.begin(serialSettings.baud_rate); // Initialize the serial port for communication
}

// --- Public Methods ---

String DataHandler::dataHandler(RequestType requestType)
{

  String resultStr;
  DataHandler::currentState = DATAHANDLING;
  switch (requestType)
  {
  case RequestType::CURRENT:
    resultStr = getCurrentData();
    break;
  case RequestType::DAILY:
    resultStr = getDailyData();
    break;
  case RequestType::MONTHLY:
    resultStr = getMonthlyData();
    break;
  case RequestType::YEARLY:
    resultStr = getYearlyData();
    break;
  default:
    resultStr = "UNKNOWN";
    break;
  }

  this->currentState = SNIFFING;
  return resultStr;
}

void DataHandler::sniffer()
{
  // Handle PC -> Optical Eye
  handleSerialBridge(Serial, this->SerialPortOpticalEye,
                     optical_eye_send_buffer, optical_eye_send_len,
                     "PC to Eye");

  // Handle Optical Eye -> PC
  handleSerialBridge(this->SerialPortOpticalEye, Serial,
                     optical_eye_read_buffer, optical_eye_read_len,
                     "Eye to PC");
}

// --- Private Methods ---

void DataHandler::handleSerialBridge(Stream &source, Stream &dest, uint8_t *buffer, size_t &bufferLen, const char *directionLabel)
{
  
  static bool validPacket = false;
  bool isEyeToPC = (strcmp(directionLabel, "Eye to PC") == 0);

  while (source.available()) 
  {
    uint8_t byte = source.read();
    dest.write(byte);

    if (bufferLen == 0) {
      // Check for valid start byte based on direction
      if ((isEyeToPC && byte == 0x40) || (!isEyeToPC && byte == 0x80)) {
        validPacket = true;
      }
    }

    if (validPacket && bufferLen < MAX_BUFFER_SIZE) 
    {
      buffer[bufferLen++] = byte;
      if (byte == 0x0D || bufferLen == MAX_BUFFER_SIZE) 
      {
        // Verify correct start byte before publishing
        if ((isEyeToPC && buffer[0] == 0x40) || (!isEyeToPC && buffer[0] == 0x80)) {
          processAndPublishBuffer(buffer, bufferLen, directionLabel);
        }
        bufferLen = 0;
        validPacket = false;
      }
    } 
    else if (!validPacket) {
      bufferLen = 0;  // Reset if invalid start byte
    }
  }
  
}

void DataHandler::processAndPublishBuffer(uint8_t *buffer, size_t length, const char *directionLabel)
{
  String hexString = "";
  hexString.reserve(length * 3);

  for (size_t i = 0; i < length; i++)
  {
    if (buffer[i] < 16)
      hexString += "0";
      hexString += String(buffer[i], HEX);
      hexString += " ";
  }

  ws.textAll(String(directionLabel) + " -> : " + hexString);
}

// Kamstrup Multical 21 CRC-16/CCITT-FALSE (polynomial 0x1021, initial 0x0000)
uint16_t DataHandler::crc_1021(const uint8_t *data, size_t length)
{
  uint16_t crc = 0x0000; // Initial value (NOT 0xFFFF)

  for (size_t i = 0; i < length; ++i)
  {
    crc ^= (uint16_t)data[i] << 8; // XOR with next byte (shifted left)

    for (uint8_t bit = 0; bit < 8; ++bit)
    {
      if (crc & 0x8000)
      {
        crc = (crc << 1) ^ 0x1021; // XOR with polynomial
      }
      else
      {
        crc <<= 1;
      }
    }
  }

  return crc; // Returns big-endian CRC (e.g., 0xDAE4)
}

size_t DataHandler::sendRequestAndReadResponse(const uint8_t* request, size_t request_len, const char* request_name) {
  
    // Clear any old data
    while(this->SerialPortOpticalEye.available()) {
        this->SerialPortOpticalEye.read();
    }
    
    yield();
    
    // Send the request
    this->SerialPortOpticalEye.write(request, request_len);

    yield();
    delay(100); // Give device time to start responding

    // Read response until 0x0D or buffer full
    size_t bufferLen = 0;
    unsigned long startTime = millis();
    const unsigned long timeout = 1000; // 1 second timeout

    while(bufferLen < MAX_BUFFER_SIZE && (millis() - startTime) < timeout) {
        if (this->SerialPortOpticalEye.available()) {
            uint8_t byte = this->SerialPortOpticalEye.read();
            optical_eye_read_buffer[bufferLen++] = byte;
            
            if (byte == 0x0D) {
                break; // Found end of message
            }
        }
        yield(); // Prevent watchdog triggers
    }

    return bufferLen;
}

String DataHandler::getCurrentData(){
  
  KamData kamData;
  kamData.answerType = RequestType::CURRENT; // Set the answer type for correct JSON serialization
  size_t bytesRead;
  bytesRead = sendRequestAndReadResponse(initRequest, sizeof(initRequest), "Init 1");
  if (bytesRead == 0) { ws.textAll("No data for first initRequest of getCurrentData"); return ""; }
  kamData.SerialNumber = getDecimalValue(bytesRead, 8, 11);

  // Second init request to get Serial Number
  bytesRead = sendRequestAndReadResponse(currentRequest, sizeof(currentRequest), "currentRequest");
  if (bytesRead == 0) { ws.textAll("No data for second currentRequest of getCurrentData"); return ""; }
  kamData.WaterConsumption = getDecimalValue(bytesRead, 8, 11);
  kamData.WaterFlow = getDecimalValue(bytesRead, 17, 18);
  kamData.WaterTemp = getSingleDecimalValue(optical_eye_read_buffer, 24);
  kamData.DeviceTemp = getSingleDecimalValue(optical_eye_read_buffer, 30);

  //ws.textAll(kamData.toJson());
  return kamData.toJson();

} 


String DataHandler::getDailyData()
{
  KamData kamData;
  kamData.answerType = RequestType::DAILY; // Set the answer type for correct JSON serialization
  size_t bytesRead;

  // First init request
  bytesRead = sendRequestAndReadResponse(initRequest, sizeof(initRequest), "Init 1");
  if (bytesRead == 0) { ws.textAll("No data for first initRequest of getDailyData"); return ""; }

  // Second init request to get Serial Number
  bytesRead = sendRequestAndReadResponse(initRequest, sizeof(initRequest), "Init 2 (for SN)");
  if (bytesRead == 0) { ws.textAll("No data for second initRequest of getDailyData"); return ""; }
  kamData.SerialNumber = getDecimalValue(bytesRead, 8, 11);

  // Get Daily Water Consumption from last day
  bytesRead = sendRequestAndReadResponse(Volume_V1_DayRequest, sizeof(Volume_V1_DayRequest), "Volume_V1_Day");
  if (bytesRead == 0) { ws.textAll("No data for Volume_V1_DayRequest of getDailyData"); return ""; }
  kamData.WaterConsumption = getDecimalValue(bytesRead, 8, 11);

  // Get Daily Max Flow
  bytesRead = sendRequestAndReadResponse(FlowMax_DayRequest, sizeof(FlowMax_DayRequest), "FlowMax_Day");
  if (bytesRead == 0) { ws.textAll("No data for FlowMax_DayRequest of getDailyData"); return ""; }
  kamData.WaterFlowMax = getDecimalValue(bytesRead, 8, 9);

  // Get Daily Average Water Temp
  bytesRead = sendRequestAndReadResponse(TempWaterAvg_DayRequest, sizeof(TempWaterAvg_DayRequest), "TempWaterAvg_Day");
  if (bytesRead == 0) { ws.textAll("No data for TempWaterAvg_DayRequest of getDailyData"); return ""; }
  kamData.WaterTempAvg = getSingleDecimalValue(optical_eye_read_buffer, 8);

  // Get Daily Average Ambient Temp
  bytesRead = sendRequestAndReadResponse(TempMeterAvg_DayRequest, sizeof(TempMeterAvg_DayRequest), "TempMeterAvg_Day");
  if (bytesRead == 0) { ws.textAll("No data for TempMeterAvg_DayRequest of getDailyData"); return ""; }
  kamData.DeviceTempAvg = getSingleDecimalValue(optical_eye_read_buffer, 8);
  
  //ws.textAll(kamData.toJson());
  return kamData.toJson();
}

String DataHandler::getMonthlyData() {
    KamData kamData;
    kamData.answerType = RequestType::MONTHLY;
    size_t bytesRead;

    // First init request
    bytesRead = sendRequestAndReadResponse(initRequest, sizeof(initRequest), "Init 1");
    if (bytesRead == 0) { ws.textAll("No data for first initRequest of getMonthlyData"); return ""; }

    // Second init request to get Serial Number
    bytesRead = sendRequestAndReadResponse(initRequest, sizeof(initRequest), "Init 2 (for SN)");
    if (bytesRead == 0) { ws.textAll("No data for second initRequest of getMonthlyData"); return ""; }
    kamData.SerialNumber = getDecimalValue(bytesRead, 8, 11);

    // Get Monthly Water Consumption from last day of previous month
    bytesRead = sendRequestAndReadResponse(Volume_V1_MonthRequest, sizeof(Volume_V1_MonthRequest), "Volume_V1_Month");
    if (bytesRead == 0) { ws.textAll("No data for Volume_V1_MonthRequest"); return ""; }
    kamData.WaterConsumption = getDecimalValue(bytesRead, 8, 11);

    bytesRead = sendRequestAndReadResponse(FlowMax_MonthRequest, sizeof(FlowMax_MonthRequest), "FlowMax_Month");
    if (bytesRead == 0) { ws.textAll("No data for FlowMax_MonthRequest"); return ""; }
    kamData.WaterFlowMax = getDecimalValue(bytesRead, 8, 9);

    bytesRead = sendRequestAndReadResponse(TempWaterAvg_MonthRequest, sizeof(TempWaterAvg_MonthRequest), "TempWaterAvg_Month");
    if (bytesRead == 0) { ws.textAll("No data for TempWater Avg_MonthRequest"); return ""; }
    kamData.WaterTempAvg = getSingleDecimalValue(optical_eye_read_buffer, 8);

    bytesRead = sendRequestAndReadResponse(TempMeterAvg_MonthRequest, sizeof(TempMeterAvg_MonthRequest), "TempMeterAvg_Month");
    if (bytesRead == 0) { ws.textAll("No data for TempMeter Avg_MonthRequest"); return ""; }
    kamData.DeviceTempAvg = getSingleDecimalValue(optical_eye_read_buffer, 8  );

    //ws.textAll(kamData.toJson());
    return kamData.toJson();
}

String DataHandler::getYearlyData()
{
 KamData kamData;
    kamData.answerType = RequestType::YEARLY;
    size_t bytesRead;

    // First init request
    bytesRead = sendRequestAndReadResponse(initRequest, sizeof(initRequest), "Init 1");
    if (bytesRead == 0) { ws.textAll("No data for first initRequest of getYearlyData"); return ""; }

    // Second init request to get Serial Number
    bytesRead = sendRequestAndReadResponse(initRequest, sizeof(initRequest), "Init 2 (for SN)");
    if (bytesRead == 0) { ws.textAll("No data for second initRequest of getYearlyData"); return ""; }
    kamData.SerialNumber = getDecimalValue(bytesRead, 8, 11);

    // Get Yearly Water Consumption last day of previous year
    bytesRead = sendRequestAndReadResponse(Volume_V1_YearRequest, sizeof(Volume_V1_YearRequest), "Volume_V1_Year");
    if (bytesRead == 0) { ws.textAll("No data for Volume_V1_YearRequest"); return ""; }
    kamData.WaterConsumption = getDecimalValue(bytesRead, 8, 11);

    bytesRead = sendRequestAndReadResponse(FlowMax_YearRequest, sizeof(FlowMax_YearRequest), "FlowMax_Year");
    if (bytesRead == 0) { ws.textAll("No data for FlowMax_YearRequest"); return ""; }
    kamData.WaterFlowMax = getDecimalValue(bytesRead, 8, 9);

    bytesRead = sendRequestAndReadResponse(TempWaterMin_YearRequest, sizeof(TempWaterMin_YearRequest), "TempWaterMin_Year");
    if (bytesRead == 0) { ws.textAll("No data for TempWater Min_YearRequest"); return ""; }
    kamData.WaterTempMin  = getSingleDecimalValue(optical_eye_read_buffer, 8);

    bytesRead = sendRequestAndReadResponse(TempWaterMax_YearRequest, sizeof(TempWaterMax_YearRequest), "TempWater Max_Year");
    if (bytesRead == 0) { ws.textAll("No data for TempWater Max_YearRequest"); return ""; }
    kamData.WaterTempMax = getSingleDecimalValue(optical_eye_read_buffer, 8);

    bytesRead = sendRequestAndReadResponse(TempMeterMin_YearRequest, sizeof(TempMeterMin_YearRequest), "TempMeterMin_Year");
    if (bytesRead == 0) { ws.textAll("No data for TempMeter Min_YearRequest"); return ""; }
    kamData.DeviceTempMin = getSingleDecimalValue(optical_eye_read_buffer, 8);

    bytesRead = sendRequestAndReadResponse(TempMeterMax_YearRequest, sizeof(TempMeterMax_YearRequest), "TempMeterMax_Year");
    if (bytesRead == 0) { ws.textAll("No data for TempMeter Max_YearRequest"); return ""; } 
    kamData.DeviceTempMax = getSingleDecimalValue(optical_eye_read_buffer, 8);

    //ws.textAll(kamData.toJson());
    return kamData.toJson();
}

void DataHandler::bufferToString(size_t bytesRead, String &response)
{
  for (size_t i = 0; i < bytesRead; ++i)
  {
    if (optical_eye_read_buffer[i] < 16)
      response += "0";
    response += String(optical_eye_read_buffer[i], HEX);
    if (i < bytesRead - 1)
      response += " ";
  }
  response.toUpperCase();
}

String DataHandler::getDecimalValue(size_t bytesRead, size_t start_index, size_t end_index)
{
  if (start_index <= end_index && end_index < bytesRead)
  {
    uint8_t value_bytes[end_index - start_index + 1];
    memcpy(value_bytes, &optical_eye_read_buffer[start_index], end_index - start_index + 1);
    uint32_t decimalValue = 0;
    for (size_t i = 0; i < sizeof(value_bytes); ++i)
    {
      decimalValue = (decimalValue << 8) | value_bytes[i];
    }
    return String(decimalValue);
  }
  return String("0");
}

String DataHandler::getSingleDecimalValue(const uint8_t *buffer, int pos)
{
  return String(buffer[pos]);
}
