
/*
 * Project chicken gate
 */

#include <Wire.h>
#include "rgb_lcd.h"
#include <CytronMotorDriver.h>


rgb_lcd display;

const int colorR = 128;
const int colorG = 128;
const int colorB = 255;

const int LightLEDPIN = A0;


enum manualAutoState{
  ma_manual,
  ma_auto
 };

enum motorState{
  m_stopped,
  m_opening,
  m_closing
};

class Motor{
  private:
    motorState _state;
    unsigned long _startTime;
    unsigned long _runTime;
    CytronMD* _motor;
  public:
    Motor( CytronMD* motor ){
      _state = m_stopped;
      _startTime = 0;
      _runTime = 20 *1000; // Run for 20 seconds   
      _motor = motor;
    }
    motorState getState(){
      return _state;
    }
    void motorStop(){
      Serial.println("----------Stopping the motor");
      _state = m_stopped;
      _motor->setSpeed(0);  // Stop Motor
    }

    void motorOpen(){
      Serial.println("-----------Opening the gate");
      _startTime = millis();
      _state = m_opening;
      _motor->setSpeed(255);  // Run forward at full speed.
    }
    void motorClose(){
      Serial.println("----------Closing the gate");
      _startTime = millis();
      _state = m_closing;
      _motor->setSpeed(-255);  // Run reverse at full speed.
    }

    boolean checkMotorStop(){
      if( _state != m_stopped ){
        unsigned long currentTime = millis();
        // Check for time overflow
        if( currentTime < _startTime ){
          _startTime = currentTime;
        }else{
          if( (currentTime - _startTime) > _runTime ){
            return true;
          }
        }
      }
      return false;
    }
    
};

enum dayNightState{
  dn_unknown,
  dn_day,
  dn_night
};

class LightLED{
  private:
    int _pin;
    int _value;
    dayNightState _dnState;
    int _dayToNightChange;
    int _nightToDayChange;

  public:
    LightLED(){
      _pin = LightLEDPIN;
      _value = 0;
      _dnState = dn_unknown;
      _nightToDayChange = 600; // When value rises above 600 -> switch to dn_day
      _dayToNightChange = 500; // Whn value drops below 500 -> swithc to dn_night
    }
    int getPin(){
      return _pin;
    }
    int getValue(){
      return _value;
    }
    int updateValue(){
      _value = analogRead(_pin);
      return _value;
    }
    dayNightState getState(){
      return _dnState;
    }
    void setState( dayNightState newState ){
      _dnState = newState;
    }
    
    dayNightState getNewStateFromValue(){
      if( _dnState == dn_unknown ){
        if( _value > (_nightToDayChange + _dayToNightChange)/2 ){
          return dn_day;
        }else{
          return dn_night;
        }
      }else{
        if( _dnState == dn_day && _value <= _dayToNightChange ){
          return dn_night;
        }else if( _dnState == dn_night && _value >= _nightToDayChange ){
          return dn_day;
        }
        return _dnState;
      }
    }
    
};

class ManualAutoSwitch {
  private:
    manualAutoState _state;
    int _pin = 7;
  public:
    ManualAutoSwitch(){
      _state = ma_manual;
      _pin = 7;
    }
    manualAutoState getState(){
      return _state;
    }
    manualAutoState updateState(){
      _state = (manualAutoState)digitalRead(_pin);
      return _state;
    }
    void setPin(){
      pinMode(_pin, INPUT);
    }
};

enum buttonMode{
  m_none,
  m_open,
  m_close
};

enum buttonState {
  b_off,
  b_on
 };

class OpenCloseButton {
    private:
      int _pin;
      buttonMode _mode;
      buttonState _state;
      buttonState _lastState;
    public:
      OpenCloseButton(){
        _mode = m_open;
        _pin = 8;
        _state = b_off;
        _lastState = b_off;
      }
      void setPin(){
        pinMode(_pin, INPUT);
      }
      buttonMode getMode(){
        return _mode;
      }
      buttonState getState(){
        _state = (buttonState)digitalRead(_pin);
        return _state;
      }
      buttonMode getPressed(){
        _state = (buttonState)digitalRead(_pin);
        if( _state == b_on && _lastState == b_off ){  // Button is pressed
          _lastState = b_on;
          return _mode;
        }else if( _state == b_off && _lastState == b_on ){ // Button is released
          _lastState = b_off;
          if( _mode == m_open ) _mode = m_close;
          else _mode = m_open;
          return m_none;
        }else{
          return m_none;  // Button not on
        }
          
      }
};


