#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <Adafruit_MLX90614.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define MIC_PIN 34

PulseOximeter pox;
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

uint32_t tsLastReport = 0;
float lastHR = 0;
float lastSpO2 = 0;

void setup() {
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000); 

    Serial.println("System Initializing...");

    if (!pox.begin()) {
        Serial.println("MAX30100 Init Failed!");
    } else {
        pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
    }

    if (!mlx.begin()) {
        Serial.println("MLX90614 Init Failed!");
    }
}

void loop() {
    pox.update();

    // 1. VOICE BIOMARKER (ZCR Calculation)
    long zcrCount = 0;
    int lastSample = 2048;
    unsigned long micWindow = millis();
    // Smaller window (20ms) to prevent starving the PulseOx sensor
    while (millis() - micWindow < 20) {
        int s = analogRead(MIC_PIN);
        if ((s > 2100 && lastSample <= 2100) || (s < 2000 && lastSample >= 2000)) {
            zcrCount++;
        }
        lastSample = s;
    }

    // 2. DATA PROCESSING & TRIAGE (Every 2 Seconds)
    if (millis() - tsLastReport > 2000) {
        
        lastHR = pox.getHeartRate();
        lastSpO2 = pox.getSpO2();
        float tempC = mlx.readObjectTempC();

        // FINGER DETECTION LOGIC
        // If HR is 0, the sensor isn't getting a signal (Finger is off)
        bool fingerOn = (lastHR > 10.0); 

        // MLX90614 NAN FIX
        if (isnan(tempC) || tempC < 15.0) {
            Wire.begin(SDA_PIN, SCL_PIN); // Reset I2C Bus
            tempC = 0;
        }

        // 3. WEIGHTED RISK SCORING
        int riskLevel = 0;
        
        if (fingerOn) {
            // SpO2 Risk (Heavily weighted)
            if (lastSpO2 < 90) riskLevel += 5;
            else if (lastSpO2 < 94) riskLevel += 2;

            // HR Risk
            if (lastHR > 115 || lastHR < 50) riskLevel += 2;
        }

        // Temperature Risk
        if (tempC > 38.0) riskLevel += 2;
        else if (tempC > 37.5) riskLevel += 1;

        // Voice Risk (Multiplied for 20ms window)
        if (zcrCount > 150) riskLevel += 1; 

        // 4. FINAL OUTPUT
        Serial.println("\n--- HEALTH ASSESSMENT REPORT ---");
        
        if (!fingerOn) {
            Serial.println(">> PLEASE PLACE FINGER CORRECTLY");
            Serial.print("Temp: "); Serial.print(tempC); Serial.println(" C");
        } else {
            Serial.print("HR: "); Serial.print(lastHR); Serial.print(" bpm | ");
            Serial.print("SpO2: "); Serial.print(lastSpO2); Serial.println(" %");
            Serial.print("Body Temp: "); Serial.print(tempC); Serial.println(" C");
        }

        Serial.print("Voice Biomarker (ZCR): "); Serial.println(zcrCount);
        Serial.print("COMPOSITE RISK LEVEL: "); Serial.print(riskLevel); Serial.println("/10");

        if (riskLevel >= 7) Serial.println("STATUS: [!!!] CRITICAL");
        else if (riskLevel >= 4) Serial.println("STATUS: [!] ABNORMAL");
        else Serial.println("STATUS: [OK] HEALTHY");

        tsLastReport = millis();
    }
} 