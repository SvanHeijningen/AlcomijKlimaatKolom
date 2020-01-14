#include <PubSubClient.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

#define LEDPIN            7         // Pin which is connected to LED.
#define FANPIN            6         // Pin which is connected to FAN.
#define KLEPPIN            5         // Pin which is connected to KLEP.

#define BBT "mqtt.beebotte.com"     // Domain name of Beebotte MQTT service
#define TOKEN "token:token_7PdJmhz4OAi8Lrd6"    // Set your channel token here
#define CHANNEL "Arduinno"          // Replace with your device name
#define LED_RESOURCE "Led"
#define PWM_RESOURCE "Fan"
#define KLEP_RESOURCE "Klep"

// Enter a MAC address of your shield
// It might be printed on a sticker on the shield
byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x3E, 0xAB };
IPAddress ip(192,168,2,22); //192.168.2.22

EthernetClient ethClient;
PubSubClient client(ethClient);

// to track delay since last reconnection
long lastReconnectAttempt = 0;

const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";

char id[17];

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
  Serial.print("PWM ");
  Serial.print(PWM);
  Serial.println();

  if (strcmp(topic,"Arduinno/Fan")==0)
  analogWrite(FANPIN, PWM);
 
  if (strcmp(topic,"Arduinno/Led")==0)  
  digitalWrite(LEDPIN, data ? HIGH : LOW);

  if (strcmp(topic,"Arduinno/Klep")==0)  
  analogWrite(KLEPPIN, PWM);
}

// reconnects to Beebotte MQTT server
boolean reconnect() {
  if (client.connect(generateID(), TOKEN, "")) {
    char FAN [64];
    char LED [64];
    char KLEP [64];
    sprintf(FAN, "%s/%s", CHANNEL, PWM_RESOURCE);  
    sprintf(LED, "%s/%s", CHANNEL, LED_RESOURCE);  
    sprintf(KLEP, "%s/%s", CHANNEL, KLEP_RESOURCE); 
    client.subscribe(FAN);
    client.subscribe(LED);
    client.subscribe(KLEP);
        
    Serial.println("Connected to Beebotte MQTT");
  return client.connected();
}
  Serial.println("I think connect failed.");
  return 0;
}

void setup()
{
  pinMode(LEDPIN, OUTPUT);
  pinMode(FANPIN, OUTPUT);
  pinMode(KLEPPIN, OUTPUT);

  client.setServer(BBT, 1883);
  // Set the on message callback
  // onMesage function will be called
  // every time the client received a message
  client.setCallback(onMessage);

  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

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
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    client.loop();
  }
}
