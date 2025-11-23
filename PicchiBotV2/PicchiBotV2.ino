/***************************************************
  PicchiBot v3.5 - The Ultimate Companion
  
  HARDWARE PIN MAP (ESP32):
  - OLED SDA: GPIO 21
  - OLED SCL: GPIO 22
  - MPU6050 SDA: GPIO 21
  - MPU6050 SCL: GPIO 22
  - BUZZER:   GPIO 14
  - BTN_UP:   GPIO 26
  - BTN_DOWN: GPIO 33
  - BTN_LEFT: GPIO 25
  - BTN_RIGHT:GPIO 27
  - BTN_OK:   GPIO 32

  FEATURES:
  - Gyro-Controlled Eyes (Tilt to look)
  - WiFi Manager (Connect to "PicchiSetup" to configure)
  - User Profile (Asks for your name)
  - 3 Games: Dino Run, Flappy Bot, Brick Breaker
  - Real-time Clock & Weather
****************************************************/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <ArduinoJson.h>
#include <WiFiManager.h> 
#include <Preferences.h>

// --- 1. ASSET INCLUDES ---
#include "Normal.h"
#include "Happy.h"
#include "Sad.h"
#include "Sleepy.h"
#include "Angry.h"
#include "Furious.h"
#include "Worried.h"
#include "Scared.h"
#include "Surprised.h"
#include "Disoriented.h"
#include "Despair.h"
#include "Alert.h"
#include "Bored.h"
#include "Excited.h"
#include "Focused.h"
#include "Look_Up.h"
#include "Look_Down.h"
#include "Look_Left.h"
#include "Look_Right.h"
#include "Blink.h"
#include "Blink_Up.h"
#include "Blink_Down.h"
#include "Wink_Left.h"
#include "Wink_Right.h"

// --- 2. CONFIGURATION ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C

#define PIN_BUZZER 14
#define BTN_UP     26
#define BTN_DOWN   33
#define BTN_LEFT   25
#define BTN_RIGHT  27
#define BTN_OK     32

const char* OW_APIKEY = "9c5ed9a6da0fc29aac5db777c9638d1a"; // OpenWeatherMap Key

// --- 3. GLOBALS ---
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_MPU6050 mpu;
Preferences preferences;

char userName[32] = "Friend"; // Default name variable

// State Machine
enum AppState { FACE, MENU, CLOCK, WEATHER, GAME_DINO, GAME_FLAPPY, GAME_BRICK, TIMER, JOKE };
AppState currentState = FACE;

// Menu
const char* menuItems[] = { "Face Mode", "Dino Run", "Flappy Bot", "Brick Breaker", "Clock", "Weather", "Timer", "Joke", "Reset WiFi" };
int menuIndex = 0;
const int menuLen = 9;

// Eye Logic
const unsigned char* currentEye = normal;
unsigned long lastBlink = 0;
unsigned long lastSaccade = 0;
float ax, ay, az; 

// Data Holders
String tempStr = "--";
String weatherDesc = "Loading...";
String randomMsg = "Press OK...";

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  
  // Init Pins
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);

  // Init I2C
  Wire.begin();

  // Init Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 Allocation Failed"));
    for(;;); // Loop forever if display fails
  }
  display.clearDisplay();
  display.display();

  // Init MPU6050 (Gyro)
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found - Gyro features disabled");
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  // Init Storage & Load Name
  preferences.begin("user-config", false);
  String savedName = preferences.getString("name", "Friend");
  savedName.toCharArray(userName, 32);

  // Sound Check
  tone(PIN_BUZZER, 2000, 100); delay(100);
  
  // Start WiFi Manager
  runWiFiManager();
}

void runWiFiManager() {
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(WHITE);
  
  WiFiManager wm;
  
  // Create custom text box for Name
  WiFiManagerParameter custom_name("name", "Enter Your Name", userName, 32);
  wm.addParameter(&custom_name);

  // Callback to show info on screen if connection fails/needs setup
  wm.setAPCallback([](WiFiManager *myWiFiManager) {
    display.clearDisplay();
    display.setCursor(0, 0); display.println("SETUP REQUIRED!");
    display.setCursor(0, 20); display.println("Connect WiFi to:");
    display.setTextSize(2); display.println("PicchiSetup");
    display.setTextSize(1); display.setCursor(0, 50); display.println("IP: 192.168.4.1");
    display.display();
    tone(PIN_BUZZER, 1000, 500); // Alert sound
  });

  // Try connecting. If fails, create AP "PicchiSetup"
  if (!wm.autoConnect("PicchiSetup")) {
    Serial.println("Failed to connect and hit timeout");
    ESP.restart();
  }

  // --- WE ARE CONNECTED ---
  
  // Save Name if changed
  if (strlen(custom_name.getValue()) > 0) {
    strcpy(userName, custom_name.getValue());
    preferences.putString("name", userName);
  }

  // Setup Time (Dhaka GMT+6)
  configTime(21600, 0, "pool.ntp.org");

  // Welcome Screen
  display.clearDisplay();
  display.setCursor(0, 10); display.println("WiFi Connected!");
  display.setCursor(0, 30); display.print("Hi, ");
  display.setTextSize(2); display.println(userName);
  display.display();
  
  tone(PIN_BUZZER, 1000, 100); delay(100);
  tone(PIN_BUZZER, 3000, 200);
  delay(2000);
}

