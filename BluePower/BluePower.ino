#include <ArduinoBLE.h>
#include <stdarg.h>
#include "BluePower.h"
#define BATTERYLEVEL 72
const int pedalSensorPin = 2;     // the number of input for the cadence sensor
const int upButton = 3;
const int downButton = 10;
const float forceLevels[] = {1000,3000,800,900,14000,16000,12000,20000};
const float crankLength = 0.175f;
const long bounceDelay = 250;


// These are global and used in interupt handlers, so must be volatile, ie. in RAM always
volatile uint16_t crank=0;
volatile unsigned long lastEvent; // time in ms since boot of most recent revolution
volatile unsigned long deltaTime; // tims in ms between previous rotation and lastEvent
volatile bool ledState;
volatile int powerSetting;

BLEService powerService("1818");
BLECharacteristic powerLevelChar("2A63", BLERead | BLENotify, 4, false);
BLECharacteristic cadenceChar("2A5B", BLERead | BLENotify, 5, false);



// https://en.wikibooks.org/wiki/C_Programming/stdarg.h
void log(const char *format,...) {
  char printBuffer[1024];
  va_list args;
  
  va_start(args,format);
  vsprintf(printBuffer, format, args); 
  Serial.println(printBuffer);
  va_end(args); 
}  







void isrPedal() { 
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

/*void isrPowerSwitch() {
    bool buttonState;

    // Let it settle, only viable for low change
    delay(25); 
    
    buttonState = digitalRead(switchPin);
  if (
  buttonState ) {
      log("Power 8");
      powerSetting=8;
    } else { 
      log("Power 7");
      powerSetting=7;
    }
  }
*/

/* How to handle the Buttons 
 * Need to handle bounce
 * And a delay between presses
 */
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

void setup() {
  Serial.begin(9600);
  //while (!Serial);

  log("Logging %s","test");

  // Flash twice to indicate we are running
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  ledState=false;
  
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
  attachInterrupt(digitalPinToInterrupt(pedalSensorPin), isrPedal, CHANGE);
//  attachInterrupt(digitalPinToInterrupt(switchPin), isrPowerSwitch, CHANGE);

  attachInterrupt(digitalPinToInterrupt(downButton), upButtonIH, RISING);
  attachInterrupt(digitalPinToInterrupt(upButton), downButtonIH, RISING);


  powerSetting=5; // Default to a mid setting

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
 
  
  central = BLE.central();
  if (central)
  {
    log("Connected to central: %s", central.address());

    while (central.connected()) {
      //power = 100 + rand() % 25 ; // ???? I wonder what that equates to...

      if (old_crank != crank) { // There has been a revolution -
      old_crank=crank;
      
      
      // Calc rpm
      rpm = (60000 / deltaTime); // one minute in ms divided by time between last two events
      // Calculate Power
      // Torque
      torque = (forceLevels[powerSetting-1] / 1000) * 9.81 * crankLength; 
      // Power
      power = (int) ( torque * PI * ( rpm/30 ));
      
      powerData[0]=0; // NO FLAGS
      powerData[1]=0; // NO FLAGS
      powerData[2]=power % 256; // LSB
      powerData[3]=power / 256; // MSB
      powerLevelChar.writeValue(powerData,4);
 
      cadenceData[0] = 0b10; // Crank only
      cadenceData[1] = crank & 0xff;
      cadenceData[2] = (crank >> 8) & 0xff;
      cadenceData[3] = (lastEvent & 0xffff) % 0xff;
      cadenceData[4] = (lastEvent & 0xffff) / 0xff;
      cadenceChar.writeValue(cadenceData,5);
      log("rpm %f torque %f power %d lastTime %d crank %d", rpm, torque, power, lastEvent, crank);      
      } else {
        // No changes
      }
    //log("upButton %d downButton %d at %lu",digitalRead(upButton),digitalRead(downButton),millis());
    delay(100);
    }
  }

}
