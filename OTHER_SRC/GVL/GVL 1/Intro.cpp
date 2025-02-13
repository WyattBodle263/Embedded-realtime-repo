#include <Arduino.h>
#include <M5Core2.h>

//Program-wide variables
static bool ledIsOn = false;
static unsigned long count = 0;
static unsigned long  lastTS = 0;
static unsigned long secondsOn = 0;
static HotZone hz(0,0,320,240);

void setup(){
  M5.begin();


  Serial.println("Hello, World!");

  //Print "Hello world!" to m5Core display
  M5.Lcd.setTextSize(3);
  M5.Lcd.fillScreen(LAVENDAR);
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.println("Hello Wyatt");

  //Initalize LED
  M5.Axp.SetLed(ledIsOn);
  ledIsOn = !ledIsOn;
  lastTS = millis();

}

void loop(){
  //Check for button input
  M5.update();
  if(M5.BtnA.wasPressed()){
    Serial.printf("\tYou Pressed button A %lu times. \n", ++count);
  }

  if(M5.Touch.ispressed()){
    TouchPoint tp = M5.Touch.getPressPoint();
    if(hz.inHotZone(tp)){
    Serial.printf("Pressed Screen (%d, %d) \n", tp.x, tp.y);
    }
  }

  if(millis() >= lastTS + 1000){
    M5.Axp.SetLed(ledIsOn);
    ledIsOn = !ledIsOn;
    lastTS = millis();
  }
}