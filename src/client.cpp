#include "Adafruit_seesaw.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include "M5Core2.h"

///////////////////////////////////////////////////////////////
// UUID's
///////////////////////////////////////////////////////////////
#define SERVER_DEVICE_NAME "M5Core2_BLE_Server"
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
//BLE
BLEClient* pClient;
BLERemoteCharacteristic* pRemoteCharacteristic;


//Regular
int dotSize = 10;
boolean deviceConnected = false;
DOT serverDot = {0, 0, 1};
DOT localDot = {320 - dotSize, 240 - dotSize, 1};

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
// BLE Server Callback Method
// These methods are called upon connection and disconnection
// to BLE service.
///////////////////////////////////////////////////////////////
class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *pclient)
    {
        deviceConnected = true;
        M5.lcd.fillScreen(BLACK);
        Serial.println("Device connected...");
    }

    void onDisconnect(BLEClient *pclient)
    {
        deviceConnected = false;
        Serial.println("Device disconnected...");
        //drawScreenTextWithBackground("LOST connection to device.\n\nAttempting re-connection...", TFT_RED);
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
                M5.Lcd.fillScreen(BLACK);
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

void sendXY(int x, int y) {
    if (pRemoteCharacteristic) {
        char buffer[20];
        snprintf(buffer, sizeof(buffer), "%d,%d", x, y);
        pRemoteCharacteristic->writeValue(buffer);
        // Serial.printf("Sent: %s\n", buffer);
    }
}

void readResponse() {
    if (pRemoteCharacteristic) {
        std::string response = pRemoteCharacteristic->readValue();
        // Serial.printf("Received: %s\n", response.c_str());
        
        // Find the position of the comma
        size_t commaIndex = response.find(',');

        // If the comma is found
        if (commaIndex != std::string::npos) {
            // Split the string into x and y
            std::string xStr = response.substr(0, commaIndex);
            std::string yStr = response.substr(commaIndex + 1);

            // Convert to integers
            serverDot.x = std::stoi(xStr);
            serverDot.y = std::stoi(yStr);
            // Serial.println(xStr.c_str());
            // Serial.println(yStr.c_str());
        }
    }
}

///////////////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////////////
void drawScreenTextWithBackground(String text, int backgroundColor);
String millisToTime(unsigned long millisVal);
Point getRandomNum();

void setup() {
    M5.begin();
    Serial.begin(115200);
    M5.Lcd.setTextSize(3);
    drawScreenTextWithBackground("Looking for BLE server!" , TFT_ORANGE);

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
    if(deviceConnected){
        //Draw black dot over old area
        M5.Lcd.fillRect(serverDot.lastX, serverDot.lastY, dotSize, dotSize, BLACK);
        M5.Lcd.fillRect(localDot.lastX, localDot.lastY, dotSize, dotSize, BLACK);


        // Draw the new positions
        M5.Lcd.fillRect(localDot.x, localDot.y, dotSize, dotSize, BLUE);
        M5.Lcd.fillRect(serverDot.x, serverDot.y, dotSize, dotSize, PINK);

        // Store last position
        if (localDot.lastX != localDot.x) localDot.lastX = localDot.x;
        if (localDot.lastY != localDot.y) localDot.lastY = localDot.y;
        if (serverDot.lastX != serverDot.x) serverDot.lastX = serverDot.x;
        if (serverDot.lastY != serverDot.y) serverDot.lastY = serverDot.y;


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

            if (sqrt(pow(serverDot.x - localDot.x, 2) + pow(serverDot.y - localDot.y, 2)) <= 20) {
                isGameOver = true;
                endTime = millis();
            }
            
            if (pRemoteCharacteristic) {
                sendXY(localDot.x, localDot.y);
                readResponse();
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