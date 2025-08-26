#ifndef WEBSERVER_MANAGER_H
#define WEBSERVER_MANAGER_H

#include <Arduino.h>
#include <WebServer.h>
#include "config.h"
#include "BatteryProtocol.h"

class WebServerManager {
public:
    WebServerManager();
    ~WebServerManager();
    
    // Initialization
    void begin();
    
    // Main loop handling
    void handleClient();
    
    // Data management
    void updateBatteryData(int batteryIndex, const BatteryData& batteryData);
    void setBatteryDataUpdateTime(int batteryIndex, unsigned long updateTime);
    
    // Status
    bool isRunning() const;

private:
    WebServer* webServer;
    bool serverRunning;
    
    // Battery data storage for web display
    BatteryData latestBatteryData[BATTERY_COUNT];
    unsigned long lastDataUpdate[BATTERY_COUNT];
    
    // HTTP handlers
    void handleRoot();
    void handleApiData();
    
    // Helper methods
    void initializeBatteryData();
};

#endif // WEBSERVER_MANAGER_H
