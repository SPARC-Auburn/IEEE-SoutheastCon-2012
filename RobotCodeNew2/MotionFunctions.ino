/**  Code for the defining the motion of the robot.
* 
* Defines the motions and trajectories that the robot 
* may make on the course. Included motions are turning,
* forward/backward, approach task, leave task, etc.
*
* Depends: MotorFunctions.ino
*
* Credits: Tyler Crumpton, Levi Smolin
*
*/

// Termination types:
#define ON_LINE       0 // Bot is on the line
#define OFF_LINE      1 // Bot is off the line
#define HIT_SWITCH    2 // Physical switch is triggered
#define AT_ANY        3 // Bot is at any type of turn (all types above this line do not count)
#define AT_T          4 // Bot is at a T-intersection
#define AT_LEFT       5 // Bot is at left turn
#define AT_RIGHT      6 // Bot is at right turn
#define AT_CENTER     7 // Bot is at the center



// Bot move actions
void setMove(int moveType)
{
  switch (moveType)
  {
    case STOP:
      forwardSpeed = 0;
      leftDelta    = 0;
      rightDelta   = 0;
      break;
    case TURN_LEFT:
      forwardSpeed = 0;
      leftDelta    = -TURN_SPEED * MAX_VELOCITY;
      rightDelta   = TURN_SPEED * MAX_VELOCITY;
      break;
    case TURN_RIGHT:
      forwardSpeed = 0;
      leftDelta    = TURN_SPEED * MAX_VELOCITY;
      rightDelta   = -TURN_SPEED * MAX_VELOCITY;
      break;
    case LEFT_FORWARD:
      forwardSpeed = 0;
      leftDelta    = TURN_SPEED * MAX_VELOCITY;
      rightDelta   = 0;
      break;
    case LEFT_BACK:
      forwardSpeed = 0;
      leftDelta    = -TURN_SPEED * MAX_VELOCITY;
      rightDelta   = 0;
      break;
    case RIGHT_FORWARD:
      forwardSpeed = 0;
      leftDelta    = 0;
      rightDelta   = TURN_SPEED * MAX_VELOCITY;;
      break;
    case RIGHT_BACK:
      forwardSpeed = 0;
      leftDelta    = 0;
      rightDelta   = -TURN_SPEED * MAX_VELOCITY;
      break;
    case MOVE_FORWARD:
      forwardSpeed = FULL_SPEED*MAX_VELOCITY;
      leftDelta    = 0;
      rightDelta   = 0;
      break;
    case MOVE_BACKWARD:
      forwardSpeed = -FULL_SPEED*MAX_VELOCITY;
      leftDelta    = 0;
      rightDelta   = 0;
      break;
    case MOVE_FAST:
      forwardSpeed = MAX_VELOCITY;
      leftDelta    = 0;
      rightDelta   = 0;
      break;
    case STOP_LEFT_WHEEL:
      leftDelta    = 0;
      break;
    case STOP_RIGHT_WHEEL:
      rightDelta   = 0;
      break;
      
    default:
      forwardSpeed = FULL_SPEED*MAX_VELOCITY;
      leftDelta    = 0;
      rightDelta   = 0;
  }
  updateMotors();
}


void followLine()
{
  
  // Read calibrated front sensor values and obtain a measure of the line position from 0 to NUM_SENSORS-1

  unsigned int pOsitIon = fSensors.readLine(fSensorValues,QTR_EMITTERS_ON,1);
 
  inputPID = pOsitIon;            // set PID input to position of line
  
  lfPID.Compute();                // compute correction, store in outputPID
  
  if (outputPID < 0)
  {
    forwardSpeed = FULL_SPEED*MAX_VELOCITY;
    leftDelta = 0;//outputPID;         // sets right wheel's speed variation
    rightDelta = outputPID;//outputPID;
  }
  else
  {
    forwardSpeed = FULL_SPEED*MAX_VELOCITY;
    rightDelta  = 0;        // sets left wheel's speed variation
    leftDelta   = -outputPID;//-outputPID;
  }
  updateMotors();
}

//Arguments are number of clicks to turn and true if leftwheel, false if right
void turnWheel(int clicks, boolean leftWheel)
{
  int nClicks=clicks;
  boolean lastColor;
  int wheel=!leftWheel;
  
  if(leftWheel)
  {
    if(nClicks>0){setMove(LEFT_FORWARD);}
    else if(nClicks<0){setMove(LEFT_BACK);nClicks*=-1;}  
  }
  else 
  {
    if(nClicks>0){setMove(RIGHT_FORWARD);}
    else if(nClicks<0){setMove(RIGHT_BACK);nClicks*=-1;}      
  }
  
  encoders.readCalibrated(encoderValues,QTR_EMITTERS_ON);
  lastColor=(encoderValues[wheel]>500);
  
  while(true)
  {
    encoders.readCalibrated(encoderValues,QTR_EMITTERS_ON);
    if(nClicks>0) 
    {
      
      if(lastColor)
      {
        if(encoderValues[wheel]<=450)
        {
          lastColor=!lastColor;
          nClicks--;
        }  
      }
      else
      {
        if(encoderValues[wheel]>=550)
        {
          lastColor=!lastColor;
          nClicks--;
        }
      }
    }
    else {break;}        
  }
  
  if(leftWheel){setMove(STOP_LEFT_WHEEL);}
  else {setMove(STOP_RIGHT_WHEEL);} 
}


