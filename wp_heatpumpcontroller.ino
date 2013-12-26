// See the README.md

#include <Arduino.h>
#include <SPI.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>      // For the hardware watchdog
#include <Ethernet.h>
#include <EEPROM.h>

// Timer.h consumes 0xAA SRAM for the 10 timers on 'timer'
#include <Timer.h>        // For the CKP cancel timer, and for the watchdog

// aJSON.h consumes 0x100 bytes of SRAM for 'global_buffer'
#include <aJSON.h> // from https://github.com/interactive-matter/aJson

// Windows Phone notification
#include "WPNotification.h"
WPNotification *wpNotification = new WPNotification();

// All heatpumps
#include <FujitsuHeatpumpIR.h>
#include <PanasonicCKPHeatpumpIR.h>
#include <PanasonicHeatpumpIR.h>
#include <CarrierHeatpumpIR.h>
#include <MideaHeatpumpIR.h>
#include <MitsubishiHeatpumpIR.h>

PanasonicCKPHeatpumpIR *panasonicCKP = new PanasonicCKPHeatpumpIR(); // Need to use the timer cancel method
HeatpumpIR *panasonicDKE = new PanasonicDKEHeatpumpIR();
HeatpumpIR *panasonicJKE = new PanasonicJKEHeatpumpIR();
HeatpumpIR *panasonicNKE = new PanasonicNKEHeatpumpIR();
HeatpumpIR *fujitsu = new FujitsuHeatpumpIR();
HeatpumpIR *carrier = new CarrierHeatpumpIR();
HeatpumpIR *midea = new MideaHeatpumpIR();
HeatpumpIR *mitsubishi = new MitsubishiHeatpumpIR();

// This sketch uses DHCP. and a random-generated MAC address
byte macAddress[6] = { 0x02, 0x26, 0x89, 0x00, 0x00, 0x00 };
char macstr[18] = "02:26:89:00:00:00";

// The UDP listen socket
EthernetUDP WPudp;

// Stream for the aJson class
aJsonStream WPudp_stream(&WPudp);

// Timer for Panasonic CKP timer cancel event, and for the watchdog
Timer timer;
byte panasonicCancelTimer = 0;

// The UDP port the server listens to, randomly chosen :)
const int WPudpPort = 49722;

// Infrared LED on digital PIN 3 (needs a PWM pin)
// Connect with 100 Ohm resistor in series to GND
#define IR_LED_PIN        3

IRSender irSender(IR_LED_PIN);

// Ethernet shield reset pin
#define ETHERNET_RST      A0

// Entropy pin needs to be unconnected
#define ENTROPY_PIN       A5

// JSON data about supported pumps
// * model
// * displayName
// * numberOfModes
// * minTemperature
// * maxTemperature
// * numberOfFanSpeeds
// * maintenance modes
// Tight on flash space, needed to shink down the JSON key names...

static const prog_char heatpumpModelData[] PROGMEM = {"\"heatpumpmodels\":["
"{\"mdl\":\"panasonic_ckp\",\"dn\":\"Panasonic CKP\",\"mds\":5,\"mT\":16,\"xT\":30,\"fs\":6},"
"{\"mdl\":\"panasonic_dke\",\"dn\":\"Panasonic DKE\",\"mds\":5,\"mT\":16,\"xT\":30,\"fs\":6},"
"{\"mdl\":\"panasonic_jke\",\"dn\":\"Panasonic JKE\",\"mds\":5,\"mT\":16,\"xT\":30,\"fs\":6},"
"{\"mdl\":\"panasonic_nke\",\"dn\":\"Panasonic NKE\",\"mds\":6,\"mT\":16,\"xT\":30,\"fs\":6,\"maintenance\":[8,10]},"
"{\"mdl\":\"carrier\",\"dn\":\"Carrier\",\"mds\":5,\"mT\":17,\"xT\":30,\"fs\":6},"
"{\"mdl\":\"midea\",\"dn\":\"Ultimate Pro Plus 13FP\",\"mds\":6,\"mT\":16,\"xT\":30,\"fs\":4,\"maintenance\":[10]},"
"{\"mdl\":\"fujitsu_awyz\",\"dn\":\"Fujitsu AWYZ\",\"mds\":5,\"mT\":16,\"xT\":30,\"fs\":5},"
"{\"mdl\":\"mitsubishi_fd\",\"dn\":\"Mitsubishi FD\",\"mds\":5,\"mT\":16,\"xT\":31,\"fs\":4}"
"]"};


