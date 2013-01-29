/*
 Arduino code to phase control a triac
 
 Copyright (C) 2012 Dave Berkeley projects@rotwang.co.uk
 modified by mmmaxwwwell 1/29/2013
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 USA
 */
#include <LiquidCrystal.h>
#include <max6675.h>
#include <Wire.h>
#include <MsTimer2.h>
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

//triac stuff
const int ledPin = 13;
const int optoPowerPin = 3;
const int triacPin = 4;
const int optoPin = 2;

/*
  * The timer is set to ck/8 prescalar. At 16MHz this gives
 * 500us per count. With 50Hz mains, half cycle is 10ms long.
 * Therefore 20000 timer counts (10000us) equals one half cycle.
 */

static int percent;
static long time;
static long start_t = 0;

#define COUNTS(n) ((n) << 1) // convert us to counts

//  empirically determined overhead for state switch
#define OVERHEAD 18

// Triac pulse train
#define PWM_MARK (100 - OVERHEAD)
#define PWM_SPACE (250 - OVERHEAD)

/*
  *  Timer / counter config
 */

void clear_timer()
{
  TIMSK1 = 0; // no interrupts
  TCCR1A = 0; // counter off
  TCCR1B = 0;
  TCCR1C = 0;
  TCNT1 = 0;
  OCR1A = 0;
  OCR1B = 0;
}

void set_timer(unsigned long us)
{
  TCCR1A = 0;
  // divide by 8 prescaling
  TCCR1B = 0 << CS22 | 1 << CS21 | 0 << CS20;
  TCCR1B |= 0 << WGM13 | 1 << WGM12; // CTC mode

  OCR1A = COUNTS(us);
  // enable output compare A match interrupt
  TIMSK1 = 1 << OCIE1A;
}

/*
  *  Auto generated table of % power to timer count.
 * 
 *  Uses http://www.ditutor.com/integrals/integral_sin_squared.html
 *  curve to divide the power into even bins.
 */

static int power_lut_50Hz[100] = {
  0,      1161,   1472,   1694,   1872,   2022,   2155,   2277,   2388,   2494,
  2594,   2683,   2772,   2855,   2933,   3011,   3088,   3161,   3233,   3299,
  3366,   3433,   3494,   3555,   3616,   3677,   3738,   3794,   3855,   3911,
  3966,   4022,   4077,   4133,   4183,   4238,   4288,   4344,   4394,   4444,
  4500,   4550,   4600,   4650,   4700,   4750,   4800,   4850,   4900,   4949,
  5000,   5055,   5105,   5155,   5205,   5255,   5305,   5355,   5405,   5455,
  5505,   5561,   5611,   5661,   5716,   5766,   5822,   5872,   5927,   5983,
  6038,   6094,   6150,   6211,   6266,   6327,   6388,   6450,   6511,   6572,
  6638,   6705,   6772,   6844,   6916,   6994,   7072,   7149,   7233,   7322,
  7411,   7511,   7616,   7727,   7850,   7983,   8133,   8311,   8533,   8844,
};
static int power_lut_60Hz[100] = {
  0.00,	1857.60,	2355.20,	2710.40,	2995.20,	3235.20,	3448.00,	3643.20,	3820.80,	3990.40,	4150.40,	

  4292.80,	4435.20,	4568.00,	4692.80,	4817.60,	4940.80,	5057.60,	5172.80,	5278.40,	5385.60,	

  5492.80,	5590.40,	5688.00,	5785.60,	5883.20,	5980.80,	6070.40,	6168.00,	6257.60,	6345.60,	

  6435.20,	6523.20,	6612.80,	6692.80,	6780.80,	6860.80,	6950.40,	7030.40,	7110.40,	7200.00,	

  7280.00,	7360.00,	7440.00,	7520.00,	7600.00,	7680.00,	7760.00,	7840.00,	7918.40,	8000.00,	

  8088.00,	8168.00,	8248.00,	8328.00,	8408.00,	8488.00,	8568.00,	8648.00,	8728.00,	8808.00,	

  8897.60,	8977.60,	9057.60,	9145.60,	9225.60,	9315.20,	9395.20,	9483.20,	9572.80,	9660.80,	

  9750.40,	9840.00,	9937.60,	10025.60,	10123.20,	10220.80,	10320.00,	10417.60,	10515.20,	10620.80,	

  10728.00,	10835.20,	10950.40,	11065.60,	11190.40,	11315.20,	11438.40,	11572.80,	11715.20,	11857.60,	

  12017.60,	12185.60,	12363.20,	12560.00,	12772.80,	13012.80,	13297.60,	13652.80,	14150.40,	
};

