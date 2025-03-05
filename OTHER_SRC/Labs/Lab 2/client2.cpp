#include "Adafruit_seesaw.h"
#include "M5Core2.h"
#include <random>
#include <BLEDevice.h>
#include <BLE2902.h>
#include <M5Core2.h>

///////////////////////////////////////////////////////////////
// BLE Variables
///////////////////////////////////////////////////////////////
static BLERemoteCharacteristic *bleRemoteCharacteristic;
static BLEAdvertisedDevice *bleRemoteServer;
static boolean doConnect = false;
static boolean doScan = false;
bool deviceConnected = false;

// See the following for generating UUIDs: https://www.uuidgenerator.net/
// UUIDs
static BLEUUID SERVICE_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b"); 
static BLEUUID CHARACTERISTIC_UUID_BLUE("beb5483e-36e1-4688-b7f5-ea07361b26a8"); // Characteristic for Blue
static BLEUUID CHARACTERISTIC_UUID_PINK("12345678-1234-5678-1234-56789abcdef0"); // Characteristic for Pink
static BLERemoteCharacteristic *bleRemoteCharacteristicBlue;
static BLERemoteCharacteristic *bleRemoteCharacteristicPink;

// BLE Broadcast Name
static String BLE_BROADCAST_NAME = "abc";

///////////////////////////////////////////////////////////////
// Seesaw Variables
///////////////////////////////////////////////////////////////
Adafruit_seesaw ss;

#define BUTTON_X 6
#define BUTTON_Y 2
#define BUTTON_A 5
#define BUTTON_B 1
#define BUTTON_SELECT 0
#define BUTTON_START 16

unsigned long lastSelectPress = 0;  


///////////////////////////////////////////////////////////////
// Dot Variables
///////////////////////////////////////////////////////////////
int pinkDotMultiplier = 1;
int blueDotMultiplier = 1;
int pinkDotX = 200;
int pinkDotY = 200;
int blueDotX = 0;
int blueDotY = 0;

///////////////////////////////////////////////////////////////
// Game State Variables
///////////////////////////////////////////////////////////////
uint32_t button_mask = (1UL << BUTTON_START) | (1UL << BUTTON_SELECT);
boolean isGameOver = false;
double startTime = millis();
double endTime;

///////////////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////////////
void drawScreenTextWithBackground(String text, int backgroundColor);
String millisToTime(unsigned long millisVal);
void sendBlueDotCoordinates();
Point getRandomNum();

///////////////////////////////////////////////////////////////
// BLE Client Callback Methods
// This method is called when the server that this client is
// connected to NOTIFIES this client (or any client listening)
// that it has changed the remote characteristic
///////////////////////////////////////////////////////////////



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
// Method is called to connect to server
///////////////////////////////////////////////////////////////
bool connectToServer() {
    Serial.printf("Forming a connection to %s\n", bleRemoteServer->getName().c_str());
    BLEClient *bleClient = BLEDevice::createClient();
    bleClient->setClientCallbacks(new MyClientCallback());

    if (!bleClient->connect(bleRemoteServer)) {
        Serial.println("Failed to connect.");
        return false;
    }

    BLERemoteService *bleRemoteService = bleClient->getService(SERVICE_UUID);
    if (bleRemoteService == nullptr) {
        Serial.println("Service not found.");
        bleClient->disconnect();
        return false;
    }

    // Find Send Characteristic
    bleRemoteCharacteristicBlue = bleRemoteService->getCharacteristic(CHARACTERISTIC_UUID_BLUE);
    if (bleRemoteCharacteristicBlue == nullptr) {
        Serial.println("Blue Characteristic not found.");
        bleClient->disconnect();
        return false;
    }

    // Find Receive Characteristic
    bleRemoteCharacteristicPink = bleRemoteService->getCharacteristic(CHARACTERISTIC_UUID_PINK);
    if (bleRemoteCharacteristicPink == nullptr) {
        Serial.println("Pink Characteristic not found.");
        bleClient->disconnect();
        return false;
    }

    // Subscribe to notifications for receiving data
    if (bleRemoteCharacteristicPink->canNotify()) {
        bleRemoteCharacteristicPink->registerForNotify(notifyCallback);
    }

    return true;
}


///////////////////////////////////////////////////////////////
// Scan for BLE servers and find the first one that advertises
// the service we are looking for.
///////////////////////////////////////////////////////////////
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    /**
     * Called for each advertising BLE server.
     */
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        // Print device found
        Serial.print("BLE Advertised Device found:");
        Serial.printf("\tName: %s\n", advertisedDevice.getName().c_str());

        if (advertisedDevice.haveServiceUUID() && 
                advertisedDevice.isAdvertisingService(SERVICE_UUID) && 
                advertisedDevice.getName() == BLE_BROADCAST_NAME.c_str()) {
            BLEDevice::getScan()->stop();
            bleRemoteServer = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = true;
        }

    }     
};    

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

    M5.Lcd.setTextSize(3);
    drawScreenTextWithBackground("Scanning for BLE server...", TFT_BLUE);

    BLEDevice::init("");

    // Retrieve a Scanner and set the callback we want to use to be informed when we
    // have detected a new device.  Specify that we want active scanning and start the
    // scan to run indefinitely (by passing in 0 for the "duration")
    BLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(0, false);
}

