#include <Seeed_Arduino_SSCMA.h>
#include <Servo.h>

SSCMA AI;
Servo myservo;

const int SWEEP_DELAY = 5;  // Reduced delay for faster scanning
const int CENTER_THRESHOLD = 50;  // Defines the "middle" zone width
const int FRAME_CENTER = 320;  // Center of the frame (640/2)
const float SMOOTHING_FACTOR = 0.2;
int currentAngle = 90;

void setup() {
    AI.begin();
    Serial.begin(9600);
    myservo.attach(0);
    myservo.write(currentAngle);
}

void loop() {
    static int sweepAngle = 0;
    static bool increasing = true;
    static unsigned long lastSweepTime = 0;

    if (!AI.invoke()) {
        if (AI.boxes().size() > 0) {
            int personX = AI.boxes()[0].y;
            
            // Check if person is in the middle zone
            if (abs(personX - FRAME_CENTER) < CENTER_THRESHOLD) {
                // Person is in middle, maintain current position
                return;
            }
            
            // Person detected but not in middle, track them
            int targetAngle = map(personX, 0, 640, 0, 180);
            currentAngle = currentAngle + (targetAngle - currentAngle) * SMOOTHING_FACTOR;
            myservo.write(currentAngle);
        } else {
            // No person detected, perform faster sweep
            if (millis() - lastSweepTime >= SWEEP_DELAY) {
                if (increasing) {
                    sweepAngle += 2;  // Increment by 2 for faster sweep
                    if (sweepAngle >= 180) increasing = false;
                } else {
                    sweepAngle -= 2;  // Decrement by 2 for faster sweep
                    if (sweepAngle <= 0) increasing = true;
                }
                currentAngle = sweepAngle;
                myservo.write(currentAngle);
                lastSweepTime = millis();
            }
        }
    }
}
