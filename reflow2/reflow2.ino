#include <LiquidCrystal.h>
#include <max6675.h>
#include <Wire.h>

// The pin we use to control the relay
#define RELAYPIN 4

// The SPI pins we use for the thermocouple sensor
#define MAX_CLK 5
#define MAX_CS 6
#define MAX_DATA 7

// the Proportional control constant
#define Kp  10
// the Integral control constant
#define Ki  0.5
// the Derivative control constant 
#define Kd  100

// Windup error prevention, 5% by default
#define WINDUPPERCENT 0.05  

MAX6675 thermocouple(MAX_CLK, MAX_CS, MAX_DATA);

// Classic 16x2 LCD used
LiquidCrystal lcd(8,9,10,11,12,13);

// volatile means it is going to be messed with inside an interrupt 
// otherwise the optimization code will ignore the interrupt

volatile long seconds_time = -1;  // this will get incremented once a second, -1 for no init
volatile float the_temperature;  // in celsius
volatile float previous_temperature;  // the last reading (1 second ago)

// the current temperature
volatile float target_temperature;

// we need this to be a global variable because we add error each second
float Summation;        // The integral of error since time = 0

int relay_state;       // whether the relay pin is high (on) or low (off)

volatile long uptime;
int opmode = 3;//opmode  is  0standby 1preheat 2ready 3on 
#define phases 10
int targettemp[phases]= {150,150,190,220,240,220,190,160,  0,  0};//these 10 values should be interpered as points on an xy coordinate system
int targettime[phases]= {  0, 15,105,120,135,150,165,180,181,480};//x is the time and y is the temperature.
volatile long phaseseconds;//a counter in seconds starting at the begining of this phase
volatile float m,b;
volatile int deltax,deltay,t1,t2;

/*int pan1315sec[phases]  = {  0, 15,105,120,135,150,165,180,181,480};
 int pan1315temp[phases] =  {150,150,190,220,240,220,190,160,  0,  0};
 int blank[phases] = {0,0,0,0,0,0,0,0,0,0};
 
 int (*targettemp[])(void) = {
 pan1315temp,
 blank 
 };
 int (*targettime[])(void)  = {
 pan1315sec,
 blank
 };*/


void setup() {  
  Serial.begin(9600); 
  Serial.println("Reflowduino!");

  // The data header (we have a bunch of data to track)
  Serial.print("Time (s)\tTemp (C)\tSet Temp\tError\tSlope\tSummation\tPID Controller\tRelay");

  // Now that we are mucking with stuff, we should track our variables
  Serial.print("\t\tKp = "); 
  Serial.print(Kp);
  Serial.print(" Ki = "); 
  Serial.print(Ki);
  Serial.print(" Kd = "); 
  Serial.println(Kd);

  // the relay pin controls the plate
  pinMode(RELAYPIN, OUTPUT);
  // ...and turn it off to start!
  pinMode(RELAYPIN, LOW);

  // Set up 16x2 standard LCD  
  lcd.begin(16,2);

  // clear the screen and print out the current version
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Reflowduino!");
  lcd.setCursor(0,1);
  // compile date
  lcd.print(__DATE__);

  // pause for dramatic effect!
  delay(2000);
  lcd.clear();

  // where we want to be
  target_temperature = 100.0;  // 100 degrees C
  // set the integral to 0
  Summation = 0;

  // Setup 1 Hz timer to refresh display using 16 Timer 1
  TCCR1A = 0;                           // CTC mode (interrupt after timer reaches OCR1A)
  TCCR1B = _BV(WGM12) | _BV(CS10) | _BV(CS12);    // CTC & clock div 1024
  OCR1A = 15609;                                 // 16mhz / 1024 / 15609 = 1 Hz
  TIMSK1 = _BV(OCIE1A);                          // turn on interrupt
}


