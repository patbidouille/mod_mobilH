/*
 * storage_manager.h
 * Gestion du stockage persistant (Preferences/EEPROM)
 */

#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Preferences.h>
#include <Arduino.h>

class StorageManager {
private:
  Preferences prefs;
  
  // Clés de stockage
  const char* KEY_LAST_SEND = "last_send";
  const char* KEY_AC_POWER = "ac_power";
  const char* KEY_FIRE_COUNT = "fire_count";
  const char* KEY_PIR_COUNT = "pir_count";
  const char* KEY_BOOT_COUNT = "boot_count";
  const char* KEY_TOTAL_UPTIME = "total_uptime";
  const char* KEY_FAILED_DATA = "failed_data";
  const char* KEY_CONFIG_VERSION = "cfg_version";
  
public:
  StorageManager() {}
  
  // Initialisation
  bool begin() {
    Serial.println("💾 Initialisation stockage...");
    
    // Ouvrir l'espace de stockage
    if (!prefs.begin("surveillance", false)) {
      Serial.println("❌ Échec initialisation Preferences");
      return false;
    }
    
    // Incrémenter le compteur de démarrages
    unsigned int bootCount = prefs.getUInt(KEY_BOOT_COUNT, 0);
    bootCount++;
    prefs.putUInt(KEY_BOOT_COUNT, bootCount);
    
    Serial.printf("✅ Stockage initialisé (Démarrage #%u)\n", bootCount);
    
    return true;
  }
  
  // Sauvegarder le temps du dernier envoi
  void saveLastSendTime(unsigned long time) {
    prefs.putULong(KEY_LAST_SEND, time);
  }
  
  // Charger le temps du dernier envoi
  unsigned long loadLastSendTime() {
    return prefs.getULong(KEY_LAST_SEND, 0);
  }
  
  // Sauvegarder l'état de l'alimentation AC
  void saveACPowerState(bool state) {
    prefs.putBool(KEY_AC_POWER, state);
  }
  
  // Charger l'état de l'alimentation AC
  bool loadACPowerState() {
    return prefs.getBool(KEY_AC_POWER, true); // Par défaut: AC présent
  }
  
  // Sauvegarder un événement incendie
  void saveFireEvent() {
    unsigned int count = prefs.getUInt(KEY_FIRE_COUNT, 0);
    count++;
    prefs.putUInt(KEY_FIRE_COUNT, count);
    Serial.printf("🔥 Événement incendie #%u enregistré\n", count);
  }
  
  // Obtenir le nombre d'événements incendie
  unsigned int getFireEventCount() {
    return prefs.getUInt(KEY_FIRE_COUNT, 0);
  }
  
  // Sauvegarder un événement PIR
  void savePIREvent() {
    unsigned int count = prefs.getUInt(KEY_PIR_COUNT, 0);
    count++;
    prefs.putUInt(KEY_PIR_COUNT, count);
  }
  
  // Obtenir le nombre de détections PIR
  unsigned int getPIREventCount() {
    return prefs.getUInt(KEY_PIR_COUNT, 0);
  }
  
  // Sauvegarder des données qui n'ont pas pu être envoyées
  bool saveFailedData(const String& jsonData) {
    if (jsonData.length() > 4000) {
      Serial.println("❌ Données trop volumineuses pour le stockage");
      return false;
    }
    
    prefs.putString(KEY_FAILED_DATA, jsonData);
    Serial.println("💾 Données non envoyées sauvegardées");
    return true;
  }
  
  // Récupérer les données qui n'ont pas pu être envoyées
  String loadFailedData() {
    return prefs.getString(KEY_FAILED_DATA, "");
  }
  
  // Effacer les données en attente
  void clearFailedData() {
    prefs.remove(KEY_FAILED_DATA);
    Serial.println("🗑️  Données en attente effacées");
  }
  
