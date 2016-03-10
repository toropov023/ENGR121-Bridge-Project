/*
  Authors:

  Kirill Toropov
  Nolan Caissie
  Jeff Travis
  Theresa Jensen

  Date: 2016-03-09

  Engineering Bridge Project control system prototype
*/

#include <Stepper.h>
#include <NewPing.h>

//Define pin locations for Ultrasonic Sensor
#define TRIGGER_PIN  12   //Sensor Trigger Pin
#define ECHO_PIN     13   //Sensor echo pin
#define MAX_DISTANCE 100  //Set max distance to 100cm

//Define pin locations for motor control buttons
#define F_BTN        2    //Forwards button
#define B_BTN        3    //Backwards button

//Define pin locations for motor speed potentiometer
// and motor enable transistor
#define POTEN        A5   //Speed potentiometer
#define MOTOR_TR     7    //H-bridge enable transistor

//Define pin locations for traffic light LEDs
#define RED_LED      4    //Red LED
#define YELLOW_LED   5    //Yellow LED
#define GREEN_LED    6    //Green LED

//Define pin locations for Custom Made switch
#define PRES_SENSOR  A0

//The motor has 512 steps per a single revolution
const int stepsPerRevolution = 512;
 
// initialize the stepper library on pins 8 through 11:
Stepper myStepper(stepsPerRevolution, 8, 9, 10, 11);

//Create an instance of the NewPing proximity class
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);     

//Old speed of the motor - used to check with the new speed
// taken from the potentiometer
int oldSpeed = 0;
 
void setup() {
  //Set initial speed
  myStepper.setSpeed(10);
  
  //Initialize the serial port
  Serial.begin(9600);

  //Set up the pin modes
  pinMode(F_BTN, INPUT);
  pinMode(B_BTN, INPUT);
  
  pinMode(PRES_SENSOR, INPUT);

  pinMode(MOTOR_TR, OUTPUT);

  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
}
 
void loop() {
  //Constant delay for the whole system
  delay(100);

  //Get sonar distance
  int dis = getSonarDistance();

  //Loop the trafic funtion every time
  // if the distance <= 20cm then the lights will start to change 
  // to yellow and then red
  loopTrafficLights(dis <= 20);

  //Read the custom made pressure sensor value
  boolean presSensor = digitalRead(PRES_SENSOR);
  if(presSensor)
    Serial.println("Presssure sensor is working!");

  //Check if speed of the motor should be changed
  checkMotorSpeed();

  //Call the move motor function
  moveMotor(presSensor);
}

//Return the proximity sensor distance in cm
int getSonarDistance(){  
  int uS = sonar.ping(); //Get ping value
  return (uS / US_ROUNDTRIP_CM); //Convert to cm
}

//Change the motor speed depending on the potentiometer value
void checkMotorSpeed(){
  int potenValue = analogRead(POTEN); //Read poten. value
  int sSpeed = map(potenValue, 0, 1023, 0, 80); //Map it from (0, 1023) to (0, 80) 
                                                // (e.i. converts to a value from 0 to 80)
  //If the newly read poten. value is not the same as the old speed,
  // change it
  if(oldSpeed != sSpeed && sSpeed > 0){
    Serial.print(sSpeed > oldSpeed ? "Speeding Up! : " : "Slowing Down! : ");
    Serial.println(sSpeed);

    //Set the motor speed to the new value
    myStepper.setSpeed(sSpeed);

    //Make sure we keep track of the changed speed
    // i.e. update the old speed
    oldSpeed = sSpeed;
  }
}

//Method used to move the motor
// Pass if the custom sensor is pressed
void moveMotor(boolean presSensor){
  //Get forwards button value
  boolean forward = digitalRead(F_BTN) == HIGH;
  //Get backwards button value
  boolean backward = digitalRead(B_BTN) == HIGH;

  //If the custom sensor is not pressed
  // and we are moving forwards or backwards
  if(!presSensor && (forward || backward)){
    Serial.print("Moving ");
    Serial.print(forward ? "forward" : "backward");
    Serial.print(" with speed ");
    Serial.println(oldSpeed);

    //Switch the motor enable transiston on
    digitalWrite(MOTOR_TR, HIGH);

    //Move the motor while the buttons are pressed
    while(forward || backward){
      //Update the buttons
      forward = digitalRead(F_BTN) == HIGH;
      backward = digitalRead(B_BTN) == HIGH;

      //Move by a single step every loop
      myStepper.step(forward ? 1 : -1);
    }
  }

  //Switch the motor enable transiston off
  digitalWrite(MOTOR_TR, LOW);
}

//Keep track of the number of times this funtion was called
// when the yellow/red lights were changing
int lightsTicks = 0;
//Are the light changing (false if it's just a blinking green)
boolean lightAreChanging = false;
//Is the green led on or off? Used to make the blinking effect
boolean greenOn = false;

//The function is used to control traffic lights
// If true is passed, the lights will change to yellow and then red
void loopTrafficLights(boolean change) {
  //Should we change the lights?
  if(change && !lightAreChanging)
    lightAreChanging = true;

  lightsTicks++; //Keep track of the time
    
  //If the lights are in the yellow/red state
  if(lightAreChanging){
    //Has it been less than 3 seconds?
    if(lightsTicks <= 30){
      //Turn yellow LED on
      setLEDs(0, 1, 0);
    } else if(lightsTicks <= 100){ //Otherwise, less than 10 seconds?
      //Set red LED on
      setLEDs(1, 0, 0);
    } else { //Otherwise we want to blink the green 
      lightAreChanging = false;  //Reset the yellow/red state variable
      lightsTicks = 0; //Reset the yellow/red state time
    }    
  } else if(lightsTicks >= 5){ //If the lights are not in the yellow/red state and time is every .5 second
    greenOn = !greenOn; //Toggle green LED state
    setLEDs(0, 0, greenOn); //Set the green LED on or off
    lightsTicks = 0; //Reset time
  }
}

//The function is used to set the traffic lights
void setLEDs(boolean r, boolean y, boolean g){
  digitalWrite(RED_LED, r);
  digitalWrite(YELLOW_LED, y);
  digitalWrite(GREEN_LED, g);
}
