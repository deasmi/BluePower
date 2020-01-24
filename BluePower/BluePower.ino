#include <ArduinoBLE.h>
#include <stdarg.h>
#define BATTERYLEVEL 72
const int pedalSensorPin = 2;     // the number of input for the cadence sensor
const int switchPin = 3;     // the number of input for the power switch
volatile int powerSetting;
const float forceLevels[] = {1000,3000,800,900,14000,16000,12000,20000};
const float crankLength = 0.175f;
volatile uint16_t crank=0;
volatile unsigned long lastEvent; // time in ms since boot of most recent revolution
volatile unsigned long deltaTime; // tims in ms between previous rotation and lastEvent

BLEService powerService("1818");
BLECharacteristic powerLevelChar("2A63", BLERead | BLENotify, 4, false);
BLECharacteristic cadenceChar("2A5B", BLERead | BLENotify, 5, false);

void setup() {
  Serial.begin(9600);
  while (!Serial);

  log("Logging %s","test");

  pinMode(LED_BUILTIN, OUTPUT);
  if (!BLE.begin())
  {
    log("starting BLE failed!");
    while (1);
  }

  lastEvent=millis(); // Initialise lastEvent with sane value

  // Set pins to input mode
  pinMode(pedalSensorPin,INPUT); 
  pinMode(switchPin,INPUT);

  // Attach interupts
  attachInterrupt(digitalPinToInterrupt(pedalSensorPin), isrPedal, CHANGE);
  attachInterrupt(digitalPinToInterrupt(switchPin), isrPowerSwitch, CHANGE);

  // Power setting
  if (digitalRead(pedalSensorPin)) {
    powerSetting = 8;
  } else {
    powerSetting = 7;
  }
  
  BLE.setDeviceName("BluePower");
  BLE.setLocalName("BluePower");
  BLE.setAdvertisedService(powerService);
  powerService.addCharacteristic(powerLevelChar);
  powerService.addCharacteristic(cadenceChar);
  BLE.addService(powerService);
  BLE.advertise();
  log("Bluetooth device active, waiting for connections...");
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
    } 

}

void isrPowerSwitch() {
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


// https://en.wikibooks.org/wiki/C_Programming/stdarg.h
void log(const char *format,...) {
  char printBuffer[1024];
  va_list args;
  
  va_start(args,format);
  vsprintf(printBuffer, format, args); 
  Serial.println(printBuffer);
  va_end(args); 
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
    digitalWrite(LED_BUILTIN, HIGH);

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
    delay(100);
    }
  }

}
