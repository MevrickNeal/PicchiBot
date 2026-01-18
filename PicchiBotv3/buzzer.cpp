// -----------------------------
// FILE: src/buzzer.cpp
// -----------------------------
#include "buzzer.h"
#include "config.h"
#include <Arduino.h>


static unsigned long toneEnd = 0;
static int toneFreq = 0;


void Buzzer::begin() {
ledcSetup(BUZZER_CHANNEL, 2000, 8);
ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);
}


void Buzzer::note(int freq, unsigned long duration) {
if (freq <= 0) return;
toneFreq = freq;
toneEnd = millis() + duration;
ledcWriteTone(BUZZER_CHANNEL, freq);
}


void Buzzer::tick() {
if (toneFreq != 0 && millis() >= toneEnd) {
ledcWriteTone(BUZZER_CHANNEL, 0);
toneFreq = 0;
}
}


void Buzzer::shortBeep() {
Buzzer::note(900, 70);
}
