/*
 * main.ino — Système de surveillance LTE-M complet
 * ─────────────────────────────────────────────────────────────
 * Matériel : LILYGO T-A7670E R2
 * SIM     : Things Mobile (APN=TM, itinérance EE UK)
 *
 * Plan de câblage :
 *   I2C SDA           → GPIO 21
 *   I2C SCL           → GPIO 22
 *   PZEM-004T RX      → GPIO 34  (INPUT-ONLY)
 *   PZEM-004T TX      → GPIO 18
 *   Détection 220V    → GPIO 36  ⚠ INPUT-ONLY, pas de pull-up
 *   Détecteur incendie → GPIO 39  ⚠ INPUT-ONLY, pas de pull-up
 *   Bouton alarme     → GPIO 19
 *   PIR HC-SR501      → GPIO 32  (GPIO RTC → réveil deep-sleep)
 *   LED WS2812        → GPIO  0  ⚠ pin boot
 *   Coupe 5V VMOS     → GPIO 23  (NPN+P-MOS, HIGH=5V actif)
 *
 * Circuit VMOS coupe-5V (GPIO 23) :
 *   GPIO23 ──[1kΩ]── Base NPN (BC547/2N2222)
 *                    Collector ──[10kΩ]── 5V_IN
 *                               └──────── Gate P-MOS (AO3401/IRF9540)
 *                    Emitter ── GND
 *   5V_IN  ── Source P-MOS
 *              Drain ──── 5V_composants (LED WS2812, PZEM-004T, ...)
 *   GPIO23=HIGH → NPN passant → Gate=GND → P-MOS ON  → 5V actif
 *   GPIO23=LOW  → NPN bloqué  → Gate=5V  → P-MOS OFF → 5V coupé
 *
 * ⚙️  Configuration utilisateur → secret.h
 * ─────────────────────────────────────────────────────────────
 */

#define TINY_GSM_MODEM_SIM7600
#include "secret.h"         // ← toute la config utilisateur ici
#include "ltem_manager.h"
#include "mqtt_manager.h"
#include "sensor_manager.h"
#include "json_builder.h"
#include "storage_manager.h"
#include <FastLED.h>

// ════════════════════════════════════════════════════════════
// PINS
// ════════════════════════════════════════════════════════════
#define PIN_PIR           32   // GPIO RTC → réveil deep-sleep EXT0
#define PIN_FIRE_ALARM    39   // INPUT-ONLY, actif bas
#define PIN_ALARM_SWITCH  19
#define PIN_AC_DETECT     36   // INPUT-ONLY
#define PIN_PZEM_RX       34
#define PIN_PZEM_TX       18
#define PIN_I2C_SDA       21
#define PIN_I2C_SCL       22
#define PIN_PWR_5V        23   // Coupe-5V VMOS : HIGH=5V actif, LOW=5V coupé

// ── LED WS2812 ────────────────────────────────────────────────
#define LED_PIN   0            // ⚠️ pin boot — initié dans setup()
#define NUM_LEDS  1
CRGB leds[NUM_LEDS];

// ════════════════════════════════════════════════════════════
// TIMINGS DEEP-SLEEP
// ════════════════════════════════════════════════════════════
#define SLEEP_TIME_BATTERY  3600000000ULL  // 1 heure
#define SLEEP_TIME_AC        600000000ULL  // 10 minutes

// ════════════════════════════════════════════════════════════
// INSTANCES
// ════════════════════════════════════════════════════════════
LTEMManager    ltem;
MQTTManager    mqtt(MQTT_BROKER, MQTT_PORT, MQTT_USER, MQTT_PASSWORD);
SensorManager  sensors;
JSONBuilder    jsonBuilder;
StorageManager storage;

// ════════════════════════════════════════════════════════════
// ÉTAT
// ════════════════════════════════════════════════════════════
unsigned long lastSendTime        = 0;
unsigned long sendInterval        = 600000UL;  // 10 min par défaut
unsigned long connectionStartTime = 0;
const unsigned long CONN_TIMEOUT  = 60000UL;   // 60 s max par phase

bool lastPirState   = false;
bool lastFireState  = false;
bool lastAlarmMode  = false;
bool acPowerPresent = true;

bool isConnectingLTEM = false;
bool isConnectingMQTT = false;

