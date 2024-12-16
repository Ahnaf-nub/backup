#include <TFT_eSPI.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> // For parsing JSON responses from the API

TFT_eSPI tft = TFT_eSPI();

// Button GPIO pins
#define BUTTON_UP 5
#define BUTTON_DOWN 7
#define BUTTON_LEFT 6
#define BUTTON_RIGHT 8
#define BUTTON_SELECT 12
#define BUTTON_DELETE 15
#define BUTTON_ENTER 13
#define BUTTON_SCROLL 14

// Keyboard layout constants
#define NUM_ROWS 6
#define NUM_COLS 7
const char keyboardUppercase[NUM_ROWS][NUM_COLS] = {
    {'A', 'B', 'C', 'D', 'E', 'F', 'G'},
    {'H', 'I', 'J', 'K', 'L', 'M', 'N'},
    {'O', 'P', 'Q', 'R', 'S', 'T', 'U'},
    {'V', 'W', 'X', 'Y', 'Z', '0', '1'},
    {'2', '3', '4', '5', '6', '7', '8'},
    {'9', '+', '-', '*', '/', '=', ','}
};

const char keyboardLowercase[NUM_ROWS][NUM_COLS] = {
    {'a', 'b', 'c', 'd', 'e', 'f', 'g'},
    {'h', 'i', 'j', 'k', 'l', 'm', 'n'},
    {'o', 'p', 'q', 'r', 's', 't', 'u'},
    {'v', 'w', 'x', 'y', 'z', '0', '1'},
    {'2', '3', '4', '5', '6', '7', '8'},
    {'9', '+', '-', '*', '/', '=', ','}
};

bool isUppercase = true;

// Cursor position
int cursorRow = 0;
int cursorCol = 0;
String ssid = "";
String password = "";
String userPrompt = "";
String apiResponse = "";
bool enteringSSID = true; // True if entering SSID, false if entering password
bool enteringPrompt = false; // True when entering a prompt
int responseScroll = 0; // For scrolling through the response
const char* API_KEY = "AIzaSyD2-6kmQ7_WHmGnpYWFHtZsaLBBVpjDTQ0"; // Replace with your Gemini API key
#define MAX_TOKENS 200

void saveWiFiCredentials(const String& ssid, const String& password) {
    File file = LittleFS.open("/wifi.txt", "w");
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.println(ssid);
    file.println(password);
    file.close();
    Serial.println("Wi-Fi credentials saved.");
}

bool loadWiFiCredentials(String& ssid, String& password) {
    if (!LittleFS.exists("/wifi.txt")) {
        return false;
    }

    File file = LittleFS.open("/wifi.txt", "r");
    if (!file) {
        Serial.println("Failed to open file for reading");
        return false;
    }
    ssid = file.readStringUntil('\n');
    ssid.trim();
    password = file.readStringUntil('\n');
    password.trim();
    file.close();
    Serial.println("Wi-Fi credentials loaded.");
    return true;
}

void drawKeyboard() {
    tft.fillRect(10, 20, 140, 100, TFT_BLACK); // Clear keyboard area

    int startX = 15;
    int startY = 30;
    const char (*keyboard)[NUM_COLS] = isUppercase ? keyboardUppercase : keyboardLowercase;

    for (int row = 0; row < NUM_ROWS; row++) {
        for (int col = 0; col < NUM_COLS; col++) {
            int x = startX + col * 20;
            int y = startY + row * 15;

            // Highlight the current cursor position
            if (row == cursorRow && col == cursorCol) {
                tft.fillRect(x - 2, y - 2, 18, 14, TFT_BLUE);
                tft.setTextColor(TFT_BLACK, TFT_BLUE);
            } else {
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
            }
            tft.setCursor(x, y);
            tft.print(keyboard[row][col]);
        }
    }
}

