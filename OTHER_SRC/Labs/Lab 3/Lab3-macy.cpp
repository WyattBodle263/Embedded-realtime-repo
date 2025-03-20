#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "WiFi.h"
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Adafruit_VCNL4040.h>
#include "Adafruit_SHT4x.h"


// WiFi Credentials
const char* ssid = "CBU-LANCERS";
const char* password = "L@ncerN@tion";

// Cloud Function URLs
const String URL_GCF_UPLOAD = "https://servicename-887918089944.us-central1.run.app";
const String URL_GCF_GET = "https://latest-sensor-data-887918089944.us-central1.run.app";

// Sensor Objects
Adafruit_VCNL4040 vcnl4040 = Adafruit_VCNL4040();
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

// WiFiUDP ntpUDP;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -7 * 3600); // UTC-7 for PST

// User Details
String userId = "Macy2025";

// Structure for Device Data
struct deviceDetails {
    int prox;
    int ambientLight;
    int whiteLight;
    double temp;
    double rHum;
    double accX;
    double accY;
    double accZ;
};

// Function Prototypes
bool sendDataToCloud();
bool getDataFromCloud();
String createJsonPayload(deviceDetails* details);
void displayResponseData(String payload);
String formatTime12Hour(int epochTime);
// String getCurrentTime12Hour();

void setup() {
    M5.begin();
    M5.Lcd.setTextSize(2);
    M5.IMU.Init();
    Serial.begin(115200);  


    // Initialize Sensors
    if (!vcnl4040.begin()) {
        M5.Lcd.println("VCNL4040 not found!");
        while (1);
    }
    if (!sht4.begin()) {
        M5.Lcd.println("SHT4x not found!");
        while (1);
    }
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);

    // Connect to WiFi
    M5.Lcd.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        M5.Lcd.print(".");
    }
    M5.Lcd.println("\nConnected!");

    // Start NTP time
    timeClient.begin();

}

void loop() {
    M5.update();

    // If ButtonA is pressed, send data
    if (M5.BtnA.wasPressed()) {
        if (sendDataToCloud()) {
            M5.Lcd.println("Data Sent!");
        } else {
            M5.Lcd.println("Send Failed!");
        }
    }

    // If ButtonB is pressed, get data
    if (M5.BtnB.wasPressed()) {
        if (getDataFromCloud()) {
            Serial.println("Data Received!");
        } else {
            M5.Lcd.println("Fetch Failed!");
        }
    }

    delay(100);
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

// Function to fetch and display data from Cloud
bool getDataFromCloud() {
    HTTPClient http;
    http.begin(URL_GCF_GET);  // Only use the base URL
    http.addHeader("Content-Type", "application/json");
    
    String fullURL = "/?userId=" + userId;  
    http.setURL(fullURL);  // Correctly add query parameter

    int httpCode = http.GET();
    if (httpCode > 0) {
        String payload = http.getString();
        displayResponseData(payload);
        Serial.println("Response: " + payload);
    } else {
        return false;
        M5.Lcd.println("HTTP Error: " + String(http.errorToString(httpCode).c_str()));
    }

    http.end();
    return true;
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


// Function to display received JSON response
void displayResponseData(String payload) {
    StaticJsonDocument<512> jsonDoc;
DeserializationError error = deserializeJson(jsonDoc, payload);
if (error) {
    M5.Lcd.println("JSON Parse Error!");
    return;
}
if (jsonDoc["otherDetails"].containsKey("timeCaptured")) {
    Serial.println(jsonDoc["otherDetails"]["timeCaptured"].as<int>());
} else {
    Serial.println("Key not found!");
}
    M5.Lcd.setTextSize(3);
    M5.Lcd.fillScreen(PINK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.printf("Temp: %.2f C\n", jsonDoc["shtDetails"]["temp"].as<double>());
    M5.Lcd.setCursor(0, 40);
    M5.Lcd.printf("Humidity: %.2f%%\n", jsonDoc["shtDetails"]["rHum"].as<double>());
    M5.Lcd.setCursor(0, 90);
    // Serial.println("Debug: Before citical function");
     M5.Lcd.println("Cloud Upload Time");
    M5.Lcd.printf("%s\n", formatTime12Hour(jsonDoc["otherDetails"]["cloudUploadTime"].as<int>()));
    Serial.print(jsonDoc["otherDetails"]["cloudUploadTime"].as<int>());
    M5.Lcd.setCursor(0, 150);
    timeClient.update();
    int epochTime = timeClient.getEpochTime();
    M5.Lcd.println("Data Capture Time");
    M5.Lcd.printf("%s\n", formatTime12Hour(jsonDoc["otherDetails"]["timeCaptured"].as<int>()));
    // Serial.println("Debug: After critical function");
}

String formatTime12Hour(int epochTime) {
    // Convert the epoch time into struct tm
    struct tm *timeInfo;
    time_t rawTime = epochTime;
    timeInfo = localtime(&rawTime);
  
    // Get the hour, minute, and AM/PM
    int hour = timeInfo->tm_hour;
    int minute = timeInfo->tm_min;
    String ampm = (hour >= 12) ? "PM" : "AM";
  
    // Convert to 12-hour format
    if (hour > 12) {
      hour -= 12;
    } else if (hour == 0) {
      hour = 12; // Midnight is 12 AM
    }

   // Create a buffer to hold the formatted time string
  char formattedTime[9];  // hh:mm AM/PM format is 8 characters + null terminator
  
  // Use snprintf to format the time
  snprintf(formattedTime, sizeof(formattedTime), "%02d:%02d %s", hour, minute, ampm.c_str());

  return String(formattedTime);  // Return as String
  
}
