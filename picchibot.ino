#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "FluxGarage_RoboEyes.h"
#include <MPU6050_light.h>

// ---------------- DISPLAY ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
RoboEyes<Adafruit_SSD1306> eyes(display);

// ---------------- INPUTS ----------------
#define BTN_LEFT   12
#define BTN_RIGHT  14
unsigned long lastInteraction = 0;

// ---------------- BUZZER ----------------
#define PIN_BUZZER 14
void beep(int freq, int dur) { tone(PIN_BUZZER, freq, dur); }
void tinyBeep() { beep(2000, 40); }
void petBeep() { beep(1400, 80); beep(1800, 80); }

// ---------------- MPU ----------------
MPU6050 mpu(Wire);
float xTilt = 0, yTilt = 0;

// ---------------- STATE ----------------
bool asleep = false;

// =====================================================
// BOOT ANIMATION
// =====================================================
void bootAnimation() {
  eyes.close();  
  display.display();
  delay(300);

  eyes.open();
  for (int i = 0; i < 25; i++) {
    eyes.update();
    delay(15);
  }

  beep(1200, 80);
}

// =====================================================
// HANDLE BUTTONS
// =====================================================
void handleButtons() {
  if (!digitalRead(BTN_LEFT)) {
    tinyBeep();
    eyes.anim_laugh();   // happy animation
    lastInteraction = millis();
  }

  if (!digitalRead(BTN_RIGHT)) {
    petBeep();
    eyes.blink();        // cute blink
    lastInteraction = millis();
  }
}

// =====================================================
// SLEEP MODE
// =====================================================
void checkSleep() {
  if (!asleep && millis() - lastInteraction > 180000) {  // 3 minutes
    eyes.close();
    asleep = true;
  }

  if (asleep) {
    // wake on button press
    if (!digitalRead(BTN_LEFT) || !digitalRead(BTN_RIGHT)) {
      asleep = false;
      eyes.open();
      beep(1000, 80);
    }
  }
}

// =====================================================
// GYRO → EYE MOVEMENT
// =====================================================
void updateGyro() {
  mpu.update();

  xTilt = mpu.getAngleX();  // -90 to +90
  yTilt = mpu.getAngleY();

  int posX = map(xTilt, -45, 45, 0, eyes.getScreenConstraint_X());
  int posY = map(yTilt, -45, 45, 0, eyes.getScreenConstraint_Y());

  eyes.setPosition(DEFAULT);   // fallback center
  eyes.eyeLxNext = posX;
  eyes.eyeLyNext = posY;
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);

  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  eyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 50);
  eyes.setAutoblinker(true, 2, 3);
  eyes.setIdleMode(true, 3, 4);

  // MPU init
  mpu.begin();
  mpu.calcOffsets(true, true);

  bootAnimation();
  lastInteraction = millis();
}

// =====================================================
// LOOP
// =====================================================
void loop() {

  handleButtons();
  updateGyro();
  checkSleep();

  if (!asleep) {
    eyes.update();
  } else {
    eyes.drawEyes();   // static closed eyes
  }
}
