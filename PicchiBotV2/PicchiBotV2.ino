/***************************************************
  PicchiBot v11.0 - Tick Tock Edition
  
  HARDWARE MAP:
  - OLED & MPU6050: SDA(21), SCL(22)
  - BUZZER:         GPIO 14
  - BUTTONS:        UP(26), DOWN(33), LEFT(25), RIGHT(27), OK(32)

  CHANGELOG:
  - [x] Bluetooth Removed (Lighter, Faster)
  - [x] WiFi Manager: Standard Blocking Mode (Fixes "Can't Connect")
  - [x] Timer: UI with Selection Box (Min/Sec) + Bomb Sound
  - [x] Pomodoro: Logic fixed
  - [x] Face: WiFi Icon added
****************************************************/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <ArduinoJson.h>
#include <WiFiManager.h> 
#include <Preferences.h>

// --- 1. ASSETS ---
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

// --- 2. CONFIG ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C

#define PIN_BUZZER 14
#define BTN_UP     26
#define BTN_DOWN   33
#define BTN_LEFT   25
#define BTN_RIGHT  27
#define BTN_OK     32

const char* OW_APIKEY = "9c5ed9a6da0fc29aac5db777c9638d1a"; 
const char* NEWS_API_KEY = "5df4945cab76462896d02c54d23ca77d"; 

// --- 3. GLOBALS ---
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_MPU6050 mpu;
Preferences preferences;

char userName[32] = "Friend"; 
bool wifiConnected = false;

enum AppState { FACE, MENU, CLOCK, WEATHER, NEWS, GAME_DINO, GAME_FLAPPY, GAME_BRICK, TIMER, POMODORO, JOKE, ALERT_MODE };
AppState currentState = FACE;

const char* menuItems[] = { "Back to Face", "Timer", "Pomodoro", "Breaking News", "Dino Run", "Flappy Bot", "Brick Breaker", "Clock", "Weather", "Joke", "Reset WiFi" };
int menuIndex = 0;
const int menuLen = 11;

// Face Vars
const unsigned char* currentEye = normal;
unsigned long lastBlink = 0;
unsigned long lastSaccade = 0;
unsigned long lastSound = 0;
unsigned long shakeStartTime = 0;
bool isShaking = false;
unsigned long lastMotionTime = 0; 
unsigned long emotionTimer = 0;
bool isSleeping = false;
bool isAngry = false;
bool isExcited = false;

// Alert Vars
unsigned long lastQuakeCheck = 0;
unsigned long lastRainCheck = 0;
String tempStr = "--";
String weatherDesc = "Offline";
String randomMsg = "Connect WiFi";
String alertType = "";
String alertInfo = "";

// News Vars
String newsTitle = "Loading...";
String newsSource = "";
int newsPage = 1;
int newsScrollX = 128;

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);

  Wire.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Display Error")); for(;;);
  }
  
  if (!mpu.begin()) Serial.println("MPU Error");
  else {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  preferences.begin("user-config", false);
  String savedName = preferences.getString("name", "Friend");
  savedName.toCharArray(userName, 32);

  cyberpunkBoot();
  runWiFiManager();
  lastMotionTime = millis();
}

void cyberpunkBoot() {
  display.clearDisplay();
  display.setTextSize(2); display.setTextColor(WHITE);
  display.setCursor(5, 15); display.print("PicchiBot");
  display.setTextSize(1); display.setCursor(115, 15); display.print("V11");
  display.drawLine(0, 35, 128, 35, WHITE);
  display.setCursor(75, 45); display.print("by Lian");
  display.display();
  tone(PIN_BUZZER, 1000, 100); delay(100);
  tone(PIN_BUZZER, 2000, 100); delay(100);
  tone(PIN_BUZZER, 4000, 400); delay(1500);
}

