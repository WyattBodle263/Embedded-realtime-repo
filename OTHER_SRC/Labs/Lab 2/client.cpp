#include "Adafruit_seesaw.h"
#include "M5Core2.h"
#include <random>
#include <BLEDevice.h>
#include <BLE2902.h>
#include <M5Core2.h>

///////////////////////////////////////////////////////////////
// Variables
///////////////////////////////////////////////////////////////
static BLERemoteCharacteristic *bleRemoteCharacteristic;
static BLEAdvertisedDevice *bleRemoteServer;
static boolean doConnect = false;
static boolean doScan = false;
bool deviceConnected = false;

Adafruit_seesaw ss;

// See the following for generating UUIDs: https://www.uuidgenerator.net/
static BLEUUID SERVICE_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b"); // Dr. Dan's Service
static BLEUUID CHARACTERISTIC_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8"); // Dr. Dan's Characteristic

// BLE Broadcast Name
static String BLE_BROADCAST_NAME = "abc";


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
void sendBlueDotCoordinates();
Point getRandomNum();

uint32_t button_mask = (1UL << BUTTON_START) | (1UL << BUTTON_SELECT);


///////////////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////////////
void drawScreenTextWithBackground(String text, int backgroundColor);

///////////////////////////////////////////////////////////////
// BLE Client Callback Methods
// This method is called when the server that this client is
// connected to NOTIFIES this client (or any client listening)
// that it has changed the remote characteristic
///////////////////////////////////////////////////////////////
static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
    Serial.printf("Notify callback for characteristic %s of data length %d\n", pBLERemoteCharacteristic->getUUID().toString().c_str(), length);
    Serial.printf("\tData: %s", (char *)pData);
    std::string value = pBLERemoteCharacteristic->readValue();
    Serial.printf("\tValue was: %s", value.c_str());
}

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
bool connectToServer()
{
    // Create the client
    Serial.printf("Forming a connection to %s\n", bleRemoteServer->getName().c_str());
    BLEClient *bleClient = BLEDevice::createClient();
    bleClient->setClientCallbacks(new MyClientCallback());
    Serial.println("\tClient connected");

    // Connect to the remote BLE Server.
    if (!bleClient->connect(bleRemoteServer))
        Serial.printf("FAILED to connect to server (%s)\n", bleRemoteServer->getName().c_str());
    Serial.printf("\tConnected to server (%s)\n", bleRemoteServer->getName().c_str());

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService *bleRemoteService = bleClient->getService(SERVICE_UUID);
    if (bleRemoteService == nullptr) {
        Serial.printf("Failed to find our service UUID: %s\n", SERVICE_UUID.toString().c_str());
        bleClient->disconnect();
        return false;
    }
    Serial.printf("\tFound our service UUID: %s\n", SERVICE_UUID.toString().c_str());

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    bleRemoteCharacteristic = bleRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
    if (bleRemoteCharacteristic == nullptr) {
        Serial.printf("Failed to find our characteristic UUID: %s\n", CHARACTERISTIC_UUID.toString().c_str());
        bleClient->disconnect();
        return false;
    }
    Serial.printf("\tFound our characteristic UUID: %s\n", CHARACTERISTIC_UUID.toString().c_str());

    // Read the value of the characteristic
    if (bleRemoteCharacteristic->canRead()) {
        std::string value = bleRemoteCharacteristic->readValue();
        Serial.printf("The characteristic value was: %s", value.c_str());
        // drawScreenTextWithBackground("Initial characteristic value read from server:\n\n" + String(value.c_str()), TFT_GREEN);
        delay(3000);
    }
    
    // Check if server's characteristic can notify client of changes and register to listen if so
    if (bleRemoteCharacteristic->canNotify())
        bleRemoteCharacteristic->registerForNotify(notifyCallback);

    //deviceConnected = true;
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


        // More debugging print
        // Serial.printf("\tAddress: %s\n", advertisedDevice.getAddress().toString().c_str());
        // Serial.printf("\tHas a ServiceUUID: %s\n", advertisedDevice.haveServiceUUID() ? "True" : "False");
        // for (int i = 0; i < advertisedDevice.getServiceUUIDCount(); i++) {
        //    Serial.printf("\t\t%s\n", advertisedDevice.getServiceUUID(i).toString().c_str());
        // }
        // Serial.printf("\tHas our service: %s\n\n", advertisedDevice.isAdvertisingService(SERVICE_UUID) ? "True" : "False");
        
        // We have found a device, let us now see if it contains the service we are looking for.
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

void loop() {

    if (deviceConnected){
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
            sendBlueDotCoordinates();    

    
    
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
    }else{
         // If the flag "doConnect" is true then we have scanned for and found the desired
    // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
    // connected we set the connected flag to be false.
    if (doConnect == true)
    {
        if (connectToServer()) {
            Serial.println("We are now connected to the BLE Server.");
            // drawScreenTextWithBackground("Connected to BLE server: " + String(bleRemoteServer->getName().c_str()), TFT_GREEN);
            doConnect = false;
            delay(3000);
        }
        else {
            Serial.println("We have failed to connect to the server; there is nothin more we will do.");
            // drawScreenTextWithBackground("FAILED to connect to BLE server: " + String(bleRemoteServer->getName().c_str()), TFT_GREEN);
            delay(3000);
        }
    }

    // If we are connected to a peer BLE Server, update the characteristic each time we are reached
    // with the current time since boot.
    if (deviceConnected)
    {
        // Format string to send to server
        String newValue = "Time since boot: " + String(millis() / 1000);
        Serial.println("Setting new characteristic value to \"" + newValue + "\"");

        // Set the characteristic's value to be the array of bytes that is actually a string.
        bleRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());
        // drawScreenTextWithBackground("Wrote to server:\n\n" + String(newValue.c_str()), TFT_YELLOW); // Give feedback on screen
    }
    else if (doScan) {
        drawScreenTextWithBackground("Disconnected....re-scanning for BLE server...", TFT_ORANGE);
        BLEDevice::getScan()->start(0); // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
    }
    delay(1000); // Delay a second between loops.
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
    if (deviceConnected && bleRemoteCharacteristic) {
        String coords = String(blueDotX) + "," + String(blueDotY);
        Serial.println("Sending Blue Dot Coordinates: " + coords);
        bleRemoteCharacteristic->writeValue(coords.c_str(), coords.length());
    }
}
