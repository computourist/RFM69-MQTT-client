#include <avr/wdt.h>
#include <avr/sleep.h>
#include <device.h>
#include <RFM69.h>
#include <SPI.h>

/**************************************
CONFIGURATION PARAMETERS
**************************************/

#define NODEID 2 				// unique node ID within the closed network
#define GATEWAYID 1				// node ID of the Gateway is always 1
#define NETWORKID 1				// network ID of the network
#define ENCRYPTKEY "1234567891011121" 		// 16-char encryption key; same as on Gateway!
#define DEBUG					// uncomment for debugging
#define VERSION "RELAY V1.0"			// this value can be queried as device 3
#define SERIAL_BAUD 9600

/**************************************
Wireless settings
Match frequency to the hardware version of the radio
**************************************/

//#define FREQUENCY RF69_433MHZ
//#define FREQUENCY RF69_868MHZ
#define FREQUENCY RF69_915MHZ

#define IS_RFM69HW 				// uncomment only for RFM69HW!
#define PROMISCUOUS_MODE false
#define ACK_TIME 30 				// max # of ms to wait for an ack
#define RETRIES 5                               //number of tx retries

/**************************************
device settings
**************************************/

#define BTN_INT 1                               //interrupt number
#define BTN_PIN 3                               //button pin
#define RELAY 9                                 //relay pin

/**************************************
global variables
**************************************/
int 	TXinterval = 20;			// periodic transmission interval in seconds
bool	setACK = false;				// send ACK message on 'SET' request
bool    toggle = true;

volatile bool btnPressed = false;
volatile long lastBtnPress = -1;		// timestamp last buttonpress
volatile long wdtCounter = 0;

const Message DEFAULT_MSG = {NODEID, 0, 0, 0, 0, VERSION};

/**************************************
configure devices
**************************************/

//Device name(devID, tx_periodically, read_function, optional_write_function)

Device uptimeDev(0, false, readUptime);
Device txIntDev(1, false, readTXInt, writeTXInt);
Device rssiDev(2, false, readRSSI);
Device verDev(3, false);
Device voltDev(4, false, readVoltage);
Device ackDev(5, false, readACK, writeACK);
Device toggleDev(6, false, readToggle, writeToggle);
Device relayDev(17, true, readRelay, writeRelay);

Device devices[] = {uptimeDev, txIntDev, rssiDev, verDev,
                    voltDev, ackDev, toggleDev, relayDev};
                    
RFM69 radio;

void setup() {
  //disable watchdog timer during setup
  wdt_disable();
  
  //set all pins as input with pullups, floating pins can waste power
  DDRD &= B00000011;       // set Arduino pins 2 to 7 as inputs, leaves 0 & 1 (RX & TX) as is
  DDRB = B00000000;        // set pins 8 to 13 as inputs
  PORTD |= B11111100;      // enable pullups on pins 2 to 7, leave pins 0 and 1 alone
  PORTB |= B11111111;      // enable pullups on pins 8 to 13
  
  //if debug enabled, initialize serial
  #ifdef DEBUG
    Serial.begin(SERIAL_BAUD);
    Serial.println("Serial initialized");
  #endif

  //initialize IO
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);
  
  //initialize radio
  radio.initialize(FREQUENCY,NODEID,NETWORKID);	
  radio.rcCalibration();
  #ifdef IS_RFM69HW
    radio.setHighPower(); 				// only for RFM69HW!
  #endif
  radio.encrypt(ENCRYPTKEY);				// set radio encryption	
  radio.promiscuous(PROMISCUOUS_MODE);			// only listen to closed network
  
  #ifdef DEBUG
    Serial.print("Node Software Version ");
    Serial.println(VERSION);
    Serial.print("\nTransmitting at ");
    Serial.print(FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
    Serial.println(" Mhz...");
  #endif
  
  //configure watchdog as 1s counter for uptime and to wake from sleep 
  watchdogSetup();
  
  //setup interrupt for button
  attachInterrupt(BTN_INT, buttonHandler, LOW);
  
  //send wakeup message
  Message wakeup = DEFAULT_MSG;
  wakeup.devID = 99;
  txRadio(&wakeup);
}

