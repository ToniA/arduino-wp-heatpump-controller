// JSON / Windows Phone Push Messages based application to control Panasonic heat pumps / airconditioning devices
// * Panasonic CKP series
// * Panasonic DKE series
//
// Connect an IR led (with 1k resistor in series)
// between GND and digital pin 3
//
// Requires the Ethernet shield
//

#include <Arduino.h>
#include "SPI.h"
#include "avr/pgmspace.h"
#include <avr/wdt.h>      // For the hardware watchdog
#include "Ethernet.h"
#include <Timer.h>        // For the CKP cancel timer, and for the watchdog

#include <Ethernet.h>
#include <EEPROM.h>
#include <aJSON.h> // from https://github.com/interactive-matter/aJson

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte macAddress[] = { 0x02, 0x26, 0x89, 0x00, 0x00, 0x00 };
char macstr[18];

// IP address settings
IPAddress ip(192, 168, 0, 12);  // This MAC/IP address pair is also set as a static lease on the router
IPAddress broadcast(192, 168, 0, 255);
EthernetUDP WPudp;
EthernetClient client;

// Stream for the aJson class
aJsonStream WPudp_stream(&WPudp);

// Timer for Panasonic CKP timer cancel event, and for the watchdog
Timer timer;
byte panasonicCancelTimer = 0;

// The Windows Phone application port, randomly chosen :)
const int WPudpPort = 49722;

// Infrared LED on digital PIN 3 (needs a PWM pin)
// Connect with 1 kOhm resistor in series to GND
#define IR_LED_PIN        3

// Ethernet shield reset pin
#define ETHERNET_RST      A0

// Entropy pin needs to be unconnected
#define ENTROPY_PIN       A5

// Panasonic CKP timing constants
#define PANASONIC_AIRCON1_HDR_MARK   3400
#define PANASONIC_AIRCON1_HDR_SPACE  3500
#define PANASONIC_AIRCON1_BIT_MARK   800
#define PANASONIC_AIRCON1_ONE_SPACE  2700
#define PANASONIC_AIRCON1_ZERO_SPACE 1000
#define PANASONIC_AIRCON1_MSG_SPACE  14000

// Panasonic CKP codes
#define PANASONIC_AIRCON1_MODE_AUTO  0x06 // Operating mode
#define PANASONIC_AIRCON1_MODE_HEAT  0x04
#define PANASONIC_AIRCON1_MODE_COOL  0x02
#define PANASONIC_AIRCON1_MODE_DRY   0x03
#define PANASONIC_AIRCON1_MODE_FAN   0x01
#define PANASONIC_AIRCON1_MODE_ONOFF 0x00 // Toggle ON/OFF
#define PANASONIC_AIRCON1_MODE_KEEP  0x08 // Do not toggle ON/OFF
#define PANASONIC_AIRCON1_FAN_AUTO   0xF0 // Fan speed
#define PANASONIC_AIRCON1_FAN1       0x20
#define PANASONIC_AIRCON1_FAN2       0x30
#define PANASONIC_AIRCON1_FAN3       0x40
#define PANASONIC_AIRCON1_FAN4       0x50
#define PANASONIC_AIRCON1_FAN5       0x60
#define PANASONIC_AIRCON1_VS_AUTO    0xF0 // Vertical swing
#define PANASONIC_AIRCON1_VS_UP      0x90
#define PANASONIC_AIRCON1_VS_MUP     0xA0
#define PANASONIC_AIRCON1_VS_MIDDLE  0xB0
#define PANASONIC_AIRCON1_VS_MDOWN   0xC0
#define PANASONIC_AIRCON1_VS_DOWN    0xD0
#define PANASONIC_AIRCON1_HS_AUTO    0x08 // Horizontal swing
#define PANASONIC_AIRCON1_HS_MANUAL  0x00

// Panasonic DKE, JKE & NKE timing constants
#define PANASONIC_AIRCON2_HDR_MARK   3500
#define PANASONIC_AIRCON2_HDR_SPACE  1800
#define PANASONIC_AIRCON2_BIT_MARK   420
#define PANASONIC_AIRCON2_ONE_SPACE  1350
#define PANASONIC_AIRCON2_ZERO_SPACE 470
#define PANASONIC_AIRCON2_MSG_SPACE  10000

// Panasonic DKE, JNE & NKE codes
#define PANASONIC_AIRCON2_MODE_AUTO  0x00 // Operating mode
#define PANASONIC_AIRCON2_MODE_HEAT  0x40
#define PANASONIC_AIRCON2_MODE_COOL  0x30
#define PANASONIC_AIRCON2_MODE_DRY   0x20
#define PANASONIC_AIRCON2_MODE_FAN   0x60
#define PANASONIC_AIRCON2_MODE_OFF   0x00 // Power OFF
#define PANASONIC_AIRCON2_MODE_ON    0x01
#define PANASONIC_AIRCON2_TIMER_CNL  0x08
#define PANASONIC_AIRCON2_FAN_AUTO   0xA0 // Fan speed
#define PANASONIC_AIRCON2_FAN1       0x30
#define PANASONIC_AIRCON2_FAN2       0x40
#define PANASONIC_AIRCON2_FAN3       0x50
#define PANASONIC_AIRCON2_FAN4       0x60
#define PANASONIC_AIRCON2_FAN5       0x70
#define PANASONIC_AIRCON2_VS_AUTO    0x0F // Vertical swing
#define PANASONIC_AIRCON2_VS_UP      0x01
#define PANASONIC_AIRCON2_VS_MUP     0x02
#define PANASONIC_AIRCON2_VS_MIDDLE  0x03
#define PANASONIC_AIRCON2_VS_MDOWN   0x04
#define PANASONIC_AIRCON2_VS_DOWN    0x05
#define PANASONIC_AIRCON2_HS_AUTO    0x0D // Horizontal swing
#define PANASONIC_AIRCON2_HS_MIDDLE  0x06
#define PANASONIC_AIRCON2_HS_LEFT    0x09
#define PANASONIC_AIRCON2_HS_MLEFT   0x0A
#define PANASONIC_AIRCON2_HS_MRIGHT  0x0B
#define PANASONIC_AIRCON2_HS_RIGHT   0x0C

