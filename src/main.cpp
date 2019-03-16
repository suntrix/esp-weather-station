#define EASYESP_SERIAL_BAUD 76800
#define EASYESP_SERIAL_TIMEOUT 1e3      // 1 second

#define EASYESP_STATUS_LED_FLASH_ENABLED 1

#define EASYESP_WATCHDOG_SETUP_TIMEOUT 15e3     // 15 seconds
// #define EASYESP_WATCHDOG_LOOP_TIMEOUT 10e3      // 10 seconds
#define EASYESP_WATCHDOG_DEEP_SLEEP 60e3        // 60 seconds

#include "secrets.h"

#include <Framework.h>

#include <brzo_i2c.h>
#include <BME280I2C_BRZO.h>
#include <EnvironmentCalculations.h>

#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

EasyESP::Framework framework;

BME280I2C_BRZO bme;

struct SensorData {
    float temperature;
    float pressure;
    float humidity;
    float dewPoint;
};

bool findBME280() {
    brzo_i2c_setup(SDA, SCL, I2C_ACK_TIMEOUT);

    Serial.println("Searching BME280 / BMP280 sensor");

    unsigned long timeout = millis() + SENSOR_SEARCH_TIMEOUT;

    while(!bme.begin()) {
        delay(250);
        Serial.print(".");

        if (millis() > timeout) {
            Serial.println("Failed to find BME280 / BMP280 sensor!");
            return false;
        }
    }

    Serial.println();

    switch(bme.chipModel())
    {
        case BME280::ChipModel_BME280:
            Serial.println("Found BME280 sensor!");
        break;

        case BME280::ChipModel_BMP280:
            Serial.println("Found BMP280 sensor! No Humidity available.");
        break;

        default:
            Serial.println("Error, UNKNOWN sensor found!");
            return false;
    }

    return true;
}

SensorData readSensorData() {
    float temperature(NAN), humidity(NAN), pressure(NAN), dewPoint(NAN);

    Serial.println("Gathering BME280 sensor data...");

    delay(500);

    bme.read(pressure, temperature, humidity, BME280::TempUnit_Celsius, BME280::PresUnit_hPa);

    if (!isnan(humidity)) {
        dewPoint = EnvironmentCalculations::DewPoint(temperature, humidity, EnvironmentCalculations::TempUnit_Celsius);
    }

    SensorData data = {
        .temperature = temperature,
        .pressure = pressure,
        .humidity = humidity,
        .dewPoint = dewPoint
    };

    Serial.println("pressure\ttemperature\thumidity\tdewPoint");
    Serial.print(data.pressure);
    Serial.print("\t\t");
    Serial.print(data.temperature);
    Serial.print("\t\t");
    Serial.print(data.humidity);
    Serial.print("\t\t");
    Serial.print(data.dewPoint);
    Serial.println();

    return data;
}

void setupCallback(EasyESP::Watchdog *watchdog, EasyESP::StatusLED *statusLED) {
    HTTPClient http;

    StaticJsonDocument<500> doc;
    doc["name"] = WiFi.hostname();

    if (findBME280()) {
        SensorData currentSensorData = readSensorData();

        doc["pressure"] = currentSensorData.pressure;
        doc["temperature"] = currentSensorData.temperature;
        doc["humidity"] = currentSensorData.humidity;
        doc["dewPoint"] = currentSensorData.dewPoint;
    } else {
        framework.signalError(2);

        doc["error"] = "BME280 / BMP280 not found";
    }

    char payload[120];
    serializeJson(doc, payload);

    Serial.println(String("Payload: ") + payload);

    http.begin(DATA_API_URL);
    http.addHeader("Content-Type", "application/json");
 
    int httpCode = http.POST(payload);
    String response = http.getString();

    Serial.println(String("httpCode: ") + httpCode);
    Serial.println(String("response: ") + response);
 
    http.end();
}

void setup() {
    framework.setup(&setupCallback);
    framework.deepSleep(RUNTIME_INTERVAL - micros());
}

void loop() {}