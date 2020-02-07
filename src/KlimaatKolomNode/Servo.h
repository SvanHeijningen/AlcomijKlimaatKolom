

int first = -1;
int last = -1;

long stuck_threshold = -1;

long measureVoltage(){
    long voltage = 0;
    unsigned long start = millis();
    while( millis() - start < 16) {
      voltage += analogRead(A3);
    }
    DebugSerial.print("Voltage: ");
    DebugSerial.println(voltage);
    return voltage;
}

bool isServoStuck(){
    long voltage = measureVoltage();
    bool stuck = voltage < stuck_threshold;
    return stuck;
}

void setServoPercent(int percent) 
{
    int pwm = map(percent, 0, 100, first, last);
    DebugSerial.print("Setting servo to");
    DebugSerial.println(pwm);
    SoftPWMSet(VALVE_SERVO_PIN, pwm);
    delay(1000);
    if( isServoStuck() )
       DebugSerial.println("ERROR Servo stuck!");
    SoftPWMSet(VALVE_SERVO_PIN, 0);    
}

void setupServoWithDefaults()
{
   first = 1;
   last = 255;   
}

void setupServoWithCurrentMeasuring() 
{   
  DebugSerial.println(F("PWM\tAnalogRead"));

  SoftPWMSet(VALVE_SERVO_PIN, 0);
  long idleVoltage = measureVoltage();
  SoftPWMSet(VALVE_SERVO_PIN, 5);
  delay(1000);
  long stuckVoltage = measureVoltage();
  stuck_threshold = (idleVoltage + stuckVoltage) / 2;
  
  DebugSerial.print("Stuck threshold: ");
  DebugSerial.println(stuck_threshold);
  
  for( int pwm = 5; pwm <= 32; pwm++) {  
    SoftPWMSet(VALVE_SERVO_PIN, pwm);
    delay(1000);
    
    bool stuck = isServoStuck();
        
    DebugSerial.print(pwm);
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
  if( last - first < 10 )
    DebugSerial.println("ERROR Valve is probably stuck during initialization");
}


void setupServo() 
{
  
  SoftPWMBegin();         
  setupServoWithDefaults();
  setServoPercent(50);
}