#define PANASONIC_DKE 0
#define PANASONIC_JKE 1
#define PANASONIC_NKE 2

// Midea timing constants
#define MIDEA_AIRCON1_HDR_MARK       4350
#define MIDEA_AIRCON1_HDR_SPACE      4230
#define MIDEA_AIRCON1_BIT_MARK       520
#define MIDEA_AIRCON1_ONE_SPACE      1650
#define MIDEA_AIRCON1_ZERO_SPACE     550
#define MIDEA_AIRCON1_MSG_SPACE      5100

// MIDEA codes
#define MIDEA_AIRCON1_MODE_AUTO      0x10 // Operating mode
#define MIDEA_AIRCON1_MODE_HEAT      0x30
#define MIDEA_AIRCON1_MODE_COOL      0x00
#define MIDEA_AIRCON1_MODE_DRY       0x20
#define MIDEA_AIRCON1_MODE_FAN       0x60
#define MIDEA_AIRCON1_MODE_FP        0x70 // Not a real mode...
#define MIDEA_AIRCON1_MODE_OFF       0xFE // Power OFF - not real codes, but we need something...
#define MIDEA_AIRCON1_MODE_ON        0xFF // Power ON
#define MIDEA_AIRCON1_FAN_AUTO       0x02 // Fan speed
#define MIDEA_AIRCON1_FAN1           0x06
#define MIDEA_AIRCON1_FAN2           0x05
#define MIDEA_AIRCON1_FAN3           0x03

// Carrier 42NQV035G / 38NYV035H2 (remote control WH-L05SE) timing constants and codes

#define CARRIER_AIRCON1_HDR_MARK   4320
#define CARRIER_AIRCON1_HDR_SPACE  4350
#define CARRIER_AIRCON1_BIT_MARK   500
#define CARRIER_AIRCON1_ONE_SPACE  1650
#define CARRIER_AIRCON1_ZERO_SPACE 550
#define CARRIER_AIRCON1_MSG_SPACE  7400

#define CARRIER_AIRCON1_MODE_AUTO  0x00 // Operating mode
#define CARRIER_AIRCON1_MODE_HEAT  0xC0
#define CARRIER_AIRCON1_MODE_COOL  0x80
#define CARRIER_AIRCON1_MODE_DRY   0x40
#define CARRIER_AIRCON1_MODE_FAN   0x20
#define CARRIER_AIRCON1_MODE_OFF   0xE0 // Power OFF
#define CARRIER_AIRCON1_FAN_AUTO   0x00 // Fan speed
#define CARRIER_AIRCON1_FAN1       0x02
#define CARRIER_AIRCON1_FAN2       0x06
#define CARRIER_AIRCON1_FAN3       0x01
#define CARRIER_AIRCON1_FAN4       0x05
#define CARRIER_AIRCON1_FAN5       0x03

// JSON data about supported pumps
// * name
// * displayName
// * numberOfModes
// * minTemperature
// * maxTemperature
// * numberOfFanSpeeds
// * maintenance modes

prog_char heatpumpModelData[] PROGMEM = {"\"heatpumpmodels\":["
"{\"model\":\"panasonic_ckp\",\"displayName\":\"Panasonic CKP\",\"numberOfModes\":5,\"minTemperature\":16,\"maxTemperature\":30,\"numberOfFanSpeeds\":6},"
"{\"model\":\"panasonic_dke\",\"displayName\":\"Panasonic DKE\",\"numberOfModes\":5,\"minTemperature\":16,\"maxTemperature\":30,\"numberOfFanSpeeds\":6},"
"{\"model\":\"panasonic_jke\",\"displayName\":\"Panasonic JKE\",\"numberOfModes\":5,\"minTemperature\":16,\"maxTemperature\":30,\"numberOfFanSpeeds\":6},"
"{\"model\":\"panasonic_nke\",\"displayName\":\"Panasonic NKE\",\"numberOfModes\":6,\"minTemperature\":16,\"maxTemperature\":30,\"numberOfFanSpeeds\":6,\"maintenance\":[8,10]},"
"{\"model\":\"carrier\",\"displayName\":\"Carrier\",\"numberOfModes\":5,\"minTemperature\":17,\"maxTemperature\":30,\"numberOfFanSpeeds\":6},"
"{\"model\":\"midea\",\"displayName\":\"Ultimate Pro Plus 13FP\",\"numberOfModes\":6,\"minTemperature\":16,\"maxTemperature\":30,\"numberOfFanSpeeds\":4,\"maintenance\":[10]}"
"]"};

// Send the Panasonic CKP code

void sendPanasonicCKP(byte operatingMode, byte fanSpeed, byte temperature, byte swingV, byte swingH)
{
  byte sendBuffer[4];

  // Fan speed & temperature, temperature needs to be 27 in FAN mode
  if (operatingMode == PANASONIC_AIRCON1_MODE_FAN || operatingMode == (PANASONIC_AIRCON1_MODE_FAN | PANASONIC_AIRCON1_MODE_KEEP ))
  {
    temperature = 27;
  }

  sendBuffer[0] = fanSpeed | (temperature - 15);

  // Power toggle & operation mode
  sendBuffer[1] = operatingMode;

  // Swings
  sendBuffer[2] = swingV | swingH;

  // Always 0x36
  sendBuffer[3]  = 0x36;

  // Send the code
  sendPanasonicCKPraw(sendBuffer);
}

