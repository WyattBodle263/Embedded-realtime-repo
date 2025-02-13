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
// Register Defines
///////////////////////////////////////////////////////////////
#define VCNL_REG_CHIP_ID 0x0C
#define VCNL_REG_ALS_CONF 0x00

// Method forward declarations
static uint16_t i2cRead8Addr16Data(int regAddr);
static void i2cWrite8Addr16Data(int regAddr, uint16_t data);

///////////////////////////////////////////////////////////////
// Put your setup code here, to run once
///////////////////////////////////////////////////////////////
void setup() {
    // Init device
    M5.begin();

    // Initialize I2C port
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);

    // I2C Variables
    //byte regAddr = 0x0C; // 8-bit register addresses

    // Test that the chip ID is 0x0186
    i2cRead8Addr16Data(VCNL_REG_CHIP_ID);

    // Test that the write function works
    i2cRead8Addr16Data(VCNL_REG_ALS_CONF);
    i2cWrite8Addr16Data(VCNL_REG_ALS_CONF, 0x1234);
    i2cRead8Addr16Data(VCNL_REG_ALS_CONF);
}

///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop() { }

///////////////////////////////////////////////////////////////
// Given an 8-bit address, writes the provided 16-bit data into
// the register at that address.
///////////////////////////////////////////////////////////////
static void i2cWrite8Addr16Data(int regAddr, uint16_t data) {
    // Initiate I2C connection
    Wire.beginTransmission(VCNL4040_I2C_ADDRESS); // Enable I2C connection
    int bytesWritten = Wire.write(regAddr);
    bytesWritten += Wire.write(data & 0xFF); // Write LSB
    bytesWritten += Wire.write(data >> 8); // Write MSB
    Wire.endTransmission(); // (to send all data)
}

///////////////////////////////////////////////////////////////
// Given an 8-bit address, reads the registers at that address
// and returns the 16-bit data.
///////////////////////////////////////////////////////////////
static uint16_t i2cRead8Addr16Data(int regAddr) {
    int numBytesToRead = 2; // 16-bit data

    // Initiate I2C connection
    Wire.beginTransmission(VCNL4040_I2C_ADDRESS); // Enable I2C connection
    Wire.write(regAddr); // Prepare and write address (command code)
    Wire.endTransmission(false); // End transmission (not writing...reading)
    Wire.requestFrom(VCNL4040_I2C_ADDRESS, numBytesToRead); // Request to read the data from the device/register addressed above

    // Grab the data from the data line
    if (Wire.available() == numBytesToRead) {
        uint16_t data = 0;
        uint8_t lsb = Wire.read();
        uint8_t msb = Wire.read();
        data = (uint16_t)msb << 8 | lsb;
        Serial.printf("STATUS: I2C read at address 0x%02X = 0x%04X\n", regAddr, data);
        return data;
    }
}