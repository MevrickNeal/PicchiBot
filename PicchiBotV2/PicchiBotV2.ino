/***************************************************
  PicchiBot v2 - Advanced OLED Eye Firmware
  
  FEATURES:
  - Context-Aware Blinking (Uses Blink Up/Down based on look direction)
  - Saccadic Movement (Randomly looks around)
  - Simulated WiFi Boot Sequence
  - Simulated Weather Data (Ready for IoT integration)
  - Huge Expression Library support
  
  HARDWARE:
  - ESP32 Dev Kit v1
  - 0.96" I2C OLED Display (SSD1306 driver)
****************************************************/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- 1. INCLUDE ALL LOCAL HEADER FILES ---
// Make sure all these .h files are in the same folder as this sketch
#include "Normal.h"
#include "Happy.h"
#include "Sad.h"
#include "Sleepy.h"
#include "Angry.h"
#include "Furious.h"
#include "Worried.h"
#include "Scared.h"
#include "Surprised.h"
#include "Disoriented.h"
#include "Despair.h"
#include "Alert.h"
#include "Bored.h"
#include "Excited.h"
#include "Focused.h"

// Directional & Blinking Headers
#include "Look_Up.h"      // Ensure filename matches exactly (spaces vs underscores)
#include "Look_Down.h"
#include "Look_Left.h"
#include "Look_Right.h"
#include "Blink.h"
#include "Blink_Up.h"
#include "Blink_Down.h"
#include "Wink_Left.h"
#include "Wink_Right.h"

// --- 2. DISPLAY SETTINGS ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 
#define SCREEN_ADDRESS 0x3C // Common I2C address

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- 3. STATE VARIABLES ---
// Pointers to track what the eye is currently doing
const unsigned char* currentEyeState = normal; 
const unsigned char* lastActiveState = normal; // Remembers state before blinking

// Timers
unsigned long lastBlinkTime = 0;
unsigned long blinkInterval = 3000; // Initial blink delay
unsigned long lastMoveTime = 0;
unsigned long moveInterval = 2000;
unsigned long lastWeatherUpdate = 0;

// Dummy Weather Data
int temperature = 25;
int humidity = 60;

void setup() {
  Serial.begin(115200);

  // Initialize Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  // Run the Boot Sequence
  bootSequence();
}

void loop() {
  // 1. Handle Blinking (Highest Priority)
  handleBlinking();

  // 2. Handle Looking Around (Saccadic movement)
  handleEyeMovement();

  // 3. Update Weather Data (Simulated)
  if (millis() - lastWeatherUpdate > 5000) {
    updateWeather();
    lastWeatherUpdate = millis();
  }
}

// --- CORE FUNCTIONS ---

void drawEye(const unsigned char* bitmap) {
  display.clearDisplay();
  
  // Draw the eye bitmap
  display.drawBitmap(0, 0, bitmap, 128, 64, WHITE);
  
  // Draw the UI Overlay (Weather)
  drawOverlay();
  
  display.display();
  
  // Update global tracker (unless it's a blink, handled separately)
  if (bitmap != blink && bitmap != blink_up && bitmap != blink_down) {
    currentEyeState = bitmap;
  }
}

void drawOverlay() {
  // Small text at the bottom for "Dr. Water IoT" style data
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  // Bottom Left: Temp
  display.setCursor(0, 56);
  display.print("T:");
  display.print(temperature);
  display.print("C");

  // Bottom Right: Wifi Icon (Fake) & Humidity
  display.setCursor(90, 56);
  display.print("H:");
  display.print(humidity);
  display.print("%");
  
  // Tiny WiFi bars
  display.drawLine(80, 63, 80, 61, WHITE);
  display.drawLine(82, 63, 82, 59, WHITE);
  display.drawLine(84, 63, 84, 57, WHITE);
}

// --- BEHAVIOR LOGIC ---

void handleBlinking() {
  if (millis() - lastBlinkTime > blinkInterval) {
    // 1. Determine WHICH blink to use based on where we are looking
    const unsigned char* blinkFrame = blink; // Default
    
    if (currentEyeState == look_up) {
      blinkFrame = blink_up;
    } else if (currentEyeState == look_down) {
      blinkFrame = blink_down;
    }

    // 2. Perform Blink Animation
    // Draw closed eye
    display.clearDisplay();
    display.drawBitmap(0, 0, blinkFrame, 128, 64, WHITE);
    drawOverlay();
    display.display();
    
    delay(150); // Keep eyes closed for 150ms

    // 3. Open eyes (Restore previous state)
    drawEye(currentEyeState);

    // 4. Reset Timer & Randomize next blink time
    lastBlinkTime = millis();
    blinkInterval = random(2000, 6000); // Blink every 2 to 6 seconds
  }
}

void handleEyeMovement() {
  // Only move eyes if enough time has passed
  if (millis() - lastMoveTime > moveInterval) {
    
    // Simple probability logic for natural movement
    int randAction = random(0, 10);

    if (randAction < 4) {
      // 40% chance to return to NORMAL
      currentEyeState = normal;
    } 
    else if (randAction < 8) {
      // 40% chance to Look Around
      int dir = random(0, 4);
      switch(dir) {
        case 0: currentEyeState = look_right; break;
        case 1: currentEyeState = look_left; break;
        case 2: currentEyeState = look_up; break;
        case 3: currentEyeState = look_down; break;
      }
    }
    else {
      // 20% chance to show an emotion (Demo purposes)
      int emo = random(0, 5);
      switch(emo) {
        case 0: currentEyeState = happy; break;
        case 1: currentEyeState = bored; break;
        case 2: currentEyeState = focused; break;
        case 3: currentEyeState = excited; break;
        case 4: currentEyeState = wink_left; delay(500); break; // Quick wink
      }
    }

    drawEye(currentEyeState);
    lastMoveTime = millis();
    moveInterval = random(1500, 4000); // Hold gaze for 1.5 to 4 seconds
  }
}

void bootSequence() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  // Step 1: Connecting
  display.setCursor(10, 20);
  display.println("Connecting to");
  display.setCursor(10, 30);
  display.println("PicchiBot_Net...");
  display.display();
  delay(1500);

  // Step 2: Connected
  display.clearDisplay();
  display.setCursor(10, 25);
  display.println("WiFi Connected!");
  display.setCursor(10, 35);
  display.println("IP: 192.168.1.105");
  display.display();
  delay(1500);

  // Step 3: Wake up
  drawEye(sleepy); // Start sleepy
  delay(2000);
  drawEye(blink);  // Blink awake
  delay(200);
  drawEye(normal); // Ready
}

void updateWeather() {
  // Simulating sensor fluctuation
  temperature = 24 + random(0, 3); 
  humidity = 58 + random(0, 5);
}
