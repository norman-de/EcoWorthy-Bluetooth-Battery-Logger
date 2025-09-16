#include "BatteryProtocol.h"
#include "config.h"

BatteryProtocol::BatteryProtocol() {
}

void BatteryProtocol::createCommand(uint8_t cmd, uint8_t* buffer, uint8_t& length) {
    buffer[0] = FRAME_START;    // 0xDD
    buffer[1] = FRAME_READ;     // 0xA5
    buffer[2] = cmd;            // Command code
    buffer[3] = 0x00;           // Data length (0 for read commands)
    
    // Calculate checksum exactly as in ewbatlog.py
    // For cmd 0x03: DD A5 03 00 -> sum = 0x185 -> ~0x185 + 1 = 0xFE7B -> but we need 0xFFFD
    // Looking at ewbatlog.py commands:
    // cmd 0x03: dd a5 03 00 ff fd 77 -> checksum is 0xFFFD
    // cmd 0x04: dd a5 04 00 ff fc 77 -> checksum is 0xFFFC
    // This suggests: checksum = 0x10000 - cmd
    uint16_t checksum = 0x10000 - cmd;
    
    buffer[4] = (checksum >> 8) & 0xFF;  // High byte
    buffer[5] = checksum & 0xFF;         // Low byte
    buffer[6] = FRAME_END;               // 0x77
    
    length = 7;
}

void BatteryProtocol::createBasicInfoCommand(uint8_t* buffer, uint8_t& length) {
    createCommand(CMD_READ_BASIC_INFO, buffer, length);
}

void BatteryProtocol::createCellVoltageCommand(uint8_t* buffer, uint8_t& length) {
    createCommand(CMD_READ_CELL_VOLTAGES, buffer, length);
}

void BatteryProtocol::createHardwareVersionCommand(uint8_t* buffer, uint8_t& length) {
    createCommand(CMD_READ_HARDWARE_VERSION, buffer, length);
}

bool BatteryProtocol::parseBasicInfoResponse(const uint8_t* data, uint8_t length, BatteryData& batteryData) {
    printHex(data, length);
    
    // Check minimum length for error response
    if (length < 7) {
        return false;
    }
    
    // ECO-WORTHY BMS response format (from ewbatlog.py):
    // DD 03 [length_high] [length_low] [data...] [checksum_high] [checksum_low] 77
    
    // Check frame start and end
    if (data[0] != FRAME_START) {
        return false;
    }
    
    if (data[length-1] != FRAME_END) {
        return false;
    }
    
    // Check command echo
    if (data[1] != CMD_READ_BASIC_INFO) {
        return false;
    }
    
    // Get data length from bytes 2-3 (big endian)
    uint16_t dataLength = (data[2] << 8) | data[3];
    
    // Check if we have enough data
    if (length < (dataLength + 7)) {
        return false;
    }
    
    // Parse data starting from byte 4 (based on ewbatlog.py decodeParams1)
    uint8_t offset = 4;
    
    // Total voltage (2 bytes, unit: 10mV) - bytes 4-5
    uint16_t voltage_raw = (data[offset] << 8) | data[offset+1];
    batteryData.voltage = voltage_raw / 100.0;
    offset += 2;
    
    // Current (2 bytes, unit: 10mA, signed) - bytes 6-7
    int16_t current_raw = (data[offset] << 8) | data[offset+1];
    if (current_raw > 0x7fff) {
        current_raw = current_raw - 0x10000;
    }
    batteryData.current = current_raw / 100.0;
    offset += 2;
    
    // Remaining capacity (2 bytes, unit: 10mAh) - bytes 8-9
    uint16_t remaining_raw = (data[offset] << 8) | data[offset+1];
    batteryData.remainingAh = remaining_raw / 100.0;
    offset += 2;
    
    // Nominal capacity (2 bytes, unit: 10mAh) - bytes 10-11
    uint16_t max_raw = (data[offset] << 8) | data[offset+1];
    batteryData.maxAh = max_raw / 100.0;
    offset += 2;
    
    // Calculate watts and SOC
    batteryData.watts = batteryData.voltage * batteryData.current;
    if (batteryData.maxAh > 0) {
        batteryData.soc = 100.0 * (batteryData.remainingAh / batteryData.maxAh);
    } else {
        batteryData.soc = 0.0;
    }
    
    // Skip to switches at byte 24 (offset 20 from start of data)
    offset = 24;
    if (offset < length - 3) {
        // FET control status (1 byte) - byte 24
        uint8_t switches = data[offset];
        batteryData.switches = "";
        if (switches & 0x01) {
            batteryData.switches += "C+";
        } else {
            batteryData.switches += "C-";
        }
        if (switches & 0x02) {
            batteryData.switches += "D+";
        } else {
            batteryData.switches += "D-";
        }
    }
    
    // Temperature at bytes 27-28 (offset 23-24 from start of data)
    offset = 27;
    if (offset + 1 < length - 3) {
        uint16_t temp_raw = (data[offset] << 8) | data[offset+1];
        batteryData.temperature = (temp_raw - 2731) * 0.1;
    } else {
        batteryData.temperature = 0.0;
    }
    
    batteryData.dataValid = true;
    batteryData.timestamp = millis();
    
    return true;
}

bool BatteryProtocol::parseCellVoltageResponse(const uint8_t* data, uint8_t length, BatteryData& batteryData) {
    if (length < 7) return false;
    
    // Check frame structure
    if (data[0] != FRAME_START || data[length-1] != FRAME_END) {
        return false;
    }
    
    // Check command code
    if (data[1] != CMD_READ_CELL_VOLTAGES) {
        return false;
    }
    
    // ECO-WORTHY format: DD 04 [length_high] [length_low] [data...] [checksum] 77
    // Get data length from bytes 2-3 (big endian)
    uint16_t dataLength = (data[2] << 8) | data[3];
    
    if (length < (dataLength + 7)) {
        return false;
    }
    
    // Calculate number of cells from ewbatlog.py: n_cells = int(data[3] / 2)
    // But data[3] is low byte of length, so we use dataLength / 2
    batteryData.numCells = dataLength / 2;
    
    // Parse cell voltages starting from byte 4
    uint8_t offset = 4;
    for (int i = 0; i < batteryData.numCells && i < 32; i++) {
        uint16_t cellVoltage = (data[offset] << 8) | data[offset+1];
        batteryData.cellVoltages[i] = cellVoltage / 1000.0; // Convert mV to V
        offset += 2;
    }
    
    batteryData.dataValid = true;
    batteryData.timestamp = millis();
    
    return true;
}

uint16_t BatteryProtocol::calculateChecksum(const uint8_t* data, uint8_t length) {
    uint16_t sum = 0;
    // ECO-WORTHY checksum includes all bytes except checksum and end byte
    for (int i = 0; i < length - 3; i++) {
        sum += data[i];
    }
    return (~sum) + 1; // Invert and add 1
}

bool BatteryProtocol::verifyChecksum(const uint8_t* data, uint8_t length) {
    if (length < 7) return false;
    
    uint16_t calculatedChecksum = calculateChecksum(data, length);
    uint16_t receivedChecksum = (data[length-3] << 8) | data[length-2];
    
    return calculatedChecksum == receivedChecksum;
}

void BatteryProtocol::printHex(const uint8_t* data, uint8_t length) {
    for (int i = 0; i < length; i++) {
    }
}