// The setup

void setup()
{
  // Initialize serial
  Serial.begin(9600);
  delay(100);
  Serial.println("Starting... ");
 
  // Ethernet shield reset trick
  // Need to cut the RESET lines (also from ICSP header) and connect an I/O to RESET on the shield

  pinMode(ETHERNET_RST, OUTPUT);
  digitalWrite(ETHERNET_RST, HIGH);
  delay(50);
  digitalWrite(ETHERNET_RST, LOW);
  delay(50);
  digitalWrite(ETHERNET_RST, HIGH);
  delay(100);

  // generate or read the already generated MAC address
  generateMAC();

  Serial.println(F("Obtaining IP address from DHCP server..."));

  // initialize the Ethernet adapter with DHCP
  if(Ethernet.begin(macAddress) == 0) {
    Serial.println(F("Failed to configure Ethernet using DHCP"));

    wdt_enable(WDTO_8S);

    while(true)   // no point in carrying on, so stay in endless loop until watchdog reset
      ;
  }

  delay(1000); // give the Ethernet shield a second to initialize

  Serial.print(F("IP address from DHCP server: "));
  Serial.println(Ethernet.localIP());

  // Initialize the Windows Phone app UDP port
  WPudp.begin(WPudpPort);


  // Initialize the hardware watchdog
  // Watchdog requires ADABOOT to work
  // See:
  // * http://www.ladyada.net/library/arduino/bootloader.html
  // * http://www.grozeaion.com/electronics/arduino/131-atmega328p-burning-bootloader.html
  wdt_enable(WDTO_8S);
  timer.every(2000, feedWatchdog);

  Serial.println(F("\nStarted\n"));
}


// The loop
// * heartbeats
// * xPL message processing

void loop()
{
  aJsonObject* jsonObject;

  // Sensible defaults
  int temperature = 23;
  int operatingMode = 2;
  int fanSpeed = 1;

  // The JSON response
  char response[54];
  const char *responseFmt = PSTR("{\"command\":\"%s\",\"identity\":\"%s\"");

  // Process the timers
  timer.update();

  // process incoming UDP packets
  int WPPacketSize = WPudp.parsePacket();
  if (WPPacketSize)
  {
    Serial.print(F("Received UDP packet of size "));
    Serial.println(WPPacketSize);
    Serial.print(F("From "));
    IPAddress remote = WPudp.remoteIP();
    for (int i =0; i < 4; i++)
    {
      Serial.print(remote[i], DEC);
      if (i < 3)
      {
        Serial.print(".");
      }
    }
    Serial.print(F(", port "));
    Serial.println(WPudp.remotePort());

    // Parse JSON
    Serial.println(F("Parsing JSON"));
    jsonObject = aJson.parse(&WPudp_stream);

    // Get the command and push channel
    aJsonObject* command = aJson.getObjectItem(jsonObject, "command");
    Serial.print(F("Command: "));
    Serial.println(command->valuestring);

    aJsonObject* pushurl_channel = aJson.getObjectItem(jsonObject, "channel");

    if (pushurl_channel != NULL)
    {
      Serial.print(F("Pushurl channel: "));
      Serial.println(pushurl_channel->valuestring);
    }

    if (strcmp_P(command->valuestring, PSTR("identify")) == 0)
    {
      // Create the response JSON just by printing it - this consumes less memory than using the aJson
      snprintf_P(response, sizeof(response), responseFmt, command->valuestring, macstr);
      wpNotification->sendNotification(WPudp, pushurl_channel, response, heatpumpModelData, 3);
    }
    else if (strcmp_P(command->valuestring, PSTR("command")) == 0)
    {
      // Is the message intended for us?
      aJsonObject* identity = aJson.getObjectItem(jsonObject, "identity");

      if (strcmp(identity->valuestring, macstr) == 0)
      {
        aJsonObject* model = aJson.getObjectItem(jsonObject, "model");
        aJsonObject* power = aJson.getObjectItem(jsonObject, "power");
        aJsonObject* mode = aJson.getObjectItem(jsonObject, "mode");
        aJsonObject* fan = aJson.getObjectItem(jsonObject, "fan");
        aJsonObject* temperature = aJson.getObjectItem(jsonObject, "temperature");

        // Create the response JSON just by printing it - this consumes less memory than using the aJson
        snprintf_P(response, sizeof(response), responseFmt, command->valuestring, macstr);
        wpNotification->sendNotification(WPudp, pushurl_channel, response, NULL, 3);

        if (strcmp_P(model->valuestring, PSTR("panasonic_ckp")) == 0)
        {
          panasonicCKP->send(irSender, power->valueint, mode->valueint, fan->valueint, temperature->valueint, 2, 1);

          // Send the 'timer cancel' signal 2 minutes later
          if (panasonicCancelTimer != 0)
          {
            timer.stop(panasonicCancelTimer);
            panasonicCancelTimer = 0;
          }
          
          // Note that the argument to 'timer.after' has to be explicitly cast into 'long'
          panasonicCancelTimer = timer.after(2L*60L*1000L, sendPanasonicCKPCancelTimer);
        }
        else if (strcmp_P(model->valuestring, PSTR("panasonic_dke")) == 0)
        {
          panasonicDKE->send(irSender, power->valueint, mode->valueint, fan->valueint, temperature->valueint, 2, 1);
        }
        else if (strcmp_P(model->valuestring, PSTR("panasonic_jke")) == 0)
        {
          panasonicJKE->send(irSender, power->valueint, mode->valueint, fan->valueint, temperature->valueint, 2, 1);
        }
        else if (strcmp_P(model->valuestring, PSTR("panasonic_nke")) == 0)
        {
          panasonicNKE->send(irSender, power->valueint, mode->valueint, fan->valueint, temperature->valueint, 2, 1);
        }
        else if (strcmp_P(model->valuestring, PSTR("midea")) == 0)
        {
          midea->send(irSender, power->valueint, mode->valueint, fan->valueint, temperature->valueint, 0, 0);
        }
        else if (strcmp_P(model->valuestring, PSTR("carrier")) == 0)
        {
          carrier->send(irSender, power->valueint, mode->valueint, fan->valueint, temperature->valueint, 0, 0);
        }
        else if (strcmp_P(model->valuestring, PSTR("fujitsu_awyz")) == 0)
        {
          fujitsu->send(irSender, power->valueint, mode->valueint, fan->valueint, temperature->valueint, 0, 0);
        }
        else if (strcmp_P(model->valuestring, PSTR("mitsubishi_fd")) == 0)
        {
          mitsubishi->send(irSender, power->valueint, mode->valueint, fan->valueint, temperature->valueint, 0, 0);
        }     
      }
    }

    // Free the memory used by the JSON objects
    aJson.deleteItem(jsonObject);
  }
}

