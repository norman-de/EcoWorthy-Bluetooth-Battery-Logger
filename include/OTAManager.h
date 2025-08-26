#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <WiFi.h>
#include <ArduinoOTA.h>

class OTAManager {
public:
    OTAManager();
    
    bool begin();
    void loop();
    bool isUpdateInProgress();
    
    // Callback functions
    void setOnStart(std::function<void()> callback);
    void setOnEnd(std::function<void()> callback);
    void setOnProgress(std::function<void(size_t current, size_t total)> callback);
    void setOnError(std::function<void(String error)> callback);
    
private:
    bool updateInProgress;
    
    std::function<void()> onStartCallback;
    std::function<void()> onEndCallback;
    std::function<void(size_t, size_t)> onProgressCallback;
    std::function<void(String)> onErrorCallback;
    
    String getOTAErrorString(ota_error_t error);
};

#endif
