/*
 * main_test_diag.ino — v2
 * Diagnostic robuste : teste d'abord si le modem répond SANS power-on,
 * puis essaie plusieurs baud rates, puis tente la séquence PWRKEY.
 */

#define TINY_GSM_MODEM_SIM7600
#include "secret.h"
#include <TinyGsmClient.h>

// ── Pins modem T-A7670E R2 ────────────────────────────────────
#define MODEM_RX     26   // ESP reçoit ← Modem TX
#define MODEM_TX     27   // ESP envoie → Modem RX
#define MODEM_PWRKEY  4
#define MODEM_DTR    25

HardwareSerial SerialAT(1);
TinyGsm        modem(SerialAT);

// ── Envoi AT avec affichage réponse ───────────────────────────
String sendAT(const char* cmd, uint16_t waitMs = 1500) {
  while (SerialAT.available()) SerialAT.read();
  SerialAT.println(cmd);
  delay(waitMs);
  String resp = "";
  while (SerialAT.available()) resp += (char)SerialAT.read();
  resp.trim();
  Serial.printf(">>> %-20s  →  [%s]\n", cmd, resp.length() ? resp.c_str() : "(pas de réponse)");
  return resp;
}

// ── Test AT sur un baud rate donné ───────────────────────────
bool testBaud(uint32_t baud) {
  Serial.printf("\n--- Test baud %d ---\n", baud);
  SerialAT.begin(baud, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(500);
  String r = sendAT("AT");
  if (r.indexOf("OK") >= 0) {
    Serial.printf("✅ Modem répond à %d baud !\n", baud);
    return true;
  }
  SerialAT.end();
  return false;
}

// ── Séquence power-on PWRKEY ──────────────────────────────────
void powerOnModem() {
  Serial.println("\n⚡ Séquence PWRKEY (800 ms)...");
  digitalWrite(MODEM_PWRKEY, LOW);  delay(100);
  digitalWrite(MODEM_PWRKEY, HIGH); delay(800);   // T-A7670E : 800 ms
  digitalWrite(MODEM_PWRKEY, LOW);
  Serial.println("Attente démarrage modem (15 s)...");
  delay(15000);
}

// ── setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("\n╔══════════════════════════════════════╗");
  Serial.println("║  DIAGNOSTIC LTE-M T-A7670E  v2      ║");
  Serial.println("╚══════════════════════════════════════╝");

  pinMode(MODEM_PWRKEY, OUTPUT);
  pinMode(MODEM_DTR,    OUTPUT);
  digitalWrite(MODEM_PWRKEY, LOW);
  digitalWrite(MODEM_DTR,    LOW);

  // ════════════════════════════════════════
  // PHASE 1 : Modem déjà allumé ? (sans PWRKEY)
  // ════════════════════════════════════════
  Serial.println("\n══ PHASE 1 : Modem déjà actif ? (pas de PWRKEY) ══");
  uint32_t bauds[] = {115200, 9600, 57600, 38400};
  bool found = false;
  for (uint32_t b : bauds) {
    if (testBaud(b)) { found = true; break; }
  }

  if (!found) {
    // ════════════════════════════════════════
    // PHASE 2 : Première tentative PWRKEY
    // ════════════════════════════════════════
    Serial.println("\n══ PHASE 2 : Tentative power-on #1 ══");
    powerOnModem();
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(500);
    String r = sendAT("AT");
    if (r.indexOf("OK") >= 0) {
      found = true;
      Serial.println("✅ Modem répond après power-on #1 !");
    }
  }

  if (!found) {
    // ════════════════════════════════════════
    // PHASE 3 : Deuxième tentative PWRKEY
    // (cas où le modem était allumé → on l'a éteint → on le rallume)
    // ════════════════════════════════════════
    Serial.println("\n══ PHASE 3 : Tentative power-on #2 (modem était peut-être ON→OFF→ON) ══");
    powerOnModem();
    for (uint32_t b : bauds) {
      if (testBaud(b)) { found = true; break; }
    }
  }

  if (!found) {
    Serial.println("\n❌ MODEM NE RÉPOND PAS — causes possibles :");
    Serial.println("   1. Pins RX/TX inversés → vérifier câblage");
    Serial.println("   2. Alimentation insuffisante → vérifier 3.7V batterie ou USB");
    Serial.println("   3. Carte défectueuse");
    Serial.println("\nPassage en mode passerelle série manuelle (tapez AT dans le moniteur)");
  } else {
    // ════════════════════════════════════════
    // PHASE 4 : Diagnostic complet
    // ════════════════════════════════════════
    Serial.println("\n══ PHASE 4 : Diagnostic complet ══");

    sendAT("ATE1");           // echo ON
    sendAT("AT+GMR", 2000);   // firmware
    sendAT("AT+GSN");         // IMEI
    sendAT("AT+CPIN?");       // SIM
    sendAT("AT+ICCID");       // ICCID
    sendAT("AT+CSQ");         // signal
    sendAT("AT+CREG?");       // enregistrement
    sendAT("AT+CPSI?", 2000); // type réseau détaillé
    sendAT("AT+COPS?", 2000); // opérateur

    Serial.println("\n══ Config LTE-M ══");
    sendAT("AT+CNMP=38");

    Serial.println("\n══ Attente réseau (90 s max) ══");
    for (int i = 0; i < 18; i++) {
      String cr = sendAT("AT+CREG?", 500);
      String cs = sendAT("AT+CPSI?", 500);
      sendAT("AT+CSQ", 500);
      if (cr.indexOf(",1") >= 0 || cr.indexOf(",5") >= 0 || cr.indexOf(",7") >= 0) {
        Serial.println("✅ Réseau enregistré !");
        break;
      }
      Serial.printf("  [%d/18] pas encore enregistré...\n", i+1);
      delay(5000);
    }

    Serial.println("\n══ Connexion données ══");
    sendAT("AT+CGDCONT=1,\"IP\",\"" APN "\"");
    sendAT("AT+CGACT=1,1", 8000);
    sendAT("AT+CGPADDR=1");

    Serial.println("\n══ Test TinyGSM ══");
    bool ok = modem.gprsConnect(APN, GPRS_USER, GPRS_PASS);
    Serial.printf("gprsConnect: %s\n", ok ? "✅ OK" : "❌ ÉCHEC");
    if (ok) Serial.printf("IP: %s\n", modem.getLocalIP().c_str());
  }

  Serial.println("\n══ Mode passerelle série (tapez AT ci-dessous) ══");
}

void loop() {
  if (Serial.available())   SerialAT.write(Serial.read());
  if (SerialAT.available()) Serial.write(SerialAT.read());
}
