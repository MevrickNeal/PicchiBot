#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// --- Pin Definitions (From Your Configuration) ---
// OLED Display
#define PIN_SDA 21
#define PIN_SCL 22

// Push Buttons (Using Left/Right as "Pet" sensors)
#define PIN_SW_OK 32
#define PIN_UP 26
#define PIN_DOWN 33
#define PIN_PET_LEFT 25   // Was PIN_LEFT
#define PIN_PET_RIGHT 27  // Was PIN_RIGHT

// Buzzer (MOVED TO A VALID OUTPUT PIN)
#define PIN_BUZZER 14  // GPIO 34 was INPUT_ONLY. This is a fix.
#define BUZZER_CHANNEL 0 // PWM Channel for the buzzer

// --- OLED Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- MPU6050 Sensor ---
Adafruit_MPU6050 mpu;

// --- App State Management ---
// We now have more screens. 0 is the default pet.
// 0 = Pet Mode, 1 = Main Menu, 2 = Weather, 3 = Animations, 4 = About
int currentScreen = 0; 
long lastButtonPress = 0;
long debounceDelay = 200; // Shorter delay for better menu feel

// --- Pet State Variables ---
int eyeX = 64; // Center X
int eyeY = 32; // Center Y
long lastBlinkTime = 0;
bool isBlinking = false;
long lastPetTime = 0;
bool isPet = false;


// --- Menu Variables ---
String menuItems[] = {"Weather", "Animations", "About", "Exit"}; // Added Exit
int menuItemCount = 4;
int currentItemIndex = 0;


// --- Animation Bitmaps (XBM) ---
// These are 1-bit images stored as arrays.

// Animation: Eye Opening (3 frames)
const unsigned char boot_anim_1[] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x0C, 0x3F, 0xFE, 0xFF, 0xFF, 0xFF, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFE, 0x3F, 0x0C, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const unsigned char boot_anim_2[] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF0, 0xE0, 0xC0, 0x80, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x07, 0x03, 0x03, 0x07, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 
	0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0xF0, 0xE0, 0xC0, 0x80, 0x80, 0xC0, 0xE0, 0xF0, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x03, 0x07, 0x0F, 0x1F, 0x1F, 0x0F, 0x07, 0x03, 0x00, 0x00, 0x00, 0x00
};
const unsigned char boot_anim_3[] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x80, 0xE0, 0xF8, 0xFC, 0xFE, 0xFF, 0xFF, 0xFE, 0xFC, 0xF8, 0xE0, 0x80, 0x00, 0x00, 
	0x00, 0x00, 0x07, 0x1F, 0x3F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x3F, 0x1F, 0x07, 0x00, 0x00, 
	0x00, 0xC0, 0xF8, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xF8, 0xC0, 0x00, 
	0x00, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3F, 0x00, 
	0x00, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3F, 0x00, 
	0x00, 0xC0, 0xF8, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xF8, 0xC0, 0x00, 
	0x00, 0x00, 0x07, 0x1F, 0x3F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x3F, 0x1F, 0x07, 0x00, 0x00, 
	0x00, 0x00, 0x80, 0xE0, 0xF8, 0xFC, 0xFE, 0xFF, 0xFF, 0xFE, 0xFC, 0xF8, 0xE0, 0x80, 0x00, 0x00
};

// Animation: Happy/Pet (1 frame, simple)
const unsigned char pet_anim[] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0xFE, 0xFF, 0xFF, 0x03, 0x03, 0x03, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xE0, 0xF8, 0xFC, 0xFE, 0xFF, 0xFB, 0xFB, 0xFF, 0xFE, 0xFC, 0xF8, 0xE0, 0x00, 0x00, 
	0x00, 0x00, 0x0F, 0x3F, 0x7F, 0xFF, 0xFF, 0xDF, 0xDF, 0xFF, 0xFF, 0x7F, 0x3F, 0x0F, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0xFE, 0xFF, 0xFF, 0x03, 0x03, 0x03, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0xE0, 0xF8, 0xFC, 0xFE, 0xFF, 0xFB, 0xFB, 0xFF, 0xFE, 0xFC, 0xF8, 0xE0, 0x00, 0x00, 
	0x00, 0x00, 0x0F, 0x3F, 0x7F, 0xFF, 0xFF, 0xDF, 0xDF, 0xFF, 0xFF, 0x7F, 0x3F, 0x0F, 0x00, 0x00
};


