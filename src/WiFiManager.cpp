#include "WiFiManager.h"
#include <esp_system.h>

// Configuration variables (can be modified at runtime)
static unsigned long reconnectInterval = RECONNECT_INTERVAL_MS;
static unsigned long checkInterval = WIFI_CHECK_INTERVAL_MS;
static int maxReconnectAttempts = MAX_RECONNECT_ATTEMPTS;
static bool systemRestartEnabled = ENABLE_SYSTEM_RESTART;

WiFiManager::WiFiManager() {
    currentState = WiFiState::DISCONNECTED;
    lastCheckTime = 0;
    lastReconnectAttempt = 0;
    reconnectAttempts = 0;
    isInitialized = false;
    
    // Initialize callbacks to nullptr
    onConnectedCallback = nullptr;
    onDisconnectedCallback = nullptr;
    onReconnectAttemptCallback = nullptr;
    onMaxAttemptsReachedCallback = nullptr;
}

void WiFiManager::begin(const String& ssid, const String& password) {
    this->ssid = ssid;
    this->password = password;
    
    Serial.println("[WiFiManager] Initializing WiFi...");
    Serial.println("[WiFiManager] SSID: " + ssid);
    
    // Set WiFi mode
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false); // We handle reconnection ourselves
    
    // Start initial connection
    currentState = WiFiState::CONNECTING;
    WiFi.begin(ssid.c_str(), password.c_str());
    
    // Wait for initial connection with timeout
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && 
           (millis() - startTime) < INITIAL_CONNECT_TIMEOUT_MS) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        handleStateChange(WiFiState::CONNECTED);
        Serial.println();
        Serial.println("[WiFiManager] Connected successfully!");
        Serial.println("[WiFiManager] IP: " + WiFi.localIP().toString());
        Serial.println("[WiFiManager] Signal: " + String(WiFi.RSSI()) + " dBm");
    } else {
        handleStateChange(WiFiState::DISCONNECTED);
        Serial.println();
        Serial.println("[WiFiManager] Initial connection failed, will retry...");
    }
    
    isInitialized = true;
    lastCheckTime = millis();
}

void WiFiManager::loop() {
    if (!isInitialized) return;
    
    unsigned long currentTime = millis();
    
    // Check connection status periodically
    if (currentTime - lastCheckTime >= checkInterval) {
        checkConnection();
        lastCheckTime = currentTime;
    }
    
    // Handle reconnection attempts
    if (currentState == WiFiState::RECONNECTING || 
        (currentState == WiFiState::DISCONNECTED && reconnectAttempts < maxReconnectAttempts)) {
        
        if (currentTime - lastReconnectAttempt >= reconnectInterval) {
            performReconnect();
        }
    }
    
    // Handle system restart if max attempts reached
    if (currentState == WiFiState::RESTART_PENDING) {
        Serial.println("[WiFiManager] Restarting system in 5 seconds...");
        delay(5000);
        restartSystem();
    }
}

void WiFiManager::checkConnection() {
    wl_status_t status = WiFi.status();
    
    switch (status) {
        case WL_CONNECTED:
            if (currentState != WiFiState::CONNECTED) {
                handleStateChange(WiFiState::CONNECTED);
                resetReconnectCounter();
            }
            break;
            
        case WL_NO_SSID_AVAIL:
        case WL_CONNECT_FAILED:
        case WL_CONNECTION_LOST:
        case WL_DISCONNECTED:
            if (currentState == WiFiState::CONNECTED) {
                Serial.println("[WiFiManager] Connection lost, starting reconnection...");
                handleStateChange(WiFiState::DISCONNECTED);
            }
            break;
            
        default:
            // Handle other states if needed
            break;
    }
}

void WiFiManager::performReconnect() {
    if (reconnectAttempts >= maxReconnectAttempts) {
        Serial.println("[WiFiManager] Maximum reconnect attempts reached!");
        handleStateChange(WiFiState::FAILED);
        
        if (onMaxAttemptsReachedCallback) {
            onMaxAttemptsReachedCallback();
        }
        
        if (systemRestartEnabled) {
            handleStateChange(WiFiState::RESTART_PENDING);
        }
        return;
    }
    
    reconnectAttempts++;
    lastReconnectAttempt = millis();
    
    Serial.println("[WiFiManager] Reconnect attempt " + String(reconnectAttempts) + 
                   "/" + String(maxReconnectAttempts));
    
    if (onReconnectAttemptCallback) {
        onReconnectAttemptCallback(reconnectAttempts);
    }
    
    handleStateChange(WiFiState::RECONNECTING);
    
    // Disconnect and reconnect
    WiFi.disconnect();
    delay(1000);
    WiFi.begin(ssid.c_str(), password.c_str());
}

