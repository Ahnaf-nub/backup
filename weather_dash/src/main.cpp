#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <WiFiManager.h>
#include "rain.h"
#include "clear_sky.h"

const char* apiKey = "67ae6393bea0c753302aa72ccdeb9283";
const char* city = "Dhaka";
const char* countryCode = "BD"; // e.g., "US"
String weatherURL = "http://api.openweathermap.org/data/2.5/weather?q=" + String(city) + "," + String(countryCode) + "&appid=" + String(apiKey);

// TFT Display Setup
TFT_eSPI tft = TFT_eSPI();

// NTP Client Setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 21600, 60000); // UTC+6 hours (adjust offset for Dhaka, Bangladesh)

void setup() {
  Serial.begin(115200);
  
  // Initialize TFT display
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  WiFiManager wifiManager;
  wifiManager.autoConnect("WeatherDashAP");

  // Initialize NTP client
  timeClient.begin();
}

void drawWeatherIcon(String description) {
  if (description.indexOf("clear") >= 0) {
    for (int frame = 0; frame < FRAME_COUNT; frame++) {
      tft.fillScreen(TFT_BLACK);
      tft.drawBitmap(100, 30, frames[frame], FRAME_WIDTH, FRAME_HEIGHT, TFT_YELLOW);
      delay(FRAME_DELAY);
    }
  } else if (description.indexOf("rain") >= 0) {
    for (int frame = 0; frame < FRAME_COUNT; frame++) {
      tft.fillScreen(TFT_BLACK);
      tft.drawBitmap(100, 30, framer[frame], FRAME_WIDTH, FRAME_HEIGHT, TFT_BLUE);
      delay(FRAME_DELAY);
    }
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("No icon for: " + description, 10, 80);
  }
}

void displayClock() {
  timeClient.update();
  String formattedTime = timeClient.getFormattedTime();
  
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(10, 100);
  tft.println("Time: " + formattedTime);
}

void displayWeather() {
  HTTPClient http;
  http.begin(weatherURL);
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    String weatherDescription = doc["weather"][0]["description"];
    float temperature = doc["main"]["temp"].as<float>() - 273.15; // Convert from Kelvin to Celsius
    int humidity = doc["main"]["humidity"];
    
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 10);
    tft.println("Weather in " + String(city) + ":");
    tft.println("Temp: " + String(temperature, 1) + "°C");
    tft.println("Humidity: " + String(humidity) + "%");

    drawWeatherIcon(weatherDescription);
  } else {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Failed to fetch weather!", 10, 10);
  }
  http.end();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    displayWeather();
    displayClock();
  } else {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Wi-Fi Disconnected!", 10, 10);
  }
  delay(60000); // Update every minute
}