//true in the argument means left then right, false means right then left
void turnLeftThenRight(int rightC, int leftC, boolean defaultWay)
{
  if(defaultWay){turnWheel(leftC,1);turnWheel(rightC,0);}
  else {turnWheel(rightC,0);turnWheel(leftC,1);}
}


//Turn both wheels in place clicks times, if clicks is negative that means turn left, if positive, turn right
void turnInPlace(int clicks)
{
  int rClicks=clicks, lClicks=clicks;
  boolean lLastColor,rLastColor;
  
  encoders.readCalibrated(encoderValues,QTR_EMITTERS_ON);
  
  if(clicks > 0)
  {
    setMove(TURN_RIGHT);
    rClicks=clicks;
    lClicks=clicks;
  }
  else if(clicks < 0)
  {
    setMove(TURN_LEFT);
    rClicks=-clicks;
    lClicks=-clicks;
  }
  
  lLastColor = (encoderValues[LEFT]>500);
  rLastColor = (encoderValues[RIGHT]>500);
  
  while(true)
  {
    encoders.readCalibrated(encoderValues,QTR_EMITTERS_ON);
 
    
    if(lClicks>0) 
    {
      
      if(lLastColor)
      {
        if(encoderValues[LEFT]<=450)
        {
          lLastColor=!lLastColor;
          lClicks--;
        }  
      }
      else
      {
        if(encoderValues[LEFT]>=550)
        {
          lLastColor=!lLastColor;
          lClicks--;
        }
      }
        
    }
    else {setMove(STOP_LEFT_WHEEL);if(rClicks<=0){setMove(STOP_RIGHT_WHEEL);break;}}    
    
    if(rClicks>0) 
    {
      
      if(rLastColor)
      {
        if(encoderValues[RIGHT]<=450)
        {
          rLastColor=!rLastColor;
          rClicks--;
        }  
      }
      else
      {
        if(encoderValues[RIGHT]>=550)
        {
          rLastColor=!rLastColor;
          rClicks--;
        }
      }
        
    }
    else {setMove(STOP_RIGHT_WHEEL);if(lClicks<=0){setMove(STOP_LEFT_WHEEL);break;}}
      
  } 
}

void encoderMove(int clicks)
{
  int rClicks=clicks,lClicks=clicks;
  boolean lLastColor,rLastColor;
  float prop;                        //A proportionality constant
  
  if(lClicks>0){setMove(MOVE_FORWARD);}
  else if(lClicks<0){setMove(MOVE_BACKWARD);rClicks*=-1;lClicks*=-1;}      
  
  encoders.readCalibrated(encoderValues,QTR_EMITTERS_ON);
  lLastColor=(encoderValues[LEFT]>500);
  rLastColor=(encoderValues[RIGHT]>500);
  
  while(true)
  {
    prop = 20*(lClicks-rClicks);
    
    if(lClicks>0)leftDelta=prop;
    if(rClicks>0)rightDelta=-prop;
    if(lClicks>0||rClicks>0)updateMotors();    
    
    encoders.readCalibrated(encoderValues,QTR_EMITTERS_ON);
    if(lClicks>0) 
    {
      
      if(lLastColor)
      {
        if(encoderValues[LEFT]<=450)
        {
          lLastColor=!lLastColor;
          lClicks--;
        }  
      }
      else
      {
        if(encoderValues[LEFT]>=550)
        {
          lLastColor=!lLastColor;
          lClicks--;
        }
      }
    }
    else {setMove(STOP_LEFT_WHEEL);if(rClicks<=0){setMove(STOP_RIGHT_WHEEL);break;}} 


    if(rClicks>0) 
    {
      
      if(rLastColor)
      {
        if(encoderValues[RIGHT]<=450)
        {
          rLastColor=!rLastColor;
          rClicks--;
        }  
      }
      else
      {
        if(encoderValues[RIGHT]>=550)
        {
          rLastColor=!rLastColor;
          rClicks--;
        }
      }
    }
    else {setMove(STOP_RIGHT_WHEEL);if(lClicks<=0){setMove(STOP_LEFT_WHEEL);break;}}     
  } 
}