// --- SOUND HELPER ---
void sndClick() { tone(PIN_BUZZER, 4000, 20); }
void sndBack()  { tone(PIN_BUZZER, 2000, 50); }
void sndJump()  { tone(PIN_BUZZER, 600, 50); }
void sndDie()   { tone(PIN_BUZZER, 150, 300); }
void sndPoint() { tone(PIN_BUZZER, 2500, 100); }

// --- MAIN LOOP ---
void loop() {
  // Global Back Button (Long Press OK)
  if (digitalRead(BTN_OK) == LOW) {
    unsigned long p = millis();
    while(digitalRead(BTN_OK) == LOW); // Wait
    if (millis() - p > 800) {
      currentState = FACE;
      sndBack();
      return;
    } else {
      handleShortPress();
    }
  }

  // State Handler
  switch(currentState) {
    case FACE:        runFaceEngine(); break;
    case MENU:        runMenu(); break;
    case GAME_DINO:   runDinoGame(); break;
    case GAME_FLAPPY: runFlappyGame(); break;
    case GAME_BRICK:  runBrickGame(); break;
    case CLOCK:       runClock(); break;
    case WEATHER:     runWeather(); break;
    case TIMER:       runTimer(); break;
    case JOKE:        runJoke(); break;
  }
}

void handleShortPress() {
  sndClick();
  if (currentState == FACE) {
    currentState = MENU;
  } 
  else if (currentState == MENU) {
    switch(menuIndex) {
      case 0: currentState = FACE; break;
      case 1: currentState = GAME_DINO; break;
      case 2: currentState = GAME_FLAPPY; break;
      case 3: currentState = GAME_BRICK; break;
      case 4: currentState = CLOCK; break;
      case 5: currentState = WEATHER; fetchWeather(); break;
      case 6: currentState = TIMER; break;
      case 7: currentState = JOKE; fetchJoke(); break;
      case 8: // Reset WiFi
        display.clearDisplay();
        display.setCursor(10,20); display.print("Erasing WiFi...");
        display.display();
        WiFiManager wm;
        wm.resetSettings();
        ESP.restart();
        break;
    }
  }
}

// ==================================================
// 1. GYRO FACE ENGINE
// ==================================================
void runFaceEngine() {
  // Gyro Logic
  sensors_event_t a, g, temp;
  bool gyroActive = mpu.getEvent(&a, &g, &temp);
  
  if(gyroActive) {
    ay = a.acceleration.y; // Tilt Up/Down
    ax = a.acceleration.x; // Tilt Left/Right
    
    // Thresholds (Adjust based on mounting)
    if (ay > 3.0) currentEye = look_down;
    else if (ay < -3.0) currentEye = look_up;
    else if (ax > 3.0) currentEye = look_left;
    else if (ax < -3.0) currentEye = look_right;
    else {
      // Saccades if stable
      if (millis() - lastSaccade > random(3000, 8000)) {
        int r = random(0, 10);
        if (r < 5) currentEye = normal;
        else if (r == 6) currentEye = focused;
        else if (r == 7) currentEye = happy;
        else if (r == 8) currentEye = blink; // quick blink
        lastSaccade = millis();
      }
    }
  }

  // Blink Logic
  if (millis() - lastBlink > random(2000, 6000)) {
    display.clearDisplay();
    // Context Blink
    if(currentEye == look_up) display.drawBitmap(0,0, blink_up, 128,64, WHITE);
    else if(currentEye == look_down) display.drawBitmap(0,0, blink_down, 128,64, WHITE);
    else display.drawBitmap(0,0, blink, 128,64, WHITE);
    
    display.display();
    delay(150);
    lastBlink = millis();
  }

  // Render
  display.clearDisplay();
  display.drawBitmap(0, 0, currentEye, 128, 64, WHITE);
  display.display();
}