void runWiFiManager() {
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(WHITE);
  
  WiFiManager wm;
  
  // 1. FORCE OFFLINE OPTION
  // Check if OK button held during boot
  if(digitalRead(BTN_OK) == LOW) {
    wifiConnected = false;
    display.clearDisplay();
    display.setCursor(0, 20); display.println("Force Offline");
    display.display();
    delay(1000);
    return;
  }

  WiFiManagerParameter custom_name("name", "Enter Your Name", userName, 32);
  wm.addParameter(&custom_name);

  // 2. STANDARD MODE (Solid, reliable)
  wm.setAPCallback([](WiFiManager *myWiFiManager) {
    display.clearDisplay();
    display.setCursor(0,0); display.println("SETUP MODE");
    display.setCursor(0,15); display.println("WiFi: PicchiSetup");
    display.setCursor(0,25); display.println("Pass: 12345678");
    display.setCursor(0,40); display.print("IP: "); display.println(WiFi.softAPIP()); 
    display.display();
    tone(PIN_BUZZER, 1000, 500);
  });

  // This will BLOCK until connected. Reliable.
  if(!wm.autoConnect("PicchiSetup", "12345678")) {
    wifiConnected = false;
    display.clearDisplay(); display.setCursor(0,20); display.println("Setup Failed"); display.display();
  } else {
    wifiConnected = true;
    if (strlen(custom_name.getValue()) > 0) {
      strcpy(userName, custom_name.getValue());
      preferences.putString("name", userName);
    }
    configTime(21600, 0, "pool.ntp.org");
    
    display.clearDisplay();
    display.setCursor(0,20); display.println("Connected!");
    display.setCursor(0,40); display.print("Hi "); display.println(userName);
    display.display();
    delay(1500);
  }
}

// --- SOUND FX ---
void makeHappySound() { tone(PIN_BUZZER,2000,80); delay(100); tone(PIN_BUZZER,2500,80); delay(100); tone(PIN_BUZZER,3500,150); }
void makeAngrySound() { tone(PIN_BUZZER,150,300); delay(350); tone(PIN_BUZZER,100,500); }
void makeRobotSound() { for (int i=0; i<3; i++) { tone(PIN_BUZZER, random(1000, 3000), 100); delay(150); } }
void sndClick() { tone(PIN_BUZZER, 4000, 20); }
void sndBack()  { tone(PIN_BUZZER, 2000, 50); }
void sndJump()  { tone(PIN_BUZZER, 600, 30); }
void sndDie()   { tone(PIN_BUZZER, 150, 400); }
void sndPoint() { tone(PIN_BUZZER, 2500, 100); }
void sndAlarm() { for(int i=0; i<3; i++) { tone(PIN_BUZZER, 3000, 200); delay(250); tone(PIN_BUZZER, 2000, 200); delay(250); } }

// --- MAIN LOOP ---
void loop() {
  if (wifiConnected && currentState == FACE) {
    if (millis() - lastQuakeCheck > 300000) { checkEarthquake(); lastQuakeCheck = millis(); }
    if (millis() - lastRainCheck > 600000) { checkRain(); lastRainCheck = millis(); }
  }

  if (digitalRead(BTN_OK) == LOW) {
    unsigned long p = millis();
    while(digitalRead(BTN_OK) == LOW); 
    if (millis() - p > 800) { currentState = FACE; sndBack(); return; } 
    else handleShortPress();
  }

  switch(currentState) {
    case FACE:        runFaceEngine(); break;
    case MENU:        runMenu(); break;
    case GAME_DINO:   runDinoGame(); break;
    case GAME_FLAPPY: runFlappyGame(); break;
    case GAME_BRICK:  runBrickGame(); break;
    case CLOCK:       runClock(); break;
    case WEATHER:     runWeather(); break;
    case NEWS:        runNewsApp(); break; 
    case TIMER:       runTimer(); break;
    case POMODORO:    runPomodoro(); break;
    case JOKE:        runJoke(); break;
    case ALERT_MODE:   runAlert(); break;
  }
}

