/*
 * sensor_manager.h
 * Gestion BME280 (GY-BMME-B-280), TSL2561 et PZEM-004T
 *
 * Pins corrigés :
 *   I2C  SDA → GPIO 21 | SCL → GPIO 22
 *   PZEM RX  → GPIO 34 | TX  → GPIO 18
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_TSL2561_U.h>
#include <PZEM004Tv30.h>

// ── Structure de données capteurs ────────────────────────────
struct SensorData {
  float temperature;
  float humidity;
  float pressure;
  float light;
  float voltage;
  float current;
  float power;
  float energy;
  bool  pirDetected;
  bool  fireDetected;
  bool  alarmMode;
  bool  acPower;

  SensorData() : temperature(0), humidity(0), pressure(0), light(0),
                 voltage(0), current(0), power(0), energy(0),
                 pirDetected(false), fireDetected(false),
                 alarmMode(false), acPower(true) {}
};

// ── Classe SensorManager ─────────────────────────────────────
class SensorManager {
private:
  Adafruit_BME280        bme;
  Adafruit_TSL2561_Unified tsl;
  PZEM004Tv30*           pzem;

  bool bmeOK;
  bool tslOK;
  bool pzemOK;

public:
  SensorManager()
    : tsl(TSL2561_ADDR_FLOAT, 12345),
      bmeOK(false), tslOK(false), pzemOK(false) {}

  // ── Initialisation ──────────────────────────────────────────
  bool begin(uint8_t sdaPin, uint8_t sclPin,
             uint8_t pzemRx, uint8_t pzemTx) {
    Serial.println("🔧 Init capteurs...");
    Wire.begin(sdaPin, sclPin);
    delay(100);

    // BME280
    Serial.print("  BME280... ");
    bmeOK = bme.begin(0x76);
    if (!bmeOK) bmeOK = bme.begin(0x77);
    if (bmeOK) {
      bme.setSampling(Adafruit_BME280::MODE_FORCED,
                      Adafruit_BME280::SAMPLING_X1,
                      Adafruit_BME280::SAMPLING_X1,
                      Adafruit_BME280::SAMPLING_X1,
                      Adafruit_BME280::FILTER_OFF);
      Serial.println("✅");
    } else {
      Serial.println("❌");
    }

    // TSL2561
    Serial.print("  TSL2561... ");
    tslOK = tsl.begin();
    if (tslOK) {
      tsl.enableAutoRange(true);
      tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);
      Serial.println("✅");
    } else {
      Serial.println("❌");
    }

    // PZEM-004T — Serial2 sur RX=34, TX=18
    Serial.print("  PZEM-004T... ");
    pzem = new PZEM004Tv30(Serial2, pzemRx, pzemTx);
    delay(1000);
    float testV = pzem->voltage();
    pzemOK = (!isnan(testV) && testV >= 0);
    Serial.println(pzemOK ? "✅" : "❌ (vérifier câblage)");

    return (bmeOK || tslOK || pzemOK);
  }

  // ── Lecture de tous les capteurs ────────────────────────────
  SensorData readAll() {
    SensorData d;

    if (bmeOK) {
      bme.takeForcedMeasurement();
      d.temperature = bme.readTemperature();
      d.humidity    = bme.readHumidity();
      d.pressure    = bme.readPressure() / 100.0F;
      if (isnan(d.temperature)) d.temperature = 0;
      if (isnan(d.humidity))    d.humidity    = 0;
      if (isnan(d.pressure))    d.pressure    = 0;
    }

    if (tslOK) {
      sensors_event_t ev;
      tsl.getEvent(&ev);
      d.light = (ev.light > 0) ? ev.light : 0;
    }

    if (pzemOK) {
      d.voltage = pzem->voltage();
      d.current = pzem->current();
      d.power   = pzem->power();
      d.energy  = pzem->energy();
      if (isnan(d.voltage)) d.voltage = 0;
      if (isnan(d.current)) d.current = 0;
      if (isnan(d.power))   d.power   = 0;
      if (isnan(d.energy))  d.energy  = 0;
      if (d.voltage < 50)   { d.current = 0; d.power = 0; }
    }

    return d;
  }

  void printStatus() {
    Serial.printf("  BME280:   %s\n", bmeOK  ? "✅" : "❌");
    Serial.printf("  TSL2561:  %s\n", tslOK  ? "✅" : "❌");
    Serial.printf("  PZEM-004T:%s\n", pzemOK ? "✅" : "❌");
  }

  bool resetEnergy() {
    if (!pzemOK) return false;
    return pzem->resetEnergy();
  }
};

#endif // SENSOR_MANAGER_H
