// =======================================================
//  PICCHIBOT - PART 10: "PICCHI-SAYS" GAME
// =======================================================
// This code adds a new surprise feature to the menu:
//
// 1.  "PICCHI-SAYS" GAME: A non-blocking "Simon Says"
//     memory game. PicchiBot plays an animation
//     sequence, and you must repeat it with the
//     buttons (UP, DOWN, LEFT, RIGHT).
// 2.  MENU UPDATE: "Picchi-Says" is added to the menu.
//
// All previous features (Pins, Shake, Morse) are included.
// =======================================================

// --- Core & Sensor Libraries ---
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MPU6050_light.h>
#include "FluxGarage_RoboEyes.h" // Your provided library
#include <math.h> 

// --- FIX: 'N' and 'E' MACRO CONFLICT ---
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

// --- CORRECTED PIN MAP ---
#define PIN_BUZZER 14
#define BTN_UP     26
#define BTN_DOWN   33
#define BTN_LEFT   25
#define BTN_RIGHT  27
#define BTN_OK     32

// Wi-Fi AP creds
const char *AP_SSID = "PicchiBot";
const char *AP_PASS = "amishudhutomar";

// OpenWeather
const char* OW_APIKEY = "9c5ed9a6da0fc29aac5db777c9638d1a";

// --- MORSE CODE ---
#define DOT_TIME 100

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
String globalUserName = "Shona";
String globalWeatherCond = "Loading...";
float globalWeatherTemp = -99;
bool globalIsConnected = false;

// --- UI State (Core 1 Only) ---
enum Screen { SCR_PET, SCR_MENU, SCR_WEATHER, SCR_TIMER_SETUP, SCR_TIMER_RUNNING, SCR_GAME_PICCHI_SAYS };
Screen currentScreen = SCR_PET;

int menuIndex = 0;
// *** NEW: Added "Picchi-Says" to menu ***
String menuItems[] = {"Weather", "Timer", "Picchi-Says", "Reboot", "Exit"};
int menuItemCount = 5;

unsigned long lastButtonPress = 0;
unsigned long debounceDelay = 200;

// --- Autonomous States ---
unsigned long lastInteractionTime = 0;
bool isSad = false;
bool isAsleep = false;
const unsigned long SAD_TIMEOUT = 120000UL;
const unsigned long SLEEP_TIMEOUT = 300000UL;

// --- Countdown Timer Globals ---
bool countdownRunning = false;
unsigned long countdownRemaining = 0; 
unsigned long countdownTick = 0;
unsigned long timerSetupSeconds = 30;

// --- Shake Detection Global ---
unsigned long violentShakeStartTime = 0;

// --- *** NEW: "Picchi-Says" Game Globals *** ---
#define MAX_GAME_LEVEL 20
int gameSequence[MAX_GAME_LEVEL];
int gameLevel = 0;
int playerStep = 0;
// Game states
enum GameState { GAME_START, GAME_BOT_TURN, GAME_PLAYER_TURN, GAME_OVER };
GameState currentGameState = GAME_START;
unsigned long gameStepTimer = 0;
int gameBotStep = 0;
// Define game moves (matches button directions)
#define MOVE_UP 0
#define MOVE_DOWN 1
#define MOVE_LEFT 2
#define MOVE_RIGHT 3
// ----------------------------------------------


// ----------------- HELPER SOUNDS -----------------
void beep(int freq, int duration) {
  tone(PIN_BUZZER, freq, duration);
  delay(duration + 10);
}

