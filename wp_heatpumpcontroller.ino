// See the README.md

#include <Arduino.h>
#include <SPI.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>      // For the hardware watchdog
#include <Ethernet.h>
#include <EEPROM.h>

// The default Timer class consumes 0xAA bytes of SRAM for the 10 timers on 'timer'
#include <Timer.h>        // For the Panasonic CKP cancel timer, and for the watchdog

// aJSON.h consumes 0x100 bytes of SRAM for 'global_buffer'
#include <aJSON.h>        // from https://github.com/interactive-matter/aJson

// UDP & Windows Phone notification
#include "Notification.h"
Notification *notification = new Notification();

// DHCP or static IP address. DHCP requires almost 3kbytes more flash
#define DHCP 0                     // 0 for static IP address, 1 for DHCP
#define STATIC_IP 192, 168, 0, 12  // The static IP address when not using DHCP

// All heatpumps
#include <FujitsuHeatpumpIR.h>
#include <PanasonicCKPHeatpumpIR.h>
#include <PanasonicHeatpumpIR.h>
#include <CarrierHeatpumpIR.h>
#include <MideaHeatpumpIR.h>
#include <MitsubishiHeatpumpIR.h>

// Array with all supported heatpumps, comment out the ones which are not needed (to save FLASH and SRAM on Duemilanove)
HeatpumpIR *heatpumpIR[] = {
                             new PanasonicCKPHeatpumpIR(),
                             new PanasonicDKEHeatpumpIR(),
                             new PanasonicJKEHeatpumpIR(),
                             new PanasonicNKEHeatpumpIR(),
                             new CarrierHeatpumpIR(),
                             new MideaHeatpumpIR(),
                             new MideaHeatpumpIR(),
                             new FujitsuHeatpumpIR(),
                             new MitsubishiFDHeatpumpIR(),
                             new MitsubishiFEHeatpumpIR(),
                             NULL // The array must be NULL-terminated
                           };

// For the Panasonic CKP timer cancel
PanasonicCKPHeatpumpIR *panasonicCKP;

// Infrared LED on digital PIN 9 (needs a PWM pin)
// Connect with 100 Ohm resistor in series to GND
#define IR_LED_PIN        9
IRSender irSender(IR_LED_PIN);

#if DHCP == 0
IPAddress ip(STATIC_IP);  // This MAC/IP address pair should also be set as a static lease on the router, or set to be outside the DHCP address pool
#endif

// The MAC address is generated randomly and stored in the EEPROM
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

// Ethernet shield reset pin
#define ETHERNET_RST      8

// Entropy pin needs to be unconnected
#define ENTROPY_PIN       A5


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

#if DHCP == 0
  // initialize the Ethernet adapter with a static IP address
  Ethernet.begin(macAddress, ip);
#else
  Serial.println(F("Obtaining IP address from DHCP server..."));

  // initialize the Ethernet adapter with DHCP
  if(Ethernet.begin(macAddress) == 0) {
    Serial.println(F("Failed to configure Ethernet using DHCP"));

    wdt_enable(WDTO_8S);

    while(true)   // no point in carrying on, so stay in endless loop until watchdog reset
      ;
  }
#endif

  delay(1000); // give the Ethernet shield a second to initialize

#if DHCP == 0
  Serial.print(F("IP address (static): "));
#else
  Serial.print(F("IP address from DHCP server: "));