// ==================================================
// 2. MENU SYSTEM
// ==================================================
void runMenu() {
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(WHITE);
  
  // Header with User Name
  display.setCursor(0, 0); 
  display.print("USER: "); display.println(userName);
  display.drawLine(0, 10, 128, 10, WHITE);

  // Navigation
  if (digitalRead(BTN_UP) == LOW) { menuIndex--; sndClick(); delay(150); }
  if (digitalRead(BTN_DOWN) == LOW) { menuIndex++; sndClick(); delay(150); }
  
  if (menuIndex < 0) menuIndex = menuLen - 1;
  if (menuIndex >= menuLen) menuIndex = 0;

  int startY = 15;
  for (int i = 0; i < 4; i++) { 
    int idx = (menuIndex + i) % menuLen;
    display.setCursor(10, startY + (i * 12));
    if(i == 0) { display.print("> "); } 
    else { display.print("  "); }
    display.print(menuItems[idx]);
  }
  display.display();
}

// ==================================================
// 3. GAME: DINO RUN
// ==================================================
int dinoY = 45;
int dinoV = 0;
bool dinoJump = false;
int cactusX = 128;
int dinoScore = 0;
bool dinoDead = false;

void runDinoGame() {
  if(dinoDead) {
    display.clearDisplay();
    display.setCursor(30,20); display.setTextSize(2); display.print("GAME OVER");
    display.setCursor(40,40); display.setTextSize(1); display.print("Score: "); display.print(dinoScore);
    display.display();
    if(digitalRead(BTN_OK)==LOW) { dinoDead=false; dinoScore=0; cactusX=128; delay(500); }
    return;
  }

  if(digitalRead(BTN_OK)==LOW && !dinoJump) { dinoV = -8; dinoJump = true; sndJump(); }
  
  dinoY += dinoV;
  if(dinoY < 45) dinoV++; else { dinoY=45; dinoV=0; dinoJump=false; }
  
  cactusX -= 4;
  if(cactusX < -10) { cactusX = 128 + random(0,60); dinoScore++; sndClick(); }
  
  if(cactusX < 20 && cactusX > 5 && dinoY > 35) { sndDie(); dinoDead = true; }
  
  display.clearDisplay();
  display.setCursor(0,0); display.print(dinoScore);
  display.drawLine(0,56,128,56,WHITE);
  display.fillRect(10, dinoY, 10, 10, WHITE);
  display.fillRect(cactusX, 45, 6, 10, WHITE);
  display.display();
}

// ==================================================
// 4. GAME: FLAPPY BOT
// ==================================================
float birdY = 32, birdV = 0;
int pipeX = 128, gapY = 20, flappyScore = 0;
bool flappyDead = false;

void runFlappyGame() {
  if(flappyDead) {
    display.clearDisplay();
    display.setCursor(30,20); display.setTextSize(2); display.print("CRASHED");
    display.setCursor(40,40); display.setTextSize(1); display.print("Score: "); display.print(flappyScore);
    display.display();
    if(digitalRead(BTN_OK)==LOW) { flappyDead=false; flappyScore=0; birdY=32; pipeX=128; delay(500); }
    return;
  }

  if(digitalRead(BTN_OK)==LOW) { birdV = -3.0; sndJump(); }
  birdV += 0.4; birdY += birdV;
  
  pipeX -= 2;
  if(pipeX < -10) { pipeX = 128; gapY = random(10,40); flappyScore++; sndClick(); }
  
  if(birdY<0 || birdY>63 || (pipeX<26 && pipeX>14 && (birdY<gapY || birdY>gapY+25))) { sndDie(); flappyDead=true; }
  
  display.clearDisplay();
  display.setCursor(0,0); display.print(flappyScore);
  display.fillCircle(20, (int)birdY, 3, WHITE);
  display.fillRect(pipeX, 0, 10, gapY, WHITE);
  display.fillRect(pipeX, gapY+25, 10, 64-(gapY+25), WHITE);
  display.display();
}

// ==================================================
// 5. GAME: BRICK BREAKER
// ==================================================
float ballX=64, ballY=50, ballDX=1.5, ballDY=-1.5;
int padX=50, brickScore=0;
bool bricks[4][10], brickInit=false, brickOver=false;

void resetBricks() {
  for(int r=0;r<4;r++) for(int c=0;c<10;c++) bricks[r][c]=true;
  brickInit=true; ballX=64; ballY=50; ballDY=-1.5; brickScore=0;
}