// =======================================================
//  MORSE CODE FUNCTIONS (RUNS ON CORE 0)
// =======================================================
// (This section is unchanged from Part 9)
const char* getMorse(char c) {
  switch (toupper(c)) {
    case 'A': return ".-"; case 'B': return "-..."; case 'C': return "-.-.";
    case 'D': return "-.."; case 'E': return "."; case 'F': return "..-.";
    case 'G': return "--."; case 'H': return "...."; case 'I': return "..";
    case 'J': return ".---"; case 'K': return "-.-"; case 'L': return ".-..";
    case 'M': return "--"; case 'N': return "-."; case 'O': return "---";
    case 'P': return ".--."; case 'Q': return "--.-"; case 'R': return ".-.";
    case 'S': return "..."; case 'T': return "-"; case 'U': return "..-";
    case 'V': return "...-"; case 'W': return ".--"; case 'X': return "-..-";
    case 'Y': return "-.--"; case 'Z': return "--..";
    case '0': return "-----"; case '1': return ".----"; case '2': return "..---";
    case '3': return "...--"; case '4': return "....-"; case '5': return ".....";
    case '6': return "-...."; case '7': return "--..."; case '8': return "---..";
    case '9': return "----."; case ' ': return " ";
    default: return "";
  }
}
void playMorse(String msg) {
  Serial.print("Morse: "); Serial.println(msg);
  for (int i = 0; i < msg.length(); i++) {
    const char* code = getMorse(msg[i]);
    if (strcmp(code, " ") == 0) {
      vTaskDelay((7 * DOT_TIME) / portTICK_PERIOD_MS);
      continue;
    }
    for (int j = 0; j < strlen(code); j++) {
      if (code[j] == '.') {
        tone(PIN_BUZZER, 1200, DOT_TIME);
        vTaskDelay((DOT_TIME) / portTICK_PERIOD_MS);
      } else {
        tone(PIN_BUZZER, 1200, 3 * DOT_TIME);
        vTaskDelay((3 * DOT_TIME) / portTICK_PERIOD_MS);
      }
      vTaskDelay((DOT_TIME) / portTICK_PERIOD_MS);
    }
    vTaskDelay((3 * DOT_TIME) / portTICK_PERIOD_MS);
  }
}

// =======================================================
//  CORE 0 - NETWORK TASK
// =======================================================
// (This section is unchanged from Part 9)
WebServer webServer(80);
DNSServer dnsServer;

void handleRoot() {
  String page = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>PicchiBot Setup</title></head><body>";
  page += "<h3>PicchiBot Setup</h3>";
  page += "<form method='POST' action='/save'>";
  page += "Your Name:<br><input name='name' value='Shona'><br>City:<br><input name='city' value='Dhaka'><br><hr>";
  page += "Home WiFi SSID:<br><input name='ssid'><br>Home WiFi PASS:<br><input name='pass'><br>";
  page += "<br><input type='submit' value='Save & Connect'></form>";
  page += "</body></html>";
  webServer.send(200, "text/html", page);
}
void handleSave() {
  String savedSSID = webServer.arg("ssid");
  String savedPASS = webServer.arg("pass");
  String savedCity = webServer.arg("city");
  String savedName = webServer.arg("name");
  if (savedSSID.length() > 0) {
    prefs.begin("picchi", false);
    prefs.putString("city", savedCity);
    prefs.putString("name", savedName);
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
    } else { Serial.println("JSON Parse Error"); }
  } else { Serial.print("HTTP Error: "); Serial.println(code); }
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
  String name = prefs.getString("name", "Shona");
  prefs.end();
  bool connected = false;
  if (ssid.length() > 0) {
    Serial.print("Trying to connect to: "); Serial.println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      vTaskDelay(500 / portTICK_PERIOD_MS); Serial.print("."); retries++;
    }
    if (WiFi.status() == WL_CONNECTED) connected = true;
  }
  portENTER_CRITICAL(&stateMux);
  if (connected) {
    globalIsConnected = true;
    globalIP = WiFi.localIP().toString();
    globalCity = city;
    globalUserName = name;
  } else {
    globalIsConnected = false;
    globalIP = "AP Mode";
    globalUserName = "Shona";
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
      vTaskDelay(300000 / portTICK_PERIOD_MS); // Wait 5 minutes
      fetchWeather();
      vTaskDelay(300000 / portTICK_PERIOD_MS); // Wait 5 minutes
      String nameCopy;
      portENTER_CRITICAL(&stateMux);
      nameCopy = globalUserName;
      portEXIT_CRITICAL(&stateMux);
      playMorse("I LOVE YOU " + nameCopy);
      vTaskDelay(300000 / portTICK_PERIOD_MS); // Wait 5 minutes
      playMorse("DRINK WATER");
      vTaskDelay(300000 / portTICK_PERIOD_MS); // Wait 5 minutes
      playMorse("LETS MAKE LOVE");
    }
  }
}
// =======================================================
//  END OF NETWORK TASK
// =======================================================


