# Chronograph
This is the codebase for the Chronograph project, by DS Guitar Engineering.  The goal of the project was to create a device with clock, stopwatch, and countdown timer functionality in a pedalboard friendly package.  The pedal needed to be as compact as possible therefore it is in a small plastic enclosure and only uses one footswitch for all needed user input.  An internal battery is necessary to maintain a running clock.  External 9VDC power is required for the microcontroller and display.

## Getting Started

Follow these instructions to build a Chronograph development board.  See the depolyment section if you wish to use the official Chronograph hardware for development.

### Prerequisites

The following is a minimum BOM needed to make a Chronograph development board.  Links are provided for the DIP variant (if available) for simplified breadboard wiring.  The official hardware utilizies 100% surface mount packages.
* Any [ATtiny85](http://www.mouser.com/ProductDetail/Microchip-Technology-Atmel/ATtiny85-20PU/?qs=sGAEpiMZZMtkfMPOFRTOl5CRAVRAdtfp) variant
* Any variant of the [DS1307](http://www.mouser.com/ProductDetail/Maxim-Integrated/DS1307+/?qs=sGAEpiMZZMsWkX3fPoxIPao0OKuDwxf4) RTC
* [AS1115](http://www.mouser.com/Search/ProductDetail.aspx?R=AS1115-BSSTvirtualkey58040000virtualkey985-AS1115-BSST) display driver
* [QSOP-24 Breakout Board](https://www.digikey.com/products/en?mpart=PA0030&v=315)
* Avago/Broadcom [HDSP-B09G](https://www.mouser.com/Search/ProductDetail.aspx?R=HDSP-B09Gvirtualkey63050000virtualkey630-HDSP-B09G) clock display
* [78L05 Regulator](http://www.mouser.com/ProductDetail/STMicroelectronics/L78L05CZ/?qs=sGAEpiMZZMvHdo5hUx%252bJYu5Iq5FsYDe%252b), 5VDC, 100mA minimum
* Resistors: (2) 4k7, (1) 10k, (1) 24k
* Capacitors: (2) 1u, (3) 0.1u
* Any SPST momentary switch
* Any AVR programmer supported by the Arduino IDE.  You may also use an Arduino board as a programmer by following [these instructions](https://www.arduino.cc/en/Tutorial/ArduinoToBreadboard).  Follow the "Minimal Circuit" example.

### Installing

Follow these steps to wire and write firmware to the Chronograph breadboard.

#### Hardware Setup
1. Use the schematic to wire the breadboard as shown.

#### Software Setup
1. Download and install the [Arduino IDE](https://www.arduino.cc/en/Main/Software).
2. Install the necessary boards and libraries.
3. Select "ATTiny85 (8MHz internal clock)" for the board.
4. Select your programmer or "ArduinoAsISP".

#### Uploading the Firmware
1. Open the Chronograph_v1.0.2.ino file.
2. Burn the bootloader to the chip (only necessary for first upload).
3. Compile and upload the sketch.


## Deployment

Follow these steps to write firmware to the official Chronograph hardware.  Your hardware version is printed on the PCB.

### Hardware Version 1.1 or older

1. Carefully use desoldering tools to remove the ATTiny85 IC.
2. I recommend using a new uC to upload new firmware.
3. Insert the uC in your breadboard or programmer and follow the same steps in "Uploading the Firmware" above.

## Contributing

Please initiate contact through our [official website](https://ds.engineering/contact) if you're interested in contributing to this project.

## Versioning

We use [SemVer](http://semver.org/) for versioning. For the versions available, see the [tags on this repository](https://github.com/DSGuitarEngineering/Chronograph/tags). 

## Authors

* **Daniel Shields** - *Original Author* - [HungryCow212](https://github.com/HungryCow212)

See also the list of [contributors](https://github.com/DSGuitarEngineering/Chronograph/contributors) who participated in this project.

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

* Ryan, co-host of the [60 Cycle Hum podcast](https://60cyclehumcast.com/) for inspiration
* [Adafruit](https://github.com/Adafruit) for software libraries and other incredible resources