// Send the Panasonic CKP raw code

void sendPanasonicCKPraw(byte sendBuffer[])
{
  // 40 kHz PWM frequency
  enableIROut(40);

  // Header, two first bytes repeated
  mark(PANASONIC_AIRCON1_HDR_MARK);
  space(PANASONIC_AIRCON1_HDR_SPACE);

  for (int i=0; i<2; i++) {
    sendIRByte(sendBuffer[0], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);
    sendIRByte(sendBuffer[0], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);
    sendIRByte(sendBuffer[1], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);
    sendIRByte(sendBuffer[1], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);

    mark(PANASONIC_AIRCON1_HDR_MARK);
    space(PANASONIC_AIRCON1_HDR_SPACE);
  }

  // Pause

  mark(PANASONIC_AIRCON1_BIT_MARK);
  space(PANASONIC_AIRCON1_MSG_SPACE);

  // Header, two last bytes repeated

  mark(PANASONIC_AIRCON1_HDR_MARK);
  space(PANASONIC_AIRCON1_HDR_SPACE);

  for (int i=0; i<2; i++) {
    sendIRByte(sendBuffer[2], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);
    sendIRByte(sendBuffer[2], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);
    sendIRByte(sendBuffer[3], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);
    sendIRByte(sendBuffer[3], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);

    mark(PANASONIC_AIRCON1_HDR_MARK);
    space(PANASONIC_AIRCON1_HDR_SPACE);
  }

  mark(PANASONIC_AIRCON1_BIT_MARK);
  space(0);
}

// Send the Panasonic CKP On/Off code
//
// CKP does not have discrete ON/OFF commands, but this can be emulated by using the timer
// The side-effects of using the timer are:
// * ONE minute delay before the power state changes
// * the 'TIMER' led (orange) is lit
// * a timer event is scheduled to cancel the timer after TWO minutes (the 'TIMER' led turns off

void sendPanasonicCKPOnOff(boolean powerState, boolean cancelTimer)
{
  byte ON_msg[] =     { 0x7F, 0x38, 0xBF, 0x38, 0x10, 0x3D, 0x80, 0x3D, 0x09, 0x34, 0x80, 0x34 }; //  ON at 00:10, time now 00:09, no OFF timing
  byte OFF_msg[] =    { 0x10, 0x38, 0x80, 0x38, 0x7F, 0x3D, 0xBF, 0x3D, 0x09, 0x34, 0x80, 0x34 }; // OFF at 00:10, time now 00:09, no ON timing
  byte CANCEL_msg[] = { 0x7F, 0x38, 0xBF, 0x38, 0x7F, 0x3D, 0xBF, 0x3D, 0x17, 0x34, 0x80, 0x34 }; // Timer CANCEL

  byte *sendBuffer;

  if ( cancelTimer == true ) {
    sendBuffer = CANCEL_msg;
  } else {
    if ( powerState == true ) {
      sendBuffer = ON_msg;
    } else {
      sendBuffer = OFF_msg;
    }
  }

  // 40 kHz PWM frequency
  enableIROut(40);

  for (int i=0; i<6; i++) {
    mark(PANASONIC_AIRCON1_HDR_MARK);
    space(PANASONIC_AIRCON1_HDR_SPACE);

    sendIRByte(sendBuffer[i*2 + 0], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);
    sendIRByte(sendBuffer[i*2 + 0], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);
    sendIRByte(sendBuffer[i*2 + 1], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);
    sendIRByte(sendBuffer[i*2 + 1], PANASONIC_AIRCON1_BIT_MARK, PANASONIC_AIRCON1_ZERO_SPACE, PANASONIC_AIRCON1_ONE_SPACE);

    mark(PANASONIC_AIRCON1_HDR_MARK);
    space(PANASONIC_AIRCON1_HDR_SPACE);

    if ( i < 5 ) {
      mark(PANASONIC_AIRCON1_BIT_MARK);
      space(PANASONIC_AIRCON1_MSG_SPACE);
    }
  }

  mark(PANASONIC_AIRCON1_BIT_MARK);
  space(0);
}


// Send the Panasonic CKP timer cancel

void sendPanasonicCKPCancelTimer()
{
  Serial.println(F("Sending Panasonic CKP timer cancel"));

  sendPanasonicCKPOnOff(false, true);
}


// Send the Panasonic DKE/JKE/NKE code

