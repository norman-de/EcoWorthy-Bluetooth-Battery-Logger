#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "BatteryProtocol.h"

class MqttClient {
public:
    MqttClient();
    
    bool begin(const char* server, int port, const char* user, const char* password, const char* clientId);
    void loop();
    bool isConnected();
    void reconnect();
    
    bool publishBatteryData(const BatteryData& data);
    bool publishStatus(const String& message);
    
    
private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    
    String server;
    int port;
    String user;
    String password;
    String clientId;
    String topicPrefix;
    
    unsigned long lastReconnectAttempt;
    static const unsigned long RECONNECT_INTERVAL = 5000; // 5 seconds
    
    
    String createBatteryTopic(const String& macAddress, const String& subtopic);
    String createStatusTopic();
};

#endif
