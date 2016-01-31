RFM69 RFID node sketch

Version 2.0

Lewishollow had a look at this code and proposed blocking interrupts at sketch level, rather than in the libraries.
The code is now stable; RFID cards are being reported reliably and the code doesn't crash any more.

Sending READ commands to the node is still not perfect: the blocking of interrupts sometimes causes radio packets to get lost. 
The gateway interprets this as a loss of signal of the radiolink. 

The unedited version of the MFRC522 library can be used.



Version 1.0

This node talks to the MQTT-Gateway and will:
- send sensor data periodically 
- detect and read RFID cards and send the UID to the MQTT broker; 

The node is stable when only reporting RFID UID codes and node-initiated data.
The node is unstable when trying to read values from the broker. 
Somehow the handling of received radio packets interferes with RFID code.

The MFRC522 library can be found at: https://github.com/miguelbalboa/rfid
Make sure to change the library file MFRC522.cpp before compiling:

	- add "cli();" to every occurrence of "digitalWrite(_chipSelectPin,LOW)"
	- add "sei();" to every occurrence of "digitalWrite(_chipSelectPin,HIGH)"

Hardware used is a 3.3 Volt 8MHz arduino Pro; this is easier to interface to RFM69 and RFID-RC522.
Power requirements dictate that a reparate 3.3 Volts regulator is used to power both RFM69 and RFID reader 
A RED led (D7) is used to indicate an RFID card has been detected and read. It will stay on during HOLDOFF msec.
 
Whenever an RFID card is detected am MQTT message will be generated with topic: 

	home/rfm_gw/nb/node05/dev72 

The messageblock will contain the detected RFID-UID in ASCII.