void drawInterface() {
    tft.fillScreen(TFT_BLACK); // Clear the screen
    tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set text color
    tft.setCursor(0, 0);
    tft.setTextSize(1);

    if (!WiFi.isConnected() && enteringSSID) {
        tft.print("SSID: ");
        tft.println(ssid);
        drawKeyboard();
    } else if (!WiFi.isConnected() && !enteringSSID) {
        tft.print("PASS: ");
        tft.println(password);
        drawKeyboard();
    } else if (enteringPrompt) {
        tft.print("Prompt: ");
        tft.println(userPrompt);

        drawKeyboard(); // Always draw the keyboard for entering the prompt

        // Show API response if available
        if (apiResponse.length() > 0) {
            tft.setCursor(0, 140);
            int maxVisibleChars = 120;
            int end = min(responseScroll + maxVisibleChars, apiResponse.length());
            tft.print(apiResponse.substring(responseScroll, end));
        } else {
            tft.print("Type Something...");
        }
    }
}

void connectToWiFi() {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextSize(2);
    tft.print("Connecting to Wi-Fi...");
    tft.setTextSize(1);
    WiFi.begin(ssid.c_str(), password.c_str());

    int attempts = 0;
    while (!WiFi.isConnected() && attempts < 20) { // Retry for 10 seconds
        delay(500);
        attempts++;
        tft.setCursor(0, 20);
        tft.print("Connecting.");
        for (int i = 0; i < attempts % 4; i++) {
            tft.print(".");
        }
        tft.print("   ");
    }

    if (WiFi.isConnected()) {
        tft.setCursor(0, 40);
        tft.println("Connected!");
    } else {
        tft.setCursor(0, 40);
        tft.println("Failed to connect.");
        enteringSSID = true; // Return to SSID entry if connection fails
    }
    delay(1000);
    drawInterface();
}

