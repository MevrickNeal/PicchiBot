// -----------------------------
// FILE: src/inputs.cpp
// -----------------------------
#include "inputs.h"
#include "config.h"
#include "buzzer.h"
#include "display.h"
#include "imu.h"
#include <Arduino.h>


static unsigned long lastButton = 0;
static const unsigned long debounce = 200;


void Inputs::begin() {
pinMode(PIN_SW_OK, INPUT_PULLUP);
pinMode(PIN_UP, INPUT_PULLUP);
pinMode(PIN_DOWN, INPUT_PULLUP);
pinMode(PIN_PET_LEFT, INPUT_PULLUP);
pinMode(PIN_PET_RIGHT, INPUT_PULLUP);
pinMode(PIN_TOUCH_1, INPUT_PULLUP);
pinMode(PIN_TOUCH_2, INPUT_PULLUP);
}


void Inputs::poll() {
if (digitalRead(PIN_SW_OK) == LOW && (millis() - lastButton) > debounce) {
lastButton = millis(); Buzzer::shortBeep();
}
if ((digitalRead(PIN_PET_LEFT) == LOW || digitalRead(PIN_PET_RIGHT) == LOW) && (millis() - lastButton) > debounce) {
lastButton = millis(); Buzzer::note(1000,80); Display::playBootAnimation();
}
}
