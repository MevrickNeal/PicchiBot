/***************************************************
  PicchiBot v24.0 - Game Master & Physics Engine
  
  HARDWARE MAP:
  - OLED & MPU6050: SDA(21), SCL(22)
  - BUZZER:         GPIO 14
  - BUTTONS:        UP(26), DOWN(33), LEFT(25), RIGHT(27), OK(32)

  FIXES:
  - [x] Pong/DXBall: Fixed Ball "Tunneling" through paddle
  - [x] DXBall: Added Powerups (+/-) and Infinite Levels
  - [x] Timer/Pomo: 2s Hold to Start, 3s Hold to Exit
  - [x] Network: Aggressive SSL/HTTP Fixes for News/AI
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
const char* GEMINI_API_KEY = "AIzaSyB_NfMMWJyxwrgkkG7cKUd74_MxUyMOePA"; 

// --- 3. GLOBALS ---
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_MPU6050 mpu;
Preferences preferences;

char userName[32] = "Friend"; 
bool wifiConnected = false;

enum AppState { 
  FACE, MENU, THERAPY, KEYBOARD, WIFI_SCAN, 
  GAME_PONG, GAME_DINO, GAME_BRICK, 
  CLOCK, WEATHER, NEWS, TIMER, POMODORO, JOKE, 
  ALERT_MODE, AI_THINKING 
};
AppState currentState = FACE;
AppState returnState = FACE; 

const char* menuItems[] = { "Back to Face", "Table Tennis", "Brick Breaker", "Dino Run", "Timer", "Pomodoro", "Therapy (AI)", "News", "Clock", "Weather", "Joke", "WiFi Setup" };
int menuIndex = 0;
const int menuLen = 12;

// Face
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

// Therapy / AI
unsigned long lastTherapyTime = 0;
const unsigned long THERAPY_INTERVAL = 300000; 
String aiResponse = "Hello! I am PicchiBot.";
String keyboardBuffer = ""; 
int keyboardTarget = 0; 
String targetSSID = ""; 
int aiScrollY = 0;

// Keyboard
int kbRow = 0; int kbCol = 0; int kbPage = 0;
const char* keys[3][3][10] = {
  { {"Q","W","E","R","T","Y","U","I","O","P"}, {"A","S","D","F","G","H","J","K","L","<"}, {"Z","X","C","V","B","N","M",".","_","^"} },
  { {"q","w","e","r","t","y","u","i","o","p"}, {"a","s","d","f","g","h","j","k","l","<"}, {"z","x","c","v","b","n","m",",","_","^"} },
  { {"1","2","3","4","5","6","7","8","9","0"}, {"!","@","#","$","%","&","*","-","+","<"}, {"(",")","?",":","/","=","!","'","_","^"} }
};

// WiFi Scan
int wifiCount = 0; int wifiScroll = 0;

// Games & Data
float ballX=64, ballY=32, ballDX=2, ballDY=1.5; float paddleY=24, cpuY=24; int pScore=0, cScore=0;
String tempStr = "--"; String weatherDesc = "Offline"; String randomMsg = "Connect WiFi";
String newsTitle = "Loading..."; String newsSource = ""; int newsPage = 1; int newsScrollX = 128;
unsigned long lastQuakeCheck = 0; unsigned long lastRainCheck = 0;
String alertType = ""; String alertInfo = "";

// Powerups
struct PowerUp { float x, y; bool active; int type; }; // 0=Expand, 1=Shrink
PowerUp pUp;
int paddleWidth = 20; 

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  pinMode(BTN_UP, INPUT_PULLUP); pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP); pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP); pinMode(PIN_BUZZER, OUTPUT);

  Wire.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) { for(;;); }
  
  if (!mpu.begin()) Serial.println("MPU Error");
  else { mpu.setAccelerometerRange(MPU6050_RANGE_8_G); mpu.setFilterBandwidth(MPU6050_BAND_21_HZ); }

  preferences.begin("user-config", false);
  String savedName = preferences.getString("name", "Friend");
  savedName.toCharArray(userName, 32);

  cyberpunkBoot();
  silentWiFiConnect(); 
  
  lastMotionTime = millis();
  lastTherapyTime = millis();
}

void cyberpunkBoot() {
  display.clearDisplay();
  display.setTextSize(2); display.setTextColor(WHITE);
  display.setCursor(5, 15); display.print("PicchiBot");
  display.setTextSize(1); display.setCursor(115, 15); display.print("24");
  display.drawLine(0, 35, 128, 35, WHITE);
  display.setCursor(75, 45); display.print("by Lian");
  display.display();
  tone(PIN_BUZZER, 1000, 100); delay(100);
  tone(PIN_BUZZER, 2000, 100); delay(100);
  tone(PIN_BUZZER, 4000, 400); delay(1500);
}

void silentWiFiConnect() {
  String savedSSID = preferences.getString("ssid", "");
  String savedPass = preferences.getString("pass", "");

  if(savedSSID.length() > 0) {
    display.clearDisplay(); display.setCursor(0,20); display.print("Connecting..."); 
    display.setCursor(0,35); display.print(savedSSID); display.display();
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    int t = 0;
    while(WiFi.status() != WL_CONNECTED && t < 20) { delay(250); t++; }
    
    if(WiFi.status() == WL_CONNECTED) {
      wifiConnected = true; configTime(21600, 0, "pool.ntp.org");
      display.clearDisplay(); display.setCursor(0,20); display.print("Online!"); display.display(); delay(1000);
      checkRain();
    } else {
      wifiConnected = false;
      display.clearDisplay(); display.setCursor(0,20); display.print("Offline"); display.display(); delay(1000);
    }
  } else {
    wifiConnected = false;
  }
}

// --- SOUND FX ---
void sndClick() { tone(PIN_BUZZER, 4000, 20); }
void sndBack()  { tone(PIN_BUZZER, 2000, 50); }
void sndType()  { tone(PIN_BUZZER, 3000, 10); }
void sndJump()  { tone(PIN_BUZZER, 600, 30); }
void sndDie()   { tone(PIN_BUZZER, 150, 400); }
void sndPing()  { tone(PIN_BUZZER, 1500, 50); }
void sndNotify(){ tone(PIN_BUZZER, 1000, 100); delay(100); tone(PIN_BUZZER, 2000, 200); }
void makeHappy() { tone(PIN_BUZZER,2000,80); delay(100); tone(PIN_BUZZER,3500,150); }
void makeAngry() { tone(PIN_BUZZER,150,300); delay(350); tone(PIN_BUZZER,100,500); }
void sndAlarm()  { for(int i=0; i<3; i++) { tone(PIN_BUZZER, 3000, 200); delay(250); tone(PIN_BUZZER, 2000, 200); delay(250); } }
void makeRobotSound() { for (int i=0; i<3; i++) { tone(PIN_BUZZER, random(1000, 3000), 100); delay(150); } }

// --- MAIN LOOP ---
void loop() {
  if (wifiConnected && currentState == FACE) {
    unsigned long now = millis();
    if (now - lastQuakeCheck > 300000) { checkEarthquake(); lastQuakeCheck = now; }
    if (now - lastRainCheck > 600000) { checkRain(); lastRainCheck = now; }
  }

  // Auto Therapy Trigger
  if (currentState == FACE && (millis() - lastTherapyTime > THERAPY_INTERVAL)) {
    aiResponse = "Checking in... How are you feeling right now?";
    currentState = THERAPY; sndNotify(); lastTherapyTime = millis();
  }

  if (currentState != KEYBOARD && currentState != WIFI_SCAN && currentState != AI_THINKING && currentState != THERAPY) { 
    if (digitalRead(BTN_OK) == LOW) {
      unsigned long p = millis();
      while(digitalRead(BTN_OK) == LOW); 
      if (millis() - p > 800) { currentState = FACE; sndBack(); return; } 
      else handleShortPress();
    }
  } else if (currentState == KEYBOARD) {
    runKeyboard(); 
  } else if (currentState == WIFI_SCAN) {
    runWiFiScanner();
  } else if (currentState == THERAPY) {
    runTherapySession();
  }

  switch(currentState) {
    case FACE:        runFaceEngine(); break;
    case MENU:        runMenu(); break;
    case THERAPY:     /* Handled above */ break;
    case GAME_PONG:   runPongGame(); break;
    case GAME_DINO:   runDinoGame(); break;
    case GAME_BRICK:  runBrickGame(); break;
    case CLOCK:       runClock(); break;
    case WEATHER:     runWeather(); break;
    case NEWS:        runNewsApp(); break; 
    case TIMER:       runTimer(); break;
    case POMODORO:    runPomodoro(); break;
    case JOKE:        runJoke(); break;
    case ALERT_MODE:  runAlert(); break;
    case KEYBOARD:    break; 
    case WIFI_SCAN:   break;
    case AI_THINKING: break; 
  }
}

