#include <M5Core2.h>

///////////////////////////////////////////////////////////////
// CHALLENGE: Upgrade the snowman program so each button press
// of A, B or C will increment a Red, Green or Blue color value,
// respectively. The three distinct RGB color values should be
// used to generate one overall color which will be the
// background color behind the snowman. The background should
// update each time a button is pressed.
//
// LCD Details
// 320 x 240 pixels (width x height)
//      - (0, 0) is the TOP LEFT pixel
//      - Colors seem to be 16-bit as: RRRR RGGG GGGB BBBB (32 Reds, 64 Greens, 32 Blues)
//      TFT_RED         0xF800      /* 255,   0,   0 */     1111 1000 0000 0000
//      TFT_GREEN       0x07E0      /*   0, 255,   0 */     0000 0111 1110 0000
//      TFT_BLUE        0x001F      /*   0,   0, 255 */     0000 0000 0001 1111
///////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
// Put your setup code here, to run once
///////////////////////////////////////////////////////////////
// Constants for LCD screen and snowman properties
const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;
const int RADIUS = SCREEN_HEIGHT / 4;
const int RADIUS_ADJUSTED = RADIUS * 0.8;
const int RADIUS_BODY = RADIUS * 0.6;
const int RADIUS_HEAD = RADIUS * 0.4;
const int EYE_OFFSET = RADIUS_HEAD / 2;

// Variables for RGB background color
int redValue = 0;
int greenValue = 0;
int blueValue = 0;

///////////////////////////////////////////////////////////////
// Setup code runs once
///////////////////////////////////////////////////////////////
void setup() {
    M5.begin();
    M5.Lcd.fillScreen(BLACK);

    // Draw the snowman
    drawSnowman();
}

///////////////////////////////////////////////////////////////
// Main loop runs repeatedly
///////////////////////////////////////////////////////////////
void loop() {
    // Check button presses to update RGB values
    if (M5.BtnA.wasPressed()) {
        redValue = (redValue + 8) % 256; // Increment red (wraps at 255)
    }
    if (M5.BtnB.wasPressed()) {
        greenValue = (greenValue + 8) % 256; // Increment green (wraps at 255)
    }
    if (M5.BtnC.wasPressed()) {
        blueValue = (blueValue + 8) % 256; // Increment blue (wraps at 255)
    }

    // Update the background color
    uint16_t bgColor = M5.Lcd.color565(redValue, greenValue, blueValue);
    M5.Lcd.fillScreen(bgColor);

    // Redraw the snowman after updating the background
    drawSnowman();

    M5.update(); // Update button states
}

///////////////////////////////////////////////////////////////
// Draw the snowman
///////////////////////////////////////////////////////////////
void drawSnowman() {
    int centerX = SCREEN_WIDTH / 2;
    int baseY = SCREEN_HEIGHT - RADIUS_ADJUSTED;

    // Draw body and head
    M5.Lcd.fillCircle(centerX, baseY, RADIUS, WHITE);                  // Base
    M5.Lcd.fillCircle(centerX, baseY - RADIUS_BODY * 2, RADIUS_BODY, WHITE); // Body
    M5.Lcd.fillCircle(centerX, baseY - RADIUS_BODY * 2 - RADIUS_HEAD * 2, RADIUS_HEAD, WHITE); // Head

    // Draw buttons
    M5.Lcd.fillCircle(centerX, baseY, 10, BLACK);
    M5.Lcd.fillCircle(centerX, baseY - RADIUS_BODY, 10, BLACK);
    M5.Lcd.fillCircle(centerX, baseY - RADIUS_BODY * 1.5, 10, BLACK);

    // Draw eyes
    M5.Lcd.fillCircle(centerX - EYE_OFFSET, baseY - RADIUS_BODY * 2 - RADIUS_HEAD, 5, BLACK);
    M5.Lcd.fillCircle(centerX + EYE_OFFSET, baseY - RADIUS_BODY * 2 - RADIUS_HEAD, 5, BLACK);

    // Draw carrot nose
    M5.Lcd.fillTriangle(
        centerX, baseY - RADIUS_BODY * 2 - RADIUS_HEAD + 10,
        centerX, baseY - RADIUS_BODY * 2 - RADIUS_HEAD - 5 + 10,
        centerX + 20, baseY - RADIUS_BODY * 2 - RADIUS_HEAD + 2 + 10,
        ORANGE
    );
}

//////////////////////////////////////////////////////////////////////////////////
// For more documentation see the following link:
// https://github.com/m5stack/m5-docs/blob/master/docs/en/api/
//////////////////////////////////////////////////////////////////////////////////