void loop() {
  Message reply = DEFAULT_MSG;
  
  if (radio.receiveDone()){
    if (radio.DATALEN != sizeof(Message)){
      //invalid packet format
    }else{
      Message mess = *(Message*)radio.DATA;
      if (radio.ACKRequested()){
        radio.sendACK();
      }
      bool match = false;
      
      //check if message is for any devices registered on node
      for (int i = 0; i <= sizeof(devices) / sizeof(Device); i++){
        if (mess.devID == devices[i].id){
          match = true;
          reply.devID = devices[i].id;
          //write for cmd 0
          if (mess.cmd == 0){
            devices[i].write(&mess);
            if (setACK){
              devices[i].read(&reply);
              txRadio(&reply);
            }
          //read for cmd 1
          }else if (mess.cmd == 1){
            devices[i].read(&reply);
            txRadio(&reply);
          }
        }
      }
      
      //check if any devices need to transmit periodic info
      if (wdtCounter % TXinterval == 0){
        Serial.println("Sending periodic updates");
        for (int i = 0; i <= sizeof(devices) / sizeof(Device); i++){
          if (devices[i].setTX){
            reply = DEFAULT_MSG;
            reply.devID = devices[i].id;
            devices[i].read(&reply);
            txRadio(&reply);
          }
        }
      }
  
      //invalid device id in message
      if (!match){
        reply.devID = 92;
        txRadio(&reply);
      }
    }
  }
  
  //if button was pressed and enabled toggle the relay
  if (btnPressed && toggle){
    digitalWrite(RELAY, !digitalRead(RELAY));
    reply = DEFAULT_MSG;
    reply.devID = 17;
    relayDev.read(&reply);
    txRadio(&reply);
  }
  btnPressed = false;

  #ifdef DEBUG
    //make sure serial tx is done before sleeping
    Serial.flush();
    while ((UCSR0A & _BV (TXC0)) == 0){}
  #endif
  
  //put chip to sleep until button is pressed, packet is RX, or watchdog timer fires
  sleep();
}

void txRadio(Message * mess){
  if (radio.sendWithRetry(GATEWAYID, mess, sizeof(*mess), RETRIES, ACK_TIME))
  #ifdef DEBUG
    {Serial.print(" message ");
    Serial.print(mess->devID);
    Serial.println(" sent...");}
    else Serial.println("No connection...")
  #endif
;}

void readUptime(Message *mess){
  mess->intVal = wdtCounter / 60;
}

void readTXInt(Message *mess){
  mess->intVal = TXinterval;
}

void writeTXInt(const Message *mess){
  TXinterval = mess->intVal;
  if (TXinterval <10 && TXinterval !=0) TXinterval = 10;	// minimum interval is 10 seconds
}

void readRSSI(Message *mess){
  mess->intVal = radio.RSSI;
}

void readVoltage(Message *mess){
  long result;					// Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2);					// Wait for Vref to settle
  ADCSRA |= _BV(ADSC);				// Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1126400L / result; 			// Back-calculate in mV
  mess->fltVal = float(result/1000.0);		// Voltage in Volt (float)
}

void readACK(Message *mess){
  setACK ? mess->intVal = 1 : mess->intVal = 0;
}

void writeACK(const Message *mess){
  mess->intVal ? setACK = true: setACK = false;
}

void readToggle(Message *mess){
  toggle ? mess->intVal = 1 : mess->intVal = 0;
}

void writeToggle(const Message *mess){
  mess->intVal ? toggle = true: toggle = false;
}

void readRelay(Message *mess){
  digitalRead(RELAY) ? mess->intVal = 1 : mess->intVal = 0;
}

void writeRelay(const Message *mess){
  digitalWrite(RELAY, mess->intVal);
}

void sleep(){
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_bod_disable();
  sleep_mode();
  sleep_disable();
}

void watchdogSetup(void){
  cli();
  wdt_reset();
  WDTCSR |=(1<<WDCE) | (1<<WDE);
  //set for 1s
  WDTCSR = (1 <<WDIE) |(1<<WDP2) | (1<<WDP1);
  sei();
}

void buttonHandler(){
  if (lastBtnPress != wdtCounter){
    lastBtnPress = wdtCounter;
    btnPressed = true;
  }
}

ISR(WDT_vect) // Watchdog timer interrupt.
{
  wdtCounter++;
}
