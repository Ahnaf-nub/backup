#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFiManager.h>

const char* API_KEY = "AIzaSyC9sSO-6aPBSa8R2w8cROpDYYJidbquTSs";  // Replace with your Gemini API key

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define MAX_TOKENS 150

// Buttons
#define BUTTON_PIN_SELECT 23
#define BUTTON_PIN_SEND 12
bool buttonHeldSelect = false;
bool buttonHeldSend = false;
bool responseDisplayed = false;

// Objects
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
Adafruit_MPU6050 mpu;

// Chat-specific variables
char keyboard[4][10] = {
    {'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P'},
    {'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '?'},
    {'Z', 'X', 'C', 'V', 'B', 'N', 'M', ' ', '.', '/'},
    {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'}
};

String inputText = "";
String aiResponse = "";
int cursorX = 0, cursorY = 0;
int previousX = 0, previousY = 0;  
int scrollOffset = 0;

// Story-specific variables
String currentText = "";
String option1 = "";
String option2 = "";
int selectedOption = 0;
String storyContext = "";

// Miscellaneous
unsigned long lastMoveTime = 0;
const int moveDelay = 200;  // Delay for cursor movement

void initWiFi() {
  WiFiManager wifiManager;
  wifiManager.autoConnect("Gemini32");
  display.clearDisplay();
  display.setCursor(0, 0);
  if (WiFi.status() != WL_CONNECTED) {
    display.print("To connect to WiFi Go to ");
    display.setCursor(0, 10);
    display.println(WiFi.localIP());
    display.display();
  } else {
    display.println("Connected to WiFi");
  }
}

void displayAIResponse() {
  display.clearDisplay();

  int textHeight = ((aiResponse.length() / 21) + 1) * 8; // Estimate height based on length and line wrap
  int maxScrollOffset = max(0, textHeight - SCREEN_HEIGHT);

  // Constrain scrollOffset within limits
  scrollOffset = constrain(scrollOffset, 0, maxScrollOffset);

  // Set cursor for scrolling text
  display.setCursor(0, -scrollOffset);  // Adjust starting position

  display.setTextWrap(true);
  display.print("AI: ");
  display.println(aiResponse);

  display.display();
}

void sendToGeminiAPI(String userInput) {
  HTTPClient https;

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print("Cooking...");
  display.display();
  display.setTextSize(1);

  if (https.begin("https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + (String)API_KEY)) {
    https.addHeader("Content-Type", "application/json");

    String payload = "{\"prompt\": {\"text\": \"" + userInput + "\"},\"max_tokens\": " + String(MAX_TOKENS) + "}";
    int httpCode = https.POST(payload);
    if (httpCode == HTTP_CODE_OK) {
      String response = https.getString();
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, response);

      aiResponse = doc["choices"][0]["text"].as<String>();
      scrollOffset = 0;  // Reset scroll offset for new response

      displayAIResponse();
      responseDisplayed = true;
      Serial.println("Gemini AI Response: " + aiResponse);
    } else {
      Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }
    https.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
}

void displayStory() {
  display.clearDisplay();
  display.setTextWrap(true);
  display.println(currentText);
  display.println();

  // Highlight option 1 if selected
  if (selectedOption == 0) {
    display.setTextColor(SH110X_BLACK, SH110X_WHITE); // Invert colors for highlight
  } else {
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  }
  display.println("1. " + option1);

  // Highlight option 2 if selected
  if (selectedOption == 1) {
    display.setTextColor(SH110X_BLACK, SH110X_WHITE); // Invert colors for highlight
  } else {
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  }
  display.println("2. " + option2);

  display.setTextColor(SH110X_WHITE, SH110X_BLACK); // Reset text color
  display.display();
}

void sendStoryToGeminiAPI(String storyInput) {
  HTTPClient https;

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print("Cooking...");
  display.display();
  display.setTextSize(1);

  if (https.begin("https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + (String)API_KEY)) {
    https.addHeader("Content-Type", "application/json");

    // Construct the prompt
    String prompt = "Continue the story based on the following context and the player's choice. Provide the next part of the story and two options for the player to choose from, in the following JSON format:\n\n{\n  \"currentText\": \"...\",\n  \"option1\": \"...\",\n  \"option2\": \"...\"\n}\n\nStory Context:\n" + storyInput;

    String payload = "{\"prompt\": {\"text\": \"" + prompt + "\"},\"max_tokens\": " + String(MAX_TOKENS) + "}";
    int httpCode = https.POST(payload);

    if (httpCode == HTTP_CODE_OK) {
      String response = https.getString();
      DynamicJsonDocument doc(4096);
      deserializeJson(doc, response);

      String aiResponse = doc["choices"][0]["text"].as<String>();

      // Parse the AI response as JSON
      DynamicJsonDocument storyDoc(4096);
      DeserializationError error = deserializeJson(storyDoc, aiResponse);

      if (!error) {
        currentText = storyDoc["currentText"].as<String>();
        option1 = storyDoc["option1"].as<String>();
        option2 = storyDoc["option2"].as<String>();
      } else {
        // Handle parsing error
        currentText = "Error parsing story.";
        option1 = "Try again";
        option2 = "";
      }

      // Reset selectedOption
      selectedOption = 0;

      displayStory();
      responseDisplayed = true;
      Serial.println("Gemini AI Story Response: " + aiResponse);
    } else {
      Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }
    https.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
}

void drawKeyboard() {
  int xOffset = 0;
  int yOffset = 20;

  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 10; col++) {
      display.setCursor(xOffset + col * 12, yOffset + row * 12);
      display.print(keyboard[row][col]);
    }
  }
  display.display(); // Ensure display refresh after drawing the keyboard
}

void drawCursor(int x, int y) {
  int cursorXPos = x * 12;
  int cursorYPos = 20 + y * 12;
  display.drawRect(cursorXPos, cursorYPos, 12, 12, SH110X_WHITE);
}

void clearCursor(int x, int y) {
  int cursorXPos = x * 12;
  int cursorYPos = 20 + y * 12;

  display.fillRect(cursorXPos, cursorYPos, 12, 12, SH110X_BLACK);
  display.setCursor(cursorXPos, cursorYPos);
  display.print(keyboard[y][x]);
}

void updateCursorPosition(int x, int y) {
  clearCursor(previousX, previousY);
  drawCursor(x, y);
  display.display();
  previousX = x;
  previousY = y;
}

void handleMPUInput(sensors_event_t a, sensors_event_t g) {
  const float threshold = 3.5;
  static unsigned long lastMoveTime = 0;
  const int moveDelay = 200;

  if (responseDisplayed) {
    // Track if the scrollOffset changes to trigger a display update
    int previousOffset = scrollOffset;

    // Calculate the angle of rotation around the Z-axis
    float angle = atan2(g.gyro.y, g.gyro.x) * 180 / PI;

    if (angle > 15) {
      scrollOffset += 5;
    } else if (angle < -15) {
      scrollOffset -= 5;
    }

    if (scrollOffset != previousOffset) {
      displayAIResponse();
      delay(5);
    }
    return;
  }

  // Handle cursor movement
  if (millis() - lastMoveTime > moveDelay) {
    if (a.acceleration.x > threshold) {
      cursorY++;
      if (cursorY >= 4) cursorY = 3;
    } else if (a.acceleration.x < -threshold) {
      cursorY--;
      if (cursorY < 0) cursorY = 0;
    }
    if (a.acceleration.y > threshold) {
      cursorX--;
      if (cursorX < 0) cursorX = 0;
    } else if (a.acceleration.y < -threshold) {
      cursorX++;
      if (cursorX >= 10) cursorX = 9;
    }

    if (cursorX != previousX || cursorY != previousY) {
      updateCursorPosition(cursorX, cursorY);
      lastMoveTime = millis();
    }
  }
}

void selectKey() {
  char selectedKey = keyboard[cursorY][cursorX];
  inputText += selectedKey;

  display.fillRect(0, 0, SCREEN_WIDTH, 16, SH110X_BLACK);
  display.setCursor(0, 0);
  display.print("You: ");
  display.print(inputText);
  display.display();
}

void checkButtons() {
  bool selectButtonState = digitalRead(BUTTON_PIN_SELECT);
  bool sendButtonState = digitalRead(BUTTON_PIN_SEND);

  if (responseDisplayed && (selectButtonState == HIGH || sendButtonState == HIGH)) {
    // Clear response if a button is pressed after response is shown
    responseDisplayed = false;
    inputText = "";
    display.clearDisplay();
    drawKeyboard();
    drawCursor(cursorX, cursorY);
  }

  if (selectButtonState == HIGH && !buttonHeldSelect) {
    buttonHeldSelect = true;
    selectKey();
  } else if (selectButtonState == LOW) {
    buttonHeldSelect = false;
  }

  if (sendButtonState == HIGH && !buttonHeldSend) {
    buttonHeldSend = true;

    if (inputText.length() > 0) {
      sendToGeminiAPI(inputText);
      inputText = "";
    }
  } else if (sendButtonState == LOW) {
    buttonHeldSend = false;
  }
}

void handleStoryButtons() {
  if (digitalRead(BUTTON_PIN_SELECT) == LOW) {
    selectedOption = (selectedOption == 0) ? 1 : 0; // Toggle between 0 and 1
    displayStory();
    delay(200); // Debounce delay
  }

  // Confirm selection on SEND button press
  if (digitalRead(BUTTON_PIN_SEND) == LOW) {
    // Append the selected option to the story context
    String userChoice = (selectedOption == 0) ? option1 : option2;

    storyContext += "\nPlayer chooses: " + userChoice + "\n";

    sendStoryToGeminiAPI(storyContext);

    delay(200); // Debounce delay
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(BUTTON_PIN_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_PIN_SEND, INPUT_PULLUP);

  if (!display.begin(0x3C, true)) {
    Serial.println("Display initialization failed!");
    while (1);
  }

  if (!mpu.begin()) {
    Serial.println("MPU6050 initialization failed!");
    while (1);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  initWiFi();

  // Initialize story
  storyContext = "You wake up in a dark forest. What do you do?";
  sendStoryToGeminiAPI(storyContext);
}

int push() {
  int count = 0;
p:
  int t = 0;
  if (digitalRead(BUTTON_PIN_SELECT) == 0) {
    while (digitalRead(BUTTON_PIN_SELECT) == 0) {
      delay(1);
      t++;
    }
    if (t > 15) {
      count++;
      t = 0;
      while (digitalRead(BUTTON_PIN_SELECT) == 1) {
        delay(1);
        t++;
        if (t > 1000) break;
      }
      if (t < 1000) goto p;
    }
  }
  return count;
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  int r = push();
  if (r != 0) {
    if (r == 1) {
      handleStoryButtons();
    } else if (r == 2) {
      display.clearDisplay();
      drawKeyboard();
      drawCursor(cursorX, cursorY);
      while (true) {
        mpu.getEvent(&a, &g, &temp);
        handleMPUInput(a, g);
        checkButtons();
        delay(100);
      }
    }
  }
}