  // Sauvegarder le temps de fonctionnement total
  void saveTotalUptime(unsigned long uptime) {
    unsigned long totalUptime = prefs.getULong(KEY_TOTAL_UPTIME, 0);
    totalUptime += uptime;
    prefs.putULong(KEY_TOTAL_UPTIME, totalUptime);
  }
  
  // Obtenir le temps de fonctionnement total
  unsigned long getTotalUptime() {
    return prefs.getULong(KEY_TOTAL_UPTIME, 0);
  }
  
  // Obtenir le nombre de démarrages
  unsigned int getBootCount() {
    return prefs.getUInt(KEY_BOOT_COUNT, 0);
  }
  
  // Afficher les statistiques
  void printStatistics() {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║       STATISTIQUES SYSTÈME             ║");
    Serial.println("╚════════════════════════════════════════╝");
    
    unsigned int bootCount = getBootCount();
    unsigned int fireCount = getFireEventCount();
    unsigned int pirCount = getPIREventCount();
    unsigned long totalUptime = getTotalUptime();
    bool acPower = loadACPowerState();
    
    Serial.printf("Démarrages:           %u\n", bootCount);
    Serial.printf("Alertes incendie:     %u\n", fireCount);
    Serial.printf("Détections PIR:       %u\n", pirCount);
    Serial.printf("Uptime total:         %lu heures\n", totalUptime / 3600000);
    Serial.printf("État alimentation:    %s\n", acPower ? "Secteur" : "Batterie");
    
    String failedData = loadFailedData();
    if (failedData.length() > 0) {
      Serial.printf("Données en attente:   %u octets\n", failedData.length());
    } else {
      Serial.println("Données en attente:   Aucune");
    }
    
    Serial.println("╚════════════════════════════════════════╝\n");
  }
  
  // Réinitialiser toutes les statistiques (sauf config)
  void resetStatistics() {
    prefs.remove(KEY_FIRE_COUNT);
    prefs.remove(KEY_PIR_COUNT);
    prefs.remove(KEY_TOTAL_UPTIME);
    prefs.remove(KEY_FAILED_DATA);
    Serial.println("🔄 Statistiques réinitialisées");
  }
  
  // Réinitialisation complète (factory reset)
  void factoryReset() {
    Serial.println("⚠️  Réinitialisation complète...");
    prefs.clear();
    Serial.println("✅ Stockage réinitialisé");
  }
  
  // Sauvegarder une configuration personnalisée
  void saveConfig(const char* key, const String& value) {
    prefs.putString(key, value);
  }
  
  // Charger une configuration personnalisée
  String loadConfig(const char* key, const String& defaultValue = "") {
    return prefs.getString(key, defaultValue);
  }
  
  // Vérifier si une clé existe
  bool hasKey(const char* key) {
    return prefs.isKey(key);
  }
  
  // Obtenir l'espace utilisé (approximatif)
  void printStorageInfo() {
    Serial.println("\n💾 Informations stockage:");
    
    // Compter les clés utilisées
    int keyCount = 0;
    if (prefs.isKey(KEY_LAST_SEND)) keyCount++;
    if (prefs.isKey(KEY_AC_POWER)) keyCount++;
    if (prefs.isKey(KEY_FIRE_COUNT)) keyCount++;
    if (prefs.isKey(KEY_PIR_COUNT)) keyCount++;
    if (prefs.isKey(KEY_BOOT_COUNT)) keyCount++;
    if (prefs.isKey(KEY_TOTAL_UPTIME)) keyCount++;
    if (prefs.isKey(KEY_FAILED_DATA)) keyCount++;
    
    Serial.printf("  Clés utilisées: %d\n", keyCount);
    
    String failedData = loadFailedData();
    if (failedData.length() > 0) {
      Serial.printf("  Données temporaires: %u octets\n", failedData.length());
    }
    
    Serial.println();
  }
  
  // Fermer proprement
  void end() {
    prefs.end();
  }
};

#endif // STORAGE_MANAGER_H