void sendPanasonic(byte model, byte operatingMode, byte fanSpeed, byte temperature, byte swingV, byte swingH)
{
  // Only bytes 13, 14, 16, 17 and 26 are modified, DKE and JKE seem to share the same template?
  byte panasonicTemplate[][27] = {
    // DKE, model 0
    { 0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x0E, 0xE0, 0x00, 0x00, 0x01, 0x00, 0x06, 0x00 },
    // JKE, model 1
    { 0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x0E, 0xE0, 0x00, 0x00, 0x81, 0x00, 0x00, 0x00 },
    // NKE, model 2
    { 0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x80, 0x00, 0x06, 0x00, 0x0E, 0xE0, 0x00, 0x00, 0x81, 0x00, 0x00, 0x00 }
    //   0     1     2     3     4     5     6     7     8     9    10    11    12    13    14   15     16    17    18    19    20    21    22    23    24    25    26
  };


  panasonicTemplate[model][13] = operatingMode;
  panasonicTemplate[model][14] = temperature << 1;
  panasonicTemplate[model][16] = fanSpeed | swingV;

  // Only the DKE model has a setting for the horizontal air flow
  if ( model == PANASONIC_DKE) {
    panasonicTemplate[model][17] = swingH;
  }

  // Checksum calculation

  byte checksum = 0xF4;

  for (int i=0; i<26; i++) {
    checksum += panasonicTemplate[model][i];
  }

  panasonicTemplate[model][26] = checksum;

  // 40 kHz PWM frequency
  enableIROut(40);

  // Header
  mark(PANASONIC_AIRCON2_HDR_MARK);
  space(PANASONIC_AIRCON2_HDR_SPACE);

  // First 8 bytes
  for (int i=0; i<8; i++) {
    sendIRByte(panasonicTemplate[model][i], PANASONIC_AIRCON2_BIT_MARK, PANASONIC_AIRCON2_ZERO_SPACE, PANASONIC_AIRCON2_ONE_SPACE);
  }

  // Pause
  mark(PANASONIC_AIRCON2_BIT_MARK);
  space(PANASONIC_AIRCON2_MSG_SPACE);

  // Header
  mark(PANASONIC_AIRCON2_HDR_MARK);
  space(PANASONIC_AIRCON2_HDR_SPACE);

  // Last 19 bytes
  for (int i=8; i<27; i++) {
    sendIRByte(panasonicTemplate[model][i], PANASONIC_AIRCON2_BIT_MARK, PANASONIC_AIRCON2_ZERO_SPACE, PANASONIC_AIRCON2_ONE_SPACE);
  }

  mark(PANASONIC_AIRCON2_BIT_MARK);
  space(0);
}


// Send the Midea code

void sendMidea(byte operatingMode, byte fanSpeed, byte temperature)
{
  byte sendBuffer[3] = { 0x4D, 0x00, 0x00 }; // First byte is always 0x4D

  byte temperatures[] = {0, 8, 12, 4, 6, 14, 10, 2, 3, 11, 9, 1, 5, 13 };

  byte OffMsg[] = {0x4D, 0xDE, 0x07 };
  byte FPMsg[]  = {0xAD, 0xAF, 0xB5 };

  if (operatingMode == MIDEA_AIRCON1_MODE_OFF)
  {
    sendMidearaw( OffMsg );
  }
  else if (operatingMode == MIDEA_AIRCON1_MODE_FP)
  {
    sendMidearaw( FPMsg );
  }
  else
  {
    sendBuffer[1] = ~fanSpeed;

    if ( operatingMode == MIDEA_AIRCON1_MODE_FAN )
    {
      sendBuffer[2] = MIDEA_AIRCON1_MODE_DRY | 0x07;
    }
    else
    {
      sendBuffer[2] = operatingMode | temperatures[temperature-17];
    }

    // Send the code
    sendMidearaw(sendBuffer);
  }
}

// Send the Midea raw code

void sendMidearaw(byte sendBuffer[])
{
  // 40 kHz PWM frequency
  enableIROut(40);

  // Header
  mark(MIDEA_AIRCON1_HDR_MARK);
  space(MIDEA_AIRCON1_HDR_SPACE);

  // Six bytes, every second byte is a bitwise not of the previous byte
  for (int i=0; i<3; i++) {
    sendIRByte(sendBuffer[i], MIDEA_AIRCON1_BIT_MARK, MIDEA_AIRCON1_ZERO_SPACE, MIDEA_AIRCON1_ONE_SPACE);
    sendIRByte(~sendBuffer[i], MIDEA_AIRCON1_BIT_MARK, MIDEA_AIRCON1_ZERO_SPACE, MIDEA_AIRCON1_ONE_SPACE);
  }

  // Pause

  mark(MIDEA_AIRCON1_BIT_MARK);
  space(MIDEA_AIRCON1_MSG_SPACE);

  // Header, two last bytes repeated

  mark(MIDEA_AIRCON1_HDR_MARK);
  space(MIDEA_AIRCON1_HDR_SPACE);

  // Six bytes, every second byte is a bitwise not of the previous byte
  for (int i=0; i<3; i++) {
    sendIRByte(sendBuffer[i], MIDEA_AIRCON1_BIT_MARK, MIDEA_AIRCON1_ZERO_SPACE, MIDEA_AIRCON1_ONE_SPACE);
    sendIRByte(~sendBuffer[i], MIDEA_AIRCON1_BIT_MARK, MIDEA_AIRCON1_ZERO_SPACE, MIDEA_AIRCON1_ONE_SPACE);
  }

  // End mark

  mark(MIDEA_AIRCON1_BIT_MARK);
  space(0);
}

// Send the Carrier code
// Carrier has the LSB and MSB in different format than Panasonic

