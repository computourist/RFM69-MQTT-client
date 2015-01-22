// RFM69 MQTT gateway sketch
//
// This gateway relays messages between a MQTT-broker and several wireless nodes and will:
// - receive sensor data from several nodes periodically and on-demand
// - send/receive commands from the broker to control actuators and node parameters
//
//	Connection to the MQTT broker is over a fixed ethernet connection:
//
//		The MQTT topic is /home/rfm_gw/direction/nodeid/devid
//		where direction is: southbound (sb) towards the remote node and northbound (nb) towards MQTT broker
//
//	Connection to the nodes is over a closed radio network:
//
//		RFM Message format is: nodeID/deviceID/command/integer/float/string
//		where Command = 1 for a read request and 0 for a write request
//
//	Current defined devices are:
//	0	error:			Tx only: error message if no wireless connection 
//	1	node:			read/set transmission interval in seconds, 0 means no periodic transmission
//	2	RSSI:			read reception strength
//	3	Version:		read version node software
//	4	voltage:		read battery level
//	5	ACK:			read/set acknowledge message after 'SET' request
//	6	toggle:			read/set toggle function on button press
//	7	timer:			read/set activation timer after button press in seconds, 0 means no timer
//	8	buttonpress:		read/set flag to send a message when button pressed
//
//	10	actuator:		read/set LED or relay output
//	20	Button:			Tx only: message sent when button pressed
//	31	temperature:		read temperature
//	32	humidity:		read humidity
//	99	wakeup:			Tx only: sends a message on node startup
//
//	==> Note: 
//		- Interrupts are disabled during ethernet transactions in w5100.h (ethernet library)
//		  (See http://harizanov.com/2012/04/rfm12b-and-arduino-ethernet-with-wiznet5100-chip/)
//		- Ethernet card and RFM68 board default use the same Slave Select pin (10) on the SPI bus;
//		  To avoid conflict the RFM module is controlled by another SS pin (8).
//
//
// RFM69 Library by Felix Rusu - felix@lowpowerlab.com
// Get the RFM69 library at: https://github.com/LowPowerLab/s
//
// version 1.8 by Computourist@gmail.com december 2014
// version 1.9 fixed resubscription after network outage  Jan 2015


#include <RFM69.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

//#define DEBUG					// uncomment for debugging
#define VERSION "GW V1.9"

// Ethernet settings
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xBA, 0xBE };	// MAC address for ethernet
byte mqtt_server[] = { 192, 168, xxx, xxx };		// MQTT broker address
byte ip[] = { 192, 168, xxx, xxx };			// Gateway address (if DHCP fails)


// Wireless settings
#define NODEID 1 				// unique node ID in the closed radio network; gateway is 1
#define RFM_SS 8				// Slave Select RFM69 is connected to pin 8
#define NETWORKID 100				// closed radio network ID

//Match frequency to the hardware version of the radio (uncomment one):
//#define FREQUENCY RF69_433MHZ
#define FREQUENCY RF69_868MHZ
//#define FREQUENCY RF69_915MHZ

#define ENCRYPTKEY "xxxxxxxxxxxxxxxx" 		// shared 16-char encryption key is equal on Gateway and nodes
#define IS_RFM69HW 				// uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ACK_TIME 50 				// max # of ms to wait for an ack

// PIN settings
#define MQCON 7					// MQTT Connection indicator
#define R_LED 9					// Radio activity indicator
#define SERIAL_BAUD 115200

typedef struct {				// Radio packet structure
int		nodeID;				// node identifier
int		devID;				// device identifier 0 is node; 31 is temperature, 32 is humidity
int		cmd;				// read or write
int		intVal;				// integer payload
float		fltVal;				// floating payload
char		payLoad[10];			// char array payload
} Message;

Message mes;

#ifdef DEBUG
bool	act1Stat = false;			// remember LED state in debug mode
bool	msgToSend = false;			// message request by debug action
int	curstat = 0;				// current polling interval in debug mode
int	stat[] = {0,1,6,20};			// status 0 means no polling, 1, 6, 20 seconds interval
#endif							

int	dest;					// destination node for radio packet
bool	Rstat = false;				// radio indicator flag
bool	mqttCon = false;			// MQTT broker connection flag
bool	respNeeded = false;			// MQTT message flag in case of radio connection failure
bool	mqttToSend = false;			// message request issued by MQTT request
bool	promiscuousMode = false;		// only receive closed network nodes
long	onMillis;				// timestamp when radio LED was turned on
char	*subTopic = "home/rfm_gw/sb/#";		// MQTT subscription topic ; direction is southbound
char	*clientName = "RFM_gateway";		// MQTT system name of gateway
char	buff_topic[30];				// MQTT publish topic string
char	buff_mess[30];				// MQTT publish message string


