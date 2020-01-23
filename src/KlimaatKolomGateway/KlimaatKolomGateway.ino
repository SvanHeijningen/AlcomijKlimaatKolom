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

IPAddress ip(10,2,0,121); //192,168,2,22 //10,2,0,23 //10.2.0.121

EthernetClient ethClient;
PubSubClient client(ethClient);

// to track delay since last reconnection
unsigned long lastReconnectAttempt = 0;
unsigned long last_tx_time = 0;

char id[8]; // Identifier for the MQTT connection - will set it randomly
const char * generateID()
{ 
  char chars[] = "abcdefghijklmnopqrstuvwxyz";
  randomSeed(analogRead(0));
  int i = 0;
  for(i = 0; i < sizeof(id) - 1; i++) {
    id[i] = chars[random(sizeof(chars))];
  }
  id[sizeof(id) -1] = '\0';

  return id;
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

        if (!client.connected()) {
          DebugSerial.print(F("Not connected"));
          return;
        }
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
  
  char resource_with_mac[32];
  if( mac != NULL)
    sprintf(resource_with_mac, "%s_%s", mac, resource);
  else 
    sprintf(resource_with_mac, "%s", resource);
    
  StaticJsonBuffer<100> jsonOutBuffer;
  JsonObject& root = jsonOutBuffer.createObject();
  root["channel"] = CHANNEL;
  root["resource"] = resource_with_mac;
  if (persist) {
    root["write"] = true;
  }
  root["data"] = data;

  // Now print the JSON into a char buffer
  char buffer[100];
  root.printTo(buffer, sizeof(buffer));
  
  // Create the topic to publish to
  char topic[35];
  sprintf(topic, "%s/%s", CHANNEL, resource_with_mac);

  DebugSerial.println(F("Sending to topic:"));
  DebugSerial.println(topic);
  DebugSerial.println(buffer);

  // Now publish the char buffer to Beebotte
  client.publish(topic, buffer);
}


// reconnects to Beebotte MQTT server
boolean reconnect() {
  if (client.connect(generateID(), TOKEN, "")) {
    Serial.println(F("Connected to Beebotte MQTT"));
  }
  return client.connected();
}


void setup() {
  // Setup debug serial output
  DebugSerial.begin(115200);
  DebugSerial.println(F("Starting..."));

  // Setup XBee serial communication
  XBeeSerial.begin(9600);
  xbee.begin(XBeeSerial);
  
  client.setServer(BBT, 1883);

  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // try to congifure using static IP address instead of DHCP:
    // Feel free to change this according to your network settings
    Ethernet.begin(mac, ip);
  }

  // give the Ethernet shield a second to initialize:
  delay(1000);

  Serial.println("Connecting...");
  lastReconnectAttempt = 0;
  
  // Setup callbacks
  xbee.onZBRxResponse(processRxPacket);
}

void loop() {
  // Check the serial port to see if there is a new packet available
  xbee.loop();
  // log a mark every 10 seconds
  if (millis() - last_tx_time > 10000) {
    last_tx_time = millis();
    DebugSerial.println(F("Still here"));
    if( client.connected()) {
        client.publish("temp", NULL, 12.3, true);
    }
  }
  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    client.loop();
  }  
}
