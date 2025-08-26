#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "BatteryProtocol.h"
#include "MqttClient.h"
#include "OTAManager.h"
#include "BluetoothManager.h"
#include "WebServerManager.h"
#include "WiFiManager.h"


// Global objects
WiFiManager wifiManager;
MqttClient mqttClient;
OTAManager otaManager;
BluetoothManager bluetoothManager;
WebServerManager webServerManager;

// M5Stack Stamp S3 pin definitions
#define LED_PIN 21        // RGB LED pin (WS2812B)
#define BUTTON_PIN 0      // Button pin
#define NUM_LEDS 1        // Number of LEDs

// FastLED setup
CRGB leds[NUM_LEDS];

// LED colors
CRGB COLOR_RED = CRGB::Red;
CRGB COLOR_GREEN = CRGB::Green;
CRGB COLOR_BLUE = CRGB::Blue;
CRGB COLOR_YELLOW = CRGB::Yellow;
CRGB COLOR_OFF = CRGB::Black;

// State variables
unsigned long lastScanTime = 0;
unsigned long lastWatchdogFeed = 0;

// Button state
bool lastButtonState = HIGH;
bool buttonPressed = false;
unsigned long lastButtonTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// Watchdog and timeout functions
void setupWatchdog() {
    if (WATCHDOG_ENABLED) {
        esp_task_wdt_init(WATCHDOG_TIMEOUT_MS / 1000, true); // Convert to seconds
        esp_task_wdt_add(NULL); // Add current task to watchdog
        Serial.println("[Main] Watchdog timer enabled (" + String(WATCHDOG_TIMEOUT_MS / 1000) + "s timeout)");
    } else {
        Serial.println("[Main] Watchdog timer disabled");
    }
}

void feedWatchdog() {
    if (WATCHDOG_ENABLED) {
        unsigned long currentTime = millis();
        if (currentTime - lastWatchdogFeed >= 1000) { // Feed every second
            esp_task_wdt_reset();
            lastWatchdogFeed = currentTime;
        }
    }
}

bool executeWithTimeout(std::function<bool()> operation, unsigned long timeoutMs, const String& operationName) {
    unsigned long startTime = millis();
    Serial.println("[Timeout] Starting " + operationName + " (timeout: " + String(timeoutMs) + "ms)");
    
    while ((millis() - startTime) < timeoutMs) {
        feedWatchdog(); // Keep watchdog happy during operation
        
        if (operation()) {
            Serial.println("[Timeout] " + operationName + " completed successfully");
            return true;
        }
        
        delay(100); // Small delay to prevent tight loop
    }
    
    Serial.println("[Timeout] " + operationName + " timed out after " + String(timeoutMs) + "ms");
    return false;
}

// LED control functions
void setLED(CRGB color) {
    if (LED_ENABLED) {
        leds[0] = color;
        FastLED.show();
    }
}

void updateButton() {
    bool currentButtonState = digitalRead(BUTTON_PIN);
    
    if (currentButtonState != lastButtonState) {
        lastButtonTime = millis();
    }
    
    if ((millis() - lastButtonTime) > DEBOUNCE_DELAY) {
        if (currentButtonState == LOW && lastButtonState == HIGH) {
            buttonPressed = true;
        }
    }
    
    lastButtonState = currentButtonState;
}

bool wasButtonPressed() {
    if (buttonPressed) {
        buttonPressed = false;
        return true;
    }
    return false;
}


void setupWiFi() {
    // Set up WiFi callbacks for status indication
    wifiManager.setOnConnected([]() {
        setLED(COLOR_GREEN);
        Serial.println("[Main] WiFi connected" + String(LED_ENABLED ? " - LED set to GREEN" : ""));
    });
    
    wifiManager.setOnDisconnected([]() {
        setLED(COLOR_RED);
        Serial.println("[Main] WiFi disconnected" + String(LED_ENABLED ? " - LED set to RED" : ""));
    });
    
    wifiManager.setOnReconnectAttempt([](int attempt) {
        setLED(COLOR_YELLOW);
        Serial.println("[Main] WiFi reconnect attempt " + String(attempt) + String(LED_ENABLED ? " - LED set to YELLOW" : ""));
    });
    
    wifiManager.setOnMaxAttemptsReached([]() {
        setLED(COLOR_RED);
        Serial.println("[Main] WiFi max attempts reached - System will restart!" + String(LED_ENABLED ? " - LED set to RED" : ""));
    });
    
    // Initialize WiFi with credentials from config
    setLED(COLOR_YELLOW);
    wifiManager.begin(WIFI_SSID, WIFI_PASSWORD);
}

