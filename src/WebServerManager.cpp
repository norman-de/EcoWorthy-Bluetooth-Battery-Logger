#include "WebServerManager.h"

WebServerManager::WebServerManager() 
    : webServer(nullptr)
    , serverRunning(false)
{
    initializeBatteryData();
}

WebServerManager::~WebServerManager() {
    if (webServer) {
        delete webServer;
        webServer = nullptr;
    }
}

void WebServerManager::begin() {
    if (webServer) {
        delete webServer;
    }
    
    webServer = new WebServer(80);
    
    // Set up routes
    webServer->on("/", [this]() { handleRoot(); });
    webServer->on("/api/data", [this]() { handleApiData(); });
    
    webServer->begin();
    serverRunning = true;
    
    Serial.println("WebServerManager started successfully");
}

void WebServerManager::handleClient() {
    if (webServer && serverRunning) {
        webServer->handleClient();
    }
}

void WebServerManager::updateBatteryData(int batteryIndex, const BatteryData& batteryData) {
    if (batteryIndex >= 0 && batteryIndex < BATTERY_COUNT) {
        latestBatteryData[batteryIndex] = batteryData;
    }
}

void WebServerManager::setBatteryDataUpdateTime(int batteryIndex, unsigned long updateTime) {
    if (batteryIndex >= 0 && batteryIndex < BATTERY_COUNT) {
        lastDataUpdate[batteryIndex] = updateTime;
    }
}

bool WebServerManager::isRunning() const {
    return serverRunning;
}

void WebServerManager::initializeBatteryData() {
    for (int i = 0; i < BATTERY_COUNT; i++) {
        // Only initialize if no valid data exists yet
        if (lastDataUpdate[i] == 0) {
            latestBatteryData[i].dataValid = false;
            latestBatteryData[i].soc = 0;
            latestBatteryData[i].voltage = 0.0;
            latestBatteryData[i].current = 0.0;
            latestBatteryData[i].watts = 0.0;
            latestBatteryData[i].temperature = 0.0;
            latestBatteryData[i].remainingAh = 0.0;
            latestBatteryData[i].numCells = 0;
        }
        // If data already exists, preserve it but don't reset lastDataUpdate
    }
}

// Memory-efficient HTML page (stored in PROGMEM)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ECO-WORTHY Batterien</title>
<style>
body{font-family:Arial;margin:10px;background:#f0f0f0}
.container{max-width:800px;margin:0 auto}
.battery{background:white;margin:10px 0;padding:15px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}
.header{color:#333;margin:0 0 10px 0;font-size:18px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:10px}
.item{text-align:center}
.label{font-size:12px;color:#666;margin-bottom:2px}
.value{font-size:16px;font-weight:bold;color:#333}
.soc{font-size:24px;color:#2196F3}
.voltage{color:#4CAF50}
.current{color:#FF9800}
.temp{color:#9C27B0}
.offline{opacity:0.5}
.cells{margin-top:10px}
.cell{display:inline-block;margin:2px;padding:4px 6px;background:#e0e0e0;border-radius:4px;font-size:11px}
</style>
</head>
<body>
<div class="container">
<h1>ECO-WORTHY Batterie Monitor</h1>
<div id="batteries"></div>
</div>
<script>
function updateData(){
fetch('/api/data').then(r=>r.json()).then(data=>{
let html='';
data.forEach((bat,i)=>{
const offline=bat.ageSeconds>120;
html+=`<div class="battery ${offline?'offline':''}">
<h2 class="header">Batterie ${i+1} ${offline?'(Offline)':''}</h2>
<div class="grid">
<div class="item"><div class="label">SOC</div><div class="value soc">${bat.soc}%</div></div>
<div class="item"><div class="label">Spannung</div><div class="value voltage">${bat.voltage}V</div></div>
<div class="item"><div class="label">Strom</div><div class="value current">${bat.current}A</div></div>
<div class="item"><div class="label">Leistung</div><div class="value">${bat.watts}W</div></div>
<div class="item"><div class="label">Temperatur</div><div class="value temp">${bat.temperature}°C</div></div>
<div class="item"><div class="label">Verbleibend</div><div class="value">${bat.remainingAh}Ah</div></div>
</div>`;
if(bat.numCells>0){
html+='<div class="cells">';
for(let j=0;j<bat.numCells;j++){
html+=`<span class="cell">${bat.cellVoltages[j]}V</span>`;
}
html+='</div>';
}
html+='</div>';
});
document.getElementById('batteries').innerHTML=html;
}).catch(e=>console.error('Fehler:',e));
}
updateData();
setInterval(updateData,5000);
</script>
</body>
</html>
)rawliteral";

void WebServerManager::handleRoot() {
    if (webServer) {
        webServer->send_P(200, "text/html", index_html);
    }
}

void WebServerManager::handleApiData() {
    if (!webServer) {
        return;
    }
    
    // Pre-allocate string with estimated size to prevent frequent reallocations
    String json;
    json.reserve(2048); // Reserve space to prevent memory fragmentation
    
    json = "[";
    for (int i = 0; i < BATTERY_COUNT; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"soc\":" + String(latestBatteryData[i].soc) + ",";
        json += "\"voltage\":" + String(latestBatteryData[i].voltage, 2) + ",";
        json += "\"current\":" + String(latestBatteryData[i].current, 2) + ",";
        json += "\"watts\":" + String(latestBatteryData[i].watts, 1) + ",";
        json += "\"temperature\":" + String(latestBatteryData[i].temperature, 1) + ",";
        json += "\"remainingAh\":" + String(latestBatteryData[i].remainingAh, 1) + ",";
        json += "\"numCells\":" + String(latestBatteryData[i].numCells) + ",";
        json += "\"cellVoltages\":[";
        
        // Limit cell voltages to prevent excessive memory usage
        int maxCells = min((int)latestBatteryData[i].numCells, 16); // Limit to 16 cells max
        for (int j = 0; j < maxCells; j++) {
            if (j > 0) json += ",";
            json += String(latestBatteryData[i].cellVoltages[j], 3);
        }
        json += "],";
        
        // Calculate age in seconds since last update
        unsigned long currentTime = millis();
        unsigned long ageSeconds = 0;
        if (lastDataUpdate[i] > 0) {
            if (currentTime >= lastDataUpdate[i]) {
                ageSeconds = (currentTime - lastDataUpdate[i]) / 1000;
            } else {
                // Handle millis() overflow (happens every ~49 days)
                // When overflow occurs, calculate the time correctly
                unsigned long timeBeforeOverflow = (0xFFFFFFFF - lastDataUpdate[i]);
                unsigned long timeAfterOverflow = currentTime;
                ageSeconds = (timeBeforeOverflow + timeAfterOverflow + 1) / 1000;
            }
        } else {
            ageSeconds = 999; // No data received yet
        }
        
        json += "\"ageSeconds\":" + String(ageSeconds);
        json += "}";
    }
    json += "]";
    
    webServer->send(200, "application/json", json);
}
