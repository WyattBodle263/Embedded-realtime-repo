#include <M5Unified.h>
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
bool hasDrawnOver = false;

// Crosshair position
int cursorX = 160, cursorY = 120; 
int prevCursorX = cursorX, prevCursorY = cursorY;
bool isDuckHit = false;
bool drawFirstBackground = true;
bool isWinner = false;

// Structure to represent a duck
struct Duck {
    int x, y, width, height;
    bool isAlive;
    int targetX, targetY;
    bool isCpu;
    int prevX;
};

struct Crosshair {
  int x, y, width, height, points;
};

// Initialize ducks
Duck ducks[6] = {
    {100, 60, 30, 30, true, 100, 60, false, 100},
    {50, 60, 30, 30, true, 100, 60, false, 50},
    {200, 20, 30, 30, true, 100, 60, false, 200},
    {100, 60, 30, 30, true, 100, 60, false, 100},
    {50, 60, 30, 30, true, 100, 60, false, 50},
    {200, 20, 30, 30, true, 100, 60, false, 200},
};

///////////////////////////////////////////////////////////////
// UUID's
///////////////////////////////////////////////////////////////
#define SERVICE_UUID "12345678-1234-5678-1234-56789abcdef0"
#define CHARACTERISTIC_UUID "87654321-4321-6789-4321-abcdef987654"

// Bluetooth Variables
boolean deviceConnected = false;
Duck clientDuck = {50, 50, 30, 30, true, 50, 50, true, 50};
Crosshair shooter = {cursorX - 25, cursorY - 25, 50, 50};
int prevClientDuckX = clientDuck.x;
int prevClientDuckY = clientDuck.y;

// Function prototypes
bool checkDuckHit(int cursorX, int cursorY, Duck duck);
void drawBitmapBackground(const uint16_t* bitmap);
void drawBitmapRegion(const uint16_t* bitmap, int x1, int y1, int width, int height, int destX, int destY);
void playGunshotSound();
void drawImage(const uint16_t* bitmap, int x, int y, int width, int height);
void drawScreenTextWithBackground(String text, int backgroundColor);

///////////////////////////////////////////////////////////////
// BLE Server Callback Methods
///////////////////////////////////////////////////////////////
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
      deviceConnected = true;
      Serial.println("Device connected...");
  }
  void onDisconnect(BLEServer *pServer) {
      deviceConnected = false;
      Serial.println("Device disconnected...");
  }
};

///////////////////////////////////////////////////////////////
// Characteristic Callbacks
///////////////////////////////////////////////////////////////
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *pCharacteristic) override {
        StaticJsonDocument<1024> doc;  // Increase from 512 to 1024 bytes
        
        // Crosshair position
        JsonObject crosshair = doc.createNestedObject("crosshair");
        crosshair["x"] = cursorX;
        crosshair["y"] = cursorY;
    
        // Ducks positions
        JsonArray ducksArray = doc.createNestedArray("ducks");
        for (int i = 0; i < 6; i++) {
            JsonObject duck = ducksArray.createNestedObject();
            duck["x"] = ducks[i].x;
            duck["y"] = ducks[i].y;
            duck["isAlive"] = ducks[i].isAlive;
        }
    
        // Game state
        doc["gameOver"] = gameOver; 
        doc["isWinner"] = !isWinner;
    
        char buffer[1024];
        serializeJson(doc, buffer, sizeof(buffer));
        pCharacteristic->setValue(buffer);
        pCharacteristic->notify();
    }
    
    
    void onWrite(BLECharacteristic *pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        Serial.print("GETTING CLIENT VALUE: ");
        if (!value.empty()) {
            Serial.printf("Received: %s\n", value.c_str());  // Debugging received data
            sscanf(value.c_str(), "%d,%d", &clientDuck.x, &clientDuck.y);
            Serial.printf("Client wrote: x=%d, y=%d\n", clientDuck.x, clientDuck.y);
        }
    }
    
  };

///////////////////////////////////////////////////////////////
// Broadcasting BLE server to client
///////////////////////////////////////////////////////////////
void broadcastServer(){
  BLEDevice::init("M5Core2_BLE_Server");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pServer->setCallbacks(new MyServerCallbacks());

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();
  Serial.println("BLE Server is running...");
}

void setup() {
    M5.begin();
    M5.Lcd.setRotation(1);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(3);
    M5.Imu.init();
    M5.Speaker.begin();

    drawScreenTextWithBackground("Looking for BLE Client!" , TFT_ORANGE);
    broadcastServer();
}