void setupMQTT() {
    mqttClient.begin(MQTT_SERVER, MQTT_PORT, MQTT_USER, MQTT_PASSWORD, MQTT_CLIENT_ID);
}

void setupOTA() {
    if (!OTA_ENABLED) {
        return;
    }
    
    // Set up OTA callbacks
    otaManager.setOnStart([]() {
        setLED(COLOR_YELLOW);
    });
    
    otaManager.setOnEnd([]() {
        setLED(COLOR_GREEN);
    });
    
    otaManager.setOnProgress([](size_t current, size_t total) {
        int progress = (current * 100) / total;
        
        // Blink LED during update
        if (progress % 10 == 0) {
            setLED((progress % 20 == 0) ? COLOR_BLUE : COLOR_YELLOW);
        }
    });
    
    otaManager.setOnError([](String error) {
        setLED(COLOR_RED);
    });
    
    otaManager.begin();
}

void setupBLE() {
    bluetoothManager.begin();
    
    // Set up LED callbacks for connection status
    bluetoothManager.setOnConnect([]() {
        setLED(COLOR_BLUE);
    });
    
    bluetoothManager.setOnDisconnect([]() {
        setLED(COLOR_RED);
    });
}

void readBatteryData(const String& macAddress) {
    BatteryData batteryData;
    
    try {
        // Use BluetoothManager to read battery data
        bool success = bluetoothManager.readBatteryData(macAddress, batteryData);
        
        // Store data for web display and MQTT
        if (success && batteryData.dataValid) {
            for (int i = 0; i < BATTERY_COUNT; i++) {
                if (BATTERY_MAC_ADDRESSES[i] == macAddress) {
                    // Update web server data with new values
                    webServerManager.updateBatteryData(i, batteryData);
                    webServerManager.setBatteryDataUpdateTime(i, millis());
                    Serial.println("Battery data updated for battery " + String(i + 1));
                    break;
                }
            }
            
            // Publish battery data to MQTT
            if (mqttClient.isConnected()) {
                mqttClient.publishBatteryData(batteryData);
            }
        } else {
            // Battery read failed - preserve existing data but don't update timestamp
            // This allows the UI to detect the battery as offline while keeping last known values
            Serial.println("Failed to read battery data from " + macAddress + " - preserving last known values");
        }
    } catch (...) {
        Serial.println("Exception during battery data reading for " + macAddress + " - preserving last known values");
    }
}

void setupWebServer() {
    webServerManager.begin();
}

void setup() {
    Serial.begin(115200);
    Serial.println("[Main] System starting...");
    
    // Initialize Watchdog Timer first
    setupWatchdog();
    feedWatchdog();
    
    // Initialize button pin
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    // Initialize FastLED only if LEDs are enabled
    if (LED_ENABLED) {
        FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
        FastLED.setBrightness(50); // Set brightness to 50%
        setLED(COLOR_RED);
        Serial.println("[Main] LED indicators enabled");
    } else {
        Serial.println("[Main] LED indicators disabled");
    }
    
    feedWatchdog();
    
    // Setup WiFi with timeout handling
    Serial.println("[Main] Setting up WiFi...");
    setupWiFi();
    feedWatchdog();
    
    // Setup MQTT with timeout handling
    Serial.println("[Main] Setting up MQTT...");
    executeWithTimeout([]() {
        setupMQTT();
        return true; // MQTT setup doesn't have a return value, assume success
    }, MANAGER_TIMEOUT_MS, "MQTT Setup");
    feedWatchdog();
    
    // Setup OTA with timeout handling
    Serial.println("[Main] Setting up OTA...");
    executeWithTimeout([]() {
        setupOTA();
        return true; // OTA setup doesn't have a return value, assume success
    }, MANAGER_TIMEOUT_MS, "OTA Setup");
    feedWatchdog();
    
    // Setup BLE with timeout handling
    Serial.println("[Main] Setting up BLE...");
    executeWithTimeout([]() {
        setupBLE();
        return true; // BLE setup doesn't have a return value, assume success
    }, MANAGER_TIMEOUT_MS, "BLE Setup");
    feedWatchdog();
    
    // Setup Web Server with timeout handling
    if (wifiManager.isConnected()) {
        Serial.println("[Main] Setting up Web Server...");
        executeWithTimeout([]() {
            setupWebServer();
            return true; // WebServer setup doesn't have a return value, assume success
        }, MANAGER_TIMEOUT_MS, "WebServer Setup");
        Serial.println("Web server started at http://" + wifiManager.getLocalIP());
    }
    feedWatchdog();
    
    // Force first scan to happen soon
    lastScanTime = millis() - SCAN_INTERVAL_MS + 5000; // Start first scan in 5 seconds
    
    setLED(COLOR_GREEN);
    Serial.println("[Main] System initialization completed");
}

