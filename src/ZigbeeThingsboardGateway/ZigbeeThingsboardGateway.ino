#include <NtpClientLib.h>

#include <Printers.h>
#include <XBee.h>
#include "ThingsBoard.h"
#include <SPI.h>
#include <Ethernet.h>

#include "PubSubClient.h"

#include "binary.h"

// Helper macro to calculate array size
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

// Update these with values suitable for your network.
byte mac[]    = { 0xA8, 0x61, 0x0A, 0xAE, 0x3E, 0xAB };

char thingsboardServer[] = "10.2.0.122";
 
#define TOKEN "uWEc3OmSYGeKhWZcrT7W" // ThingsBoard Device Auth Token

int lastSend = 0;
int lastMaintain = 0;
int pwm = 0;

bool subscribed = false;

EthernetClient ethClient;
ThingsBoardSized<MQTT_MAX_PACKET_SIZE> tb(ethClient);

#define XBeeSerial Serial1

XBeeWithCallbacks xbee;

void setup()
{
  Serial.begin(115200);
    while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("Hello");

  XBeeSerial.begin(9600);
  xbee.begin(XBeeSerial);
  // Setup callbacks
  xbee.onZBRxResponse(processRxPacket);
  
  Ethernet.begin(mac);
  // Allow the hardware to sort itself out
  delay(1500);
  NTP.begin (); // Only statement needed to start NTP sync.
}

void loop()
{  
  int timestamp = millis();
  if ( timestamp - lastSend > 10000 ) { // Keepalive
    Serial.print(F("Mem: "));
    Serial.println(freeMemory());
    lastSend = timestamp;
  } 
  
  if ( !tb.connected() ) {
    reconnect();
  }  
  tb.loop();
  Ethernet.maintain();
  xbee.loop();
}

// Processes function for RPC call "getValue"
// RPC_Data is a JSON variant, that can be queried using operator[]
// See https://arduinojson.org/v5/api/jsonvariant/subscript/ for more details
RPC_Response processGetValue(const RPC_Data &data)
{
  Serial.println("Received the get value RPC method");
 
  return RPC_Response(NULL, pwm);
}

// Processes function for RPC call "setGpioStatus"
// RPC_Data is a JSON variant, that can be queried using operator[]
// See https://arduinojson.org/v5/api/jsonvariant/subscript/ for more details
RPC_Response processSetValue(const RPC_Data &data)
{
  Serial.println("Received the set value RPC method");

  int newpwm = data;
  Serial.print("new pwm:");
  Serial.println(newpwm);
  if( newpwm < 0 || newpwm > 255)
  {
    Serial.println("Invalid pwm");
  } else {
    pwm = newpwm;    
    analogWrite(6, pwm);
  }
  return RPC_Response(NULL, pwm);
}

RPC_Response processSetNodeFanPWM(const RPC_Data &data)
{  
  return setNodePWM('F', data);
}

RPC_Response processSetNodeValvePWM(const RPC_Data &data)
{
  return setNodePWM('V', data);
}

RPC_Response setNodePWM(const char messagetype, const RPC_Data &data)
{  
  Serial.println("Received the set value RPC method");

  const char *deviceName = data["device"];
  Serial.print("for");
  Serial.println(deviceName);
  uint8_t newpwm = data["data"]["params"];
  Serial.print("new pwm:");
  Serial.println(newpwm);

  // Prepare the Zigbee Transmit Request API packet
  ZBTxRequest txRequest;
  String device = String(deviceName);
  long addressMsb = strtol(device.substring(2,10).c_str(), NULL, 16);
  long addressLsb = strtol(device.substring(10).c_str(), NULL, 16);
  XBeeAddress64 destination = XBeeAddress64(addressMsb, addressLsb);
  
  Serial.print("Sending to ");
  printHex(Serial, destination );
  Serial.println();
  
  txRequest.setAddress64(destination);

  AllocBuffer<27> packet;
  packet.append<uint8_t>(messagetype);
  packet.append<uint8_t>(newpwm);
  txRequest.setPayload(packet.head, packet.len());
  
  Serial.print(F("Sending "));
  Serial.println(packet.len());
  // And send it
  uint8_t status = xbee.sendAndWait(txRequest, 5000);
  if (status == 0) {
    Serial.println(F("Succesfully sent packet"));
  } else {
    Serial.print(F("Failed to send packet. Status: 0x"));
    Serial.println(status, HEX);
  }
    
  return RPC_Response(NULL, pwm);
}

RPC_Response requestGetNodeValue(const char messagetype, const RPC_Data &data)
{  
  Serial.println("Received the get value RPC method");
  
  const char *deviceName = data["device"];
  Serial.print("for");
  Serial.println(deviceName);
  long messageId = data["data"]["id"];
  Serial.print("requestId:");
  Serial.println(messageId);

  // Prepare the Zigbee Transmit Request API packet
  ZBTxRequest txRequest;
  String device = String(deviceName);
  long addressMsb = strtol(device.substring(2,10).c_str(), NULL, 16);
  long addressLsb = strtol(device.substring(10).c_str(), NULL, 16);
  XBeeAddress64 destination = XBeeAddress64(addressMsb, addressLsb);
  
  Serial.print("Sending to ");
  printHex(Serial, destination );
  Serial.println();
  
  txRequest.setAddress64(destination);

  AllocBuffer<27> packet;
  packet.append<uint8_t>(messagetype);
  if(! packet.append<long>(messageId))
    Serial.print(F("send buffer too small"));
  txRequest.setPayload(packet.head, packet.len());
  
  Serial.print(F("Sending "));
  Serial.println(packet.len());
  // And send it
  uint8_t status = xbee.sendAndWait(txRequest, 5000);
  if (status == 0) {
    Serial.println(F("Succesfully sent packet"));
  } else {
    Serial.print(F("Failed to send packet. Status: 0x"));
    Serial.println(status, HEX);
  }
    
  return RPC_Response(NULL, pwm);
}

