

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
#include <RingBuffer.h>

enum WorkMode {
  MIN_MODE = -1,
  MODE_MANUAL,
  MODE_DEHUMIDIFY,
  MODE_MEASURE,
  MODE_SETPOINT_TEMP,
  MODE_SETPOINT_ABS_HUM, 
  MAX_MODE
}; 

XBeeWithCallbacks xbee;

AltSoftSerial SoftSerial;

Adafruit_SHT31 SHT31_a = Adafruit_SHT31();
Adafruit_SHT31 SHT31_b = Adafruit_SHT31();
SCD30 SCD30_a;
long pulses = 0;
long previousCalculationMs;

uint8_t fanPwm = 1;

WorkMode workMode = MODE_MANUAL;

struct sendData { char type; int messageId; byte value; };
RingBuffer<sendData, 8> outboxBuffer;

float temperatureSetpoint = NAN;
float absoluteHumiditySetpoint = NAN; 

struct sendData { char type; int messageId; byte value; };
RingBuffer<sendData, 8> outboxBuffer;

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

void queueResponse( char type, long messageId, uint8_t value) {
  if( outboxBuffer.capacity() == outboxBuffer.size())
      DebugSerial.println(F("response buffer full, overwriting oldest entry"));
    
  for(size_t i = 0; i < outboxBuffer.size(); ++i)
  {
    if( outboxBuffer[i].messageId = messageId) {
      DebugSerial.println(F("ignoring duplicate message"));
      return;
    }
  }
  sendData response;
  response.type = type;
  response.messageId = messageId;
  response.value = value; 
  outboxBuffer.push(response);  
  
  DebugSerial.print(F("pushed message "));
  DebugSerial.print(messageId);
  DebugSerial.print(F(", buffer size "));
  DebugSerial.println(outboxBuffer.size());
}

void trySendFromOutbox() {
  if( outboxBuffer.empty()){
    return;
  }
  sendData response = outboxBuffer.front();
  if(sendResponse(response.type, response.messageId, response.value));
    outboxBuffer.pop();  
}

bool sendResponse( char type, long messageId, uint8_t value) {
    ZBTxRequest txRequest;
    txRequest.setAddress16(0x0000/* coordinator */);
    AllocBuffer<24> packet;
    packet.append<uint8_t>(type);
    packet.append<long>(messageId);
    packet.append<uint8_t>(value);    
    txRequest.setPayload(packet.head, packet.len());    
    DebugSerial.print(F("Sending ")); 
    DebugSerial.print(type);
    DebugSerial.println(packet.len());
    // And send it
    uint8_t status = xbee.sendAndWait(txRequest, 2000);
    if (status == 0) {
      DebugSerial.println(F("Succesfully sent packet"));
      return true;
    } else {
      DebugSerial.print(F("Failed to send packet. Status: 0x"));
      DebugSerial.println(status, HEX);
      return false;
    }
}

bool sendSetpointResponse( char type, char subtype, long messageId, float value) {
    ZBTxRequest txRequest;
    txRequest.setAddress16(0x0000/* coordinator */);
    AllocBuffer<24> packet;
    packet.append<uint8_t>(type);
    packet.append<uint8_t>(subtype);
    packet.append<long>(messageId);
    packet.append<float>(value);    
    txRequest.setPayload(packet.head, packet.len());    
    DebugSerial.print(F("Sending ")); 
    DebugSerial.print(type);
    DebugSerial.print(subtype);
    DebugSerial.println(packet.len());
    // And send it
    uint8_t status = xbee.sendAndWait(txRequest, 2000);
    if (status == 0) {
      DebugSerial.println(F("Succesfully sent packet"));
      return true;
    } else {
      DebugSerial.print(F("Failed to send packet. Status: 0x"));
      DebugSerial.println(status, HEX);
      return false;
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
    txRequest.setAddress16(0x0000/* coordinator */);

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
    uint8_t status = xbee.sendAndWait(txRequest, 2000);
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
        if( work > MIN_MODE && work < MAX_MODE)
        {
           workMode = work; 
        } else {            
           DebugSerial.print(F("Ignoring unknown work mode"));     
        }
    } else if (type == 'S' ) {
        char subtype = b.remove<char>();
        DebugSerial.print(F("Change setpoint:"));
        DebugSerial.println(subtype); 
        if( subtype == 'T' )
        {
           temperatureSetpoint = b.remove<float>(); 
        } else if( subtype == 'A')
        {
           absoluteHumiditySetpoint = b.remove<float>(); 
        } else {            
           DebugSerial.print(F("Ignoring unknown setpoint"));     
        }
    } else if (type == 'f' ) { // fan pwm setting request
        long messageId = b.remove<long>();
        queueResponse(type, messageId, fanPwm);        
    } else if (type == 'v' ) { // valve setting request
        long messageId = b.remove<long>();
        queueResponse(type, messageId, valvePercentage);        
    } else if (type == 'w' ) { // workmode setting request
        long messageId = b.remove<long>();
        queueResponse(type, messageId, workMode);        
    } else if (type == 's' ) { // setpoint setting request
        char subtype = b.remove<char>();
        long messageId = b.remove<long>();
        float setpoint;
        if( subtype == 'T' )
        {
          setpoint = temperatureSetpoint; 
        } else if( subtype == 'A')
        {
          setpoint = absoluteHumiditySetpoint;
        } else {            
          DebugSerial.print(F("Ignoring unknown setpoint"));     
          DebugSerial.print(subtype);     
          setpoint = NAN;
        }
        sendSetpointResponse(type, subtype, messageId, setpoint);        
    }
}

unsigned long last_tx_time = 0;

