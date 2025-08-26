#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// WiFi Configuration
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// MQTT Configuration
#define MQTT_SERVER "YOUR_MQTT_SERVER_IP"
#define MQTT_PORT 1883
#define MQTT_USER "YOUR_MQTT_USERNAME"
#define MQTT_PASSWORD "YOUR_MQTT_PASSWORD"
#define MQTT_CLIENT_ID "eco-worthy-logger"
#define MQTT_TOPIC_PREFIX "eco-worthy"

// OTA Configuration
#define OTA_ENABLED true
#define OTA_PASSWORD "YOUR_OTA_PASSWORD"  // OTA update password

// LED Configuration
#define LED_ENABLED true  // Enable/disable LED status indicators

// Watchdog Configuration
#define WATCHDOG_ENABLED true        // Enable hardware watchdog timer
#define WATCHDOG_TIMEOUT_MS 30000    // 30 seconds watchdog timeout
#define MANAGER_TIMEOUT_MS 5000      // 5 seconds timeout for manager operations

// Battery Configuration
#define BATTERY_COUNT 2
#define SCAN_INTERVAL_MS 30000  // 30 seconds between scans
#define CONNECTION_TIMEOUT_MS 10000  // 10 seconds connection timeout

// Battery MAC Addresses
// Replace with your actual battery MAC addresses
const String BATTERY_MAC_ADDRESSES[BATTERY_COUNT] = {
    "XX:XX:XX:XX:XX:XX",  // Battery 1 MAC address
    "XX:XX:XX:XX:XX:XX"   // Battery 2 MAC address
};

// Bluetooth Service and Characteristic UUIDs
#define SERVICE_UUID "0000ff00-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_WRITE_UUID "0000ff02-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_READ_UUID "0000ff01-0000-1000-8000-00805f9b34fb"

// Protocol Commands
#define CMD_READ_BASIC_INFO 0x03
#define CMD_READ_CELL_VOLTAGES 0x04
#define CMD_READ_HARDWARE_VERSION 0x05

// Protocol Frame Structure
#define FRAME_START 0xDD
#define FRAME_READ 0xA5
#define FRAME_WRITE 0x5A
#define FRAME_END 0x77

#endif