// ── États LED ─────────────────────────────────────────────────
enum LEDState {
  LED_OFF,
  LED_CONNECTING_LTEM,   // 🔵 bleu clignotant
  LED_CONNECTING_MQTT,   // 🔵🟢 cyan clignotant
  LED_OK_ALARM_OFF,      // 🟢 vert fixe
  LED_OK_ALARM_ON,       // 🟠 orange fixe
  LED_ERROR,             // 🔴 rouge clignotant lent
  LED_ALERT,             // 🔴 rouge clignotant rapide
  LED_AC_LOSS            // 🔴🔵 rouge-bleu alternés
};
LEDState currentLED = LED_OFF;

// ── Variables blink ───────────────────────────────────────────
static bool          blinkState    = false;
static unsigned long lastBlinkTime = 0;

// ════════════════════════════════════════════════════════════
// PROTOTYPES
// ════════════════════════════════════════════════════════════
void   setupPins();
void   setupWakeupSources();
void   setLED(uint8_t r, uint8_t g, uint8_t b);
void   updateLED();
void   attemptConnection();
void   checkACPower();
void   checkImmediateEvents();
void   sendAllData();
void   loadStoredConfig();
void   enterDeepSleep();

// ════════════════════════════════════════════════════════════
// SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  mod_mobilH — Surveillance LTE-M      ║");
  Serial.println("║  T-A7670E + Home Assistant             ║");
  Serial.println("╚════════════════════════════════════════╝\n");

  // Raison du réveil
  esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
  switch (wakeup) {
    case ESP_SLEEP_WAKEUP_EXT0:   Serial.println("⏰ Réveil — PIR détection (GPIO 32)"); break;
    case ESP_SLEEP_WAKEUP_EXT1:   Serial.println("⏰ Réveil — alarme incendie (GPIO 39)"); break;
    case ESP_SLEEP_WAKEUP_TIMER:  Serial.println("⏰ Réveil — timer"); break;
    default:                      Serial.println("🔌 Démarrage initial"); break;
  }

  // LED (init APRÈS la séquence boot)
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(60);
  setLED(0, 0, 0);

  setupPins();

  // Alimenter les composants 5V immédiatement (LED, PZEM...)
  digitalWrite(PIN_PWR_5V, HIGH);
  Serial.println("✅ Alimentation 5V composants : ON");

  // État 220V immédiat
  acPowerPresent = (digitalRead(PIN_AC_DETECT) == HIGH);
  Serial.printf("⚡ Alimentation : %s\n", acPowerPresent ? "SECTEUR" : "BATTERIE");

  storage.begin();
  loadStoredConfig();
  sensors.begin(PIN_I2C_SDA, PIN_I2C_SCL, PIN_PZEM_RX, PIN_PZEM_TX);
  setupWakeupSources();

  // Init modem (non-bloquant : lance juste la séquence power-on)
  if (!ltem.begin()) {
    Serial.println("❌ Échec init modem");
    currentLED = LED_ERROR;
  } else {
    isConnectingLTEM    = true;
    connectionStartTime = millis();
    currentLED          = LED_CONNECTING_LTEM;
  }

  Serial.println("\n✅ Setup terminé\n");
}

// ════════════════════════════════════════════════════════════
// LOOP
// ════════════════════════════════════════════════════════════
void loop() {
  unsigned long now = millis();

  // LED (éteinte sur batterie pour économie)
  if (acPowerPresent) updateLED();
  else                setLED(0, 0, 0);

  checkACPower();
  checkImmediateEvents();

  if (isConnectingLTEM || isConnectingMQTT) {
    attemptConnection();
  }

  if (mqtt.isConnected()) {
    mqtt.loop();
  }

  // Envoi périodique
  if (mqtt.isConnected() && (now - lastSendTime >= sendInterval)) {
    sendAllData();
    lastSendTime = now;

    if (!acPowerPresent) {
      Serial.println("💤 Deep sleep (batterie)");
      delay(500);
      enterDeepSleep();
    }
  }

  // Reconnexion si déconnecté
  if (!isConnectingLTEM && !isConnectingMQTT && !mqtt.isConnected()) {
    if (now - lastSendTime > 60000UL) {
      Serial.println("🔄 Reconnexion...");
      isConnectingLTEM    = true;
      connectionStartTime = now;
      currentLED          = LED_CONNECTING_LTEM;
    }
  }

  delay(100);
}

