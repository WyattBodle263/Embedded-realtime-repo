#include <M5Core2.h>
#include <Wire.h>

///////////////////////////////////////////////////////////////
// Variables
///////////////////////////////////////////////////////////////
const int I2C_SDA_PIN = 32; // 21 for internal; 32 for port A
const int I2C_SCL_PIN = 33; // 22 for internal; 33 for port A
const int I2C_FREQ = 400000;
const int VCNL4040_I2C_ADDRESS = 0x60;

///////////////////////////////////////////////////////////////
// Put your setup code here, to run once
///////////////////////////////////////////////////////////////
void setup() {
    // Init device
    M5.begin();

    // Initialize I2C port
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);

    // I2C Variables
    byte regAddr = 0x0C; // 8-bit register addresses
    int numBytesToRead = 2; // 16-bit data

    // Enable I2C connection
    Wire.beginTransmission(VCNL4040_I2C_ADDRESS);

    // Prepare and write address (command code)
    Wire.write(regAddr);

    // End transmission (not writing...reading)
    Wire.endTransmission(false);

    // Request to read the data from the device/register addressed above
    Wire.requestFrom(VCNL4040_I2C_ADDRESS, numBytesToRead);

    // Grab the data from the data line
    if (Wire.available() == numBytesToRead) {
        uint16_t data = 0;
        uint8_t lsb = Wire.read();
        uint8_t msb = Wire.read();
        data = (uint16_t)msb << 8 | lsb;
        Serial.printf("STATUS: I2C read at address 0x%02X = 0x%04X (expected device ID of 0x0186)\n", regAddr, data);
    }

}

///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop() {
}