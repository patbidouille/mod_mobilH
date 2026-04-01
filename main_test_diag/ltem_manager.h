/*
 * ltem_manager.h
 * Gestion connexion LTE Cat-1 — LILYGO T-A7670E R2
 *
 * ⚠️ Le T-A7670E est LTE Cat-1 (pas Cat-M / NB-IoT)
 *    → AT+CMNB et AT+CBANDCFG="CAT-M" ne sont PAS supportés
 *    → Enregistrement LTE vérifié via AT+CEREG (EPS), pas AT+CREG
 */

#ifndef LTEM_MANAGER_H
#define LTEM_MANAGER_H

#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>
#include "secret.h"

// ── Pins modem T-A7670E R2 ────────────────────────────────────
#define MODEM_RX     27   // ESP reçoit ← Modem TX (GPIO26 sur la carte)
#define MODEM_TX     26   // ESP envoie → Modem RX (GPIO27 sur la carte)
#define MODEM_PWRKEY  4
#define MODEM_DTR    25
#define MODEM_RI     33

class LTEMManager {
private:
  HardwareSerial SerialAT;
  TinyGsm*       modem;
  TinyGsmClient* client;
  bool           connected;
  int            signalQuality;

  String sendAT(const char* cmd, uint16_t waitMs = 800) {
    while (SerialAT.available()) SerialAT.read();
    SerialAT.println(cmd);
    delay(waitMs);
    String r = "";
    while (SerialAT.available()) r += (char)SerialAT.read();
    r.trim();
    return r;
  }

public:
  LTEMManager() : SerialAT(1), connected(false), signalQuality(0) {
    modem  = new TinyGsm(SerialAT);
    client = new TinyGsmClient(*modem);
  }
  ~LTEMManager() { delete client; delete modem; }

  // ── Initialisation + power-on ─────────────────────────────
  bool begin() {
    Serial.println("⚙️  Init modem T-A7670E Cat-1...");

    pinMode(MODEM_PWRKEY, OUTPUT);
    pinMode(MODEM_DTR,    OUTPUT);
    digitalWrite(MODEM_PWRKEY, LOW);
    digitalWrite(MODEM_DTR,    LOW);

    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(1000);

    // Tester si le modem répond déjà (cas reset logiciel)
    String r = sendAT("AT", 1000);
    if (r.indexOf("OK") >= 0) {
      Serial.println("✅ Modem déjà actif");
      configureLTE();
      return true;
    }

    // Sinon : pulse PWRKEY 800 ms pour démarrer
    Serial.println("⚡ Power-on PWRKEY...");
    digitalWrite(MODEM_PWRKEY, HIGH); delay(800);
    digitalWrite(MODEM_PWRKEY, LOW);
    Serial.println("Attente démarrage modem (15 s)...");
    delay(15000);

    r = sendAT("AT", 1000);
    if (r.indexOf("OK") < 0) {
      Serial.println("❌ Modem ne répond pas");
      return false;
    }

    Serial.println("✅ Modem démarré");
    configureLTE();
    return true;
  }

  // ── Configuration LTE Cat-1 ───────────────────────────────
  void configureLTE() {
    sendAT("ATE0");                     // echo OFF
    sendAT("AT+CNMP=38");               // LTE uniquement
    // ⚠️ AT+CMNB et AT+CBANDCFG="CAT-M" supprimés : non supportés sur Cat-1

    // Itinérance (SIM Things Mobile / réseau Orange FR)
    sendAT("AT+CTZU=1");                // sync heure réseau
    sendAT("AT+CREG=2");                // mode enregistrement CS complet
    sendAT("AT+CGREG=2");               // mode enregistrement GPRS complet
    sendAT("AT+CEREG=2");               // mode enregistrement EPS/LTE ← clé pour Cat-1

    // Centre SMS Things Mobile
    sendAT("AT+CMGF=1");                // format texte
    sendAT("AT+CSCA=\"" SMS_CENTER "\"");

    Serial.println("✅ Configuration LTE Cat-1 terminée");
  }

  // ── Connexion réseau ──────────────────────────────────────
  bool connect() {
    Serial.println("🌐 Connexion réseau LTE...");

    // Sur Cat-1 : vérifier CEREG (EPS) et non CREG (CS)
    // CEREG ,1 = enregistré local  ,5 = itinérance
    Serial.print("Enregistrement EPS");
    unsigned long t = millis();
    while (millis() - t < 60000) {
      String cereg = sendAT("AT+CEREG?", 500);
      if (cereg.indexOf(",1") >= 0 || cereg.indexOf(",5") >= 0) {
        Serial.println(" ✅");
        break;
      }
      Serial.print(".");
      delay(3000);
      if (millis() - t > 59000) {
        Serial.println("\n❌ Timeout enregistrement EPS");
        return false;
      }
    }

    displayNetworkInfo();

    Serial.println("🔗 Activation contexte données...");
    if (!modem->gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
      Serial.println("❌ Échec gprsConnect");
      return false;
    }

    connected     = true;
    signalQuality = modem->getSignalQuality();
    Serial.printf("✅ IP: %s  Signal: %d/31\n",
                  modem->getLocalIP().c_str(), signalQuality);
    return true;
  }

  // ── Vérification connexion ────────────────────────────────
  bool isConnected() {
    if (!connected) return false;
    // Vérifier via CEREG pour LTE Cat-1
    String cereg = sendAT("AT+CEREG?", 300);
    if (cereg.indexOf(",1") < 0 && cereg.indexOf(",5") < 0) {
      connected = false;
      return false;
    }
    return true;
  }

  TinyGsmClient* getClient()        { return client; }
  int  getSignalQuality()           { signalQuality = modem->getSignalQuality(); return signalQuality; }

  // ── Envoi SMS ─────────────────────────────────────────────
  bool sendSMS(const char* phone, const String& message) {
    Serial.printf("📱 SMS → %s : %s\n", phone, message.c_str());
    bool ok = modem->sendSMS(phone, message);
    Serial.println(ok ? "✅ SMS envoyé" : "❌ Échec SMS");
    return ok;
  }

  // ── Infos réseau ──────────────────────────────────────────
  void displayNetworkInfo() {
    Serial.printf("📡 Opérateur: %s\n", modem->getOperator().c_str());
    String cpsi = sendAT("AT+CPSI?");
    Serial.printf("📡 Réseau:    %s\n", cpsi.c_str());
    // CEREG ,5 = itinérance
    String cereg = sendAT("AT+CEREG?");
    Serial.printf("📡 CEREG:     %s %s\n", cereg.c_str(),
                  cereg.indexOf(",5") >= 0 ? "(itinérance ✅)" :
                  cereg.indexOf(",1") >= 0 ? "(local ✅)" : "(?)");
  }

  // ── Diagnostic ────────────────────────────────────────────
  void runDiagnostics() {
    Serial.println("╔══════════ DIAGNOSTIC LTE Cat-1 ══════════╗");
    Serial.println(sendAT("AT+CPIN?").c_str());
    Serial.printf("Signal: %d/31\n", modem->getSignalQuality());
    Serial.println(sendAT("AT+CEREG?").c_str());
    Serial.println(sendAT("AT+CPSI?").c_str());
    if (connected) Serial.printf("IP: %s\n", modem->getLocalIP().c_str());
    Serial.println("╚══════════════════════════════════════════╝");
  }
};

#endif // LTEM_MANAGER_H
