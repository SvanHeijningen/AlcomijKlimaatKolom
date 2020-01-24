#include <ThingsBoard.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include "Adafruit_SHT31.h"

// Update these with values suitable for your network.
byte mac[]    = {  0xA8, 0x61, 0x0A, 0xAE, 0x3E, 0xAB };

char thingsboardServer[] = "10.2.0.122";
// Initialize SHT sensor.
Adafruit_SHT31 sht31 = Adafruit_SHT31();
  
#define TOKEN "IbG1vPQ4Y1r2hd8HYeo8" // ThingsBoard Device Auth Token

int lastSend = 0;
int lastMaintain = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

EthernetClient ethClient;
ThingsBoard tb(ethClient);

void setup()
{
  Serial.begin(115200);
    while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // Setup SHT sensor
  sht31.begin(0x44);   // Set to 0x45 for alternate i2c addr
  
  Ethernet.begin(mac);
  // Allow the hardware to sort itself out
  delay(1500);
}

void loop()
{
  if ( !tb.connected() ) {
    reconnect();
  }

  if ( millis() - lastSend > 1000 ) { // Update and send only after 1 seconds
    getAndSendTemperatureAndHumidityData();
    lastSend = millis();
  }

  tb.loop();
     Ethernet.maintain();
}

void getAndSendTemperatureAndHumidityData()
{
  Serial.println("Collecting temperature data.");

  // Reading temperature or humidity takes about 250 milliseconds!
  float humidity = sht31.readHumidity();
  // Read temperature as Celsius (the default)
  float temperature = sht31.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Serial.println("Sending data to ThingsBoard:");
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" *C ");

  tb.sendTelemetryFloat("temperature", temperature);
  tb.sendTelemetryFloat("humidity", humidity);
}


void reconnect() {
  // Loop until we're reconnected
  while (!tb.connected()) {
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
  }
}