void handleShortPress() {
  sndClick();
  if (currentState == FACE) { currentState = MENU; } 
  else if (currentState == MENU) {
    switch(menuIndex) {
      case 0: currentState = FACE; break;
      case 1: currentState = GAME_PONG; break;
      case 2: currentState = GAME_BRICK; break;
      case 3: currentState = GAME_DINO; break;
      case 4: currentState = TIMER; break;
      case 5: currentState = POMODORO; break;
      case 6: currentState = THERAPY; aiScrollY=0; break; 
      case 7: currentState = NEWS; newsPage=1; fetchNews(); break;
      case 8: currentState = CLOCK; break;
      case 9: currentState = WEATHER; if(wifiConnected) checkRain(); break;
      case 10: currentState = JOKE; if(wifiConnected) fetchJoke(); break;
      case 11: currentState = WIFI_SCAN; startWiFiScan(); break; 
    }
  } else if (currentState == ALERT_MODE) {
    currentState = FACE;
  }
}

// ==================================================
// 1. TIMER & POMODORO (SMART HOLD LOGIC)
// ==================================================
int timerMin=0, timerSec=0, timerSel=0; bool timerActive=false; unsigned long timerTarget=0; unsigned long lastBomb=0;
void runTimer() {
  display.clearDisplay();
  
  // HOLD TO EXIT (3s)
  if(digitalRead(BTN_OK)==LOW) {
    unsigned long p=millis(); while(digitalRead(BTN_OK)==LOW);
    if(millis()-p > 3000) { currentState=FACE; sndBack(); return; }
    // HOLD TO START (2s)
    else if (millis()-p > 1500 && !timerActive) {
      if(timerMin>0||timerSec>0){ timerTarget=millis()+(timerMin*60000)+(timerSec*1000); timerActive=true; sndClick(); }
    }
  }

  if(timerActive){
    long rem=(long)(timerTarget-millis());
    if(rem<=0){timerActive=false;sndAlarm();}
    else{
      long inv=rem/20; if(inv<50)inv=50; if(inv>1000)inv=1000;
      if(millis()-lastBomb>inv){tone(PIN_BUZZER,3000,20);lastBomb=millis();}
      int m=(rem/1000)/60; int s=(rem/1000)%60;
      display.setTextSize(3);display.setCursor(20,30);if(m<10)display.print("0");display.print(m);display.print(":");if(s<10)display.print("0");display.print(s);
      display.setTextSize(1);display.setCursor(25,55);display.print("Hold OK to Stop");
    }
  } else {
    display.setTextSize(1);display.setCursor(35,0);display.print("SET TIMER");
    
    // Draw selection box
    if(timerSel==0) display.drawRoundRect(15,25,42,28,4,WHITE); 
    else display.drawRoundRect(70,25,42,28,4,WHITE);
    
    if(digitalRead(BTN_LEFT)==LOW||digitalRead(BTN_RIGHT)==LOW){timerSel=!timerSel;sndClick();delay(200);}
    if(digitalRead(BTN_UP)==LOW){if(timerSel==0){timerMin++;if(timerMin>99)timerMin=0;}else{timerSec+=5;if(timerSec>59)timerSec=0;}delay(100);}
    if(digitalRead(BTN_DOWN)==LOW){if(timerSel==0){timerMin--;if(timerMin<0)timerMin=99;}else{timerSec-=5;if(timerSec<0)timerSec=55;}delay(100);}
    
    display.setTextSize(3);display.setCursor(20,30);if(timerMin<10)display.print("0");display.print(timerMin);display.setCursor(55,30);display.print(":");display.setCursor(75,30);if(timerSec<10)display.print("0");display.print(timerSec);
    display.setTextSize(1);display.setCursor(10,55);display.print("Hold 2s:START 3s:EXIT");
  }
  display.display();
}

