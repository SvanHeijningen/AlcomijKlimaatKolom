
#include <SoftPWM.h>

#define DebugSerial Serial
#define FAN_PIN 5
#define VALVE_SERVO_PIN 6


void setup() {
  // Setup debug serial output
  DebugSerial.begin(115200);
  DebugSerial.println(F("Starting..."));

  SoftPWMBegin();   

  int first = -1;
  int last = -1;
      
  DebugSerial.println(F("PWM\tAnalogRead"));
  
  for( int pwm = 5; pwm <= 32; pwm++) {  
    long voltage = 0;
    SoftPWMSet(VALVE_SERVO_PIN, pwm);
    delay(1000);
    unsigned long start = millis();
    while( millis() - start < 16) {
      voltage += analogRead(A3);
    }
    bool stuck = voltage < (120L*1000L);
    
    DebugSerial.print(pwm);
    DebugSerial.print("\t");
    DebugSerial.print(voltage);
    if( stuck) {
      DebugSerial.print( "  stuck");
    }
    DebugSerial.println();    
    if( !stuck ) {
      if( first == -1)
        first = pwm;
      last = pwm;
    } else {
      if( first > 0)
        break; // we found the end, which is in last
    }    
  }

  // first and last are now set
  
  DebugSerial.print("first:");
  DebugSerial.println(first);
  DebugSerial.print("last:");
  DebugSerial.println(last);
  int middle = (first + last + 1) / 2;
  DebugSerial.println(middle);
  SoftPWMSet(VALVE_SERVO_PIN, middle);
  if( last - first < 10 )
    DebugSerial.println("ERROR Valve is probably stuck during initialization");
}

void loop() 
{
  long pwm = DebugSerial.readStringUntil('\n').toInt();
  DebugSerial.println(pwm);
  
  SoftPWMSet(VALVE_SERVO_PIN, pwm);
}
