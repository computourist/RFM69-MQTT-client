RFM-MQTT-Gateway Release 2.2 

by Computourist@gmail.com March 2015

- changed handling of binary inputs.
In order to be more compatible with the way openhab handles messages the binary inputs were changed.
On every state transition a message is generated with "ON" or "OFF" in the message block, 
depending on the actual state of the input.

in Openhab a "contact" item needs to be declared:

Contact window "Status [%s]" {mqtt="<[mosquitto:home/rfm_gw/nb/node02/dev40:state:OPEN:ON],<[mosquitto:home/rfm_gw/nb/node02/dev40:state:CLOSED:OFF]"}

in the openhab Sitemap the following frame should be included to show the state of the binary input:

Frame label="Contacts" { Text item=window }

- fixed RSSI reporting
Originally the end node reported on signal strength. 
As long as no packets were being received from the gateway its value would remain constant.
Reporting RSSI by means of the end-node transmission interval would therefor always report the same value.
RSSI is now calculated by the reception strength of packets received in the gateway. 
Every time a packet is received from the end node (whether in push- or pull mode) a new and actual value is reported.

- partly removed DEBUG code
Memory restrictions forced the removal of part of the debug code. 
Voltage reporting by entering 'v' on serial input was removed.

Note that these changes should also be reflected in end nodes that have binary input:
- LCD node from version 2.0
- DHT node from version 2.1



RFM-MQTT-Gateway Release 2.1 

by Computourist@gmail.com Feb 2015

- implemented device 72 for transparant string transmission
- implemented uniform handling for binary input devices
- minor bugfixing
