/*
  Authors:
    Kirill Toropov
    Nolan Caissie
    Jeff Travis
    Theresa Jensen

  Date: 2016-03-09

  Engineering Bridge Project
*/

#include "Arduino.h"
#include "Stepper.h"
#include "NewPing.h"
#include "SPI.h"
#include "MCP23S17.h"

const int NUM_OF_LIGHTS = 4;
const int MAX_PATTERN_SIZE = 5;

const int TIME_DELTA = 10;
long time = 0;

MCP io(0, 10);

const int motorEnab = 13;

Stepper motor(512, 12, 11, 8, 9);

bool closeSensor;
bool openSensor;

NewPing sonarBoatNorth(5, 4, 50);
NewPing sonarBoatSouth(3, 2, 40);
NewPing sonarWalk(7, 6, 90);

const int westSonarEnable = 13;
const int eastSonarEnable = 14;

namespace TrafficLights {
  typedef struct {
    int size;
    int pins[3];
    bool onExpander;
    int ticks;
    int pattern[3];
    int nextPattern;
    bool ticking;
  } Light;

  Light create(Light light){
    for(int i = 0; i < light.size; i++){
      if(light.onExpander){
        io.pinMode(light.pins[i], HIGH);
      } else {
        pinMode(light.pins[i], OUTPUT);
      }
    }
    return light;
  }

  void lightPattern(Light &light){
    Serial.println("LIGHT");
    for(int i = 0; i < 3; i++){
      if(light.onExpander){
        io.digitalWrite(light.pins[i], light.pattern[i]);
      } else {
         digitalWrite(light.pins[i], light.pattern[i]);
      }
    }
  }

  void setPattern(Light &light, int pattern[]){
    if(pattern[0] == 0){
      light.ticking = false;
      light.nextPattern = 0;
      light.ticks = 0;

      Serial.println("Ain't ticking");
      return;
    }

    light.ticks = pattern[0];
    light.nextPattern = pattern[1];
    light.ticking = true;

    for(int i = 2; i < 5; i++){
      light.pattern[i-2] = pattern[i];
    }

    lightPattern(light);
  }
}

/*
 DECK       - 0
 WALKWAYS   - 1
 BOAT_NORTH - 2
 BOAT_SOUTH - 3
*/
TrafficLights::Light lights[NUM_OF_LIGHTS];

//time - nextIndex - pin1 - pin2 - pin3
int patterns[][5] = {
  //0 - Reset
  {0,   0, 0, 0, 0},

  //1 - Traffic normal
  {200, 2, 1, 0, 0},
  {50,  1, 0, 0, 0},

  //3 - Traffic to red
  {500, 4, 0, 1, 0},
  {0, 0, 0, 0, 1},

  //4 - Pedestrian green
  {0, 0, 1, 0, 0},

  //5 - Pedestrian red
  {0, 0, 0, 1, 0},

  //6 - Boat yellow
  {0, 0, 1, 0, 0},

  //7 - Boat blink yellow
  {100, 8, 1, 0, 0},
  {100, 7, 0, 1, 0},

  //9 - Boat red
  {0, 0, 0, 1, 0}
};

void setLights(int id, int pattern){
  TrafficLights::Light light = lights[id];
  TrafficLights::setPattern(light, patterns[pattern]);
  lights[id] = light;
}

void setup(){
  Serial.begin(9600);
  pinMode(motorEnab, OUTPUT);

  //Set up IO Expander
  io.begin();

  lights[0] = TrafficLights::create({3, {6, 5, 4}, true});
  lights[1] = TrafficLights::create({2, {8, 7}, true});
  lights[2] = TrafficLights::create({2, {9, 10}, true});
  lights[3] = TrafficLights::create({2, {11, 12}, true});

  //Initial lights' patterns
  setLights(0, 1);
  setLights(1, 4);
  setLights(2, 7);
  setLights(3, 7);

  //set up deck sensors
  io.pinMode(3, HIGH); //Output
  io.pinMode(2, LOW); //Input
  io.pinMode(1, LOW); //Input
  io.digitalWrite(3, HIGH);

  //deck sonar transistors
  io.pinMode(westSonarEnable, HIGH);
  io.pinMode(eastSonarEnable, HIGH);
}

int motorDirection = 1;
int motorSpeed = 5;

void loop_DERP(){
    SPI.end();
    digitalWrite(13, HIGH);
    motor.setSpeed(10);

    delay(100);
    motor.step(512);

    digitalWrite(13, LOW);
    delay(100);
    motor.step(-512);
}

int readings[5];
const int precision = 2;
bool checkBoat(NewPing sonar){
  for(int i=1; i<5; i++){
    readings[i - 1] = readings[i];
  }

  int uS = sonar.ping_median(5);
  int dis = sonar.convert_cm(uS);
  Serial.println(dis);
  if(dis == 0){
    return false;
  }
  readings[4] = dis;

  //Calculate values
  int total = 0;
  for(int i=0; i<5; i++){
    if(readings[i] == 0){
      return false;
    }
    total += readings[i];
  }

  int avg = total / 5;
  int match = 0;
  for(int i=0; i<5; i++){
    if(abs(readings[i] - avg) <= precision){
      match++;
    }
  }

  if(match == 5){
    return true;
  }

  return false;
}

int freeDeckTicks = 0;
bool checkDeck(){
  io.digitalWrite(westSonarEnable, HIGH);
  io.digitalWrite(eastSonarEnable, LOW);

  int uS = sonarWalk.ping();
  int dis = sonarWalk.convert_cm(uS);
  Serial.print("West: ");
  Serial.println(dis);

  //
  // io.digitalWrite(westSonarEnable, LOW);
  // io.digitalWrite(eastSonarEnable, HIGH);
  //
  // uS = sonarWalk.ping();
  // dis = sonarWalk.convert_cm(uS);
  // Serial.print("East: ");
  // Serial.println(dis);

  return false;
}

void loop(){
  //Loop through traffic lights
  SPI.begin();
  for(int i = 0; i < NUM_OF_LIGHTS; i++){
    TrafficLights::Light light = lights[i];
    if(light.ticking){
      if(light.ticks <= 0){
         TrafficLights::setPattern(light, patterns[light.nextPattern]);
      }

      light.ticks--;
      lights[i] = light;
    }
  }

  //Get sensor readings
  closeSensor = io.digitalRead(2);
  openSensor = io.digitalRead(1);
  // Serial.println(closeSensor);

  SPI.end();

  delay(TIME_DELTA);

  //Find the boat
  // checkBoat(sonarBoatSouth);
  checkDeck();

  // Serial.println(checkBoat(false));

  // motor.setSpeed(motorSpeed);
  // if(motorDirection != 0){
  //   motor.step(motorDirection);
  // }
  //
  // if(time % 3000 == 0){
  //   digitalWrite(motorEnab, HIGH);
  //   if(motorDirection == 0){
  //     motorDirection = 1;
  //   }
  //   motorDirection *= -1;
  // }
  // if(time % 6000 == 0){
  //   motorDirection = 0;
  //   digitalWrite(motorEnab, LOW);
  // }

  time += TIME_DELTA;
}