// --- Cyberpunk Boot Animation ---
void showBootIntro() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  randomSeed(analogRead(A0)); 

  for (int i=0;i<6;i++){
    display.clearDisplay();
    int x = random(-6,6);
    int y = random(-4,4);
    display.setCursor(8 + x, 6 + y);
    display.println("PicchiBot");
    display.setTextSize(1);
    display.setCursor(20 + x, 40 + y);
    display.println("by Lian");
    display.display();
    tone(PIN_BUZZER, 1000 + i*200, 60);
    delay(90);
  }
  display.clearDisplay();
  display.setTextSize(2); 
  display.setCursor(10, 20);
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

  // --- Manual Wi-Fi Reset Check (uses BTN_OK 32) ---
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
    prefs.clear();
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
  Serial.println("MPU Done.");

  // --- Play Your Boot Animation ---
  showBootIntro();
  
  eyes.open();

  // --- START THE NETWORK TASK (Core 0) ---
  xTaskCreatePinnedToCore(
      taskNetwork, "Network Task", 10000,
      NULL, 1, &hNetworkTask, 0
  );
  
  Serial.println("Setup finished. Starting UI loop on Core 1.");
  lastInteractionTime = millis();
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

  // --- Handle Autonomous States (Sad/Sleep) ---
  if (currentScreen == SCR_PET) {
    if (!isAsleep && millis() - lastInteractionTime > SLEEP_TIMEOUT) {
      isAsleep = true;
      eyes.anim_sleep();
    } else if (!isSad && millis() - lastInteractionTime > SAD_TIMEOUT) {
      isSad = true;
      eyes.anim_sad();
    }
  }

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
    case SCR_TIMER_SETUP:
      handleTimerSetupScreen(btnUp, btnDown, btnLeft, btnRight, btnOk);
      break;
    case SCR_TIMER_RUNNING:
      handleTimerRunningScreen(btnUp, btnDown, btnLeft, btnRight, btnOk);
      break;
    // --- *** NEW: Added Game Screen *** ---
    case SCR_GAME_PICCHI_SAYS:
      handleGameScreen(btnUp, btnDown, btnLeft, btnRight, btnOk);
      break;
  }
  
  // --- Global Countdown Timer Logic ---
  if (countdownRunning) {
    if (millis() - countdownTick >= 500) { 
      countdownRemaining = (countdownRemaining > 500) ? countdownRemaining - 500 : 0;
      countdownTick = millis();
      
      if (countdownRemaining == 0) {
        countdownRunning = false;
        if (currentScreen != SCR_TIMER_RUNNING) {
          beep(1200, 200);
          delay(100);
          beep(900, 200);
        }
      } else if (currentScreen != SCR_TIMER_RUNNING) {
        int beepFreq = 600 + (int)((1.0 - (float)countdownRemaining / 30000.0) * 1000);
        tone(PIN_BUZZER, beepFreq, 80);
      }
    }
  }

  // Only update eyes on the pet screen
  if (currentScreen == SCR_PET) {
    eyes.update();
  }
  
  vTaskDelay(10 / portTICK_PERIOD_MS); // Small yield
}

// =======================================================
//  UI SCREEN HANDLERS (Non-Blocking)
// =======================================================

// --- New "Shy" Animation ---
void anim_shy() {
  eyes.setMood(TIRED); // Use TIRED for "droopy" eyelids
  eyes.eyeLxNext = eyes.getScreenConstraint_X(); 
  eyes.eyeLyNext = eyes.getScreenConstraint_Y();
}

