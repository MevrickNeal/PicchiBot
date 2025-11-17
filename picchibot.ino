// PicchiBot.ino
// Single-file: Arduino IDE ready
// Requires: FluxGarage_RoboEyes.h (you uploaded), Adafruit SSD1306, MPU6050_light, Arduino_JSON

#include <Wire.h>
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "FluxGarage_RoboEyes.h"
#include <MPU6050_light.h>

// ----------------- CONFIG -----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

// Pins
#define PIN_BUZZER 14        // as requested
#define BTN_UP    32
#define BTN_DOWN  33
#define BTN_LEFT  25
#define BTN_RIGHT 26
#define BTN_OK    27

// Wi-Fi AP creds
const char *AP_SSID = "PicchiBot";
const char *AP_PASS = "tumiamar";

// OpenWeather
const char* OW_APIKEY = "9c5ed9a6da0fc29aac5db777c9638d1a";
String defaultCity = "Dhaka";
String defaultCountryCode = "BD";

// Menu & timing
const unsigned long IDLE_ANIM_TIMEOUT = 40000UL;   // 40s for cute idle
const unsigned long SAD_TIMEOUT = 120000UL;       // 2 min -> sad
const unsigned long SLEEP_TIMEOUT = 180000UL;     // 3 min -> sleep
const unsigned long DRINK_REMINDER_INTERVAL = 1800000UL; // 30 min

// NOTE frequencies (pitches.h style)
#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523

// Melody from your message (note constants)
int melody[] = {
  NOTE_C4, NOTE_C4,
  NOTE_D4, NOTE_C4, NOTE_F4,
  NOTE_E4, NOTE_C4, NOTE_C4,
  NOTE_D4, NOTE_C4, NOTE_G4,
  NOTE_F4, NOTE_C4, NOTE_C4,

  NOTE_C5, NOTE_A4, NOTE_F4,
  NOTE_E4, NOTE_D4, NOTE_AS4, NOTE_AS4,
  NOTE_A4, NOTE_F4, NOTE_G4,
  NOTE_F4
};

int durations[] = {
  4, 8,
  4, 4, 4,
  2, 4, 8,
  4, 4, 4,
  2, 4, 8,

  4, 4, 4,
  4, 4, 4, 8,
  4, 4, 4,
  2
};

const int MELODY_LEN = sizeof(melody)/sizeof(int);

// ----------------- GLOBALS -----------------
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
RoboEyes<Adafruit_SSD1306> eyes(display);

MPU6050 mpu(Wire);
Preferences prefs;

unsigned long lastInteraction = 0;
unsigned long lastIdleAnim = 0;
unsigned long lastDrinkReminder = 0;
bool asleep = false;
bool sad = false;

// WiFi server for simple captive portal
WebServer webServer(80);
DNSServer dnsServer;

// User/profile data (persisted)
String userName = "";
String userCity = "Dhaka";
String userCountry = "BD";
String savedSSID = "";
String savedPASS = "";

// Key mapping (stored in prefs) - default map: UP=UP, DOWN=DOWN, LEFT=LEFT, RIGHT=RIGHT
enum Action { ACT_UP=1, ACT_DOWN=2, ACT_LEFT=3, ACT_RIGHT=4 };
int mapUp = ACT_UP, mapDown = ACT_DOWN, mapLeft = ACT_LEFT, mapRight = ACT_RIGHT;

// button state helpers
unsigned long leftPressFirst = 0;
int leftPressCount = 0;

// boot flags
bool bootDone = false;

// menu variables
bool inMenu = false;
int menuIndex = 0;
String menuItems[4] = {"Time", "Weather", "KeyMap", "Timer"};

// countdown timer friend
unsigned long countdownRemaining = 0; // ms
unsigned long countdownTick = 0;
bool countdownRunning = false;

// WiFi/AP ephemeral state
bool inAPMode = false;

// --------- FOR DEBOUNCE ----------
unsigned long lastDebounce[6] = {0,0,0,0,0,0};
const unsigned long DEBOUNCE_MS = 180;

// --------- FOR SHAKE DETECTION ----------
float lastAx=0, lastAy=0;
unsigned long lastShake = 0;

// ----------------- HELPER SOUNDS -----------------
void beep(int freq, int dur) {
  if (dur <= 0 || freq <= 0) return;
  tone(PIN_BUZZER, freq, dur);
}

