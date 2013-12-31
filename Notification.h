#ifndef Notification_h
#define Notification_h

#include <Arduino.h>
#include <aJSON.h>
#include <Ethernet.h>

typedef int (*extData)(Stream *);

class Notification
{
  public:
    Notification();
    void sendUDPNotification(EthernetUDP &udpSocket, aJsonObject *pushurl_channel, char *payload1, extData payload2);
    void sendWPNotification(aJsonObject *pushurl_channel, char *payload1, extData payload2, int notificationType);
};

#endif

