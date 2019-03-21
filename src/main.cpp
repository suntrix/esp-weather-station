// #define EASYESP_STATUS_LED_FLASH_ENABLED 1
// #define EASYESP_WATCHDOG_SETUP_TIMEOUT 15e3

#include "secrets.h"

#include <Framework.h>

#include <brzo_i2c.h>
#include <BME280I2C_BRZO.h>
#include <EnvironmentCalculations.h>

#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

EasyESP::Config config = {
    .ssid = SECRETS_WIFI_SSID,
    .password = SECRETS_WIFI_PASSWORD,
    .otaHttpsUrl = SECRETS_OTA_SERVER_HTTPS_URL,
    .otaHttpsFingerPrint = SECRETS_OTA_SERVER_FINGERPRINT,
    .otaHttpUrl = SECRETS_OTA_SERVER_HTTP_URL,
    .versionName = "2.0.1"
};

EasyESP::Framework framework(config);

BME280I2C_BRZO::Settings settings(
   BME280::OSR_X1,
   BME280::OSR_X1,
   BME280::OSR_X1,
   BME280::Mode_Forced,
   BME280::StandbyTime_1000ms,
   BME280::Filter_Off,
   BME280::SpiEnable_False,
   BME280I2C::I2CAddr_0x76
);

BME280I2C_BRZO bme(settings);

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

    http.begin(SECRETS_DATA_API_URL);
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