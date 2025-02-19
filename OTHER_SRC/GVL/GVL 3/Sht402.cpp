#include <M5Core2.h>
#include "I2C_RW.h"

// I2C pins and address
const int PIN_SDA = 32;
const int PIN_SCL = 33;
#define SHT40_I2C_ADDRESS 0x44
#define SHT40_REG_MEASURE_HIGH_REPEATABILITY 0xFD // Command to start measurement

void setup() {
    // Initialize M5 Core2
    M5.begin();
    Serial.begin(115200); // Start serial communication for debugging

    // Initialize I2C connection to SHT40
    I2C_RW::initI2C(SHT40_I2C_ADDRESS, 400000, PIN_SDA, PIN_SCL);
}

void loop() {
    // Step 1: Send the measurement command to the SHT40 sensor
    I2C_RW::writeReg8Addr16Data(SHT40_REG_MEASURE_HIGH_REPEATABILITY, 0, "Start Measurement", true);
    delay(10); // Wait for measurement to complete

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

    // Calculate relative humidity percentage
    float rh_pRH = -6 + 125.0 * rh_ticks / 65535.0;
    rh_pRH = constrain(rh_pRH, 0.0f, 100.0f); // Ensure humidity is within bounds

    // Step 4: Print results to Serial
    Serial.printf("Temperature: %.2f °C\n", t_degC);
    Serial.printf("Humidity: %.2f %%\n", rh_pRH);

    // Delay to avoid reading sensors too frequently
    delay(1000);
}
