#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "EGR425_Phase1_weather_bitmap_images.h"
#include "WiFi.h"
#include <NTPClient.h>
#include <string>
#include <sstream>
#include "I2C_RW.h"
#include <Wire.h>


////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////
// TODO 3: Register for openweather account and get API key
// String urlOpenWeather = "http://api.openweathermap.org/geo/1.0/zip?";
String urlOpenWeather = "https://api.openweathermap.org/data/2.5/weather?";
String apiKey = "83b2a51482fb2bf8884ead1e4b348e15";


// TODO 1: WiFi variables
String wifiNetworkName = "CBU-LANCERS";
String wifiPassword = "L@ncerN@tion";

//Pins
int const PIN_SDA = 32;
int const PIN_SCL = 33;

// Time variables
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;  // 5000; 5 minutes (300,000ms) or 5 seconds (5,000ms)
unsigned long localTimerDelay = 5000;  // 5000; 5 minutes (300,000ms) or 5 seconds (5,000ms)
unsigned long lastZipRefresh = 0;  
unsigned long lastLocalRefresh = 0;  
const unsigned long zipRefreshDelay = 500; 

//Time Stamp Variables using NTPClient Library
WiFiUDP ntpUDP;
int timeOffset = -8 * 3600; // Adjust based on your time zone (-8 for Pacific Standard Time)
NTPClient timeClient(ntpUDP, "pool.ntp.org", timeOffset);
NTPClient localTimeClient(ntpUDP, "pool.ntp.org", timeOffset);

// LCD variables
int sWidth;
int sHeight;
#define MAX_CHARS_PER_LINE 20  // Set the max number of characters per line (adjust if needed)

// Weather/zip variables
String strWeatherIcon;
String strWeatherDesc;
String cityName;
double tempNow;
double tempNowLocal;
double tempMin;
double tempMax;

//Sensor Variables
int proximity;       
int ambient_light;   
int white_light;     
float humidity;     
float localTempNow;
int brightness = 128;

//Boolean Screen Flags
boolean isFarenheit = true;
boolean prevTempMeasure = true;
boolean inZipCodeMode = false;
boolean inLocalMode = false;

//Zip code start 
int inputNumber = 92504;

///////////////////////////////////////////////////////////////
// Register defines
///////////////////////////////////////////////////////////////

//VCNL4040
#define VCNL_I2C_ADDRESS 0x60
#define VCNL_REG_PROX_DATA 0x08
#define VCNL_REG_ALS_DATA 0x09
#define VCNL_REG_WHITE_DATA 0x0A

#define VCNL_REG_PS_CONFIG 0x03
#define VCNL_REG_ALS_CONFIG 0x00
#define VCNL_REG_WHITE_CONFIG 0x04

//SHT40
#define SHT40_I2C_ADDRESS 0x44
#define SHT40_REG_MEASURE_HIGH_REPEATABILITY 0xFD // Command to start measurement

////////////////////////////////////////////////////////////////////
// Method header declarations
////////////////////////////////////////////////////////////////////
String convertTo12HourFormat(int hours, int minutes, int seconds);
void drawWeatherImage(String iconId, int resizeMult);
String formatTextWithNewlines(const String& text);
String httpGETRequest(const char* serverName);
void drawWeatherDisplayLocal();
void fetchLocalWeatherDetails();
void fetchWeatherDetails();
void drawWeatherDisplay();
void drawZipCodeDisplay();
void fetchVCNL4040Data();
void fetchSHT40Data();
boolean turnOffScreen();
void getData();

