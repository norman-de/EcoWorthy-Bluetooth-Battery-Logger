#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <functional>

// WiFi Manager Configuration
#define MAX_RECONNECT_ATTEMPTS 10
#define RECONNECT_INTERVAL_MS 5000      // 5 seconds between reconnect attempts
#define WIFI_CHECK_INTERVAL_MS 10000    // Check WiFi status every 10 seconds
#define INITIAL_CONNECT_TIMEOUT_MS 30000 // 30 seconds for initial connection
#define ENABLE_SYSTEM_RESTART true      // Enable system restart after max attempts

enum class WiFiState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    FAILED,
    RESTART_PENDING
};

class WiFiManager {
private:
    // Configuration
    String ssid;
    String password;
    
    // State tracking
    WiFiState currentState;
    unsigned long lastCheckTime;
    unsigned long lastReconnectAttempt;
    int reconnectAttempts;
    bool isInitialized;
    
    // Callback functions
    std::function<void()> onConnectedCallback;
    std::function<void()> onDisconnectedCallback;
    std::function<void(int)> onReconnectAttemptCallback;
    std::function<void()> onMaxAttemptsReachedCallback;
    
    // Internal methods
    void performReconnect();
    void checkConnection();
    void handleStateChange(WiFiState newState);
    void restartSystem();
    
public:
    WiFiManager();
    
    // Initialization and configuration
    void begin(const String& ssid, const String& password);
    void setReconnectInterval(unsigned long intervalMs);
    void setCheckInterval(unsigned long intervalMs);
    void setMaxReconnectAttempts(int maxAttempts);
    void enableSystemRestart(bool enable);
    
    // Main loop function
    void loop();
    
    // Status methods
    bool isConnected() const;
    WiFiState getState() const;
    String getStateString() const;
    int getReconnectAttempts() const;
    String getLocalIP() const;
    int getSignalStrength() const;
    
    // Control methods
    void forceReconnect();
    void resetReconnectCounter();
    
    // Callback setters
    void setOnConnected(std::function<void()> callback);
    void setOnDisconnected(std::function<void()> callback);
    void setOnReconnectAttempt(std::function<void(int)> callback);
    void setOnMaxAttemptsReached(std::function<void()> callback);
    
    // Utility methods
    void printStatus() const;
    void scanNetworks() const;
};

#endif
