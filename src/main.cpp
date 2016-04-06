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

const int NUM_OF_LIGHTS = 4;
const int MAX_PATTERN_SIZE = 5;

const int TIME_DELTA = 10;
long time = 0;

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
        //TODO
      } else {
        pinMode(light.pins[i], OUTPUT);
      }
    }
    return light;
  }

  void lightPattern(Light &light){
    for(int i = 0; i < 3; i++){
      if(light.onExpander){
        //TODO
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
 BOAT_LEFT  - 2
 BOAT_RIGHT - 3
*/
TrafficLights::Light lights[NUM_OF_LIGHTS];

//time - nextIndex - pin1 - pin2 - pin3
int patterns[][5] = {
  {0,   0, 0, 0, 0},
  {100, 2, 1, 0, 1},
  {100, 1, 0, 1, 0},
  {1,   0, 1, 0, 1},
  {0,   0, 0, 0, 0},
  {0,   0, 0, 0, 0}
};


void setLights(int id, int pattern){
  TrafficLights::Light light = lights[id];
  TrafficLights::setPattern(light, patterns[pattern]);
  lights[id] = light;
}

void setup(){
  Serial.begin(9600);

  lights[0] = TrafficLights::create({3, {7, 8, 9}});
  lights[1] = TrafficLights::create({2, {5, 6}, true});
  lights[2] = TrafficLights::create({2, {3, 4}, true});
  lights[3] = TrafficLights::create({2, {1, 2}, true});

  //TODO TEST
  setLights(0, 1);
}

void loop(){
  delay(TIME_DELTA);

  //Loop through traffic lights
  for(int i = 0; i < NUM_OF_LIGHTS; i++){
    TrafficLights::Light light = lights[i];
    if(light.ticking){
      if(light.ticks <= 0){
         TrafficLights::setPattern(light, patterns[light.nextPattern]);
      }

      light.ticks--;
      lights[0] = light;
    }
  }

  time += TIME_DELTA;
}
