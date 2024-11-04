# M5StickC AC Remote

This project utilizes the M5StickC to control a Mitsubishi air conditioner via infrared signals and AWS IoT Device Shadow, enabling remote management.

## Setup Instructions

### Prerequisites

- **Arduino IDE**: Install the Arduino IDE.
- **Board Configuration**:
  - In the Boards Manager, install **esp32** version 2.0.17.
  - Select **M5Stick-C** as the board (Tools > Board > esp32 > M5Stick-C).
- **Libraries**:
  - Install **IRremoteESP8266** version 2.7.12 for infrared control of Mitsubishi air conditioners.

## References

- [ESP32 AWS IoT Blog](https://blog.maripo.org/2017/07/esp32-aws-iot/)
- [TurnOnMitsubishiAC Example Code](https://github.com/crankyoldgit/IRremoteESP8266/blob/master/examples/TurnOnMitsubishiAC/TurnOnMitsubishiAC.ino)
- [Mitsubishi IR Codes](https://github.com/crankyoldgit/IRremoteESP8266/blob/master/src/ir_Mitsubishi.h)