void setup() {
  Serial.begin(115200);

  // --- Initialize Button Pins ---
  pinMode(PIN_SW_OK, INPUT_PULLUP);
  pinMode(PIN_UP, INPUT_PULLUP);
  pinMode(PIN_DOWN, INPUT_PULLUP);
  pinMode(PIN_PET_LEFT, INPUT_PULLUP);
  pinMode(PIN_PET_RIGHT, INPUT_PULLUP);

  // --- Initialize Buzzer ---
  // Setup PWM channel 0 at 5000Hz with 8-bit resolution
  ledcSetup(BUZZER_CHANNEL, 5000, 8);
  // Attach the channel to the pin
  ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);

  // --- Initialize I2C ---
  Wire.begin(PIN_SDA, PIN_SCL);
  // *** FIX FOR GLITCHES? ***
  // Let's try setting the I2C clock to 400kHz (fast mode)
  // This can make the screen and sensor more responsive.
  Wire.setClock(400000); 

  // --- Initialize OLED ---
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  // --- Initialize MPU6050 ---
  if (!mpu.begin()) {
    Serial.println(F("MPU6050 not found"));
    for (;;);
  }
  Serial.println("MPU6050 Found!");
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // --- Play Boot Animation ---
  playBootAnimation();
  delay(500);

  // Start in Pet Mode
  currentScreen = 0;
  lastBlinkTime = millis();
}


void loop() {
  // This is the main "state machine"
  switch (currentScreen) {
    case 0:
      handlePetScreen();
      break;
    case 1:
      handleMenuScreen();
      break;
    case 2: // Weather
    case 3: // Animations
    case 4: // About
      handleSubMenu();
      break;
  }
}

// =======================================================
//  PRIMARY APP SCREENS
// =======================================================

void handlePetScreen() {
  // --- Check for Inputs ---
  
  // Check for "Menu" button press
  if (digitalRead(PIN_SW_OK) == LOW && (millis() - lastButtonPress) > debounceDelay) {
    currentScreen = 1; // Go to Main Menu
    currentItemIndex = 0; // Reset menu selection
    lastButtonPress = millis();
    beep(400, 50); // Beep to acknowledge
    return; // Exit this loop
  }

  // Check for "Pet" button press
  if ((digitalRead(PIN_PET_LEFT) == LOW || digitalRead(PIN_PET_RIGHT) == LOW) && (millis() - lastButtonPress) > debounceDelay) {
    lastButtonPress = millis();
    lastPetTime = millis(); // Remember when we were last pet
    isPet = true;
    playPetAnimation(); // Show the happy face
    return; // Exit this loop
  }

  // Stop "isPet" state after 2 seconds
  if (isPet && (millis() - lastPetTime > 2000)) {
    isPet = false;
  }

  // If we are not being pet, do the normal gazing/blinking
  if (!isPet) {
    // --- Get Sensor Data ---
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    // Map accelerometer data to screen coordinates
    // We use a small range (-5 to 5) to make it sensitive
    // The Y and X axes are flipped depending on mounting
    int targetX = map(a.acceleration.y, -5, 5, 20, 108); // (108 = 128-20)
    int targetY = map(a.acceleration.x, -5, 5, 12, 52);  // (52 = 64-12)

    // Smooth the eye movement (e.g., move 1/10th of the way to the target)
    eyeX += (targetX - eyeX) * 0.1;
    eyeY += (targetY - eyeY) * 0.1;

    // --- Draw the Screen ---
    display.clearDisplay();
    drawGazeEyes();
    display.display();
  }
}

void handleMenuScreen() {
  // This screen is for navigation
  
  // --- Read Inputs ---
  if ((millis() - lastButtonPress) > debounceDelay) {
    if (digitalRead(PIN_DOWN) == LOW) {
      currentItemIndex++;
      if (currentItemIndex >= menuItemCount) currentItemIndex = 0;
      lastButtonPress = millis();
    } 
    else if (digitalRead(PIN_UP) == LOW) {
      currentItemIndex--;
      if (currentItemIndex < 0) currentItemIndex = menuItemCount - 1;
      lastButtonPress = millis();
    } 
    else if (digitalRead(PIN_SW_OK) == LOW) {
      lastButtonPress = millis();
      beep(600, 50);

      // Check for "Exit"
      if (menuItems[currentItemIndex] == "Exit") {
        currentScreen = 0; // Go back to Pet Mode
      } else {
        // Go to the corresponding sub-screen
        // (Weather=0, Anim=1, About=2) -> (Screen 2, 3, 4)
        currentScreen = currentItemIndex + 2; 
      }
    }
  }

  // --- Draw Screen ---
  drawMenu();
}