// --- Modified Pet Screen ---
void handlePetScreen(bool up, bool down, bool left, bool right, bool ok) {
  
  // --- 1. Check for Wake-up ---
  if (isAsleep || isSad) {
    if (up || down || left || right || ok) {
      isAsleep = false;
      isSad = false;
      lastInteractionTime = millis();
      eyes.anim_wake_dramatic();
      beep(1500, 100);
      return; 
    }
  }

  // --- 2. MPU Update (for both eyes and shake) ---
  mpu.update();

  // --- 3. Violent Shake Detection ---
  if (!isAsleep) {
    float ax_accel = mpu.getAccX();
    float ay_accel = mpu.getAccY();
    float az_accel = mpu.getAccZ();
    float accelMag = sqrt(ax_accel * ax_accel + ay_accel * ay_accel + az_accel * az_accel);

    const float SHAKE_THRESHOLD = 2.5; 
    const unsigned long SHAKE_DURATION = 5000; // 5 seconds

    if (accelMag > SHAKE_THRESHOLD) {
      if (violentShakeStartTime == 0) {
        violentShakeStartTime = millis();
        Serial.println("Violent shake detected, starting 5s timer...");
      } else {
        if (millis() - violentShakeStartTime > SHAKE_DURATION) {
          Serial.println("Shake duration met! Starting countdown.");
          
          countdownRemaining = 30000; // 30-second prank
          countdownTick = millis();
          countdownRunning = true;
          currentScreen = SCR_TIMER_RUNNING; 
          
          beep(2000, 100); delay(50); beep(2000, 100);
          
          violentShakeStartTime = 0;
          lastInteractionTime = millis();
          return;
        }
      }
    } else {
      if (violentShakeStartTime != 0) {
         Serial.println("Shake stopped, resetting 5s timer.");
         violentShakeStartTime = 0; // Reset
      }
    }
  }
  
  // --- 4. MPU Eye Control (if not asleep/sad) ---
  if (!isAsleep && !isSad) {
    float ax = mpu.getAngleX();
    float ay = mpu.getAngleY(); 
    int posX = map((int)ay, -15, 15, 0, eyes.getScreenConstraint_X());
    int posY = map((int)ax, -15, 15, 0, eyes.getScreenConstraint_Y());
    eyes.eyeLxNext = constrain(posX, 0, eyes.getScreenConstraint_X());
    eyes.eyeLyNext = constrain(posY, 0, eyes.getScreenConstraint_Y());
  }
  
  // --- 5. Button Inputs (using NEW pins) ---
  if ((millis() - lastButtonPress) > debounceDelay) {
    if (left) { // BTN_LEFT (25) -> "Shy"
      anim_shy();
      beep(1500,80);
      lastInteractionTime = millis();
      lastButtonPress = millis();
    }
    else if (right) { // BTN_RIGHT (27) -> "Dizzy"
      eyes.anim_confused(); // Use confused for "dizzy"
      beep(1600, 80);
      lastInteractionTime = millis();
      lastButtonPress = millis();
    }
    else if (up) { // BTN_UP (26) -> "Shocked"
      eyes.anim_shocked();
      beep(1800, 80);
      lastInteractionTime = millis();
      lastButtonPress = millis();
    }
    else if (down) { // BTN_DOWN (33) -> "Sad"
      eyes.anim_sad();
      beep(400, 100);
      lastInteractionTime = millis();
      lastButtonPress = millis();
    }
    else if (ok) { // BTN_OK (32) -> "Menu"
      beep(800, 50);
      currentScreen = SCR_MENU;
      menuIndex = 0;
      lastInteractionTime = millis();
      lastButtonPress = millis();
      
      // *** NEW: Reset game state when entering menu ***
      currentGameState = GAME_START;
    }
  }

  // --- 6. Draw WiFi/IP Status ---
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
  if ((millis() - lastButtonPress) > debounceDelay) {
    if (down) { // BTN_DOWN (33)
      menuIndex = (menuIndex + 1) % menuItemCount;
      beep(1000, 50); lastButtonPress = millis();
    }
    if (up) { // BTN_UP (26)
      menuIndex = (menuIndex - 1 + menuItemCount) % menuItemCount;
      beep(1000, 50); lastButtonPress = millis();
    }
    if (ok) { // BTN_OK (32)
      beep(1200, 80);
      lastButtonPress = millis();
      if (menuItems[menuIndex] == "Weather") {
        currentScreen = SCR_WEATHER;
      }
      if (menuItems[menuIndex] == "Timer") {
        currentScreen = SCR_TIMER_SETUP; 
        timerSetupSeconds = 30;
      }
      if (menuItems[menuIndex] == "Picchi-Says") {
        currentScreen = SCR_GAME_PICCHI_SAYS;
        currentGameState = GAME_START; // Start the game
      }
      if (menuItems[menuIndex] == "Reboot") {
        display.clearDisplay(); display.setCursor(10,20);
        display.println("Rebooting..."); display.display();
        delay(1000); ESP.restart();
      }
      if (menuItems[menuIndex] == "Exit") {
        currentScreen = SCR_PET;
      }
    }
    if (left) { // BTN_LEFT (25) - Use as "Back"
      beep(800, 50);
      currentScreen = SCR_PET;
      lastButtonPress = millis();
    }
  }
  
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

// --- Timer Setup Screen ---
void handleTimerSetupScreen(bool up, bool down, bool left, bool right, bool ok) {
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
      countdownRemaining = timerSetupSeconds * 1000;
      countdownTick = millis();
      countdownRunning = true;
      currentScreen = SCR_TIMER_RUNNING; // Go to running screen
      beep(1400, 100);
      lastButtonPress = millis();
    }
    if (left) { // BTN_LEFT (25)
      beep(800, 50);
      currentScreen = SCR_MENU; // Go back to menu
      lastButtonPress = millis();
    }
  }

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

