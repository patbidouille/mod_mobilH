/*
 * main_test_mqtt.ino
 * ─────────────────────────────────────────────────────────────
 * Test MQTT : envoie "main_test_mqtt" toutes les minutes
 * sur le topic mod_mobilH/test
 *
 * Matériel : LILYGO T-A7670E R2  (LTE Cat-1)
 * MQTT     : stack natif A7670E (AT+CMQTT*)
 * Config   : secret.h
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

#define MQTT_TOPIC     "mod_mobilH/test"
#define MQTT_CLIENT_ID "mod_mobilH_test"
#define SEND_INTERVAL  60000UL

CRGB leds[NUM_LEDS];
HardwareSerial SerialAT(1);
TinyGsm        modem(SerialAT);

bool          ltemOK  = false;
bool          mqttOK  = false;
unsigned long lastSend = 0;

void setLED(uint8_t r, uint8_t g, uint8_t b) { leds[0]=CRGB(r,g,b); FastLED.show(); }
static bool blinkState=false; static unsigned long lastBlink=0;
void blinkLED(uint8_t r,uint8_t g,uint8_t b,uint16_t ms=500){
  if(millis()-lastBlink>=ms){blinkState=!blinkState;lastBlink=millis();
  setLED(blinkState?r:0,blinkState?g:0,blinkState?b:0);}
}

// ── AT brut ───────────────────────────────────────────────────
String sendAT(const String& cmd, uint16_t waitMs=2000) {
  while (SerialAT.available()) SerialAT.read();
  SerialAT.println(cmd);
  unsigned long t = millis();
  String r = "";
  while (millis()-t < waitMs) {
    while (SerialAT.available()) r += (char)SerialAT.read();
    if (r.indexOf("OK\r") >= 0 || r.indexOf("ERROR") >= 0 ||
        r.indexOf(">") >= 0) break;
    delay(10);
  }
  r.trim();
  Serial.printf("  [%s] → [%s]\n", cmd.c_str(), r.length()?r.c_str():"(vide)");
  return r;
}

String sendRaw(const String& data, uint16_t waitMs=3000) {
  while (SerialAT.available()) SerialAT.read();
  SerialAT.print(data);
  unsigned long t = millis();
  String r = "";
  while (millis()-t < waitMs) {
    while (SerialAT.available()) r += (char)SerialAT.read();
    if (r.indexOf("OK") >= 0 || r.indexOf("ERROR") >= 0) break;
    delay(10);
  }
  r.trim();
  return r;
}

// ── Connexion LTE ─────────────────────────────────────────────
bool connectLTE() {
  Serial.print("Réseau...");
  if (!modem.waitForNetwork(60000)) { Serial.println(" ❌"); return false; }
  Serial.println(" ✅");
  Serial.printf("GPRS APN=%s ...", APN);
  if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) { Serial.println(" ❌"); return false; }
  Serial.printf(" ✅  IP: %s\n", modem.getLocalIP().c_str());
  return true;
}

// ── Connexion MQTT (stack natif A7670E) ───────────────────────
bool connectMQTT() {
  Serial.println("─── Connexion MQTT (AT+CMQTT) ───");

  // Nettoyage propre
  sendAT("AT+CMQTTDISC=0,120", 3000);
  sendAT("AT+CMQTTREL=0",      1000);
  sendAT("AT+CMQTTSTOP",       2000);
  delay(500);

  // Démarrer le service
  String r = sendAT("AT+CMQTTSTART", 5000);
  if (r.indexOf("OK") < 0 && r.indexOf("+CMQTTSTART: 0") < 0) {
    Serial.println("❌ CMQTTSTART"); return false;
  }
  delay(300);

  // Acquérir un client
  r = sendAT("AT+CMQTTACCQ=0,\"" MQTT_CLIENT_ID "\",0", 3000);
  if (r.indexOf("OK") < 0) { Serial.println("❌ CMQTTACCQ"); return false; }

  // Connexion au broker
  String cmd = "AT+CMQTTCONNECT=0,\"tcp://";
  cmd += MQTT_BROKER; cmd += ":"; cmd += MQTT_PORT;
  cmd += "\",60,1,\""; cmd += MQTT_USER;
  cmd += "\",\""; cmd += MQTT_PASSWORD; cmd += "\"";

  sendAT(cmd, 3000);

  // Attendre URC +CMQTTCONNECT: 0,0
  unsigned long t = millis();
  String urc = "";
  while (millis()-t < 10000) {
    while (SerialAT.available()) urc += (char)SerialAT.read();
    if (urc.indexOf("+CMQTTCONNECT:") >= 0) break;
    delay(100);
  }
  urc.trim();
  if (urc.length()) Serial.printf("  URC: [%s]\n", urc.c_str());

  if (urc.indexOf("+CMQTTCONNECT: 0,0") >= 0) {
    Serial.println("✅ MQTT connecté !");
    return true;
  }
  Serial.println("❌ MQTT connexion refusée");
  return false;
}

// ── Publication MQTT ──────────────────────────────────────────
bool mqttPublish(const char* topic, const char* payload) {
  String r;
  r = sendAT("AT+CMQTTTOPIC=0," + String(strlen(topic)), 2000);
  if (r.indexOf(">") < 0) return false;
  sendRaw(topic);

  r = sendAT("AT+CMQTTPAYLOAD=0," + String(strlen(payload)), 2000);
  if (r.indexOf(">") < 0) return false;
  sendRaw(payload);

  r = sendAT("AT+CMQTTPUB=0,1,60", 5000);
  bool ok = r.indexOf("OK") >= 0;
  Serial.printf("%s pub %s : %s\n", ok?"✅":"❌", topic, payload);
  return ok;
}

// ── setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== mod_mobilH — TEST MQTT ===");

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(50);
  setLED(0, 0, 50);

  pinMode(MODEM_PWRKEY, OUTPUT);
  pinMode(MODEM_DTR,    OUTPUT);
  digitalWrite(MODEM_PWRKEY, LOW);
  digitalWrite(MODEM_DTR,    LOW);

  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(1000);

  Serial.print("Vérif modem... ");
  SerialAT.println("AT"); delay(1500);
  String r=""; while(SerialAT.available()) r+=(char)SerialAT.read();
  if (r.indexOf("OK") < 0) {
    Serial.println("power-on PWRKEY 800 ms");
    digitalWrite(MODEM_PWRKEY, HIGH); delay(800);
    digitalWrite(MODEM_PWRKEY, LOW);
    Serial.println("Attente 15 s..."); delay(15000);
  } else { Serial.println("déjà actif ✅"); }

  modem.restart();
  Serial.println("Setup OK\n");
}

// ── loop ──────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  if (!ltemOK) {
    blinkLED(0, 0, 100, 400);
    if (connectLTE()) { ltemOK=true; setLED(0,50,50); }
    else { delay(15000); return; }
  }

  if (!mqttOK) {
    blinkLED(0, 100, 100, 400);
    if (connectMQTT()) { mqttOK=true; setLED(0,50,0); lastSend=0; }
    else { setLED(50,0,0); delay(15000); return; }
  }

  if (now - lastSend >= SEND_INTERVAL) {
    Serial.printf("[%lus] pub → %s\n", now/1000, MQTT_TOPIC);
    if (!mqttPublish(MQTT_TOPIC, "main_test_mqtt")) {
      mqttOK=false; ltemOK=false;
    }
    lastSend = now;
  }

  delay(200);
}
