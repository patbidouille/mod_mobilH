/*
 * json_builder.h
 * Construction de payloads JSON pour les données
 */

#ifndef JSON_BUILDER_H
#define JSON_BUILDER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "sensor_manager.h"

class JSONBuilder {
private:
  StaticJsonDocument<1024> doc;
  
public:
  JSONBuilder() {}
  
  // Construire le payload JSON complet
  String buildFullPayload(const SensorData& data) {
    doc.clear();
    
    // Données environnementales
    JsonObject environment = doc.createNestedObject("environment");
    environment["temperature"] = roundf(data.temperature * 10) / 10.0;
    environment["humidity"] = roundf(data.humidity * 10) / 10.0;
    environment["pressure"] = roundf(data.pressure);
    environment["light"] = roundf(data.light);
    
    // Données électriques
    JsonObject electrical = doc.createNestedObject("electrical");
    electrical["voltage"] = roundf(data.voltage * 10) / 10.0;
    electrical["current"] = roundf(data.current * 100) / 100.0;
    electrical["power"] = roundf(data.power * 10) / 10.0;
    electrical["energy"] = roundf(data.energy * 100) / 100.0;
    
    // États des capteurs binaires
    JsonObject status = doc.createNestedObject("status");
    status["pir"] = data.pirDetected;
    status["fire"] = data.fireDetected;
    status["alarm_mode"] = data.alarmMode;
    status["ac_power"] = data.acPower;
    
    // Timestamp
    doc["timestamp"] = millis();
    
    String output;
    serializeJson(doc, output);
    return output;
  }
  
  // Construire payload compact (données essentielles uniquement)
  String buildCompactPayload(const SensorData& data) {
    doc.clear();
    
    doc["t"] = roundf(data.temperature * 10) / 10.0;
    doc["h"] = roundf(data.humidity * 10) / 10.0;
    doc["l"] = roundf(data.light);
    doc["v"] = roundf(data.voltage * 10) / 10.0;
    doc["p"] = roundf(data.power * 10) / 10.0;
    doc["pir"] = data.pirDetected ? 1 : 0;
    doc["fire"] = data.fireDetected ? 1 : 0;
    doc["alarm"] = data.alarmMode ? 1 : 0;
    doc["ac"] = data.acPower ? 1 : 0;
    
    String output;
    serializeJson(doc, output);
    return output;
  }
  
  // Construire payload d'alerte
  String buildAlertPayload(const char* alertType, const SensorData& data) {
    doc.clear();
    
    doc["alert_type"] = alertType;
    doc["timestamp"] = millis();
    
    if (strcmp(alertType, "fire") == 0) {
      doc["temperature"] = roundf(data.temperature * 10) / 10.0;
      doc["location"] = "surveillance_point";
      doc["severity"] = "critical";
    } else if (strcmp(alertType, "intrusion") == 0) {
      doc["alarm_mode"] = data.alarmMode;
      doc["light"] = roundf(data.light);
      doc["severity"] = "high";
    } else if (strcmp(alertType, "power_loss") == 0) {
      doc["voltage"] = roundf(data.voltage * 10) / 10.0;
      doc["battery_mode"] = !data.acPower;
      doc["severity"] = "medium";
    }
    
    String output;
    serializeJson(doc, output);
    return output;
  }
  
  // Construire payload de diagnostic
  String buildDiagnosticPayload(int signalQuality, const char* ipAddress, unsigned long uptime) {
    doc.clear();
    
    doc["signal_quality"] = signalQuality;
    doc["signal_percent"] = (signalQuality * 100) / 31;
    doc["ip_address"] = ipAddress;
    doc["uptime_ms"] = uptime;
    doc["uptime_hours"] = uptime / 3600000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["heap_size"] = ESP.getHeapSize();
    doc["wifi_connected"] = false; // Pas de WiFi sur ce module
    doc["lte_connected"] = true;
    
    String output;
    serializeJson(doc, output);
    return output;
  }
  
  // Parser un JSON reçu
  bool parseJSON(const String& jsonString, SensorData& data) {
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
      Serial.print("❌ Erreur parsing JSON: ");
      Serial.println(error.c_str());
      return false;
    }
    
