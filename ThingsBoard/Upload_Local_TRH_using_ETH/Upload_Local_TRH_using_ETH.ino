#include <Printers.h>
#include <XBee.h>

#include <Adafruit_SHT31.h>

#include <ThingsBoard.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include "binary.h"

// Helper macro to calculate array size
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

// Update these with values suitable for your network.
byte mac[]    = {  0xA8, 0x61, 0x0A, 0xAE, 0x3E, 0xAB };

char thingsboardServer[] = "10.2.0.122";
 
#define TOKEN "IbG1vPQ4Y1r2hd8HYeo8" // ThingsBoard Device Auth Token

int lastSend = 0;
int lastMaintain = 0;
int pwm = 0;

bool subscribed = false;

EthernetClient ethClient;
ThingsBoard tb(ethClient);

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
  
  Wire.setClock(400000);
  
  Ethernet.begin(mac);
  // Allow the hardware to sort itself out
  delay(1500);
}

void loop()
{  
  int timestamp = millis();
  if ( timestamp - lastSend > 10000 ) { // Keepalive
    Serial.println( ".");
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
/*
  int pin = data["pin"];
  int pwm = data["pwm"];
  if( pin
  */
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

// RPC handlers
RPC_Callback callbacks[] = {
  { "setValue",         processSetValue },
  { "getValue",         processGetValue },
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
      if (!tb.RPC_Subscribe(callbacks, COUNT_OF(callbacks))) {
        Serial.println("Failed to subscribe for RPC");
        return;
      }
  
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
    uint8_t type = b.remove<uint8_t>();
    if (type == 'A' ) {
        float temperature = b.remove<float>();
        float humidity = b.remove<float>();
        Serial.print(F("His temperature:"));
        Serial.println(temperature);
        Serial.print(F("His humidity:"));
        Serial.println(humidity);

        if (!tb.connected()) {
          Serial.print(F("Not connected"));
          return;
        }
        char mac[16];
        byte* addr = (byte*)&remoteAddress;
        sprintf(mac, "%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X", addr[3], addr[2], addr[1], addr[0], addr[7], addr[6], addr[5], addr[4]);
        
        const int data_items = 3;
        Telemetry data[data_items] = {
          { "temperature", temperature },
          { "humidity",    humidity },
          { "originmac",   mac },
        };
        tb.sendTelemetry(data, data_items);        
    }
}
