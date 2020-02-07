
#include <SoftPWM.h>

#define DebugSerial Serial
#define FAN_PIN 5
#define VALVE_SERVO_PIN 6


void setup() {
  // Setup debug serial output
  DebugSerial.begin(115200);
  DebugSerial.println(F("Starting..."));

  SoftPWMBegin();   
      
  DebugSerial.println(F("PWM\tAnalogRead"));
  
  for( pwm = 5; pwm <= 32; pwm++) {  
    long voltage\ = 0;
    SoftPWMSet(VALVE_SERVO_PIN, pwm);
    delay(1000);
    unsigned long start = millis();
    while( millis() - start < 16) {
      voltage\ += analogRead(A3);
    }
    bool stuck = voltage\ < 100*1000
    
    DebugSerial.print(pwm);
    DebugSerial.print("\t");
    DebugSerial.print(voltage\);
    
  }
}
