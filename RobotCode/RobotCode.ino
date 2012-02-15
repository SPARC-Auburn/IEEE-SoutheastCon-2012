/**  Main code for operation of SECON robot.
* 
* Includes navigation system and line-following system, as well as calibration
* and setup routines. For reference, high values from line sensor are black, 
* low are white.
*
* Depends: MotionFunctions.ino
*          MotorFunctions.ino
*          TaskSensorFunctions.ino
*          PID_v1.h
*          PololuQTRSensors.h
*
* Credits: Tyler Crumpton
*
*/
#include <PID_v1.h>
#include <PololuQTRSensors.h>
#include <EEPROM.h>


//#define CALIBRATE_MOTORS   // Run in motor calibration mode.
//#define CALIBRATE_ENCODERS  // Run in encoder calibration mode.
//#define DEBUG_MOTOR_SPEED  // Display motor speeds in Serial window.
//#define DEBUG_LINE         // Display line sensor data
//#define DEBUG_COURSE       // Display info on robot's course location

#define FTA_CYCLE_SIZE  7 // Defines the size (in bytes) of the ftaCycle struct.

#define NUM_SENSORS   8     // number of sensors used on each array
#define TIMEOUT       2500  // waits for 2500 us for sensor outputs to go low
#define MID_LINE      3500  // value of sensor when line is centered (0-7000)
#define WHITE_LINE    1     // '1' = white line, '0' = black line

#define REFLECT_THRESHOLD 750  // part of 1000 at which line is not found

// Direction Definitions
#define LEFT    true    // Left direction 
#define RIGHT   false   // Right direction

// Movement Actions
#define STOP          0 // Bot completely stops
#define TURN_LEFT     1 // Bot turns ~90 degrees left
#define TURN_RIGHT    2 // Bot turns ~90 degrees right
#define MOVE_FORWARD  3 // Bot moves forward at FULL_SPEED

#define MAX_VELOCITY  255  // Maximum motor velocity

#define FULL_SPEED    0.40 // Fraction of MAX_VELOCITY that is 'full' speed 
#define TURN_SPEED    0.25 // Fraction of MAX_VELOCITY that is 'turning' speed 
//#define TURN_TIME     570  // Number of mSeconds to turn robot 90 degrees
#define TURN_STOPS    10   // Number of encoder stops for a bot 90 degree turn

#define TL_NEGONE_COUNT 200
#define TL_ZERO_COUNT   200
#define TL_ONE_COUNT    200
#define TL_TWO_COUNT    200

#define ML_ZERO_COUNT   200
#define ML_THREE_COUNT  400
#define ML_FOUR_COUNT   400
#define ML_SEVEN_COUNT  400

// Maximum difference in wheel speeds when line-following
#define MAX_PID_DELTA (1-FULL_SPEED)*MAX_VELOCITY  
// PWM offset for motor speeds to be equal (Left motor is faster = +)
#define MOTOR_OFFSET  0

union u_double
{
  byte b[4];
  float dval;
  int ival;
};  //A structure to read in floats from the serial ports
  

// Course Locations
boolean leftRightLoc = RIGHT;  // (RIGHT or LEFT)
short   taskLoc      = -1;     // (-1 to 2)
short   mainLoc      = 0;      // (0 to 7)

// PID Coeffs
double KP = .015; //.015
double KI = .0001; //.0001
double KD = .001; //.001

// Movement speeds
double leftDelta = 0;
double rightDelta = 0;
double forwardSpeed = 0;

int delayCounter = 0;

// Change these pins when you need to
unsigned char fSensorPins[] = {33,35,37,39,41,43,45,47};
unsigned char lEncoderPins[] = {46};
unsigned char rEncoderPins[] = {48};

//unsigned char rSensorPins[] = {2,3,4,5,6,7,8,9};
#define LEFT_PWM_PIN   10
#define LEFT_DIR_PIN   11
#define RIGHT_PWM_PIN  12
#define RIGHT_DIR_PIN  13
//#define CAL_LED_PIN    5
#define RELAY_K1_PIN   52
#define RELAY_K2_PIN   53
#define TEST_LED_PIN   51
#define LEFT_ENC_PIN   36
#define RIGHT_ENC_PIN  50


// Container for the "follow, terminate, action" cycle values.
struct ftaCycle{
  byte follow;      // "Line-following" vs. "simple encoder travel"
  byte terminate;   // "At right turn", "at left turn", "off the line", etc.
  byte action;      // "turn in place", "turn left wheel then right", "turn right wheel then left"
  int leftAmount;   // Number of encoder clicks to turn left wheel forward (negative is backward)
  int rightAmount;  // Number of encoder clicks to turn right wheel forward (negative is backward)
};


