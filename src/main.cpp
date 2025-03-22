#include <M5Unified.h>
#include "images.h"

float accX, accY, accZ;
float smoothX = 0, smoothZ = 0; 
const float alpha = 0.2; 

int cursorX = 160, cursorY = 120; 
int prevCursorX = cursorX, prevCursorY = cursorY;
bool isDuckHit = false;

struct Duck {
  int x, y, width, height;
  bool isAlive;
  int targetX, targetY;
};

// Create an array of ducks
Duck ducks[3] = {
  {100, 60, 30, 30, true, 100, 60},
  {50, 60, 30, 30, true, 100, 60},
  {200, 20, 30, 30, true, 100, 60}
};

bool checkDuckHit(int cursorX, int cursorY, Duck duck);
void drawBitmapBackground(const uint16_t* bitmap);
void drawBitmapRegion(const uint16_t* bitmap, int x1, int y1, int width, int height, int destX, int destY);
void playGunshotSound();
void drawImage(const uint16_t* bitmap, int x, int y, int width, int height);

void setup() {
  M5.begin();
  M5.Lcd.setRotation(1);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(1);
  M5.Imu.init();

  drawBitmapBackground(background);

  for (int i = 0; i < 3; i++) {
    if (ducks[i].isAlive) {
      drawImage(duckRight, ducks[i].x, ducks[i].y, 30, 30); // Draw each duck
    }
  }

  M5.Speaker.begin();
}

void loop() {
  M5.update();
  M5.Imu.getAccelData(&accX, &accY, &accZ);

  smoothX = alpha * accX + (1 - alpha) * smoothX;
  smoothZ = alpha * accZ + (1 - alpha) * smoothZ;

  cursorX -= smoothX * 8;
  cursorY -= smoothZ * 8;

  cursorX = constrain(cursorX, 0, 320);
  cursorY = constrain(cursorY, 0, 240);

  // Erase previous cursor position using background
  drawBitmapRegion(background, prevCursorX - 25, prevCursorY - 25, 50, 50, prevCursorX - 25, prevCursorY - 25);

  // Draw new cursor (crosshair image)
  drawImage(crosshair, cursorX - 25, cursorY - 25, 50, 50);

  prevCursorX = cursorX;
  prevCursorY = cursorY;

  if (M5.BtnC.wasPressed()) {
    M5.Power.setVibration(1000);
    playGunshotSound();
    delay(50);
    M5.Power.setVibration(0);

    M5.Lcd.fillScreen(WHITE);
    delay(50);
    drawBitmapBackground(background);

    // Check if any duck is hit
    for (int i = 0; i < 3; i++) {
      isDuckHit = checkDuckHit(cursorX, cursorY, ducks[i]);
      if (isDuckHit) {
        ducks[i].isAlive = false;
        drawBitmapRegion(background, ducks[i].x, ducks[i].y, ducks[i].width, ducks[i].height, ducks[i].x, ducks[i].y);
      }
    }
  }

  // Update and draw all ducks
  for (int i = 0; i < 3; i++) {
    if (ducks[i].isAlive) {
      drawBitmapRegion(background, ducks[i].x, ducks[i].y, ducks[i].width, ducks[i].height, ducks[i].x, ducks[i].y);

      int dx = ducks[i].targetX - ducks[i].x;
      int dy = ducks[i].targetY - ducks[i].y;

      if (abs(dx) < 5 && abs(dy) < 5) {
        ducks[i].targetX = random(0, 320 - ducks[i].width);
        ducks[i].targetY = random(0, 240 - ducks[i].height);
      } else {
        if (dx != 0) ducks[i].x += (dx > 0) ? 1 : -1;
        if (dy != 0) ducks[i].y += (dy > 0) ? 1 : -1;
      }

      drawImage(duckRight, ducks[i].x, ducks[i].y, 30, 30); // Draw the duck
    }
  }

  M5.Lcd.setCursor(10, 10);
  M5.Lcd.fillRect(10, 10, 100, 10, BLACK);
  M5.Lcd.print(isDuckHit ? "Duck Hit!" : "Shoot the Duck!");

  delay(10);
}

bool checkDuckHit(int cursorX, int cursorY, Duck duck) {
  return (cursorX >= duck.x && cursorX <= duck.x + duck.width) &&
         (cursorY >= duck.y && cursorY <= duck.y + duck.height);
}

void playGunshotSound() {
  M5.Speaker.tone(150, 30);
  delay(10);
  M5.Speaker.tone(800, 50);
  delay(10);
  M5.Speaker.tone(200, 20);
}

void drawBitmapBackground(const uint16_t* bitmap) {
  M5.Lcd.pushImage(0, 0, 320, 240, bitmap);
}

void drawImage(const uint16_t* bitmap, int x, int y, int width, int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      uint16_t color = bitmap[j * width + i];  // Get pixel color
      if (color != 0x0000) {  // Skip transparent pixels (black in this case)
        M5.Lcd.drawPixel(x + i, y + j, color);  // Draw the pixel
      }
    }
  }
}

void drawBitmapRegion(const uint16_t* bitmap, int x1, int y1, int width, int height, int destX, int destY) {
  uint16_t buffer[width * height];

  for (int y = 0; y < height; y++) {
    memcpy_P(buffer + (y * width), bitmap + ((y1 + y) * 320 + x1), width * sizeof(uint16_t));
  }

  M5.Lcd.pushImage(destX, destY, width, height, buffer);
}
