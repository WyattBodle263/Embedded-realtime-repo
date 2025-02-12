#include <M5Unified.h>
#include <Wire.h>
#include "Adafruit_seesaw.h"

Adafruit_seesaw ss;
bool gamepadConnected = false;  // Track if Gamepad QT is detected

// Dot positions
int blueDotX = 50, blueDotY = 50;  // Initial position of blue dot
int redDotX = 100, redDotY = 100;  // Initial position of red dot

// Speed of movement for each dot
int blueSpeed = 5, redSpeed = 5;

void setup() {
   M5.begin();
   Serial.begin(115200);
   Wire.begin();

   // Initialize Screen
   M5.Lcd.fillScreen(BLACK);
   M5.Lcd.setTextColor(WHITE);
   M5.Lcd.setTextSize(2);
   M5.Lcd.setCursor(50, 50);
   M5.Lcd.println("Initializing Gamepad QT...");

   // Try detecting Gamepad QT at multiple addresses
   if (ss.begin(0x49)) {
       gamepadConnected = true;
       Serial.println("Gamepad QT detected at 0x49");
   } else if (ss.begin(0x50)) {
       gamepadConnected = true;
       Serial.println("Gamepad QT detected at 0x50");
   } else {
       Serial.println("ERROR: Gamepad QT NOT detected!");
       M5.Lcd.setCursor(50, 100);
       M5.Lcd.setTextColor(RED);
       M5.Lcd.println("Gamepad QT NOT detected!");
       return;  // Do NOT freeze the program
   }

   // If Gamepad is detected, update the screen
   M5.Lcd.fillScreen(BLACK);
   M5.Lcd.setCursor(50, 50);
   M5.Lcd.setTextColor(GREEN);
   M5.Lcd.println("Gamepad QT Ready!");
}

void loop() {
   if (!gamepadConnected) {
       Serial.println("Gamepad QT is NOT connected. Waiting...");
       delay(1000);  // Avoid spamming Serial Monitor
       return;
   }

   // Read Joystick and Buttons
   uint16_t xAxis = ss.analogRead(2); //2 is left button
   uint16_t yAxis = ss.analogRead(3);
   uint32_t buttons = ss.digitalReadBulk(0xFF);

   // Print joystick and button states to Serial Monitor
   Serial.printf("Joystick X: %d, Y: %d\n", xAxis, yAxis);
   Serial.printf("Buttons: %08X\n", buttons);

   // Map joystick X and Y to screen coordinates, adjusting for dead zone
   int joystickThreshold = 100;  // Adjust the threshold for joystick response
   int joystickCenter = 512;  // Joystick center position (roughly)

   // Move the blue dot using the joystick
   if (xAxis > joystickCenter + joystickThreshold) {  // Joystick moved right
       blueDotX += blueSpeed;
   } else if (xAxis < joystickCenter - joystickThreshold) {  // Joystick moved left
       blueDotX -= blueSpeed;
   }

   if (yAxis > joystickCenter + joystickThreshold) {  // Joystick moved down
       blueDotY += blueSpeed;
   } else if (yAxis < joystickCenter - joystickThreshold) {  // Joystick moved up
       blueDotY -= blueSpeed;
   }

   // Boundaries for blue dot
   if (blueDotX < 0) blueDotX = 0;
   if (blueDotX > M5.Lcd.width() - 10) blueDotX = M5.Lcd.width() - 10;
   if (blueDotY < 0) blueDotY = 0;
   if (blueDotY > M5.Lcd.height() - 10) blueDotY = M5.Lcd.height() - 10;

   // Move the red dot using the D-pad buttons (A, B, X, Y)
   if (buttons & (1 << 0)) {  // Button A pressed (move up)
       redDotY -= redSpeed;
   }
   if (buttons & (1 << 1)) {  // Button B pressed (move down)
       redDotY += redSpeed;
   }
   if (buttons & (1 << 2)) {  // Button X pressed (move left)
       redDotX -= redSpeed;
   }
   if (buttons & (1 << 3)) {  // Button Y pressed (move right)
       redDotX += redSpeed;
   }

   // Boundaries for red dot
   if (redDotX < 0) redDotX = 0;
   if (redDotX > M5.Lcd.width() - 10) redDotX = M5.Lcd.width() - 10;
   if (redDotY < 0) redDotY = 0;
   if (redDotY > M5.Lcd.height() - 10) redDotY = M5.Lcd.height() - 10;

   // Draw the dots on the screen
   M5.Lcd.fillScreen(BLACK);  // Clear the screen
   M5.Lcd.fillRect(blueDotX, blueDotY, 10, 10, BLUE);  // Blue dot (joystick control)
   M5.Lcd.fillRect(redDotX, redDotY, 10, 10, RED);    // Red dot (button control)

   delay(100);  // Update the screen at a reasonable rate
}
