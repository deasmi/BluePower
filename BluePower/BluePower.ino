#include <ArduinoBLE.h>
#define BATTERYLEVEL 72
const int buttonPin = 2;     // the number of the pushbutton pin
const int force_levels[] = {600,700,800,900,1100,1300,1500,1900};
const long crank_length = 0.16;
uint16_t crank=0;
  

BLEService powerService("1818");
BLECharacteristic powerLevelChar("2A63", BLERead | BLENotify, 4, false);
BLECharacteristic cadenceChar("2A5B", BLERead | BLENotify, 5, false);
char printBuffer[1024];
unsigned long lastEvent;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  pinMode(LED_BUILTIN, OUTPUT);
  if (!BLE.begin())
  {
    Serial.println("starting BLE failed!");
    while (1);
  }

  lastEvent=millis();

 attachInterrupt(digitalPinToInterrupt(2), pin_ISR, CHANGE);


  BLE.setDeviceName("BluePower");
  BLE.setLocalName("BluePower");
  BLE.setAdvertisedService(powerService);
  powerService.addCharacteristic(powerLevelChar);
  powerService.addCharacteristic(cadenceChar);
  BLE.addService(powerService);
  BLE.advertise();
  Serial.println("Bluetooth device active, waiting for connections...");
}



void pin_ISR() { 
    bool buttonState;

  buttonState = digitalRead(buttonPin);
  if (buttonState) {
    Serial.println("Revolution");
    crank++;
  }

} 

void loop()
{  
  BLEDevice central;
  char powerData[4];
  char cadenceData[5];
  unsigned long thisEvent;
  uint16_t lastTime=0;
  uint16_t power=0;
  int resistor;
  int old_crank;
  
  central = BLE.central();
  if (central)
  {
    Serial.print("Connected to central: ");
    Serial.println(central.address());
    digitalWrite(LED_BUILTIN, HIGH);

    while (central.connected()) {
      power = 100 + rand() % 25 ; // ???? I wonder what that equates to...

      if (old_crank != crank) { // There has been a revolution -
      old_crank=crank;
      
      thisEvent=millis();
      lastEvent=thisEvent;
      lastTime = thisEvent & 0xffff;
      
      powerData[0]=0; // NO FLAGS
      powerData[1]=0; // NO FLAGS
      powerData[2]=power % 256; // LSB
      powerData[3]=power / 256; // MSB
      powerLevelChar.writeValue(powerData,4);
 
      cadenceData[0] = 0b10; // Crank only
      cadenceData[1] = crank & 0xff;
      cadenceData[2] = (crank >> 8) & 0xff;
      cadenceData[3] = lastTime % 256;
      cadenceData[4] = lastTime / 256;
      cadenceChar.writeValue(cadenceData,5);
      sprintf(printBuffer,"Set power to %d at time %d last time %d crank %d resistor %d\n", power, millis(),lastTime,crank,resistor);
      Serial.print(printBuffer);
      } else {
        // No changes
      }
    delay(100);
    }
  }

}
