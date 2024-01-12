/*
*/

#ifndef BEENHERE
#include "SDT.h"
#endif

#ifdef NETWORK

#define PORT 1024
static EthernetUDP Udp;
bool connected=false;

bool Ethernet_init() {
  Serial.println("Configuring Ethernet using DHCP");
  if (Ethernet.begin(my_mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    } else if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    return false;
  }
  
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());

  Serial.print("Server listeneing on: ");
  Serial.println(PORT);
  Udp.begin(PORT);
  return true;
}

extern char *processCATCommand(char *catCommand);

void EthernetEvent() {
  char inputbuffer[256];
  char *outputBuffer;
  int bytesread=0;
  int packetsize=Udp.parsePacket();
  if(packetsize!=0) {
    while(bytesread!=packetsize) {
      bytesread+=Udp.read(&inputbuffer[bytesread], packetsize-bytesread);
    }
    outputBuffer=processCATCommand(inputbuffer);

    if(outputBuffer[0]!='\0') {
      Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
      Udp.write(outputBuffer,strlen(outputBuffer));
      Udp.endPacket();
    }
  }
}
#endif