void sendCarrier(byte operatingMode, byte fanSpeed, byte temperature)
{
  byte sendBuffer[9] = { 0x4f, 0xb0, 0xc0, 0x3f, 0x80, 0x00, 0x00, 0x00, 0x00 }; // The data is on the last four bytes

  byte temperatures[] = { 0x00, 0x08, 0x04, 0x0c, 0x02, 0x0a, 0x06, 0x0e, 0x01, 0x09, 0x05, 0x0d, 0x03, 0x0b };
  byte checksum = 0;

  sendBuffer[5] = temperatures[(temperature-17)];
  sendBuffer[6] = operatingMode | fanSpeed;

  // Checksum

  for (int i=0; i<8; i++) {
    checksum += Bit_Reverse(sendBuffer[i]);
  }

  sendBuffer[8] = Bit_Reverse(checksum);

  // 40 kHz PWM frequency
  enableIROut(40);

  // Header
  mark(CARRIER_AIRCON1_HDR_MARK);
  space(CARRIER_AIRCON1_HDR_SPACE);

  // Payload
  for (int i=0; i<sizeof(sendBuffer); i++) {
    sendIRByte(sendBuffer[i], CARRIER_AIRCON1_BIT_MARK, CARRIER_AIRCON1_ZERO_SPACE, CARRIER_AIRCON1_ONE_SPACE);
  }

  // Pause + new header
  mark(CARRIER_AIRCON1_BIT_MARK);
  space(CARRIER_AIRCON1_MSG_SPACE);

  mark(CARRIER_AIRCON1_HDR_MARK);
  space(CARRIER_AIRCON1_HDR_SPACE);

  // Payload again
  for (int i=0; i<sizeof(sendBuffer); i++) {
    sendIRByte(sendBuffer[i], CARRIER_AIRCON1_BIT_MARK, CARRIER_AIRCON1_ZERO_SPACE, CARRIER_AIRCON1_ONE_SPACE);
  }

  // End mark
  mark(CARRIER_AIRCON1_BIT_MARK);
  space(0);
}

// See http://www.nrtm.org/index.php/2013/07/25/reverse-bits-in-a-byte/
byte Bit_Reverse( byte x )
{
  //          01010101  |         10101010
  x = ((x >> 1) & 0x55) | ((x << 1) & 0xaa);
  //          00110011  |         11001100
  x = ((x >> 2) & 0x33) | ((x << 2) & 0xcc);
  //          00001111  |         11110000
  x = ((x >> 4) & 0x0f) | ((x << 4) & 0xf0);
  return x;
}

// Panasonic CKP numeric values to command bytes

void sendCKPCmd(byte powerModeCmd, byte operatingModeCmd, byte fanSpeedCmd, byte temperatureCmd, byte swingVCmd, byte swingHCmd)
{
  // Sensible defaults for the heat pump mode

  byte powerMode     = false;
  byte operatingMode = PANASONIC_AIRCON1_MODE_KEEP;
  byte fanSpeed      = PANASONIC_AIRCON1_FAN_AUTO;
  byte temperature   = 23;
  byte swingV        = PANASONIC_AIRCON1_VS_UP;
  byte swingH        = PANASONIC_AIRCON1_HS_AUTO;

  switch (powerModeCmd)
  {
    case 1:
      powerMode = true;
      break;
  }

  switch (operatingModeCmd)
  {
    case 1:
      operatingMode |= PANASONIC_AIRCON1_MODE_AUTO;
      break;
    case 2:
      operatingMode |= PANASONIC_AIRCON1_MODE_HEAT;
      break;
    case 3:
      operatingMode |= PANASONIC_AIRCON1_MODE_COOL;
      break;
    case 4:
      operatingMode |= PANASONIC_AIRCON1_MODE_DRY;
      break;
    case 5:
      operatingMode |= PANASONIC_AIRCON1_MODE_FAN;
      temperatureCmd = 27; // Temperature is always 27 in FAN mode
      break;
    default:
      operatingMode |= PANASONIC_AIRCON1_MODE_HEAT;
      break;
  }

  switch (fanSpeedCmd)
  {
    case 1:
      fanSpeed = PANASONIC_AIRCON1_FAN_AUTO;
      break;
    case 2:
      fanSpeed = PANASONIC_AIRCON1_FAN1;
      break;
    case 3:
      fanSpeed = PANASONIC_AIRCON1_FAN2;
      break;
    case 4:
      fanSpeed = PANASONIC_AIRCON1_FAN3;
      break;
    case 5:
      fanSpeed = PANASONIC_AIRCON1_FAN4;
      break;
    case 6:
      fanSpeed = PANASONIC_AIRCON1_FAN5;
      break;
  }

  if ( temperatureCmd > 15 && temperatureCmd < 31)
  {
    temperature = temperatureCmd;
  }

  switch (swingVCmd)
  {
    case 1:
      swingV = PANASONIC_AIRCON1_VS_AUTO;
      break;
    case 2:
      swingV = PANASONIC_AIRCON1_VS_UP;
      break;
    case 3:
      swingV = PANASONIC_AIRCON1_VS_MUP;
      break;
    case 4:
      swingV = PANASONIC_AIRCON1_VS_MIDDLE;
      break;
    case 5:
      swingV = PANASONIC_AIRCON1_VS_MDOWN;
      break;
    case 6:
      swingV = PANASONIC_AIRCON1_VS_DOWN;
      break;
  }

  switch (swingHCmd)
  {
    case 1:
      swingH = PANASONIC_AIRCON1_HS_AUTO;
      break;
    case 2:
      swingH = PANASONIC_AIRCON1_HS_MANUAL;
      break;
  }

  sendPanasonicCKP(operatingMode, fanSpeed, temperature, swingV, swingH);
  delay(1000); // Sleep 1 second between the messages

  // This will change the power state in one minute from now
  sendPanasonicCKPOnOff(powerMode, false);

  // Send the 'timer cancel' signal 2 minutes later
  if (panasonicCancelTimer != 0)
  {
    timer.stop(panasonicCancelTimer);
    panasonicCancelTimer = 0;
  }

  // Note that the argument to 'timer.after' has to be explicitly cast into 'long'
  panasonicCancelTimer = timer.after(2L*60L*1000L, sendPanasonicCKPCancelTimer);

  Serial.print(F("'Timer cancel' timer ID: "));
  Serial.println(panasonicCancelTimer);
}