CytronMD cytron_motor(PWM_DIR, 3, 2);  // PWM = Pin 3, DIR = Pin 2.
Motor openCloseMotor(&cytron_motor);
LightLED lightSensor;
ManualAutoSwitch manualAutoSwitch;
OpenCloseButton openCloseButton;
String Line1Message = "Chicken Gate!";


void setup() {
  // Initialize
  Serial.begin(9600);
//  while (!Serial);             // Leonardo: wait for serial monitor
  Serial.println("\nChicken gate!");
  Serial.print("Motor State ");
  Serial.print(openCloseMotor.getState());
  Serial.print("\nLight Sensor PIN ");
  Serial.println(lightSensor.getPin());
  manualAutoSwitch.setPin();

  
  display.begin(16, 2);

  display.setRGB(colorR, colorG, colorB);
  mainLCDStatus();
//  display.print("Chicken Gate!");

}


void loop() {
 // put your main code here, to run repeatedly:
  manualAutoState oldMAState = manualAutoSwitch.getState();
  manualAutoState newMAState =  manualAutoSwitch.updateState();
  if( oldMAState != newMAState ){
    Serial.print("Manual/Auto changed! ");
    openCloseMotor.motorStop();
  }
  Serial.print("ManualAuto state: ");
  if( newMAState == ma_manual )  Serial.println("Manual");
  else Serial.println("Auto");
  if( newMAState == ma_auto ){
    Serial.print("Auto mode - Light Sensor value ");
    lightSensor.updateValue();
    Serial.println(lightSensor.getValue());
    dayNightState newState = lightSensor.getNewStateFromValue();
    dayNightState oldState = lightSensor.getState();
    if( newState != oldState){
      // Open or close the door based on light change
      if( newState == dn_day ){
//        mainLCDStatus("Opening gate");
        Line1Message = "Opening - Auto";
        openGate();
      }else{
//        mainLCDStatus("Closing gate");
        Line1Message = "Closing - Auto";
        closeGate();
      }
      lightSensor.setState( newState );
    }else{
      testMotorStop();
    }
    
  }else{
    Serial.println("Manual mode ");
    testMotorStop();
    buttonMode mode = openCloseButton.getPressed();
    if( mode == m_open ){
        Serial.println("Opening gate");
        Line1Message = "Opening - Man.";
//        mainLCDStatus("Opening gate - manual");
        openGate();      
    }else if( mode == m_close ){
        Line1Message = "Closing - Man.";
        Serial.println("Closing gate");
//        mainLCDStatus("Closing gate - nanual");
        closeGate();
    }
  }
  mainLCDStatus();
  delay(1000);
}

void testMotorStop(){
  if( openCloseMotor.checkMotorStop() ){
      Line1Message = "Ready";
//       mainLCDStatus("Ready");
      openCloseMotor.motorStop();
  }
}
void openGate(){
  openCloseMotor.motorOpen();
}

void closeGate(){
  openCloseMotor.motorClose();
}

void mainLCDStatus( ){
  display.clear();
  display.setCursor(0, 0);
  display.print(Line1Message);
  display.setCursor(0, 1);
  manualAutoState state = manualAutoSwitch.getState();
  String mode = "Auto";
  if( state == ma_manual ){
    mode = "Man.";
  }
  lightSensor.updateValue();
  dayNightState dayNight = lightSensor.getNewStateFromValue();
  String dayNightString = "Night";
  if( dayNight == dn_day ){
    dayNightString = "Day";
  }
  display.print(mode + " " + "L:" + lightSensor.getValue() + " " + dayNightString);

}