int pomoState=0; unsigned long pomoEnd=0;
void runPomodoro(){ 
  display.clearDisplay();
  
  // HOLD LOGIC
  if(digitalRead(BTN_OK)==LOW) {
    unsigned long p=millis(); while(digitalRead(BTN_OK)==LOW);
    if(millis()-p > 3000) { currentState=FACE; sndBack(); return; }
    else if (millis()-p > 1500 && pomoState==0) {
      pomoState=1; pomoEnd=millis()+(20*60*1000); sndClick();
    }
  }

  if(pomoState==0){
    display.setTextSize(2);display.setCursor(20,10);display.print("POMODORO");
    display.setTextSize(1);display.setCursor(10,40);display.print("Hold 2s to Start");
  } else {
    long rem=(long)(pomoEnd-millis())/1000;
    if(rem<=0){sndAlarm();if(pomoState==1){pomoState=2;pomoEnd=millis()+(5*60*1000);}else{pomoState=0;}} 
    display.setTextSize(2);display.setCursor(10,0);if(pomoState==1)display.print(" STUDY ");else display.print(" CHILL "); 
    int m=rem/60;int s=rem%60;display.setTextSize(4);display.setCursor(5,30);if(m<10)display.print("0");display.print(m);display.print(":");if(s<10)display.print("0");display.print(s);
  } 
  display.display(); 
}

