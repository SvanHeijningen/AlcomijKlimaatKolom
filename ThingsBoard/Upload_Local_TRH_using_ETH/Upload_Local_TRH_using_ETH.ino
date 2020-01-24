#include <Adafruit_SHT31.h>

#include <ThingsBoard.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

// Helper macro to calculate array size
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

// Update these with values suitable for your network.
byte mac[]    = {  0xA8, 0x61, 0x0A, 0xAE, 0x3E, 0xAB };

char thingsboardServer[] = "10.2.0.122";
// Initialize SHT sensor.
Adafruit_SHT31 sht31 = Adafruit_SHT31();
  
#define TOKEN "IbG1vPQ4Y1r2hd8HYeo8" // ThingsBoard Device Auth Token

int lastSend = 0;
int lastMaintain = 0;
int pwm = 0;

bool subscribed = false;

EthernetClient ethClient;
ThingsBoard tb(ethClient);

void setup()
{
  Serial.begin(115200);
    while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Wire.setClock(400000);
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
  tb.loop();
  Ethernet.maintain();
  if ( millis() - lastSend > 1000 ) { // Update and send only after 1 seconds
    getAndSendTemperatureAndHumidityData();
    lastSend = millis();
  } 
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