void handleShortPress() {
  sndClick();
  if (currentState == FACE) { currentState = MENU; } 
  else if (currentState == MENU) {
    switch(menuIndex) {
      case 0: currentState = FACE; break;
      case 1: currentState = TIMER; break;
      case 2: currentState = POMODORO; break;
      case 3: currentState = NEWS; newsPage=1; fetchNews(); break;
      case 4: currentState = GAME_DINO; break;
      case 5: currentState = GAME_FLAPPY; break;
      case 6: currentState = GAME_BRICK; break;
      case 7: currentState = CLOCK; break;
      case 8: currentState = WEATHER; if(wifiConnected) checkRain(); break;
      case 9: currentState = JOKE; if(wifiConnected) fetchJoke(); break;
      case 10: 
        display.clearDisplay(); display.setCursor(10,20); display.print("Resetting..."); display.display();
        WiFiManager wm; wm.resetSettings(); ESP.restart(); 
        break;
    }
  } else if (currentState == ALERT_MODE) {
    currentState = FACE;
  }
}

// ==================================================
// 1. FACE ENGINE
// ==================================================
void drawWifiIcon() {
  if(wifiConnected) {
    // Draw small WiFi arcs top right
    display.drawLine(118, 10, 120, 10, WHITE);
    display.drawLine(116, 8, 122, 8, WHITE);
    display.drawLine(114, 6, 124, 6, WHITE);
  }
}

void runFaceEngine() {
  sensors_event_t a, g, temp;
  if(mpu.getEvent(&a, &g, &temp)) {
    float mag = sqrt(sq(a.acceleration.x) + sq(a.acceleration.y) + sq(a.acceleration.z));
    if (mag > 13.0 || mag < 7.0) {
      lastMotionTime = millis();
      if (isSleeping) { isSleeping = false; isExcited = true; emotionTimer = millis(); makeHappySound(); currentEye = excited; }
    }
    if (isExcited && millis() - emotionTimer > 2000) { isExcited = false; currentEye = normal; }
    if (mag > 20.0) { 
      if (shakeStartTime == 0) shakeStartTime = millis();
      if (millis() - shakeStartTime > 4000) {
        if (!isAngry) { isAngry = true; emotionTimer = millis(); makeAngrySound(); currentEye = furious; }
        shakeStartTime = millis();
      } else if (millis() - shakeStartTime > 1000) { currentEye = disoriented; }
    } else { shakeStartTime = 0; }
    if (isAngry && millis() - emotionTimer > 3000) { isAngry = false; currentEye = normal; }
    if (!isSleeping && !isAngry && !isExcited && (millis() - lastMotionTime > 30000)) { isSleeping = true; currentEye = sleepy; }
    if (!isSleeping && !isAngry && !isExcited && !shakeStartTime) {
      if (a.acceleration.y > 4.0) currentEye = look_down;
      else if (a.acceleration.y < -4.0) currentEye = look_up;
      else if (a.acceleration.x > 4.0) currentEye = look_left;
      else if (a.acceleration.x < -4.0) currentEye = look_right;
      else if (millis() - lastSaccade > random(3000, 8000)) {
        int rand = random(0, 100);
        if (rand < 50) currentEye = normal; else if (rand < 60) currentEye = focused; else if (rand < 70) currentEye = bored; else currentEye = blink; 
        lastSaccade = millis();
      }
    }
  }
  if (currentEye != furious && currentEye != disoriented && currentEye != sleepy) {
    if (millis() - lastBlink > random(2000, 6000)) {
      display.clearDisplay();
      if(currentEye == look_up) display.drawBitmap(0,0, blink_up, 128,64, WHITE);
      else if(currentEye == look_down) display.drawBitmap(0,0, blink_down, 128,64, WHITE);
      else display.drawBitmap(0,0, blink, 128,64, WHITE);
      drawWifiIcon(); display.display(); delay(150); lastBlink = millis();
    }
  }
  display.clearDisplay(); display.drawBitmap(0, 0, currentEye, 128, 64, WHITE); 
  drawWifiIcon(); display.display();
}

