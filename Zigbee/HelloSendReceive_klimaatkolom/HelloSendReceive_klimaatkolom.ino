// Copyright 2015, Matthijs Kooijman <matthijs@stdin.nl>
//
// Permission is hereby granted, free of charge, to anyone
// obtaining a copy of this document and accompanying files, to do
// whatever they want with them without any restriction, including, but
// not limited to, copying, modification and redistribution.
//
// NO WARRANTY OF ANY KIND IS PROVIDED.
//
// This example reads values from a DHT22 sensor every 10 seconds and
// sends them to the coordinator XBee, to be read by Coordinator.ino.

#include <XBee.h>
#include <Printers.h>
#include <AltSoftSerial.h>
#include "Adafruit_SHT31.h"
#include "binary.h"
#include "SparkFun_SCD30_Arduino_Library.h" 

XBeeWithCallbacks xbee;
Adafruit_SHT31 SHT31_a = Adafruit_SHT31();
Adafruit_SHT31 SHT31_b = Adafruit_SHT31();

SCD30 airSensor;

AltSoftSerial SoftSerial;

#define DebugSerial Serial
#define XBeeSerial SoftSerial
#define servoPin 5 
#define fanPin 6


void setup() {
  // Setup debug serial output
  DebugSerial.begin(115200);

  // Setup sensors
  airSensor.begin();
  SHT31_a.begin(0x44);
  SHT31_b.begin(0x45);
  Wire.begin();
    
  // Setup XBee serial communication
  XBeeSerial.begin(9600);
  xbee.begin(XBeeSerial);
  delay(1);

  // Setup callbacks
  xbee.onPacketError(printErrorCb, (uintptr_t)(Print*)&DebugSerial);
  xbee.onResponse(printErrorCb, (uintptr_t)(Print*)&DebugSerial);

  // Send a first packet right away
  sendPacket();

  Serial.println("Alcomij System Version 1.0 Started...");
}




//=========================XBEE ROUTINES=========================================================



void sendPacket() {
    // Prepare the Zigbee Transmit Request API packet
    ZBTxRequest txRequest;
    txRequest.setAddress64(0x0000000000000000);

    // Allocate 27 payload bytes: 1 type byte + 3x4 floats + 3x3 floats + 1 floats of 5 bytes
    AllocBuffer<27> packet;

    //Read sensors
    float a_Temp = SHT31_a.readTemperature();
    float b_Temp = SHT31_b.readTemperature();
    float a_Hum = SHT31_a.readHumidity();
    float b_Hum = SHT31_b.readHumidity();
    float c_Temp = airSensor.getTemperature();   
    float c_Hum = airSensor.getHumidity();
    float d_CO2 = airSensor.getCO2();

    // Packet type
    packet.append<uint8_t>(1);
    packet.append<float>(a_Temp);
    packet.append<float>(b_Temp);
    packet.append<float>(c_Temp);
    packet.append<float>(a_Hum);
    packet.append<float>(b_Hum);
    packet.append<float>(c_Hum);  
    packet.append<float>(d_CO2);
    txRequest.setPayload(packet.head, packet.len());

    // And send it
    xbee.send(txRequest);
    DebugSerial.println(F("Packet send"));
}

void processRxPacket(ZBRxResponse& rx, uintptr_t) {
  Buffer b(rx.getData(), rx.getDataLength());
  uint8_t type = b.remove<uint8_t>();

  if (type == 1 && b.len() == 4) {
    DebugSerial.print(F("Packet received from: "));
    printHex(DebugSerial, rx.getRemoteAddress64());
    DebugSerial.println();
    DebugSerial.print(F("Fan Speed: "));
    DebugSerial.println(b.remove<float>());
    DebugSerial.print(F("Klep stand: "));
    DebugSerial.println(b.remove<float>());
    return;
  }
}

//=========================XBEE ROUTINES=========================================================

unsigned long last_tx_time = 0;

void loop() {
  // Check the serial port to see if there is a new packet available
  xbee.loop();

  // Send a packet every 10 seconds
  if (millis() - last_tx_time > 10000) {
    sendPacket();
    last_tx_time = millis();
  }
}
