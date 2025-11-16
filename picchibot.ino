// =======================================================
//  PICCHIBOT - PART 3: WIFI & RTOS
// =======================================================
// This code adds a parallel FreeRTOS task to handle
// Wi-Fi connectivity and a web setup portal
// without freezing the main pet animations.
// =======================================================

// --- Core Arduino & Sensor Libraries ---
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// --- New Network Libraries ---
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Preferences.h> // For saving Wi-Fi creds

// --- Pin Definitions (From Your Configuration) ---
#define PIN_SDA 21
#define PIN_SCL 22
#define PIN_SW_OK 32
#define PIN_UP 26
#define PIN_DOWN 33
#define PIN_PET_LEFT 25
#define PIN_PET_RIGHT 27
#define PIN_BUZZER 14
#define BUZZER_CHANNEL 0

// --- OLED Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- MPU6050 Sensor ---
Adafruit_MPU6050 mpu;

// --- New Network Objects ---
AsyncWebServer server(80);
DNSServer dnsServer;
Preferences preferences;
TaskHandle_t hNetworkTask; // Handle for our network task

// --- Global State Variables ---
// These are marked 'volatile' because they are shared
// between the two parallel tasks (Core 0 and Core 1).
volatile bool isConnected = false;
volatile String ipAddress = "No IP";

// --- App State Management ---
int currentScreen = 0; // 0=Pet, 1=Menu, 2=Weather...
long lastButtonPress = 0;
long debounceDelay = 200;

// --- Pet State Variables ---
int eyeX = 64, eyeY = 32;
long lastBlinkTime = 0;
bool isBlinking = false;
long lastPetTime = 0;
bool isPet = false;

// --- Menu Variables ---
String menuItems[] = {"Weather", "Animations", "About", "Exit"};
int menuItemCount = 4;
int currentItemIndex = 0;


// --- Animation Bitmaps (XBM) ---
// (Same as before, truncated for brevity...
//  Make sure you copy/paste the FULL arrays from the previous code)
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
// *** END OF BITMAPS ***