// --- Full-Screen Running Timer ---
void handleTimerRunningScreen(bool up, bool down, bool left, bool right, bool ok) {
  
  if ((millis() - lastButtonPress) > debounceDelay) {
    if (left || ok) { // Cancel with LEFT (25) or OK (32)
      countdownRunning = false;
      beep(800, 50);
      currentScreen = SCR_MENU; // Go back to menu
      lastButtonPress = millis();
      return;
    }
  }
  
  if (!countdownRunning) {
    // Timer finished!
    display.clearDisplay();
    display.setTextSize(3);
    display.setCursor(25, 20);
    display.print("BOOM!");
    display.display();
    
    beep(1200, 200);
    delay(100);
    beep(900, 200);
    delay(1000);
    
    currentScreen = SCR_PET;
    return;
  }

  // --- Draw Running Timer ---
  display.clearDisplay();
  display.setTextColor(WHITE);
  
  int seconds = (countdownRemaining / 1000) + 1;
  display.setTextSize(3);
  
  if (seconds < 10) display.setCursor(55, 20); // x, y
  else if (seconds < 100) display.setCursor(35, 20);
  else display.setCursor(15, 20);
  
  display.print(seconds);
  
  display.setTextSize(1);
  display.setCursor(15, 55);
  display.print("Press LEFT or OK to cancel");
  display.display();

  // Play beeps on this screen
  if (countdownRemaining < 5000) { // Last 5 seconds
     if (millis() - countdownTick >= 250) {
        tone(PIN_BUZZER, 1500, 100);
     }
  } else {
     if (millis() - countdownTick >= 500) {
       tone(PIN_BUZZER, 1200, 80);
     }
  }
}

