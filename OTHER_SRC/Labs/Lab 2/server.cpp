#include "Adafruit_seesaw.h"
#include "M5Core2.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

///////////////////////////////////////////////////////////////
// UUID's
///////////////////////////////////////////////////////////////
#define SERVICE_UUID "12345678-1234-5678-1234-56789abcdef0"
#define CHARACTERISTIC_UUID "87654321-4321-6789-4321-abcdef987654"


///////////////////////////////////////////////////////////////
// Point Struct
///////////////////////////////////////////////////////////////
struct DOT {
    int x;
    int y;
    int multiplier;
    int lastX;
    int lastY;
};


///////////////////////////////////////////////////////////////
// Variables
///////////////////////////////////////////////////////////////

//Regular
int dotSize = 10;
boolean deviceConnected = false;
DOT clientDot = {310, 230};
DOT localDot = {0, 0};
boolean screenClear = false;

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


//Game states
boolean isGameOver = false;
double startTime = millis();
double endTime;

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
// Forward Declarations
///////////////////////////////////////////////////////////////
void drawScreenTextWithBackground(String text, int backgroundColor);
String millisToTime(unsigned long millisVal);
Point getRandomNum();

///////////////////////////////////////////////////////////////
// Characteristic Callbacks
///////////////////////////////////////////////////////////////
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *pCharacteristic) override {
        char buffer[20];
        snprintf(buffer, sizeof(buffer), "%d,%d", localDot.x, localDot.y);
        pCharacteristic->setValue(buffer);
        // Serial.printf("Client read response: %s\n", buffer);
    }
    void onWrite(BLECharacteristic *pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        if (!value.empty()) {
            sscanf(value.c_str(), "%d,%d", &clientDot.x, &clientDot.y);
            // Serial.printf("Client wrote: x=%d, y=%d\n", clientDot.x, clientDot.y);
        }
    }
    void onConnect(BLEClient *pclient)
    {
        M5.lcd.fillScreen(BLACK);
        Serial.println("Device connected...");
        deviceConnected = true;
    }

    void onDisconnect(BLEClient *pclient)
    {
        deviceConnected = false;
        Serial.println("Device disconnected...");
        drawScreenTextWithBackground("LOST connection to device.\n\nAttempting re-connection...", TFT_RED);
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
    Serial.begin(115200);
    M5.Lcd.setTextSize(3);
    ss.begin();
    drawScreenTextWithBackground("Looking for BLE Client!" , TFT_ORANGE);
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
    
    broadcastServer();

}

void loop() {
    if(deviceConnected){
        if(!screenClear){
            M5.lcd.fillScreen(BLACK);
            screenClear = true;
        }
        //Draw black dot over old area
        M5.Lcd.fillRect(clientDot.lastX, clientDot.lastY, dotSize, dotSize, BLACK);
        M5.Lcd.fillRect(localDot.lastX, localDot.lastY, dotSize, dotSize, BLACK);


        // Draw the new positions
        M5.Lcd.fillRect(localDot.x, localDot.y, dotSize, dotSize, BLUE);
        M5.Lcd.fillRect(clientDot.x, clientDot.y, dotSize, dotSize, PINK);

        // Store last position
        if (localDot.lastX != localDot.x) localDot.lastX = localDot.x;
        if (localDot.lastY != localDot.y) localDot.lastY = localDot.y;
        if (clientDot.lastX != clientDot.x) clientDot.lastX = clientDot.x;
        if (clientDot.lastY != clientDot.y) clientDot.lastY = clientDot.y;


        if (!isGameOver) {
            delay(10);

            int x = 1023 - ss.analogRead(14);
            int y = 1023 - ss.analogRead(15);

            if (x > 560 && localDot.x < M5.Lcd.width() - 3) {
                localDot.x += 1 * localDot.multiplier;
            }
            if (x < 500 && localDot.x > 3) {
                localDot.x -= 1 * localDot.multiplier;
            }
            if (y < 490 && localDot.y < M5.Lcd.height() - 3) {
                localDot.y += 1 * localDot.multiplier;
            }
            if (y > 550 && localDot.y > 3) {
                localDot.y -= 1 * localDot.multiplier;
            }

            uint32_t buttons = ss.digitalReadBulk(button_mask);
            unsigned long currentTime = millis();

            if (!(buttons & (1UL << BUTTON_SELECT))) {
                if (currentTime - lastSelectPress >= 500) {
                    Point rand = getRandomNum();
                    localDot.x = rand.x;
                    localDot.y = rand.y;
                    lastSelectPress = currentTime;
                }
            }

            if (!(buttons & (1UL << BUTTON_START))) {
                if (localDot.multiplier < 5) {
                    localDot.multiplier++;
                } else {
                    localDot.multiplier = 1;
                }
            }

            if (sqrt(pow(clientDot.x - localDot.x, 2) + pow(clientDot.y - localDot.y, 2)) <= 20) {
                isGameOver = true;
                endTime = millis();
            }

        } else {
            M5.Lcd.setCursor(M5.Lcd.width() / 3, M5.Lcd.height() / 2);
            M5.Lcd.setTextSize(3);
            M5.Lcd.printf("GAME OVER");

            int timeDiff = endTime - startTime;
            M5.Lcd.setCursor(M5.Lcd.width() / 3, M5.Lcd.height() / 2 + 50);
            M5.Lcd.printf("%ss", millisToTime(timeDiff));
            
        }
    }
}

///////////////////////////////////////////////////////////////
// Colors the background and then writes the text on top
///////////////////////////////////////////////////////////////
void drawScreenTextWithBackground(String text, int backgroundColor) {
    M5.Lcd.fillScreen(backgroundColor);
    M5.Lcd.setCursor(0,0);
    M5.Lcd.println(text);
}


///////////////////////////////////////////////////////////////
// Gets random number for X and Y coords
///////////////////////////////////////////////////////////////
Point getRandomNum(){
    int randomNumberX = random(0, M5.Lcd.width() - 3);
    int randomNumberY = random(0, M5.Lcd.height() - 3);

    return Point(randomNumberX,randomNumberY);

    // Serial.printf("Random number: %d\n", randomNumberX);
    // Serial.printf("Random number: %d\n", randomNumberY);

}

///////////////////////////////////////////////////////////////
// Converts miliseconds to seconds and miliseconds ss:mm
///////////////////////////////////////////////////////////////
String millisToTime(unsigned long millisVal) {
    unsigned long seconds = millisVal / 1000;
    unsigned long milliseconds = millisVal % 1000;

    char timeString[10];
    sprintf(timeString, "%02lu:%03lu", seconds, milliseconds);

    return String(timeString);
}