    // Extraction des données avec support des formats compacts et complets
    if (doc.containsKey("temperature") || doc.containsKey("t")) {
      data.temperature = doc.containsKey("temperature") ? doc["temperature"].as<float>() : doc["t"].as<float>();
    }
    if (doc.containsKey("humidity") || doc.containsKey("h")) {
      data.humidity = doc.containsKey("humidity") ? doc["humidity"].as<float>() : doc["h"].as<float>();
    }
    if (doc.containsKey("pressure")) {
      data.pressure = doc["pressure"];
    }
    if (doc.containsKey("light") || doc.containsKey("l")) {
      data.light = doc.containsKey("light") ? doc["light"].as<float>() : doc["l"].as<float>();
    }
    if (doc.containsKey("voltage") || doc.containsKey("v")) {
      data.voltage = doc.containsKey("voltage") ? doc["voltage"].as<float>() : doc["v"].as<float>();
    }
    if (doc.containsKey("current")) {
      data.current = doc["current"];
    }
    if (doc.containsKey("power") || doc.containsKey("p")) {
      data.power = doc.containsKey("power") ? doc["power"].as<float>() : doc["p"].as<float>();
    }
    if (doc.containsKey("energy")) {
      data.energy = doc["energy"];
    }
    if (doc.containsKey("pir")) {
      data.pirDetected = doc["pir"].as<bool>();
    }
    if (doc.containsKey("fire")) {
      data.fireDetected = doc["fire"].as<bool>();
    }
    if (doc.containsKey("alarm") || doc.containsKey("alarm_mode")) {
      data.alarmMode = doc.containsKey("alarm") ? doc["alarm"].as<bool>() : doc["alarm_mode"].as<bool>();
    }
    if (doc.containsKey("ac") || doc.containsKey("ac_power")) {
      data.acPower = doc.containsKey("ac") ? doc["ac"].as<bool>() : doc["ac_power"].as<bool>();
    }
    
    return true;
  }
  
  // Créer un JSON de configuration pour Home Assistant
  String buildHADiscoveryConfig(const char* component, const char* objectId, 
                                 const char* name, const char* deviceClass,
                                 const char* stateTopic, const char* unit = nullptr) {
    doc.clear();
    
    doc["name"] = name;
    doc["state_topic"] = stateTopic;
    doc["availability_topic"] = "homeassistant/sensor/surveillance/availability";
    doc["unique_id"] = String("t7670_") + objectId;
    
    if (unit != nullptr) {
      doc["unit_of_measurement"] = unit;
    }
    
    if (deviceClass != nullptr && strlen(deviceClass) > 0) {
      doc["device_class"] = deviceClass;
    }
    
    // Information du device
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add("t7670_surveillance");
    device["name"] = "Surveillance LTE-M";
    device["model"] = "T-A7670E";
    device["manufacturer"] = "LilyGO";
    device["sw_version"] = "1.0.0";
    
    String output;
    serializeJson(doc, output);
    return output;
  }
  
  // Afficher un JSON formaté
  void printJSON(const String& jsonString) {
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
      Serial.println("❌ JSON invalide");
      return;
    }
    
    serializeJsonPretty(doc, Serial);
    Serial.println();
  }
  
  // Obtenir la taille du JSON
  size_t getJSONSize(const String& jsonString) {
    return jsonString.length();
  }
  
  // Valider un JSON
  bool isValidJSON(const String& jsonString) {
    DeserializationError error = deserializeJson(doc, jsonString);
    return !error;
  }
  
  // Créer un JSON de statistiques système
  String buildSystemStats(unsigned long uptime, int bootCount, int fireEvents, int pirEvents) {
    doc.clear();
    
    doc["uptime_ms"] = uptime;
    doc["uptime_hours"] = uptime / 3600000;
    doc["boot_count"] = bootCount;
    doc["fire_events"] = fireEvents;
    doc["pir_events"] = pirEvents;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["heap_size"] = ESP.getHeapSize();
    doc["chip_model"] = ESP.getChipModel();
    doc["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
    
    String output;
    serializeJson(doc, output);
    return output;
  }
  
  // Minimiser un JSON (retirer espaces)
  String minifyJSON(const String& jsonString) {
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) return jsonString;
    
    String output;
    serializeJson(doc, output);
    return output;
  }
  
  // Embellir un JSON (avec indentation)
  String beautifyJSON(const String& jsonString) {
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) return jsonString;
    
    String output;
    serializeJsonPretty(doc, output);
    return output;
  }
  
  // Extraire une valeur spécifique d'un JSON
  String extractValue(const String& jsonString, const char* key) {
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) return "";
    
    if (doc.containsKey(key)) {
      if (doc[key].is<const char*>()) {
        return String(doc[key].as<const char*>());
      } else if (doc[key].is<int>()) {
        return String(doc[key].as<int>());
      } else if (doc[key].is<float>()) {
        return String(doc[key].as<float>(), 2);
      } else if (doc[key].is<bool>()) {
        return doc[key].as<bool>() ? "true" : "false";
      }
    }
    
    return "";
  }
  
  // Fusionner deux JSON
  String mergeJSON(const String& json1, const String& json2) {
    StaticJsonDocument<512> doc1;
    StaticJsonDocument<512> doc2;
    
    deserializeJson(doc1, json1);
    deserializeJson(doc2, json2);
    
    // Copier doc2 dans doc1 (doc2 écrase les valeurs de doc1)
    JsonObject obj1 = doc1.as<JsonObject>();
    JsonObject obj2 = doc2.as<JsonObject>();
    
    for (JsonPair kv : obj2) {
      obj1[kv.key()] = kv.value();
    }
    
    String output;
    serializeJson(doc1, output);
    return output;
  }
};

#endif // JSON_BUILDER_H