// Sensors (f)(c)0 through (f)(c)7 are connected to (f)(c)SensorPins[]
PololuQTRSensorsRC fSensor(fSensorPins, NUM_SENSORS, TIMEOUT); 
//PololuQTRSensorsRC rSensor(rSensorPins, NUM_SENSORS, TIMEOUT); 
PololuQTRSensorsRC lEncoder(lEncoderPins, 1, TIMEOUT);//, LEFT_ENC_PIN);
PololuQTRSensorsRC rEncoder(rEncoderPins, 1, TIMEOUT);//, RIGHT_ENC_PIN); 
unsigned int fSensorValues[NUM_SENSORS];
//unsigned int rSensorValues[NUM_SENSORS];
unsigned int lEncoderValues[1];
unsigned int rEncoderValues[1];


// Setup PID computation
double setpointPID, inputPID, outputPID;
PID lfPID(&inputPID, &outputPID, &setpointPID, KP, KI, KD, DIRECT);

void setup()
{
  Serial.begin(9600);        // Begin serial comm for debugging  
  delay(500);                // Wait for serial comm to come online
  
  // Setup pin IO:
  pinMode(LEFT_PWM_PIN, OUTPUT);
  pinMode(LEFT_DIR_PIN, OUTPUT);
  pinMode(RIGHT_PWM_PIN, OUTPUT);
  pinMode(RIGHT_DIR_PIN, OUTPUT);
  //pinMode(CAL_LED_PIN, OUTPUT);
  pinMode(RELAY_K1_PIN, OUTPUT);
  pinMode(RELAY_K2_PIN, OUTPUT);
  pinMode(TEST_LED_PIN, OUTPUT);
  
  digitalWrite(TEST_LED_PIN, LOW);

  delay(50);
  
  digitalWrite(RELAY_K1_PIN, 0);
  digitalWrite(RELAY_K2_PIN, 0);
  
  motorCalibrate();          // Does nothing ifdef CALIBRATE_MOTORS
  
  setpointPID = MID_LINE;    // Set the point we want our PID loop to adjust to
    
  lfPID.SetMode(AUTOMATIC);  // turn on the PID
  lfPID.SetOutputLimits(-MAX_PID_DELTA, MAX_PID_DELTA); // force PID to the range of motor speeds. 
  
  calibrateSensors(); // Calibrate line sensors
  
  delay(5000);
  
  // Start movement (currently starting at mainLoc 'M0')
  mainLoc = 0;
  setMove(MOVE_FORWARD);
 
}




