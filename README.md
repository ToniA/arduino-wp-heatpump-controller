arduino-wp-heatpump-controller
==============================

Control a Panasonic or Midea heat pump/split unit air conditioner with Arduino using a Windows Phone 8 application
Currently supports at least these models 
* Panasonic E9/E12-CKP and E9/E12-DKE (Panasonic remote control P/N A75C2295 and P/N A75C2616)
* Midea MSR1-12HRN1-QC2 + MOA1-12HN1-QC2, sold as Ultimate Pro Plus Basic 13FP in Finland (Midea remote control P/N RG51M1/E)

For the Windows Phone 8 application source, see my other repository
https://github.com/ToniA/wp8-heatpumpcontrol

Instructions
============
* Compile the software, and program your Arduino
    * Note that the default IP address is 192.168.0.12 (DHCP is not used)
	* Adjust that, as well as the broadcast address as needed
	* Note that for watchdog to work, you need Adaboot (see the source for links)
* Wire up your Arduino as instructed in the schema
    * Ethernet connection to a switch
	* You can power the device for example with a USB cellphone charger (like Nokia AC-16E)
* Place the IR led so that the IR receiver on the indoor unit can see it
* Use the Windows Phone app to search for heatpump controllers :)

Schema
------

Bill of materials
* Arduino Duemilanove :)
* Arduino Ethernet shield
* IR led
* 1 kOhm resistor for the IR led
		
Connect an IR led (with 1k resistor in series) between GND and digital pin 3

![Schema](https://raw.github.com/ToniA/arduino-wp-heatpump-controller/master/arduino_irsender_bb.png)