///////////////////////////////////////////////////////////////
// Put your setup code here, to run once
///////////////////////////////////////////////////////////////
void setup() {
   // Initialize the device
   M5.begin();
   Wire.begin();


   //Initalize I2C Connection
    I2C_RW::initI2C(VCNL_I2C_ADDRESS, 400000, PIN_SDA, PIN_SCL);
    // I2C_RW::initI2C(SHT40_I2C_ADDRESS, 400000, PIN_SDA, PIN_SCL);

     // Write registers to initialize/enable VCNL sensors
     I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_PS_CONFIG, 2, 0x0800, " to enable proximity sensor", true);
     I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_ALS_CONFIG, 2, 0x0000, " to enable ambient light sensor", true);
     I2C_RW::writeReg8Addr16DataWithProof(VCNL_REG_WHITE_CONFIG, 2, 0x0000, " to enable raw white light sensor", true);
 
   // Set screen orientation and get height/width
   sWidth = M5.Lcd.width();
   sHeight = M5.Lcd.height();

   // TODO 2: Connect to WiFi
   WiFi.begin(wifiNetworkName.c_str(), wifiPassword.c_str());
   Serial.printf("Connecting");
   while (WiFi.status() != WL_CONNECTED) {
       delay(500);
       Serial.print(".");
   }
   Serial.print("\n\nConnected to WiFi network with IP address: ");
   Serial.println(WiFi.localIP());

    timeClient.begin();
    localTimeClient.begin();
}


///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop() {
    M5.update();
    //Button A Sets display to API Weather
    if (M5.BtnA.wasPressed()) {
        isFarenheit = !isFarenheit;
        if(!inLocalMode && !inZipCodeMode){
            drawWeatherDisplay();  
        }else if(!inZipCodeMode){
            drawWeatherDisplayLocal();
        }
    }

    //Button B Sets Display to Zip Code Selection
    if (M5.BtnB.wasPressed()) {
        inZipCodeMode = !inZipCodeMode;
    }

    //Button C Sets Display to Local Weather
    if (M5.BtnC.wasPressed()) {
        inLocalMode = !inLocalMode;
    }

    //If in Zip code mode draw zip code and get data
    if (inZipCodeMode) {
        M5.Axp.SetLcdVoltage(3300);
        getData();
        if ((millis() - lastZipRefresh) > zipRefreshDelay) { //Set zip code refresh rate
            drawZipCodeDisplay();
            lastZipRefresh = millis();
        }
    } else if (inLocalMode){ //If in Local code mode draw Local and get data
        //Turn off screen if proximity is close
        fetchVCNL4040Data();
        if(!turnOffScreen()){
            M5.Lcd.wakeup();
        }else{
            M5.Lcd.sleep();
        }

        // Define the ranges
        const int minAmbient = 0;      // Minimum ambient light reading
        const int maxAmbient = 1500;   // Maximum ambient light reading
        const int minBrightness = 2500; // Minimum brightness level
        const int maxBrightness = 3300; // Maximum brightness level

        // Calculate the range of ambient light
        int ambientRange = maxAmbient - minAmbient;
        
        // Calculate the range of brightness
        int brightnessRange = maxBrightness - minBrightness;

        // Map the ambient light reading to brightness
        int mappedBrightness = minBrightness + ((ambient_light - minAmbient) * brightnessRange) / ambientRange;

        M5.Axp.SetLcdVoltage(constrain(mappedBrightness, minBrightness, maxBrightness));
        
        if ((millis() - lastLocalRefresh) > localTimerDelay) {
                localTimeClient.update();
                fetchLocalWeatherDetails();
                    M5.Lcd.wakeup();
                    drawWeatherDisplayLocal();  
 
            lastLocalRefresh = millis();
        }
    }else if (!inLocalMode && !inZipCodeMode) { //If in Normal mode draw Normal and get data
        M5.Axp.SetLcdVoltage(3300);
        if ((millis() - lastTime) > timerDelay) {
            if (WiFi.status() == WL_CONNECTED) {
                timeClient.update();
                fetchWeatherDetails();
                drawWeatherDisplay();  
            } else {
                Serial.println("WiFi Disconnected");
            }
            lastTime = millis();
        }
    }
}

