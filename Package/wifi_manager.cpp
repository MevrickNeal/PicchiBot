// -----------------------------
// FILE: src/wifi_manager.cpp
// -----------------------------
#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>


static Preferences preferences;
static AsyncWebServer server(80);
static DNSServer dnsServer;


void saveCreds(const String &s, const String &p) {
preferences.begin("picchi-creds", false);
preferences.putString("ssid", s);
preferences.putString("pass", p);
preferences.end();
}


const char index_html[] PROGMEM = R"rawliteral(
<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>PicchiBot Setup</title></head><body><h3>PicchiBot Setup</h3>
<form action="/save" method="post">
SSID:<br><input name="ssid"><br>Password:<br><input name="pass"><br><input type="submit" value="Save">
</form></body></html>
)rawliteral";


void WiFiManager::taskEntry(void *pvParameters) {
preferences.begin("picchi-creds", false);
String ssid = preferences.getString("ssid", "");
String pass = preferences.getString("pass", "");
preferences.end();


bool connected = false;
if (ssid.length() > 0) {
WiFi.begin(ssid.c_str(), pass.c_str());
int retries = 0;
while (WiFi.status() != WL_CONNECTED && retries++ < 40) vTaskDelay(500 / portTICK_PERIOD_MS);
if (WiFi.status() == WL_CONNECTED) connected = true;
}


if (!connected) {
WiFi.softAP("PicchiBot", "amishudhutomar");
IPAddress apIP = WiFi.softAPIP();
dnsServer.start(53, "*", apIP);

server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){ req->send_P(200, "text/html", index_html); });
server.on("/save", HTTP_POST, [](AsyncWebServerRequest *req){
String s="", p="";
if (req->hasParam("ssid", true)) s = req->getParam("ssid", true)->value();
if (req->hasParam("pass", true)) p = req->getParam("pass", true)->value();
saveCreds(s,p);
req->send(200, "text/html", "Saved. Rebooting...");
vTaskDelay(1000 / portTICK_PERIOD_MS);
ESP.restart();
});
server.onNotFound([](AsyncWebServerRequest *req){ req->send_P(200, "text/html", index_html); });
server.begin();
while(true) { dnsServer.processNextRequest(); vTaskDelay(10 / portTICK_PERIOD_MS); }
}


for(;;) { vTaskDelay(10000 / portTICK_PERIOD_MS); if (WiFi.status() != WL_CONNECTED) { ESP.restart(); } }
}
