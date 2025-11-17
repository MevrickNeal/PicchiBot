// =======================================================
//  PICCHIBOT - PART 7: .INO COMPILER FIX
// =======================================================
// This code fixes all 3 compiler errors from your log
// by only editing the .ino file.
//
// 1. Adds '#undef' to fix the 'N' and 'E' macro collision.
// 2. Replaces 'ledcSetup' with 'tone()' for the buzzer.
// 3. Fixes the 'mpy.getAngleY()' typo.
// =======================================================

// --- Core & Sensor Libraries ---
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MPU6050_light.h>
#include "FluxGarage_RoboEyes.h" // Your provided library

// --- FIX #1: 'N' and 'E' MACRO CONFLICT ---
// We undefine the macros from RoboEyes.h immediately
// so they don't conflict with the ESP32 libraries.
#undef N
#undef NE
#undef E
#undef SE
#undef S
#undef SW
#undef W
#undef NW
// --- END OF FIX #1 ---

// --- Network & Utility Libraries ---
#include <WiFi.h>
#include <WebServer.h> // Using the blocking server from your plan
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <Preferences.h>

// ----------------- CONFIG (From Your Plan) -----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

// --- NEW PIN MAP (FROM YOUR PLAN) ---
#define PIN_BUZZER 14 //
#define BTN_UP     26 //
#define BTN_DOWN   33 //
#define BTN_LEFT   25 //
#define BTN_RIGHT  27 
#define BTN_OK     32
// BUZZER_CHANNEL is no longer needed

// Wi-Fi AP creds
const char *AP_SSID = "PicchiBot";
const char *AP_PASS = "amishudhutomar";

// OpenWeather
const char* OW_APIKEY = "9c5ed9a6da0fc29aac5db777c9638d1a";

// ----------------- GLOBALS -----------------
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
RoboEyes<Adafruit_SSD1306> eyes(display);
MPU6050 mpu(Wire);
Preferences prefs;

// --- FreeRTOS & Task Handles ---
TaskHandle_t hNetworkTask;
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

// --- SHARED STATE VARIABLES (Protected by Mutex) ---
String globalIP = "No IP";
String globalCity = "Dhaka";
String globalWeatherCond = "Loading...";
float globalWeatherTemp = -99;
bool globalIsConnected = false;

// --- UI State (Core 1 Only) ---
enum Screen { SCR_PET, SCR_MENU, SCR_WEATHER };
Screen currentScreen = SCR_PET;

int menuIndex = 0;
String menuItems[] = {"Weather", "Reboot", "Exit"};
int menuItemCount = 3;

unsigned long lastButtonPress = 0;
unsigned long debounceDelay = 200;

// ----------------- HELPER SOUNDS -----------------
// *** FIX #2: Replaced ledc with tone() ***
void beep(int freq, int duration) {
  tone(PIN_BUZZER, freq, duration);
  // We add a small delay so the sound can play.
  // This is a tradeoff for not using the non-blocking ledc.
  delay(duration + 10); 
}

// =======================================================
//  CORE 0 - NETWORK TASK
// =======================================================
// (This section is unchanged)
WebServer webServer(80);
DNSServer dnsServer;

void handleRoot() {
  String page = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>PicchiBot Setup</title></head><body>";
  page += "<h3>PicchiBot Setup</h3>";
  page += "<form method='POST' action='/save'>";
  page += "Your Name:<br><input name='name'><br>City:<br><input name='city' value='Dhaka'><br><hr>";
  page += "Home WiFi SSID:<br><input name='ssid'><br>Home WiFi PASS:<br><input name='pass'><br>";
  page += "<br><input type='submit' value='Save & Connect'></form>";
  page += "</body></html>";
  webServer.send(200, "text/html", page);
}

void handleSave() {
  String savedSSID = webServer.arg("ssid");
  String savedPASS = webServer.arg("pass");
  String savedCity = webServer.arg("city");

  if (savedSSID.length() > 0) {
    prefs.begin("picchi", false);
    prefs.putString("city", savedCity);
    prefs.putString("ssid", savedSSID);
    prefs.putString("pass", savedPASS);
    prefs.end();
    String resp = "<html><body><h3>Saved. Rebooting...</h3></body></html>";
    webServer.send(200, "text/html", resp);
    delay(1000);
    ESP.restart();
  } else {
    webServer.send(400, "text/plain", "missing");
  }
}

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  prefs.begin("picchi", true);
  String city = prefs.getString("city", "Dhaka");
  String country = prefs.getString("country", "BD");
  prefs.end();
  Serial.print("Fetching weather for: "); Serial.println(city);
  String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + country + "&APPID=" + String(OW_APIKEY) + "&units=metric";
  HTTPClient http;
  WiFiClient client;
  http.begin(client, serverPath);
  int code = http.GET();
  float temp = -99;
  String cond = "Error";
  if (code > 0) {
    String payload = http.getString();
    JSONVar o = JSON.parse(payload);
    if (JSON.typeof(o) != "undefined") {
      if (o["main"]["temp"] != NULL) temp = (double)o["main"]["temp"];
      if (o["weather"][0]["main"] != NULL) cond = (const char*)o["weather"][0]["main"];
    } else {
      Serial.println("JSON Parse Error");
    }
  } else {
    Serial.print("HTTP Error: "); Serial.println(code);
  }
  http.end();
  portENTER_CRITICAL(&stateMux);
  globalWeatherTemp = temp;
  globalWeatherCond = cond;
  portEXIT_CRITICAL(&stateMux);
  Serial.print("Weather updated: "); Serial.print(temp); Serial.println("C");
}

