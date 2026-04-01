/*
 * mqtt_manager.h
 * Gestion de la connexion MQTT et publication des données
 */

#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <PubSubClient.h>
#include "sensor_manager.h"
#include "secret.h"

// Topics MQTT pour Home Assistant
#define TOPIC_BASE "homeassistant/sensor/surveillance"
#define TOPIC_AVAILABILITY "homeassistant/sensor/surveillance/availability"
#define TOPIC_TEMPERATURE "homeassistant/sensor/surveillance/temperature"
#define TOPIC_HUMIDITY "homeassistant/sensor/surveillance/humidity"
#define TOPIC_PRESSURE "homeassistant/sensor/surveillance/pressure"
#define TOPIC_LIGHT "homeassistant/sensor/surveillance/light"
#define TOPIC_VOLTAGE "homeassistant/sensor/surveillance/voltage"
#define TOPIC_CURRENT "homeassistant/sensor/surveillance/current"
#define TOPIC_POWER "homeassistant/sensor/surveillance/power"
#define TOPIC_ENERGY "homeassistant/sensor/surveillance/energy"
#define TOPIC_PIR "homeassistant/binary_sensor/surveillance/motion"
#define TOPIC_FIRE "homeassistant/binary_sensor/surveillance/fire"
#define TOPIC_ALARM_MODE "homeassistant/binary_sensor/surveillance/alarm_mode"
#define TOPIC_AC_POWER "homeassistant/binary_sensor/surveillance/ac_power"

class MQTTManager {
private:
  PubSubClient mqtt;
  const char* server;
  int port;
  const char* user;
  const char* password;
  const char* clientId;
  
public:
  MQTTManager(const char* srv, int prt, const char* usr, const char* pwd) 
    : server(srv), port(prt), user(usr), password(pwd), clientId("t-a7670e-surveillance") {
  }
  
  // Initialisation
  void begin(TinyGsmClient* client) {
    mqtt.setClient(*client);
    mqtt.setServer(server, port);
    mqtt.setKeepAlive(90);
    mqtt.setSocketTimeout(30);
    
    Serial.println("✅ MQTT configuré");
  }
  
  // Connexion au broker
  bool connect() {
    Serial.print("💬 Connexion MQTT à ");
    Serial.print(server);
    Serial.print(":");
    Serial.print(port);
    Serial.print("...");
    
    int attempts = 0;
    while (!mqtt.connected() && attempts < 3) {
      if (mqtt.connect(clientId, user, password, TOPIC_AVAILABILITY, 0, true, "offline")) {
        Serial.println(" ✅");
        mqtt.publish(TOPIC_AVAILABILITY, "online", true);
        return true;
      } else {
        Serial.print(" ❌ (");
        Serial.print(mqtt.state());
        Serial.print(")");
        attempts++;
        if (attempts < 3) {
          Serial.print(" Retry...");
          delay(3000);
        }
      }
    }
    
    Serial.println(" ÉCHEC");
    return false;
  }
  
  // Vérifier connexion
  bool isConnected() {
    return mqtt.connected();
  }
  
  // Loop MQTT
  void loop() {
    mqtt.loop();
  }
  
  // Publier toutes les données des capteurs
  bool publishSensorData(const SensorData& data) {
    if (!mqtt.connected()) {
      Serial.println("❌ MQTT non connecté");
      return false;
    }
    
    bool allSuccess = true;
    char buffer[16];
    
    // Température
    dtostrf(data.temperature, 5, 1, buffer);
    allSuccess &= mqtt.publish(TOPIC_TEMPERATURE, buffer, true);
    
    // Humidité
    dtostrf(data.humidity, 5, 1, buffer);
    allSuccess &= mqtt.publish(TOPIC_HUMIDITY, buffer, true);
    
    // Pression
    dtostrf(data.pressure, 7, 0, buffer);
    allSuccess &= mqtt.publish(TOPIC_PRESSURE, buffer, true);
    
    // Lumière
    dtostrf(data.light, 8, 0, buffer);
    allSuccess &= mqtt.publish(TOPIC_LIGHT, buffer, true);
    
    // Tension
    dtostrf(data.voltage, 6, 1, buffer);
    allSuccess &= mqtt.publish(TOPIC_VOLTAGE, buffer, true);
    
    // Courant
    dtostrf(data.current, 6, 2, buffer);
    allSuccess &= mqtt.publish(TOPIC_CURRENT, buffer, true);
    
    // Puissance
    dtostrf(data.power, 7, 1, buffer);
    allSuccess &= mqtt.publish(TOPIC_POWER, buffer, true);
    
    // Énergie
    dtostrf(data.energy, 8, 2, buffer);
    allSuccess &= mqtt.publish(TOPIC_ENERGY, buffer, true);
    
    // États binaires
    allSuccess &= mqtt.publish(TOPIC_PIR, data.pirDetected ? "ON" : "OFF", true);
    allSuccess &= mqtt.publish(TOPIC_FIRE, data.fireDetected ? "ON" : "OFF", true);
    allSuccess &= mqtt.publish(TOPIC_ALARM_MODE, data.alarmMode ? "ON" : "OFF", true);
    allSuccess &= mqtt.publish(TOPIC_AC_POWER, data.acPower ? "ON" : "OFF", true);
    
    // Disponibilité
    mqtt.publish(TOPIC_AVAILABILITY, "online", true);
    
    return allSuccess;
  }
  
