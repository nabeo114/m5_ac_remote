# M5StickC AC Remote

This project uses the M5StickC to control a Mitsubishi air conditioner via AWS IoT Device Shadow, enabling remote control of the air conditioner. It also collects environmental data (temperature, humidity, and pressure) using the ENV2 Unit and stores it in InfluxDB Cloud. This sample code demonstrates how to remotely control your air conditioner and monitor environmental data using IoT technology.

## Setup Instructions

### Prerequisites

- **Arduino IDE**: Install the Arduino IDE.
- **Board Configuration**:
  - In the Boards Manager, install **esp32** version 2.0.17.
  - Select **M5Stick-C** as the board (Tools > Board > esp32 > M5Stick-C).
- **Libraries**:
  - Install **M5StickC** version 0.3.0
  - Install **PubSubClient** version 2.8
  - Install **ArduinoJson** version 7.2.0
  - Install **Adafruit SHT31 Library** version 2.2.2
  - Install **Adafruit BMP280 Library** version 2.6.8
  - Install **IRremoteESP8266** version 2.7.12 for infrared control of Mitsubishi air conditioners.
  - Install **ESP8266 Influxdb** version 3.13.2

### Configuration Instructions

1. **Modify `m5_ac_remote.h`**:

   Open the `m5_ac_remote.h` file and update the configuration settings to match your environment. Ensure the following values are properly set:

- **WiFi Settings**:

  Update `WIFI_SSID` and `WIFI_PASSWORD` with your Wi-Fi credentials.

- **AWS IoT Settings**:

  Update `MQTT_ENDPOINT`, `MQTT_SHADOW_UPDATE_TOPIC`, and other MQTT-related constants with your AWS IoT configuration.

- **InfluxDB Settings**:

  Update `INFLUXDB_URL`, `INFLUXDB_ORG`, `INFLUXDB_BUCKET`, and `INFLUXDB_TOKEN` to match your InfluxDB environment.

- **Certificates and Keys**:
  
  Paste your AWS IoT Root CA, Client Certificate, and Private Key into the appropriate fields.

2. **Ensure Accurate Settings**:

    Ensure that all settings in `m5_ac_remote.h` are accurate for your environment. Missing or incorrect configurations can cause runtime errors.

## Usage Instructions

1. **Upload Code**:

- Open the project in Arduino IDE.
- Select the correct board and port (Tools > Board > esp32 > M5Stick-C).
- Upload the code to the M5StickC.

2. **Monitor Logs**:

- Open the Serial Monitor in Arduino IDE.
- Verify that the device connects to Wi-Fi, AWS IoT, and InfluxDB successfully.

3. **Control Air Conditioner**:

- Use the AWS IoT Device Shadow to update the air conditioner's settings remotely.
- Monitor sensor data (temperature, humidity, and pressure) sent to your InfluxDB instance.

## Notes

- The infrared control is specifically configured for Mitsubishi air conditioners. If using a different model, you may need to update the `IRsend` commands.
- Ensure all certificates and keys are kept secure and not exposed in public repositories.

## References

- [ESP32 AWS IoT Blog](https://blog.maripo.org/2017/07/esp32-aws-iot/)
- [TurnOnMitsubishiAC Example Code](https://github.com/crankyoldgit/IRremoteESP8266/blob/master/examples/TurnOnMitsubishiAC/TurnOnMitsubishiAC.ino)
- [Mitsubishi IR Codes](https://github.com/crankyoldgit/IRremoteESP8266/blob/master/src/ir_Mitsubishi.h)