/////////////////////////////////////////////////////////////////
// This method fetches the weather details from the OpenWeather
// API and saves them into the fields defined above
/////////////////////////////////////////////////////////////////
void fetchWeatherDetails() {
   //////////////////////////////////////////////////////////////////
   // Hardcode the specific city,state,country into the query
   // Examples: https://api.openweathermap.org/data/2.5/weather?q=riverside,ca,usa&units=imperial&appid=YOUR_API_KEY
   //////////////////////////////////////////////////////////////////
   String serverURL = urlOpenWeather + "zip=" + inputNumber +",US&units=imperial&appid=" + apiKey;
   Serial.println(serverURL); // Debug print


   //////////////////////////////////////////////////////////////////
   // Make GET request and store reponse
   //////////////////////////////////////////////////////////////////
   String response = httpGETRequest(serverURL.c_str());
   //Serial.print(response); // Debug print

   //////////////////////////////////////////////////////////////////
   // Import ArduinoJSON Library and then use arduinojson.org/v6/assistant to
   // compute the proper capacity (this is a weird library thing) and initialize
   // the json object
   //////////////////////////////////////////////////////////////////
   const size_t jsonCapacity = 768+250;
   DynamicJsonDocument objResponse(jsonCapacity);


   //////////////////////////////////////////////////////////////////
   // Deserialize the JSON document and test if parsing succeeded
   //////////////////////////////////////////////////////////////////
   DeserializationError error = deserializeJson(objResponse, response);
   if (error) {
       Serial.print(F("deserializeJson() failed: "));
       Serial.println(error.f_str());
       return;
   }
   //serializeJsonPretty(objResponse, Serial); // Debug print


   //////////////////////////////////////////////////////////////////
   // Parse Response to get the weather description and icon
   //////////////////////////////////////////////////////////////////
   JsonArray arrWeather = objResponse["weather"];
   JsonObject objWeather0 = arrWeather[0];
   String desc = objWeather0["main"];
   String icon = objWeather0["icon"];
   String city = objResponse["name"];


   // ArduionJson library will not let us save directly to these
   // variables in the 3 lines above for unknown reason
   strWeatherDesc = desc;
   strWeatherIcon = icon;
   cityName = city;


   // Parse response to get the temperatures
   JsonObject objMain = objResponse["main"];
   tempNow = objMain["temp"];
   tempMin = objMain["temp_min"];
   tempMax = objMain["temp_max"];

   Serial.printf("NOW: %.1f F and %s\tMIN: %.1f F\tMax: %.1f F\n", tempNow, strWeatherDesc, tempMin, tempMax);
}


/////////////////////////////////////////////////////////////////
// Update the display based on the weather variables defined
// at the top of the screen.
/////////////////////////////////////////////////////////////////
void drawWeatherDisplay() {
    static String lastCityName = "";
    static float lastTempNow = 0;
    static float lastTempMin = 0;
    static float lastTempMax = 0;
    static String lastWeatherIcon = "";

    // Only update if values change to prevent unnecessary flickering
        lastCityName = cityName;
        lastTempNow = tempNow;
        lastTempMin = tempMin;
        lastTempMax = tempMax;
        lastWeatherIcon = strWeatherIcon;
        prevTempMeasure = isFarenheit;

        // Now proceed with full screen refresh
        uint16_t primaryTextColor;
        if (strWeatherIcon.indexOf("d") >= 0) {
            M5.Lcd.fillScreen(TFT_CYAN);
            primaryTextColor = TFT_DARKGREY;
        } else {
            M5.Lcd.fillScreen(TFT_NAVY);
            primaryTextColor = TFT_WHITE;
        }

        drawWeatherImage(strWeatherIcon, 3);

        M5.Lcd.setCursor(10, 10);
        M5.Lcd.setTextColor(primaryTextColor);
        M5.Lcd.setTextSize(3);
        M5.Lcd.printf("%s", cityName.c_str());

        M5.Lcd.setCursor(10, 50);
        M5.Lcd.setTextSize(10);
        if (isFarenheit) {
            M5.Lcd.printf("%0.fF\n", tempNow);
        } else {
            M5.Lcd.printf("%0.fC\n", (tempNow - 32) * 5 / 9);
        }

        M5.Lcd.setCursor(10, 110);
        M5.Lcd.setTextSize(3);
        M5.Lcd.setTextColor(TFT_RED);
        M5.Lcd.printf("HI: %0.fF\n", tempMax);

        M5.Lcd.setCursor(10, 150);
        M5.Lcd.printf("LO: %0.fF\n", tempMin);

        int hours = timeClient.getHours();
        int minutes = timeClient.getMinutes();
        int seconds = timeClient.getSeconds();

        // Convert to 12-hour format
        bool isPM = hours >= 12;
        int displayHours = (hours % 12 == 0) ? 12 : hours % 12;
        M5.Lcd.setTextColor(TFT_BLACK);
        // Print formatted time
        M5.Lcd.setCursor(10, 180);
        M5.Lcd.printf("%02d:%02d:%02d %s\n", displayHours, minutes, seconds, isPM ? "PM" : "AM");
    
}