// ════════════════════════════════════════════════════════════
// CONFIGURATION PINS
// ════════════════════════════════════════════════════════════
void setupPins() {
  // Coupe-5V : OUTPUT, LOW par défaut (sécurité boot) → mis HIGH après setup LED
  pinMode(PIN_PWR_5V,       OUTPUT);
  digitalWrite(PIN_PWR_5V, LOW);   // sera mis HIGH juste après dans setup()

  // GPIO 32 et 19 ont des pull internes disponibles
  pinMode(PIN_PIR,          INPUT);
  pinMode(PIN_ALARM_SWITCH, INPUT_PULLUP);

  // GPIO 36 et 39 : INPUT-ONLY, AUCUN pull-up interne
  pinMode(PIN_AC_DETECT,    INPUT);
  pinMode(PIN_FIRE_ALARM,   INPUT);

  Serial.println("✅ Pins configurés");
}

// ════════════════════════════════════════════════════════════
// SOURCES DE RÉVEIL DEEP-SLEEP
// ════════════════════════════════════════════════════════════
void setupWakeupSources() {
  // Timer
  uint64_t sleepTime = acPowerPresent ? SLEEP_TIME_AC : SLEEP_TIME_BATTERY;
  esp_sleep_enable_timer_wakeup(sleepTime);

  // PIR sur GPIO 32 (RTC) → réveil sur front montant (HIGH)
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_32, 1);

  // Incendie sur GPIO 39 (RTC) → réveil sur niveau bas
  esp_sleep_enable_ext1_wakeup(1ULL << 39, ESP_EXT1_WAKEUP_ANY_LOW);

  Serial.println("✅ Sources de réveil configurées");
}

// ════════════════════════════════════════════════════════════
// LED WS2812
// ════════════════════════════════════════════════════════════
void setLED(uint8_t r, uint8_t g, uint8_t b) {
  leds[0] = CRGB(r, g, b);
  FastLED.show();
}

void updateLED() {
  unsigned long now = millis();

  switch (currentLED) {
    case LED_OFF:
      setLED(0, 0, 0);
      break;

    case LED_CONNECTING_LTEM:  // 🔵 bleu clignotant 500 ms
      if (now - lastBlinkTime >= 500) { blinkState = !blinkState; lastBlinkTime = now; }
      setLED(0, 0, blinkState ? 100 : 0);
      break;

    case LED_CONNECTING_MQTT:  // 🔵🟢 cyan clignotant 500 ms
      if (now - lastBlinkTime >= 500) { blinkState = !blinkState; lastBlinkTime = now; }
      setLED(0, blinkState ? 100 : 0, blinkState ? 100 : 0);
      break;

    case LED_OK_ALARM_OFF:     // 🟢 vert fixe
      setLED(0, 70, 0);
      break;

    case LED_OK_ALARM_ON:      // 🟠 orange fixe
      setLED(120, 50, 0);
      break;

    case LED_ERROR:            // 🔴 rouge clignotant lent
      if (now - lastBlinkTime >= 800) { blinkState = !blinkState; lastBlinkTime = now; }
      setLED(blinkState ? 100 : 0, 0, 0);
      break;

    case LED_ALERT:            // 🔴 rouge clignotant rapide
      if (now - lastBlinkTime >= 150) { blinkState = !blinkState; lastBlinkTime = now; }
      setLED(blinkState ? 255 : 0, 0, 0);
      break;

    case LED_AC_LOSS: {        // 🔴🔵 rouge puis bleu, alternés 600 ms
      static uint8_t phase = 0;
      if (now - lastBlinkTime >= 600) { phase = (phase + 1) % 3; lastBlinkTime = now; }
      if      (phase == 0) setLED(150, 0, 0);   // rouge
      else if (phase == 1) setLED(0, 0, 0);      // éteint
      else                 setLED(0, 0, 150);    // bleu
      break;
    }
  }
}