void chirp() { beep(1800,50); }
void happyChirp() { beep(1500,80); delay(90); beep(2200,60); }
void tinyPurr() {
  // short non-blocking-ish purr sequence (uses small delays but short)
  beep(700,40); delay(50);
  beep(900,40); delay(50);
  beep(600,40);
}
void playMelodyBlocking() {
  // tempo: quarter note = 500ms (120bpm) -> you can adjust tempo
  int tempo = 120;
  int whole = (60000 / tempo) * 4; // ms
  for (int i = 0; i < MELODY_LEN; ++i) {
    int note = melody[i];
    int durationType = durations[i]; // 4=quarter, 8=eighth, etc.
    int noteDuration = whole / durationType;
    if (note > 0) tone(PIN_BUZZER, note, noteDuration * 0.9);
    delay(noteDuration);
  }
}

// ----------------- DISPLAY HELPERS -----------------
void showBootIntro() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);

  // glitch/cyber effect - simple
  for (int i=0;i<6;i++){
    display.clearDisplay();
    int x = random(-6,6);
    int y = random(-4,4);
    display.setCursor(8 + x, 6 + y);
    display.println("Picchi Bot");
    display.setTextSize(1);
    display.setCursor(20 + x, 40 + y);
    display.println("by Lian Mollick");
    display.display();
    beep(1000 + i*200, 60);
    delay(90);
  }
  // final stable
  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(4,6);
  display.println("PicchiBot");
  display.setTextSize(1);
  display.setCursor(30,46);
  display.println("by Lian Mollick");
  display.display();
  delay(800);
}

// small helper to show text then return
void showTempOnScreen(float tC) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10,16);
  display.print(String(tC,1));
  display.print(" C");
  display.display();
  delay(1800);
}

// Show Bangla water reminder text (transliterated or Bengali script)
void showDrinkReminder() {
  display.clearDisplay();
  // Bengali text - ensure font supports; SSD1306 likely doesn't support Bengali,
  // so we use Bangla transliteration (Roman) or short English if font issue:
  // The user asked: write this part in bangla "Please Pani Khao Shona"
  // We'll display in transliterated Bangla:
  display.setTextSize(1);
  display.setCursor(5,20);
  display.println("Please Pani Khao Shona"); // transliteration
  display.setCursor(5,34);
  display.println("পানি খাও শোনা"); // attempt in Bengali - may not render on SSD1306
  display.display();
  delay(2500);
}

// -------------- SIMPLE UI / MENU --------------
void enterMenu() {
  inMenu = true;
  menuIndex = 0;
  lastInteraction = millis();
}

void exitMenu() {
  inMenu = false;
  lastInteraction = millis();
  display.clearDisplay();
  display.display();
}

// key mapping editor - cycle mapping for a button
void cycleMapFor(int which) {
  // which: 0=UP mapping, 1=DOWN, 2=LEFT, 3=RIGHT
  // cycle between ACT_UP..ACT_RIGHT
  int *target = nullptr;
  if (which==0) target = &mapUp;
  if (which==1) target = &mapDown;
  if (which==2) target = &mapLeft;
  if (which==3) target = &mapRight;
  if (!target) return;
  *target = ((*target) % 4) + 1;
}

// countdown bomb joke
void startCountdown(unsigned long ms) {
  countdownRunning = true;
  countdownRemaining = ms;
  countdownTick = millis();
}

// ----------------- WIFI SETUP PORTAL (non-async) -----------------
String processorHtml(String var) {
  // not used complexly now
  return String("");
}

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
  if (webServer.hasArg("ssid")) {
    savedSSID = webServer.arg("ssid");
    savedPASS = webServer.arg("pass");
    if (webServer.hasArg("name")) userName = webServer.arg("name");
    if (webServer.hasArg("city")) userCity = webServer.arg("city");
    // persist
    prefs.begin("picchi", false);
    prefs.putString("name", userName);
    prefs.putString("city", userCity);
    prefs.putString("ssid", savedSSID);
    prefs.putString("pass", savedPASS);
    prefs.end();

    String resp = "<html><body><h3>Saved. Rebooting to connect...</h3></body></html>";
    webServer.send(200, "text/html", resp);
    delay(1000);
    ESP.restart();
  } else {
    webServer.send(400, "text/plain", "missing");
  }
}

// start AP mode and webserver
void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP IP: "); Serial.println(apIP);

  // start DNS redirect for captive-ish behavior
  dnsServer.start(53, "*", apIP);

  webServer.on("/", handleRoot);
  webServer.on("/save", HTTP_POST, handleSave);
  webServer.begin();
  inAPMode = true;
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("AP Mode: PicchiBot");
  display.display();
}

