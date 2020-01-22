const int buttonPin = 2;     // the number of the pushbutton pin
const int ledPin =  3;      // the number of the LED pin
char msg[256];

// variables will change:
volatile int buttonState = 0;         // variable for reading the pushbutton status

void setup() {

  Serial.begin(9600);
    while (!Serial);  
  
  // initialize the LED pin as an output:
  pinMode(ledPin, OUTPUT);
  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);
  // Attach an interrupt to the ISR vector
  attachInterrupt(digitalPinToInterrupt(2), pin_ISR, CHANGE);
  Serial.println("Setup");
}

void loop() {
  // Nothing here!
}

void pin_ISR() {
  sprintf(msg,"INT @ %lu", millis());
  Serial.println(msg);
  buttonState = digitalRead(buttonPin);
  if (buttonState) {
    Serial.println("Closed");
  } else {
    Serial.println("Open");
  }
}
