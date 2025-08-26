#include "BluetoothManager.h"

// Static instance pointer for callbacks
BluetoothManager* BluetoothManager::instance = nullptr;

BluetoothManager::BluetoothManager() 
    : pClient(nullptr)
    , pWriteCharacteristic(nullptr)
    , pReadCharacteristic(nullptr)
    , bleConnected(false)
    , currentBatteryMac("")
    , responseLength(0)
    , responseReceived(false)
    , expectingMoreData(false)
    , expectedTotalLength(0)
    , clientCallback(nullptr)
{
    instance = this;
    // Initialize response buffer
    memset(responseBuffer, 0, sizeof(responseBuffer));
}

BluetoothManager::~BluetoothManager() {
    // Safely disconnect and cleanup
    try {
        if (pClient && bleConnected) {
            pClient->disconnect();
            delay(100); // Give time for disconnect
        }
        
        if (clientCallback) {
            delete clientCallback;
            clientCallback = nullptr;
        }
        
        // Clear characteristics
        pWriteCharacteristic = nullptr;
        pReadCharacteristic = nullptr;
        pClient = nullptr;
        
    } catch (...) {
        // Ignore exceptions during cleanup
    }
    
    instance = nullptr;
}

void BluetoothManager::begin() {
    try {
        BLEDevice::init("ECO-WORTHY-Logger");
        
        pClient = BLEDevice::createClient();
        if (pClient == nullptr) {
            Serial.println("Failed to create BLE client");
            return;
        }
        
        clientCallback = new MyClientCallback(this);
        pClient->setClientCallbacks(clientCallback);
        
        Serial.println("BluetoothManager initialized successfully");
    } catch (const std::exception& e) {
        Serial.print("BluetoothManager init failed: ");
        Serial.println(e.what());
    } catch (...) {
        Serial.println("BluetoothManager init failed with unknown error");
    }
}

bool BluetoothManager::connectToBattery(const String& macAddress) {
    const unsigned long CONNECT_TIMEOUT_MS = 10000; // 10 seconds timeout
    const unsigned long SERVICE_TIMEOUT_MS = 5000;  // 5 seconds for service discovery
    
    try {
        // Safety check
        if (pClient == nullptr) {
            Serial.println("[BLE] BLE client not initialized");
            return false;
        }
        
        // Disconnect if already connected
        if (bleConnected) {
            Serial.println("[BLE] Disconnecting from previous connection");
            pClient->disconnect();
            delay(500);
            bleConnected = false;
        }
        
        // Clear previous characteristics
        pWriteCharacteristic = nullptr;
        pReadCharacteristic = nullptr;
        
        BLEAddress bleAddress(macAddress.c_str());
        Serial.println("[BLE] Attempting to connect to: " + macAddress);
        
        // Connect with explicit timeout protection
        unsigned long connectStartTime = millis();
        bool connectSuccess = false;
        
        // Non-blocking connect attempt with timeout
        while ((millis() - connectStartTime) < CONNECT_TIMEOUT_MS) {
            if (pClient->connect(bleAddress)) {
                connectSuccess = true;
                break;
            }
            
            // Check every 500ms and allow other tasks to run
            delay(500);
            yield();
            
            // Early exit if connection is established
            if (pClient->isConnected()) {
                connectSuccess = true;
                break;
            }
        }
        
        if (!connectSuccess) {
            Serial.println("[BLE] Connection timeout after " + String(millis() - connectStartTime) + "ms");
            return false;
        }
        
        Serial.println("[BLE] Connected successfully, discovering services...");
        
        // Get the service with timeout protection
        unsigned long serviceStartTime = millis();
        BLERemoteService* pRemoteService = nullptr;
        
        while ((millis() - serviceStartTime) < SERVICE_TIMEOUT_MS) {
            pRemoteService = pClient->getService(SERVICE_UUID);
            if (pRemoteService != nullptr) {
                break;
            }
            
            delay(200);
            yield();
            
            // Check if connection is still valid
            if (!pClient->isConnected()) {
                Serial.println("[BLE] Connection lost during service discovery");
                return false;
            }
        }
        
        if (pRemoteService == nullptr) {
            Serial.println("[BLE] Service discovery timeout or service not found");
            pClient->disconnect();
            return false;
        }
        
        // Get the characteristics with safety checks and timeout
        unsigned long charStartTime = millis();
        while ((millis() - charStartTime) < 3000) { // 3 second timeout for characteristics
            pWriteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_WRITE_UUID);
            pReadCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_READ_UUID);
            
            if (pWriteCharacteristic != nullptr && pReadCharacteristic != nullptr) {
                break;
            }
            
            delay(100);
            yield();
        }
        
        if (pWriteCharacteristic == nullptr || pReadCharacteristic == nullptr) {
            Serial.println("[BLE] Required characteristics not found");
            pClient->disconnect();
            return false;
        }
        
        // Register for notifications with safety checks and timeout
        if (pReadCharacteristic->canNotify()) {
            Serial.println("[BLE] Setting up notifications...");
            
            try {
                pReadCharacteristic->registerForNotify(notifyCallback);
                
                // Enable notifications by writing to CCCD with timeout protection
                uint8_t notificationOn[] = {0x01, 0x00};
                BLERemoteDescriptor* pCCCD = pReadCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902));
                if (pCCCD != nullptr) {
                    pCCCD->writeValue(notificationOn, 2, true);
                }
                
                // Give time for notification setup with watchdog feeding
                unsigned long notifyStartTime = millis();
                while (millis() - notifyStartTime < 500) {
                    delay(50);
                    yield();
                }
                
            } catch (...) {
                Serial.println("[BLE] Failed to setup notifications, continuing anyway");
            }
        }
        
        currentBatteryMac = macAddress;
        Serial.println("[BLE] Successfully connected and configured");
        return true;
        
    } catch (const std::exception& e) {
        Serial.print("[BLE] Connect error: ");
        Serial.println(e.what());
        if (pClient && pClient->isConnected()) {
            pClient->disconnect();
        }
        return false;
    } catch (...) {
        Serial.println("[BLE] Connect failed with unknown error");
        if (pClient && pClient->isConnected()) {
            pClient->disconnect();
        }
        return false;
    }
}

