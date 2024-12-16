#include <Parkinson_inferencing.h>
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

MPU6050 imu;
int16_t ax, ay, az;

#define ACC_RANGE           1 // 0: -/+2G; 1: +/-4G
#define CONVERT_G_TO_MS2    (9.81 / (16384 / (1.0 + ACC_RANGE)))
#define MAX_ACCEPTED_RANGE  ((2 * 9.81) + (2 * 9.81 * ACC_RANGE))

static bool debug_nn = false; // Debug mode for Edge Impulse
ei_impulse_result_t result = { 0 };

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLEScan* pBLEScan = NULL;
bool deviceConnected = false;
bool panicMode = false;

float ei_get_sign(float number) {
    return (number >= 0.0) ? 1.0 : -1.0;
}

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        panicMode = false; // Reset panic mode on connection
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        panicMode = true;
        delay(500); // Delay before restarting advertising
        pServer->startAdvertising();
        Serial.println("Device disconnected. Panic mode activated.");
    }
};

void setup() {
    Serial.begin(9600);
    while (!Serial);
    Serial.println("Neurosense Initialized");

    // Initialize MPU6050
    Wire.begin();
    imu.initialize();

    if (!imu.testConnection()) {
        Serial.println("Failed to connect to MPU6050!");
        while (1); // Stop execution if sensor is not working
    }

    imu.setXAccelOffset(-4732);
    imu.setYAccelOffset(4703);
    imu.setZAccelOffset(8867);
    imu.setXGyroOffset(61);
    imu.setYGyroOffset(-73);
    imu.setZGyroOffset(35);
    imu.setFullScaleAccelRange(ACC_RANGE);

    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 3) {
        Serial.println("Error: EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME should be 3.");
        while (1); // Stop execution
    }

    // Initialize BLE
    BLEDevice::init("Parkinson Device");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService* pService = pServer->createService(BLEUUID((uint16_t)0xFFE0));
    pCharacteristic = pService->createCharacteristic(
        BLEUUID((uint16_t)0xFFE1),
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );

    pService->start();
    pServer->getAdvertising()->start();

    // Initialize BLE Scanner
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    Serial.println("BLE Initialized and Advertising");
}

void loop() {
    static unsigned long lastInferenceTime = 0;

    // Panic mode: Scan and connect to the nearest device
    if (panicMode) {
        Serial.println("Scanning for nearby BLE devices...");

        // Scan for 5 seconds
        BLEScanResults scanResults = pBLEScan->start(5, false);
        int deviceCount = scanResults.getCount();
        Serial.printf("Found %d devices\n", deviceCount);

        if (deviceCount == 0) {
            Serial.println("No devices found. Retrying...");
            delay(1000);
            return;
        }

        BLEAdvertisedDevice* nearestDevice = nullptr;
        int strongestRSSI = -100; // Initialize to a low signal strength

        // Find the device with the strongest signal
        for (int i = 0; i < deviceCount; i++) {
            BLEAdvertisedDevice device = scanResults.getDevice(i);
            Serial.printf("Device: %s, RSSI: %d\n", device.toString().c_str(), device.getRSSI());

            if (device.getRSSI() > strongestRSSI) {
                strongestRSSI = device.getRSSI();
                BLEAdvertisedDevice device = scanResults.getDevice(i);
                nearestDevice = new BLEAdvertisedDevice(device);
            }
        }

        if (nearestDevice) {
            Serial.printf("Attempting connection to nearest device: %s (RSSI: %d)\n",
                          nearestDevice->toString().c_str(), strongestRSSI);

            BLEClient* pClient = BLEDevice::createClient();
            Serial.println("Created BLE client.");

            // Debug advertised services before connecting
            if (nearestDevice->haveServiceUUID()) {
                Serial.printf("Found Service UUID: %s\n", nearestDevice->getServiceUUID().toString().c_str());
            } else {
                Serial.println("No Service UUID found. Attempting connection anyway...");
            }

            // Try to connect
            if (pClient->connect(nearestDevice)) {
                Serial.println("Connected to nearest device!");

                // Attempt to send a panic message
                BLERemoteService* pService = pClient->getService(BLEUUID((uint16_t)0xFFE0));
                if (pService) {
                    BLERemoteCharacteristic* pCharacteristicRemote = pService->getCharacteristic(BLEUUID((uint16_t)0xFFE1));
                    if (pCharacteristicRemote && pCharacteristicRemote->canWrite()) {
                        pCharacteristicRemote->writeValue("This is a device for Parkinson Patient. Please return it to the patient.");
                        Serial.println("Panic message sent.");
                    } else {
                        Serial.println("Remote characteristic not writable.");
                    }
                } else {
                    Serial.println("Service UUID not found on connected device.");
                }

                // Disconnect after sending the message
                pClient->disconnect();
                delete pClient;
                panicMode = false; // Exit panic mode after successful connection
                return;
            } else {
                Serial.println("Failed to connect to the nearest device.");
                delete pClient;
            }
        }

        Serial.println("No connection established. Retrying...");
        delay(1000);
        return;
    }

    // Perform inference every 2 seconds if connected
    if (deviceConnected && millis() - lastInferenceTime > 2000) {
        // Perform inference (unchanged logic)
        Serial.println("Starting inferencing...");

        // Prepare buffer for IMU data
        float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = { 0 };

        for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += 3) {
            uint64_t next_tick = micros() + (EI_CLASSIFIER_INTERVAL_MS * 1000);

            imu.getAcceleration(&ax, &ay, &az);
            buffer[ix + 0] = ax * CONVERT_G_TO_MS2;
            buffer[ix + 1] = ay * CONVERT_G_TO_MS2;
            buffer[ix + 2] = az * CONVERT_G_TO_MS2;

            for (int i = 0; i < 3; i++) {
                if (fabs(buffer[ix + i]) > MAX_ACCEPTED_RANGE) {
                    buffer[ix + i] = ei_get_sign(buffer[ix + i]) * MAX_ACCEPTED_RANGE;
                }
            }

            delayMicroseconds(next_tick - micros());
        }

        // Turn raw buffer into a signal for classification
        signal_t signal;
        int err = numpy::signal_from_buffer(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
        if (err != 0) {
            Serial.printf("Failed to create signal from buffer (%d)\n", err);
            return;
        }

        // Run the classifier
        err = run_classifier(&signal, &result, debug_nn);
        if (err != EI_IMPULSE_OK) {
            Serial.printf("Failed to run classifier (%d)\n", err);
            return;
        }

        // Display predictions
        Serial.printf("Predictions (DSP: %d ms, Classification: %d ms, Anomaly: %d ms):\n",
                      result.timing.dsp, result.timing.classification, result.timing.anomaly);
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            Serial.printf("    %s: %.5f\n", result.classification[ix].label, result.classification[ix].value);
        }

    #if EI_CLASSIFIER_HAS_ANOMALY == 1
        Serial.printf("    Anomaly score: %.3f\n", result.anomaly);
    #endif

        // Send predictions over BLE
        String data = "Predictions:\n";
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            data += String(result.classification[ix].label) + ": " + String(result.classification[ix].value) + "\n";
        }
    #if EI_CLASSIFIER_HAS_ANOMALY == 1
        data += "Anomaly score: " + String(result.anomaly) + "\n";
    #endif
        pCharacteristic->setValue(data.c_str());
        pCharacteristic->notify();

        lastInferenceTime = millis();
    }
}