#endif
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
// * Update the watchdog timer
// * Listen to the UDP socket
void loop()
{
  aJsonObject* jsonObject;
  boolean foundHeatpump;
  int heatpumpIndex;
  prog_char* heatpumpModel;

  // The JSON response
  char response[54];
  const char *responseFmt = PSTR("{\"command\":\"%s\",\"identity\":\"%s\"");

  // Process the timers
  timer.update();

  // process incoming UDP packets
  int WPPacketSize = WPudp.parsePacket();
  if (WPPacketSize)
  {
    // Watch out the amount of free SRAM
    // This program uses dynamic allocation, and using too much also causes problems on freeing up the memory
    // The amount of free memory should always be the same at the beginning of the loop
    Serial.print(F("Start: free SRAM: "));
    Serial.println(freeRam());

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

    if (pushurl_channel != NULL && pushurl_channel->type != aJson_NULL)
    {
      Serial.print(F("Pushurl channel: "));
      Serial.println(pushurl_channel->valuestring);
    }

    if (strcmp_P(command->valuestring, PSTR("identify")) == 0)
    {
      // Create the response JSON just by printing it - this consumes less memory than using the aJson
      snprintf_P(response, sizeof(response), responseFmt, command->valuestring, macstr);

      if (pushurl_channel == NULL || pushurl_channel->type == aJson_NULL) {
        notification->sendUDPNotification(WPudp, pushurl_channel, response, sendHeatpumpJson);
      }
      else {
        notification->sendWPNotification(pushurl_channel, response, NULL, 3);
      }
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

        byte powerCmd = power->valueint;
        byte operatingModeCmd = mode->valueint;
        byte temperatureCmd = temperature->valueint;
        byte fanSpeedCmd = fan->valueint;

        // Create the response JSON just by printing it - this consumes less memory than using the aJson
        snprintf_P(response, sizeof(response), responseFmt, command->valuestring, macstr);
        if (pushurl_channel == NULL || pushurl_channel->type == aJson_NULL) {
          notification->sendUDPNotification(WPudp, pushurl_channel, response, NULL);
        }
        else {
          notification->sendWPNotification(pushurl_channel, response, NULL, 3);
        }

        foundHeatpump = false;
        heatpumpIndex=0;

        while (heatpumpIR[heatpumpIndex] != NULL && foundHeatpump == false)
        {
          Serial.print(F("Looping for "));

          heatpumpModel = (prog_char*)heatpumpIR[heatpumpIndex]->model();

          // 'heatpumpModel' is a PROGMEM pointer, so need to write a byte at a time
          while (char infoChar = pgm_read_byte(heatpumpModel++))
          {
            Serial.print(infoChar);
          }
          Serial.println();

          if (strcmp_P(model->valuestring, heatpumpIR[heatpumpIndex]->model()) == 0)
          {
            foundHeatpump = true;

            Serial.print(F("Found: "));
            Serial.println(model->valuestring);

            // Send the IR command
            Serial.println(F("Sending IR"));
            heatpumpIR[heatpumpIndex]->send(irSender, powerCmd, operatingModeCmd, fanSpeedCmd, temperatureCmd, VDIR_UP, HDIR_MIDDLE);

            // Is this a Panasonic CKP?
            if (strcmp_P(model->valuestring, PSTR("panasonic_ckp")) == 0) {
              // Send the 'timer cancel' signal 2 minutes later
              if (panasonicCancelTimer != 0)
              {
                Serial.println(F("Canceling existing Panasonic CKP timer"));
                timer.stop(panasonicCancelTimer);
                panasonicCancelTimer = 0;
              }

              // Set the Panasonic instance for 'sendPanasonicCKPCancelTimer'
              panasonicCKP = (PanasonicCKPHeatpumpIR*)heatpumpIR[heatpumpIndex];

              // Note that the argument to 'timer.after' has to be explicitly cast into 'long'
              // This timer fires in two minutes
              Serial.println(F("Scheduling timer cancel for Panasonic CKP"));
              panasonicCancelTimer = timer.after(2L*60L*1000L, sendPanasonicCKPCancelTimer);
            }
          }
          heatpumpIndex++;
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

// Send the JSON info about all supported heatpumps into the Stream (UDP, TCP etc)
// If the stream is NULL, just return the number of bytes which would be sent
int sendHeatpumpJson(Stream *stream)
{
  int i;
  int bytesSent;
  prog_char* info;

  const prog_char* header PROGMEM = ",\"heatpumpmodels\":[";
  if (stream != NULL) {
    stream->write(header);
  }
  bytesSent = strlen(header);

  i = 0;
  do {
    info = (prog_char*)heatpumpIR[i]->info();

    if (stream != NULL && info != NULL) {
      // Info is a PROGMEM pointer, so need to write a byte at a time
      while (char infoChar = pgm_read_byte(info++))
      {
        stream->write(infoChar);
      }

      bytesSent += strlen_P(heatpumpIR[i]->info());
    }

    if (heatpumpIR[i+1] != NULL) {
      if (stream != NULL) {
        stream->write(",");
      }
      bytesSent++;
    }

  }
  while (heatpumpIR[++i] != NULL);

  if (stream != NULL) {
    stream->write("]");
  }
  bytesSent++;

  return bytesSent;
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
// Free RAM - for figuring out the reason to upgrade from IDE 1.0.1 to 1.0.5
// Returns the free RAM in bytes - you'd be surprised to see how little that is :)
// http://playground.arduino.cc/Code/AvailableMemory
//

int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

// The most important thing of all, feed the watchdog
void feedWatchdog()
{
  wdt_reset();
}