void handleSubMenu() {
  // This screen is a placeholder for Weather, etc.
  
  // --- Read Inputs ---
  if (digitalRead(PIN_SW_OK) == LOW && (millis() - lastButtonPress) > debounceDelay) {
    currentScreen = 1; // Go back to Main Menu
    currentItemIndex = 0; // Reset menu selection
    lastButtonPress = millis();
    beep(400, 50);
  }

  // --- Draw Screen ---
  display.clearDisplay();
  display.setTextColor(WHITE);
  
  // Get the title from the menu item
  String title = menuItems[currentScreen - 2]; 
  title.toUpperCase();

  display.setTextSize(2);
  display.setCursor(20, 0);
  display.println(title);
  display.drawFastHLine(0, 20, display.width(), WHITE);
  
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.println(F("Feature coming soon..."));
  display.setCursor(15, 55);
  display.println(F("(Press OK to go back)"));
  display.display();
}


// =======================================================
//  DRAWING & ANIMATION FUNCTIONS
// =======================================================

void playBootAnimation() {
  display.clearDisplay();
  display.drawBitmap(0, 0, boot_anim_1, 128, 64, WHITE);
  display.display();
  beep(300, 100);
  delay(200);

  display.clearDisplay();
  display.drawBitmap(0, 0, boot_anim_2, 128, 64, WHITE);
  display.display();
  beep(400, 100);
  delay(200);

  display.clearDisplay();
  display.drawBitmap(0, 0, boot_anim_3, 128, 64, WHITE);
  display.display();
  beep(600, 100);
  delay(500);
}

void playPetAnimation() {
  display.clearDisplay();
  display.drawBitmap(0, 0, pet_anim, 128, 64, WHITE);
  display.display();
  beep(1000, 50);
  delay(20);
  beep(1200, 50);
}

void drawGazeEyes() {
  // Handle Blinking
  if (millis() - lastBlinkTime > 3000) { // Every 3 seconds
    isBlinking = true;
    lastBlinkTime = millis();
  }
  if (isBlinking && (millis() - lastBlinkTime > 150)) { // Blink duration
    isBlinking = false;
  }

  // --- Draw Left Eye ---
  int leftEyeX = 42; // X-center of left eye
  int leftEyeY = 32; // Y-center of left eye
  if (isBlinking) {
    // Draw a line for a blink
    display.drawFastHLine(leftEyeX - 18, leftEyeY, 36, WHITE);
  } else {
    // Draw the eye
    display.drawRoundRect(leftEyeX - 20, leftEyeY - 18, 40, 36, 12, WHITE); // Outer
    // Draw Pupil (mapped from the global eyeX, eyeY)
    int pupilX = map(eyeX, 0, 128, leftEyeX - 10, leftEyeX + 10);
    int pupilY = map(eyeY, 0, 64, leftEyeY - 8, leftEyeY + 8);
    display.fillCircle(pupilX, pupilY, 6, WHITE);
  }

  // --- Draw Right Eye ---
  int rightEyeX = 86; // X-center of right eye
  int rightEyeY = 32; // Y-center of right eye
  if (isBlinking) {
    display.drawFastHLine(rightEyeX - 18, rightEyeY, 36, WHITE);
  } else {
    display.drawRoundRect(rightEyeX - 20, rightEyeY - 18, 40, 36, 12, WHITE); // Outer
    int pupilX = map(eyeX, 0, 128, rightEyeX - 10, rightEyeX + 10);
    int pupilY = map(eyeY, 0, 64, rightEyeY - 8, rightEyeY + 8);
    display.fillCircle(pupilX, pupilY, 6, WHITE);
  }
}

// NEW improved menu drawing function
void drawMenu() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  for (int i = 0; i < menuItemCount; i++) {
    int yPos = 10 + (i * 13);
    if (i == currentItemIndex) {
      // Draw a filled rounded rectangle as the selector
      display.fillRoundRect(5, yPos - 3, 118, 12, 3, WHITE);
      display.setTextColor(BLACK); // Invert text
      display.setCursor(25, yPos);
      display.println(menuItems[i]);
      display.setTextColor(WHITE); // Set back to white
    } else {
      // Draw normal text
      display.setCursor(30, yPos);
      display.println(menuItems[i]);
    }
  }
  display.display();
}

void beep(int freq, int duration) {
  ledcWriteTone(BUZZER_CHANNEL, freq); // Play sound
  delay(duration);
  ledcWriteTone(BUZZER_CHANNEL, 0); // Stop sound
}
