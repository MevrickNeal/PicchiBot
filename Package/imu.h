// -----------------------------
// FILE: src/imu.h
// -----------------------------
#ifndef IMU_H
#define IMU_H


namespace IMU {
void begin();
void tick();
float eyeX();
float eyeY();
bool blinkNow();
}


#endif // IMU_H
