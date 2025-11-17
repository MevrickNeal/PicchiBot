// =======================================================
//  PICCHIBOT - PART 8: PIN MAP & FEATURE FIX
// =======================================================
// This code fixes all the issues you reported:
//
// 1.  CORRECTED PIN MAP: Uses your latest pin map.
//     - OK: 32, UP: 26, DOWN: 33, LEFT: 25, RIGHT: 27
// 2.  CYBERPUNK BOOT: Replaced the simple boot with
//     your "glitch/cyber" effect boot animation.
// 3.  COUNTDOWN TIMER: Added the "Timer" to the menu.
//     It's now non-blocking and runs in the background.
// =======================================================

// --- Core & Sensor Libraries ---
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MPU6050_light.h>
#include "FluxGarage_RoboEyes.h" // Your provided library

// --- FIX #1: 'N' and 'E' MACRO CONFLICT ---
// (This is still required to fix the .h file conflict)
#undef N
#undef NE
#undef E
#undef SE
#undef S
#undef SW
#undef W
#undef NW

// --- Network & Utility Libraries ---
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <Preferences.h>

// ----------------- CONFIG -----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

// --- *** FIX #1: YOUR NEW PIN MAP *** ---
#define PIN_BUZZER 14
#define BTN_UP     26  // Was 32
#define BTN_DOWN   33  // Was 33
#define BTN_LEFT   25  // Was 25
#define BTN_RIGHT  27  // Was 26
#define BTN_OK     32  // Was 27
// -----------------------------------------

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
enum Screen { SCR_PET, SCR_MENU, SCR_WEATHER, SCR_TIMER_SETUP };
Screen currentScreen = SCR_PET;

int menuIndex = 0;
// *** FIX #4: Added Timer to menu ***
String menuItems[] = {"Weather", "Timer", "Reboot", "Exit"};
int menuItemCount = 4;

unsigned long lastButtonPress = 0;
unsigned long debounceDelay = 200;

// --- FIX #4: Countdown Timer Globals ---
bool countdownRunning = false;
unsigned long countdownRemaining = 0; // ms
unsigned long countdownTick = 0;
unsigned long timerSetupSeconds = 30; // Default 30s


// ----------------- HELPER SOUNDS -----------------
void beep(int freq, int duration) {
  tone(PIN_BUZZER, freq, duration);
  delay(duration + 10);
}

// =======================================================
//  CORE 0 - NETWORK TASK
// =======================================================
// (This section is unchanged from Part 7)
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
  if (WiFi.status() != WL_CONNECTED) return;
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
    if (WiFi.status() == WL_CONNECTED) connected = true;
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
//  END OF NETWORK TASK
// =======================================================


