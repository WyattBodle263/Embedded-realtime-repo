#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "WiFi.h"
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Adafruit_VCNL4040.h>
#include "Adafruit_SHT4x.h"

const char* ssid = "bula bula";
const char* password = "7608810449";

enum State {S_ONE, S_TWO, S_THREE};

const String URL_GCF_UPLOAD = "https://send-data-420625770268.us-west2.run.app";
const String URL_GCF_GET = "https://get-data-420625770268.us-west2.run.app";

Adafruit_VCNL4040 vcnl4040 = Adafruit_VCNL4040();
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

String thisPayload = "";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 + 1620);

String userId = "User2";

static State state = S_ONE;
static unsigned long lastTS = 0;
static unsigned long timeChangeInterval = 5000;
static bool stateChangedThisLoop = false;

struct deviceDetails {
    int prox, ambientLight, whiteLight;
    double temp, rHum, accX, accY, accZ;
};

struct buttonState {
    String text;
    bool selected;
    int x, y, w, h, row;
};

// Buttons
buttonState uploadButton_s1 = {"Upload", false, 200,45, 100,50,0};
buttonState userButton_s1 = {"User1", true, 20,10, 80,50,1};
buttonState userButton_s2 = {"User2", false, 120,10, 80,50,1};
buttonState bothButton_s2 = {"Both", false, 220,10, 80,50,1};

buttonState timeButton_s1 = {"5s", true, 24,70, 50,50,2};
buttonState timeButton_s2 = {"30s", false, 98,70, 50,50,2};
buttonState timeButton_s3 = {"60s", false, 172,70, 50,50,2};
buttonState timeButton_s4 = {"5m", false, 246,70, 50,50,2};

buttonState typeButton_s1 = {"Temp", true, 20,130, 80,50,3};
buttonState typeButton_s2 = {"Humidity", false, 120,130, 80,50,3};
buttonState typeButton_s3 = {"Lux", false, 220,130, 80,50,3};

buttonState uploadButton_s2 = {"Get Average", false, 60,190, 200,50,4};

// Array of pointers to buttons
int buttonCount = 11;
buttonState* buttonArr[11] = {
    &userButton_s1, &userButton_s2, &bothButton_s2,
    &timeButton_s1, &timeButton_s2, &timeButton_s3, &timeButton_s4,
    &typeButton_s1, &typeButton_s2, &typeButton_s3, &uploadButton_s2
};

// Function declarations
bool sendDataToCloud();
bool getDataFromCloud();
String createJsonPayload(deviceDetails* details);
void displayResponseData(String payload);
deviceDetails getSensorData();
bool isButtonPressed(int touchX, int touchY, buttonState button);
void drawScreenTwo();
void handlePress(int row);
String getSelectedButtonStates();
void drawReadingsDisplay();
void drawReadingsDisplayPage3();

void setup() {
    M5.begin();
    M5.Lcd.setTextSize(2);
    M5.IMU.Init();
    Serial.begin(115200);  
    stateChangedThisLoop = true;

    if (!vcnl4040.begin()) {
        M5.Lcd.println("VCNL4040 not found!"); while (1);
    }
    if (!sht4.begin()) {
        M5.Lcd.println("SHT4x not found!"); while (1);
    }
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);

    M5.Lcd.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        M5.Lcd.print(".");
    }
    M5.Lcd.println("\nConnected!");
    timeClient.begin();
}

void loop() {
    M5.update();

    if (M5.BtnA.wasPressed()) state = S_ONE;
    if (M5.BtnB.wasPressed()) state = S_TWO;

    if (state == S_ONE) {
        if (millis() >= lastTS + timeChangeInterval) {
            lastTS = millis();  
            drawReadingsDisplay();
        }
        if (M5.Touch.ispressed()) {
            auto t = M5.Touch.getPressPoint();
            if (isButtonPressed(t.x, t.y, uploadButton_s1)) {
                M5.Lcd.fillScreen(BLACK);
                if (sendDataToCloud()) drawReadingsDisplay();
            }
        }
    } else if (state == S_TWO) {
        if (stateChangedThisLoop) drawScreenTwo();
        stateChangedThisLoop = false;

        if (M5.Touch.ispressed()) {
            auto t = M5.Touch.getPressPoint();
            for (int i = 0; i < buttonCount; i++) {
                if (isButtonPressed(t.x, t.y, *buttonArr[i])) {
                    handlePress(buttonArr[i]->row);
                    buttonArr[i]->selected = !buttonArr[i]->selected;
                    stateChangedThisLoop = true;
                }
            }
        }
    } else if (state == S_THREE) {
        // ...
    }
}

