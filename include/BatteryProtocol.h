#ifndef BATTERY_PROTOCOL_H
#define BATTERY_PROTOCOL_H

#include <Arduino.h>

struct BatteryData {
    String macAddress;
    float voltage;          // V
    float current;          // A
    float remainingAh;      // Ah
    float maxAh;            // Ah
    float watts;            // W
    float soc;              // %
    float temperature;      // °C
    String switches;        // Charge/Discharge status
    uint8_t numCells;
    float cellVoltages[32]; // mV
    bool dataValid;
    unsigned long timestamp;
};

class BatteryProtocol {
public:
    BatteryProtocol();
    
    // Create command frames
    void createCommand(uint8_t cmd, uint8_t* buffer, uint8_t& length);
    void createBasicInfoCommand(uint8_t* buffer, uint8_t& length);
    void createCellVoltageCommand(uint8_t* buffer, uint8_t& length);
    void createHardwareVersionCommand(uint8_t* buffer, uint8_t& length);
    
    // Parse response data
    bool parseBasicInfoResponse(const uint8_t* data, uint8_t length, BatteryData& batteryData);
    bool parseCellVoltageResponse(const uint8_t* data, uint8_t length, BatteryData& batteryData);
    
    // Utility functions
    uint16_t calculateChecksum(const uint8_t* data, uint8_t length);
    bool verifyChecksum(const uint8_t* data, uint8_t length);
    void printHex(const uint8_t* data, uint8_t length);
};

#endif