void sendToGeminiAPI(String userInput) {
    HTTPClient https;

    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextSize(2);
    tft.print("Processing...");
    tft.setTextSize(1);

    // Prepare API call
    if (https.begin("https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + (String)API_KEY)) {
        https.addHeader("Content-Type", "application/json");

        String payload = "{\"prompt\": {\"text\": \"" + userInput + "\"},\"max_tokens\": " + String(MAX_TOKENS) + "}";
        int httpCode = https.POST(payload);

        if (httpCode == HTTP_CODE_OK) {
            String response = https.getString();
            Serial.println("API Response: " + response);

            // Parse the response
            DynamicJsonDocument doc(4096); // Increased size to handle large responses
            DeserializationError error = deserializeJson(doc, response);

            if (!error) {
                apiResponse = doc["candidates"][0]["output"].as<String>();
                responseScroll = 0; // Reset scroll offset
                Serial.println("Parsed API Response: " + apiResponse);
            } else {
                Serial.println("JSON Parsing Error: " + String(error.c_str()));
                apiResponse = "Error parsing response.";
            }
        } else {
            Serial.printf("[HTTPS] POST failed, error: %s\n", https.errorToString(httpCode).c_str());
            apiResponse = "Failed to get response from API.";
        }
        https.end();
    } else {
        Serial.println("[HTTPS] Unable to connect");
        apiResponse = "Unable to connect to API.";
    }

    enteringPrompt = false; // Exit prompt entry mode
    drawInterface();        // Update the display with the response or error
}

void handleInput() {
    if (!WiFi.isConnected()) {
        if (digitalRead(BUTTON_UP) == LOW) {
            cursorRow = (cursorRow - 1 + NUM_ROWS) % NUM_ROWS;
            drawInterface();
            delay(200);
        } else if (digitalRead(BUTTON_DOWN) == LOW) {
            cursorRow = (cursorRow + 1) % NUM_ROWS;
            drawInterface();
            delay(200);
        } else if (digitalRead(BUTTON_LEFT) == LOW) {
            cursorCol = (cursorCol - 1 + NUM_COLS) % NUM_COLS;
            drawInterface();
            delay(200);
        } else if (digitalRead(BUTTON_RIGHT) == LOW) {
            cursorCol = (cursorCol + 1) % NUM_COLS;
            drawInterface();
            delay(200);
        } else if (digitalRead(BUTTON_SELECT) == LOW) {
            const char (*keyboard)[NUM_COLS] = isUppercase ? keyboardUppercase : keyboardLowercase;
            char selectedChar = keyboard[cursorRow][cursorCol];
            if (enteringSSID) {
                ssid += selectedChar;
            } else if (!enteringSSID) {
                password += selectedChar;
            }
            drawInterface();
            delay(200);
        } else if (digitalRead(BUTTON_DELETE) == LOW) {
            if (enteringSSID && ssid.length() > 0) {
                ssid.remove(ssid.length() - 1);
            } else if (!enteringSSID && password.length() > 0) {
                password.remove(password.length() - 1);
            }
            drawInterface();
            delay(200);
        } else if (digitalRead(BUTTON_ENTER) == LOW) {
            if (enteringSSID) {
                enteringSSID = false;
            } else {
                WiFi.begin(ssid.c_str(), password.c_str());
                saveWiFiCredentials(ssid, password);
                delay(5000); // Give time for Wi-Fi connection
                if (!WiFi.isConnected()) {
                    enteringSSID = true;
                }
            }
            drawInterface();
            delay(200);
        }
    } else if (enteringPrompt) {
        if (digitalRead(BUTTON_UP) == LOW) {
            cursorRow = (cursorRow - 1 + NUM_ROWS) % NUM_ROWS;
            drawInterface();
            delay(200);
        } else if (digitalRead(BUTTON_DOWN) == LOW) {
            cursorRow = (cursorRow + 1) % NUM_ROWS;
            drawInterface();
            delay(200);
        } else if (digitalRead(BUTTON_LEFT) == LOW) {
            cursorCol = (cursorCol - 1 + NUM_COLS) % NUM_COLS;
            drawInterface();
            delay(200);
        } else if (digitalRead(BUTTON_RIGHT) == LOW) {
            cursorCol = (cursorCol + 1) % NUM_COLS;
            drawInterface();
            delay(200);
        } else if (digitalRead(BUTTON_SELECT) == LOW) {
            const char (*keyboard)[NUM_COLS] = isUppercase ? keyboardUppercase : keyboardLowercase;
            userPrompt += keyboard[cursorRow][cursorCol];
            drawInterface();
            delay(200);
        } else if (digitalRead(BUTTON_DELETE) == LOW) {
            if (userPrompt.length() > 0) {
                userPrompt.remove(userPrompt.length() - 1);
            }
            drawInterface();
            delay(200);
        } else if (digitalRead(BUTTON_ENTER) == LOW) {
            sendToGeminiAPI(userPrompt);
            delay(200);
        } else if (digitalRead(BUTTON_SCROLL) == LOW) {
            // Scroll through the response
            if (responseScroll + 10 < apiResponse.length()) {
                responseScroll += 10;
            }
            drawInterface();
            delay(200);
        }
    } else {
        if (digitalRead(BUTTON_ENTER) == LOW) {
            enteringPrompt = true; // Allow re-entry of a new prompt
            userPrompt = "";       // Clear old prompt
            drawInterface();
            delay(200);
        }
    }
}

void setup() {
    Serial.begin(9600);

    if (!LittleFS.begin()) {
        Serial.println("An error has occurred while mounting LittleFS");
        return;
    }

    pinMode(BUTTON_UP, INPUT_PULLUP);
    pinMode(BUTTON_DOWN, INPUT_PULLUP);
    pinMode(BUTTON_LEFT, INPUT_PULLUP);
    pinMode(BUTTON_RIGHT, INPUT_PULLUP);
    pinMode(BUTTON_SELECT, INPUT_PULLUP);
    pinMode(BUTTON_DELETE, INPUT_PULLUP);
    pinMode(BUTTON_ENTER, INPUT_PULLUP);
    pinMode(BUTTON_SCROLL, INPUT_PULLUP);

    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    const char loadingChars[] = {'|', '/', '-', '\\'};
    
    // Show loading animation while checking filesystem
    for(int i = 0; i < 22; i++) {
        tft.setCursor(0, 0);
        tft.setTextSize(2);
        tft.print("Loading.. ");
        tft.print(loadingChars[i % 4]);
        delay(250);
    }
    tft.setTextSize(1);

    if (loadWiFiCredentials(ssid, password)) {
        WiFi.begin(ssid.c_str(), password.c_str());
    }
    drawInterface();
}

void loop() {
    handleInput();

    if (WiFi.isConnected() && !enteringPrompt) {
        enteringPrompt = true;
        drawInterface();
    }
}