String formatTextWithNewlines(const String& text) {
    String result = "";  // The result string to hold the formatted output
    String word = "";
    int charCount = 0;

    for (size_t i = 0; i < text.length(); i++) {
        char currentChar = text[i];

        // If current character is space or we're at the end of the text
        if (currentChar == ' ' || i == text.length() - 1) {
            // Check if adding this word would exceed the max character limit
            if (charCount + word.length() > MAX_CHARS_PER_LINE) {
                result += "\n";  // Add a newline to the result
                charCount = 0;   // Reset character count
            }

            result += word + " ";  // Append the word and a space
            charCount += word.length() + 1;  // Update character count (+1 for space)
            word = "";  // Reset word after printing
        } else {
            word += currentChar;  // Build the word
        }
    }

    return result;  // Return the formatted text with newlines
}

void getData() {
    Point p = M5.Touch.getPressPoint();  // Get touch point

    if (M5.Touch.ispressed()) {
      int x = p.x;
      int y = p.y;

      // Check if Done button is pressed (Updated coordinates)
      if (x > 250 && x < 300 && y > 200 && y < 240) {
        M5.Lcd.setCursor(20, 150);
        M5.Lcd.print("Done! Number: ");
        M5.Lcd.println(inputNumber);
        delay(1000);
        drawZipCodeDisplay();
        inZipCodeMode = !inZipCodeMode;
    }
    // Check if Back button is pressed (Updated coordinates)
    else if (x > 250 && x < 300 && y > 150 && y < 190) {
        if (inputNumber > 0) {
            inputNumber /= 10;
        }
        drawZipCodeDisplay();
    }
      // Check if number buttons are pressed (1-9)
      else {
        // Number 1 button area
        if (x > 20 && x < 80 && y > 60 && y < 120) {
          inputNumber = inputNumber * 10 + 1;
          drawZipCodeDisplay();
        }
        // Number 2 button area
        else if (x > 90 && x < 150 && y > 60 && y < 120) {
          inputNumber = inputNumber * 10 + 2;
          drawZipCodeDisplay();
        }
        // Number 3 button area
        else if (x > 160 && x < 220 && y > 60 && y < 120) {
          inputNumber = inputNumber * 10 + 3;
          drawZipCodeDisplay();
        }// Number 0 button area
        else if (x > 230 && x < 290 && y > 60 && y < 120) {
            inputNumber = inputNumber * 10;
            drawZipCodeDisplay();
          }
        // Number 4 button area
        else if (x > 20 && x < 80 && y > 130 && y < 190) {
          inputNumber = inputNumber * 10 + 4;
          drawZipCodeDisplay();
        }
        // Number 5 button area
        else if (x > 90 && x < 150 && y > 130 && y < 190) {
          inputNumber = inputNumber * 10 + 5;
          drawZipCodeDisplay();
        }
        // Number 6 button area
        else if (x > 160 && x < 220 && y > 130 && y < 190) {
          inputNumber = inputNumber * 10 + 6;
          drawZipCodeDisplay();
        }
        // Number 7 button area
        else if (x > 20 && x < 80 && y > 200 && y < 260) {
          inputNumber = inputNumber * 10 + 7;
          drawZipCodeDisplay();
        }
        // Number 8 button area
        else if (x > 90 && x < 150 && y > 200 && y < 260) {
          inputNumber = inputNumber * 10 + 8;
          drawZipCodeDisplay();
        }
        // Number 9 button area
        else if (x > 160 && x < 220 && y > 200 && y < 260) {
          inputNumber = inputNumber * 10 + 9;
          drawZipCodeDisplay();
        }
      }
    }
}

