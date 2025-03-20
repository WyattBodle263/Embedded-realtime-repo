#include <M5Unified.h>

float accX, accY, accZ;
int cursorX = 160, cursorY = 120; // Initial cursor position at the center of the screen
bool isDuckHit = false;

struct Duck {
  int x, y, width, height;
  bool isAlive;
};

// Duck initialization
Duck duck = {100, 60, 50, 30, true};

bool checkDuckHit(int cursorX, int cursorY, Duck duck);

void setup() {
  // Initialize M5Stack Core2
  M5.begin();
  M5.Lcd.setRotation(3); // Set landscape mode
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(1);
  
  // Set up accelerometer
  M5.Imu.init();
}

void loop() {
  M5.Imu.getAccelData(&accX, &accY, &accZ);

  // Scale the accelerometer data to move the cursor
  cursorX = map(accX * 100, -50, 50, 0, 320); // Adjust for the screen width
  cursorY = map(accY * 100, -50, 50, 0, 240); // Adjust for the screen height

  // Bound the cursor within screen limits
  cursorX = constrain(cursorX, 0, 320);
  cursorY = constrain(cursorY, 0, 240);

  // Check if Button A is pressed to shoot
  if (M5.BtnA.wasPressed()) {
    isDuckHit = checkDuckHit(cursorX, cursorY, duck);
    if (isDuckHit) {
      duck.isAlive = false; // Duck is hit
    }
  }

  // Clear screen and draw everything
  M5.Lcd.fillScreen(BLACK);

  // Draw the duck if it's still alive
  if (duck.isAlive) {
    M5.Lcd.fillRect(duck.x, duck.y, duck.width, duck.height, GREEN);
  }

  // Draw the cursor
  M5.Lcd.fillRect(cursorX - 5, cursorY - 5, 10, 10, RED); // Cursor as a small rectangle
  
  // Display hit status
  if (isDuckHit) {
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.print("Duck Hit!");
  } else {
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.print("Shoot the Duck!");
  }

  // Update the display
  M5.update();
  delay(10); // Small delay to make the game playable
}

// Function to check if the cursor is within the duck's rectangle
bool checkDuckHit(int cursorX, int cursorY, Duck duck) {
  return (cursorX >= duck.x && cursorX <= duck.x + duck.width) &&
         (cursorY >= duck.y && cursorY <= duck.y + duck.height);
}
