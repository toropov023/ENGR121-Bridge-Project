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

const int TIME_DELTA = 100;
int delta = 0;
long time = 0;

MCP io(0, 10);

const int motorEnab = 13;

Stepper motor(512, 12, 11, 8, 9);

int motorDirection = 1;
int motorSpeed = 3;

const int closedPin = A4;
const int openPin = A5;

NewPing sonarBoatNorth(3, 2, 30);
NewPing sonarBoatSouth(5, 4, 30);
NewPing sonarWalk(7, 6, 90);

const int westSonarEnable = 13;
const int eastSonarEnable = 14;

int boatPresent = 0; //0 - no boat, 1 - at north end, 2 - at south end

//0 - initial/reset
//1 - waiting for boat
//2 - checking the deck
//3 - changing lights
//4 - moving the bridge
//5 - checking the boat leave
//6 - closing the bridge
//0 - reset
int state = 0;
const int numOfStates = 7;

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
    for(int i = 0; i < 3; i++){
      if(light.onExpander){
        io.digitalWrite(light.pins[i], light.pattern[i]);
      } else {
         digitalWrite(light.pins[i], light.pattern[i]);
      }
    }
  }

  void setPattern(Light &light, int pattern[]){
    light.ticks = pattern[0];
    light.nextPattern = pattern[1];
    light.ticking = true;

    if(light.ticks == 0){
      light.ticking = false;
      light.nextPattern = 0;
      light.ticks = 0;
    }

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
  {20, 2, 1, 0, 0},
  {5,  1, 0, 0, 0},

  //3 - Traffic to red
  {50, 4, 0, 1, 0},
  {0, 0, 0, 0, 1},

  //5 - Pedestrian green
  {0, 0, 1, 0, 0},

  //6 - Pedestrian red
  {0, 0, 0, 1, 0},

  //7 - Boat yellow
  {0, 0, 1, 0, 0},

  //8 - Boat blink red
  {10, 9, 0, 1, 0},
  {10, 8, 0, 0, 0},

  //10 - Boat red
  {0, 0, 0, 1, 0},

  //11 - Pedestrian RED blink
  {5, 12, 0, 1, 0},
  {5, 11, 0, 0, 0},
};

void setLights(int id, int pattern){
  TrafficLights::Light light = lights[id];
  TrafficLights::setPattern(light, patterns[pattern]);
  lights[id] = light;
}

void setup(){
  Serial.begin(9600);
  pinMode(motorEnab, OUTPUT);

  pinMode(A3, OUTPUT);
  pinMode(A4, INPUT);
  pinMode(A5, INPUT);

  //Set up IO Expander
  io.begin();

  lights[0] = TrafficLights::create({3, {6, 5, 4}, true});
  lights[1] = TrafficLights::create({2, {8, 7}, true});
  lights[2] = TrafficLights::create({2, {9, 10}, true});
  lights[3] = TrafficLights::create({2, {11, 12}, true});

  //set up deck sensors
  io.pinMode(3, HIGH); //Output
  io.pinMode(2, LOW); //Input
  io.pinMode(1, LOW); //Input
  io.digitalWrite(3, HIGH);

  //deck sonar transistors
  io.pinMode(westSonarEnable, HIGH);
  io.pinMode(eastSonarEnable, HIGH);
}

bool checkSonar(NewPing sonar){
  int uS = sonar.ping_median(5);
  int dis = sonar.convert_cm(uS);
  // Serial.println(dis);
  // return false;
  return dis != 0;
}

int boatLeaveTicks = 0;
bool boatPassed = false;
bool checkBoatLeave(NewPing sonar){
  if(!boatPassed){
    if(checkSonar(sonar)){
      boatPassed = true;
    } else {
      return false;
    }
  } else if(checkSonar(sonar)){
    boatLeaveTicks = 0;
    return false;
  }

  boatLeaveTicks++;
  if(boatLeaveTicks < 15){
    return false;
  }

  return true;
}

