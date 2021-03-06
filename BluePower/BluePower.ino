#include <ArduinoBLE.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "BluePower.h"

#undef DEBUG
#define BATTERYLEVEL 72

const int pedalSensorPin = 3;     // the number of input for the cadence sensor
const int upButton = 9;
const int downButton = 2;
const float forceLevels[] = {500,1000,2000,4000,8000,10000,12000,20000};
const float crankLength = 0.175f;
const long bounceDelay = 250;
const char welcomeMessage[] = "BluePower\nStarting up..";
const int dcPIN=A5;
const int csPIN=A6;
const int rstPIN=A7;

const uint32_t updateRate=500; // How often to update power, rpm and screen
const uint32_t pairingDelay=5000; // How long to wait before pairing attempts

// These are global and used in interupt handlers, so must be volatile, ie. in RAM always
volatile uint16_t crank=0;
volatile unsigned long lastEvent; // time in ms since boot of most recent revolution
volatile unsigned long deltaTime; // tims in ms between previous rotation and lastEvent
volatile bool ledState;
volatile int powerSetting;
volatile long currentPower;
volatile long currentRPM;
volatile int statusDot;

BLEService powerService("1818");
BLECharacteristic powerLevelChar("2A63", BLERead | BLENotify, 4, false);
BLECharacteristic cadenceChar("2A5B", BLERead | BLENotify, 5, false);

Adafruit_PCD8544 display = Adafruit_PCD8544(dcPIN, csPIN, rstPIN);


/* Utility functions */
// https://en.wikibooks.org/wiki/C_Programming/stdarg.h
void log(const char *format,...) {
  char printBuffer[1024];
  va_list args;

	#ifdef DEBUG
  va_start(args,format);
  vsprintf(printBuffer, format, args); 
  Serial.println(printBuffer);
  va_end(args);
	#endif
}  

/***********************************
 * Input interupt handlers         *
 ***********************************/
void pedalIH() { 
    bool newState;
    bool pedalState;
    unsigned long thisEvent;

    thisEvent = millis();
    newState = digitalRead(pedalSensorPin);

    log("pedalIH-%lu",thisEvent);

    if ((thisEvent - lastEvent > 100) && newState != pedalState) { // 100ms min for a pedal rotation, seems fair
      crank++;
      pedalState=newState;
      deltaTime = thisEvent - lastEvent;
      lastEvent=thisEvent;
      ledState = !ledState;
      digitalWrite(LED_BUILTIN, ledState);
    } 

}

void upButtonIH() {
  static bool status;
  static long lastEvent;
  static bool state;
  static bool oldState;
  unsigned long now;

  state = digitalRead(upButton);
  now = millis();


  if ((now - lastEvent) > bounceDelay) {
    lastEvent = now;
    powerSetting++;
    if (powerSetting>8) { powerSetting=8; }  
    log("upBottonIH-%lu",now);
    }
}


void downButtonIH() {
  static bool status;
  static long lastEvent;
  static bool state;
  static bool oldState;
  unsigned long now;

  state = digitalRead(downButton);
  now = millis();

  if ((now - lastEvent) > bounceDelay) {
    lastEvent = now;
      powerSetting--;
      if (powerSetting<1) { powerSetting=1; }
      log("downBottonIH-%lu",now);
  }
}


void updateValues() {
  unsigned long thisEvent;
  int resistor;
  static int old_crank=0;
	uint16_t power;
  float rpm;
  float torque;
 
	// Do our maths
	if (old_crank != crank) { // There has been a revolution -
		old_crank=crank;
		
		// Calc rpm
		rpm = (60000 / deltaTime); // one minute in ms divided by time between last two events
		currentRPM=(long)rpm;
		
		// Calculate Power
		// Torque
		torque = (forceLevels[powerSetting-1] / 1000) * 9.81 * crankLength; 
		// Power
		power = (int) ( torque * PI * ( rpm/30 ));
		currentPower = power;
	}
	
	if (abs(millis() - lastEvent) > 5000) {
		currentPower=0;
		currentRPM=0;
	}
}

/***********************************
 * Display functions               *
 ***********************************/
void initDisplay() {
	// Setup the screen
	display.begin();
	display.setContrast(50);
	display.setRotation(0);
  display.clearDisplay();   // clears the screen and buffer

	display.setTextSize(1);
	display.setTextColor(BLACK);
	display.setCursor(2,2);
	display.print(welcomeMessage);

	statusDot=WHITE;
	display.display();
}

void updateDisplay()
{
	char buf[20]; // Tiny buffer for our strings
	int len;
	display.clearDisplay();

	// Draw frame
	display.drawRect(0,0,84,48,BLACK);
	
	// The main resistance display
	display.setTextSize(4);
	display.setTextColor(BLACK);
	display.setCursor(2,2);
	display.write(48+powerSetting); // ASCII 0 is 48

	display.setTextSize(1);
	display.setCursor(2,36);
	display.print("Tension");

	display.setCursor(52,2);
	display.print("Power");
	len = sprintf(buf,"%lu",currentPower);
	display.setCursor(82-(6*len),10);
	display.print(buf);

	
	display.setCursor(62,26);
	display.print("RPM");
	len=sprintf(buf,"%lu",currentRPM);
	display.setCursor(82-(6*len),34);
	display.print(buf);

	if (statusDot==WHITE) {
		statusDot=BLACK; } else {
		statusDot=WHITE; }
	display.fillRect(80,44,2,2,statusDot);
	
	display.display();	
}


