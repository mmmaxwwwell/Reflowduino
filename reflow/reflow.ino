#include "Adafruit_MAX31855.h"
#include <Wire.h>

// The pin we use to control the relay
#define RELAYPINbottom 4
#define RELAYPINtop 3
#define RELAYPINfan 2
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

// Windup error prevention, 10% by default
#define WINDUPPERCENT 0.15  

Adafruit_MAX31855 thermocouple(MAX_CLK, MAX_CS, MAX_DATA);

// volatile means it is going to be messed with inside an interrupt 
// otherwise the optimization code will ignore the interrupt
volatile long seconds_time = 0;  // this will get incremented once a second
volatile float the_temperature;  // in celsius
volatile float previous_temperature;  // the last reading (1 second ago)

// the current temperature
float target_temperature;

// we need this to be a global variable because we add error each second
float Summation;        // The integral of error since time = 0

int relay_state;       // whether the relay pin is high (on) or low (off)

int opmode = 1;//opmode  is  0? 1preheat 2ready 3on 
#define pretemp 150 // preheat to 100 C
boolean preheat = false;
//source for mapping  http://interface.khm.de/index.php/lab/experiments/nonlinear-mapping/
float nodepoints[][2]= {//

  {
    0,150}//soak start, 0:00
  ,{ // +0.27 degC/sec
    180,200} //soak end, 3:00
  , { // +0.14 degC/sec
    300,217}// melting point 5:00
  , {//+ 0.55 degC/sec
    360,250}// peak temp of 6:00
  , {//end of cycle
    361,1}
  , {
362,1}
    
//  ,{
//    135,240}
//  ,{
//    150,220}
//  ,{
//    165,190}
//  ,{
//    180,160}
//  ,{
//    181,1}
//  ,{
//    480,1}
//  ,
//  {481,1}
};

int phases = sizeof(nodepoints)/8;

void setup() {  
  Serial.begin(115200); 
  Serial.println("Reflowduino!");
  Serial.print("We have ");
  Serial.print(phases);
  Serial.print(" nodes spanning ");
  Serial.print(nodepoints[phases-1][0]/60);
  Serial.println(" mins  ");
  //Serial.println("Raw dump start:");
  //serialdump();
  //Serial.println("Draw");

  // The data header (we have a bunch of data to track)
  Serial.print("Time (s)\tTemp (C)\tSet Temp\tError\tPID Controller\tRelay");

  // Now that we are mucking with stuff, we should track our variables
  Serial.print("\t\tKp = "); 
  Serial.print(Kp);
  Serial.print(" Ki = "); 
  Serial.print(Ki);
  Serial.print(" Kd = "); 
  Serial.println(Kd);


  // the relay pin controls the plate
  pinMode(RELAYPINtop, OUTPUT);
  pinMode(RELAYPINbottom, OUTPUT);
  pinMode(RELAYPINfan, OUTPUT);
  // ...and turn it off to start!
  digitalWrite(RELAYPINtop, HIGH);
  digitalWrite(RELAYPINbottom, HIGH);
  digitalWrite(RELAYPINfan, LOW);

  // pause for dramatic effect!
  delay(2000);


  // where we want to be
  target_temperature = 1;  // 1 degree C (keep it off & away from 0)
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

  //We have 2 heating elements on 2 seperate relays. 
  //The top has a cold resistance of ~16 ohms so it should draw over 7.5 amps at 120v ac rms (+900 watts). 
  //The bottom coil has a cold resistance of 23.5 ohms and should draw over 5 amps at 120v ac rms
  //what kind of heating element is this? resistive? is CEMF playing into the equation? whats the hot resistance of the two coils?
  //We have 4 states that go from least power consumption to most, top/botom
  //1 00 off
  //2 01 bottom on top off
  //3 10 top on bottom off
  //4 11 both on
  // if (MV >= 1.0) {
  //   relay_state = HIGH;
  //   digitalWrite(RELAYPINbottom, LOW);
  // } 
  // else {
  //   relay_state = LOW;
  //   digitalWrite(RELAYPINbottom, HIGH);
  // }

  if(MV >= 100){ //state 4 both on
        digitalWrite(RELAYPINtop, LOW);
        digitalWrite(RELAYPINbottom, LOW);
        relay_state = 3;
      }
  else {
    if(MV >=75){//state 3 top on bottom off
      digitalWrite(RELAYPINtop, LOW);
      digitalWrite(RELAYPINbottom, HIGH);
      relay_state = 2;
    }
    else{//state 2 bottom on top off
    if (MV >= 50) {// state 2 bottom on top off
    digitalWrite(RELAYPINtop, HIGH);
    digitalWrite(RELAYPINbottom, LOW);
    relay_state = 1;
  } 
      else{//state 1 both off
    if (MV < 25) {// state 2 bottom on top off
    digitalWrite(RELAYPINtop, HIGH);
    digitalWrite(RELAYPINbottom, HIGH);
    relay_state = 0;
  } 
      }
    }

  }


/*
if (MV >= 85) {// state 2 bottom on top off
    digitalWrite(RELAYPINtop, HIGH);
    digitalWrite(RELAYPINbottom, LOW);
    relay_state = 1;
  } 
  else {
    if(MV >=175){//state 3 top on bottom off
      digitalWrite(RELAYPINtop, LOW);
      digitalWrite(RELAYPINbottom, HIGH);
      relay_state = 2;
    }
    else{//state 4 both on
      if(MV >= 350){
        digitalWrite(RELAYPINtop, LOW);
        digitalWrite(RELAYPINbottom, LOW);
        relay_state = 3;
      }
      else{//state 1 both off
        if(MV < 85){
          digitalWrite(RELAYPINtop, HIGH);
          digitalWrite(RELAYPINbottom,  HIGH);
          relay_state = 0;
        }
      }
    }

  }


*/
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

  if(opmode>=3){
    target_temperature = reMap(nodepoints,seconds_time);
  }
  else{
    if(opmode==1){
      target_temperature = pretemp;


      if(the_temperature >= pretemp)
      {
        Serial.println("Preheated, ready to run");
        opmode = 2;
      }
    }
    else{
      if(opmode==2){
        if(Serial.available()){
          opmode = 3;

        }
      } 
    }
  }


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