RPC_Response processGetNodeFanPWM(const RPC_Data &data) 
{   
  requestGetNodeValue('f', data);
  return RPC_Response(NULL, 0);
}

RPC_Response processGetNodeValvePWM(const RPC_Data &data) 
{   
  requestGetNodeValue('v', data);
  return RPC_Response(NULL, 0);
}

RPC_Response processGetNodeWorkMode(const RPC_Data &data) 
{   
  requestGetNodeValue('w', data);
  return RPC_Response(NULL, 0);
}

RPC_Response processSetNodeWorkMode(const RPC_Data &data)
{
  return setNodePWM('W', data);
}

// RPC handlers
RPC_Callback callbacks[] = {
  { "getValue",         processGetValue },
  { "setValue",         processSetValue }, 
  { "getFanPWM",        processGetNodeFanPWM },
  { "setFanPWM",        processSetNodeFanPWM },
  { "getValvePWM",      processGetNodeValvePWM },
  { "setValvePWM",      processSetNodeValvePWM },
  { "getWorkMode",      processGetNodeWorkMode },
  { "setWorkMode",      processSetNodeWorkMode }
};

void reconnect() {
  // Loop until we're reconnected
  while (!tb.connected()) {    
    subscribed = false;
    Serial.print("Connecting to ThingsBoard node ...");
    // Attempt to connect (clientId, username, password)
    if ( tb.connect(thingsboardServer, TOKEN) ) {
      Serial.println( "[DONE]" );      
    } else {
      Serial.print( "[FAILED]" );
      Serial.println( " : retrying in 5 seconds" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
    if (!subscribed) {
      Serial.println("Subscribing for RPC...");
  
      // Perform a subscription. All consequent data processing will happen in
      // callbacks as denoted by callbacks[] array.
      if (!tb.RPC_SubscribeWithGateway(callbacks, COUNT_OF(callbacks))) {
        Serial.println("Failed to subscribe for gateway RPC");
        return;
      }  
      // register broadcast device
      tb.connectDevice("KK000000000000FFFF");
      Serial.println("Subscribe done");
      
      subscribed = true;
    }
  }
}

void processRxPacket(ZBRxResponse& rx, uintptr_t) {
    XBeeAddress64 remoteAddress = rx.getRemoteAddress64();
    Serial.print(F("Received packet from "));
    
    printHex(Serial, remoteAddress );
    Serial.println();
    Serial.print(F("Payload size: "));
    Serial.print(rx.getDataLength());
    Serial.println();
    Buffer b = Buffer(rx.getData(), rx.getDataLength());
    
    char devicename[19];
    byte* addr = (byte*)&remoteAddress;
    sprintf(devicename, "KK%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X", addr[3], addr[2], addr[1], addr[0], addr[7], addr[6], addr[5], addr[4]);

    uint8_t type = b.remove<uint8_t>();
    if (type == 'A' ) {
        float temp_1 = b.remove<float>();
        float humi_1 = b.remove<float>();        
        float temp_3 = b.remove<float>();
        float humi_3 = b.remove<float>();
        float temp_2 = b.remove<float>();
        float humi_2 = b.remove<float>();
        float co2_2  = b.remove<float>();
        float rpm    = b.remove<float>();
        byte servoQ  = b.remove<byte>();
        byte servoPercent = b.remove<byte>();
        byte workMode = b.remove<byte>();
        
        Serial.print(F("=> temp_1:")); Serial.println(temp_1); 
        Serial.print(F("=> temp_2:")); Serial.println(temp_2); 
        Serial.print(F("=> temp_3:")); Serial.println(temp_3); 
        Serial.print(F("=> co2_2: ")); Serial.println(co2_2); 
        Serial.print(F("=> rpm:   ")); Serial.println(rpm); 
        if (!tb.connected()) {
          Serial.print(F("Not connected"));
          return;
        }
        
        bool result;
        result = tb.connectDevice(devicename);
        Serial.println(result);
        const int data_items = 11;
        Telemetry data[data_items] = {
          { "temp_1", temp_1}, 
          { "humi_1", humi_1}, 
          { "temp_3", temp_3}, 
          { "humi_3", humi_3},
		  
          { "temp_2", temp_2}, 
          { "humi_2", humi_2}, 
          { "co2_2", co2_2 }, 
          { "rpm", rpm   },
          { "servoQ", servoQ },  
          { "servoPercent", servoPercent },   
          { "workMode", workMode }          
        };
        result = tb.sendTelemetryForDeviceJson(devicename, data, data_items);
        Serial.println(result);
    } else if (type == 'v' || 
               type == 'f' || 
               type == 'w') {
      long messageId = b.remove<long>();
      uint8_t value = b.remove<uint8_t>();    
      tb.sendGatewayRpcResponse(devicename, messageId, RPC_Response(NULL, value));
    }
}