// try to connect to saved WiFi
bool tryConnectSavedWiFi() {
  prefs.begin("picchi", true);
  userName = prefs.getString("name", "");
  userCity = prefs.getString("city", "Dhaka");
  savedSSID = prefs.getString("ssid", "");
  savedPASS = prefs.getString("pass", "");
  prefs.end();

  if (savedSSID.length() == 0) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(savedSSID.c_str(), savedPASS.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
  }
  if (WiFi.status() == WL_CONNECTED) {
    inAPMode = false;
    return true;
  }
  return false;
}

// fetch weather via OpenWeatherMap - returns JSON string
String fetchWeather(String city, String country, const char* key) {
  if (WiFi.status() != WL_CONNECTED) return String("");
  String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + country + "&APPID=" + String(key) + "&units=metric";
  HTTPClient http;
  WiFiClient client;
  http.begin(client, serverPath);
  int code = http.GET();
  String payload = "";
  if (code > 0) {
    payload = http.getString();
  }
  http.end();
  return payload;
}

// parse weather and show on screen (simple)
void showWeather() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  if (WiFi.status() != WL_CONNECTED) {
    display.println("No WiFi");
    display.display();
    delay(1000);
    return;
  }
  String json = fetchWeather(userCity, userCountry, OW_APIKEY);
  if (json.length() == 0) {
    display.println("Fail to fetch");
    display.display();
    delay(1000);
    return;
  }
  JSONVar o = JSON.parse(json);
  if (JSON.typeof(o) == "undefined") {
    display.println("Parse error");
    display.display();
    delay(1000);
    return;
  }
  float temp = 0;
  float hum = 0;
  if (o["main"]["temp"] != NULL) temp = (double)o["main"]["temp"];
  if (o["main"]["humidity"] != NULL) hum = (double)o["main"]["humidity"];
  display.setTextSize(2);
  display.setCursor(0,8);
  display.print(String(temp,1));
  display.println(" C");
  display.setTextSize(1);
  display.setCursor(0,46);
  display.print("Hum: "); display.print(String(hum,0)); display.println("%");
  display.display();
  delay(2500);
}

// ----------------- SETUP & LOOP -----------------
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));
  pinMode(PIN_BUZZER, OUTPUT);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);

  // display init
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 failed");
    for(;;);
  }
  display.clearDisplay();

  // RoboEyes init
  eyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 50);
  eyes.setAutoblinker(true, 2, 4);
  eyes.setIdleMode(true, 3, 4);
  eyes.open();

  // MPU init
  mpu.begin();
  mpu.calcOffsets(true, true);

  // prefs
  prefs.begin("picchi", true);
  userName = prefs.getString("name", "");
  userCity = prefs.getString("city", "Dhaka");
  userCountry = prefs.getString("country", "BD");
  savedSSID = prefs.getString("ssid", "");
  savedPASS = prefs.getString("pass", "");
  mapUp = prefs.getInt("mapUp", ACT_UP);
  mapDown = prefs.getInt("mapDown", ACT_DOWN);
  mapLeft = prefs.getInt("mapLeft", ACT_LEFT);
  mapRight = prefs.getInt("mapRight", ACT_RIGHT);
  prefs.end();

  // boot intro
  showBootIntro();
  // Try saved wifi first
  if (!tryConnectSavedWiFi()) {
    startAPMode();
  } else {
    // show WiFi IP small icon/text
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println(WiFi.localIP().toString());
    display.display();
    delay(800);
  }

  lastInteraction = millis();
  lastDrinkReminder = millis();
  bootDone = true;
}

// read a button with debounce
bool readBtn(int pin, int idx) {
  int val = digitalRead(pin);
  if (val == LOW) {
    if (millis() - lastDebounce[idx] > DEBOUNCE_MS) {
      lastDebounce[idx] = millis();
      return true;
    }
  }
  return false;
}

// show dramatic wakeup
void dramaticWakeup() {
  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(6,8);
  display.println("PicchiBot");
  display.setTextSize(1);
  display.setCursor(30,46);
  display.println("by Lian Mollick");
  display.display();
  // dramatic beep sequence
  beep(800,100); delay(80);
  beep(1200,150); delay(120);
  beep(1600,180);
  eyes.open();
  delay(600);
}

