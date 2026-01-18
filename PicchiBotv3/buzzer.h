// -----------------------------
// FILE: src/buzzer.h
// -----------------------------
#ifndef BUZZER_H
#define BUZZER_H


namespace Buzzer {
void begin();
void tick();
void note(int freq, unsigned long duration);
void shortBeep();
}


#endif // BUZZER_H