void runBrickGame() {
  if(!brickInit) resetBricks();
  if(brickOver) {
    display.clearDisplay();
    display.setCursor(30,20); display.print("GAME OVER");
    display.setCursor(30,35); display.print("Score: "); display.print(brickScore);
    display.display();
    if(digitalRead(BTN_OK)==LOW) { brickOver=false; resetBricks(); delay(500); }
    return;
  }

  if(digitalRead(BTN_LEFT)==LOW) padX -= 4;
  if(digitalRead(BTN_RIGHT)==LOW) padX += 4;
  if(padX<0) padX=0; if(padX>108) padX=108;

  ballX+=ballDX; ballY+=ballDY;
  if(ballX<=0 || ballX>=128) { ballDX=-ballDX; sndClick(); }
  if(ballY<=0) { ballDY=-ballDY; sndClick(); }
  if(ballY>64) { sndDie(); brickOver=true; }

  if(ballY>=58 && ballY<=60 && ballX>=padX && ballX<=padX+20) { ballDY=-ballDY; sndJump(); }

  int col = (ballX-4)/12; int row = (ballY-4)/6;
  if(row>=0 && row<4 && col>=0 && col<10 && bricks[row][col]) {
    bricks[row][col]=false; ballDY=-ballDY; brickScore+=10; sndPoint();
  }

  display.clearDisplay();
  display.fillRect(padX, 60, 20, 2, WHITE);
  display.fillCircle((int)ballX, (int)ballY, 2, WHITE);
  for(int r=0;r<4;r++) for(int c=0;c<10;c++) 
    if(bricks[r][c]) display.fillRect(4+(c*12)+1, 4+(r*6)+1, 10, 4, WHITE);
  display.display();
}

// ==================================================
// 6. UTILITIES
// ==================================================
void runClock() {
  struct tm t;
  if(getLocalTime(&t)) {
    char tb[10], db[20];
    strftime(tb, 10, "%H:%M:%S", &t);
    strftime(db, 20, "%d %b %Y", &t);
    display.clearDisplay();
    display.setTextSize(2); display.setCursor(15,20); display.print(tb);
    display.setTextSize(1); display.setCursor(30,45); display.print(db);
    display.display();
  }
}

void runTimer() {
  static unsigned long tStart = 0;
  static bool tRun = false;
  if(digitalRead(BTN_OK)==LOW) { tRun = !tRun; tStart = millis(); delay(300); }
  
  display.clearDisplay();
  display.setCursor(40,10); display.print("TIMER");
  long elapsed = (tRun) ? (millis() - tStart) : 0;
  int s = (elapsed/1000)%60; int m = elapsed/60000;
  
  display.setTextSize(3); display.setCursor(20,30);
  if(m<10) display.print("0"); display.print(m); display.print(":");
  if(s<10) display.print("0"); display.print(s);
  display.display();
}

void runWeather() {
  display.clearDisplay();
  display.setTextSize(1); display.setCursor(0,0); display.print("DHAKA WEATHER");
  display.drawLine(0,9,128,9,WHITE);
  display.setTextSize(3); display.setCursor(10,20); display.print(tempStr); display.setTextSize(1); display.print("C");
  display.setCursor(0,55); display.print(weatherDesc);
  display.display();
}

void runJoke() {
  display.clearDisplay();
  display.setCursor(0,0); display.print("Daily Joke:");
  display.drawLine(0,9,128,9,WHITE);
  display.setCursor(0,15); display.print(randomMsg);
  display.display();
}

void fetchWeather() {
  if(WiFi.status()!=WL_CONNECTED) return;
  display.clearDisplay(); display.setCursor(10,20); display.print("Updating..."); display.display();
  HTTPClient http;
  http.begin("https://api.openweathermap.org/data/2.5/weather?q=Dhaka,bd&units=metric&appid=" + String(OW_APIKEY));
  if(http.GET()>0) {
    String p = http.getString();
    DynamicJsonDocument doc(1024); deserializeJson(doc, p);
    tempStr = String(doc["main"]["temp"].as<float>(), 1);
    weatherDesc = doc["weather"][0]["description"].as<String>();
  }
  http.end();
}

void fetchJoke() {
  if(WiFi.status()!=WL_CONNECTED) return;
  display.clearDisplay(); display.setCursor(10,20); display.print("Fetching..."); display.display();
  HTTPClient http;
  http.begin("https://v2.jokeapi.dev/joke/Programming?type=single&blacklistFlags=nsfw,religious,political,racist,sexist");
  if(http.GET()>0) {
    String p = http.getString();
    DynamicJsonDocument doc(1024); deserializeJson(doc, p);
    randomMsg = doc["joke"].as<String>();
  }
  http.end();
}
