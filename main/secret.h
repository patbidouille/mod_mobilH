#ifndef SECRET_H
#define SECRET_H

// ════════════════════════════════════════════════════════════
//  secret.h — Configuration utilisateur centralisée
//  ⚠️  Ne pas versionner ce fichier (ajouter à .gitignore)
// ════════════════════════════════════════════════════════════

// ── Carte SIM Things Mobile ───────────────────────────────────
#define APN             "TM"
#define GPRS_USER       ""
#define GPRS_PASS       ""
#define SMS_CENTER      "+447797704000"   // Centre SMS Things Mobile

// ── MQTT Broker ───────────────────────────────────────────────
#define MQTT_BROKER     "votre-domaine.ddns.net"  // ← IP ou DDNS de Home Assistant
#define MQTT_PORT       1883
#define MQTT_USER       "mqtt_user"               // ← login MQTT HA
#define MQTT_PASSWORD   "mqtt_password"           // ← mot de passe MQTT HA

// ── Numéro d'urgence SMS ──────────────────────────────────────
#define EMERGENCY_PHONE "+33612345678"            // ← votre numéro personnel

// ── Codes RF 433 MHz (hérités, non utilisés dans ce projet) ──
#define Con           5201      // Allume vitrine
#define Coff          5204      // Éteint vitrine
#define pir_hall      5592405   // PIR hall
#define pir_couloir   5264467   // PIR couloir
#define Acouloir      5558768   // Allumage couloir
#define ctc_porte     5510992   // Contact porte d'entrée
#define bp_vit        70960     // Bp vitrine

#endif // SECRET_H
