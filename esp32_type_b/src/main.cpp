#include <Arduino.h>
/*#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>

RTC_DS3231 rtc;

void setup() {
    Serial.begin(9600);
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        while (1);
    }

    if (rtc.lostPower()) {
        Serial.println("RTC lost power, let's set the time!");
        rtc.adjust(DateTime(2024, 10, 21, 17, 47, 20)); // Set to desired date and time (YYYY, MM, DD, HH, MM, SS)
    }
}

void loop() {
    DateTime now = rtc.now();
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();

    delay(1000);
}*/

#include <Adafruit_NeoPixel.h>

#define PIN D8
#define NUMPIXELS 7

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    pixels.begin();
}

uint32_t Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if (WheelPos < 85) {
        return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    if (WheelPos < 170) {
        WheelPos -= 85;
        return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    WheelPos -= 170;
    return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}


void loop() {
    for (int j = 0; j < 256; j++) { // cycle through the colors
        for (int i = 0; i < NUMPIXELS; i++) {
            pixels.setPixelColor(i, Wheel((i + j) & 255));
        }
        pixels.show();
        delay(20);
    }
}