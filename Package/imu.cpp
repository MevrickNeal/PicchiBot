// -----------------------------
// FILE: src/imu.cpp
// -----------------------------
#include "imu.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>


static Adafruit_MPU6050 mpu;
static float sx = 0, sy = 0;
static unsigned long lastBlink = 0;
static bool blinking = false;


void IMU::begin() {
if (!mpu.begin()) {
Serial.println("MPU init failed");
for(;;);
}
mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
mpu.setGyroRange(MPU6050_RANGE_500_DEG);
mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}


void IMU::tick() {
sensors_event_t a, g, tmp;
mpu.getEvent(&a, &g, &tmp);
float tx = constrain(a.acceleration.y / 5.0, -1.0, 1.0);
float ty = constrain(a.acceleration.x / 5.0, -1.0, 1.0);
sx += (tx - sx) * 0.12;
sy += (ty - sy) * 0.12;
if (millis() - lastBlink > 3000) { blinking = true; lastBlink = millis(); }
if (blinking && millis() - lastBlink > 150) blinking = false;
}


float IMU::eyeX() { return sx; }
float IMU::eyeY() { return sy; }
bool IMU::blinkNow() { return blinking; }