// --- *** FIX #3: YOUR BOOT ANIMATION *** ---
void showBootIntro() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);

  // glitch/cyber effect
  for (int i=0;i<6;i++){
    display.clearDisplay();
    int x = random(-6,6);
    int y = random(-4,4);
    display.setCursor(8 + x, 6 + y);
    display.println("PicchiBot"); // Changed to 2 lines
    display.setTextSize(1);
    display.setCursor(20 + x, 40 + y);
    display.println("by Lian"); // Changed
    display.display();
    beep(1000 + i*200, 60);
  }
  // final stable
  display.clearDisplay();
  display.setTextSize(2); // Set size for "PicchiBot"
  display.setCursor(10, 20); // Centered
  display.println("PicchiBot");
  display.setTextSize(1);
  display.setCursor(40, 45);
  display.println("by Lian");
  display.display();
  beep(1800, 150);
  delay(1500);
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
  pinMode(PIN_BUZZER, OUTPUT);

  Wire.begin(21, 22); // Explicit SDA, SCL
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 failed");
    for (;;);
  }
  display.clearDisplay();

  // --- Manual Wi-Fi Reset Check ---
  // *** USES NEW PIN: BTN_OK (32) ***
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
  eyes.open(); // Start with eyes open

  mpu.begin();
  Serial.println("Calculating MPU offsets...");
  mpu.calcOffsets(true, true);
  Serial.println("MPU Done.");

  // --- *** FIX #3: Play Your Boot Animation *** ---
  showBootIntro();
  
  // After intro, show the pet
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
  // --- Global Button Reads (using NEW pins) ---
  bool btnUp = (digitalRead(BTN_UP) == LOW);       // 26
  bool btnDown = (digitalRead(BTN_DOWN) == LOW);   // 33
  bool btnLeft = (digitalRead(BTN_LEFT) == LOW);   // 25
  bool btnRight = (digitalRead(BTN_RIGHT) == LOW); // 27
  bool btnOk = (digitalRead(BTN_OK) == LOW);       // 32

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
    // --- FIX #4: Added Timer setup screen ---
    case SCR_TIMER_SETUP:
      handleTimerSetupScreen(btnUp, btnDown, btnLeft, btnRight, btnOk);
      break;
  }
  
  // --- FIX #4: Global Countdown Timer Logic ---
  // This runs in the background, no matter what screen you are on.
  if (countdownRunning) {
    if (millis() - countdownTick >= 500) { // Every half-second
      countdownRemaining = (countdownRemaining > 500) ? countdownRemaining - 500 : 0;
      countdownTick = millis();

      if (countdownRemaining == 0) {
        countdownRunning = false;
        // Big "boom"
        beep(1200, 200);
        delay(100);
        beep(900, 200);
      } else {
        // Prank "bomb" beep, gets faster
        int beepFreq = 600 + (int)((1.0 - (float)countdownRemaining / 30000.0) * 1000);
        beep(beepFreq, 80);
      }
    }
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
  float ay = mpu.getAngleY(); // Corrected 'mpy' typo

  int posX = map((int)ay, -15, 15, 0, eyes.getScreenConstraint_X());
  int posY = map((int)ax, -15, 15, 0, eyes.getScreenConstraint_Y());
  eyes.eyeLxNext = constrain(posX, 0, eyes.getScreenConstraint_X());
  eyes.eyeLyNext = constrain(posY, 0, eyes.getScreenConstraint_Y());

  // --- Button Inputs (using NEW pins) ---
  if ((millis() - lastButtonPress) > debounceDelay) {
    if (left) { // BTN_LEFT (25)
      eyes.anim_happy();
      beep(1500,80);
      lastButtonPress = millis();
    }
    if (right) { // BTN_RIGHT (27)
      // For now, also pet. We can change this.
      eyes.anim_happy();
      beep(1600, 80);
      lastButtonPress = millis();
    }
    if (up) { // BTN_UP (26)
      eyes.anim_shocked();
      beep(1800, 80);
      lastButtonPress = millis();
    }
    if (down) { // BTN_DOWN (33)
      eyes.anim_sad();
      beep(400, 100);
      lastButtonPress = millis();
    }
    if (ok) { // BTN_OK (32)
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
  
  String ip;
  portENTER_CRITICAL(&stateMux);
  ip = globalIP;
  portEXIT_CRITICAL(&stateMux);
  display.print(ip);
}

void handleMenuScreen(bool up, bool down, bool left, bool right, bool ok) {
  // --- Handle Menu Navigation (using NEW pins) ---
  if ((millis() - lastButtonPress) > debounceDelay) {
    if (down) { // BTN_DOWN (33)
      menuIndex = (menuIndex + 1) % menuItemCount;
      beep(1000, 50);
      lastButtonPress = millis();
    }
    if (up) { // BTN_UP (26)
      menuIndex = (menuIndex - 1 + menuItemCount) % menuItemCount;
      beep(1000, 50);
      lastButtonPress = millis();
    }
    if (ok) { // BTN_OK (32)
      beep(1200, 80);
      if (menuItems[menuIndex] == "Weather") {
        currentScreen = SCR_WEATHER;
      }
      if (menuItems[menuIndex] == "Timer") {
        currentScreen = SCR_TIMER_SETUP; // Go to timer setup
        timerSetupSeconds = 30; // Reset to default
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
    if (left) { // BTN_LEFT (25) - Use as "Back"
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
  float temp;
  String cond;
  portENTER_CRITICAL(&stateMux);
  temp = globalWeatherTemp;
  cond = globalWeatherCond;
  portEXIT_CRITICAL(&stateMux);

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
  display.setCursor(10, 55);
  display.print("(Press LEFT or OK)");
  display.display();
  
  if ((millis() - lastButtonPress) > debounceDelay) {
    if (left || ok) { // Use LEFT (25) or OK (32)
      beep(800, 50);
      currentScreen = SCR_MENU; // Go back to menu
      lastButtonPress = millis();
    }
  }
}

// --- *** FIX #4: New Timer Setup Screen *** ---
void handleTimerSetupScreen(bool up, bool down, bool left, bool right, bool ok) {
  // --- Handle Input ---
  if ((millis() - lastButtonPress) > debounceDelay) {
    if (up) { // BTN_UP (26)
      timerSetupSeconds += 5;
      if (timerSetupSeconds > 300) timerSetupSeconds = 300; // 5 min max
      beep(1100, 50);
      lastButtonPress = millis();
    }
    if (down) { // BTN_DOWN (33)
      timerSetupSeconds -= 5;
      if (timerSetupSeconds < 5) timerSetupSeconds = 5; // 5 sec min
      beep(900, 50);
      lastButtonPress = millis();
    }
    if (ok) { // BTN_OK (32)
      // START the timer
      countdownRemaining = timerSetupSeconds * 1000;
      countdownTick = millis();
      countdownRunning = true;
      currentScreen = SCR_PET; // Go back to pet screen
      beep(1400, 100);
      lastButtonPress = millis();
    }
    if (left) { // BTN_LEFT (25)
      // CANCEL / Back
      beep(800, 50);
      currentScreen = SCR_MENU; // Go back to menu
      lastButtonPress = millis();
    }
  }

  // --- Draw Screen ---
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10, 0);
  display.println("--- PRANK TIMER ---");
  
  display.setTextSize(2);
  display.setCursor(35, 20);
  display.print(timerSetupSeconds);
  display.println("s");
  
  display.setTextSize(1);
  display.setCursor(10, 45);
  display.print("UP/DOWN to change");
  display.setCursor(10, 55);
  display.print("OK to start, LEFT to exit");
  display.display();
}