void taskNetwork(void *pvParameters) {
  Serial.println("Network Task started on Core 0");
  prefs.begin("picchi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  String city = prefs.getString("city", "Dhaka");
  prefs.end();
  bool connected = false;
  if (ssid.length() > 0) {
    Serial.print("Trying to connect to: "); Serial.println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      vTaskDelay(500 / portTICK_PERIOD_MS);
      Serial.print(".");
      retries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
    }
  }
  portENTER_CRITICAL(&stateMux);
  if (connected) {
    globalIsConnected = true;
    globalIP = WiFi.localIP().toString();
    globalCity = city;
  } else {
    globalIsConnected = false;
    globalIP = "AP Mode";
  }
  portEXIT_CRITICAL(&stateMux);
  if (!connected) {
    Serial.println("Starting Setup Portal (AP Mode)...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    IPAddress apIP = WiFi.softAPIP();
    Serial.print("AP IP: "); Serial.println(apIP);
    dnsServer.start(53, "*", apIP);
    webServer.on("/", handleRoot);
    webServer.on("/save", HTTP_POST, handleSave);
    webServer.onNotFound(handleRoot);
    webServer.begin();
    while (true) {
      dnsServer.processNextRequest();
      webServer.handleClient();
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
  } else {
    Serial.println("WiFi Connected!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    while (true) {
      Serial.println("Fetching weather...");
      fetchWeather();
      vTaskDelay(900000 / portTICK_PERIOD_MS); // 15 min
    }
  }
}

// =======================================================
//  SETUP()
// =======================================================
void setup() {
  Serial.begin(115200);

  // --- Initialize Hardware (using NEW pins) ---
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);

  // *** FIX #2: Replaced ledcSetup/ledcAttachPin ***
  pinMode(PIN_BUZZER, OUTPUT);

  Wire.begin(21, 22); // Explicit SDA, SCL
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 failed");
    for (;;);
  }
  display.clearDisplay();

  // --- Manual Wi-Fi Reset Check ---
  if (digitalRead(BTN_OK) == LOW) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(10, 15);
    display.println(F("Wi-Fi Reset Detected!"));
    display.setCursor(10, 30);
    display.println(F("Erasing saved Wi-Fi."));
    display.display();
    beep(800, 100);
    beep(400, 100);
    prefs.begin("picchi", false);
    prefs.clear(); // Erase all keys
    prefs.end();
    delay(2000);
  }

  // --- RoboEyes & MPU init ---
  eyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 50);
  eyes.setAutoblinker(true, 2, 4);
  eyes.setIdleMode(true, 3, 4);
  eyes.open();

  mpu.begin();
  Serial.println("Calculating MPU offsets...");
  mpu.calcOffsets(true, true);
  Serial.println("MPU Done."); // *** FIX: Added missing parenthesis ***

  // --- Boot Animation ---
  eyes.anim_wake_dramatic();
  for(int i=0; i<30; i++) { eyes.update(); delay(20); }
  beep(1200, 80);
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.println("PicchiBot");
  display.display();
  delay(1500);
  eyes.open();

  // --- START THE NETWORK TASK (Core 0) ---
  xTaskCreatePinnedToCore(
      taskNetwork,      // Function to run
      "Network Task",   // Name
      10000,            // Stack size (10k for WebServer+JSON)
      NULL,             // Parameters
      1,                // Priority
      &hNetworkTask,    // Task handle
      0                 // Core (Core 0)
  );
  
  Serial.println("Setup finished. Starting UI loop on Core 1.");
  currentScreen = SCR_PET;
}

// =======================================================
//  LOOP() - CORE 1 - UI TASK
// =======================================================
void loop() {
  // --- Global Button Reads ---
  bool btnUp = (digitalRead(BTN_UP) == LOW);
  bool btnDown = (digitalRead(BTN_DOWN) == LOW);
  bool btnLeft = (digitalRead(BTN_LEFT) == LOW);
  bool btnRight = (digitalRead(BTN_RIGHT) == LOW);
  bool btnOk = (digitalRead(BTN_OK) == LOW);

  // --- Main UI State Machine ---
  switch (currentScreen) {
    case SCR_PET:
      handlePetScreen(btnUp, btnDown, btnLeft, btnRight, btnOk);
      break;
    case SCR_MENU:
      handleMenuScreen(btnUp, btnDown, btnLeft, btnRight, btnOk);
      break;
    case SCR_WEATHER:
      handleWeatherScreen(btnUp, btnDown, btnLeft, btnRight, btnOk);
      break;
  }

  // This is the ONLY place eyes.update() should be called.
  if (currentScreen == SCR_PET) {
    eyes.update();
  }
  
  vTaskDelay(10 / portTICK_PERIOD_MS); // Small yield
}

