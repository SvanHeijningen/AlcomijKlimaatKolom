
#include <XBee.h>
#include <Printers.h>
#include <SoftwareSerial.h>
#include "Adafruit_SHT31.h"
#include "binary.h"

XBeeWithCallbacks xbee;
Adafruit_SHT31 sht31 = Adafruit_SHT31();

#define XBeeSerial Serial

void setup() {

  // Setup XBee serial communication
  XBeeSerial.begin(9600);
  xbee.begin(XBeeSerial);
  delay(1);

  // Setup SHT sensor
  sht31.begin(0x44);

  // Send a first packet right away
  sendPacket();
}

void sendPacket() {
    // Prepare the Zigbee Transmit Request API packet
    ZBTxRequest txRequest;
    txRequest.setAddress64(0x0000000000000000);

    // Allocate 9 payload bytes: 1 type byte plus two floats of 4 bytes each
    AllocBuffer<9> packet;

    // Packet type, temperature, humidity
    packet.append<uint8_t>(1);
    packet.append<float>(sht31.readTemperature());
    packet.append<float>(sht31.readHumidity());
    txRequest.setPayload(packet.head, packet.len());

    // And send it
    xbee.send(txRequest);

}

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
