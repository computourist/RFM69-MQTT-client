RFM-MQTT-RC Release 2.0 
by Computourist@gmail.com Oct 2015

Made this node compatible to gateway 2.3:
- made changes in function TXradio to handle retransmissions.
- added device 9 to expose the number of retransmission

RFM69 RC node sketch

This node talks to the MQTT-Gateway and will send out commands to RC-controlled mains switches.
I use devices of brand KlikAanKlikUit. The library used supports other European brands.

Hardware used is a 3.3 Volt 8MHz arduino Pro; this is easier to interface to RFM69 

A 433 MHZ transmitter is used to send out commands in the appropriate format.

Commands are sent as a string and have the following format:
KuKa XY ON of KuKa XY OFF where XY is the device address of the switch.

Example:

topic: home/rfm_gw/sb/node04/dev72 with message: KAKU B1 ON will switch the device with code B1 On.

The RemoteSwitch library by Randy Simons is used. 
Get this library at : https://bitbucket.org/fuzzillogic
