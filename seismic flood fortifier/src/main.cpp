// MAC of Receiver FC:B4:67:73:E7:18
/*
#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Define joystick pins
const int joyX1 = 34; // Joystick 1 X-axis
const int joyY1 = 35; // Joystick 1 Y-axis
const int joyX2 = 39; // Joystick 2 X-axis
const int joyY2 = 36; // Joystick 2 Y-axis

// Define joystick pushbutton pin
const int buttonPin = 25; // Joystick pushbutton pin
const int trackingButtonPin = 32; // New button to activate solar tracking

// Define rotary encoder pins
const int encoderPinA = 27;
const int encoderPinB = 26;

volatile int encoderValue = 0;
int lastEncoderState = 0;
int mappedServoValue = 90; // Initial position for servo, center (90 degrees)

// Structure to send joystick, button, and servo data
typedef struct struct_message {
  int x1;
  int y1;
  int x2;
  int y2;
  int buttonState;       // Joystick button state
  int servoPos;          // Mapped servo position
  int trackingCommand;   // Command to start solar tracking (0 = off, 1 = on)
} struct_message;

struct_message dataToSend;

// Broadcast address of the receiver ESP32
uint8_t broadcastAddress[] = {0xFC, 0xB4, 0x67, 0x73, 0xE7, 0x18};

// OLED display setup
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Function to send data
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

esp_now_peer_info_t peerInfo;

// Rotary encoder interrupt for pin A
void IRAM_ATTR handleEncoderA() {
  int stateA = digitalRead(encoderPinA);
  if (stateA != lastEncoderState) {
    if (digitalRead(encoderPinB) != stateA) {
      encoderValue++;
    } else {
      encoderValue--;
    }
  }
  lastEncoderState = stateA;

  // Bound the encoder value to prevent overflow
  if (encoderValue < 0) encoderValue = 0;
  if (encoderValue > 1023) encoderValue = 1023;
}

void setup() {
  Serial.begin(9600);

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for (;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.display();

  // Set Joystick pins as input
  pinMode(joyX1, INPUT);
  pinMode(joyY1, INPUT);
  pinMode(joyX2, INPUT);
  pinMode(joyY2, INPUT);

  // Set button pins as input
  pinMode(buttonPin, INPUT_PULLUP); // Button pin with pull-up
  pinMode(trackingButtonPin, INPUT_PULLUP); // Solar tracking button with pull-up

  // Set rotary encoder pins
  pinMode(encoderPinA, INPUT);
  pinMode(encoderPinB, INPUT);

  // Attach interrupts to the encoder pins
  attachInterrupt(digitalPinToInterrupt(encoderPinA), handleEncoderA, CHANGE);

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  Serial.println("ESP32 Joystick Controller");

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_del_peer(broadcastAddress);  // Remove peer if it's already registered

  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Register send callback
  esp_now_register_send_cb(OnDataSent);
}

void loop() {
  // Read joystick values
  dataToSend.x1 = analogRead(joyX1);
  dataToSend.y1 = analogRead(joyY1);
  dataToSend.x2 = analogRead(joyX2);
  dataToSend.y2 = analogRead(joyY2);

  // Read button state (0 when pressed, 1 when not pressed)
  dataToSend.buttonState = digitalRead(buttonPin);

  // Read the tracking button state (if pressed, trigger solar tracking mode)
  if (digitalRead(trackingButtonPin) == LOW) {
    dataToSend.trackingCommand = 1; // Solar tracking mode ON
  } else {
    dataToSend.trackingCommand = 0; // Solar tracking mode OFF
  }

  // Map encoder value (0 - 1023) to servo position (0 - 180 degrees)
  mappedServoValue = map(encoderValue, 0, 1023, 0, 180);
  dataToSend.servoPos = mappedServoValue  * 20; // Send mapped servo position

  // Send the data via ESP-NOW
  esp_now_send(broadcastAddress, (uint8_t *) &dataToSend, sizeof(dataToSend));

  // Display the mapped servo value on OLED
  display.clearDisplay(); // Clear the display before writing new values
  display.setTextSize(1); // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Set text color to white
  display.setCursor(0, 0); // Set position to display the first line

  // Display mapped servo value
  display.print("Servo Pos: ");
  display.println(mappedServoValue); // Show the mapped value (0-180 degrees)

  // Display button state and relay status
  display.print("Relay: ");
  if (dataToSend.buttonState == 0) {  // Button pressed
    display.println("ON");
  } else {
    display.println("OFF");
  }

  // Display solar tracking mode status
  display.print("Tracking: ");
  if (dataToSend.trackingCommand == 1) {
    display.println("ON");
  } else {
    display.println("OFF");
  }

  // Display the values on the OLED screen
  display.display(); 
  delay(100); // Small delay between sends
}

*/