void setupInterupts() {

	// Input interupts

	// Set pins to input mode
  pinMode(pedalSensorPin,INPUT); 
  pinMode(upButton,INPUT);
  pinMode(downButton,INPUT);

  // Attach interupts
  attachInterrupt(digitalPinToInterrupt(pedalSensorPin), pedalIH, CHANGE);
  attachInterrupt(digitalPinToInterrupt(upButton), upButtonIH, RISING);
  attachInterrupt(digitalPinToInterrupt(downButton), downButtonIH, RISING);


	// 0.25s timer for data and screen updates
	tcConfigure(updateRate);
	tcStartCounter();
}	
	
void TC5_Handler(void) {
	updateValues();
	updateDisplay();
	TC5->COUNT16.INTFLAG.bit.MC0 = 1; // Reset interupt
}
	
/* https://gist.github.com/nonsintetic/ad13e70f164801325f5f552f84306d6f */
/* 
 *  TIMER SPECIFIC FUNCTIONS FOLLOW
 *  you shouldn't change these unless you know what you're doing
 */

//Configures the TC to generate output events at the sample frequency.
//Configures the TC in Frequency Generation mode, with an event output once
//each time the audio sample frequency period expires.
 void tcConfigure(int sampleRate)
{
 // Enable GCLK for TCC2 and TC5 (timer counter input clock)
 GCLK->CLKCTRL.reg = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID(GCM_TC4_TC5)) ;
 while (GCLK->STATUS.bit.SYNCBUSY);

 tcReset(); //reset TC5

 // Set Timer counter Mode to 16 bits
 TC5->COUNT16.CTRLA.reg |= TC_CTRLA_MODE_COUNT16;
 // Set TC5 mode as match frequency
 TC5->COUNT16.CTRLA.reg |= TC_CTRLA_WAVEGEN_MFRQ;
 //set prescaler and enable TC5
 TC5->COUNT16.CTRLA.reg |= TC_CTRLA_PRESCALER_DIV1024 | TC_CTRLA_ENABLE; //you can use different prescaler divisons here like TC_CTRLA_PRESCALER_DIV1 to get different ranges of frequencies
 //set TC5 timer counter based off of the system clock and the user defined sample rate or waveform
 TC5->COUNT16.CC[0].reg = (uint16_t) (SystemCoreClock / sampleRate - 1);
 while (tcIsSyncing());
 
 // Configure interrupt request
 NVIC_DisableIRQ(TC5_IRQn);
 NVIC_ClearPendingIRQ(TC5_IRQn);
 NVIC_SetPriority(TC5_IRQn, 0);
 NVIC_EnableIRQ(TC5_IRQn);

 // Enable the TC5 interrupt request
 TC5->COUNT16.INTENSET.bit.MC0 = 1;
 while (tcIsSyncing()); //wait until TC5 is done syncing 
} 

//Function that is used to check if TC5 is done syncing
//returns true when it is done syncing
bool tcIsSyncing()
{
  return TC5->COUNT16.STATUS.reg & TC_STATUS_SYNCBUSY;
}

//This function enables TC5 and waits for it to be ready
void tcStartCounter()
{
  TC5->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE; //set the CTRLA register
  while (tcIsSyncing()); //wait until snyc'd
}

//Reset TC5 
void tcReset()
{
  TC5->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
  while (tcIsSyncing());
  while (TC5->COUNT16.CTRLA.bit.SWRST);
}

//disable TC5
void tcDisable()
{
  TC5->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE;
  while (tcIsSyncing());
}

/***********************************
 * Setup and loop below here       *
 ***********************************/

void setup() {
#ifdef DEBUG
	// No serial if we aren't debugging
	Serial.begin(9600);
#endif

	initDisplay(); // Setup and show logo
	delay(2000); // Wait 1s before main display

	if (!BLE.begin())
  {
    log("starting BLE failed!");
    while (1);
  }

  lastEvent=millis(); // Initialise lastEvent with sane value

	setupInterupts();
	
	// Setup key variables
  powerSetting=5; // Default to a mid setting
	currentPower=0; // Assume static
	currentRPM=0;

	// Setup Bluetooth
  BLE.setDeviceName("BluePower");
  BLE.setLocalName("BluePower");
  BLE.setAdvertisedService(powerService);
  powerService.addCharacteristic(powerLevelChar);
  powerService.addCharacteristic(cadenceChar);
  BLE.addService(powerService);
  BLE.advertise();
  log("Bluetooth device active, waiting for connections...");
}

void loop()
{  
  BLEDevice central;
  char powerData[4];
  char cadenceData[5];
	log("Main loop");
	// We want to run in central BLE mode
  central = BLE.central();

	// If we have a connection
	if (central)
  {
    log("Connected to central: %s", central.address().c_str());
    while (central.connected()) {
			
      // Flatten the power into the structure
      powerData[0]=0; // NO FLAGS
      powerData[1]=0; // NO FLAGS
      powerData[2]=currentPower % 256; // LSB
      powerData[3]=currentPower / 256; // MSB
      powerLevelChar.writeValue(powerData,4);
 
			// Flatter the crank time to structure
      cadenceData[0] = 0b10; // Crank only
      cadenceData[1] = crank & 0xff;
      cadenceData[2] = (crank >> 8) & 0xff;
      cadenceData[3] = (lastEvent & 0xffff) % 0xff;
      cadenceData[4] = (lastEvent & 0xffff) / 0xff;
      cadenceChar.writeValue(cadenceData,5);
			delay(250); // Inner delay if we are sending to a BLE device
		}
	} else {
		log("Disconnected");
	}
 	delay(pairingDelay); // Outer delay between trying to pair with BLE device
}
