#include "WPNotification.h"

WPNotification::WPNotification()
{
}

void WPNotification::sendNotification(EthernetUDP &udpSocket, aJsonObject *pushurl_channel, char *payload1, const prog_char *payload2, int notificationType)
{
  EthernetClient client;
  
  if (pushurl_channel == NULL)
  {
    Serial.print(F("Sending a UDP response: "));
    Serial.println(payload1);

    udpSocket.beginPacket(udpSocket.remoteIP(), udpSocket.remotePort());

    // The actual payload
    udpSocket.write(payload1);

    // The extra payload from PROGMEM
    if (payload2 != NULL)
    {
      udpSocket.write(",");

      byte character;
      while ( (character = pgm_read_byte(payload2++)) != 0)
      {
        udpSocket.write(character);
      }
    }

    udpSocket.write("}"); // payload1 does not end with '}'
    udpSocket.endPacket();
  }
  else
  {
    // The channel is in form 'host:port:path'
    // Break it down in place to save precious RAM
    char *host = pushurl_channel->valuestring;
    char *port = strchr(host, ':');
    port[0] = '\0';
    port++;
    char *path = strchr(port, ':');
    path[0] = '\0';
    path++;

    if (client.connect(host, atoi(port)) != true ) {
      Serial.println(F("Failed to connect to PUSH channel"));
      return;
    }

    // HTTP header, note the notification headers
    client.print(F("POST "));
    client.print(path);
    client.print(F(" HTTP/1.1\r\n"));

    client.print(F("Host: "));
    client.print(Ethernet.localIP());
    client.print(F("\r\nUser-Agent: Arduino/1.0\r\n"));
    client.print(F("Content-Type: text/xml\r\n"));
    client.print(F("Connection: close\r\n"));
    
    // Only 'tile' or 'toast' notifications use the 'X-WindowsPhone-Target' header
    if (notificationType < 3)
    {
      client.print(F("X-WindowsPhone-Target: "));
      
      switch (notificationType)
      {
        case 1: // Tile notification
          client.print(F("token\r\n"));
          break;
        case 2: // Toast notification
          client.print(F("toast\r\n"));
          break;
      }
    }

    client.print(F("X-NotificationClass: "));
    client.print(notificationType);
    client.print(F("\r\n"));  

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
  }
}