  // Publier détection PIR uniquement
  bool publishPIR(bool detected) {
    return mqtt.publish(TOPIC_PIR, detected ? "ON" : "OFF", true);
  }
  
  // Publier alarme incendie
  bool publishFire(bool detected) {
    return mqtt.publish(TOPIC_FIRE, detected ? "ON" : "OFF", true);
  }
  
  // Publier mode alarme
  bool publishAlarmMode(bool active) {
    return mqtt.publish(TOPIC_ALARM_MODE, active ? "ON" : "OFF", true);
  }
  
  // Envoyer messages de découverte Home Assistant
  void sendDiscovery() {
    Serial.println("📢 Envoi messages de découverte Home Assistant...");
    
    // Configuration température
    const char* tempConfig = "{"
      "\"name\":\"Température\","
      "\"state_topic\":\"" TOPIC_TEMPERATURE "\","
      "\"unit_of_measurement\":\"°C\","
      "\"device_class\":\"temperature\","
      "\"availability_topic\":\"" TOPIC_AVAILABILITY "\","
      "\"unique_id\":\"t7670_temp\","
      "\"device\":{\"identifiers\":[\"t7670_surveillance\"],\"name\":\"Surveillance LTE-M\",\"model\":\"T-A7670E\",\"manufacturer\":\"LilyGO\"}"
    "}";
    mqtt.publish("homeassistant/sensor/t7670_temp/config", tempConfig, true);
    
    // Configuration humidité
    const char* humConfig = "{"
      "\"name\":\"Humidité\","
      "\"state_topic\":\"" TOPIC_HUMIDITY "\","
      "\"unit_of_measurement\":\"%\","
      "\"device_class\":\"humidity\","
      "\"availability_topic\":\"" TOPIC_AVAILABILITY "\","
      "\"unique_id\":\"t7670_hum\","
      "\"device\":{\"identifiers\":[\"t7670_surveillance\"]}"
    "}";
    mqtt.publish("homeassistant/sensor/t7670_hum/config", humConfig, true);
    
    // Configuration pression
    const char* pressConfig = "{"
      "\"name\":\"Pression\","
      "\"state_topic\":\"" TOPIC_PRESSURE "\","
      "\"unit_of_measurement\":\"hPa\","
      "\"device_class\":\"pressure\","
      "\"availability_topic\":\"" TOPIC_AVAILABILITY "\","
      "\"unique_id\":\"t7670_press\","
      "\"device\":{\"identifiers\":[\"t7670_surveillance\"]}"
    "}";
    mqtt.publish("homeassistant/sensor/t7670_press/config", pressConfig, true);
    
    // Configuration lumière
    const char* lightConfig = "{"
      "\"name\":\"Luminosité\","
      "\"state_topic\":\"" TOPIC_LIGHT "\","
      "\"unit_of_measurement\":\"lux\","
      "\"device_class\":\"illuminance\","
      "\"availability_topic\":\"" TOPIC_AVAILABILITY "\","
      "\"unique_id\":\"t7670_light\","
      "\"device\":{\"identifiers\":[\"t7670_surveillance\"]}"
    "}";
    mqtt.publish("homeassistant/sensor/t7670_light/config", lightConfig, true);
    
    // Configuration tension
    const char* voltConfig = "{"
      "\"name\":\"Tension\","
      "\"state_topic\":\"" TOPIC_VOLTAGE "\","
      "\"unit_of_measurement\":\"V\","
      "\"device_class\":\"voltage\","
      "\"availability_topic\":\"" TOPIC_AVAILABILITY "\","
      "\"unique_id\":\"t7670_volt\","
      "\"device\":{\"identifiers\":[\"t7670_surveillance\"]}"
    "}";
    mqtt.publish("homeassistant/sensor/t7670_volt/config", voltConfig, true);
    
    // Configuration courant
    const char* currConfig = "{"
      "\"name\":\"Courant\","
      "\"state_topic\":\"" TOPIC_CURRENT "\","
      "\"unit_of_measurement\":\"A\","
      "\"device_class\":\"current\","
      "\"availability_topic\":\"" TOPIC_AVAILABILITY "\","
      "\"unique_id\":\"t7670_curr\","
      "\"device\":{\"identifiers\":[\"t7670_surveillance\"]}"
    "}";
    mqtt.publish("homeassistant/sensor/t7670_curr/config", currConfig, true);
    
    // Configuration puissance
    const char* powerConfig = "{"
      "\"name\":\"Puissance\","
      "\"state_topic\":\"" TOPIC_POWER "\","
      "\"unit_of_measurement\":\"W\","
      "\"device_class\":\"power\","
      "\"availability_topic\":\"" TOPIC_AVAILABILITY "\","
      "\"unique_id\":\"t7670_power\","
      "\"device\":{\"identifiers\":[\"t7670_surveillance\"]}"
    "}";
    mqtt.publish("homeassistant/sensor/t7670_power/config", powerConfig, true);
    
    // Configuration énergie
    const char* energyConfig = "{"
      "\"name\":\"Énergie\","
      "\"state_topic\":\"" TOPIC_ENERGY "\","
      "\"unit_of_measurement\":\"kWh\","
      "\"device_class\":\"energy\","
      "\"state_class\":\"total_increasing\","
      "\"availability_topic\":\"" TOPIC_AVAILABILITY "\","
      "\"unique_id\":\"t7670_energy\","
      "\"device\":{\"identifiers\":[\"t7670_surveillance\"]}"
    "}";
    mqtt.publish("homeassistant/sensor/t7670_energy/config", energyConfig, true);
    
    // Configuration PIR
    const char* pirConfig = "{"
      "\"name\":\"Détecteur mouvement\","
      "\"state_topic\":\"" TOPIC_PIR "\","
      "\"device_class\":\"motion\","
      "\"availability_topic\":\"" TOPIC_AVAILABILITY "\","
      "\"unique_id\":\"t7670_pir\","
      "\"device\":{\"identifiers\":[\"t7670_surveillance\"]}"
    "}";
    mqtt.publish("homeassistant/binary_sensor/t7670_pir/config", pirConfig, true);
    
    // Configuration détecteur incendie
    const char* fireConfig = "{"
      "\"name\":\"Détecteur incendie\","
      "\"state_topic\":\"" TOPIC_FIRE "\","
      "\"device_class\":\"smoke\","
      "\"availability_topic\":\"" TOPIC_AVAILABILITY "\","
      "\"unique_id\":\"t7670_fire\","
      "\"device\":{\"identifiers\":[\"t7670_surveillance\"]}"
    "}";
    mqtt.publish("homeassistant/binary_sensor/t7670_fire/config", fireConfig, true);
    
    // Configuration mode alarme
    const char* alarmConfig = "{"
      "\"name\":\"Mode alarme\","
      "\"state_topic\":\"" TOPIC_ALARM_MODE "\","
      "\"availability_topic\":\"" TOPIC_AVAILABILITY "\","
      "\"unique_id\":\"t7670_alarm\","
      "\"device\":{\"identifiers\":[\"t7670_surveillance\"]}"
    "}";
    mqtt.publish("homeassistant/binary_sensor/t7670_alarm/config", alarmConfig, true);
    
    // Configuration alimentation secteur
    const char* acConfig = "{"
      "\"name\":\"Alimentation secteur\","
      "\"state_topic\":\"" TOPIC_AC_POWER "\","
      "\"device_class\":\"power\","
      "\"availability_topic\":\"" TOPIC_AVAILABILITY "\","
      "\"unique_id\":\"t7670_ac\","
      "\"device\":{\"identifiers\":[\"t7670_surveillance\"]}"
    "}";
    mqtt.publish("homeassistant/binary_sensor/t7670_ac/config", acConfig, true);
    
    Serial.println("✅ Découverte Home Assistant envoyée");
  }
};

#endif // MQTT_MANAGER_H
