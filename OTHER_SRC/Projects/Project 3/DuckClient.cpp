#include <M5Unified.h>
#include "Adafruit_seesaw.h"
#include "images.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <ArduinoJson.h>

// Accelerometer data and smoothing parameters
float accX, accY, accZ;
float smoothX = 0, smoothZ = 0; 
const float alpha = 0.2; 

bool gameOver = false;


// Crosshair position
int cursorX = 160, cursorY = 120; 
int prevCursorX = cursorX, prevCursorY = cursorY;
bool isDuckHit = false;
bool drawFirstBackground = true;


// Structure to represent a duck
struct Duck {
    int x, y, width, height;
    bool isAlive;
    int targetX, targetY;
    bool isCpu;
    int prevX, prevY;  // Store previous position
};


struct Crosshair {
  int x, y, width, height;
  int points;
};


// Initialize ducks
Duck ducks[4] = {
    {100, 60, 30, 30, true, 100, 60, false},
    {50, 60, 30, 30, true, 100, 60, false},
    {200, 20, 30, 30, true, 100, 60, false},
    {50, 60, 30, 30, true, 100, 60, false},
};


///////////////////////////////////////////////////////////////
// UUID's
///////////////////////////////////////////////////////////////
#define SERVER_DEVICE_NAME "M5Core2_BLE_Server"
#define SERVICE_UUID "12345678-1234-5678-1234-56789abcdef0"
#define CHARACTERISTIC_UUID "87654321-4321-6789-4321-abcdef987654"

// Bluetooth Variables
BLEClient* pClient;
BLERemoteCharacteristic* pRemoteCharacteristic;

boolean deviceConnected = false;
Duck clientDuck = {50, 50, 30, 30, true, 50, 50, true};
Crosshair shooter = {cursorX - 25, cursorY - 25, 50, 50, 0};

//Seesaw Controller
Adafruit_seesaw ss;

#define BUTTON_X 6
#define BUTTON_Y 2
#define BUTTON_A 5
#define BUTTON_B 1
#define BUTTON_SELECT 0
#define BUTTON_START 16

unsigned long lastSelectPress = 0;  

uint32_t button_mask = (1UL << BUTTON_START) | (1UL << BUTTON_SELECT);

// Function prototypes
bool checkDuckHit(int cursorX, int cursorY, Duck duck);
void drawBitmapBackground(const uint16_t* bitmap);
void drawBitmapRegion(const uint16_t* bitmap, int x1, int y1, int width, int height, int destX, int destY);
void playGunshotSound();
void drawImage(const uint16_t* bitmap, int x, int y, int width, int height);
void drawScreenTextWithBackground(String text, int backgroundColor);

///////////////////////////////////////////////////////////////
// BLE Server Callback Method
///////////////////////////////////////////////////////////////
class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *pclient)
    {
        deviceConnected = true;
        Serial.println("Device connected...");
    }

    void onDisconnect(BLEClient *pclient)
    {
        deviceConnected = false;
        Serial.println("Device disconnected...");
    }
};

///////////////////////////////////////////////////////////////
// Connecting to server
///////////////////////////////////////////////////////////////
void connectToServer() {
    Serial.println("Scanning for BLE server...");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    BLEScanResults foundDevices = pBLEScan->start(5);
    
    for (int i = 0; i < foundDevices.getCount(); i++) {
        BLEAdvertisedDevice device = foundDevices.getDevice(i);
        if (device.getName() == SERVER_DEVICE_NAME) {
            Serial.println("Found BLE Server! Connecting...");
            pClient = BLEDevice::createClient();
            if (pClient->connect(&device)) {
                Serial.println("Connected to server!");
                deviceConnected = true;
                BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
                if (pRemoteService) {
                    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
                    if (pRemoteCharacteristic) {
                        Serial.println("Characteristic found!");
                    }
                }
            }
            break;
        }
    }
}

void parseBLEData(const std::string& response) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.f_str());
        return;
    }

    // Extract crosshair data
    if (doc.containsKey("crosshair")) {
        shooter.x = doc["crosshair"]["x"];
        shooter.y = doc["crosshair"]["y"];
    }

    // Extract duck data
    if (doc.containsKey("ducks")) {
        JsonArray duckArray = doc["ducks"];
        int numDucks = duckArray.size();

        for (int i = 0; i < numDucks; i++) {
            ducks[i].prevX = ducks[i].x; // Store old position
            ducks[i].prevY = ducks[i].y;
            
            ducks[i].x = duckArray[i]["x"];
            ducks[i].y = duckArray[i]["y"];
            ducks[i].isAlive = duckArray[i]["isAlive"];
        }
    }
    shooter.points = doc["points"];
    gameOver = doc["gameOver"];
}