void handlePress(int row) {
    if (row == 1) for (int i = 0; i < 3; i++) buttonArr[i]->selected = false;
    else if (row == 2) for (int i = 3; i < 7; i++) buttonArr[i]->selected = false;
    else if (row == 4) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.print("GETTING DATA...");
        if (getDataFromCloud()) state = S_THREE;
    }
}

void drawScreenTwo() {
    M5.Lcd.fillScreen(BLACK);
    for (int i = 0; i < buttonCount; i++) {
        uint16_t color = (i == buttonCount - 1) ? GREEN : (buttonArr[i]->selected ? RED : WHITE);
        M5.Lcd.fillRect(buttonArr[i]->x, buttonArr[i]->y, buttonArr[i]->w, buttonArr[i]->h, color);
        M5.Lcd.setTextColor(BLACK);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setCursor(buttonArr[i]->x + 10, buttonArr[i]->y + 15);
        M5.Lcd.print(buttonArr[i]->text);
    }
}

bool isButtonPressed(int x, int y, buttonState button) {
    return (x > button.x && x < (button.x + button.w) &&
            y > button.y && y < (button.y + button.h));
}

void drawReadingsDisplay() {
    deviceDetails details = getSensorData();
    M5.Lcd.fillScreen(TFT_CYAN);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 10); M5.Lcd.printf("Temp: %0.f°C", details.temp);
    M5.Lcd.setCursor(150, 10); M5.Lcd.printf("ID: %s", userId.c_str());
    M5.Lcd.setCursor(10, 45); M5.Lcd.printf("Humidity: %.0f%%", details.rHum);
    M5.Lcd.setCursor(10, 80); M5.Lcd.printf("ACC X: %0.f", details.accX);
    M5.Lcd.setCursor(10, 115); M5.Lcd.printf("ACC Y: %0.f", details.accY);
    M5.Lcd.setCursor(10, 150); M5.Lcd.printf("ACC Z: %0.f", details.accZ);
    M5.Lcd.setCursor(10, 185); M5.Lcd.printf("White Light: %0.f", details.ambientLight);

    int hours = timeClient.getHours();
    int minutes = timeClient.getMinutes();
    int seconds = timeClient.getSeconds();
    bool isPM = hours >= 12;
    int displayHours = (hours % 12 == 0) ? 12 : hours % 12;
    M5.Lcd.setCursor(10, 220);
    M5.Lcd.printf("Update Time: %02d:%02d:%02d %s", displayHours, minutes, seconds, isPM ? "PM" : "AM");

    M5.Lcd.fillRect(uploadButton_s1.x, uploadButton_s1.y, uploadButton_s1.w, uploadButton_s1.h, uploadButton_s1.selected ? RED : WHITE);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.setCursor(uploadButton_s1.x + 20, uploadButton_s1.y + 15);
    M5.Lcd.print(uploadButton_s1.text);
}

void drawReadingsDisplayPage3() {
    StaticJsonDocument<512> jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, thisPayload);
    if (error) {
        Serial.println("JSON Parse Error!");
        return;
    }

    int y = 10;
    M5.Lcd.fillScreen(PINK);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.setTextSize(2);
    if(thisPayload != ""){
        if (typeButton_s1.selected) {
            M5.Lcd.setCursor(10, y); 
            M5.Lcd.printf("Temp: %.2f C\n", jsonDoc["averageTemperature"].as<double>());
            y += 25;
        }
    
        M5.Lcd.setCursor(180, 10);
        if (userButton_s1.selected) M5.Lcd.printf("ID: %s", userButton_s1.text.c_str());
        else if (userButton_s2.selected) M5.Lcd.printf("ID: %s", userButton_s2.text.c_str());
        else M5.Lcd.printf("ID: %s", bothButton_s2.text.c_str());
    
        if (typeButton_s2.selected) {
            M5.Lcd.setCursor(10, y); 
            M5.Lcd.printf("Humidity: %.2f%%\n", jsonDoc["averageHumidity"].as<double>());
            y += 25;
        }
    
        if (typeButton_s3.selected) {
            M5.Lcd.setCursor(10, y); 
            M5.Lcd.printf("White Light: %.2f Lux\n", jsonDoc["averageLux"].as<double>());
            y += 25;
        }
    
        M5.Lcd.setCursor(10, y); M5.Lcd.print("Time Range:");
        y += 25;
        M5.Lcd.setCursor(10, y); 
        String timeSpan = jsonDoc["timeSpan"].as<String>();
        int hyphenPos = timeSpan.indexOf(" -");
        if (hyphenPos != -1) {
            timeSpan = timeSpan.substring(0, hyphenPos + 2) + "\n" + timeSpan.substring(hyphenPos + 2);
        }
        M5.Lcd.print(timeSpan);
                y += 50;
        M5.Lcd.setCursor(10, y); 
        int dataPointCount = jsonDoc["dataPointCount"].as<int>();
        M5.Lcd.printf("Num Data Points: %d\n", dataPointCount);
        y += 25;
        M5.Lcd.setCursor(10, y); 
        M5.Lcd.printf("%.2f datapoints/seconds\n", jsonDoc["dataPointsPerSecond"].as<double>());
    }
}

