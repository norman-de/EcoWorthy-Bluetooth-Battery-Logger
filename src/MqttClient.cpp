#include "MqttClient.h"
#include "config.h"

MqttClient::MqttClient() : mqttClient(wifiClient), lastReconnectAttempt(0) {
    topicPrefix = MQTT_TOPIC_PREFIX;
}

bool MqttClient::begin(const char* server, int port, const char* user, const char* password, const char* clientId) {
    this->server = server;
    this->port = port;
    this->user = user;
    this->password = password;
    this->clientId = clientId;
    
    mqttClient.setServer(server, port);
    mqttClient.setBufferSize(1024); // Increase buffer size for JSON messages
    
    return true;
}

void MqttClient::loop() {
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
            lastReconnectAttempt = now;
            reconnect();
        }
    } else {
        mqttClient.loop();
    }
}

bool MqttClient::isConnected() {
    return mqttClient.connected();
}

void MqttClient::reconnect() {
    
    if (mqttClient.connect(clientId.c_str(), user.c_str(), password.c_str())) {
        
        // Publish connection status
        publishStatus("Connected");
        
    } else {
    }
}

bool MqttClient::publishBatteryData(const BatteryData& data) {
    if (!mqttClient.connected()) {
        return false;
    }
    
    // Create JSON document
    DynamicJsonDocument doc(1024);
    
    doc["timestamp"] = data.timestamp;
    doc["macAddress"] = data.macAddress;
    doc["voltage"] = data.voltage;
    doc["current"] = data.current;
    doc["remainingAh"] = data.remainingAh;
    doc["maxAh"] = data.maxAh;
    doc["watts"] = data.watts;
    doc["soc"] = data.soc;
    doc["temperature"] = data.temperature;
    doc["switches"] = data.switches;
    doc["numCells"] = data.numCells;
    doc["dataValid"] = data.dataValid;
    
    // Add cell voltages array
    JsonArray cellVoltages = doc.createNestedArray("cellVoltages");
    for (int i = 0; i < data.numCells && i < 32; i++) {
        cellVoltages.add(data.cellVoltages[i]);
    }
    
    // Serialize JSON to string
    String jsonString;
    serializeJson(doc, jsonString);
    
    // Create topic
    String topic = createBatteryTopic(data.macAddress, "data");
    
    // Publish data
    bool result = mqttClient.publish(topic.c_str(), jsonString.c_str(), true); // retained message
    
    return result;
}

bool MqttClient::publishStatus(const String& message) {
    if (!mqttClient.connected()) {
        return false;
    }
    
    String topic = createStatusTopic();
    bool result = mqttClient.publish(topic.c_str(), message.c_str(), true); // retained message
    
    if (result) {
    } else {
    }
    
    return result;
}

String MqttClient::createBatteryTopic(const String& macAddress, const String& subtopic) {
    String cleanMac = macAddress;
    cleanMac.replace(":", "");
    cleanMac.toLowerCase();
    
    return topicPrefix + "/battery/" + cleanMac + "/" + subtopic;
}

String MqttClient::createStatusTopic() {
    return topicPrefix + "/logger/status";
}