void loop() {
    M5.update();

    if (deviceConnected) {
        if(shooter.points >= 2){
            gameOver = true;
            bool isWinner = false;
        }

        if(gameOver){
            if(!hasDrawnOver){
                if(isWinner){
                    M5.Lcd.fillScreen(GREEN);
                    M5.Lcd.setCursor(0,0);
                    M5.Lcd.println("GAME OVER");
                    M5.Lcd.setCursor(0,100);
                    M5.Lcd.println("You Win");
                }else{
                    M5.Lcd.fillScreen(RED);
                    M5.Lcd.setCursor(0,0);
                    M5.Lcd.println("GAME OVER");
                    M5.Lcd.setCursor(0,100);
                    M5.Lcd.println("You Lose");
                }
                hasDrawnOver = true;
            }
            return;
        }else{
            if (drawFirstBackground) {
                // drawBitmapBackground(background);
                drawFirstBackground = false;
            }
            drawBitmapBackground(background);

    
            M5.Imu.getAccelData(&accX, &accY, &accZ);
    
            // Smooth out accelerometer data
            smoothX = alpha * accX + (1 - alpha) * smoothX;
            smoothZ = alpha * accZ + (1 - alpha) * smoothZ;
    
            // Update cursor position
            cursorX = constrain(cursorX - smoothX * 8, 0, 320);
            cursorY = constrain(cursorY - smoothZ * 8, 0, 240);
    
            // Erase previous crosshair position
            // drawBitmapRegion(background, prevCursorX - 25, prevCursorY - 25, 50, 50, prevCursorX - 25, prevCursorY - 25);
    
            // Handle shooting action
            if (M5.BtnC.wasPressed()) {
                M5.Power.setVibration(1000);
                playGunshotSound();
                delay(50);
                M5.Power.setVibration(0);
    
                M5.Lcd.fillScreen(WHITE);
                delay(50);
                drawBitmapBackground(background);

                //Check if main duck is hit
                if (checkDuckHit(cursorX, cursorY, clientDuck)){
                    gameOver = true;
                    isWinner = true;
                }

                // Check if any duck is hit
                for (int i = 0; i < 6; i++) {
                    if (checkDuckHit(cursorX, cursorY, ducks[i])) {
                        ducks[i].isAlive = false;
                        shooter.points += 1;
                    }
                }
            }
    
            // Update and draw ducks
            for (int i = 0; i < 6; i++) {
                if (ducks[i].isAlive) {
                    // Erase the previous duck's area (clear region where the duck was previously)
                    // drawBitmapRegion(background, ducks[i].x, ducks[i].y, ducks[i].width, ducks[i].height, ducks[i].x, ducks[i].y);
    
                    // Move ducks towards target
                    int dx = ducks[i].targetX - ducks[i].x;
                    int dy = ducks[i].targetY - ducks[i].y;
    
                    if (abs(dx) < 5 && abs(dy) < 5) {
                        ducks[i].targetX = random(0, 320 - ducks[i].width);
                        ducks[i].targetY = random(0, 240 - ducks[i].height);
                    } else {
                        ducks[i].x += (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
                        ducks[i].y += (dy > 0) ? 1 : (dy < 0) ? -1 : 0;
                    }
    
                    if(ducks[i].prevX >= ducks[i].x){
                        drawImage(duckLeft, ducks[i].x, ducks[i].y, 30, 30);
                    }else{
                        drawImage(duckRight, ducks[i].x, ducks[i].y, 30, 30);
                    }
                    ducks[i].prevX = ducks[i].x;
                }
            }
    
            // Erase the previous client duck's area using its previous position
            // drawBitmapRegion(background, prevClientDuckX - 10, prevClientDuckY - 10, clientDuck.width + 20, clientDuck.height + 20, prevClientDuckX, prevClientDuckY);

            if(clientDuck.x > clientDuck.prevX){
                // Draw the updated client duck in the new position
                drawImage(duckRight, clientDuck.x, clientDuck.y, 30, 30);
    
            }else{
                drawImage(duckLeft, clientDuck.x, clientDuck.y, 30, 30);
            }
            
            // Update the previous position
            prevClientDuckX = clientDuck.x;
    
            // Draw crosshairs on top
            drawImage(crosshair, cursorX - 25, cursorY - 25, 50, 50);
    
            prevCursorX = cursorX;
            prevCursorY = cursorY;

            delay(10);
        }
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