void dynamic_PID() // Sets the PID coefficients dynamically via a serial command interface...
{
  char command;			//The command coming in
  int i,j;			//Looping variables
  u_double Vals[3];		//Holds floating or double variables recieved
  
   //Variables to hold possible incoming data
  char follow,terminate,action;     
  int left_amnt,right_amnt,start_pos;
  float p_val,i_val,d_val;
  int segment;
          ftaCycle currentSegment;
          byte tempMSB,tempLSB;
  if(Serial.available())		//Are there messages coming in?
  {
    delay(100);				//Wait a tad to let them in
      command=Serial.read();		//Read the command prefix
      switch (command)			
      {
        case 'p':			//Just a test command
          Serial.print("Print!");
          break;
        case 'c':                      //Course Variables
          if(Serial.available()>=9)	//We are getting three floats so wait til we get all their data
          {
            follow=Serial.read();
            terminate=Serial.read();
            action=Serial.read();
            Serial.print(follow);
            Serial.print("\n");
            Serial.print(terminate);
            Serial.print("\n");
            Serial.print(action);
            Serial.print("\n");
            for(i=0;i<3;i++)		//Then read it in
            {
              for(j=0;j<2;j++)
              {
                Vals[i].b[j]=Serial.read();
              }
            }
            segment=Vals[0].ival;
            left_amnt=Vals[1].ival;
            right_amnt=Vals[2].ival;
            Serial.print(segment);
            Serial.print("\n");
            Serial.print(left_amnt);
            Serial.print("\n");
            Serial.print(right_amnt);
            Serial.print("\n");
            
          }
          break;
        
        case 'g':			//Global variables
          if(Serial.available()>=14)	//We are getting three floats and an int so wait til we get all their data
          {
            for(i=0;i<3;i++)		//Then read it in
            {
              for(j=0;j<4;j++)
              {
                Vals[i].b[j]=Serial.read();
              }
              Serial.print("\n");		//For now, we want to read them back out to make sure it worked
              Serial.print(Vals[i].dval);
              Serial.print("\n");Serial.flush();
            }
            for(j=0;j<2;j++)
            {
              Vals[3].b[j]=Serial.read();
            }
            Serial.print("\n");		//For now, we want to read them back out to make sure it worked
            Serial.print(Vals[3].ival);
            Serial.print("\n");Serial.flush();
          }
          break;
       }
       
       //Now store the values in EEProm
      switch (command)			
      {
        case 'c': 
        /*  EEPROM.write(segment * FTA_CYCLE_SIZE,follow);
          EEPROM.write((segment * FTA_CYCLE_SIZE) + 1,terminate);
          EEPROM.write((segment * FTA_CYCLE_SIZE) + 2, action);
          EEPROM.write((segment * FTA_CYCLE_SIZE) + 3, left_amnt);
          EEPROM.write((segment * FTA_CYCLE_SIZE) + 5, right_amnt);
            // --- Retrieve FTA information from EEPROM: ---

        currentSegment.follow = EEPROM.read(segment * FTA_CYCLE_SIZE);          // Read the follow type
        currentSegment.terminate = EEPROM.read((segment * FTA_CYCLE_SIZE) + 1); // Read the termination type
        currentSegment.action = EEPROM.read((segment * FTA_CYCLE_SIZE) + 2);    // Read the action type
        tempMSB = EEPROM.read((segment * FTA_CYCLE_SIZE) + 3); // Read MSB of leftAmount
        tempLSB = EEPROM.read((segment * FTA_CYCLE_SIZE) + 4); // Read LSB of leftAmount
        currentSegment.leftAmount = word(tempMSB,tempLSB);     // Combine MSB and LSB
        tempMSB = EEPROM.read((segment * FTA_CYCLE_SIZE) + 5); // Read MSB of rightAmount
        tempLSB = EEPROM.read((segment * FTA_CYCLE_SIZE) + 6); // Read LSB of rightAmount
        currentSegment.rightAmount = word(tempMSB,tempLSB);    // Combine MSB and LSB
                Serial.print("\n");
                        Serial.print("\n");
        Serial.print(currentSegment.follow);
        Serial.print("\n");
        Serial.print(currentSegment.terminate);
                Serial.print("\n");
        Serial.print(currentSegment.action);
                Serial.print("\n");
        Serial.print(currentSegment.leftAmount);        Serial.print("\n");
        Serial.print(currentSegment.rightAmount);
                Serial.print("\n");
                        Serial.print("\n");*/
        break;
        
        case 'g':
        break;
      }
  }
}

void loop()
{ 
  dynamic_PID();
}


//Take a reading from the task sensors and makes L/R decision 
void takeReading()
{
  switch (mainLoc)
  {
    case 1: // Voltage Task
      leftRightLoc = readVoltage();
      break;
    case 2: // Capacitance Task 
      leftRightLoc = readCapacitance();
      break;
    case 5: // Temperature Task
      leftRightLoc = readTemperature();
      break;
    case 6: // Waveform Task
      leftRightLoc = readWaveform();
      break;
    default:
      // Should not occur, means that takeReading was called at the wrong place.
      break;
  }
  digitalWrite(RELAY_K1_PIN, 0);
  digitalWrite(RELAY_K2_PIN, 0);
}



void increaseMainLoc()
{
  ++mainLoc;    // Increment mainLoc
  mainLoc %= 8; // Make sure mainLoc is never > 7
}

void calibrateSensors()
{
  boolean toggle = true;
  setMove(TURN_LEFT);
  // Calibrate sensors  (robot must be fully on the line)
  // Note: still needs calibration motor routine
  for (int i = 0; i < 50; i++)  // Make the calibration take about 5 seconds
  {
    // Reads both sensors 10 times at 2500 us per read (i.e. ~25 ms per call)
    fSensor.calibrate();
    lEncoder.calibrate();
    rEncoder.calibrate();
    digitalWrite(RELAY_K1_PIN, toggle); // Make sound!
    toggle = !toggle;
    //rSensor.calibrate();
  }
  
  toggle = true;
  setMove(TURN_RIGHT);
  for (int i = 0; i < 50; i++)  // Make the calibration take about 5 seconds
  {
    // Reads both sensors 10 times at 2500 us per read (i.e. ~25 ms per call)
    fSensor.calibrate();
    lEncoder.calibrate();
    rEncoder.calibrate();
    digitalWrite(RELAY_K1_PIN, toggle); // Make sound!
    toggle = !toggle;
    //rSensor.calibrate();
  }  
  setMove(STOP);
}