void BluetoothManager::disconnect() {
    try {
        if (bleConnected && pClient && pClient->isConnected()) {
            pClient->disconnect();
            delay(300);
        }
        bleConnected = false;
        pWriteCharacteristic = nullptr;
        pReadCharacteristic = nullptr;
    } catch (...) {
        // Ignore exceptions during disconnect
        bleConnected = false;
    }
}

bool BluetoothManager::isConnected() const {
    return bleConnected;
}

bool BluetoothManager::readBatteryData(const String& macAddress, BatteryData& batteryData) {
    if (!connectToBattery(macAddress)) {
        return false;
    }
    
    batteryData.macAddress = macAddress;
    batteryData.dataValid = false;
    
    bool foundWorkingCommand = false;
    
    try {
        // Try to get basic battery info first
        if (tryCommand(macAddress, CMD_READ_BASIC_INFO, "basic_info", batteryData)) {
            foundWorkingCommand = true;
            
            // If basic info successful, also try to get cell voltages
            tryCommand(macAddress, CMD_READ_CELL_VOLTAGES, "cell_voltages", batteryData);
        }
    } catch (...) {
        Serial.println("Error during battery data reading");
        foundWorkingCommand = false;
    }
    
    // Always disconnect properly
    disconnect();
    
    return foundWorkingCommand && batteryData.dataValid;
}

void BluetoothManager::setOnConnect(std::function<void()> callback) {
    onConnectCallback = callback;
}

void BluetoothManager::setOnDisconnect(std::function<void()> callback) {
    onDisconnectCallback = callback;
}

bool BluetoothManager::sendCommandAndWaitResponse(uint8_t* command, uint8_t commandLength, unsigned long timeoutMs) {
    // Safety checks
    if (command == nullptr || commandLength == 0 || commandLength > 20) {
        Serial.println("[BLE] Invalid command parameters");
        return false;
    }
    
    if (pWriteCharacteristic == nullptr || !bleConnected) {
        Serial.println("[BLE] Not connected or characteristic not available");
        return false;
    }
    
    // Reset response state
    responseReceived = false;
    responseLength = 0;
    expectingMoreData = false;
    expectedTotalLength = 0;
    memset(responseBuffer, 0, sizeof(responseBuffer));
    
    try {
        // Send command with safety check
        pWriteCharacteristic->writeValue(command, commandLength, false);
        protocol.printHex(command, commandLength);
        
        // Wait for response with robust timeout handling
        unsigned long startTime = millis();
        unsigned long lastWatchdogFeed = millis();
        int retryCount = 0;
        const int maxRetries = 3;
        
        while (!responseReceived && (millis() - startTime) < timeoutMs) {
            // Feed watchdog every 500ms to prevent system reset
            if (millis() - lastWatchdogFeed >= 500) {
                yield(); // Allow other tasks to run
                lastWatchdogFeed = millis();
            }
            
            // Check if BLE connection is still valid
            if (!pClient || !pClient->isConnected()) {
                Serial.println("[BLE] Connection lost during command wait");
                return false;
            }
            
            // Progressive delay with timeout protection
            delay(25); // Shorter delay for better responsiveness
            
            // Retry mechanism for stuck notifications
            if ((millis() - startTime) > (timeoutMs / 2) && retryCount < maxRetries) {
                Serial.println("[BLE] Timeout halfway reached, checking notification state");
                retryCount++;
                
                // Force a small delay to allow late notifications
                delay(100);
                
                if (responseReceived) {
                    break;
                }
            }
        }
        
        if (!responseReceived) {
            Serial.println("[BLE] Command timeout after " + String(millis() - startTime) + "ms");
        }
        
        return responseReceived;
        
    } catch (const std::exception& e) {
        Serial.print("[BLE] Send command error: ");
        Serial.println(e.what());
        return false;
    } catch (...) {
        Serial.println("[BLE] Send command failed with unknown error");
        return false;
    }
}