// =======================================================
//  HTML & CSS FOR THE SETUP WEB PAGE
// =======================================================
// This is the HTML code for the setup page. It's stored in
// program memory to save RAM.
// =======================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
<title>PicchiBot Setup</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  html { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; display: inline-block; text-align: center; }
  body { max-width: 400px; margin: 0 auto; padding: 20px; background-color: #f4f4f4; }
  .card { background-color: white; box-shadow: 0 4px 8px 0 rgba(0,0,0,0.2); border-radius: 10px; padding: 25px; }
  h2 { color: #333; }
  label { display: block; text-align: left; margin-top: 15px; font-weight: bold; color: #555; }
  input[type=text], input[type=password] { width: calc(100% - 20px); padding: 10px; margin-top: 5px; border: 1px solid #ddd; border-radius: 5px; }
  input[type=submit] { background-color: #007bff; color: white; padding: 12px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; margin-top: 20px; width: 100%; }
  input[type=submit]:hover { background-color: #0056b3; }
  .spinner { margin: 20px auto; border: 4px solid #f3f3f3; border-top: 4px solid #007bff; border-radius: 50%; width: 30px; height: 30px; animation: spin 1s linear infinite; display: none; }
  @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
</style>
</head><body>
<div class="card">
  <h2>Welcome to PicchiBot!</h2>
  <p>Let's get your bot connected to the internet.</p>
  <form action="/save" method="POST" onsubmit="showSpinner()">
    <label for="ssid">Home Wi-Fi Name (SSID)</label>
    <input type="text" name="ssid" id="ssid" required>
    <label for="pass">Home Wi-Fi Password</label>
    <input type="password" name="pass" id="pass">
    <input type="submit" value="Save & Connect">
  </form>
  <div id="spinner" class="spinner"></div>
  <p style="margin-top: 20px; font-size: 12px; color: #888;">PicchiBot will restart after you click save.</p>
</div>
<script>
  function showSpinner() {
    document.getElementById('spinner').style.display = 'block';
  }
</script>
</body></html>
)rawliteral";


// =======================================================
//  CORE 0 - NETWORK TASK
// =======================================================
// This function runs in parallel on Core 0.
// It handles Wi-Fi connection and the setup web server.
// =======================================================
void taskNetwork(void *pvParameters) {
  Serial.println("Network Task started on Core 0");

  // Load saved credentials
  preferences.begin("picchi-creds", false);
  String ssid = preferences.getString("ssid", "");
  String pass = preferences.getString("pass", "");
  preferences.end();

  bool connectedAttempt = false;

  if (ssid.length() > 0) {
    Serial.print("Trying to connect to saved WiFi: ");
    Serial.println(ssid);
    ipAddress = "Connecting..."; // Show on OLED

    WiFi.begin(ssid.c_str(), pass.c_str());
    
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      vTaskDelay(500 / portTICK_PERIOD_MS); // Wait 0.5s
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi Connected!");
      isConnected = true;
      ipAddress = WiFi.localIP().toString();
      Serial.print("IP Address: ");
      Serial.println(ipAddress);
      
      // --- SUCCESS! ---
      // This is where we will start the *main* web server
      // for weather, notifications, etc.
      // For now, the task just idles.
      
    } else {
      Serial.println("\nFailed to connect to saved WiFi.");
      isConnected = false;
      connectedAttempt = true; // Flag that we tried and failed
    }
  }

  // --- SETUP PORTAL MODE ---
  // If we have no saved SSID, or if we tried and failed...
  // ...start the Access Point (AP).
  if (!isConnected) {
    Serial.println("Starting Setup Portal (AP Mode)...");
    ipAddress = "Setup Mode";
    
    // Start the "PicchiBot" AP
    WiFi.softAP("PicchiBot", "amishudhutomar");
    IPAddress apIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(apIP);

    // Start the Captive Portal
    // This makes the setup page pop up on phones
    dnsServer.start(53, "*", apIP);

    // --- Define Web Server Routes ---
    
    // This is the main setup page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send_P(200, "text/html", index_html);
    });

    // This is called when the form is submitted
    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
      Serial.println("Got /save request");
      String new_ssid;
      String new_pass;

      // Get the form data
      if (request->hasParam("ssid", true)) {
        new_ssid = request->getParam("ssid", true)->value();
      }
      if (request->hasParam("pass", true)) {
        new_pass = request->getParam("pass", true)->value();
      }

      Serial.print("Saving new creds: ");
      Serial.println(new_ssid);

      // Save to Preferences
      preferences.begin("picchi-creds", false);
      preferences.putString("ssid", new_ssid);
      preferences.putString("pass", new_pass);
      preferences.end();

      // Send a success page and restart the bot
      String response = "<html><body><h2>Credentials Saved!</h2><p>PicchiBot will now restart and connect to <b>" + new_ssid + "</b>.</p><p>Please reconnect your phone to your home Wi-Fi.</p></body></html>";
      request->send(200, "text/html", response);

      vTaskDelay(2000 / portTICK_PERIOD_MS);
      ESP.restart();
    });
    
    // For Captive Portal
    server.onNotFound([](AsyncWebServerRequest *request) {
      request->send_P(200, "text/html", index_html);
    });

    server.begin(); // Start the server
    Serial.println("Web server started.");
    
    // This loop is required for the captive portal
    while(true) {
      dnsServer.processNextRequest();
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
  }
  
  // If we got here, we are connected to home Wi-Fi.
  // The task will now just spin, but in the future
  // it will handle weather updates.
  for(;;) {
    vTaskDelay(10000 / portTICK_PERIOD_MS); // Check every 10s
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Lost Wi-Fi connection. Rebooting...");
      ESP.restart();
    }
  }
}


// =======================================================
//  SETUP()
// =======================================================
// This runs ONCE on Core 1.
// Its only job is to start the hardware and launch tasks.
// =======================================================
void setup() {
  Serial.begin(115200);

  // --- Initialize Hardware ---
  pinMode(PIN_SW_OK, INPUT_PULLUP);
  pinMode(PIN_UP, INPUT_PULLUP);
  pinMode(PIN_DOWN, INPUT_PULLUP);
  pinMode(PIN_PET_LEFT, INPUT_PULLUP);
  pinMode(PIN_PET_RIGHT, INPUT_PULLUP);

  ledcSetup(BUZZER_CHANNEL, 5000, 8);
  ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(400000); // Fast I2C

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  if (!mpu.begin()) {
    Serial.println(F("MPU6050 not found"));
    for (;;);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // --- Play Boot Animation ---
  playBootAnimation();
  delay(500);

  // --- START THE NETWORK TASK ---
  // This is the magic line. It creates our parallel task.
  xTaskCreatePinnedToCore(
      taskNetwork,      // 1. Function to run
      "Network Task",   // 2. Name
      10000,            // 3. Stack size (10k for Wi-Fi)
      NULL,             // 4. Parameters
      1,                // 5. Priority
      &hNetworkTask,    // 6. Task handle
      0                 // 7. Core (Core 0)
  );
  
  Serial.println("Setup finished. Starting UI loop on Core 1.");
  
  // Start in Pet Mode
  currentScreen = 0;
  lastBlinkTime = millis();
}

// =======================================================
//  LOOP() - CORE 1 - UI TASK
// =======================================================
// This loop runs continuously on Core 1.
// It is ONLY responsible for the fast, responsive UI.
// =======================================================
void loop() {
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
  // Add a tiny delay to be nice to the FreeRTOS scheduler
  vTaskDelay(10 / portTICK_PERIOD_MS); 
}

// =======================================================
//  UI SCREEN HANDLERS
// =======================================================
// (These are identical to Part 2)

void handlePetScreen() {
  if (digitalRead(PIN_SW_OK) == LOW && (millis() - lastButtonPress) > debounceDelay) {
    currentScreen = 1;
    currentItemIndex = 0;
    lastButtonPress = millis();
    beep(400, 50);
    return;
  }

  if ((digitalRead(PIN_PET_LEFT) == LOW || digitalRead(PIN_PET_RIGHT) == LOW) && (millis() - lastButtonPress) > debounceDelay) {
    lastButtonPress = millis();
    lastPetTime = millis();
    isPet = true;
    playPetAnimation();
    return;
  }

  if (isPet && (millis() - lastPetTime > 2000)) {
    isPet = false;
  }

  if (!isPet) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    int targetX = map(a.acceleration.y, -5, 5, 20, 108);
    int targetY = map(a.acceleration.x, -5, 5, 12, 52);

    eyeX += (targetX - eyeX) * 0.1;
    eyeY += (targetY - eyeY) * 0.1;

    display.clearDisplay();
    drawGazeEyes(); // This function is now modified
    display.display();
  }
}

void handleMenuScreen() {
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

      if (menuItems[currentItemIndex] == "Exit") {
        currentScreen = 0;
      } else {
        currentScreen = currentItemIndex + 2;
      }
    }
  }
  drawMenu();
}

