arduino-wp-heatpump-controller
==============================

Control a Panasonic, Midea, Carrier, Fujitsu or MItsubishi heat pump/split unit air conditioner with Arduino using a Windows Phone 8 application
Currently supports at least these models 
* Panasonic E9/E12-CKP (Panasonic remote control P/N A75C2295)
* Panasonic E9/E12-DKE (Panasonic remote control P/N A75C2616)
* Panasonic E9/E12-JKE and E9/E12-NKE
* Midea MSR1-12HRN1-QC2 + MOA1-12HN1-QC2, sold as Ultimate Pro Plus Basic 13FP in Finland (Midea remote control P/N RG51M1/E)
* Carrier 42NQV035G / 38NYV035H2 (Carrier remote control P/N WH-L05SE)
* Fujitsu Nocria AWYZ14 (remote control P/N AR-PZ2)
* Mitsubishi MSZ FD-25 (probably also FD-35)

For the Windows Phone 8 application source, see my other repository
https://github.com/ToniA/wp8-heatpumpcontrol

Instructions
------------
* Compile the software, and program your Arduino
    * Note that the default IP address is 192.168.0.12 (DHCP is not used)
	* Adjust that, as well as the broadcast address as needed
	* Note that for watchdog to work, you need Adaboot (see the source for links)
* Wire up your Arduino as instructed in the schema
    * Ethernet connection to a switch
	* You can power the device for example with a USB cellphone charger (like Nokia AC-16E)
* Place the IR led so that the IR receiver on the indoor unit can see it
* Use the Windows Phone app to search for heatpump controllers :)

Usage without the Windows Phone application
-------------------------------------------

You can also use this directly by using UDP messages. The software will send a UDP reply to the sender's IP address and port if the 'channel' is not defined.

Examples:

```
echo '{"command":"identify"}' | socat -v - UDP4:192.168.0.255:49722,broadcast

echo '{"command":"command","fan":4,"identity":"02:26:89:28:25:C5","mode":2,"model":"panasonic_ckp","power":1,"temperature":24}' | socat -v - UDP4:192.168.0.255:49722,broadcast
```

And here's a piece of Python code:

```
from socket import *
sock = socket(AF_INET, SOCK_DGRAM)
sock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
sock.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)

# Ask for the identity
sock.sendto('{"command":"identify"}', ('255.255.255.255', 49722))
data, addr = sock.recvfrom(2048) # buffer size is 1024 bytes
print "message from %s: %s" % (addr, data)

# Send a command to a controller
sock.sendto('{"command":"command","fan":4,"identity":"02:26:89:28:25:C5","mode":2,"model":"panasonic_ckp","power":1,"temperature":24}', ('255.255.255.255', 49722))
data, addr = sock.recvfrom(2048) # buffer size is 1024 bytes
print "message from %s: %s" % (addr, data)
```

The output is

```
message from ('192.168.0.12', 49722): {"command":"identify","identity":"02:26:89:28:25:C5"}
message from ('192.168.0.12', 49722): {"command":"command","fan":4,"identity":"02:26:89:28:25:C5","mode":2,"model":"panasonic_ckp","power":1,"temperature":24}
```
   
Schema
------

Bill of materials
* Arduino :)
    * I'm using the Duemilanove, other models might require changes due to differences on the PWM pins
    * Don't try this with Arduino's with ATmega168, 1k of SRAM just isn't enough
* Arduino Ethernet shield
* IR led
* 1 kOhm resistor for the IR led
		
Connect an IR led (with 1k resistor in series) between GND and digital pin 3

![Schema](https://raw.github.com/ToniA/arduino-wp-heatpump-controller/master/arduino_irsender_bb.png)
