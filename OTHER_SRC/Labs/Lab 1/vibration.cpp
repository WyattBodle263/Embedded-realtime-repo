#include <M5Unified.h>
#include <Adafruit_VCNL4040.h>

#define NOTE_D3 350

///////////////////////////////////////////////////////////////
// Variables
///////////////////////////////////////////////////////////////
Adafruit_VCNL4040 vcnl4040 = Adafruit_VCNL4040();

///////////////////////////////////////////////////////////////
// Setup function
///////////////////////////////////////////////////////////////
void setup() {
    // Init device
    M5.begin();

    // Play a beep sound

    // Initialize the sensor
    Serial.println("Adafruit VCNL4040 Config demo");
    if (!vcnl4040.begin()) {
        Serial.println("Couldn't find VCNL4040 chip");
        while (1);  // Halt if sensor not found
    }
    Serial.println("Found VCNL4040 chip\n");
    M5.Speaker.begin();

}

///////////////////////////////////////////////////////////////
// Loop function
///////////////////////////////////////////////////////////////
void loop() {
    M5.Speaker.tone(vcnl4040.getProximity() * 10, 800);  // Set the speaker to ring at 661Hz for
    M5.Power.setVibration(vcnl4040.getProximity() * 10);

    // Get sensor readings
    Serial.printf("Proximity: %d\n", vcnl4040.getProximity());
    Serial.printf("Ambient light: %d\n", vcnl4040.getLux());
    Serial.printf("Raw white light: %d\n\n", vcnl4040.getWhiteLight());
}