// ==================================================
// 2. GAMES (PHYSICS 2.0)
// ==================================================
// PONG
void runPongGame() {
  ballX += ballDX; ballY += ballDY;
  
  // Easy Paddle Control
  if(digitalRead(BTN_UP)==LOW) paddleY -= 3; 
  if(digitalRead(BTN_DOWN)==LOW) paddleY += 3;
  if(paddleY < 0) paddleY = 0; if(paddleY > 48) paddleY = 48;

  // AI Tracking (Intentionally imperfect)
  if(ballX > 64 && ballDX > 0) {
    if(ballY > cpuY + 8) cpuY += 1.5; // Slower than ball
    else if(ballY < cpuY + 8) cpuY -= 1.5;
  }
  if(cpuY < 0) cpuY = 0; if(cpuY > 48) cpuY = 48;

  // Physics Bounds
  if(ballY <= 0 || ballY >= 63) { ballDY = -ballDY; sndClick(); } // Wall Hit

  // Collision - Player
  if (ballX < 6 && ballX > 2 && ballY >= paddleY && ballY <= paddleY + 16) {
    ballDX = abs(ballDX) * 1.05; // Speed up slightly
    if (ballDX > 3) ballDX = 3;  // Cap speed
    sndPing();
  }

  // Collision - CPU
  if (ballX > 120 && ballX < 124 && ballY >= cpuY && ballY <= cpuY + 16) {
    ballDX = -abs(ballDX);
    sndPing();
  }

  // Scoring
  if(ballX < 0) { cScore++; ballX=64; ballY=32; ballDX=2; ballDY=1.5; sndDie(); }
  if(ballX > 128) { pScore++; ballX=64; ballY=32; ballDX=-2; ballDY=1.5; makeHappy(); }

  display.clearDisplay();
  for(int i=0;i<64;i+=4)display.drawPixel(64,i,WHITE);
  display.setTextSize(1);display.setCursor(50,0);display.print(pScore);display.setCursor(74,0);display.print(cScore);
  display.fillRect(2,paddleY,2,16,WHITE); // Player
  display.fillRect(124,cpuY,2,16,WHITE);   // CPU
  display.fillCircle((int)ballX,(int)ballY,2,WHITE);
  display.display();
}

// BRICK BREAKER (DX BALL)
float ballX2=64,ballY2=50,ballDX2=1.5,ballDY2=-1.5; int padX2=50,brickScore2=0; 
bool bricks[4][10],brickInit=false,brickOver=false;
void resetBricks(){for(int r=0;r<4;r++)for(int c=0;c<10;c++)bricks[r][c]=true;brickInit=true;ballX2=64;ballY2=50;ballDY2=-1.5;ballDX2=1.5;brickScore2=0;pUp.active=false;}

void runBrickGame(){ 
  if(!brickInit) resetBricks();
  
  if(brickOver){
    display.clearDisplay();display.setCursor(20,20);display.print("GAME OVER");display.display();
    if(digitalRead(BTN_OK)==LOW){brickOver=false;resetBricks();delay(500);}return;
  }
  
  // Paddle (Wider default)
  if(digitalRead(BTN_LEFT)==LOW) padX2-=4; 
  if(digitalRead(BTN_RIGHT)==LOW) padX2+=4;
  if(padX2<0) padX2=0; if(padX2>128-paddleWidth) padX2=128-paddleWidth;

  // Ball Physics
  float nextX = ballX2 + ballDX2;
  float nextY = ballY2 + ballDY2;
  
  // Wall Bounce
  if(nextX<=0 || nextX>=128) ballDX2 = -ballDX2;
  if(nextY<=0) ballDY2 = -ballDY2;
  if(nextY>64) { sndDie(); brickOver=true; }

  // Paddle Bounce (Predictive)
  if(nextY >= 58 && nextY <= 62 && nextX >= padX2 && nextX <= padX2+paddleWidth) {
    ballDY2 = -abs(ballDY2); // Always go up
    sndJump();
  }

  // Brick Logic
  int c = (nextX-4)/12; int r = (nextY-4)/6;
  if(r>=0 && r<4 && c>=0 && c<10 && bricks[r][c]) {
    bricks[r][c] = false;
    ballDY2 = -ballDY2;
    brickScore2 += 10;
    sndClick();
    
    // Random Powerup Drop (10% chance)
    if(random(0,10) == 0 && !pUp.active) {
      pUp.active = true; pUp.x = nextX; pUp.y = nextY; pUp.type = random(0,2);
    }
  }
  
  // Powerup Physics
  if(pUp.active) {
    pUp.y += 1.5;
    if(pUp.y > 64) pUp.active = false;
    // Catch Powerup
    if(pUp.y >= 58 && pUp.x >= padX2 && pUp.x <= padX2+paddleWidth) {
      if(pUp.type == 0) paddleWidth = 30; // Expand
      else paddleWidth = 14; // Shrink
      sndPing();
      pUp.active = false;
    }
  }
  
  ballX2 += ballDX2; ballY2 += ballDY2;
  
  // Level Complete Check
  bool allClear = true;
  for(int r=0;r<4;r++) for(int c=0;c<10;c++) if(bricks[r][c]) allClear = false;
  if(allClear) resetBricks(); // Infinite Levels

  display.clearDisplay();
  display.fillRect(padX2,60,paddleWidth,3,WHITE);
  display.fillCircle((int)ballX2,(int)ballY2,2,WHITE);
  
  if(pUp.active) {
    display.setCursor((int)pUp.x, (int)pUp.y);
    if(pUp.type==0) display.print("+"); else display.print("-");
  }
  
  for(int r=0;r<4;r++) for(int c=0;c<10;c++) if(bricks[r][c]) display.fillRect(4+(c*12)+1,4+(r*6)+1,10,4,WHITE);
  display.display(); 
}