// handle mapped actions
void doAction(int act) {
  switch(act) {
    case ACT_UP:
      chirp(); eyes.anim_laugh();
      break;
    case ACT_DOWN:
      chirp(); eyes.blink();
      break;
    case ACT_LEFT:
      // pet
      tinyPurr();
      eyes.close(); delay(120); eyes.open();
      break;
    case ACT_RIGHT:
      // show temp
      showTempOnScreen(mpu.getTemp());
      break;
  }
}

// small helper to run menu UI
void menuLoop() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Menu:");
  for (int i=0;i<4;i++) {
    if (i==menuIndex) {
      display.setTextSize(1);
      display.drawRect(0, 12 + i*12, 128, 12, WHITE);
      display.setCursor(4, 14 + i*12);
      display.println(menuItems[i]);
    } else {
      display.setCursor(6, 14 + i*12);
      display.println(menuItems[i]);
    }
  }
  display.display();

  // navigate with physical buttons while inMenu
  unsigned long menuStart = millis();
  while (inMenu) {
    if (readBtn(BTN_UP,0)) { menuIndex = (menuIndex - 1 + 4) % 4; delay(150); menuStart = millis(); menuLoop(); }
    if (readBtn(BTN_DOWN,1)) { menuIndex = (menuIndex + 1) % 4; delay(150); menuStart = millis(); menuLoop(); }
    if (readBtn(BTN_LEFT,2)) { // back
      exitMenu();
      return;
    }
    if (readBtn(BTN_OK,5)) {
      // select
      if (menuIndex == 0) {
        // Time
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(4, 20);
        // show ms uptime as hh:mm approximate
        unsigned long s = millis()/1000;
        int hh = (s/3600)%24;
        int mm = (s/60)%60;
        char buf[16];
        sprintf(buf, "%02d:%02d", hh, mm);
        display.println(buf);
        display.display();
        delay(1200);
      } else if (menuIndex == 1) {
        showWeather();
      } else if (menuIndex == 2) {
        // Key mapping UI
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);
        display.println("Press which button to remap:");
        display.println("UP,DOWN,LEFT,RIGHT -> cycles action");
        display.display();
        delay(800);
        bool mapping = true;
        while (mapping) {
          if (readBtn(BTN_UP,0)) { cycleMapFor(0); prefs.begin("picchi", false); prefs.putInt("mapUp", mapUp); prefs.end(); menuLoop(); }
          if (readBtn(BTN_DOWN,1)) { cycleMapFor(1); prefs.begin("picchi", false); prefs.putInt("mapDown", mapDown); prefs.end(); menuLoop(); }
          if (readBtn(BTN_LEFT,2)) { cycleMapFor(2); prefs.begin("picchi", false); prefs.putInt("mapLeft", mapLeft); prefs.end(); menuLoop(); }
          if (readBtn(BTN_RIGHT,3)) { cycleMapFor(3); prefs.begin("picchi", false); prefs.putInt("mapRight", mapRight); prefs.end(); menuLoop(); }
          if (readBtn(BTN_OK,5)) { mapping=false; delay(300); menuLoop(); }
          if (millis() - menuStart > 30000) { mapping=false; exitMenu(); return; }
        }
      } else if (menuIndex == 3) {
        // Countdown timer
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);
        display.println("Enter seconds w/ UP/DOWN then OK");
        display.display();
        unsigned long secs = 10;
        while (true) {
          if (readBtn(BTN_UP,0)) { secs = min(secs+5, 3600UL); display.setCursor(0,20); display.println(String(secs) + " s    "); display.display(); delay(180); }
          if (readBtn(BTN_DOWN,1)) { secs = max(5UL, secs>5? secs-5 : 5UL); display.setCursor(0,20); display.println(String(secs) + " s    "); display.display(); delay(180); }
          if (readBtn(BTN_OK,5)) { startCountdown(secs*1000UL); break; }
          if (millis() - menuStart > 30000) break;
        }
      }
    }
    // auto-exit menu after 60s idle
    if (millis() - menuStart > 60000) { exitMenu(); return; }
    delay(20);
  }
}