// Panasonic DKE numeric values to command bytes

void sendPanasonicCmd(byte model, byte powerModeCmd, byte operatingModeCmd, byte fanSpeedCmd, byte temperatureCmd, byte swingVCmd, byte swingHCmd)
{
  // Sensible defaults for the heat pump mode

  byte operatingMode = PANASONIC_AIRCON2_TIMER_CNL;
  byte fanSpeed      = PANASONIC_AIRCON2_FAN_AUTO;
  byte temperature   = 23;
  byte swingV        = PANASONIC_AIRCON2_VS_UP;
  byte swingH        = PANASONIC_AIRCON2_HS_AUTO;

  switch (powerModeCmd)
  {
    case 1:
      operatingMode |= PANASONIC_AIRCON2_MODE_ON;
      break;
  }

  switch (operatingModeCmd)
  {
    case 1:
      operatingMode |= PANASONIC_AIRCON2_MODE_AUTO;
      break;
    case 2:
      operatingMode |= PANASONIC_AIRCON2_MODE_HEAT;
      break;
    case 3:
      operatingMode |= PANASONIC_AIRCON2_MODE_COOL;
      break;
    case 4:
      operatingMode |= PANASONIC_AIRCON2_MODE_DRY;
      break;
    case 5:
      operatingMode |= PANASONIC_AIRCON2_MODE_FAN;
      temperatureCmd = 27; // Temperature is always 27 in FAN mode
      break;
  }

  switch (fanSpeedCmd)
  {
    case 1:
      fanSpeed = PANASONIC_AIRCON2_FAN_AUTO;
      break;
    case 2:
      fanSpeed = PANASONIC_AIRCON2_FAN1;
      break;
    case 3:
      fanSpeed = PANASONIC_AIRCON2_FAN2;
      break;
    case 4:
      fanSpeed = PANASONIC_AIRCON2_FAN3;
      break;
    case 5:
      fanSpeed = PANASONIC_AIRCON2_FAN4;
      break;
    case 6:
      fanSpeed = PANASONIC_AIRCON2_FAN5;
      break;
  }

  if ( temperatureCmd > 15 && temperatureCmd < 31)
  {
    temperature = temperatureCmd;
  }

  switch (swingVCmd)
  {
    case 1:
      swingV = PANASONIC_AIRCON2_VS_AUTO;
      break;
    case 2:
      swingV = PANASONIC_AIRCON2_VS_UP;
      break;
    case 3:
      swingV = PANASONIC_AIRCON2_VS_MUP;
      break;
    case 4:
      swingV = PANASONIC_AIRCON2_VS_MIDDLE;
      break;
    case 5:
      swingV = PANASONIC_AIRCON2_VS_MDOWN;
      break;
    case 6:
      swingV = PANASONIC_AIRCON2_VS_DOWN;
      break;
  }

  switch (swingHCmd)
  {
    case 1:
      swingH = PANASONIC_AIRCON2_HS_AUTO;
      break;
    case 2:
      swingH = PANASONIC_AIRCON2_HS_MIDDLE;
      break;
    case 3:
      swingH = PANASONIC_AIRCON2_HS_LEFT;
      break;
    case 4:
      swingH = PANASONIC_AIRCON2_HS_MLEFT;
      break;
    case 5:
      swingH = PANASONIC_AIRCON2_HS_RIGHT;
      break;
    case 6:
      swingH = PANASONIC_AIRCON2_HS_MRIGHT;
      break;
  }

  // NKE has +8 / + 10 maintenance heating, which also means MAX fanspeed
  if ( model == PANASONIC_NKE )
  {
    if ( temperatureCmd == 8 || temperatureCmd == 10 )
    {
      temperature = temperatureCmd;
      fanSpeed = PANASONIC_AIRCON2_FAN5;
    }
  }

  sendPanasonic(model, operatingMode, fanSpeed, temperature, swingV, swingH);
}


// Midea MSR1-12HRN1-QC2 + MOA1-12HN1-QC2 numeric values to command bytes (Ultimate Pro Plus Basic 13FP)

void sendMideaCmd(byte powerModeCmd, byte operatingModeCmd, byte fanSpeedCmd, byte temperatureCmd, byte swingVCmd, byte swingHCmd)
{
  // Sensible defaults for the heat pump mode

  byte operatingMode = MIDEA_AIRCON1_MODE_HEAT;
  byte fanSpeed = MIDEA_AIRCON1_FAN_AUTO;
  byte temperature = 23;


  switch (powerModeCmd)
  {
    case 0:
      // OFF is a special case
      operatingMode = MIDEA_AIRCON1_MODE_OFF;
      sendMidea(operatingMode, fanSpeed, temperature);
      return;
  }

  switch (operatingModeCmd)
  {
    case 1:
      operatingMode = MIDEA_AIRCON1_MODE_AUTO;
      break;
    case 2:
      operatingMode = MIDEA_AIRCON1_MODE_HEAT;
      break;
    case 3:
      operatingMode = MIDEA_AIRCON1_MODE_COOL;
      break;
    case 4:
      operatingMode = MIDEA_AIRCON1_MODE_DRY;
      break;
    case 5:
      operatingMode = MIDEA_AIRCON1_MODE_FAN;
      break;
    case 6:
      // FP is a special case
      operatingMode = MIDEA_AIRCON1_MODE_FP;
      sendMidea(operatingMode, fanSpeed, temperature);
      return;
  }

  switch (fanSpeedCmd)
  {
    case 1:
      fanSpeed = MIDEA_AIRCON1_FAN_AUTO;
      break;
    case 2:
      fanSpeed = MIDEA_AIRCON1_FAN1;
      break;
    case 3:
      fanSpeed = MIDEA_AIRCON1_FAN2;
      break;
    case 4:
      fanSpeed = MIDEA_AIRCON1_FAN3;
      break;
  }

  if ( temperatureCmd > 15 && temperatureCmd < 31)
  {
    temperature = temperatureCmd;
  }

  sendMidea(operatingMode, fanSpeed, temperature);
}


