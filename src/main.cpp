#include "Adafruit_seesaw.h"
#include "M5Core2.h"
#include <random>

Adafruit_seesaw ss;

#define BUTTON_X 6
#define BUTTON_Y 2
#define BUTTON_A 5
#define BUTTON_B 1
#define BUTTON_SELECT 0
#define BUTTON_START 16

unsigned long lastSelectPress = 0;  

int pinkDotX = 0;
int pinkDotY = 0;
int blueDotX = 200;
int blueDotY = 200;

boolean isGameOver = false;
double startTime = millis();
double endTime;

int pinkDotMultiplier = 1;
int blueDotMultiplier = 1;
String millisToTime(unsigned long millisVal);
Point getRandomNum();

uint32_t button_mask = (1UL << BUTTON_START) | (1UL << BUTTON_SELECT);

void setup() {
    M5.begin();

    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }
    Serial.println("Gamepad QT example!");

    if (!ss.begin(0x50)) {
        Serial.println("ERROR! seesaw not found");
        while (1) delay(1);
    }
    Serial.println("seesaw started");

    uint32_t version = ((ss.getVersion() >> 16) & 0xFFFF);
    if (version != 5743) {
        Serial.print("Wrong firmware loaded? ");
        Serial.println(version);
        while (1) delay(10);
    }
    Serial.println("Found Product 5743");

    ss.pinModeBulk(button_mask, INPUT_PULLUP);
    ss.setGPIOInterrupts(button_mask, 1);

    randomSeed(millis());
}

int last_x = 0, last_y = 0;

void loop() {
    M5.Lcd.fillScreen(BLACK);

    if (!isGameOver)
    {
     delay(10); // Slow serial output

    // Reverse x/y values to match joystick orientation
    int x = 1023 - ss.analogRead(14);
    int y = 1023 - ss.analogRead(15);


        if (x > 560 && blueDotX < M5.Lcd.width() - 3)
        {
            blueDotX = blueDotX + (1 * blueDotMultiplier);
        }
        if (x < 500 && blueDotX > 3)
        {
            blueDotX = blueDotX - (1 * blueDotMultiplier);
        }

        if (y < 490 && blueDotY < M5.Lcd.height() - 3)
        {
            blueDotY = blueDotY + (1 * blueDotMultiplier);
        }
        if (y > 550 && blueDotY > 3)
        {
            blueDotY = blueDotY - (1 * blueDotMultiplier);
        }


        last_x = x;
        last_y = y;


    uint32_t buttons = ss.digitalReadBulk(button_mask);

    unsigned long currentTime = millis();

    if (!(buttons & (1UL << BUTTON_SELECT))) {  // Check if SELECT button is pressed
        if (currentTime - lastSelectPress >= 500) {  // Enforce timeout
            Serial.println("Button SELECT pressed");

            Point rand = getRandomNum();
            blueDotX = rand.x;
            blueDotY = rand.y;

            lastSelectPress = currentTime;  // Update last press time
        }
    }

    if (!(buttons & (1UL << BUTTON_START))) {
        Serial.println("Button START pressed");
        if (blueDotMultiplier < 5)
        {
            blueDotMultiplier = blueDotMultiplier + 1;
        }else
        {
            blueDotMultiplier = 1;
        }
    }
        if (sqrt(pow(pinkDotX - blueDotX, 2) + pow(pinkDotY - blueDotY, 2)) <= 20)
        {
            isGameOver = true;
            endTime = millis();
        }


    M5.Lcd.fillRect(blueDotX, blueDotY, 10, 10, BLUE);  // Blue dot (joystick control)
    M5.Lcd.fillRect(pinkDotX, pinkDotY, 10, 10, PINK);    // Red dot (button control)
    }else
    {
       M5.Lcd.setCursor(M5.Lcd.width() / 3, M5.Lcd.height() / 2);
        M5.Lcd.setTextSize(3);
        M5.Lcd.printf("GAME OVER");

        int timeDiff = endTime - startTime;
        M5.Lcd.setCursor(M5.Lcd.width() / 3, M5.Lcd.height() / 2 + 50);
        M5.Lcd.printf( "%ss",millisToTime(timeDiff));



    }
}

String millisToTime(unsigned long millisVal) {
    unsigned long seconds = millisVal / 1000;
    unsigned long milliseconds = millisVal % 1000;

    char timeString[10];
    sprintf(timeString, "%02lu:%03lu", seconds, milliseconds);

    return String(timeString);
}

Point getRandomNum(){
    int randomNumberX = random(0, M5.Lcd.width() - 3);
    int randomNumberY = random(0, M5.Lcd.height() - 3);

    return Point(randomNumberX,randomNumberY);

    Serial.printf("Random number: %d\n", randomNumberX);
    Serial.printf("Random number: %d\n", randomNumberY);

}