int last_x = 0, last_y = 0;

int lastBlueDotX = blueDotX;
int lastBlueDotY = blueDotY;

void loop() {
    if (deviceConnected) {
        M5.Lcd.fillRect(blueDotX, blueDotY, 10, 10, BLACK);
        M5.Lcd.fillRect(pinkDotX, pinkDotY, 10, 10, BLACK);

        // Draw the new positions
        M5.Lcd.fillRect(blueDotX, blueDotY, 10, 10, BLUE);
        M5.Lcd.fillRect(pinkDotX, pinkDotY, 10, 10, PINK);
        if (!isGameOver) {
            delay(10);

            int x = 1023 - ss.analogRead(14);
            int y = 1023 - ss.analogRead(15);

            if (x > 560 && blueDotX < M5.Lcd.width() - 3) {
                blueDotX += 1 * blueDotMultiplier;
            }
            if (x < 500 && blueDotX > 3) {
                blueDotX -= 1 * blueDotMultiplier;
            }
            if (y < 490 && blueDotY < M5.Lcd.height() - 3) {
                blueDotY += 1 * blueDotMultiplier;
            }
            if (y > 550 && blueDotY > 3) {
                blueDotY -= 1 * blueDotMultiplier;
            }

            uint32_t buttons = ss.digitalReadBulk(button_mask);
            unsigned long currentTime = millis();

            if (!(buttons & (1UL << BUTTON_SELECT))) {
                if (currentTime - lastSelectPress >= 500) {
                    Point rand = getRandomNum();
                    blueDotX = rand.x;
                    blueDotY = rand.y;
                    lastSelectPress = currentTime;
                }
            }

            if (!(buttons & (1UL << BUTTON_START))) {
                if (blueDotMultiplier < 5) {
                    blueDotMultiplier++;
                } else {
                    blueDotMultiplier = 1;
                }
            }

            if (sqrt(pow(pinkDotX - blueDotX, 2) + pow(pinkDotY - blueDotY, 2)) <= 20) {
                // isGameOver = true;
                endTime = millis();
            }
            
            sendBlueDotCoordinates();

            // Erase previous position of blue dot
            M5.Lcd.fillRect(lastBlueDotX, lastBlueDotY, 10, 10, BLACK);

            // Draw new positions of dots
            M5.Lcd.fillRect(blueDotX, blueDotY, 10, 10, BLUE);
            M5.Lcd.fillRect(pinkDotX, pinkDotY, 10, 10, PINK);
            
            // Store last position
            lastBlueDotX = blueDotX;
            lastBlueDotY = blueDotY;
        } else {
            M5.Lcd.setCursor(M5.Lcd.width() / 3, M5.Lcd.height() / 2);
            M5.Lcd.setTextSize(3);
            M5.Lcd.printf("GAME OVER");

            int timeDiff = endTime - startTime;
            M5.Lcd.setCursor(M5.Lcd.width() / 3, M5.Lcd.height() / 2 + 50);
            M5.Lcd.printf("%ss", millisToTime(timeDiff));
        }
    } else {
        if (doConnect) {
            if (connectToServer()) {
                Serial.println("We are now connected to the BLE Server.");
                doConnect = false;
                delay(3000);
            } else {
                Serial.println("Failed to connect to the server.");
                delay(3000);
            }
        }

        if (deviceConnected) {
            String newValue = "Time since boot: " + String(millis() / 1000);
            Serial.println("Setting new characteristic value to " + newValue);
            bleRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());
        } else if (doScan) {
            drawScreenTextWithBackground("Disconnected....re-scanning for BLE server...", TFT_ORANGE);
            BLEDevice::getScan()->start(0);
        }
        delay(0);
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

///////////////////////////////////////////////////////////////
// Colors the background and then writes the text on top
///////////////////////////////////////////////////////////////
void drawScreenTextWithBackground(String text, int backgroundColor) {
    M5.Lcd.fillScreen(backgroundColor);
    M5.Lcd.setCursor(0,0);
    M5.Lcd.println(text);
}


///////////////////////////////////////////////////////////////
// Send Blue Dot Coordinates to BLE Client
///////////////////////////////////////////////////////////////
void sendBlueDotCoordinates() {
    if (deviceConnected && bleRemoteCharacteristicSend) {
        String coords = String(blueDotX) + "," + String(blueDotY);
        Serial.println("Client Sending X,Y: " + coords);
        bleRemoteCharacteristicSend->writeValue(coords.c_str(), coords.length());
    }
}