// Carrier 42NQV035G / 38NYV035H2 (remote control WH-L05SE) numeric values to command bytes

void sendCarrierCmd(byte powerModeCmd, byte operatingModeCmd, byte fanSpeedCmd, byte temperatureCmd, byte swingVCmd, byte swingHCmd)
{
  // Sensible defaults for the heat pump mode

  byte operatingMode = CARRIER_AIRCON1_MODE_HEAT;
  byte fanSpeed = CARRIER_AIRCON1_FAN_AUTO;
  byte temperature = 23;

  if (powerModeCmd == 0)
  {
    operatingMode = CARRIER_AIRCON1_MODE_OFF;
  }
  else
  {
    switch (operatingModeCmd)
    {
      case 1:
        operatingMode = CARRIER_AIRCON1_MODE_AUTO;
        break;
      case 2:
        operatingMode = CARRIER_AIRCON1_MODE_HEAT;
        break;
      case 3:
        operatingMode = CARRIER_AIRCON1_MODE_COOL;
        break;
      case 4:
        operatingMode = CARRIER_AIRCON1_MODE_DRY;
        break;
      case 5:
        operatingMode = CARRIER_AIRCON1_MODE_FAN;
        temperatureCmd = 22; // Temperature is always 22 on FAN mode
        break;
    }
  }

  switch (fanSpeedCmd)
  {
    case 1:
      fanSpeed = CARRIER_AIRCON1_FAN_AUTO;
      break;
    case 2:
      fanSpeed = CARRIER_AIRCON1_FAN1;
      break;
    case 3:
      fanSpeed = CARRIER_AIRCON1_FAN2;
      break;
    case 4:
      fanSpeed = CARRIER_AIRCON1_FAN3;
      break;
    case 5:
      fanSpeed = CARRIER_AIRCON1_FAN4;
      break;
    case 6:
      fanSpeed = CARRIER_AIRCON1_FAN5;
      break;
  }

  if ( temperatureCmd > 16 && temperatureCmd < 31)
  {
    temperature = temperatureCmd;
  }

  sendCarrier(operatingMode, fanSpeed, temperature);
}


// Send a byte over IR

void sendIRByte(byte sendByte, int bitMarkLength, int zeroSpaceLength, int oneSpaceLength)
{
  for (int i=0; i<8 ; i++)
  {
    if (sendByte & 0x01)
    {
      mark(bitMarkLength);
      space(oneSpaceLength);
    }
    else
    {
      mark(bitMarkLength);
      space(zeroSpaceLength);
    }

    sendByte >>= 1;
  }
}

// 'mark', 'space' and 'enableIROut' have been taken
// from Ken Shirriff's IRRemote library:
// https://github.com/shirriff/Arduino-IRremote

void mark(int time) {
  // Sends an IR mark for the specified number of microseconds.
  // The mark output is modulated at the PWM frequency.
  (TCCR2A |= _BV(COM2B1)); // Enable pin 3 PWM output
  delayMicroseconds(time);
}

void space(int time) {
  // Sends an IR space for the specified number of microseconds.
  // A space is no output, so the PWM output is disabled.
  (TCCR2A &= ~(_BV(COM2B1))); // Disable pin 3 PWM output
  delayMicroseconds(time);
}

void enableIROut(int khz) {
  pinMode(IR_LED_PIN, OUTPUT);
  digitalWrite(IR_LED_PIN, LOW); // When not sending PWM, we want it low

  const uint8_t pwmval = F_CPU / 2000 / (khz);
  TCCR2A = _BV(WGM20);
  TCCR2B = _BV(WGM22) | _BV(CS20);
  OCR2A = pwmval;
  OCR2B = pwmval / 3;
}


// The setup