RFM69 radio;
EthernetClient ethClient;
PubSubClient mqttClient(mqtt_server, 1883, mqtt_subs, ethClient );

//
//==============	SETUP
//

void setup() {
#ifdef DEBUG
	Serial.begin(SERIAL_BAUD);
#endif
radio.setCS(RFM_SS);					// change default Slave Select pin for RFM
radio.initialize(FREQUENCY,NODEID,NETWORKID);		// initialise radio module
#ifdef IS_RFM69HW
radio.setHighPower(); 					// only for RFM69HW!
#endif
radio.encrypt(ENCRYPTKEY);				// encrypt with shared key
radio.promiscuous(promiscuousMode);			// listen only to nodes in closed network

pinMode(R_LED, OUTPUT);					// set pin of radio indicator
pinMode(MQCON, OUTPUT);					// set pin for MQTT connection indicator
digitalWrite(MQCON, LOW);  				// switch off MQTT connection indicator
digitalWrite(R_LED, LOW);				// switch off radio indicator


#ifdef DEBUG
	Serial.print("Gateway Software Version ");
	Serial.println(VERSION);
	Serial.print("\nListening at ");
	Serial.print(FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
	Serial.println(" Mhz...");
#endif


	if (Ethernet.begin(mac) == 0) {			// start the Ethernet connection
#ifdef DEBUG
	Serial.println("Failed to configure Ethernet using DHCP");
#endif
	Ethernet.begin(mac, ip);
}

#ifdef DEBUG
	Serial.println("connecting...");
#endif

	delay(1000);
	mqttCon = 0;						// reset connection flag
	while(mqttCon != 1){					// retry MQTT connection every 2 seconds
#ifdef DEBUG
	Serial.println("connection failed...");
#endif
	mqttCon = mqttClient.connect(clientName);		// retry connection to broker
	delay(2000);						// every 2 seconds
	}

	if(mqttCon){						// Connected !
#ifdef DEBUG
	Serial.println("got connection with MQTT server");
#endif
	digitalWrite(MQCON, HIGH);				// switch on MQTT connection indicator
	mqttClient.subscribe(subTopic);				// subscribe to all southbound messages
	}
#ifdef DEBUG
	else Serial.println("no connection with MQTT server");
#endif
}	// end setup

//
//==============	MAIN
//

void loop() {

#ifdef DEBUG
	if (!mqttToSend && Serial.available() > 0) 	// do not interfere with mqtt traffic
		{ msgToSend = serialInput();}		// get any human input in debug mode
	if (msgToSend) {sendMsg(dest);}			// send serial input instruction packets
#endif


if (Rstat) {						// turn off radio LED after 100 msec
		if (millis() - onMillis > 100) {
		Rstat = false;
		digitalWrite(R_LED, LOW);
		}
}

if (mqttToSend) {sendMsg(dest);}		// send MQTT instruction packets over the radio network

if (radio.receiveDone()) { processPacket();}	// check for received radio packets and construct MQTT message

if (!mqttClient.loop()) {			// check connection MQTT server and process MQTT subscription input
	mqttCon = 0;
	digitalWrite(MQCON, LOW);
	while(mqttCon != 1){			// try to reconnect every 2 seconds
		mqttCon = mqttClient.connect(clientName);
		delay(2000);
	}
	if(mqttCon){				// Yes, we have a link so,
		digitalWrite(MQCON, mqttCon);	// turn on MQTT link indicator and
		mqttClient.subscribe(subTopic);	// re-subscribe to mqtt topic
	}
}
}	// end loop



//
//==============	SENDMSG
//
//	sends messages over the radio network

void sendMsg(int target) {

Rstat = true;						// radio indicator on
digitalWrite(R_LED, HIGH);				// turn on radio LED
onMillis = millis();					// store timestamp

int i = 5;						// number of transmission retries

while (respNeeded && i>0) {				// first try to send packets

if (radio.sendWithRetry(target, (const void*)(&mes), sizeof(mes),5)) {
	respNeeded = false;
#ifdef DEBUG
	Serial.print("Message sent to node " );
	Serial.println(target);
#endif
	} else delay(500);				// half a second delay between retries
	i--;
}

if (respNeeded) { 					// if not succeeded in sending packets after 5 retries
	sprintf(buff_topic, "home/rfm_gw/nb/node%02d/dev00", target);	// construct MQTT topic and message
	sprintf(buff_mess, "connection lost node %d", target);		// for radio loss (device 0)
	mqttClient.publish(buff_topic,buff_mess);			// publish ...
	respNeeded = false;						// reset response needed flag
#ifdef DEBUG
	Serial.println("No connection....");
#endif
}
 
if (mqttToSend) mqttToSend = false;					// reset send trigger
#ifdef DEBUG
	if (msgToSend) msgToSend = false;				// reset debug send trigger
#endif
}	// end sendMsg

//
//==============	PROCESSPACKET
//
// receives data from the wireless network, parses the contents and constructs MQTT topic and value

void processPacket() {
Rstat = true;							// set radio indicator flag 
digitalWrite(R_LED, HIGH);					// turn on radio LED
onMillis = millis();						// store timestamp

if (radio.DATALEN != sizeof(mes))				// wrong message size means trouble
#ifdef DEBUG
	Serial.println("invalid message structure..")
#endif
;
else								// message size OK...
{
	mes = *(Message*)radio.DATA;				// copy radio packet
								// and construct MQTT northbound topic
		
	sprintf(buff_topic, "home/rfm_gw/nb/node%02d/dev%02d", radio.SENDERID, mes.devID);	

#ifdef DEBUG
	Serial.print(radio.SENDERID);	Serial.print(", ");
	Serial.print(mes.devID);	Serial.print(", ");
	Serial.print(mes.cmd);	Serial.print(", ");
	Serial.print(mes.intVal);	Serial.print(", ");
	Serial.print(mes.fltVal);	Serial.print(", RSSI= ");
	Serial.println(radio.RSSI);	Serial.print("Node: ");
	Serial.print(mes.nodeID);	Serial.print("   Version:  ");
	for (int i=0; i<sizeof(mes.payLoad); i++) Serial.print(mes.payLoad[i]);
	Serial.println();
#endif	
}

switch (mes.devID)					// construct MQTT message, according to device ID
{
case (1): 						// Transmission interval
{	sprintf(buff_mess, "%d",mes.intVal);
}
break;

case (2):						// Signal strength
{	sprintf(buff_mess, "%d", radio.RSSI);
}
break;
case (3):						// Node software version
{int i; for (i=0; i<sizeof(mes.payLoad); i++){ 
	buff_mess[i] = (mes.payLoad[i]); 
}
}
break;
case (4):						// Node voltage
{	dtostrf(mes.fltVal, 5,2, buff_mess);
}
break;
case (5):						// ACK status
{	if (mes.intVal == 1 )sprintf(buff_mess, "ON");
	if (mes.intVal == 0 )sprintf(buff_mess, "OFF");
}
break;
case (6):						// Toggle status
{	if (mes.intVal == 1 )sprintf(buff_mess, "ON");
	if (mes.intVal == 0 )sprintf(buff_mess, "OFF");
}
break;
case (7): 						// Timer interval
{	sprintf(buff_mess, "%d",mes.intVal);
}
break;
case (8):						// Button ack status
{	if (mes.intVal == 1 )sprintf(buff_mess, "ON");
	if (mes.intVal == 0 )sprintf(buff_mess, "OFF");
}
break;
case (10):						// Actuator 
{	if (mes.intVal == 1 )sprintf(buff_mess, "ON");
	if (mes.intVal == 0 )sprintf(buff_mess, "OFF");
}
break;
case (20):						// Button pressed message
{	sprintf(buff_mess, "BUTTON PRESSED");
}
break;
case (31):						// temperature
{	dtostrf(mes.fltVal, 5,2, buff_mess);
}
break;
case (32):						// humidity
{	dtostrf(mes.fltVal, 5,2, buff_mess);
}
break;
case (99):						// wakeup message
{	sprintf(buff_mess, "NODE %d WAKEUP", mes.nodeID);
}
break;
}	// end switch

#ifdef DEBUG
Serial.print("MQTT message: ");
Serial.print(buff_topic);
Serial.print(": ");
Serial.println(buff_mess);
#endif

mqttClient.publish(buff_topic,buff_mess);		// publish MQTT message in northbound topic

if (radio.ACKRequested()) radio.sendACK();		// reply to any radio ACK requests

}	// end processPacket

//
//==============	MQTT_SUBS
//
//		receive messages from subscribed topics
//		parse MQTT topic / message and construct radio message
//
//		The values in the MQTT topic/message are converted to corresponding values on the Radio network
//

void mqtt_subs(char* topic, byte* payload, unsigned int length) {	

	mes.nodeID = NODEID;				// gateway is node 1
	mes.fltVal = 0;
	mes.intVal = 0;
	mqttToSend = false;				// not a valid request yet...

#ifdef DEBUG
	Serial.println("Message received from Mosquitto...");
	Serial.println(topic);
#endif

if (strlen(topic) == 27) {				// correct topic length ?
	dest = (topic[19]-'0')*10 + topic[20]-'0';	// extract target node ID from MQTT topic
	int DID = (topic[25]-'0')*10 + topic[26]-'0';	// extract device ID from MQTT topic
	payload[length] = '\0';				// terminate string with '0'
	String strPayload = String((char*)payload);	// convert to string
	mes.devID = DID;
	mes.cmd = 0;					// default is 'SET' value
	if (strPayload == "READ") mes.cmd = 1;		// in this case 'READ' value
	if (DID == 1) {
		mes.intVal = strPayload.toInt();	// polling interval is in MQTT message
		mqttToSend = true;
	}
	if (DID == 2 && mes.cmd ==1) mqttToSend = true;	// 'READ' request for signal strength
	if (DID == 3 && mes.cmd ==1) mqttToSend = true;	// 'READ' request for node software version
	if (DID == 4 && mes.cmd ==1) mqttToSend = true;	// 'READ' request for node supply voltage
	
	if (DID == 5) {					// 'SET' or 'READ' ACK status
		mqttToSend = true;
		if (strPayload == "ON") mes.intVal = 1;			// payload value is state 
		else  if (strPayload == "OFF") mes.intVal = 0;
		else if (strPayload != "READ") mqttToSend = false;	// invalid payload; do not process
	}
	if (DID == 6) {					// 'SET' or 'READ' toggle status
		mqttToSend = true;
		if (strPayload == "ON") mes.intVal = 1;			// payload value is state 
		else  if (strPayload == "OFF") mes.intVal = 0;
		else if (strPayload != "READ") mqttToSend = false;	// invalid payload; do not process
	}
	if (DID == 7) {
		mes.intVal = strPayload.toInt();	// timer interval is in MQTT message
		mqttToSend = true;
	}
if (DID == 8) {						// 'SET' or 'READ' button ACK status
		mqttToSend = true;
		if (strPayload == "ON") mes.intVal = 1;			// payload value is state 
		else  if (strPayload == "OFF") mes.intVal = 0;
		else if (strPayload != "READ") mqttToSend = false;	// invalid payload; do not process
	}
	if (DID >= 10 && DID <= 20) {			// device 10->20 are actuators
		mqttToSend = true;
		if (strPayload == "ON") mes.intVal = 1;			// payload value is state 
		else  if (strPayload == "OFF") mes.intVal = 0;
		else if (strPayload != "READ") mqttToSend = false;	// invalid payload; do not process
	}
	if (DID == 31 && mes.cmd ==1) mqttToSend = true;		// 'READ' request for temperature
	if (DID == 32 && mes.cmd ==1) mqttToSend = true;		// 'READ' request for humidity
	
	respNeeded = mqttToSend;			// valid request needs radio response
#ifdef DEBUG
	Serial.println(strPayload);
	Serial.print("Value is:  ");
	Serial.println(mes.intVal);
#endif


}
#ifdef DEBUG
else Serial.println("wrong message format in MQTT subscription.");
#endif

} // end mqttSubs

//
//	SERIALINPUT, only used in debugging mode
//
//	some commands can be given thru the terminal to simulate actions
// 'l' will toggle LED
// 's' will toggle tranmsission interval between 4 states
// 'v' will request voltage to be sent
//

#ifdef DEBUG
bool serialInput() {				// get and process manual input
bool msgAvail = false;
char input = Serial.read();
dest = 2;					// debug against fixed node 2
						// comment out to debug against node that last sent respons
mes.nodeID = radio.SENDERID;			// initialize
mes.devID = 0;
mes.intVal = 0;
mes.fltVal = 0;
mes.cmd = 0;
if (input == 'l')						// toggle LED
{
	msgAvail = true;
	respNeeded = true;
	act1Stat = !act1Stat;
	mes.devID = 10;
	if (act1Stat) mes.intVal = 1; else mes.intVal =0;
	Serial.print("Current LED status is ");
	Serial.println(act1Stat);
}
if (input == 'v')						// read voltage
{
	msgAvail = true;
	respNeeded = true;
	mes.cmd = 1;
	mes.devID = 4;
	Serial.println("Voltage requested ");

}
if (input == 's')						// set polling interval
{
	msgAvail = true;
	respNeeded = true;
	curstat++;
	if (curstat == 4) curstat = 0;
	mes.devID = 1;
	mes.intVal = stat[curstat];
	Serial.print("Current polling interval is ");
	Serial.println(mes.intVal);

}
return msgAvail;
}	// end serialInput
#endif




