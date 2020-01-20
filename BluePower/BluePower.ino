#include <ArduinoBLE.h>
#define BATTERYLEVEL 72
BLEService powerService("1818");
BLECharacteristic powerLevelChar("2A63", BLERead | BLENotify, 4, false);
BLECharacteristic cadenceChar("2A5B", BLERead | BLENotify, 5, false);
char printBuffer[1024];

void setup() {
  Serial.begin(9600);
  while (!Serial);

  pinMode(LED_BUILTIN, OUTPUT);
  if (!BLE.begin())
  {
    Serial.println("starting BLE failed!");
    while (1);
  }

  BLE.setDeviceName("BluePower");
  BLE.setLocalName("BluePower");
  BLE.setAdvertisedService(powerService);
  powerService.addCharacteristic(powerLevelChar);
  powerService.addCharacteristic(cadenceChar);
  BLE.addService(powerService);
  BLE.advertise();
  Serial.println("Bluetooth device active, waiting for connections...");
}

void loop()
{
  BLEDevice central = BLE.central();
  char powerData[4];
  char cadenceData[5];
  unsigned long lastEvent=millis();
  unsigned long thisEvent;
  uint16_t lastTime=0;
  uint16_t crank=0;
  uint16_t power=0;
  
  if (central)
  {
    Serial.print("Connected to central: ");
    Serial.println(central.address());
    digitalWrite(LED_BUILTIN, HIGH);

    while (central.connected()) {
      power = 100 + rand() % 25 ; // ???? I wonder what that equates to...
      thisEvent=millis();

      if (thisEvent - lastEvent > 500 )
      {
        crank++;
        lastEvent=thisEvent;
        lastTime = thisEvent & 0xffff;
      }
      
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


      sprintf(printBuffer,"Set power to %d at time %d last time %d crank %d\n", power, millis(),lastTime,crank);
      Serial.print(printBuffer);

      
      delay(200);

    }
  }
  digitalWrite(LED_BUILTIN, LOW);
  //Serial.print("Disconnected from central: ");
  //Serial.println(central.address());
}
