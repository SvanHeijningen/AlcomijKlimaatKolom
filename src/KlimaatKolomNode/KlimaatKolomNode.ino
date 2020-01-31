

// Copyright 2020 Sven and Koen from Alcomij
// Hat tip to Matthijs Kooijman <matthijs@stdin.nl>
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


#include <XBee.h>
#include <Printers.h>
#include <AltSoftSerial.h>
#include "Adafruit_SHT31.h"
#include "binary.h"
#include <SparkFun_SCD30_Arduino_Library.h>
#include <SoftPWM.h>
#include <SoftPWM_timer.h>

XBeeWithCallbacks xbee;

AltSoftSerial SoftSerial;
#define DebugSerial Serial
#define XBeeSerial SoftSerial

#define FAN_PIN 5
#define VALVE_SERVO_PIN 6

Adafruit_SHT31 SHT31_a = Adafruit_SHT31();
Adafruit_SHT31 SHT31_b = Adafruit_SHT31();
SCD30 SCD30_a;

void setup() {
  // Setup debug serial output
  DebugSerial.begin(115200);
  DebugSerial.println(F("Starting..."));


  SoftPWMBegin();
    
  SHT31_a.begin(0x44);
  SHT31_a.begin(0x45);
  if (SCD30_a.begin() == false)
  {
    Serial.println("Air sensor not detected. Please check wiring. Freezing...");
    while (1)
      ;
  }
  // Setup XBee serial communication
  XBeeSerial.begin(9600);
  xbee.begin(XBeeSerial);
  delay(1);

  // Setup callbacks
  xbee.onZBRxResponse(processRxPacket);
}


void sendPacket() {
      
    // Prepare the Zigbee Transmit Request API packet
    ZBTxRequest txRequest;
    txRequest.setAddress64(0x0000000000FFFF);

    AllocBuffer<27> packet;
    packet.append<uint8_t>('A');
    packet.append<float>(SHT31_a.readTemperature());
    packet.append<float>(SHT31_a.readHumidity());
    packet.append<float>(SHT31_b.readTemperature());
    packet.append<float>(SHT31_b.readHumidity());
    packet.append<float>(SCD30_a.getTemperature());
    packet.append<float>(SCD30_a.getHumidity());
    packet.append<float>(SCD30_a.getCO2());
    
    txRequest.setPayload(packet.head, packet.len());
    
    DebugSerial.print(F("Sending "));
    DebugSerial.println(packet.len());
    // And send it
    uint8_t status = xbee.sendAndWait(txRequest, 5000);
    if (status == 0) {
      DebugSerial.println(F("Succesfully sent packet"));
    } else {
      DebugSerial.print(F("Failed to send packet. Status: 0x"));
      DebugSerial.println(status, HEX);
    }
}

void processRxPacket(ZBRxResponse& rx, uintptr_t) {
    DebugSerial.print(F("Received packet from "));
    printHex(DebugSerial, rx.getRemoteAddress64());
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
    } else if (type == 'F' ) {
        uint8_t pwm = b.remove<uint8_t>();
        DebugSerial.print(F("Desired Fan PWM:"));
        DebugSerial.println(pwm);     
        analogWrite(FAN_PIN, pwm);
    } else if (type == 'V' ) {
        uint8_t pwm = b.remove<uint8_t>();
        DebugSerial.print(F("Desired Valve servo position:"));
        DebugSerial.println(pwm); 
        // make it servo safe
        if( pwm < 5 )
          pwm = 5;
       if( pwm > 33)
          pwm = 33;        
        SoftPWMSet(VALVE_SERVO_PIN, pwm);
    }
}

unsigned long last_tx_time = 0;

void loop() {
  // Check the serial port to see if there is a new packet available
  xbee.loop();
  // log a mark every 10 seconds
  if (millis() - last_tx_time > 10000) {
    last_tx_time = millis();
    DebugSerial.println("Still here");
    sendPacket();
  }
}