// Cancel the Panasonic CKP timer
void sendPanasonicCKPCancelTimer()
{
  panasonicCKP->sendPanasonicCKPCancelTimer(irSender);
}

// Random MAC based on:
// http://files.pcode.nl/arduino/EthernetPersistentMAC.ino
// A5 is the entropy PIN for random MAC generation, leave it unconnected

void generateMAC()
{
  randomSeed(analogRead(ENTROPY_PIN));

  // Uuncomment to generate a new MAC
  //EEPROM.write(E2END - 8, 0x00);
  //EEPROM.write(E2END - 7, 0x00);

  // We store the MAC address in the last 8 bytes of the EEPROM using E2END to determine it's size
  // The first of those two bytes are checked for the magic values 0x80 and 0x23 (a reference to 802.3)

  if ((EEPROM.read(E2END - 8) == 0x80) && (EEPROM.read(E2END - 7) == 0x23))
  {
    Serial.println(F("Reading MAC address from EEPROM"));
    for (int i = 0; i <= 5; i++)
    {
       macAddress[i] = EEPROM.read(E2END - 6 + i);
    }
  }
  else
  {
    Serial.println(F("Writing new random MAC address to EEPROM"));

    EEPROM.write(E2END - 8, 0x80);
    EEPROM.write(E2END - 7, 0x23);
    for (int i = 0; i <= 5; i++)
    {
      // Skip the Organisationally Unique Identifier (OUI)
      // Randomize only the Network Interface Controller specific part
      if (i >= 3)
      {
        macAddress[i] = random(0, 255);
      }
      EEPROM.write(E2END - 6 + i, macAddress[i]);
    }
  }
  snprintf_P(macstr, 18, PSTR("%02X:%02X:%02X:%02X:%02X:%02X"), macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);

  // Print out the MAC address
  Serial.print(F("MAC: "));
  Serial.println(macstr);
}

//
// The most important thing of all, feed the watchdog
//
void feedWatchdog()
{
  wdt_reset();
}