// ==================================================
// 2. TIMER: TICK TOCK EDITION
// ==================================================
int timerMin=0, timerSec=0; 
bool timerActive=false; 
unsigned long timerTarget=0;
int timerSel = 0; // 0 = Min, 1 = Sec
unsigned long lastBombBeep = 0;

void runTimer() {
  display.clearDisplay();
  
  if (timerActive) {
    // RUNNING MODE
    long remaining = (long)(timerTarget - millis());
    
    if (remaining <= 0) {
      timerActive = false;
      // EXPLOSION SOUND
      for(int i=0; i<5; i++) { tone(PIN_BUZZER, 100 + random(0,500), 50); delay(50); }
      tone(PIN_BUZZER, 2000, 1000);
    } else {
      // BOMB SOUND LOGIC
      long interval = remaining / 20; // Beep gets faster as time drops
      if (interval < 50) interval = 50; // Cap max speed
      if (interval > 1000) interval = 1000; // Cap slow speed
      
      if (millis() - lastBombBeep > interval) {
        tone(PIN_BUZZER, 3000, 20); // Short Tick
        lastBombBeep = millis();
      }

      int m = (remaining / 1000) / 60;
      int s = (remaining / 1000) % 60;
      
      display.setTextSize(1); display.setCursor(40, 0); display.print("RUNNING");
      display.setTextSize(3); display.setCursor(20, 30);
      if(m<10) display.print("0"); display.print(m); display.print(":");
      if(s<10) display.print("0"); display.print(s);
    }
  } 
  else {
    // SETUP MODE with Rounded Box
    display.setTextSize(1); display.setCursor(35, 0); display.print("SET TIMER");
    
    // Selection Box Logic
    if (timerSel == 0) display.drawRoundRect(15, 25, 40, 30, 4, WHITE); // Box around Mins
    else display.drawRoundRect(70, 25, 40, 30, 4, WHITE); // Box around Secs
    
    // Navigation
    if(digitalRead(BTN_LEFT)==LOW || digitalRead(BTN_RIGHT)==LOW) { 
      timerSel = !timerSel; sndClick(); delay(200); 
    }
    if(digitalRead(BTN_UP)==LOW) {
      if(timerSel==0) { timerMin++; if(timerMin>99) timerMin=0; }
      else { timerSec+=5; if(timerSec>59) timerSec=0; }
      sndClick(); delay(150);
    }
    if(digitalRead(BTN_DOWN)==LOW) {
      if(timerSel==0) { timerMin--; if(timerMin<0) timerMin=99; }
      else { timerSec-=5; if(timerSec<0) timerSec=55; }
      sndClick(); delay(150);
    }
    
    // Draw Numbers
    display.setTextSize(3); display.setCursor(20, 30);
    if(timerMin<10) display.print("0"); display.print(timerMin); 
    display.setCursor(55, 30); display.print(":");
    display.setCursor(75, 30);
    if(timerSec<10) display.print("0"); display.print(timerSec);
    
    display.setTextSize(1); display.setCursor(35, 55); display.print("OK to Start");
    
    if(digitalRead(BTN_OK)==LOW && (timerMin>0||timerSec>0)){
      timerTarget = millis() + (timerMin*60000) + (timerSec*1000);
      timerActive = true;
      sndClick(); delay(500);
    }
  }
  display.display();
}

