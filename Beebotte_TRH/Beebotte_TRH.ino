#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "Adafruit_SHT31.h"

#define LEDPIN            5         // Pin which is connected to LED.
#define FANPIN            6         // Pin which is connected to FAN.

#define BBT "mqtt.beebotte.com"     // Domain name of Beebotte MQTT service
#define TOKEN "token:token_7PdJmhz4OAi8Lrd6"    // Set your channel token here
#define CHANNEL "Arduinno"          // Replace with your device name
#define TEMP_RESOURCE "temperature" // This is where we will store temperature
#define HUMID_RESOURCE "humidity"   // This is where we will store humidity
#define LED_RESOURCE "Led"
#define PWM_RESOURCE "Fan"
#define WRITE true // indicate if published data should be persisted or not


// Initialize SHT sensor.
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// Enter a MAC address of your shield
// It might be printed on a sticker on the shield
byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x3E, 0xAB };

IPAddress ip(10,2,0,23); //192.168.2.22  of  10.2.0.23

EthernetClient ethClient;
PubSubClient client(ethClient);

// will store last time DHT sensor data was read
unsigned long lastReadingMillis = 0;

// interval for sending SHT temp and humidity to Beebotte
const long interval = 10000;
// to track delay since last reconnection
unsigned long lastReconnectAttempt = 0;

const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";

char id[17]; // Identifier for the MQTT connection - will set it randomly

const char * generateID()
{
  randomSeed(analogRead(0));
  int i = 0;
  for(i = 0; i < sizeof(id) - 1; i++) {
    id[i] = chars[random(sizeof(chars))];
  }
  id[sizeof(id) -1] = '\0';

  return id;
}


// will be called every time a message is received
void onMessage(char* topic, byte* payload, unsigned int length) {
 
  // decode the JSON payload
  StaticJsonBuffer<128> jsonInBuffer;

  JsonObject& root = jsonInBuffer.parseObject(payload);

  // Test if parsing succeeded
  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
}
  // led resource is a boolean read it accordingly
  int data = root["data"];
    int PWM;
  PWM = map(data, 0, 100, 0, 255);
  
  // Print the received value to serial monitor for debugging
  Serial.print("topic: ");
  Serial.println(String(topic));
  Serial.print("Received message of length ");
  Serial.print(length);
  Serial.println();
  Serial.print("data ");
  Serial.print(data);
  Serial.println();

  if (strcmp(topic,"Arduinno/Led")==0)  
  analogWrite(LEDPIN, PWM);

  if (strcmp(topic,"Arduinno/Fan")==0)
  analogWrite(FANPIN, PWM);
}


// publishes data to the specified resource
void publish(const char* resource, float data, bool persist)
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
  sprintf(topic, "%s/%s", CHANNEL, resource);

  // Now publish the char buffer to Beebotte
  client.publish(topic, buffer);
}


void readSensorData()
{
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = sht31.readHumidity();
  // Read temperature as Celsius (the default)
  float t = sht31.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (!isnan(h) || !isnan(t)) {
    publish(TEMP_RESOURCE, t, WRITE);
    publish(HUMID_RESOURCE, h, WRITE);
  }
}


// reconnects to Beebotte MQTT server
boolean reconnect() {
  if (client.connect(generateID(), TOKEN, "")) {
    char LED [64];
    char FAN [64];
    sprintf(LED, "%s/%s", CHANNEL, LED_RESOURCE);  
    sprintf(FAN, "%s/%s", CHANNEL, PWM_RESOURCE); 
    client.subscribe(LED);
    client.subscribe(FAN);
    Serial.println("Connected to Beebotte MQTT");
  }
  return client.connected();
}


void setup()
{
  pinMode(LEDPIN, OUTPUT);
  pinMode(FANPIN, OUTPUT);
    
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  // Setup SHT sensor
  sht31.begin(0x44);   // Set to 0x45 for alternate i2c addr
  
  client.setServer(BBT, 1883);
  client.setCallback(onMessage);
  
  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // try to congifure using static IP address instead of DHCP:
    // Feel free to change this according to your network settings
    Ethernet.begin(mac, ip);
  }

  // give the Ethernet shield a second to initialize:
  delay(1000);

  Serial.println("connecting...");
  lastReconnectAttempt = 0;
}


void loop()
{
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
    // Client connected

    // read sensor data every 10 seconds
    // and publish values to Beebotte
    unsigned long currentMillis = millis();
    if (currentMillis - lastReadingMillis >= interval) {
      // save the last time we read the sensor data
      lastReadingMillis = currentMillis;

      readSensorData();
    }
    client.loop();
  }
}