void loop() {
  // Check the serial port to see if there is a new packet available
  xbee.loop();
  trySendFromOutbox();
  // log a mark every 10 seconds
  if (millis() - last_tx_time > 10000) {
    last_tx_time = millis();
    DebugSerial.print(F("Mem: "));
    DebugSerial.println(freeMemory());
    sendPacket();
  }
  switch( workMode)
  {
    case MODE_MANUAL:
      /*do nothing*/
    break;
    case MODE_DEHUMIDIFY:
      loop_dehumidify();
      break;
    case MODE_MEASURE:
      loop_measure();
      break;
    case MODE_SETPOINT_TEMP:
      loop_setpoint_temperature();
      break;
    case MODE_SETPOINT_ABS_HUM:
      loop_setpoint_absoluteHumidity();
      break;
    default:
      DebugSerial.println("unknown workmode");
  }
   
  analogWrite(FAN_PIN, fanPwm);
}

long last_dehumidify_time = 0;

void loop_dehumidify() {
  if (millis() - last_dehumidify_time < 10000) return;
  last_dehumidify_time = millis();
  
  float   hum_1 = get_absolute_humidity(SHT31_a.readTemperature(), SHT31_a.readHumidity()); //up
  DebugSerial.print ("humidity 1:");
  DebugSerial.println (hum_1);
  float   hum_2 = get_absolute_humidity(SCD30_a.getTemperature(), SCD30_a.getHumidity()); //middle
  DebugSerial.print ("humidity 2:");
  DebugSerial.println (hum_2);
  float   hum_3 = get_absolute_humidity(SHT31_b.readTemperature(), SHT31_b.readHumidity()); //down
  DebugSerial.print ("humidity 3:");
  DebugSerial.println (hum_3);

  if( hum_2 < hum_1 && hum_2 < hum_3 ) // middle is driest, so switch off fan
  {    
    DebugSerial.println ("Action: off");    
    fanPwm = 1;    
    setServoPercent(50);     
  } else {
    fanPwm = 50;  
    if( hum_1 < hum_3)
    {        
      DebugSerial.println ("Action: air from up");    
      setServoPercent(0);     
    } else {
      DebugSerial.println ("Action: air from down");    
      setServoPercent(100);
    }
  }    
}


long last_setpoint_time = 0;
void loop_setpoint_temperature() {
  if (millis() - last_setpoint_time < 10000) return;
  last_setpoint_time = millis();
  
  float temp_1 = SHT31_a.readTemperature(); //up
  float temp_2 = SCD30_a.getTemperature(); //middle
  float temp_3 = SHT31_b.readTemperature(); //down
  do_control_loop(temp_1, temp_2, temp_3, temperatureSetpoint);
}

void loop_setpoint_absoluteHumidity() {
  if (millis() - last_setpoint_time < 10000) return;
  last_setpoint_time = millis();
  
  float   hum_1 = get_absolute_humidity(SHT31_a.readTemperature(), SHT31_a.readHumidity()); //up 
  float   hum_2 = get_absolute_humidity(SCD30_a.getTemperature(), SCD30_a.getHumidity()); //middle 
  float   hum_3 = get_absolute_humidity(SHT31_b.readTemperature(), SHT31_b.readHumidity()); //down
  do_control_loop(hum_1, hum_2, hum_3, absoluteHumiditySetpoint);
}

void do_control_loop(float v1, float v2, float v3, float setpoint)
{
  if( temperatureSetpoint == NAN )
  {
    DebugSerial.println(F("No setpoint"));
    return;
  }
  if( abs(v2 - setpoint) < 0.5) // on setpooint, so switch off fan
  {    
    DebugSerial.println (F("Action: off, on setpoint"));    
    fanPwm = 1;    
    setServoPercent(50);     
  } else {
    fanPwm = 50;  
    bool need_more_heat = v2 < setpoint;
    if( (need_more_heat && v1 > v2 && v1 > v3 ) ||
       (!need_more_heat && v1 < v2 && v1 < v3 ))
    {        
      DebugSerial.println(F("Action: air from up"));    
      setServoPercent(0);     
    } else if ((need_more_heat && v3 > v2 && v3 > v1 ) ||
              (!need_more_heat && v3 < v2 && v3 < v1 ))
    {
      DebugSerial.println(F("Action: air from down"));    
      setServoPercent(100);
    } else {
      DebugSerial.println(F("Action: off, no suitable climate"));    
      fanPwm = 1;    
      setServoPercent(50);         
    }
  }    
}

long last_measure_time = 0;

enum measure_step { 
  off1,
  off2,
  off3,
  off4,
  off5,
  off6,
  off7,
  off8,
  off9,
  off10,
  off11,
  off12,
  slow, 
  medium, 
  fast 
  };

measure_step state = off1;

void loop_measure() {
  if (millis() - last_dehumidify_time < 60000) return;
  last_dehumidify_time = millis();
  DebugSerial.print(F("Measure: state "));
  DebugSerial.println(state);
    
  switch( state ) {
    case slow:
      setServoPercent(50);  
      fanPwm = 1; 
      state = (int)state + 1;
      break;
    case medium:
      setServoPercent(50);  
      fanPwm = 50;  
      state = (int)state + 1;
      break;
    case fast:
      setServoPercent(50);  
      fanPwm = 255;   
      state = off1;
      break;
    default:
      setServoPercent(100);  
      fanPwm = 1;   
      state = (int)state + 1;
  }
}

#define E 2.718281

float get_absolute_humidity(float temperature, float humidity) {
  return (6.112* pow(E, (17.67* temperature) / (temperature + 243.5)) * humidity * 2.1674) /(273.15 + temperature);
}