deviceDetails getSensorData() {
    deviceDetails d;
    d.prox = vcnl4040.getProximity();
    d.ambientLight = vcnl4040.getLux();
    d.whiteLight = vcnl4040.getWhiteLight();
    sensors_event_t rHum, temp;
    sht4.getEvent(&rHum, &temp);
    d.temp = temp.temperature;
    d.rHum = rHum.relative_humidity;
    float accX, accY, accZ;
    M5.IMU.getAccelData(&accX, &accY, &accZ);
    d.accX = accX * 9.8;
    d.accY = accY * 9.8;
    d.accZ = accZ * 9.8;
    return d;
}

// Function to collect sensor data and send it to Cloud
bool sendDataToCloud() {
    M5.Lcd.fillScreen(PURPLE);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("Reading sensors...");

    // Read VCNL4040 Sensors
    deviceDetails details;
    details.prox = vcnl4040.getProximity();
    details.ambientLight = vcnl4040.getLux();
    details.whiteLight = vcnl4040.getWhiteLight();

    // Read SHT40 Sensors
    sensors_event_t rHum, temp;
    sht4.getEvent(&rHum, &temp);
    details.temp = temp.temperature;
    details.rHum = rHum.relative_humidity;

    // Read Accelerometer
    float accX, accY, accZ;
    M5.IMU.getAccelData(&accX, &accY, &accZ);
    details.accX = accX * 9.8;
    details.accY = accY * 9.8;
    details.accZ = accZ * 9.8;

    // Create JSON payload
    String jsonPayload = createJsonPayload(&details);
    Serial.print(jsonPayload);


    // Send data via HTTP
    HTTPClient http;
    http.begin(URL_GCF_UPLOAD);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("M5-Details", jsonPayload);  // Add M5-details header here
    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("Response: " + response);  // Print response from server
    } 
        Serial.println("HTTP Error: " + String(httpResponseCode));  // Print HTTP error code

    http.end();
    return (httpResponseCode == 200);
}

bool getDataFromCloud() {
    HTTPClient http;
    http.begin(URL_GCF_GET);
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"buttonStates\": " + getSelectedButtonStates();
    payload += ", \"userId\": \"" + userId + "\"}";
    
    int code = http.POST(payload);
    Serial.print("Payload: ");
    Serial.println(payload);
    if (code > 0) {
        thisPayload = http.getString();
        displayResponseData(thisPayload);
    } else {
        M5.Lcd.println("HTTP Error: " + String(http.errorToString(code).c_str()));
        return false;
    }

    http.end();
    if(thisPayload.indexOf("error") >= 0){
        M5.Lcd.fillScreen(PINK);
        M5.Lcd.setTextColor(BLACK);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setCursor(10,10);
        M5.Lcd.print(thisPayload);
    }else{
        drawReadingsDisplayPage3();
    }
    return true;
}

String getSelectedButtonStates() {
    String states = "[";
    for (int i = 0; i < buttonCount; i++) {
        if (buttonArr[i]->text != "Get Average" && buttonArr[i]->text != "Upload") {
            states += "{\"" + buttonArr[i]->text + "\": " + (buttonArr[i]->selected ? "true" : "false") + "}";
            if (i < buttonCount - 2) states += ", ";
        }
    }
    states += "]";
    return states;
}

void displayResponseData(String payload) {
    Serial.println("Cloud Response: " + payload);
}

String createJsonPayload(deviceDetails* details) {
    StaticJsonDocument<512> jsonDoc;
    jsonDoc["vcnlDetails"]["prox"] = details->prox;
    jsonDoc["vcnlDetails"]["al"] = details->ambientLight;
    jsonDoc["vcnlDetails"]["rwl"] = details->whiteLight;
    jsonDoc["shtDetails"]["temp"] = details->temp;
    jsonDoc["shtDetails"]["rHum"] = details->rHum;
    jsonDoc["m5Details"]["ax"] = details->accX;
    jsonDoc["m5Details"]["ay"] = details->accY;
    jsonDoc["m5Details"]["az"] = details->accZ;
    timeClient.update();
    int epochTime = timeClient.getEpochTime();
    jsonDoc["otherDetails"]["timeCaptured"] = epochTime;
    jsonDoc["otherDetails"]["userId"] = userId;

    String jsonString;
    serializeJson(jsonDoc, jsonString);  
    return jsonString; 
}
