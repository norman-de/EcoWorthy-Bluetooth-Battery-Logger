#include "OTAManager.h"
#include "config.h"

OTAManager::OTAManager() : updateInProgress(false) {
}

bool OTAManager::begin() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    
    // Initialize ArduinoOTA
    ArduinoOTA.setHostname("eco-worthy-logger");
    ArduinoOTA.setPort(3232);  // Default OTA port
    
    // Set password if configured
    if (strlen(OTA_PASSWORD) > 0) {
        ArduinoOTA.setPassword(OTA_PASSWORD);
    }
    
    // Set up ArduinoOTA callbacks
    ArduinoOTA.onStart([this]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_SPIFFS
            type = "filesystem";
        }
        
        updateInProgress = true;
        if (onStartCallback) {
            onStartCallback();
        }
    });
    
    ArduinoOTA.onEnd([this]() {
        updateInProgress = false;
        if (onEndCallback) {
            onEndCallback();
        }
    });
    
    ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
        if (onProgressCallback) {
            onProgressCallback(progress, total);
        }
    });
    
    ArduinoOTA.onError([this](ota_error_t error) {
        updateInProgress = false;
        String errorMsg = getOTAErrorString(error);
        if (onErrorCallback) {
            onErrorCallback(errorMsg);
        }
    });
    
    // Start ArduinoOTA
    ArduinoOTA.begin();
    
    return true;
}

void OTAManager::loop() {
    // Handle ArduinoOTA
    ArduinoOTA.handle();
}

bool OTAManager::isUpdateInProgress() {
    return updateInProgress;
}

void OTAManager::setOnStart(std::function<void()> callback) {
    onStartCallback = callback;
}

void OTAManager::setOnEnd(std::function<void()> callback) {
    onEndCallback = callback;
}

void OTAManager::setOnProgress(std::function<void(size_t current, size_t total)> callback) {
    onProgressCallback = callback;
}

void OTAManager::setOnError(std::function<void(String error)> callback) {
    onErrorCallback = callback;
}

String OTAManager::getOTAErrorString(ota_error_t error) {
    String errorMsg = "ArduinoOTA Error[" + String(error) + "]: ";
    switch (error) {
        case OTA_AUTH_ERROR:
            errorMsg += "Authentication Failed";
            break;
        case OTA_BEGIN_ERROR:
            errorMsg += "Begin Failed";
            break;
        case OTA_CONNECT_ERROR:
            errorMsg += "Connect Failed";
            break;
        case OTA_RECEIVE_ERROR:
            errorMsg += "Receive Failed";
            break;
        case OTA_END_ERROR:
            errorMsg += "End Failed";
            break;
        default:
            errorMsg += "Unknown Error";
            break;
    }
    return errorMsg;
}
