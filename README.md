# Kamstrup Multical21 Reader for Node-Red or Home Assistant 
ESP32 based optical reader / sniffer for Kamstrup Multical21 water meters.

## Project Structure
- `/src` - Source code files
- `/include` - Header files
- `/3d_files` - 3D printable files for the optical reader housing
- `/lib` - Project libraries

## Requirements
- PlatformIO
- ESP32-C3 board
- 3D printer for housing
- TTL IR optical reading head with ring magnet (27mm)
- Basic soldering equipment

## Hardware Setup
### Optical Reading Head
The Kamstrup Multical21 requires a specific optical reading head setup:

- **Ring Magnet**: A 27mm ring magnet is **essential** for communication
  - The magnet activates the meter's optical interface
  - Without the magnet, the meter will not communicate

- **Reading Head Components**:
  - IR LED for transmission
  - IR phototransistor for receiving
  - TTL interface for ESP32 connection
  - Communication speed: 1200 baud
  - Black heat shrink tubing or similar over the receiver diode
    - Prevents IR interference from transmitter
    - Improves reading reliability

You can find compatible reading heads on eBay by searching for "TTL IR Lesekopf Volkszähler". These can come with the required ring magnet.

**Hardware Tips**:
- Shield the receiver diode with black tubing to prevent IR echoes
- Ensure proper alignment with the meter's optical interface
- Keep the ring magnet centered around the optical components (check OpticalEye-Body.stl in the 3d_files folder)

## Installation
1. Clone this repository
2. Open in PlatformIO
3. Build and upload to your ESP32

## Using the Application
### Web Interface
- View current water meter readings
- Access historical data:
  - Daily summaries
  - Monthly statistics
  - Yearly overview
- Monitor communication between meter and reader via WebSocket log
- Configure MQTT settings
- Reset WiFi settings if needed

### Communication Sniffing
The device can act as a protocol analyzer between the Kamstrup LogView Tool and the optical eye, helping to debug communication issues or adapt the code for different meter variants.

**Sniffing Mode Features**:
  - Captures all communication between LogView Tool and optical eye
  - Displays hex data in real-time via WebSocket interface
  - Distinguishes between request and response messages
  - Shows timestamps for each message

- **Message Format**:
  - PC → Eye requests: Start with `0x80`, end with `0x0D`
  - Multical21 → Eye responses: Start with `0x40`, end with `0x0D`

- **Usage**:
  1. Install Kamstrup LogView Tool (available after registration at Kamstrup website)
  2. Connect optical eye to ESP32
  3. Open web interface to view communication log
  4. Use LogView Tool to initiate requests
  5. Monitor and analyze the communication pattern

- **Troubleshooting**:
  - If your meter behaves differently, capture the communication
  - Use the logged data to adapt the code to your meter's protocol
  - Compare request/response patterns with working implementations

### MQTT Integration
- Publishes meter readings to configured MQTT topic
- Data includes:
  - Water consumption
  - Flow rate
  - Water temperature
  - Device temperature
  - Serial number

#### MQTT Commands
- Subscribe to topic: `<configured_topic>/input`
- Available commands:
  - Send `current` to receive daily statistics on `<configured_topic>/Current`
  - Send `daily` to receive daily statistics on `<configured_topic>/Daily`
  - Send `monthly` to receive monthly data on `<configured_topic>/Monthly`
  - Send `yearly` to receive yearly data on `<configured_topic>/Yearly`

Example:
- Configure topic: `multical21`
- Subscribe to: `multical21/input`
- Send payload: `daily`
- Receive data on: `multical21/Daily`

### Data Types
- **Current**: Real-time meter readings
- **Daily**: 24-hour statistics with averages
- **Monthly**: Month-to-date consumption and statistics
- **Yearly**: Annual consumption with min/max values



## License
This project is licensed under the MIT License – see the [LICENSE](LICENSE) file for details.