// ==================================================
// 3. NETWORK APPS (ROBUST SSL)
// ==================================================
void fetchNews() {
  if (!wifiConnected) { newsTitle = "Offline"; return; }
  display.clearDisplay(); display.setCursor(10,20); display.print("Loading News..."); display.display();
  
  WiFiClientSecure client; 
  client.setInsecure(); // IGNORE CERTIFICATE ERRORS (CRITICAL FIX)
  client.setTimeout(10000); // 10s timeout
  
  HTTPClient http; 
  String url = "https://newsapi.org/v2/top-headlines?country=us&category=technology&pageSize=1&page=" + String(newsPage) + "&apiKey=" + String(NEWS_API_KEY);
  
  http.begin(client, url);
  http.addHeader("User-Agent", "Mozilla/5.0"); // FAKE BROWSER HEADER
  
  int code = http.GET();
  if (code == 200) { 
    String p = http.getString(); 
    JsonDocument d; deserializeJson(d, p); 
    if (d["totalResults"].as<int>() > 0) { 
      newsTitle = d["articles"][0]["title"].as<String>(); 
      newsSource = d["articles"][0]["source"]["name"].as<String>(); 
    } else { newsTitle = "No News Found"; } 
  } else { 
    newsTitle = "HTTP Error: " + String(code); 
  } 
  http.end(); newsScrollX = 128;
}

void checkRain() {
  if (!wifiConnected) return; 
  WiFiClientSecure c; c.setInsecure(); HTTPClient h; 
  h.begin(c, "https://api.openweathermap.org/data/2.5/weather?q=Dhaka,bd&units=metric&appid=" + String(OW_APIKEY));
  if(h.GET()==200) { 
    String p=h.getString(); JsonDocument d; deserializeJson(d,p);
    tempStr=String(d["main"]["temp"].as<float>(),1); 
    String w=d["weather"][0]["main"].as<String>(); weatherDesc=w;
    if(w.indexOf("Rain")!=-1 || w.indexOf("Thunder")!=-1){alertType="BAD WEATHER";alertInfo=w;currentState=ALERT_MODE;}
  } h.end();
}

// ==================================================
// 4. STANDARD FUNCTIONS (Keyboard, WiFi Scan, etc)
// ==================================================
// (These functions remain identical to previous working versions)
void runWiFiScanner() {
  display.clearDisplay();
  display.setTextSize(1); display.setCursor(0,0); display.print("NETWORKS:"); display.drawLine(0,9,128,9,WHITE);
  if (wifiCount == 0) {
    display.setCursor(10,20); display.print("No Networks");
    if(digitalRead(BTN_OK)==LOW) { currentState = MENU; delay(200); }
  } else {
    for(int i=0; i<4; i++) {
      int idx = wifiScroll + i;
      if(idx < wifiCount) {
        display.setCursor(10, 15 + (i*12)); if(i==0) display.print(">"); 
        display.setCursor(20, 15 + (i*12)); String ssid = WiFi.SSID(idx);
        if(ssid.length()>14) ssid = ssid.substring(0,14); display.print(ssid);
      }
    }
    if(digitalRead(BTN_DOWN)==LOW) { wifiScroll++; if(wifiScroll >= wifiCount) wifiScroll=0; sndClick(); delay(150); }
    if(digitalRead(BTN_UP)==LOW) { wifiScroll--; if(wifiScroll < 0) wifiScroll=wifiCount-1; sndClick(); delay(150); }
    if(digitalRead(BTN_OK)==LOW) {
      targetSSID = WiFi.SSID(wifiScroll); keyboardBuffer = ""; keyboardTarget = 1; 
      kbPage = 0; currentState = KEYBOARD; sndClick(); delay(500);
    }
  }
  display.display();
}