// ════════════════════════════════════════════════════════════
// CONNEXION NON-BLOQUANTE (avec timeout global)
// ════════════════════════════════════════════════════════════
void attemptConnection() {
  unsigned long elapsed = millis() - connectionStartTime;

  if (elapsed > CONN_TIMEOUT) {
    Serial.println("⏱ Timeout connexion");
    isConnectingLTEM = false;
    isConnectingMQTT = false;
    currentLED       = LED_ERROR;
    return;
  }

  if (isConnectingLTEM) {
    if (ltem.isConnected()) {
      Serial.println("✅ LTE-M connecté → tentative MQTT");
      isConnectingLTEM    = false;
      isConnectingMQTT    = true;
      connectionStartTime = millis();
      currentLED          = LED_CONNECTING_MQTT;
      mqtt.begin(ltem.getClient());
    } else {
      static unsigned long lastAttempt = 0;
      if (millis() - lastAttempt > 6000) {
        Serial.println("🔄 Tentative LTE-M...");
        ltem.connect();
        lastAttempt = millis();
      }
    }
  }

  if (isConnectingMQTT) {
    if (mqtt.connect()) {
      isConnectingMQTT = false;
      mqtt.sendDiscovery();
      bool alarm = (digitalRead(PIN_ALARM_SWITCH) == LOW);
      currentLED = alarm ? LED_OK_ALARM_ON : LED_OK_ALARM_OFF;
      sendAllData();
      lastSendTime = millis();
    }
  }
}

// ════════════════════════════════════════════════════════════
// VÉRIFICATION 220V
// ════════════════════════════════════════════════════════════
void checkACPower() {
  bool acNow = (digitalRead(PIN_AC_DETECT) == HIGH);
  if (acNow == acPowerPresent) return;

  acPowerPresent = acNow;

  if (acNow) {
    Serial.println("🔌 220V rétabli");
    sendInterval = 600000UL;
    esp_sleep_enable_timer_wakeup(SLEEP_TIME_AC);

    // Rétablir l'alimentation 5V des composants
    digitalWrite(PIN_PWR_5V, HIGH);
    delay(200); // laisser le temps au PZEM et à la LED de démarrer
    Serial.println("✅ Alimentation 5V composants : ON (secteur)");

    if (mqtt.isConnected()) {
      bool alarm = (digitalRead(PIN_ALARM_SWITCH) == LOW);
      currentLED = alarm ? LED_OK_ALARM_ON : LED_OK_ALARM_OFF;
    }
  } else {
    Serial.println("🔋 Perte 220V — passage batterie");
    sendInterval = 3600000UL;
    esp_sleep_enable_timer_wakeup(SLEEP_TIME_BATTERY);
    currentLED = LED_AC_LOSS;

    // Couper l'alimentation 5V des composants non essentiels (LED, PZEM)
    setLED(0, 0, 0);          // éteindre LED avant de couper le 5V
    delay(10);
    digitalWrite(PIN_PWR_5V, LOW);
    Serial.println("🔌 Alimentation 5V composants : OFF (batterie)");

    // SMS perte 220V
    if (ltem.isConnected()) {
      ltem.sendSMS(EMERGENCY_PHONE, "ALERTE: Perte alimentation 220V !");
    }
  }

  if (mqtt.isConnected()) sendAllData();
  storage.saveACPowerState(acNow);
}