void setup()
{
  // Initialize serial
  Serial.begin(9600);
  Serial.print(F("Starting... "));

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

  // initialize the Ethernet adapter
  Ethernet.begin(macAddress, ip);

  // Initialize the Windows Phone app UDP port
  WPudp.begin(WPudpPort);


  // Initialize the hardware watchdog
  // Watchdog requires ADABOOT to work
  // See:
  // * http://www.ladyada.net/library/arduino/bootloader.html
  // * http://www.grozeaion.com/electronics/arduino/131-atmega328p-burning-bootloader.html
  wdt_enable(WDTO_8S);
  timer.every(2000, feedWatchdog);

  Serial.println(F("Started"));
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
  char *responseFmt = PSTR("{\"command\":\"%s\",\"identity\":\"%s\"");

  // Process the timers
  timer.update();

  // process incoming UDP packets
  int WPPacketSize = WPudp.parsePacket();
  if (WPPacketSize)
  {
    // Debug: show the amount of free SRAM
    Serial.print(F("Loop start: free RAM: "));
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

    if (pushurl_channel != NULL)
    {
      Serial.print(F("Pushurl channel: "));
      Serial.println(pushurl_channel->valuestring);
    }

    if (strcmp_P(command->valuestring, PSTR("identify")) == 0)
    {
      // Create the response JSON just by printing it - this consumes less memory than using the aJson
      snprintf_P(response, sizeof(response), responseFmt, command->valuestring, macstr);
      sendNotification(pushurl_channel, response, heatpumpModelData, 3);
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
        sendNotification(pushurl_channel, response, NULL, 3);

        if (strcmp_P(model->valuestring, PSTR("panasonic_ckp")) == 0)
        {
          sendCKPCmd(power->valueint, mode->valueint, fan->valueint, temperature->valueint, 2, 1);
        }
        else if (strcmp_P(model->valuestring, PSTR("panasonic_dke")) == 0)
        {
          sendPanasonicCmd(PANASONIC_DKE, power->valueint, mode->valueint, fan->valueint, temperature->valueint, 2, 1);
        }
        else if (strcmp_P(model->valuestring, PSTR("panasonic_jke")) == 0)
        {
          sendPanasonicCmd(PANASONIC_JKE, power->valueint, mode->valueint, fan->valueint, temperature->valueint, 2, 1);
        }
        else if (strcmp_P(model->valuestring, PSTR("panasonic_nke")) == 0)
        {
          sendPanasonicCmd(PANASONIC_NKE, power->valueint, mode->valueint, fan->valueint, temperature->valueint, 2, 1);
        }
        else if (strcmp_P(model->valuestring, PSTR("midea")) == 0)
        {
          sendMideaCmd(power->valueint, mode->valueint, fan->valueint, temperature->valueint, 0, 0);
        }
        else if (strcmp_P(model->valuestring, PSTR("carrier")) == 0)
        {
          sendCarrierCmd(power->valueint, mode->valueint, fan->valueint, temperature->valueint, 0, 0);
        }
        else {
         Serial.print(F("got nothing: "));
         Serial.println(model->valuestring);
        }
      }
    }

    // Free the memory used by the JSON objects
    aJson.deleteItem(jsonObject);

    // Debug: show the amount of free SRAM - this should be the same all the time, or there's a memory leak
    Serial.print(F("Loop end: free RAM: "));
    Serial.println(freeRam());
  }
}

//
// Send a notification, either through UDP or through Windows Phone push notification service
//

void sendNotification(aJsonObject *pushurl_channel, char *payload1, const prog_char *payload2, int notificationType)
{
  if (pushurl_channel == NULL)
  {
    Serial.print(F("Sending a UDP response: "));
    Serial.println(payload1);

    WPudp.beginPacket(WPudp.remoteIP(), WPudp.remotePort());

    // The actual payload
    WPudp.write(payload1);

    // The extra payload from PROGMEM
    if (payload2 != NULL)
    {
      WPudp.write(",");

      byte character;
      while ( (character = pgm_read_byte(payload2++)) != 0)
      {
        WPudp.write(character);
      }
    }

    WPudp.write("}"); // payload1 does not end with '}'
    WPudp.endPacket();
  }
  else
  {
    Serial.print(F("Sending a Windows Phone notification: "));
    Serial.println(payload1);

    // The channel is in form 'host:port:path'
    // Break it down in place to save precious RAM
    char *host = pushurl_channel->valuestring;
    char *port = strchr(host, ':');
    port[0] = '\0';
    port++;
    char *path = strchr(port, ':');
    path[0] = '\0';
    path++;

    if (client.connect(host, atoi(port)) == true ) {
      Serial.println(F("Connected to PUSH channel"));
    } else {
      Serial.println(F("Failed to connect to PUSH channel"));
      return;
    }

    // Debug: show the amount of free SRAM
    Serial.print(F("PUSH: free RAM: "));
    Serial.println(freeRam());

    // HTTP header, note the notification headers
    client.print(F("POST "));
    client.print(path);
    client.print(F(" HTTP/1.1\r\n"));

    client.print(F("Host: "));
    client.print(ip[0]);
    client.print(F("."));
    client.print(ip[1]);
    client.print(F("."));
    client.print(ip[2]);
    client.print(F("."));
    client.print(ip[3]);

    client.print(F("\r\nUser-Agent: Arduino/1.0\r\n"));
    client.print(F("Content-Type: text/xml\r\n"));
    client.print(F("Connection: close\r\n"));

    switch (notificationType)
    {
      case 1: // Tile notification
        client.print(F("X-WindowsPhone-Target: token\r\n"));
        client.print(F("X-NotificationClass: 1\r\n"));
        break;
      case 2: // Toast notification
        client.print(F("X-WindowsPhone-Target: toast\r\n"));
        client.print(F("X-NotificationClass: 2\r\n"));
        break;
      case 3: // Raw notification
        client.print(F("X-NotificationClass: 3\r\n"));
        break;
    }
    client.print(F("Content-Length: "));
    if (payload2 == NULL) {
      client.print(strlen(payload1) + 1);
    } else {
     client.print(strlen(payload1) + strlen_P(payload2) + 2);
    }
    client.print("\r\n\r\n");

    // The actual payload
    client.print(payload1);

    // The extra payload from PROGMEM
    if (payload2 != NULL)
    {
      client.print(",");

      byte character;
      while ( (character = pgm_read_byte(payload2++)) != 0)
      {
        client.write(character);
      }
    }

    client.print("}"); // payload1 does not end with '}'

    Serial.println(F("Payload sent:\n---"));
    Serial.println(payload1);

    // Print out the HTTP request response
    Serial.println(F("Response:\n---"));
    while (client.connected()) {
      while (client.available()) {
        char c = client.read();
        Serial.print(c);
      }
    }

    client.stop();

    Serial.println(F("---\ndisconnected"));
  }
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
  snprintf(macstr, 18, "%02X:%02X:%02X:%02X:%02X:%02X", macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);

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


//
// The most important thing of all, feed the watchdog
//
void feedWatchdog()
{
  wdt_reset();
}