#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>  // Servo library for controlling the servo motor

// Motor control pins
const int motorA1 = 14;
const int motorA2 = 27;
const int motorB1 = 26;
const int motorB2 = 25;
const int ldrLeft = 34; // Left LDR
const int ldrRight = 35; // Right LDR
// Relay control pin
const int relayPin = 15;
int servoPos = 90; // Initial position for the servo (center)
bool trackingEnabled = false; // Solar tracking status
// Servo control pin
const int servoPin = 13;
Servo myServo;  // Create a servo object
Servo solarServo;  // Create a servo object for solar tracking
// Structure to receive joystick, button, and encoder data
typedef struct struct_message {
  int x1;
  int y1;
  int x2;
  int y2;
  int buttonState;
  int encoderPos;
} struct_message;

struct_message receivedData;

// Function to receive data
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  memcpy(&receivedData, incomingData, sizeof(receivedData));

  // Process joystick 1 data (for motor A)
  if (receivedData.y1 > 2000) {
    digitalWrite(motorA1, HIGH);
    digitalWrite(motorA2, LOW);
  } else if (receivedData.y1 < 1000) {
    digitalWrite(motorA1, LOW);
    digitalWrite(motorA2, HIGH);
  } else {
    digitalWrite(motorA1, LOW);
    digitalWrite(motorA2, LOW);
  }

  // Process joystick 2 data (for motor B)
  if (receivedData.y2 > 2000) {
    digitalWrite(motorB1, HIGH);
    digitalWrite(motorB2, LOW);
  } else if (receivedData.y2 < 1000) {
    digitalWrite(motorB1, LOW);
    digitalWrite(motorB2, HIGH);
  } else {
    digitalWrite(motorB1, LOW);
    digitalWrite(motorB2, LOW);
  }

  // Control relay based on button press
  if (receivedData.buttonState == 0) {  // Button pressed
    digitalWrite(relayPin, HIGH);  // Turn on relay
  } else {
    digitalWrite(relayPin, LOW);   // Turn off relay
  }

  if (trackingEnabled) {
    // Read light intensity from LDRs
    int leftIntensity = analogRead(ldrLeft);
    int rightIntensity = analogRead(ldrRight);

    // Adjust the servo based on the light intensity difference
    if (leftIntensity > rightIntensity) {
      servoPos--;
    } else if (rightIntensity > leftIntensity) {
      servoPos++;
    }

    // Bound the servo position between 0 and 180 degrees
    if (servoPos < 0) servoPos = 0;
    if (servoPos > 180) servoPos = 180;

    // Move the servo to the new position
    solarServo.write(servoPos);

    // Small delay to avoid jittering
    delay(100);
  }

  // Control servo using encoder data
  int servoPosition = map(receivedData.encoderPos, -100, 100, 0, 180);  // Map encoder range to servo range
  myServo.write(servoPosition);
  solarServo.write(servoPos);
}

void setup() {
  Serial.begin(9600);

  // Set motor pins as output
  pinMode(motorA1, OUTPUT);
  pinMode(motorA2, OUTPUT);
  pinMode(motorB1, OUTPUT);
  pinMode(motorB2, OUTPUT);

  // Set relay pin as output
  pinMode(relayPin, OUTPUT);
  pinMode(ldrLeft, INPUT);
  pinMode(ldrRight, INPUT);
  // Attach servo to servo pin
  myServo.setPeriodHertz(50);
  myServo.attach(servoPin, 500, 2400);
  solarServo.setPeriodHertz(50);
  solarServo.attach(servoPin, 500, 2400);

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  Serial.println("ESP32 Robot Receiver");

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  } else {
    Serial.println("ESP-NOW Initialized");
  }

  // Register receive callback
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // Nothing to do here; everything is handled by the callback
}
