

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

#define DebugSerial Serial
#define XBeeSerial SoftSerial
#define FAN_PIN 5
#define VALVE_SERVO_PIN 6

#include <XBee.h>
#include <Printers.h>
#include <AltSoftSerial.h>
#include "Adafruit_SHT31.h"
#include "binary.h"
#include <SparkFun_SCD30_Arduino_Library.h>
#include <SoftPWM.h>
#include <SoftPWM_timer.h>
#include "Servo.h"

#define MODE_MANUAL = 0;
#define MODE_DEHUMIDIFY = 1;

XBeeWithCallbacks xbee;

AltSoftSerial SoftSerial;

Adafruit_SHT31 SHT31_a = Adafruit_SHT31();
Adafruit_SHT31 SHT31_b = Adafruit_SHT31();
SCD30 SCD30_a;
long pulses = 0;
long previousCalculationMs;

uint8_t fanPwm = 1;

uint8_t workMode = MODE_MANUAL;

void setup() {
  // Setup debug serial output
  DebugSerial.begin(115200);
  DebugSerial.println(F("Starting..."));

  //Set fan speed to a safe, low value
  analogWrite(FAN_PIN, fanPwm);
    
  SHT31_a.begin(0x45);
  SHT31_b.begin(0x44);
  if (SCD30_a.begin() == false)
  {
    Serial.println("Air sensor not detected. Please check wiring.");
  }
  // Setup XBee serial communication
  XBeeSerial.begin(9600);
  xbee.begin(XBeeSerial);
  delay(1);

  // Setup callbacks
  xbee.onZBRxResponse(processRxPacket);
  
  attachInterrupt(1/*pin 3*/, onPulse, FALLING);
  attachInterrupt(1 /*pin 3*/, onPulse, FALLING);
  previousCalculationMs = millis();
 
  setupServo();  
}

void sendResponse( char type, long messageId, uint8_t value) {
    ZBTxRequest txRequest;
    txRequest.setAddress64(0x0000000000FFFF);
    AllocBuffer<24> packet;
    packet.append<uint8_t>(type);
    packet.append<long>(messageId);
    packet.append<uint8_t>(value);    
    txRequest.setPayload(packet.head, packet.len());    
    DebugSerial.print(F("Sending ")); 
    DebugSerial.print(type);
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

void sendPacket() {
      float   temp_1 = SHT31_a.readTemperature();
      float   humi_1 = SHT31_a.readHumidity();
      float   temp_3= SHT31_b.readTemperature();
      float   humi_3 = SHT31_b.readHumidity();
      float   temp_2 = SCD30_a.getTemperature();
      float   humi_2= SCD30_a.getHumidity();
      float   co2_2 = SCD30_a.getCO2();
      float   rpm = calculateRPM();       
        Serial.print(F("=> temp_1:")); Serial.println(temp_1); 
        Serial.print(F("=> temp_2:")); Serial.println(temp_2); 
        Serial.print(F("=> temp_3:")); Serial.println(temp_3); 
        Serial.print(F("=> co2_2: ")); Serial.println(co2_2);  
        Serial.print(F("=> rpm:   ")); Serial.println(rpm); 
    // Prepare the Zigbee Transmit Request API packet
    ZBTxRequest txRequest;
    txRequest.setAddress64(0x0000000000FFFF);

    AllocBuffer<64> packet;
    if( !( 
          packet.append<uint8_t>('A') &&
          packet.append<float>(temp_1) &&
          packet.append<float>(humi_1) &&
          packet.append<float>(temp_3) &&
          packet.append<float>(humi_3) &&
          packet.append<float>(temp_2) &&
          packet.append<float>(humi_2) &&
          packet.append<float>(co2_2) &&
          packet.append<float>(rpm) &&     
          packet.append<byte>(servoQuality) &&
          packet.append<byte>(valvePercentage) &&
          packet.append<uint8_t>(workMode)
          ))
      Serial.print(F("ERROR packetbuffer too small"));
    
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

void onPulse()
{
  pulses++;
}

float calculateRPM() {
  // 2 pulses per revolution (datasheet)
  float elapsedS = (millis() - previousCalculationMs)/1000.0;
  float rpm = (60 * pulses / 2) / elapsedS;
  
  previousCalculationMs = millis();
  pulses = 0;
  Serial.print("RPM: ");
  Serial.println(rpm);
  return rpm;
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
        fanPwm = b.remove<uint8_t>();
        DebugSerial.print(F("Desired Fan PWM:"));
        DebugSerial.println(fanPwm);    
    } else if (type == 'V' ) {
        uint8_t percent = b.remove<uint8_t>();
        DebugSerial.print(F("Desired Valve servo position:"));
        DebugSerial.println(percent); 
        setServoPercent(percent);        
    } else if (type == 'W' ) {
        uint8_t work = b.remove<uint8_t>();
        DebugSerial.print(F("Desired work mode:"));
        DebugSerial.println(work); 
        if( work == MODE_MANUAL ||
            work == MODE_DEHUMIDIFY )
        {
           workMode = work; 
        }        
    } else if (type == 'f' ) { // fan pwm setting request
        long messageId = b.remove<long>();
        sendResponse(type, messageId, fanPwm);        
    } else if (type == 'v' ) { // valve setting request
        long messageId = b.remove<long>();
        sendResponse(type, messageId, valvePercentage);        
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
  if( workMode == MODE_DEHUMIDIFY)
  {
    loop_dehumidify();
  }
   
  analogWrite(FAN_PIN, fanPwm);
}

void loop_dehumidify() {
  float   hum_1 = get_absolute_humidity(SHT31_a.readTemperature(), SHT31_a.readHumidity()); //up
  float   hum_2 = get_absolute_humidity(SHT31_b.readTemperature(), SHT31_b.readHumidity()); //down
  float   hum_3 = get_absolute_humidity(SCD30_a.getTemperature(), SCD30_a.getHumidity()); //middle

  if( hum_2 < hum_1 && hum_2 < hum_3 )
      fanPwm = 1;
  else {
    fanPwm = 255;  
    if( hum_1 < hum_3)
    {        
      setServoPercent(0);     
    } else {
      setServoPercent(100);
    }
  }    
}

float get_absolute_humidity(float temp, float hum) {
  return 0.0;
}