bool BluetoothManager::tryCommand(const String& macAddress, uint8_t cmd, const String& cmdName, BatteryData& batteryData) {
    uint8_t command[10];
    uint8_t commandLength;
    
    // Create command
    if (cmd == CMD_READ_BASIC_INFO) {
        protocol.createBasicInfoCommand(command, commandLength);
    } else if (cmd == CMD_READ_CELL_VOLTAGES) {
        protocol.createCellVoltageCommand(command, commandLength);
    } else if (cmd == CMD_READ_HARDWARE_VERSION) {
        protocol.createHardwareVersionCommand(command, commandLength);
    } else {
        return false;
    }
    
    if (!sendCommandAndWaitResponse(command, commandLength, 5000)) {
        return false;
    }
    
    // Check if this is an error response
    if (responseLength < 3 || responseBuffer[2] != 0x00) {
        return false;
    }
    
    // Try to parse the response
    bool parseSuccess = false;
    if (cmd == CMD_READ_BASIC_INFO) {
        parseSuccess = protocol.parseBasicInfoResponse(responseBuffer, responseLength, batteryData);
    } else if (cmd == CMD_READ_CELL_VOLTAGES) {
        parseSuccess = protocol.parseCellVoltageResponse(responseBuffer, responseLength, batteryData);
    }
    
    return parseSuccess;
}

// Static callback function for BLE notifications
void BluetoothManager::notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (instance) {
        instance->handleNotification(pData, length);
    }
}

void BluetoothManager::handleNotification(uint8_t* pData, size_t length) {
    // Safety check for null pointer and length
    if (!pData || length == 0 || length > 256) {
        return;
    }
    
    // Prevent buffer overflow
    if (length > 0 && (responseLength + length) <= sizeof(responseBuffer)) {
        // Check if this is the first packet (starts with 0xDD)
        if (length >= 4 && pData[0] == 0xDD && responseLength == 0) {
            // First packet - get expected total length from bytes 2-3
            expectedTotalLength = (pData[2] << 8) | pData[3];
            expectedTotalLength += 7; // Add frame overhead (start + cmd + length + checksum + end)
            
            // Safety check for expected length
            if (expectedTotalLength > sizeof(responseBuffer)) {
                expectedTotalLength = sizeof(responseBuffer);
            }
            
            expectingMoreData = (expectedTotalLength > length);
            
            memcpy(responseBuffer, pData, length);
            responseLength = length;
            
        } else if (expectingMoreData && responseLength > 0) {
            // Continuation packet - append to existing data
            size_t copyLength = min((size_t)(sizeof(responseBuffer) - responseLength), length);
            memcpy(responseBuffer + responseLength, pData, copyLength);
            responseLength += copyLength;
            
        } else {
            // Single packet or unexpected packet
            size_t copyLength = min(sizeof(responseBuffer), length);
            memcpy(responseBuffer, pData, copyLength);
            responseLength = copyLength;
            expectingMoreData = false;
        }
        
        // Check if we have received the complete response
        if (!expectingMoreData || responseLength >= expectedTotalLength || 
            (responseLength > 0 && responseBuffer[responseLength-1] == 0x77)) {
            responseReceived = true;
            expectingMoreData = false;
        }
        
        // Only print hex if protocol is valid
        if (responseLength > 0) {
            protocol.printHex(pData, min(length, (size_t)20)); // Limit debug output
        }
    }
}

void BluetoothManager::handleConnect() {
    bleConnected = true;
    if (onConnectCallback) {
        onConnectCallback();
    }
}

void BluetoothManager::handleDisconnect() {
    bleConnected = false;
    if (onDisconnectCallback) {
        onDisconnectCallback();
    }
}

// BLE client callback implementations
void BluetoothManager::MyClientCallback::onConnect(BLEClient* pclient) {
    if (manager) {
        manager->handleConnect();
    }
}

void BluetoothManager::MyClientCallback::onDisconnect(BLEClient* pclient) {
    if (manager) {
        manager->handleDisconnect();
    }
}