void WiFiManager::handleStateChange(WiFiState newState) {
    if (currentState == newState) return;
    
    WiFiState oldState = currentState;
    String oldStateStr = getStateString();
    currentState = newState;
    
    Serial.println("[WiFiManager] State change: " + oldStateStr + 
                   " -> " + getStateString());
    
    // Trigger callbacks
    switch (newState) {
        case WiFiState::CONNECTED:
            if (onConnectedCallback) {
                onConnectedCallback();
            }
            break;
            
        case WiFiState::DISCONNECTED:
        case WiFiState::FAILED:
            if (onDisconnectedCallback) {
                onDisconnectedCallback();
            }
            break;
            
        default:
            break;
    }
}

void WiFiManager::restartSystem() {
    Serial.println("[WiFiManager] Performing system restart...");
    Serial.flush();
    esp_restart();
}

// Configuration methods
void WiFiManager::setReconnectInterval(unsigned long intervalMs) {
    reconnectInterval = intervalMs;
    Serial.println("[WiFiManager] Reconnect interval set to " + String(intervalMs) + "ms");
}

void WiFiManager::setCheckInterval(unsigned long intervalMs) {
    checkInterval = intervalMs;
    Serial.println("[WiFiManager] Check interval set to " + String(intervalMs) + "ms");
}

void WiFiManager::setMaxReconnectAttempts(int maxAttempts) {
    maxReconnectAttempts = maxAttempts;
    Serial.println("[WiFiManager] Max reconnect attempts set to " + String(maxAttempts));
}

void WiFiManager::enableSystemRestart(bool enable) {
    systemRestartEnabled = enable;
    Serial.println("[WiFiManager] System restart " + String(enable ? "enabled" : "disabled"));
}

// Status methods
bool WiFiManager::isConnected() const {
    return currentState == WiFiState::CONNECTED && WiFi.status() == WL_CONNECTED;
}

WiFiState WiFiManager::getState() const {
    return currentState;
}

String WiFiManager::getStateString() const {
    switch (currentState) {
        case WiFiState::DISCONNECTED: return "DISCONNECTED";
        case WiFiState::CONNECTING: return "CONNECTING";
        case WiFiState::CONNECTED: return "CONNECTED";
        case WiFiState::RECONNECTING: return "RECONNECTING";
        case WiFiState::FAILED: return "FAILED";
        case WiFiState::RESTART_PENDING: return "RESTART_PENDING";
        default: return "UNKNOWN";
    }
}

int WiFiManager::getReconnectAttempts() const {
    return reconnectAttempts;
}

String WiFiManager::getLocalIP() const {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

int WiFiManager::getSignalStrength() const {
    if (isConnected()) {
        return WiFi.RSSI();
    }
    return 0;
}

// Control methods
void WiFiManager::forceReconnect() {
    Serial.println("[WiFiManager] Forced reconnect requested");
    reconnectAttempts = 0;
    lastReconnectAttempt = 0;
    handleStateChange(WiFiState::DISCONNECTED);
}

void WiFiManager::resetReconnectCounter() {
    if (reconnectAttempts > 0) {
        Serial.println("[WiFiManager] Reconnect counter reset");
        reconnectAttempts = 0;
    }
}

// Callback setters
void WiFiManager::setOnConnected(std::function<void()> callback) {
    onConnectedCallback = callback;
}

void WiFiManager::setOnDisconnected(std::function<void()> callback) {
    onDisconnectedCallback = callback;
}

void WiFiManager::setOnReconnectAttempt(std::function<void(int)> callback) {
    onReconnectAttemptCallback = callback;
}

void WiFiManager::setOnMaxAttemptsReached(std::function<void()> callback) {
    onMaxAttemptsReachedCallback = callback;
}

// Utility methods
void WiFiManager::printStatus() const {
    Serial.println("=== WiFiManager Status ===");
    Serial.println("State: " + getStateString());
    Serial.println("Connected: " + String(isConnected() ? "Yes" : "No"));
    Serial.println("SSID: " + ssid);
    Serial.println("IP: " + getLocalIP());
    Serial.println("Signal: " + String(getSignalStrength()) + " dBm");
    Serial.println("Reconnect attempts: " + String(reconnectAttempts) + "/" + String(maxReconnectAttempts));
    Serial.println("==========================");
}

void WiFiManager::scanNetworks() const {
    Serial.println("[WiFiManager] Scanning for networks...");
    int n = WiFi.scanNetworks();
    
    if (n == 0) {
        Serial.println("[WiFiManager] No networks found");
    } else {
        Serial.println("[WiFiManager] Found " + String(n) + " networks:");
        for (int i = 0; i < n; ++i) {
            Serial.println("  " + String(i + 1) + ": " + WiFi.SSID(i) + 
                          " (" + String(WiFi.RSSI(i)) + " dBm) " +
                          (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "[Open]" : "[Secured]"));
        }
    }
    WiFi.scanDelete();
}