void loop() { 
  // we moved the LCD code into the interrupt so we don't have to worry about updating the LCD 
  // or reading from the thermocouple in the main loop

    float MV; // Manipulated Variable (ie. whether to turn on or off the relay!)
  float Error; // how off we are
  float Slope; // the change per second of the error


  Error = target_temperature - the_temperature;
  Slope = previous_temperature - the_temperature;
  // Summation is done in the interrupt

  // proportional-derivative controller only
  MV = Kp * Error + Ki * Summation + Kd * Slope;

  // Since we just have a relay, we'll decide 1.0 is 'relay on' and less than 1.0 is 'relay off'
  // this is an arbitrary number, we could pick 100 and just multiply the controller values

  if (MV >= 1.0) {
    relay_state = HIGH;
    digitalWrite(RELAYPIN, HIGH);
  } 
  else {
    relay_state = LOW;
    digitalWrite(RELAYPIN, LOW);
  }
}


// This is the Timer 1 CTC interrupt, it goes off once a second
SIGNAL(TIMER1_COMPA_vect) { 
  uptime++;
  // time moves forward!
if(opmode>=3){
  seconds_time++;
  phaseseconds+=1; 
}

  // save the last reading for our slope calculation
  previous_temperature = the_temperature;

  // we will want to know the temperauter in the main loop()
  // instead of constantly reading it, we'll just use this interrupt
  // to track it and save it once a second to 'the_temperature'
  the_temperature = thermocouple.readCelsius();

  // Sum the error over time
  Summation += target_temperature - the_temperature;

  if ( (the_temperature < (target_temperature * (1.0 - WINDUPPERCENT))) ||
    (the_temperature > (target_temperature * (1.0 + WINDUPPERCENT))) ) {
    // to avoid windup, we only integrate within 5%
    Summation = 0;
  }

  //target temperature phase
  int i;
  for(i=0;i<phases-1;i++){
    if(seconds_time==targettime[i]){//if we are on the cusp of a new leg set the variables that will remain the same throughout
      target_temperature = targettemp[i];
      deltax=max(targettime[i]-targettime[i+1],targettime[i+1]-targettime[i]);
      deltay=targettemp[i+1]-targettemp[i];
      // m =(deltay/delatx);//our slope is  rise over run(y2-y1)/(x2-x1)
      //b = targettemp[i]-((deltay/deltax)*targettime[i]);//y-mx=b
      t1 = targettemp[i];
      t2 = targettime[i];
      phaseseconds=0;//reset the phase counter
      Serial.println("BAAAAM!");
    }
  }
  
  if(seconds_time>=targettime[phases-1]){
    opmode=0;
    Serial.println("Done");
  }

  if(deltay/deltax>0){
  target_temperature += deltay/deltax;//((deltay/deltax)*phaseseconds)+(t1-((deltay/deltax)*t2));// y=mx+b y is our target temp in c, m and b we calculated from two time/temp points, and x is seconds
  }else{
   if(deltay/deltax<1&&deltay/deltax>-1){
   

   }
  }
    
  }
  // display current time and temperature
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Time: ");
  lcd.print(seconds_time);
  lcd.print(" s");

  // go to line #1
  lcd.setCursor(0,1);
  lcd.print(the_temperature);
#if ARDUINO >= 100
  lcd.write(0xDF);
#else
  lcd.print(0xDF, BYTE);
#endif
  lcd.print("C ");

  // print out a log so we can see whats up
  Serial.print(seconds_time);
  Serial.print("\t"); 
  Serial.print("\t");
  Serial.print(the_temperature);
  Serial.print("\t"); 
  Serial.print("\t");
  Serial.print(target_temperature);
  Serial.print("\t"); 
  Serial.print("\t");
  Serial.print(target_temperature - the_temperature); // the Error!
  Serial.print("\t");
  Serial.print((deltay/deltax)); // the Slope of our reflow profile
  Serial.print("\t");
  Serial.print(t1-((deltay/deltax)*t2)); // the Integral of Error
  Serial.print("\t"); 
  Serial.print("\t");  
  Serial.print(Kp*(target_temperature - the_temperature) + Ki*Summation + Kd*(previous_temperature - the_temperature)); //  controller output
  Serial.print("\t"); 
  Serial.print("\t");
  Serial.println(relay_state);
} 

