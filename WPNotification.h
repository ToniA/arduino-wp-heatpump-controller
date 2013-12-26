#ifndef WPNotification_h
#define WPNotification_h

#include <Arduino.h>
#include <aJSON.h>
#include <Ethernet.h>

class WPNotification
{
  public:
    WPNotification();
    void sendNotification(EthernetUDP &udpSocket, aJsonObject *pushurl_channel, char *payload1, const prog_char *payload2, int notificationType);
};

#endif

