# BluePower
Code for my Arduino based exercise bike power meter

This is used with an Exerputic folding bike.

This has a rational signal that was easy to hook into, generating a pulse every rotation.

Measurement of force on the pedals was complicated as it uses magnetic breaking/eddie currents.
There is an array with force for different settings, measured with a Heath Robinson method 
using a handheld luggage scale. ( Really )

Works well with Zwift and have tried a few others.

Needs two buttons + Nokia 5110 screen, pins are labelled in the code.
I use 1k pull down resistors on the buttons and rotation sensor.

This is written for a Arduino IOT 33 - SIMD21 based board.
It will _NOT_ run correctly on other boards due to timer interupts, 
this could be fixed but I don't have a BLE sheild to use to test.


	
	
	
	
	
