#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- Pin Definitions (From Your Configuration) ---
// OLED Display
#define PIN_SDA 21
#define PIN_SCL 22

// Push Buttons
#define PIN_SW_OK 32
#define PIN_UP 26
#define PIN_DOWN 33
#define PIN_LEFT 25
#define PIN_RIGHT 27

// Buzzer
#define PIN_BUZZER 34

// --- OLED Configuration ---
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1    // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Menu Variables ---
String menuItems[] = {"Weather", "Animations", "About"};
int menuItemCount = 3;
int currentItemIndex = 0; // Tracks the selected item
int currentScreen = 0;    // 0 = Main Menu, 1 = Weather, 2 = Animations, 3 = About

// --- Button Debounce Variables ---
long lastButtonPress = 0;
long debounceDelay = 250; // 250ms delay to prevent multiple presses

void setup() {
  Serial.begin(115200);

  // --- Initialize Button Pins ---
  // We use INPUT_PULLUP, which means the pin is HIGH by default.
  // When pressed (connected to GND), it will read LOW.
  pinMode(PIN_SW_OK, INPUT_PULLUP);
  pinMode(PIN_UP, INPUT_PULLUP);
  pinMode(PIN_DOWN, INPUT_PULLUP);
  pinMode(PIN_LEFT, INPUT_PULLUP);
  pinMode(PIN_RIGHT, INPUT_PULLUP);

  // --- Initialize OLED ---
  // We must set the I2C pins BEFORE calling display.begin()
  Wire.begin(PIN_SDA, PIN_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  // --- Boot Screen ---
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(10, 20);
  display.println(F("PicchiBot"));
  display.display();
  delay(2000);
}

void loop() {
  // The main loop continuously checks for button presses and draws the screen
  readButtons();
  drawScreen();
}

void readButtons() {
  // We check if enough time has passed since the last press
  if ((millis() - lastButtonPress) > debounceDelay) {

    // --- MAIN MENU SCREEN ---
    if (currentScreen == 0) {
      if (digitalRead(PIN_DOWN) == LOW) {
        currentItemIndex++; // Move down
        if (currentItemIndex >= menuItemCount) {
          currentItemIndex = 0; // Wrap around to the top
        }
        lastButtonPress = millis(); // Reset debounce timer
      } else if (digitalRead(PIN_UP) == LOW) {
        currentItemIndex--; // Move up
        if (currentItemIndex < 0) {
          currentItemIndex = menuItemCount - 1; // Wrap around to the bottom
        }
        lastButtonPress = millis(); // Reset debounce timer
      } else if (digitalRead(PIN_SW_OK) == LOW) {
        // When OK is pressed, we change the screen to the selected item
        currentScreen = currentItemIndex + 1; // (1 = Weather, 2 = Animations, 3 = About)
        lastButtonPress = millis();
      }
    }
    // --- SUB-MENU SCREENS ---
    else {
      // If we are on ANY sub-screen (Weather, About, etc.),
      // the "OK" button will act as a "Back" button
      if (digitalRead(PIN_SW_OK) == LOW) {
        currentScreen = 0; // Go back to the Main Menu
        lastButtonPress = millis();
      }
    }
  }
}

void drawScreen() {
  display.clearDisplay();
  display.setTextColor(WHITE);

  // The 'switch' statement is perfect for managing different screens
  switch (currentScreen) {
    case 0: // --- Draw Main Menu ---
      display.setTextSize(2);
      display.setCursor(20, 0);
      display.println(F("MAIN MENU"));
      display.drawFastHLine(0, 20, display.width(), WHITE);
      
      display.setTextSize(1);
      for (int i = 0; i < menuItemCount; i++) {
        if (i == currentItemIndex) {
          // Draw a selector (>) for the currently highlighted item
          display.setCursor(5, 30 + (i * 12));
          display.print(F("> "));
          display.print(menuItems[i]);
        } else {
          // Draw the other items
          display.setCursor(15, 30 + (i * 12));
          display.print(menuItems[i]);
        }
      }
      break;

    case 1: // --- Draw "Weather" Screen ---
      display.setTextSize(2);
      display.setCursor(25, 0);
      display.println(F("Weather"));
      display.drawFastHLine(0, 20, display.width(), WHITE);
      display.setTextSize(1);
      display.setCursor(0, 30);
      display.println(F("Weather feature coming"));
      display.setCursor(0, 40);
      display.println(F("soon..."));
      display.setCursor(35, 55);
      display.println(F("(Press OK to go back)"));
      break;

    case 2: // --- Draw "Animations" Screen ---
      display.setTextSize(2);
      display.setCursor(10, 0);
      display.println(F("Animations"));
      display.drawFastHLine(0, 20, display.width(), WHITE);
      display.setTextSize(1);
      display.setCursor(0, 30);
      display.println(F("Animation feature coming"));
      display.setCursor(0, 40);
      display.println(F("soon..."));
      display.setCursor(35, 55);
      display.println(F("(Press OK to go back)"));
      break;

    case 3: // --- Draw "About" Screen ---
      display.setTextSize(2);
      display.setCursor(40, 0);
      display.println(F("About"));
      display.drawFastHLine(0, 20, display.width(), WHITE);
      display.setTextSize(1);
      display.setCursor(0, 30);
      display.println(F("PicchiBot v1.0"));
      display.setCursor(0, 40);
      display.println(F("By: [Your Name]"));
      display.setCursor(35, 55);
      display.println(F("(Press OK to go back)"));
      break;
  }

  display.display(); // Push the completed frame to the OLED
}
