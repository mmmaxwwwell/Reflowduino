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

volatile long seconds_time = -60;  // this will get incremented once a second, -1 for no init
volatile float the_temperature;  // in celsius
volatile float previous_temperature;  // the last reading (1 second ago)

// the current temperature
volatile float target_temperature;

// we need this to be a global variable because we add error each second
float Summation;        // The integral of error since time = 0

int relay_state;       // whether the relay pin is high (on) or low (off)

volatile long uptime;
int opmode = 3;//opmode  is  0standby 1preheat 2ready 3on 

//source for mapping  http://interface.khm.de/index.php/lab/experiments/nonlinear-mapping/
float nodepoints[][2]= {
  {
    -60,100        }
  ,{
    0,100        }
  ,{
    15,150            } 
  , {
    105,190            }
  , {
    120,220            }
  ,{
    135,240            }
  ,{
    150,220      }
  ,{
    165,190      }
  ,{
    180,160      }
  ,{
    181,1      }
  ,{
    480,1      }
  ,
  {
    481,1  }
};
int phases = sizeof(nodepoints)/8;

void setup() {  
  Serial.begin(115200); 
  Serial.println("Reflowduino");
  Serial.print("We have ");
  Serial.print(phases);
  Serial.print(" nodes spanning ");
  Serial.print(nodepoints[phases-1][0]/60);
  Serial.println(" mins  ");
  Serial.println("Raw dump start:");
  serialdump();
  Serial.println("Draw");

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
  target_temperature = 1;  // 1 degree C
  // set the integral to 0
  Summation = 0;

  // Setup 1 Hz timer to refresh display using 16 Timer 1
  TCCR1A = 0;                           // CTC mode (interrupt after timer reaches OCR1A)
  TCCR1B = _BV(WGM12) | _BV(CS10) | _BV(CS12);    // CTC & clock div 1024
  OCR1A = 15609;                                 // 16mhz / 1024 / 15609 = 1 Hz
  TIMSK1 = _BV(OCIE1A);                          // turn on interrupt
}

void serialdump(){//dump a graph of our temp/time profile to processing
  for(int i=0;i<nodepoints[phases-1][0];i++){
    Serial.println(reMap(nodepoints,i));
    delay(10);
  }
  
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

  // time moves forward!
  if(opmode>=3){
    seconds_time++;
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


  int target_temperature = reMap(nodepoints,seconds_time);



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

  Serial.print("\t"); 
  Serial.print("\t");  
  Serial.print(Kp*(target_temperature - the_temperature) + Ki*Summation + Kd*(previous_temperature - the_temperature)); //  controller output
  Serial.print("\t"); 
  Serial.print("\t");
  Serial.println(relay_state);
} 
int reMap(float pts[10][2], int input) {
  int r;
  float m;

  for (int n=0; n < phases; n++) {

    if (input >= pts[n][0] && input <= pts[n+1][0]) {

      m = ( pts[n+1][1] - pts[n][1] ) / ( pts[n+1][0] - pts[n][0] );
      m = m * (input-pts[n][0]);
      m = m +  pts[n][1];


      //   m= ( pts[n][1] - pts[n+1][1] ) / ( pts[n][0] - pts[n+1][0] );
      //    m = m * (input-pts[n][0]);
      //   m = m +  pts[n][1];
      r = m;


    }
  }
  return(r);
}

/* PROCESSING CODE
 
 
 
 // Graphing sketch
 
 
 // This program takes ASCII-encoded strings
 // from the serial port at 9600 baud and graphs them. It expects values in the
 // range 0 to 1023, followed by a newline, or newline and carriage return
 
 // Created 20 Apr 2005
 // Updated 18 Jan 2008
 // by Tom Igoe
 // This example code is in the public domain.
 
 import processing.serial.*;
 
 Serial myPort;        // The serial port
 int xPos = 1;         // horizontal position of the graph
 
 void setup () {
    textFont(createFont("Georgia", 24));
 // set the window size:
 size(800, 600);        
 drawgraph();
 delay(100);
 // List all the available serial ports
 println(Serial.list());
 // I know that the first port in the serial list on my mac
 // is always my  Arduino, so I open Serial.list()[0].
 // Open whatever port is the one you're using.
 myPort = new Serial(this, Serial.list()[0], 115200);
 // don't generate a serialEvent() unless you get a newline character:
 myPort.bufferUntil('\n');
 // set inital background:
 background(0);
 }
 void draw () {
 // everything happens in the serialEvent()
 delay(5000);
drawgraph();
 
 }
 
 
 void serialEvent (Serial myPort) {
 // get the ASCII string:
 String inString = myPort.readStringUntil('\n');
 
 if (inString != null) {
   if(inString=="Draw"){
    drawgraph(); 
   }
 // trim off any whitespace:
 inString = trim(inString);
 // convert to an int and map to the screen height:
 float inByte = float(inString); 
 inByte = map(inByte, 0, 270, 0, height);
 
 // draw the line:
 stroke(127,34,255);
 line(xPos*2%width, height, xPos*2%width, height - inByte);
 
 // at the edge of the screen, go back to the beginning:
 if (xPos >= width) {
 xPos = 0;
 background(0); 
 } 
 else {
 // increment the horizontal position:
 xPos++;
 }
 }
 }
 
 void drawgraph(){
   
    stroke(157);
 for(int i=1;i<30;i++){
   stroke(157);line(0,map(i*10, 0, 270, height,0),800,map(i*10, 0, 270, height,0));
   stroke(157);line(i*20,0,i*20,width);
  stroke(157);text(i*10+"C", width * 0.75, map(i*10, 0, 270, height,0));
 if(i%6==0){
  stroke(157); line(i*20+1,0,i*20+1,width);
  stroke(157); line(i*20-1,0,i*20-1,width);
   stroke(157); text(i/6+"min", i*20, 50);
  }
 stroke(157); text("Reflowduino Reflow solder profile",0,25);
}
 }
 
 */
 