// ==================================================
// 3. POMODORO (FIXED)
// ==================================================
int pomoState=0; unsigned long pomoEnd=0;
void runPomodoro(){
  display.clearDisplay();
  
  if(pomoState==0){ // Setup
    display.setTextSize(2);display.setCursor(15,10);display.print("POMODORO");
    display.setTextSize(1);display.setCursor(10,40);display.print("OK: Start 20m Focus");
    if(digitalRead(BTN_OK)==LOW){pomoState=1;pomoEnd=millis()+(20*60*1000);sndClick();delay(500);}
  }
  else {
    long rem=(long)(pomoEnd-millis())/1000;
    
    // State Transition
    if(rem<=0){
      sndAlarm();
      if(pomoState==1){ // Study done -> Break
        pomoState=2; pomoEnd=millis()+(5*60*1000); 
      } else { // Break done -> Reset
        pomoState=0;
      }
    }
    
    display.setTextSize(2);display.setCursor(10,0);
    if(pomoState==1) display.print(" STUDY "); else display.print(" CHILL ");
    
    int m=rem/60;int s=rem%60;
    display.setTextSize(4);display.setCursor(5,30);
    if(m<10)display.print("0");display.print(m);display.print(":");
    if(s<10)display.print("0");display.print(s);
  }
  display.display();
}