// =======================================================
//  UI SCREEN HANDLERS (Non-Blocking)
// =======================================================

void handlePetScreen(bool up, bool down, bool left, bool right, bool ok) {
  // --- MPU6050 Eye Control (High Sensitivity) ---
  mpu.update();
  float ax = mpu.getAngleX();
  
  // *** FIX #3: 'mpy' typo corrected to 'mpu' ***
  float ay = mpu.getAngleY(); 

  // Map a small tilt (-15 to +15 deg) to the full eye range
  int posX = map((int)ay, -15, 15, 0, eyes.getScreenConstraint_X());
  int posY = map((int)ax, -15, 15, 0, eyes.getScreenConstraint_Y());

  // Constrain ensures we don't go out of bounds if tilted > 15 deg
  eyes.eyeLxNext = constrain(posX, 0, eyes.getScreenConstraint_X());
  eyes.eyeLyNext = constrain(posY, 0, eyes.getScreenConstraint_Y());

  // --- Button Inputs ---
  if ((millis() - lastButtonPress) > debounceDelay) {
    if (left || right) {
      // Petting
      eyes.anim_happy();
      beep(1500,80);
      lastButtonPress = millis();
    }
    
    if (up) {
      eyes.anim_shocked();
      beep(1800, 80);
      lastButtonPress = millis();
    }

    if (down) {
      eyes.anim_sad();
      beep(400, 100);
      lastButtonPress = millis();
    }
    
    if (ok) {
      // Go to Menu
      beep(800, 50);
      currentScreen = SCR_MENU;
      menuIndex = 0; // Reset menu
      lastButtonPress = millis();
    }
  }

  // --- Draw WiFi/IP Status ---
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  
  // Safely read the IP
  String ip;
  portENTER_CRITICAL(&stateMux);
  ip = globalIP;
  portEXIT_CRITICAL(&stateMux);
  display.print(ip);
}

void handleMenuScreen(bool up, bool down, bool left, bool right, bool ok) {
  // --- Handle Menu Navigation ---
  if ((millis() - lastButtonPress) > debounceDelay) {
    if (down) {
      menuIndex = (menuIndex + 1) % menuItemCount;
      beep(1000, 50);
      lastButtonPress = millis();
    }
    if (up) {
      menuIndex = (menuIndex - 1 + menuItemCount) % menuItemCount;
      beep(1000, 50);
      lastButtonPress = millis();
    }
    if (ok) {
      // Select Item
      beep(1200, 80);
      if (menuItems[menuIndex] == "Weather") {
        currentScreen = SCR_WEATHER;
      }
      if (menuItems[menuIndex] == "Reboot") {
        display.clearDisplay();
        display.setCursor(10,20);
        display.println("Rebooting...");
        display.display();
        delay(1000);
        ESP.restart();
      }
      if (menuItems[menuIndex] == "Exit") {
        currentScreen = SCR_PET;
      }
      lastButtonPress = millis();
    }
    if (left) {
      // Back to Pet
      beep(800, 50);
      currentScreen = SCR_PET;
      lastButtonPress = millis();
    }
  }
  
  // --- Draw Menu ---
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(30, 0);
  display.println("--- MENU ---");
  
  for (int i = 0; i < menuItemCount; i++) {
    display.setCursor(10, 15 + (i * 12));
    if (i == menuIndex) {
      display.print(">> ");
      display.print(menuItems[i]);
    } else {
      display.print("   ");
      display.print(menuItems[i]);
    }
  }
  display.display();
}

void handleWeatherScreen(bool up, bool down, bool left, bool right, bool ok) {
  // --- Safely read the weather data ---
  float temp;
  String cond;
  portENTER_CRITICAL(&stateMux);
  temp = globalWeatherTemp;
  cond = globalWeatherCond;
  portEXIT_CRITICAL(&stateMux);

  // --- Draw the Weather Screen ---
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(15, 0);
  display.println("--- WEATHER ---");
  
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.print(temp, 1);
  display.print("C");
  
  display.setTextSize(1);
  display.setCursor(10, 40);
  display.print(cond);
  
  display.setCursor(20, 55);
  display.print("(Press LEFT to Exit)");
  display.display();
  
  // --- Handle Input ---
  if ((millis() - lastButtonPress) > debounceDelay) {
    if (left || ok) {
      beep(800, 50);
      currentScreen = SCR_MENU; // Go back to menu
      lastButtonPress = millis();
    }
  }
}
