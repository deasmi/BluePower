#include <ArduinoBLE.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "BluePower.h"

#define BATTERYLEVEL 72
const int pedalSensorPin = 2;     // the number of input for the cadence sensor
const int upButton = 3;
const int downButton = 10;
const float forceLevels[] = {1000,3000,800,900,14000,16000,12000,20000};
const float crankLength = 0.175f;
const long bounceDelay = 250;
const char welcomeMessage[] = {'B','l','u','e','P','o','w','e','r',0};
const int dcPIN=A0;
const int csPIN=A1;
const int rstPIN=A2;

// These are global and used in interupt handlers, so must be volatile, ie. in RAM always
volatile uint16_t crank=0;
volatile unsigned long lastEvent; // time in ms since boot of most recent revolution
volatile unsigned long deltaTime; // tims in ms between previous rotation and lastEvent
volatile bool ledState;
volatile int powerSetting;
volatile long currentPower;
volatile long currentRPM;

BLEService powerService("1818");
BLECharacteristic powerLevelChar("2A63", BLERead | BLENotify, 4, false);
BLECharacteristic cadenceChar("2A5B", BLERead | BLENotify, 5, false);

Adafruit_PCD8544 display = Adafruit_PCD8544(dcPIN, csPIN, rstPIN);

// https://en.wikibooks.org/wiki/C_Programming/stdarg.h
void log(const char *format,...) {
  char printBuffer[1024];
  va_list args;
  
  va_start(args,format);
  vsprintf(printBuffer, format, args); 
  Serial.println(printBuffer);
  va_end(args); 
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


/***********************************
 * Display functions               *
 ***********************************/
void initDisplay() {
	// Setup the screen
	display.begin();
	display.setContrast(50);
	display.setRotation(2);
  display.clearDisplay();   // clears the screen and buffer

	display.setTextSize(1);
	display.setTextColor(BLACK);
	display.setCursor(2,2);

	for (int i=0; i<strlen(welcomeMessage); i++)
		{
			display.setCursor(i*8,0);
			display.write(welcomeMessage[i]);
		}
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

	log("Current power %d",currentPower);
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
	
	display.display();	
}
/***********************************
 * Setup and loop below here       *
 ***********************************/

void setup() {
  Serial.begin(9600);
  //while (!Serial);

	initDisplay(); // Setup and show logo
	delay(1000); // Wait 1s before main display

	if (!BLE.begin())
  {
    log("starting BLE failed!");
    while (1);
  }

  lastEvent=millis(); // Initialise lastEvent with sane value

  // Set pins to input mode
  pinMode(pedalSensorPin,INPUT); 
  pinMode(upButton,INPUT);
  pinMode(downButton,INPUT);

  // Attach interupts
  attachInterrupt(digitalPinToInterrupt(pedalSensorPin), pedalIH, CHANGE);
  attachInterrupt(digitalPinToInterrupt(downButton), upButtonIH, RISING);
  attachInterrupt(digitalPinToInterrupt(upButton), downButtonIH, RISING);

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
  unsigned long thisEvent;
  uint16_t power=0;
  int resistor;
  int old_crank;
  float rpm;
  float torque;
 

	// We want to run in central BLE mode
  central = BLE.central();

	// Do out maths
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
	
	// If we have a connection
	if (central)
  {
    log("Connected to central: %s", central.address());
    while (central.connected()) {
			
      // Flatten the power into the structure
      powerData[0]=0; // NO FLAGS
      powerData[1]=0; // NO FLAGS
      powerData[2]=power % 256; // LSB
      powerData[3]=power / 256; // MSB
      powerLevelChar.writeValue(powerData,4);
 
			// Flatter the crank time to structure
      cadenceData[0] = 0b10; // Crank only
      cadenceData[1] = crank & 0xff;
      cadenceData[2] = (crank >> 8) & 0xff;
      cadenceData[3] = (lastEvent & 0xffff) % 0xff;
      cadenceData[4] = (lastEvent & 0xffff) / 0xff;
      cadenceChar.writeValue(cadenceData,5);
		}
	}
  
	log("rpm %f torque %f power %d lastTime %d crank %d", rpm, torque, power, lastEvent, crank);      
	// Put up initial view of the world
  updateDisplay();
	delay(250);
}