int percent_to_count(int percent)
{
  if (percent == 100)
    return 0;
  if (percent == 0)
    return 10000;
  return power_lut_50Hz[100 - percent];
}

/*
  *  State Machine
 */

typedef enum { 
  ZERO,    // zero crossing period
  PHASE,   // start of cycle
  PWM_HI,  // pulse the triac
  PWM_LO   // rest the triac
} 
triac_state;

static int cycle_state;

void set_state(int next)
{
  clear_timer();
  const unsigned long now = micros();

  switch (next) {
  case ZERO :
    digitalWrite(triacPin, 0);
    digitalWrite(ledPin, 0);
    start_t = now;

  case PHASE :
    digitalWrite(triacPin, 0);
    digitalWrite(ledPin, 1);
    set_timer(percent_to_count(percent));
    break;

  case PWM_HI :
    digitalWrite(triacPin, 1);
    digitalWrite(ledPin, 0);
    set_timer(PWM_MARK);

    if (cycle_state == PHASE) {
      time = now - start_t;
    }
    break;

  case PWM_LO :
    digitalWrite(triacPin, 0);
    digitalWrite(ledPin, 0);
    set_timer(PWM_SPACE);
    break;
  }

  cycle_state = next;
}

/*
  *  Timer Compare interrupt
 */

ISR(TIMER1_COMPA_vect)
{
  switch (cycle_state)
  {
  case PWM_LO :
  case PHASE :
    set_state(PWM_HI);
    break;
  case PWM_HI :
    set_state(PWM_LO);
    break;
  }
}

/*
  *  Zero crossing interrupt handler
 */

void on_change()
{
  const int p = digitalRead(optoPin);

  if (p) 
  {
    // end of cycle
    set_state(ZERO);
  } 
  else {
    // start of cycle.
    set_state(PHASE);
  }
}
static int get_serial()
{
  static char buff[16];
  static int idx = 0;

  if (!Serial.available())
    return -1;

  const int c = Serial.read();

  buff[idx++] = c;

  if (idx >= (sizeof(buff)-1)) {
    // too many chars
    idx = 0;
    return -1;
  }

  // read a line
  if ((c != '\r') && (c != '\n'))
    return -1;

  // null out the \n
  buff[idx-1] = '\0';
  idx = 0;

  return read_command(buff);
}
static int read_command(char* buff)
{
  // command protocol :
  // sxxx set the percent
  if (*buff++ != 's')
    return -1; // bad command

  int percent = 0; 

  while (*buff) {
    const char c = *buff++;
    if ((c < '0') || (c > '9'))
      return -1; // bad number
    percent *= 10;
    percent += c - '0';
  }

  if (percent > 100)
    return -1;

  return percent;
}

//source for mapping  http://interface.khm.de/index.php/lab/experiments/nonlinear-mapping/
float nodepoints[][2]= {
  {
    -60,100            }
  ,{
    0,100            }
  ,{
    15,150                } 
  , {
    105,190                }
  , {
    120,220                }
  ,{
    135,240                }
  ,{
    150,220          }
  ,{
    165,190          }
  ,{
    180,160          }
  ,{
    181,1          }
  ,{
    480,1          }
  ,
  {
    481,1      }
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
  //Serial.println("Raw dump start:");
  //serialdump();
  //Serial.println("Draw");

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

  MsTimer2::set(1000, timer2); // 1s period
  MsTimer2::start();



  //triac stuff  
  pinMode(ledPin, OUTPUT);
  pinMode(optoPin, INPUT);
  pinMode(optoPowerPin, OUTPUT);
  pinMode(triacPin, OUTPUT);

  // initialise the triac control system
  clear_timer();
  percent = 0;
  set_state(ZERO);
  attachInterrupt(0, on_change, CHANGE);

}

void serialdump(){//dump a graph of our temp/time profile to processing
  for(int i=0;i<nodepoints[phases-1][0];i++){
    Serial.println(reMap(nodepoints,i));
    delay(10);
  }

}

void controller(){
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

void loop() { 
  controller();


}


// This is the Timer 1 CTC interrupt, it goes off once a second
void timer2(){

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







