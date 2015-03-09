RFM-MQTT-LCD Release 2.0 

changed the way binary inputs behave. 
A message is sent at every state change, ON or OFF depending on the state of the binary input.
This end node will function with gateway V2.2.

See description of Gateway 2.2 on how to configure binary inputs in openhab.



RFM-MQTT-LCD Release 1.0 

by Computourist@gmail.com Feb 2015

This end node has an LCD display and two buttons. 

The display is of type HD44780 and is available in different sizes and colors.
The number of columns and rows can be adjusted, according to display type.
Connections to the Arduino are mentioned in the heading of the sketch.

The MQTT-message for displaying a text on the display is:

topic: home/rfm_gw/sb/nodexx/dev72
message: y:message text					where y is the line number.

The node will respond with:

topic: home/rfm_gw/nb/nodexx/dev72
message: text received

The two buttons will generate a message when pressed. A holdoff time prevents flooding of the network.

The message generated at button presses is:

topic: home/rfm_gw/nb/nodexx/dev40
message: Binary input activated

Note: 
1- The buttons are connected to the Arduino pins that are normally used for the serial connection.
These buttons cannot be used in DEBUG mode. Compilation for DEBUG mode will not compile the code part for the switches.

2- This end node needs version 2.1 of the gateway