void handleSubMenu() {
  if (digitalRead(PIN_SW_OK) == LOW && (millis() - lastButtonPress) > debounceDelay) {
    currentScreen = 1;
    currentItemIndex = 0;
    lastButtonPress = millis();
    beep(400, 50);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  
  String title = menuItems[currentScreen - 2]; 
  title.toUpperCase();

  display.setTextSize(2);
  display.setCursor(20, 0);
  display.println(title);
  display.drawFastHLine(0, 20, display.width(), WHITE);
  
  display.setTextSize(1);
  display.setCursor(0, 30);
  
  // *** NEW ***
  // Give WiFi status on the Weather page
  if (title == "WEATHER") {
    if (isConnected) {
      display.println("Connected!");
      display.println(ipAddress);
    } else {
      display.println("Connecting...");
      display.println("Or in Setup Mode.");
    }
  } else {
    display.println(F("Feature coming soon..."));
  }
  
  display.setCursor(15, 55);
  display.println(F("(Press OK to go back)"));
  display.display();
}


// =======================================================
//  DRAWING, ANIMATION, & SOUND FUNCTIONS
// =======================================================
// (Identical to Part 2, except for drawGazeEyes)

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
  // *** NEW: Draw Wi-Fi Status on Pet Screen ***
  display.setTextSize(1);
  display.setCursor(0, 0);
  if (isConnected) {
    display.print(ipAddress); // Show IP
  } else {
    display.print("AP: PicchiBot"); // Show AP Mode
  }
  // ********************************************

  // Handle Blinking
  if (millis() - lastBlinkTime > 3000) {
    isBlinking = true;
    lastBlinkTime = millis();
  }
  if (isBlinking && (millis() - lastBlinkTime > 150)) {
    isBlinking = false;
  }

  // --- Draw Left Eye ---
  int leftEyeX = 42;
  int leftEyeY = 32;
  if (isBlinking) {
    display.drawFastHLine(leftEyeX - 18, leftEyeY, 36, WHITE);
  } else {
    display.drawRoundRect(leftEyeX - 20, leftEyeY - 18, 40, 36, 12, WHITE);
    int pupilX = map(eyeX, 0, 128, leftEyeX - 10, leftEyeX + 10);
    int pupilY = map(eyeY, 0, 64, leftEyeY - 8, leftEyeY + 8);
    display.fillCircle(pupilX, pupilY, 6, WHITE);
  }

  // --- Draw Right Eye ---
  int rightEyeX = 86;
  int rightEyeY = 32;
  if (isBlinking) {
    display.drawFastHLine(rightEyeX - 18, rightEyeY, 36, WHITE);
  } else {
    display.drawRoundRect(rightEyeX - 20, rightEyeY - 18, 40, 36, 12, WHITE);
    int pupilX = map(eyeX, 0, 128, rightEyeX - 10, rightEyeX + 10);
    int pupilY = map(eyeY, 0, 64, rightEyeY - 8, rightEyeY + 8);
    display.fillCircle(pupilX, pupilY, 6, WHITE);
  }
}

void drawMenu() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  for (int i = 0; i < menuItemCount; i++) {
    int yPos = 10 + (i * 13);
    if (i == currentItemIndex) {
      display.fillRoundRect(5, yPos - 3, 118, 12, 3, WHITE);
      display.setTextColor(BLACK);
      display.setCursor(25, yPos);
      display.println(menuItems[i]);
      display.setTextColor(WHITE);
    } else {
      display.setCursor(30, yPos);
      display.println(menuItems[i]);
    }
  }
  display.display();
}

void beep(int freq, int duration) {
  ledcWriteTone(BUZZER_CHANNEL, freq);
  delay(duration);
  ledcWriteTone(BUZZER_CHANNEL, 0);
}
