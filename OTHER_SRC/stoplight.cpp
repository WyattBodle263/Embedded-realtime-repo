#include <Arduino.h>
#include <M5Core2.h>

//Program-wide variables
enum State {S_GREEN, S_YELLOW, S_RED, S_BLACK};
enum Mode {M_NORMAL, M_EMERGENCY};

//Initalize variables
static State state = S_GREEN;
static Mode mode = M_NORMAL;
static unsigned long lastTS = 0;
static bool stateChangedThisLoop = false;
static unsigned long timeChangeInterval = 200;

void setup(){
  M5.begin();

}

void loop(){
  M5.update();
  //STATE TRANSITIONS

  if(M5.BtnA.wasPressed()){
    if(mode == M_NORMAL){
      mode = M_EMERGENCY;
      state = S_BLACK;
    }else{
      mode = M_NORMAL;
    }
  }

  if(millis() >= lastTS + timeChangeInterval){
    lastTS = millis();
    stateChangedThisLoop = true;

    if(mode == M_NORMAL){
      if(state == S_GREEN){
        state = S_YELLOW;
      }else if(state == S_YELLOW || state == S_BLACK){
        state = S_RED;
      }else{
        state = S_GREEN;
      }
    }else{
      if (state == S_RED || state == S_GREEN || state == S_YELLOW){
        state = S_BLACK;
      }else{
        state = S_RED;
      }
    }
  }

  if(stateChangedThisLoop){
//STATE EXECUTION/OUTPUT
    if(state == S_GREEN){
      M5.Lcd.fillScreen(GREEN);
    }else if (state == S_YELLOW){
      M5.Lcd.fillScreen(YELLOW);
    }else if (state == S_RED){
      M5.Lcd.fillScreen(RED);
    }else if (state == S_BLACK){
      M5.Lcd.fillScreen(BLACK);
    }
  }

  //Reset the state change flag
  stateChangedThisLoop = false;

}