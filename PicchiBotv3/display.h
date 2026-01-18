// -----------------------------
// FILE: src/display.h
// -----------------------------
#ifndef DISPLAY_H
#define DISPLAY_H


#include <Arduino.h>


namespace Display {
void begin();
void playBootAnimation();
void tick();
void drawGaze(float eyeX, float eyeY, bool blink, bool connected, const String &ip);
}


#endif // DISPLAY_H