// --- *** NEW: "Picchi-Says" Game Screen *** ---
void playMoveAnimation(int move) {
  switch(move) {
    case MOVE_UP: // Shocked
      eyes.anim_shocked(); beep(1800, 80); break;
    case MOVE_DOWN: // Sad
      eyes.anim_sad(); beep(400, 100); break;
    case MOVE_LEFT: // Shy
      anim_shy(); beep(1500, 80); break;
    case MOVE_RIGHT: // Dizzy
      eyes.anim_confused(); beep(1600, 80); break;
  }
  // We need to pump the eye library to show the anim
  for(int i=0; i<20; i++) {
    eyes.update();
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
  eyes.open(); // Reset eyes to open
  for(int i=0; i<10; i++) {
    eyes.update();
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

void handleGameScreen(bool up, bool down, bool left, bool right, bool ok) {
  display.clearDisplay();
  display.setTextColor(WHITE);

  // --- Main Game State Machine ---
  switch(currentGameState) {
    
    case GAME_START:
      display.setTextSize(1);
      display.setCursor(20, 10);
      display.println("PICCHI-SAYS!");
      display.setCursor(20, 30);
      display.println("Press OK to Start");
      display.setCursor(20, 40);
      display.println("LEFT to Exit");
      display.display();

      if ((millis() - lastButtonPress) > debounceDelay) {
        if (ok) {
          gameLevel = 0; // Reset level
          playerStep = 0;
          gameBotStep = 0;
          currentGameState = GAME_BOT_TURN;
          lastButtonPress = millis();
        }
        if (left) {
          currentScreen = SCR_MENU;
          lastButtonPress = millis();
        }
      }
      break;

    case GAME_BOT_TURN:
      // Show "Watch Me..."
      display.setTextSize(1);
      display.setCursor(30, 20);
      display.println("Watch me...");
      display.setTextSize(2);
      display.setCursor(55, 40);
      display.print(gameLevel + 1); // Show next level
      display.display();
      delay(1500); // Pause to let user read

      // Add a new random move to the sequence
      if (gameLevel < MAX_GAME_LEVEL) {
        gameSequence[gameLevel] = random(0, 4); // 0=UP, 1=DOWN, 2=LEFT, 3=RIGHT
      }
      gameLevel++;

      // Play the whole sequence for the bot
      for (int i = 0; i < gameLevel; i++) {
        playMoveAnimation(gameSequence[i]);
        delay(300); // Pause between moves
      }
      
      // Now it's the player's turn
      currentGameState = GAME_PLAYER_TURN;
      playerStep = 0; // Reset player's step
      gameStepTimer = millis(); // Start the turn timer
      break;

    case GAME_PLAYER_TURN:
      display.setTextSize(1);
      display.setCursor(30, 20);
      display.println("Your Turn!");
      display.setTextSize(2);
      display.setCursor(55, 40);
      display.print(gameLevel); // Show current level
      display.display();

      // Check for player input
      if ((millis() - lastButtonPress) > debounceDelay) {
        int playerMove = -1;
        
        if (up)    { playerMove = MOVE_UP; }
        if (down)  { playerMove = MOVE_DOWN; }
        if (left)  { playerMove = MOVE_LEFT; }
        if (right) { playerMove = MOVE_RIGHT; }
        
        if (playerMove != -1) {
          lastButtonPress = millis();
          playMoveAnimation(playerMove); // Show the player's move
          
          if (playerMove == gameSequence[playerStep]) {
            // --- CORRECT ---
            playerStep++;
            if (playerStep == gameLevel) {
              // --- LEVEL COMPLETE ---
              beep(1500, 100); delay(50); beep(2000, 100);
              currentGameState = GAME_BOT_TURN; // Go to next level
              delay(1000);
            }
          } else {
            // --- WRONG ---
            currentGameState = GAME_OVER;
          }
        }
      }
      
      // Exit if player takes too long (e.g., 5 seconds per step)
      if (millis() - gameStepTimer > (5000 + (gameLevel * 500))) {
        currentGameState = GAME_OVER;
      }
      break;

    case GAME_OVER:
      display.setTextSize(2);
      display.setCursor(15, 10);
      display.println("GAME OVER");
      display.setTextSize(1);
      display.setCursor(30, 40);
      display.print("Score: ");
      display.print(gameLevel - 1);
      display.display();
      
      beep(800, 150); delay(50); beep(600, 150); delay(50); beep(400, 200);
      delay(2500);
      
      currentScreen = SCR_MENU; // Go back to menu
      currentGameState = GAME_START;
      break;
  }
}
