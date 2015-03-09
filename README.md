RFM69-MQTT-client
================

Arduino - RFM69 based sensors and MQTT gateway 

Changes in Gateway version 2.2:
- changed handling of binary input devices to improve compatibility with Openhab
- improved RSSI reporting


Changes in Gateway version 2.1:
- uniform handling of binary input devices
- transparant exchange of string data (device 72) added
- minor bug fixing

Changes in Gateway version 2:
- increased data exchange data block
- implemented standard device numbering
- removed leading spaces in decimal sensor data
- implemented uptime counter
- implemented error checking & reporting
- implemented uptime and version reporting in gateway

____________________________________________________________________________________________________________________
The setup consists of two types of devices:

- an end node that measures parameters, changes output states and communicates over radio with a central gateway
- a gateway node that connects to end node over wireless and publishes/receives MQTT messages to/from a broker.

The communication between MQTT broker and multiple end nodes is duplex, meaning:
- The state of an end node device/parameter can be queried thru the broker at any time
- The end node has outputs, so external devices can be controlled over MQTT
- Sensor commands can be acknowledged by the end node
- Loss of radio signal to a certain end node is indicated to the MQTT broker
- End nodes can send sensor data on regular intervals, on incoming query or event-based
- MQTT message syntax can easily be altered by only changing the gateway code
- Multiple end nodes can be addressed by a single gateway

Communication between gateway and nodes is over an encrypted wireless link and handshake mechanisms are used to guarantee succesfull transmission. Radio link errors are reported to the MQTT broker.
Communication between gateway and MQTT broker is over fixed ethernet.

The end node software is capable of:
- switching one or more outputs (LED, relay)
- measuring temperature, humidity, battery voltage
- set transmission interval in seconds
- switch off automatic regular transmission
- report software version
- report radio signal strength
- monitor button presses 
- set button action to local toggle, local timer or generation of an MQTT event message
- set button press timer interval in seconds

The gateway software is capable of:
- Communicating with end nodes and with the MQTT broker
- Automatically set up connetions tp MQTT broker and end nodes
- Translate the internal message format to MQTT messages and vice versa
- Detect radio signal loss and report this to the MQTT broker
- Publish end node events in a northbound message stream
- Subscribe to a southbound message stream to receive, parse messages and address the corresponding end node
- Manually produce event queries in debug mode

