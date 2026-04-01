/*
 * main_test_sms.ino
 * ─────────────────────────────────────────────────────────────
 * Test SMS minimal : envoie un SMS à EMERGENCY_PHONE au démarrage
 * Tapez 's' dans le moniteur série pour renvoyer.
 *
 * Matériel : LILYGO T-A7670E R2  (LTE Cat-1)
 * Config    : secret.h  → remplir EMERGENCY_PHONE
 * ─────────────────────────────────────────────────────────────
 */

#define TINY_GSM_MODEM_SIM7600
#include "secret.h"
#include <TinyGsmClient.h>
#include <FastLED.h>

#define MODEM_RX     27
#define MODEM_TX     26
#define MODEM_PWRKEY  4
#define MODEM_DTR    25
#define LED_PIN      0
#define NUM_LEDS     1

CRGB leds[NUM_LEDS];
HardwareSerial SerialAT(1);
TinyGsm        modem(SerialAT);

void setLED(uint8_t r, uint8_t g, uint8_t b) { leds[0]=CRGB(r,g,b); FastLED.show(); }

// ── AT brut ───────────────────────────────────────────────────
String sendAT(const String& cmd, uint16_t waitMs=1500) {
  while (SerialAT.available()) SerialAT.read();
  SerialAT.println(cmd);
  unsigned long t = millis();
  String r = "";
  while (millis()-t < waitMs) {
    while (SerialAT.available()) r += (char)SerialAT.read();
    if (r.indexOf("OK") >= 0 || r.indexOf("ERROR") >= 0 ||
        r.indexOf(">") >= 0) break;
    delay(10);
  }
  r.trim();
  Serial.printf("  [%s] → [%s]\n", cmd.c_str(), r.length()?r.c_str():"(vide)");
  return r;
}

// ── Envoi SMS ─────────────────────────────────────────────────
bool sendSMS(const char* phone, const char* message) {
  Serial.printf("📱 SMS → %s\n   \"%s\"\n", phone, message);

  sendAT("AT+CMGF=1");
  sendAT("AT+CSCA=\"" SMS_CENTER "\"");

  // Attendre prompt ">"
  String r = sendAT("AT+CMGS=\"" + String(phone) + "\"", 5000);
  if (r.indexOf(">") < 0) {
    unsigned long t = millis();
    while (millis()-t < 3000) {
      while (SerialAT.available()) r += (char)SerialAT.read();
      if (r.indexOf(">") >= 0) break;
      delay(100);
    }
    if (r.indexOf(">") < 0) {
      Serial.println("❌ Pas de prompt '>'");
      return false;
    }
  }

  // Message + Ctrl+Z
  while (SerialAT.available()) SerialAT.read();
  SerialAT.print(message);
  SerialAT.write(0x1A);

  // Attendre +CMGS
  unsigned long t = millis();
  String resp = "";
  while (millis()-t < 15000) {
    while (SerialAT.available()) resp += (char)SerialAT.read();
    if (resp.indexOf("+CMGS:") >= 0 || resp.indexOf("ERROR") >= 0) break;
    delay(100);
  }
  resp.trim();
  Serial.printf("  réponse: [%s]\n", resp.c_str());

  bool ok = resp.indexOf("+CMGS:") >= 0;
  Serial.println(ok ? "✅ SMS envoyé !" : "❌ SMS échoué");
  return ok;
}

// ── setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== mod_mobilH — TEST SMS ===");
  Serial.printf("Destinataire : %s\n\n", EMERGENCY_PHONE);

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(50);
  setLED(0, 0, 50);

  pinMode(MODEM_PWRKEY, OUTPUT);
  pinMode(MODEM_DTR,    OUTPUT);
  digitalWrite(MODEM_PWRKEY, LOW);
  digitalWrite(MODEM_DTR,    LOW);

  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(1000);

  // Power-on si nécessaire
  Serial.print("Vérif modem... ");
  SerialAT.println("AT"); delay(1500);
  String r = ""; while (SerialAT.available()) r += (char)SerialAT.read();
  if (r.indexOf("OK") < 0) {
    Serial.println("power-on PWRKEY 800 ms");
    digitalWrite(MODEM_PWRKEY, HIGH); delay(800);
    digitalWrite(MODEM_PWRKEY, LOW);
    Serial.println("Attente 15 s..."); delay(15000);
  } else {
    Serial.println("déjà actif ✅");
  }

  Serial.println("Restart modem...");
  modem.restart();
  Serial.println("✅");

  // Connexion LTE
  Serial.print("Réseau LTE...");
  setLED(0, 0, 100);
  while (!modem.waitForNetwork(60000)) Serial.print(" retry...");
  Serial.println(" ✅");

  Serial.printf("GPRS APN=%s ...", APN);
  while (!modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
    Serial.print(" retry..."); delay(5000);
  }
  Serial.printf(" ✅  IP: %s\n\n", modem.getLocalIP().c_str());

  // SMS de test
  String msg = "Test SMS mod_mobilH OK - Signal: ";
  msg += modem.getSignalQuality();
  msg += "/31 - IP: ";
  msg += modem.getLocalIP();

  if (sendSMS(EMERGENCY_PHONE, msg.c_str())) {
    setLED(0, 50, 0);
  } else {
    setLED(50, 0, 0);
  }

  Serial.println("\nTapez 's' pour renvoyer un SMS.");
}

// ── loop ──────────────────────────────────────────────────────
void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 's' || c == 'S') {
      String msg = "Test SMS mod_mobilH - Signal: ";
      msg += modem.getSignalQuality();
      msg += "/31";
      sendSMS(EMERGENCY_PHONE, msg.c_str());
    }
  }
  if (SerialAT.available()) Serial.write(SerialAT.read());
  delay(100);
}
