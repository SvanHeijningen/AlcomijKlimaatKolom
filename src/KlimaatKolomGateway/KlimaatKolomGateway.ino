// Copyright 2015, Matthijs Kooijman <matthijs@stdin.nl>
//
// Permission is hereby granted, free of charge, to anyone
// obtaining a copy of this document and accompanying files, to do
// whatever they want with them without any restriction, including, but
// not limited to, copying, modification and redistribution.
//
// NO WARRANTY OF ANY KIND IS PROVIDED.
//
//
// This example prints any received ZigBee radio packets to serial.
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <XBee.h>
#include <Printers.h>
#include <AltSoftSerial.h>
#include "binary.h"

XBeeWithCallbacks xbee;

AltSoftSerial SoftSerial;
#define DebugSerial Serial
#define XBeeSerial SoftSerial


#define BBT "mqtt.beebotte.com"     // Domain name of Beebotte MQTT service
#define TOKEN "token:token_7PdJmhz4OAi8Lrd6"    // Set your channel token here

#define CHANNEL "Arduinno"          // Replace with your device name

#define TEMP_RESOURCE "temperature" // This is where we will store temperature
#define HUMID_RESOURCE "humidity"   // This is where we will store humidity
#define WRITE true // indicate if published data should be persisted or not

// Enter a MAC address of your shield
// It might be printed on a sticker on the shield
byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x3E, 0xAB };

IPAddress ip(192,168,2,22); //192.168.2.22
EthernetClient ethClient;
PubSubClient client(ethClient);

void setup() {
  // Setup debug serial output
  DebugSerial.begin(115200);
  DebugSerial.println(F("Starting..."));

  // Setup XBee serial communication
  XBeeSerial.begin(9600);
  xbee.begin(XBeeSerial);
  delay(1);

  // Setup callbacks
  xbee.onZBRxResponse(processRxPacket);
}

void processRxPacket(ZBRxResponse& rx, uintptr_t) {
    XBeeAddress64 remoteAddress = rx.getRemoteAddress64();
    DebugSerial.print(F("Received packet from "));
    
    printHex(DebugSerial, remoteAddress );
    DebugSerial.println();
    DebugSerial.print(F("Payload size: "));
    DebugSerial.print(rx.getDataLength());
    DebugSerial.println();
    Buffer b = Buffer(rx.getData(), rx.getDataLength());
    uint8_t type = b.remove<uint8_t>();
    if (type == 'A' ) {
        float his_temp = b.remove<float>();
        float his_humidity = b.remove<float>();
        DebugSerial.print(F("His temperature:"));
        DebugSerial.println(his_temp);
        DebugSerial.print(F("His humidity:"));
        DebugSerial.println(his_humidity);

        char mac[16];
        byte* addr = (byte*)&remoteAddress;
        sprintf(mac, "%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X", addr[3], addr[2], addr[1], addr[0], addr[7], addr[6], addr[5], addr[4]);

        publish(TEMP_RESOURCE, mac,  his_temp, WRITE);
        publish(HUMID_RESOURCE, mac, his_humidity, WRITE);
    }
}

// publishes data to the specified resource
void publish(const char* resource, const char* mac, float data, bool persist)
{
  StaticJsonBuffer<128> jsonOutBuffer;
  JsonObject& root = jsonOutBuffer.createObject();
  root["channel"] = CHANNEL;
  root["resource"] = resource;
  if (persist) {
    root["write"] = true;
  }
  root["data"] = data;

  // Now print the JSON into a char buffer
  char buffer[128];
  root.printTo(buffer, sizeof(buffer));
  
  // Create the topic to publish to
  char topic[64];
  sprintf(topic, "%s/%s/%s", CHANNEL, mac, resource);

  DebugSerial.println("Sending to topic:");
  DebugSerial.println(topic);
  DebugSerial.println(buffer);

  // Now publish the char buffer to Beebotte
  client.publish(topic, buffer);
}

unsigned long last_tx_time = 0;

void loop() {
  // Check the serial port to see if there is a new packet available
  xbee.loop();
  // log a mark every 10 seconds
  if (millis() - last_tx_time > 10000) {
    last_tx_time = millis();
    DebugSerial.println("Still here");
  }
}