void sendXY(int x, int y) {
    if (pRemoteCharacteristic) {
        char buffer[20];
        snprintf(buffer, sizeof(buffer), "%d,%d", x, y);
        pRemoteCharacteristic->writeValue(buffer);
    }
}

void readResponse() {
    if (pRemoteCharacteristic) {
        std::string response = pRemoteCharacteristic->readValue();
        Serial.println("Received BLE data:");
        // Serial.println(response.c_str());
        parseBLEData(response); // Parse the JSON data
    }
}


void setup() {
  M5.begin();
  Serial.begin(115200);
  M5.Lcd.setTextSize(3);
  drawScreenTextWithBackground("Looking for BLE server!", TFT_ORANGE);

  //Seesaw Setup
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
  
  //Connect to server
  BLEDevice::init("ESP32_BLE_Client");
  connectToServer();
}

void loop() {
    M5.update();

    if (deviceConnected) {  
        if (drawFirstBackground) {
            drawBitmapBackground(background);
            drawFirstBackground = false;
        }
        if (gameOver) {
            M5.Lcd.fillScreen(BLACK);
            M5.Lcd.println("GAME OVER");
            return;
        }

        // Erase previous crosshair position
        drawBitmapRegion(background, prevCursorX - 25, prevCursorY - 25, 50, 50, prevCursorX - 25, prevCursorY - 25);

        // Erase previous duck positions
        for (int i = 0; i < 4; i++) {
            if (ducks[i].isAlive) {
                drawBitmapRegion(background, ducks[i].prevX, ducks[i].prevY, 30, 30, ducks[i].prevX, ducks[i].prevY);
            }
        }

        int x = 1023 - ss.analogRead(14);
        int y = 1023 - ss.analogRead(15);

        if (x > 560 && ducks[3].x < M5.Lcd.width() - 3) ducks[3].x += 10;
        if (x < 500 && ducks[3].x > 3) ducks[3].x -= 10;
        if (y < 490 && ducks[3].y < M5.Lcd.height() - 3) ducks[3].y += 10;
        if (y > 550 && ducks[3].y > 3) ducks[3].y -= 10;

        for (int i = 0; i < 4; i++) {
            if (ducks[i].isAlive) {
                drawImage(duckRight, ducks[i].x, ducks[i].y, 30, 30);
            }
        }

        drawImage(crosshair, shooter.x - 25, shooter.y - 25, 50, 50);

        prevCursorX = cursorX;
        prevCursorY = cursorY;

        sendXY(ducks[3].x, ducks[3].y);
        readResponse();

        M5.Lcd.setCursor(10, 10);
        M5.Lcd.setTextSize(2);
        M5.Lcd.print("Points: ");
        M5.Lcd.setCursor(50, 10);
        M5.Lcd.print(shooter.points);


        delay(10);
    }
}



// Check if the cursor is within duck bounds
bool checkDuckHit(int cursorX, int cursorY, Duck duck) {
    return (cursorX >= duck.x && cursorX <= duck.x + duck.width) &&
           (cursorY >= duck.y && cursorY <= duck.y + duck.height);
}

// Play a gunshot sound
void playGunshotSound() {
    M5.Speaker.tone(150, 30);
    delay(10);
    M5.Speaker.tone(800, 50);
    delay(10);
    M5.Speaker.tone(200, 20);
}

// Draw the background image
void drawBitmapBackground(const uint16_t* bitmap) {
    M5.Lcd.pushImage(0, 0, 320, 240, bitmap);
}

// Draw an image with transparency
void drawImage(const uint16_t* bitmap, int x, int y, int width, int height) {
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            uint16_t color = bitmap[j * width + i];
            if (color != 0x0000) { // Skip black (transparent) pixels
                M5.Lcd.drawPixel(x + i, y + j, color);
            }
        }
    }
}

// Draw a portion of the background
void drawBitmapRegion(const uint16_t* bitmap, int x1, int y1, int width, int height, int destX, int destY) {
    uint16_t buffer[width * height];
    
    for (int y = 0; y < height; y++) {
        memcpy_P(buffer + (y * width), bitmap + ((y1 + y) * 320 + x1), width * sizeof(uint16_t));
    }

    M5.Lcd.pushImage(destX, destY, width, height, buffer);
}

///////////////////////////////////////////////////////////////
// Colors the background and then writes the text on top
///////////////////////////////////////////////////////////////
void drawScreenTextWithBackground(String text, int backgroundColor) {
  M5.Lcd.fillScreen(backgroundColor);
  M5.Lcd.setCursor(0,0);
  M5.Lcd.println(text);
}
