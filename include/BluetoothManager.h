#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include "config.h"
#include "BatteryProtocol.h"

class BluetoothManager {
public:
    BluetoothManager();
    ~BluetoothManager();
    
    // Initialization
    void begin();
    
    // Connection management
    bool connectToBattery(const String& macAddress);
    void disconnect();
    bool isConnected() const;
    
    // Battery data reading
    bool readBatteryData(const String& macAddress, BatteryData& batteryData);
    
    // Status callbacks
    void setOnConnect(std::function<void()> callback);
    void setOnDisconnect(std::function<void()> callback);

private:
    // BLE objects
    BLEClient* pClient;
    BLERemoteCharacteristic* pWriteCharacteristic;
    BLERemoteCharacteristic* pReadCharacteristic;
    
    // State variables
    bool bleConnected;
    String currentBatteryMac;
    
    // Data buffer for BLE responses
    uint8_t responseBuffer[256];
    int responseLength;
    bool responseReceived;
    bool expectingMoreData;
    int expectedTotalLength;
    
    // Protocol handler
    BatteryProtocol protocol;
    
    // Callbacks
    std::function<void()> onConnectCallback;
    std::function<void()> onDisconnectCallback;
    
    // Private methods
    bool sendCommandAndWaitResponse(uint8_t* command, uint8_t commandLength, unsigned long timeoutMs = 5000);
    bool tryCommand(const String& macAddress, uint8_t cmd, const String& cmdName, BatteryData& batteryData);
    
    // Static callback functions for BLE
    static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    
    // Static instance pointer for callbacks
    static BluetoothManager* instance;
    
    // Internal callback handlers
    void handleNotification(uint8_t* pData, size_t length);
    void handleConnect();
    void handleDisconnect();
    
    // BLE client callback class
    class MyClientCallback : public BLEClientCallbacks {
    public:
        MyClientCallback(BluetoothManager* manager) : manager(manager) {}
        void onConnect(BLEClient* pclient) override;
        void onDisconnect(BLEClient* pclient) override;
    private:
        BluetoothManager* manager;
    };
    
    MyClientCallback* clientCallback;
};

#endif // BLUETOOTH_MANAGER_H