// ==================================================
// 4. OTHER APPS (News, Menu, etc)
// ==================================================
void runNewsApp() {
  display.clearDisplay(); display.drawBitmap(0, 0, worried, 128, 64, WHITE);
  display.fillRect(0, 40, 128, 24, BLACK); display.drawLine(0, 40, 128, 40, WHITE);
  display.setTextSize(1); display.setCursor(0, 42); display.print("NEWS("); display.print(newsPage); display.print("): "); display.print(newsSource);
  display.setCursor(newsScrollX, 52); display.print(newsTitle);
  newsScrollX -= 2; if (newsScrollX < - (int)(newsTitle.length() * 6)) newsScrollX = 128;
  display.display();
  if(digitalRead(BTN_RIGHT)==LOW) { newsPage++; fetchNews(); delay(200); }
  if(digitalRead(BTN_LEFT)==LOW && newsPage>1) { newsPage--; fetchNews(); delay(200); }
}
void fetchNews() {
  if (!wifiConnected) { newsTitle = "No WiFi Connection"; newsSource="Error"; return; }
  display.clearDisplay(); display.drawBitmap(0, 0, alert, 128, 64, WHITE); 
  display.setCursor(30, 50); display.setTextSize(1); display.print("FETCHING..."); display.display();
  HTTPClient http; WiFiClientSecure client; client.setInsecure();
  String url = "https://newsapi.org/v2/top-headlines?country=bd&pageSize=1&page=" + String(newsPage) + "&apiKey=" + String(NEWS_API_KEY);
  http.begin(client, url); http.addHeader("User-Agent", "PicchiBot/1.0");
  if (http.GET() > 0) {
    String p = http.getString(); JsonDocument doc; deserializeJson(doc, p);
    if (doc["totalResults"].as<int>() > 0 && doc["articles"][0]["title"]) {
      newsTitle = doc["articles"][0]["title"].as<String>();
      newsSource = doc["articles"][0]["source"]["name"].as<String>();
    } else { newsTitle = "End of Headlines"; newsSource = "System"; }
  } else { newsTitle = "API Error"; display.clearDisplay(); display.drawBitmap(0, 0, despair, 128, 64, WHITE); display.display(); delay(1000); }
  http.end(); newsScrollX = 128;
}
void runAlert() {
  static bool inverted = false; inverted = !inverted; display.invertDisplay(inverted);
  display.clearDisplay(); display.drawBitmap(0, 0, scared, 128, 64, WHITE); 
  display.fillRect(0, 15, 128, 34, BLACK); display.drawRect(0, 15, 128, 34, WHITE);
  display.setCursor(10, 20); display.setTextSize(2); display.print("WARNING!");
  display.setCursor(10, 40); display.setTextSize(1); display.print(alertType);
  display.display(); sndAlarm(); 
}
void checkEarthquake() {
  WiFiClientSecure client; client.setInsecure(); HTTPClient http;
  http.begin(client, "https://earthquake.usgs.gov/fdsnws/event/1/query?format=geojson&latitude=23.81&longitude=90.41&maxradiuskm=500&minmagnitude=2.0&limit=1");
  if(http.GET()>0) { String p = http.getString(); JsonDocument doc; deserializeJson(doc, p);
    if (doc["metadata"]["count"].as<int>() > 0) { float mag = doc["features"][0]["properties"]["mag"]; alertType = "EARTHQUAKE " + String(mag, 1); alertInfo = "Recent Detected!"; currentState = ALERT_MODE; }
  } http.end();
}
void checkRain() {
  HTTPClient http; http.begin("https://api.openweathermap.org/data/2.5/weather?q=Dhaka,bd&units=metric&appid=" + String(OW_APIKEY));
  if(http.GET()>0) { String p=http.getString(); JsonDocument doc; deserializeJson(doc,p);
    tempStr=String(doc["main"]["temp"].as<float>(),1); String w=doc["weather"][0]["main"].as<String>(); weatherDesc=w;
    if(w.indexOf("Rain")!=-1 || w.indexOf("Thunder")!=-1){alertType="BAD WEATHER";alertInfo=w;currentState=ALERT_MODE;}
  } http.end();
}
void runMenu() {
  display.invertDisplay(false); display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(0,0); display.print("USER: "); display.println(userName); display.drawLine(0,9,128,9,WHITE);
  if(digitalRead(BTN_UP)==LOW) { menuIndex--; sndClick(); delay(150); }
  if(digitalRead(BTN_DOWN)==LOW) { menuIndex++; sndClick(); delay(150); }
  if(menuIndex < 0) menuIndex = menuLen-1; if(menuIndex >= menuLen) menuIndex = 0;
  int startY = 15;
  for (int i=0; i<4; i++) { int idx = (menuIndex + i) % menuLen;
    display.setCursor(10, startY + (i*12)); if(i==0) display.print("> "); else display.print("  "); display.print(menuItems[idx]);
  } display.display();
}
// Games
int dinoY=45, dinoV=0, cactusX=128, dinoScore=0; bool dinoJump=false, dinoDead=false;
void runDinoGame() {
  static unsigned long lf=0; if(millis()-lf<30)return; lf=millis();
  if(dinoDead){display.clearDisplay();display.setCursor(35,20);display.setTextSize(2);display.print("DIED");display.setCursor(40,40);display.setTextSize(1);display.print("Score: ");display.print(dinoScore);display.display();if(digitalRead(BTN_OK)==LOW||digitalRead(BTN_UP)==LOW){dinoDead=false;dinoScore=0;cactusX=128;delay(500);}return;}
  if(digitalRead(BTN_UP)==LOW&&!dinoJump){dinoV=-9;dinoJump=true;sndJump();} dinoY+=dinoV; if(dinoY<45)dinoV+=2;else{dinoY=45;dinoV=0;dinoJump=false;}
  cactusX-=6; if(cactusX<-10){cactusX=128+random(0,60);dinoScore++;sndClick();} if(cactusX<20&&cactusX>5&&dinoY>35){sndDie();dinoDead=true;}
  display.clearDisplay();display.setTextSize(1);display.setCursor(0,0);display.print(dinoScore);display.drawLine(0,56,128,56,WHITE);display.fillRect(10,dinoY,10,10,WHITE);display.fillRect(cactusX,45,6,10,WHITE);display.display();
}
float birdY=32, birdV=0; int pipeX=128, gapY=20, flappyScore=0; bool flappyDead=false;
void runFlappyGame() {
  static unsigned long lf=0; if(millis()-lf<30)return; lf=millis();
  if(flappyDead){display.clearDisplay();display.setCursor(30,20);display.setTextSize(2);display.print("CRASH");display.setCursor(40,40);display.setTextSize(1);display.print("Score: ");display.print(flappyScore);display.display();if(digitalRead(BTN_OK)==LOW||digitalRead(BTN_UP)==LOW){flappyDead=false;flappyScore=0;birdY=32;pipeX=128;delay(500);}return;}
  if(digitalRead(BTN_UP)==LOW){birdV=-4.0;sndJump();} birdV+=0.6; birdY+=birdV; pipeX-=3; if(pipeX<-10){pipeX=128;gapY=random(5,35);flappyScore++;sndClick();}
  if(birdY<0||birdY>60||(pipeX<24&&pipeX>16&&(birdY<gapY||birdY>gapY+25))){sndDie();flappyDead=true;}
  display.clearDisplay();display.setTextSize(1);display.setCursor(0,0);display.print(flappyScore);display.fillCircle(20,(int)birdY,3,WHITE);display.fillRect(pipeX,0,8,gapY,WHITE);display.fillRect(pipeX,gapY+25,8,64-(gapY+25),WHITE);display.display();
}
float ballX=64,ballY=50,ballDX=2,ballDY=-2; int padX=50,brickScore=0; bool bricks[4][10],brickInit=false,brickOver=false;
void resetBricks(){for(int r=0;r<4;r++)for(int c=0;c<10;c++)bricks[r][c]=true;brickInit=true;ballX=64;ballY=50;ballDY=-2;brickScore=0;}
void runBrickGame(){
  static unsigned long lf=0; if(millis()-lf<20)return; lf=millis(); if(!brickInit)resetBricks();
  if(brickOver){display.clearDisplay();display.setCursor(20,20);display.setTextSize(2);display.print("GAME OVER");display.setCursor(40,40);display.setTextSize(1);display.print("Score: ");display.print(brickScore);display.display();if(digitalRead(BTN_OK)==LOW){brickOver=false;resetBricks();delay(500);}return;}
  if(digitalRead(BTN_LEFT)==LOW)padX-=6;if(digitalRead(BTN_RIGHT)==LOW)padX+=6;if(padX<0)padX=0;if(padX>108)padX=108;
  ballX+=ballDX;ballY+=ballDY; if(ballX<=0||ballX>=128){ballDX=-ballDX;sndClick();}if(ballY<=0){ballDY=-ballDY;sndClick();}if(ballY>64){sndDie();brickOver=true;}
  if(ballY>=58&&ballY<=60&&ballX>=padX&&ballX<=padX+20){ballDY=-ballDY;sndJump();}
  int col=(ballX-4)/12;int row=(ballY-4)/6;if(row>=0&&row<4&&col>=0&&col<10&&bricks[row][col]){bricks[row][col]=false;ballDY=-ballDY;brickScore+=10;sndPoint();}
  display.clearDisplay();display.fillRect(padX,60,20,3,WHITE);display.fillCircle((int)ballX,(int)ballY,2,WHITE);for(int r=0;r<4;r++)for(int c=0;c<10;c++)if(bricks[r][c])display.fillRect(4+(c*12)+1,4+(r*6)+1,10,4,WHITE);display.display();
}
void runClock(){ struct tm t; if(getLocalTime(&t)){char b[10],d[20];strftime(b,10,"%H:%M:%S",&t);strftime(d,20,"%d %b",&t);display.clearDisplay();display.setTextSize(2);display.setCursor(15,20);display.print(b);display.setTextSize(1);display.setCursor(30,45);display.print(d);display.display();} }
void runJoke(){ display.clearDisplay(); display.setCursor(0,0); display.print("Joke:"); display.drawLine(0,9,128,9,WHITE); display.setCursor(0,15); if(!wifiConnected)display.print("Connect WiFi");else display.print(randomMsg); display.display();}
void fetchJoke(){ WiFiClientSecure c; c.setInsecure(); HTTPClient h; h.begin(c,"https://v2.jokeapi.dev/joke/Programming?type=single&blacklistFlags=nsfw,religious,political,racist,sexist"); if(h.GET()>0){String p=h.getString(); JsonDocument d; deserializeJson(d,p); randomMsg=d["joke"].as<String>();} h.end();}