int freeDeckTicks = 0;
bool checkDeck(){
  // io.digitalWrite(westSonarEnable, LOW);
  // io.digitalWrite(eastSonarEnable, HIGH);
  //
  // if(checkSonar(sonarWalk)){
  //   freeDeckTicks = 0;
  //   return false;
  // }
  //
  // delay(10);

  io.digitalWrite(westSonarEnable, HIGH);
  io.digitalWrite(eastSonarEnable, LOW);

  if(checkSonar(sonarWalk)){
    freeDeckTicks = 0;
    return false;
  }

  freeDeckTicks++;
  return freeDeckTicks >= 15;
}

bool spiEnabled = true;
void setSPI(bool state){
  spiEnabled = state;
  if(state){
    SPI.begin();
  } else {
    SPI.end();
  }
}

int stateTicks[10] = { 0 };
void loop_FAKE(){
  delay(100);
  // io.digitalWrite(westSonarEnable, HIGH);
  io.digitalWrite(eastSonarEnable, HIGH);

  int uS = sonarWalk.ping_median(20);
  int dis = sonarWalk.convert_cm(uS);

  Serial.println(dis);
  // delay(1000);
  // io.digitalWrite(westSonarEnable, LOW);
}

long timing = 0;
const long maxTime = 950;
void loop(){
  //Loop through traffic lights
  if(spiEnabled){
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

    // Serial.println(closeSensor);
    SPI.end();
  }

  delay(TIME_DELTA);

  Serial.print("State: ");
  Serial.println(state);
  stateTicks[state]++;
  switch(state){
    case 1:
      //Find the boat
      if(checkSonar(sonarBoatSouth)){
        boatPresent = 2;

        setLights(3, 8);
        state = 2;
      } else if(checkSonar(sonarBoatNorth)){
        boatPresent = 1;

        setLights(2, 8);
        state = 2;
      } else {
        break;
      }

      break;

    case 2:
      setLights(1, 11);
      setLights(0, 3);
      state = 3;

      break;

    case 3:
      if(stateTicks[3] > 80){
        if(checkDeck()){
          state = 4;

          break;
        }
      }

      if(stateTicks[3] == 50){
        setLights(1, 6);
      }

      break;

    case 4:
      motor.setSpeed(motorSpeed);
      SPI.end();
      digitalWrite(motorEnab, HIGH);

      while(true){
        if(timing >= maxTime || analogRead(openPin) > 600){
          SPI.begin();
          setLights((boatPresent == 1 ? 2 : 3), 7);

          digitalWrite(motorEnab, LOW);

          state = 5;
          timing = 0;
          return;
        }
        SPI.end();
        delay(1);
        motor.step(-1);
        timing++;
      }

      break;

    case 5:
      SPI.begin();
      if(stateTicks[5] > 50){
        if(boatPresent == 2){
          if(checkBoatLeave(sonarBoatNorth)){
            state = 6;
            setLights(3, 10);
          }
        } else {
          if(checkBoatLeave(sonarBoatSouth)){
            state = 6;
            setLights(2, 10);
          }
        }
      }

      break;

    case 6:
      motor.setSpeed(motorSpeed);
      SPI.end();
      digitalWrite(motorEnab, HIGH);
      while(true){
        if(timing >= maxTime || analogRead(closedPin) > 600){
          state = 0;
          return;
        }
        SPI.end();
        delay(1);
        motor.step(1);
        timing++;
      }

      break;

    default:
      setSPI(true);
      freeDeckTicks = 0;
      boatPresent = 0;
      boatLeaveTicks = 0;
      time = 0;
      memset(stateTicks, 0, numOfStates);
      timing = 0;

      //Initial lights' patterns
      setLights(0, 1);
      setLights(1, 5);
      setLights(2, 10);
      setLights(3, 10);

      state = 1;
  }

  time += TIME_DELTA;
}
