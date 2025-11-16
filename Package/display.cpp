// -----------------------------
// FILE: src/display.cpp
// -----------------------------
#include "display.h"
#include "config.h"
#include "bitmaps.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


static Adafruit_SSD1306 display(128, 64, &Wire, -1);


static unsigned long animLast = 0;
static const uint16_t ANIM_FRAME_MS = 160;
static int animIndex = 0;
static bool inBoot = true;
static unsigned long bootStart = 0;


void Display::begin() {
if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
Serial.println("SSD1306 init failed");
for(;;);
}
display.clearDisplay();
display.setTextSize(1);
display.setTextColor(WHITE);
}


void Display::playBootAnimation() {
inBoot = true; animIndex = 0; bootStart = millis(); animLast = millis();
}

void Display::tick() {
if (inBoot) {
if (millis() - animLast >= ANIM_FRAME_MS) {
animLast = millis();
display.clearDisplay();
if (animIndex == 0) display.drawBitmap(0,0, boot_anim_1, 128,64, WHITE);
else if (animIndex == 1) display.drawBitmap(0,0, boot_anim_2, 128,64, WHITE);
else display.drawBitmap(0,0, boot_anim_3, 128,64, WHITE);
display.display();
animIndex = (animIndex + 1) % 3;
}
if (millis() - bootStart > 3000) {
inBoot = false; display.clearDisplay(); display.display();
}
} else {
static unsigned long lastPet = 0;
if (millis() - lastPet > 350) {
lastPet = millis();
display.clearDisplay();
display.drawBitmap(0,0, pet_anim, 128,64, WHITE);
display.display();
}
}
}


void Display::drawGaze(float eyeX, float eyeY, bool blink, bool connected, const String &ip) {
display.clearDisplay();
display.setTextSize(1);
display.setCursor(0,0);
display.print(connected ? ip : "AP: PicchiBot");


if (blink) {
display.drawFastHLine(24,32,80, WHITE);
} else {
display.drawRoundRect(22,14,40,36,12,WHITE);
display.drawRoundRect(66,14,40,36,12,WHITE);
int lpx = constrain((int)(22 + (eyeX * 8)), 22, 42);
int rpx = constrain((int)(66 + (eyeX * 8)), 66, 86);
display.fillCircle(lpx, (int)(32 + eyeY*6), 5, WHITE);
display.fillCircle(rpx, (int)(32 + eyeY*6), 5, WHITE);
}
display.display();
}