void startWiFiScan() {
  display.clearDisplay(); display.setCursor(10,20); display.print("Scanning..."); display.display();
  WiFi.mode(WIFI_STA); WiFi.disconnect();
  wifiCount = WiFi.scanNetworks(); wifiScroll = 0;
}

void runKeyboard() {
  display.clearDisplay();
  display.setTextSize(1); display.setCursor(0,0); 
  if(keyboardTarget == 1) display.print("PASS: "); else display.print("SAY: ");
  display.setCursor(100,0); if(kbPage==0) display.print("ABC"); else if(kbPage==1) display.print("abc"); else display.print("123");

  display.drawRect(0, 10, 128, 15, WHITE);
  display.setCursor(2, 14); 
  if(keyboardBuffer.length() > 18) display.print(keyboardBuffer.substring(keyboardBuffer.length()-18));
  else display.print(keyboardBuffer);
  if ((millis()/500)%2==0) display.print("_");

  if (digitalRead(BTN_UP)==LOW) { kbRow--; if(kbRow<0) kbRow=2; delay(150); }
  if (digitalRead(BTN_DOWN)==LOW) { kbRow++; if(kbRow>2) kbRow=0; delay(150); }
  if (digitalRead(BTN_LEFT)==LOW) { kbCol--; if(kbCol<0) kbCol=9; delay(150); }
  if (digitalRead(BTN_RIGHT)==LOW) { kbCol++; if(kbCol>9) kbCol=0; delay(150); }

  int startY = 30;
  for(int r=0; r<3; r++) {
    for(int c=0; c<10; c++) {
      int x = 4 + (c * 12); int y = startY + (r * 10);
      if(r == kbRow && c == kbCol) { display.fillRect(x-1, y-1, 10, 9, WHITE); display.setTextColor(BLACK); } 
      else { display.setTextColor(WHITE); }
      display.setCursor(x+2, y); display.print(keys[kbPage][r][c]);
    }
  }
  display.setTextColor(WHITE);

  if (digitalRead(BTN_OK) == LOW) {
    unsigned long p = millis(); while(digitalRead(BTN_OK) == LOW); 
    if (millis() - p > 600) {
      if(keyboardTarget == 1) { 
        display.clearDisplay(); display.setCursor(10,20); display.print("Connecting..."); display.display();
        WiFi.begin(targetSSID.c_str(), keyboardBuffer.c_str());
        int t=0; while(WiFi.status()!=WL_CONNECTED && t<20){ delay(500); t++; }
        if(WiFi.status()==WL_CONNECTED) {
          wifiConnected=true; preferences.putString("ssid", targetSSID); preferences.putString("pass", keyboardBuffer);
          configTime(21600, 0, "pool.ntp.org"); display.setCursor(10,40); display.print("Success!");
        } else { display.setCursor(10,40); display.print("Failed."); }
        display.display(); delay(1500); currentState = FACE;
      } else { 
        callGeminiAPI();
      }
    } else {
      sndType();
      String k = String(keys[kbPage][kbRow][kbCol]);
      if (k == "^") { kbPage++; if(kbPage > 2) kbPage = 0; } 
      else if (k == "<") { if (keyboardBuffer.length() > 0) keyboardBuffer.remove(keyboardBuffer.length()-1); } 
      else if (k == "_") { keyboardBuffer += " "; } 
      else { if (keyboardBuffer.length() < 50) keyboardBuffer += k; }
    }
  }
  display.display(); 
}

void runTherapySession() {
  display.clearDisplay();
  display.drawBitmap(0, 0, focused, 128, 64, WHITE); 
  display.fillRect(5, 35, 118, 28, BLACK); display.drawRect(5, 35, 118, 28, WHITE);
  display.setCursor(8, 38); display.setTextSize(1);
  if(aiResponse.length() > 18) { display.print(aiResponse.substring(0, 18)); display.setCursor(8, 48); display.print(aiResponse.substring(18, 36)); } else { display.print(aiResponse); }
  display.setCursor(30, 5); display.print("Hold OK to Reply"); display.display();
  
  if (digitalRead(BTN_DOWN) == LOW) { aiScrollY += 8; delay(50); } 
  if (digitalRead(BTN_UP) == LOW) { aiScrollY -= 8; if(aiScrollY < 0) aiScrollY = 0; delay(50); } 

  if (digitalRead(BTN_OK) == LOW) {
     unsigned long p=millis(); while(digitalRead(BTN_OK)==LOW);
     if(millis()-p > 1000) { // Hold to reply
        keyboardBuffer = ""; keyboardTarget = 0; kbPage = 0;
        currentState = KEYBOARD; returnState = THERAPY;
        sndClick();
     }
  }
}

