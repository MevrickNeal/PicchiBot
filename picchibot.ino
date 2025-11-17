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
#define BTN_LEFT   32
#define BTN_RIGHT  33
#define BTN_UP     25
#define BTN_DOWN   26
#define BTN_OK     27

unsigned long lastInteraction = 0;
unsigned long lastIdleAnim = 0;

// triple-click detector
int leftPressCount = 0;
unsigned long firstLeftPressTime = 0;

// ---------------- BUZZER ----------------
#define PIN_BUZZER 14

void beep(int f, int d) { tone(PIN_BUZZER, f, d); }
void chirp() { beep(1800, 50); } 
void happyChirp() { beep(1500, 80); beep(2200, 60); }
void purr() {
  beep(700, 30);
  delay(40);
  beep(900, 30);
  delay(40);
  beep(600, 30);
}
void song() {
  int melody[] = {1200,1400,1600,1400,1600,1800};
  for (int i=0;i<6;i++) beep(melody[i],120);
}

// ---------------- MPU ----------------
MPU6050 mpu(Wire);
float tiltX = 0, tiltY = 0;

// ---------------- STATE ----------------
bool asleep = false;

// =====================================================
// BOOT ANIMATION
// =====================================================
void bootAnimation() {
  eyes.close();
  display.display();
  delay(350);

  eyes.open();
  for (int i = 0; i < 20; i++) {
    eyes.update();
    delay(20);
  }

  happyChirp();
}

// =====================================================
// BUTTON HANDLING
// =====================================================
void handleLeftButton() {
  if (digitalRead(BTN_LEFT) == LOW) {
    lastInteraction = millis();
    purr();
    eyes.close();
    delay(120);
    eyes.open();

    // triple press detector
    if (firstLeftPressTime == 0) {
      firstLeftPressTime = millis();
      leftPressCount = 1;
    } else {
      leftPressCount++;
    }

    // If 3 presses within 2 seconds → sing!
    if (leftPressCount >= 3 && (millis() - firstLeftPressTime < 2000)) {
      song();
      leftPressCount = 0;
      firstLeftPressTime = 0;
    }

    // reset if too slow
    if (millis() - firstLeftPressTime > 2000) {
      leftPressCount = 1;
      firstLeftPressTime = millis();
    }
  }
}

void handleRightButton() {
  if (digitalRead(BTN_RIGHT) == LOW) {
    lastInteraction = millis();
    happyChirp();

    // Show temperature for 2 seconds
    float t = mpu.getTemp();

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(30, 20);
    display.println(String(t,1) + "C");
    display.display();

    delay(2000);
  }
}

void handleUpButton() {
  if (digitalRead(BTN_UP) == LOW) {
    lastInteraction = millis();
    chirp();
    eyes.anim_laugh();
  }
}

void handleDownButton() {
  if (digitalRead(BTN_DOWN) == LOW) {
    lastInteraction = millis();
    chirp();
    eyes.blink();
  }
}

void handleOkButton() {
  if (digitalRead(BTN_OK) == LOW) {
    lastInteraction = millis();
    chirp();
    // placeholder for menu
  }
}

// =====================================================
// IDLE CUTENESS (40 seconds no interaction)
// =====================================================
void idleAnimation() {
  if (millis() - lastInteraction > 40000) {
    if (millis() - lastIdleAnim > 3000) {
      eyes.anim_laugh();
      lastIdleAnim = millis();
    }
  }
}

// =====================================================
// GYRO → EYE POSITION
// =====================================================
void updateGyro() {
  mpu.update();

  tiltX = mpu.getAngleX();
  tiltY = mpu.getAngleY();

  int x = map(tiltY, -45, 45, 0, eyes.getScreenConstraint_X());
  int y = map(tiltX, -45, 45, 0, eyes.getScreenConstraint_Y());

  eyes.eyeLxNext = x;
  eyes.eyeLyNext = y;
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  pinMode(BTN_LEFT,  INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_UP,    INPUT_PULLUP);
  pinMode(BTN_DOWN,  INPUT_PULLUP);
  pinMode(BTN_OK,    INPUT_PULLUP);

  pinMode(PIN_BUZZER, OUTPUT);

  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  eyes.begin(128, 64, 60);
  eyes.setAutoblinker(true, 2, 4);
  eyes.setIdleMode(true, 3, 4);

  mpu.begin();
  mpu.calcOffsets(true, true);

  bootAnimation();
  lastInteraction = millis();
}

// =====================================================
// LOOP
// =====================================================
void loop() {

  handleLeftButton();
  handleRightButton();
  handleUpButton();
  handleDownButton();
  handleOkButton();

  idleAnimation();
  updateGyro();

  eyes.update();
}