// ════════════════════════════════════════════════════════════
// VÉRIFICATION PIR / INCENDIE / ALARME
// ════════════════════════════════════════════════════════════
void checkImmediateEvents() {
  bool alarmMode = (digitalRead(PIN_ALARM_SWITCH) == LOW);

  // ── Mode alarme changé ──────────────────────────────────
  if (alarmMode != lastAlarmMode) {
    lastAlarmMode = alarmMode;
    Serial.printf("🚨 Mode alarme : %s\n", alarmMode ? "ACTIVÉ" : "DÉSACTIVÉ");
    if (acPowerPresent && mqtt.isConnected())
      currentLED = alarmMode ? LED_OK_ALARM_ON : LED_OK_ALARM_OFF;
    if (mqtt.isConnected()) mqtt.publishAlarmMode(alarmMode);
  }

  // ── PIR ─────────────────────────────────────────────────
  bool pirNow = (digitalRead(PIN_PIR) == HIGH);
  if (pirNow != lastPirState) {
    lastPirState = pirNow;
    if (pirNow) {
      Serial.println("👤 MOUVEMENT DÉTECTÉ !");
      if (acPowerPresent) currentLED = LED_ALERT;
      if (mqtt.isConnected()) { mqtt.publishPIR(true); sendAllData(); }
      if (alarmMode && ltem.isConnected())
        ltem.sendSMS(EMERGENCY_PHONE, "ALERTE INTRUSION: Mouvement détecté !");
      storage.savePIREvent();
      // Restauration LED après 5 s (attention : delay bloquant court)
      delay(5000);
      if (acPowerPresent && mqtt.isConnected())
        currentLED = alarmMode ? LED_OK_ALARM_ON : LED_OK_ALARM_OFF;
    } else {
      if (mqtt.isConnected()) mqtt.publishPIR(false);
    }
  }

  // ── Incendie ────────────────────────────────────────────
  // Détecteur NF : contact fermé (bas) = alarme
  // GPIO 39 INPUT-ONLY, pas de pull-up interne → prévoir résistance externe
  bool fireNow = (digitalRead(PIN_FIRE_ALARM) == LOW);
  if (fireNow != lastFireState) {
    lastFireState = fireNow;
    if (fireNow) {
      Serial.println("🔥 ALERTE INCENDIE !");
      if (acPowerPresent) currentLED = LED_ALERT;
      if (mqtt.isConnected()) { mqtt.publishFire(true); sendAllData(); }
      // SMS incendie TOUJOURS (pas besoin de mode alarme)
      if (ltem.isConnected())
        ltem.sendSMS(EMERGENCY_PHONE, "ALERTE INCENDIE ! Intervention urgente.");
      storage.saveFireEvent();
      delay(10000);
      if (acPowerPresent && mqtt.isConnected())
        currentLED = alarmMode ? LED_OK_ALARM_ON : LED_OK_ALARM_OFF;
    } else {
      if (mqtt.isConnected()) mqtt.publishFire(false);
    }
  }
}

// ════════════════════════════════════════════════════════════
// ENVOI DE TOUTES LES DONNÉES
// ════════════════════════════════════════════════════════════
void sendAllData() {
  if (!mqtt.isConnected()) return;

  Serial.println("\n📊 Lecture et envoi...");
  SensorData data = sensors.readAll();
  data.pirDetected  = (digitalRead(PIN_PIR)          == HIGH);
  data.fireDetected = (digitalRead(PIN_FIRE_ALARM)    == LOW);
  data.alarmMode    = (digitalRead(PIN_ALARM_SWITCH)  == LOW);
  data.acPower      = acPowerPresent;

  Serial.printf("  Temp  : %.1f °C   Hum : %.1f%%   Pres : %.0f hPa\n",
                data.temperature, data.humidity, data.pressure);
  Serial.printf("  Lux   : %.0f      V   : %.1f V   P    : %.1f W\n",
                data.light, data.voltage, data.power);
  Serial.printf("  PIR=%s  Feu=%s  Alrm=%s  220V=%s\n",
                data.pirDetected  ? "ON" : "OFF",
                data.fireDetected ? "ON" : "OFF",
                data.alarmMode    ? "ON" : "OFF",
                data.acPower      ? "ON" : "OFF");

  bool ok = mqtt.publishSensorData(data);
  if (ok) {
    Serial.println("✅ Données publiées\n");
    storage.saveLastSendTime(millis());
  } else {
    Serial.println("❌ Échec publication\n");
    storage.saveFailedData(jsonBuilder.buildFullPayload(data));
  }
}

// ════════════════════════════════════════════════════════════
// CHARGEMENT CONFIG STOCKÉE
// ════════════════════════════════════════════════════════════
void loadStoredConfig() {
  lastSendTime   = storage.loadLastSendTime();
  acPowerPresent = storage.loadACPowerState();
  sendInterval   = acPowerPresent ? 600000UL : 3600000UL;
  Serial.println("✅ Config chargée");
}

// ════════════════════════════════════════════════════════════
// DEEP SLEEP
// ════════════════════════════════════════════════════════════
void enterDeepSleep() {
  Serial.println("💤 Entrée en deep sleep...");
  storage.saveTotalUptime(millis());
  if (mqtt.isConnected()) {
    mqtt.publish(TOPIC_AVAILABILITY, "offline", true);
    delay(200);
  }

  // Couper l'alimentation 5V des composants (LED, PZEM, ...)
  setLED(0, 0, 0);
  delay(10);
  digitalWrite(PIN_PWR_5V, LOW);
  Serial.println("🔌 Alimentation 5V composants : OFF (deep-sleep)");

  Serial.flush();
  esp_deep_sleep_start();
}