void callGeminiAPI() {
  if (!wifiConnected) { aiResponse = "No Internet"; currentState = THERAPY; return; }
  currentState = AI_THINKING; display.clearDisplay(); display.setCursor(30, 30); display.print("Thinking..."); display.display();
  WiFiClientSecure client; client.setInsecure(); HTTPClient http;
  String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + String(GEMINI_API_KEY);
  String prompt = "User: " + keyboardBuffer + ". Reply in 10 words.";
  String json = "{\"contents\":[{\"parts\":[{\"text\":\"" + prompt + "\"}]}]}";
  http.begin(client, url); http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(json);
  if(httpCode == 200) {
    String p = http.getString(); DynamicJsonDocument doc(4096); deserializeJson(doc, p);
    if (doc["candidates"][0]["content"]["parts"][0]["text"]) {
      aiResponse = doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
      aiResponse.replace("*", ""); 
    } else { aiResponse = "Thinking..."; }
  } else { aiResponse = "Error: " + String(httpCode); }
  http.end(); aiScrollY = 0; currentState = THERAPY;
}

// --- OTHER ---
void runMenu() { display.invertDisplay(false); display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE); display.setCursor(0,0); display.print("User: "); display.println(userName); display.drawLine(0,9,128,9,WHITE); if(digitalRead(BTN_UP)==LOW) { menuIndex--; sndClick(); delay(150); } if(digitalRead(BTN_DOWN)==LOW) { menuIndex++; sndClick(); delay(150); } if(menuIndex < 0) menuIndex = menuLen-1; if(menuIndex >= menuLen) menuIndex = 0; int startY = 15; for (int i=0; i<4; i++) { int idx = (menuIndex + i) % menuLen; display.setCursor(10, startY + (i*12)); if(i==0) display.print(">"); else display.print(" "); display.print(menuItems[idx]); } display.display(); }
void runFaceEngine() { sensors_event_t a, g, temp; if(mpu.getEvent(&a, &g, &temp)) { float mag = sqrt(sq(a.acceleration.x) + sq(a.acceleration.y) + sq(a.acceleration.z)); if (mag > 13.0 || mag < 7.0) { lastMotionTime = millis(); if (isSleeping) { isSleeping=false; isExcited=true; emotionTimer=millis(); makeHappy(); currentEye=excited; } } if (isExcited && millis()-emotionTimer > 2000) { isExcited=false; currentEye=normal; } if (mag > 20.0) { if (shakeStartTime == 0) shakeStartTime = millis(); if (millis() - shakeStartTime > 4000) { if (!isAngry) { isAngry=true; emotionTimer=millis(); makeAngry(); currentEye=furious; } shakeStartTime = millis(); } else if (millis()-shakeStartTime > 1000) currentEye=disoriented; } else { shakeStartTime = 0; } if (isAngry && millis()-emotionTimer > 3000) { isAngry=false; currentEye=normal; } if (!isSleeping && !isAngry && !isExcited && (millis()-lastMotionTime > 30000)) { isSleeping=true; currentEye=sleepy; } if (!isSleeping && !isAngry && !isExcited && !shakeStartTime) { if (a.acceleration.y > 4.0) currentEye = look_down; else if (a.acceleration.y < -4.0) currentEye = look_up; else if (a.acceleration.x > 4.0) currentEye = look_left; else if (a.acceleration.x < -4.0) currentEye = look_right; else if (millis() - lastSaccade > random(3000, 8000)) { int rand = random(0, 100); if (rand < 50) currentEye = normal; else if (rand < 60) currentEye = focused; else if (rand < 70) currentEye = bored; else currentEye = blink; lastSaccade = millis(); } } } if (currentEye != furious && currentEye != disoriented && currentEye != sleepy) { if (millis() - lastBlink > random(2000, 6000)) { display.clearDisplay(); if(currentEye == look_up) display.drawBitmap(0,0, blink_up, 128,64, WHITE); else if(currentEye == look_down) display.drawBitmap(0,0, blink_down, 128,64, WHITE); else display.drawBitmap(0,0, blink, 128,64, WHITE); if(wifiConnected){display.drawLine(120, 8, 122, 8, WHITE); display.drawLine(118, 6, 124, 6, WHITE); display.drawLine(116, 4, 126, 4, WHITE);} display.display(); delay(150); lastBlink = millis(); } } display.clearDisplay(); display.drawBitmap(0, 0, currentEye, 128, 64, WHITE); if(wifiConnected){display.drawLine(120, 8, 122, 8, WHITE); display.drawLine(118, 6, 124, 6, WHITE); display.drawLine(116, 4, 126, 4, WHITE);} display.display(); }
void runNewsApp() { display.clearDisplay(); display.drawBitmap(0, 0, worried, 128, 64, WHITE); display.fillRect(0, 40, 128, 24, BLACK); display.drawLine(0, 40, 128, 40, WHITE); display.setTextSize(1); display.setCursor(0, 42); display.print("NEWS: "); display.print(newsSource); display.setCursor(newsScrollX, 52); display.print(newsTitle); newsScrollX -= 2; if (newsScrollX < - (int)(newsTitle.length() * 6)) newsScrollX = 128; display.display(); if(digitalRead(BTN_RIGHT)==LOW) { newsPage++; fetchNews(); delay(200); } if(digitalRead(BTN_LEFT)==LOW && newsPage>1) { newsPage--; fetchNews(); delay(200); } }
void checkEarthquake() { WiFiClientSecure c; c.setInsecure(); HTTPClient h; h.begin(c, "https://earthquake.usgs.gov/fdsnws/event/1/query?format=geojson&latitude=23.81&longitude=90.41&maxradiuskm=500&minmagnitude=2.0&limit=1"); if(h.GET()>0) { String p=h.getString(); JsonDocument d; deserializeJson(d,p); if (d["metadata"]["count"].as<int>() > 0) { float mag = d["features"][0]["properties"]["mag"]; alertType = "QUAKE " + String(mag, 1); currentState = ALERT_MODE; } } h.end(); }
void fetchJoke(){ if (!wifiConnected) { randomMsg = "No WiFi"; return; } display.clearDisplay(); display.setCursor(10,20); display.print("Loading..."); display.display(); WiFiClientSecure c; c.setInsecure(); HTTPClient h; h.begin(c, "https://v2.jokeapi.dev/joke/Programming?type=single&blacklistFlags=nsfw,religious,political,racist,sexist"); if(h.GET()>0){String p=h.getString(); JsonDocument d; deserializeJson(d,p); randomMsg=d["joke"].as<String>();} else { randomMsg = "Error"; } h.end(); }
void runClock(){ struct tm t; if(getLocalTime(&t)){char b[10],d[20];strftime(b,10,"%H:%M:%S",&t);strftime(d,20,"%d %b",&t);display.clearDisplay();display.setTextSize(2);display.setCursor(15,20);display.print(b);display.setTextSize(1);display.setCursor(30,45);display.print(d);display.display();} }
void runWeather(){ display.clearDisplay(); display.setTextSize(1); display.setCursor(0,0); display.print("DHAKA"); display.drawLine(0,9,128,9,WHITE); if(!wifiConnected){display.setCursor(0,20);display.print("Offline");}else{display.setTextSize(3);display.setCursor(10,20);display.print(tempStr);display.setTextSize(1);display.print("C");display.setCursor(0,55);display.print(weatherDesc);} display.display(); }
void runJoke(){ display.clearDisplay(); display.setCursor(0,0); display.print("Joke:"); display.drawLine(0,9,128,9,WHITE); display.setCursor(0,15); if(!wifiConnected)display.print("Connect WiFi");else display.print(randomMsg); display.display();}
void runAlert() { static bool inv = false; inv=!inv; display.invertDisplay(inv); display.clearDisplay(); display.drawBitmap(0, 0, scared, 128, 64, WHITE); display.fillRect(0, 15, 128, 34, BLACK); display.drawRect(0, 15, 128, 34, WHITE); display.setCursor(10, 20); display.setTextSize(2); display.print("WARNING!"); display.setCursor(10, 40); display.setTextSize(1); display.print(alertType); display.display(); sndAlarm(); }
int dinoY=45,dinoV=0,cactusX=128,dinoScore=0; bool dinoJump=false,dinoDead=false;
void runDinoGame() { static unsigned long lf=0;if(millis()-lf<30)return;lf=millis(); if(dinoDead){display.clearDisplay();display.setCursor(35,20);display.print("DIED");display.display();if(digitalRead(BTN_UP)==LOW){dinoDead=false;dinoScore=0;cactusX=128;delay(500);}return;} if(digitalRead(BTN_UP)==LOW&&!dinoJump){dinoV=-9;dinoJump=true;sndJump();} dinoY+=dinoV;if(dinoY<45)dinoV+=2;else{dinoY=45;dinoV=0;dinoJump=false;} cactusX-=6;if(cactusX<-10){cactusX=128+random(0,60);dinoScore++;sndClick();} if(cactusX<20&&cactusX>5&&dinoY>35){sndDie();dinoDead=true;} display.clearDisplay();display.setCursor(0,0);display.print(dinoScore);display.drawLine(0,56,128,56,WHITE);display.fillRect(10,dinoY,10,10,WHITE);display.fillRect(cactusX,45,6,10,WHITE);display.display(); }