void loop() {
    // Feed watchdog at the beginning of each loop iteration
    feedWatchdog();
    
    // Update button state
    updateButton();
    
    // Handle WiFi management (reconnection, monitoring, etc.) with timeout
    executeWithTimeout([]() {
        wifiManager.loop();
        return true;
    }, MANAGER_TIMEOUT_MS, "WiFi Manager Loop");
    
    // Handle MQTT with timeout
    executeWithTimeout([]() {
        mqttClient.loop();
        return true;
    }, MANAGER_TIMEOUT_MS, "MQTT Loop");
    
    // Handle OTA with timeout
    executeWithTimeout([]() {
        otaManager.loop();
        return true;
    }, MANAGER_TIMEOUT_MS, "OTA Loop");
    
    // Handle Web Server with timeout
    executeWithTimeout([]() {
        webServerManager.handleClient();
        return true;
    }, MANAGER_TIMEOUT_MS, "WebServer Loop");
    
    // Check if it's time to scan batteries
    unsigned long currentTime = millis();
    unsigned long timeSinceLastScan = currentTime - lastScanTime;
    
    if (timeSinceLastScan >= SCAN_INTERVAL_MS) {
        lastScanTime = currentTime;
        
        // Indicate scanning
        setLED(COLOR_BLUE);
        
        // Read data from all batteries in sequence with timeout handling
        Serial.println("Starting battery scan cycle...");
        
        for (int i = 0; i < BATTERY_COUNT; i++) {
            String macAddress = BATTERY_MAC_ADDRESSES[i];
            
            Serial.println("Scanning battery " + String(i + 1) + ": " + macAddress);
            
            // Execute battery read with timeout
            bool success = executeWithTimeout([macAddress]() {
                readBatteryData(macAddress);
                return true; // Assume success for now
            }, CONNECTION_TIMEOUT_MS, "Battery " + String(i + 1) + " Read");
            
            if (!success) {
                Serial.println("Battery " + String(i + 1) + " scan timed out");
            }
            
            feedWatchdog(); // Feed watchdog between battery scans
            
            // Add delay between battery scans to prevent BLE conflicts
            if (i < BATTERY_COUNT - 1) {
                unsigned long delayStart = millis();
                while (millis() - delayStart < 2000) {
                    feedWatchdog();
                    delay(100);
                }
            }
        }
        
        Serial.println("Battery scan cycle completed.");
        
        // Show status LED based on WiFi and MQTT connection status
        if (wifiManager.isConnected() && mqttClient.isConnected()) {
            setLED(COLOR_GREEN);
        } else if (wifiManager.isConnected()) {
            setLED(COLOR_YELLOW);
        } else {
            setLED(COLOR_RED);
        }
    }
    
    // Handle button press for manual scan
    if (wasButtonPressed()) {
        lastScanTime = 0; // Force immediate scan
        Serial.println("[Main] Manual scan triggered by button press");
    }
    
    // Small delay with watchdog feeding
    unsigned long delayStart = millis();
    while (millis() - delayStart < 100) {
        feedWatchdog();
        delay(10);
    }
}
