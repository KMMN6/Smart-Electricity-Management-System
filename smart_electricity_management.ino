#include <Arduino.h>
#include "HLW8012.h"
#include <ESP32Firebase.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define _SSID "FAWSTECH INNOVATION"
#define _PASSWORD "JERSARFAWS"
#define REFERENCE_URL "https://smart-electricity-manage-b2888-default-rtdb.firebaseio.com/"

#define SERIAL_BAUDRaATE 115200
#define SEL_PIN 18
#define CF1_PIN 4
#define CF_PIN 5

#define UPDATE_TIME 2000
#define FIREBASE_UPDATE_TIME 3000

#define CURRENT_MODE HIGH
#define CURRENT_RESISTOR 0.001
#define VOLTAGE_RESISTOR_UPSTREAM (5 * 470000)
#define VOLTAGE_RESISTOR_DOWNSTREAM (1000)

HLW8012 hlw8012;
Firebase firebase(REFERENCE_URL);
LiquidCrystal_I2C lcd(0x27, 16, 2);

float totalEnergy = 0.0;
unsigned long lastUpdateTime = 0;
unsigned long lastFirebaseUpdate = 0;

void unblockingDelay(unsigned long mseconds) {
    unsigned long timeout = millis();
    while ((millis() - timeout) < mseconds)
        delay(1);
}

void calibrate() {
    hlw8012.getActivePower();
    hlw8012.setMode(MODE_CURRENT);
    unblockingDelay(2000);
    hlw8012.getCurrent();
    hlw8012.setMode(MODE_VOLTAGE);
    unblockingDelay(2000);
    hlw8012.getVoltage();

    hlw8012.expectedActivePower(25.0);
    hlw8012.expectedVoltage(230.0);
    hlw8012.expectedCurrent(25.0 / 230.0);

    Serial.println("[HLW] Calibration complete.");
}

float readEnergyFromFirebase() {
    float energy = firebase.getFloat("meters/123_ABC_8000/unit");
    if (isnan(energy)) {
        Serial.println("[Firebase] Failed to read energy. Initializing to 0 kWh.");
        return 0.0;
    }
    Serial.print("[Firebase] Restored Energy (kWh): ");
    Serial.println(energy);
    return energy;
}

void send_firebase_value(float energy_kWh, float power, float current, float voltage) {
    firebase.setFloat("meters/123_ABC_8000/unit", energy_kWh);
    firebase.setFloat("meters/123_ABC_8000/power", power);
    firebase.setFloat("meters/123_ABC_8000/current", current);
    firebase.setFloat("meters/123_ABC_8000/voltage", voltage);

    Serial.println("[Firebase] Energy (kWh) updated: " + String(energy_kWh));
}

void setup() {
    Serial.begin(SERIAL_BAUDRATE);
    Serial.println();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(1000);

    Serial.print("Connecting to: ");
    Serial.println(_SSID);
    WiFi.begin(_SSID, _PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print("-");
    }

    Serial.println("\nWiFi Connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    hlw8012.begin(CF_PIN, CF1_PIN, SEL_PIN, CURRENT_MODE, false, 1000000);
    hlw8012.setResistors(CURRENT_RESISTOR, VOLTAGE_RESISTOR_UPSTREAM, VOLTAGE_RESISTOR_DOWNSTREAM);
    calibrate();

    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("HLW8012 Monitor");
    lcd.setCursor(0, 1);
    lcd.print("Initializing...");
    delay(2000);
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("METER ID :");
    lcd.setCursor(0, 1);
    lcd.print("123_ABC_8000");
    delay(2000);
    lcd.clear();

    totalEnergy = readEnergyFromFirebase();
    lastUpdateTime = millis();
    lastFirebaseUpdate = millis();
}

void loop() {
    static unsigned long last = millis();

    if ((millis() - last) > UPDATE_TIME) {
        last = millis();

        float activePower = hlw8012.getActivePower();
        float voltage = hlw8012.getVoltage();
        float current = hlw8012.getCurrent();

        Serial.print("[HLW] Power (W)  : ");
        Serial.println(activePower);
        Serial.print("[HLW] Voltage (V): ");
        Serial.println(voltage);
        Serial.print("[HLW] Current (A): ");
        Serial.println(current);

        unsigned long currentTime = millis();
        float elapsedTimeHours = (currentTime - lastUpdateTime) / 3600000.0;
        totalEnergy += activePower * elapsedTimeHours / 1000.0;
        lastUpdateTime = currentTime;

        Serial.print("[HLW] Total Energy (kWh): ");
        Serial.println(totalEnergy);

        if ((millis() - lastFirebaseUpdate) > FIREBASE_UPDATE_TIME) {
            send_firebase_value(totalEnergy, activePower, current, voltage);
            lastFirebaseUpdate = millis();
        }

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("P:");
        lcd.print(activePower, 1);
        lcd.print("W");

        lcd.setCursor(10, 0);
        lcd.print("V:");
        lcd.print(voltage, 1);

        lcd.setCursor(0, 1);
        lcd.print("C:");
        lcd.print(current, 2);
        lcd.print("A");

        lcd.setCursor(10, 1);
        lcd.print("E:");
        lcd.print(totalEnergy, 2);
        lcd.print("kWh");

        hlw8012.toggleMode();
    }
}