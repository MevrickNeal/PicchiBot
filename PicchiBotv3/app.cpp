// -----------------------------
#include "app.h"
#include "display.h"
#include "inputs.h"
#include "buzzer.h"
#include "imu.h"
#include "wifi_manager.h"
#include "bitmaps.h"


#include <queue>
#include <functional>


static std::queue<std::function<void()>> taskQueue;


void enqueue(std::function<void()> f) {
taskQueue.push(f);
}


void App::begin() {
Serial.begin(115200);
delay(50);


Wire.begin(PIN_SDA, PIN_SCL);
Wire.setClock(400000);


Buzzer::begin();
Display::begin();
IMU::begin();
Inputs::begin();


Display::playBootAnimation();


xTaskCreatePinnedToCore(WiFiManager::taskEntry, "WiFiTask", 8192, NULL, 1, NULL, 0);
}


void App::loop() {
Inputs::poll();
IMU::tick();
Buzzer::tick();


if (!taskQueue.empty()) {
auto f = taskQueue.front(); taskQueue.pop(); f();
}


Display::tick();


delay(10);
}