// ------------------ MAIN LOOP ------------------
void loop() {
  // Handle DNS redirect for AP
  if (inAPMode) dnsServer.processNextRequest();
  if (inAPMode) webServer.handleClient();

  // Poll physical buttons (map them via saved mapping)
  // Note: maps let user swap what physical buttons do (UP/DOWN/LEFT/RIGHT)
  // read raw states
  bool rawUp = readBtn(BTN_UP,0);
  bool rawDown = readBtn(BTN_DOWN,1);
  bool rawLeft = readBtn(BTN_LEFT,2);
  bool rawRight = readBtn(BTN_RIGHT,3);
  bool rawOk = readBtn(BTN_OK,5);

  // If OK long-press (2s) -> enter menu
  static unsigned long okPressedAt = 0;
  if (digitalRead(BTN_OK)==LOW) {
    if (okPressedAt==0) okPressedAt = millis();
    if (millis() - okPressedAt > 2000 && !inMenu) {
      enterMenu();
      menuLoop(); // blocking menu loop until exit
      okPressedAt = 0;
      lastInteraction = millis();
    }
  } else okPressedAt = 0;

  // if any raw button pressed -> interpret mapped action
  if (rawUp) { lastInteraction = millis(); doAction(mapUp); }
  if (rawDown) { lastInteraction = millis(); doAction(mapDown); }
  if (rawLeft) {
    lastInteraction = millis();
    // handle triple press detection
    if (leftPressFirst == 0) leftPressFirst = millis(), leftPressCount = 1;
    else {
      if (millis() - leftPressFirst < 2000) leftPressCount++;
      else { leftPressFirst = millis(); leftPressCount = 1; }
    }
    // single left action
    tinyPurr();
    eyes.close(); delay(120); eyes.open();
    if (leftPressCount >= 3) {
      // play melody
      playMelodyBlocking();
      leftPressCount = 0;
      leftPressFirst = 0;
    }
  }
  if (rawRight) {
    lastInteraction = millis();
    // show weather/temp
    showWeather();
  }

  // Idle animations after 40s
  if (millis() - lastInteraction > IDLE_ANIM_TIMEOUT && millis() - lastIdleAnim > 3000) {
    eyes.anim_laugh();
    lastIdleAnim = millis();
  }

  // sad after 2 minutes
  if (!sad && millis() - lastInteraction > SAD_TIMEOUT) {
    sad = true;
    eyes.anim_sad();
    beep(400,150);
  }

  // sleep after 3 minutes
  if (!asleep && millis() - lastInteraction > SLEEP_TIMEOUT) {
    asleep = true;
    eyes.close();
  }

  // wake up if asleep and button pressed
  if (asleep && (rawUp || rawDown || rawLeft || rawRight || rawOk)) {
    asleep = false;
    dramaticWakeup();
    lastInteraction = millis();
    sad = false;
  }

  // drink reminder
  if (millis() - lastDrinkReminder > DRINK_REMINDER_INTERVAL) {
    lastDrinkReminder = millis();
    showDrinkReminder();
  }

  // countdown tick
  if (countdownRunning) {
    if (millis() - countdownTick >= 500) {
      countdownRemaining = (countdownRemaining > 500) ? countdownRemaining - 500 : 0;
      countdownTick = millis();
      // bomb beep: faster as nearing zero
      int beepFreq = 600 + (int)((1.0 - (float)countdownRemaining / 60000.0) * 1000);
      beep(beepFreq, 80);
      if (countdownRemaining == 0) {
        countdownRunning = false;
        // big "boom" beep sequence (joke)
        beep(1200, 200); delay(100);
        beep(900, 200);
      }
    }
  }

  // MPU update & eye movement + shake detect
  mpu.update();
  float ax = mpu.getAngleX();
  float ay = mpu.getAngleY();

  // map tilt into eyes' constraints
  int posX = map((int)ay, -45, 45, 0, eyes.getScreenConstraint_X());
  int posY = map((int)ax, -45, 45, 0, eyes.getScreenConstraint_Y());
  eyes.eyeLxNext = posX;
  eyes.eyeLyNext = posY;

  // shake detect - quick jerk
  if (abs(ax - lastAx) > 20 || abs(ay - lastAy) > 20) {
    if (millis() - lastShake > 1200) {
      lastShake = millis();
      // scared reaction
      beep(2000,80); delay(80); beep(2300,80);
      eyes.anim_shocked();
    }
  }
  lastAx = ax; lastAy = ay;

  // show small WiFi icon when connected
  if (WiFi.status() == WL_CONNECTED && !inAPMode) {
    display.setTextSize(1);
    display.setCursor(110,0);
    display.print("WiFi");
    display.display();
  }

  // eyes update (render)
  if (!asleep) eyes.update();
  else { eyes.drawEyes(); } // closed when sleeping

  // small yield
  delay(12);
}
