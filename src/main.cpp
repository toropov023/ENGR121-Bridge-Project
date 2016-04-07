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
  {200, 2, 1, 0, 0},
  {50,  1, 0, 0, 0},

  //3 - Traffic to red
  {500, 4, 0, 1, 0},
  {0, 0, 0, 0, 1},

  //5 - Pedestrian green
  {0, 0, 1, 0, 0},

  //6 - Pedestrian red
  {0, 0, 0, 1, 0},

  //7 - Boat yellow
  {0, 0, 1, 0, 0},

  //8 - Boat blink red
  {100, 9, 0, 1, 0},
  {100, 8, 0, 0, 0},

  //10 - Boat red
  {0, 0, 0, 1, 0},

  //11 - Pedestrian green blink
  {50, 12, 1, 0, 0},
  {50, 11, 0, 0, 0},
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

int readings[5] = {0};
bool checkBoat(NewPing sonar, int precision, int matchAmount){
  for(int i=1; i<5; i++){
    readings[i - 1] = readings[i];
  }

  int uS = sonar.ping_median(5);
  int dis = sonar.convert_cm(uS);
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

int boatLeaveTicks = 0;
bool checkBoatLeave(NewPing sonar){
  if(!checkBoat(sonar, 10, 1)){
    boatLeaveTicks++;
  } else {
    boatLeaveTicks = 0;
  }

  if(boatLeaveTicks < 300){
    return false;
  }

  return true;
}

int freeDeckTicks = 0;
bool checkDeck(){
  io.digitalWrite(westSonarEnable, HIGH);
  io.digitalWrite(eastSonarEnable, LOW);

  int uS = sonarWalk.ping();
  int dis = sonarWalk.convert_cm(uS);
  // Serial.print("West: ");
  // Serial.println(dis);

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

int stateTicks[] = { 0 };
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


  Serial.print("State: ");
  Serial.println(state);
  stateTicks[state]++;
  switch(state){
    case 1:
      //Find the boat
      if(checkBoat(sonarBoatSouth, 2, 5)){
        boatPresent = 2;

        setLights(3, 8);
        state = 2;
      } else if(checkBoat(sonarBoatNorth, 2, 5)){
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
      if(stateTicks[3] == 500){
        setLights(1, 6);
      }

      if(stateTicks[3] > 800 && checkDeck()){
        state = 4;
      }

      break;

    case 4:
      if(openSensor || stateTicks[4] >= 6000){
        state = 5;
        setLights((boatPresent == 1 ? 2 : 3), 7);
      } else {
        //TODO move
      }

      break;

    case 5:
      if(stateTicks[5] > 1000){
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
      if(checkBoatLeave(boatPresent == 1 ? sonarBoatSouth : sonarBoatNorth)){
        //TODO move
      }

      if(closeSensor){
        state = 0;
      }

      break;

    default:
      freeDeckTicks = 0;
      boatPresent = 0;
      boatLeaveTicks = 0;
      time = 0;
      memset(stateTicks, 0, numOfStates);

      //Initial lights' patterns
      setLights(0, 1);
      setLights(1, 5);
      setLights(2, 10);
      setLights(3, 10);

      state = 1;
  }



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
