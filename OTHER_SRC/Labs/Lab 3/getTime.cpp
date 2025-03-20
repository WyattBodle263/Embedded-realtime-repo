#include <M5Core2.h>
#include <NTPClient.h>
#include <WiFi.h>


// WiFiUDP ntpUDP;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -7 * 3600); // UTC-7 for PST
// WiFi Credentials
const char* ssid = "CBU-LANCERS";
const char* password = "L@ncerN@tion";


// Function to convert epoch time to 12-hour format and display it
void displayTime(int epochTime) {
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

  // Display on M5Stack LCD
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(BLUE);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("Time: %02d:%02d %s", hour, minute, ampm);

  // Debugging in Serial Monitor
  Serial.printf("Time: %02d:%02d %s\n", hour, minute, ampm);
}

void setup() {
  M5.begin();
  Serial.begin(115200);
   // Connect to WiFi
//    M5.Lcd.println("Connecting to WiFi...");
   WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED) {
       delay(500);
    //    M5.Lcd.print(".");
   }
//    M5.Lcd.println("\nConnected!");

  timeClient.begin();


  // Example epoch time (replace with your epoch time value)
  timeClient.update();
  int epochTime = timeClient.getEpochTime();
; // For example, "2023-03-26 03:30:00 AM"

  // Call function to display the time
  displayTime(epochTime);
}

void loop() {
  // Your main loop logic here
}