void drawZipCodeDisplay() {
    //////////////////////////////////////////////////////////////////
    // Draw background - light blue if day time and navy blue at night
    //////////////////////////////////////////////////////////////////
    uint16_t primaryTextColor;
    if (strWeatherIcon.indexOf("d") >= 0) {
      M5.Lcd.fillScreen(TFT_CYAN);
      primaryTextColor = TFT_DARKGREY;
    } else {
      M5.Lcd.fillScreen(TFT_NAVY);
      primaryTextColor = TFT_WHITE;
    }
    M5.Lcd.setTextSize(2);


    // Draw numbers (1-9)
    M5.Lcd.fillRect(20, 60, 60, 60, TFT_WHITE); M5.Lcd.setTextColor(TFT_BLACK); M5.Lcd.setCursor(40, 80); M5.Lcd.print("1");
    M5.Lcd.fillRect(90, 60, 60, 60, TFT_WHITE); M5.Lcd.setCursor(110, 80); M5.Lcd.print("2");
    M5.Lcd.fillRect(160, 60, 60, 60, TFT_WHITE); M5.Lcd.setCursor(180, 80); M5.Lcd.print("3");
    M5.Lcd.fillRect(230, 60, 60, 60, TFT_WHITE); M5.Lcd.setCursor(230, 80); M5.Lcd.print("0");


    M5.Lcd.fillRect(20, 130, 60, 60, TFT_WHITE); M5.Lcd.setCursor(40, 150); M5.Lcd.print("4");
    M5.Lcd.fillRect(90, 130, 60, 60, TFT_WHITE); M5.Lcd.setCursor(110, 150); M5.Lcd.print("5");
    M5.Lcd.fillRect(160, 130, 60, 60, TFT_WHITE); M5.Lcd.setCursor(180, 150); M5.Lcd.print("6");

    M5.Lcd.fillRect(20, 200, 60, 60, TFT_WHITE); M5.Lcd.setCursor(40, 220); M5.Lcd.print("7");
    M5.Lcd.fillRect(90, 200, 60, 60, TFT_WHITE); M5.Lcd.setCursor(110, 220); M5.Lcd.print("8");
    M5.Lcd.fillRect(160, 200, 60, 60, TFT_WHITE); M5.Lcd.setCursor(180, 220); M5.Lcd.print("9");

    // Draw Done button
    M5.Lcd.fillRect(250, 200, 50, 40, TFT_GREEN);
    M5.Lcd.setCursor(250, 200);
    M5.Lcd.print("Done");

    // Draw Back button
    M5.Lcd.fillRect(250, 150, 50, 40, TFT_RED);
    M5.Lcd.setCursor(250, 150);
    M5.Lcd.setTextSize(1);
    M5.Lcd.print("Back");

    // Show the current input number on the screen
    M5.Lcd.setCursor(20, 0);
    M5.Lcd.fillRect(20, 0, 280, 40, TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print("Zip Code: ");
    M5.Lcd.print(inputNumber);
}


String convertTo12HourFormat(int hours, int minutes, int seconds) {
    String period = "AM";  // Default to AM

    // Convert hours to 12-hour format
    if (hours >= 12) {
        if (hours > 12) {
            hours -= 12;  // Convert hours greater than 12 to 12-hour format
        }
        period = "PM";  // Change to PM for hours >= 12
    } else if (hours == 0) {
        hours = 12;  // Handle midnight case
    }

    // Format the time as hh:mm:ss am/pm
    String timeString = String(hours);
    timeString += ":";
    if (minutes < 10) timeString += "0";  // Add leading zero for minutes if needed
    timeString += String(minutes);
    timeString += ":";
    if (seconds < 10) timeString += "0";  // Add leading zero for seconds if needed
    timeString += String(seconds);
    timeString += " " + period;

    return timeString;
}


/////////////////////////////////////////////////////////////////
// This method takes in a URL and makes a GET request to the
// URL, returning the response.
/////////////////////////////////////////////////////////////////
String httpGETRequest(const char* serverURL) {

   // Initialize client
   HTTPClient http;
   http.begin(serverURL);


   // Send HTTP GET request and obtain response
   int httpResponseCode = http.GET();
   String response = http.getString();


   // Check if got an error
   if (httpResponseCode > 0)
       Serial.printf("HTTP Response code: %d\n", httpResponseCode);
   else {
       Serial.printf("HTTP Response ERROR code: %d\n", httpResponseCode);
       Serial.printf("Server Response: %s\n", response);
   }


   // Free resources and return response
   http.end();
   return response;
}


/////////////////////////////////////////////////////////////////
// This method takes in an image icon string (from API) and a
// resize multiple and draws the corresponding image (bitmap byte
// arrays found in EGR425_Phase1_weather_bitmap_images.h) to scale (for
// example, if resizeMult==2, will draw the image as 200x200 instead
// of the native 100x100 pixels) on the right-hand side of the
// screen (centered vertically).
/////////////////////////////////////////////////////////////////
void drawWeatherImage(String iconId, int resizeMult) {


   // Get the corresponding byte array
   const uint16_t * weatherBitmap = getWeatherBitmap(iconId);


   // Compute offsets so that the image is centered vertically and is
   // right-aligned
   int yOffset = -(resizeMult * imgSqDim - M5.Lcd.height()) / 2;
   int xOffset = sWidth - (imgSqDim*resizeMult*.8); // Right align (image doesn't take up entire array)
   //int xOffset = (M5.Lcd.width() / 2) - (imgSqDim * resizeMult / 2); // center horizontally

   // Iterate through each pixel of the imgSqDim x imgSqDim (100 x 100) array
   for (int y = 0; y < imgSqDim; y++) {
       for (int x = 0; x < imgSqDim; x++) {
           // Compute the linear index in the array and get pixel value
           int pixNum = (y * imgSqDim) + x;
           uint16_t pixel = weatherBitmap[pixNum];


           // If the pixel is black, do NOT draw (treat it as transparent);
           // otherwise, draw the value
           if (pixel != 0) {
               // 16-bit RBG565 values give the high 5 pixels to red, the middle
               // 6 pixels to green and the low 5 pixels to blue as described
               // here: http://www.barth-dev.de/online/rgb565-color-picker/
               byte red = (pixel >> 11) & 0b0000000000011111;
               red = red << 3;
               byte green = (pixel >> 5) & 0b0000000000111111;
               green = green << 2;
               byte blue = pixel & 0b0000000000011111;
               blue = blue << 3;


               // Scale image; for example, if resizeMult == 2, draw a 2x2
               // filled square for each original pixel
               for (int i = 0; i < resizeMult; i++) {
                   for (int j = 0; j < resizeMult; j++) {
                       int xDraw = x * resizeMult + i + xOffset;
                       int yDraw = y * resizeMult + j + yOffset;
                       M5.Lcd.drawPixel(xDraw, yDraw, M5.Lcd.color565(red, green, blue));
                   }
               }
           }
       }
   }
}
/////////////////////////////////////////////////////////////////
// Update the display based on the weather variables defined
// at the top of the screen.
/////////////////////////////////////////////////////////////////
void drawWeatherDisplayLocal() {

        // Now proceed with full screen refresh
        uint16_t primaryTextColor;
        M5.Lcd.fillScreen(TFT_CYAN);
        primaryTextColor = TFT_WHITE;

        M5.Lcd.setCursor(10, 10);
        M5.Lcd.setTextColor(primaryTextColor);
        M5.Lcd.setTextSize(3);
        M5.Lcd.printf("%s", " Local Sensor\n Readings");

        M5.Lcd.setCursor(10, 80);
        M5.Lcd.setTextSize(10);
        Serial.printf("Temperature: %.2f °C\n", localTempNow);
        Serial.printf("Humidity: %.2f %%\n", humidity);

        if (isFarenheit) {
            M5.Lcd.printf("%0.fF\n", (localTempNow * 9/5) + 32);
        } else {
            M5.Lcd.printf("%0.fC\n", localTempNow);
        }

        M5.Lcd.setCursor(10, 150);
        M5.Lcd.setTextSize(3);
        M5.Lcd.setTextColor(TFT_RED);
        M5.Lcd.printf("Humidity: %0.f%%\n", humidity);

        int hours = localTimeClient.getHours();
        int minutes = localTimeClient.getMinutes();
        int seconds = localTimeClient.getSeconds();

        // Convert to 12-hour format
        bool isPM = hours >= 12;
        int displayHours = (hours % 12 == 0) ? 12 : hours % 12;
        M5.Lcd.setTextColor(TFT_BLACK);
        // Print formatted time
        M5.Lcd.setCursor(10, 190);
        M5.Lcd.printf("%02d:%02d:%02d %s\n", displayHours, minutes, seconds, isPM ? "PM" : "AM");

}


/////////////////////////////////////////////////////////////////
// This method fetches the weather details from the OpenWeather
// API and saves them into the fields defined above
/////////////////////////////////////////////////////////////////
void fetchLocalWeatherDetails() {
    fetchSHT40Data();
    fetchVCNL4040Data();
 }

 void fetchVCNL4040Data(){
    I2C_RW::setI2CAddress(VCNL_I2C_ADDRESS);
    // I2C call to read sensor proximity data and print
    int prox = I2C_RW::readReg8Addr16Data(VCNL_REG_PROX_DATA, 2, "to read proximity data", false);
    // Serial.printf("Proximity: %d\n", prox);
    proximity = prox;

	// I2C call to read sensor ambient light data and print
    int als = I2C_RW::readReg8Addr16Data(VCNL_REG_ALS_DATA, 2, "to read ambient light data", false);
    als = als * 0.1; // See pg 12 of datasheet - we are using ALS_IT (7:6)=(0,0)=80ms integration time = 0.10 lux/step for a max range of 6553.5 lux
    // Serial.printf("Ambient Light: %d\n", als);
    ambient_light = als;

	// I2C call to read white light data and print
    int rwl = I2C_RW::readReg8Addr16Data(VCNL_REG_WHITE_DATA, 2, "to read white light data", false);
    rwl = rwl * 0.1;
    // Serial.printf("White Light: %d\n\n", rwl);
    white_light = rwl;
 }

 void fetchSHT40Data() {
    I2C_RW::setI2CAddress(SHT40_I2C_ADDRESS);

    // Step 1: Send the measurement command to the SHT40 sensor
    I2C_RW::writeReg8Addr16Data(SHT40_REG_MEASURE_HIGH_REPEATABILITY, 0, "Start Measurement", true);
    delay(500); // Wait for measurement to complete

    // Step 2: Read 6 bytes of data from the sensor
    uint8_t rx_bytes[6];
    Wire.requestFrom(SHT40_I2C_ADDRESS, 6);
    for (int i = 0; i < 6; i++) {
        if (Wire.available()) {
            rx_bytes[i] = Wire.read(); // Read each byte
        }
    }

    // Step 3: Process the received data
    int t_ticks = (rx_bytes[0] << 8) + rx_bytes[1]; // Combine MSB and LSB for temperature
    int rh_ticks = (rx_bytes[3] << 8) + rx_bytes[4]; // Combine MSB and LSB for humidity

    // Calculate temperature in degrees Celsius
    float t_degC = -45 + 175.0 * t_ticks / 65535.0;
    // localTempNow = (t_degC * (9/5)) + 32;
    localTempNow = t_degC;

    // Calculate relative humidity percentage
    float rh_pRH = -6 + 125.0 * rh_ticks / 65535.0;
    humidity = constrain(rh_pRH, 0.0f, 100.0f); // Ensure humidity is within bounds

    // Step 4: Print results to Serial
    // Serial.printf("Temperature: %.2f °C\n", localTempNow);
    // Serial.printf("Humidity: %.2f %%\n", humidity);
}

boolean turnOffScreen(){
    if(proximity >= 400){
        return true;
    }else{
        return false;
    }
}