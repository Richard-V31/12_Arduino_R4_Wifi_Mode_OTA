/* ==========================================================================
 *  OTA_R4_WiFi.ino  —  Mise à jour sans fil (OTA) pour Arduino UNO R4 WiFi
 * ==========================================================================
 *
 *  Ce fichier est un MODÈLE : il contient 4 blocs à recopier dans
 *  n'importe quel programme pour lui ajouter le téléversement par WiFi
 *  depuis l'IDE Arduino.
 *
 *     BLOC 1 : includes + réglages        -> tout en haut du programme
 *     BLOC 2 : fonctions OTA              -> avant setup()
 *     BLOC 3 : 1 ligne dans setup()       -> activerOTA();
 *     BLOC 4 : 1 ligne dans loop()        -> gererOTA();
 *
 *  Option : modeOTA(duree) = « mode OTA dédié » bloquant (ex. sur bouton).
 *
 *  Prérequis (voir le manuel PDF) :
 *   - Bibliothèque « ArduinoOTA » de Juraj Andrassy (Gestionnaire de bibliothèques)
 *   - ⚠️ 🚨 Le Fichier platform.local.txt (extras/renesas de la bibliothèque) copié
 *     dans le dossier du paquet de cartes « renesas_uno »
 *   - Premier téléversement par câble USB, les suivants par WiFi
 *
 *  !!! IMPORTANT !!!
 *  Chaque programme envoyé par OTA DOIT contenir ces blocs, sinon la carte
 *  ne pourra plus être mise à jour par WiFi (retour obligatoire à l'USB).
 *
 *  Limite : avec le stockage interne, la taille du programme est limitée
 *  à la moitié de la flash, soit environ 128 Ko (< ~48 % affiché par l'IDE).
 * ========================================================================== */


/* ##########################################################################
 * ##  BLOC 1 — INCLUDES ET RÉGLAGES  (à placer tout en haut du programme) ##
 * ########################################################################## */

#include <WiFiS3.h>       // Pilote WiFi de l'UNO R4 WiFi — TOUJOURS avant ArduinoOTA.h
#include <ArduinoOTA.h>   // Bibliothèque ArduinoOTA (J. Andrassy)

// ------------------ PARAMÈTRES À ADAPTER --------------------
// Les valeurs réelles sont dans l'onglet "arduino_secrets.h"
#include "arduino_secrets.h"
const char WIFI_SSID[]   = SECRET_SSID;   // Nom du réseau WiFi (2,4 GHz uniquement)
const char WIFI_PASSWORD[] = SECRET_PASS;   // Mot de passe du réseau WiFi
const char OTA_NOM[]    = OTA_SSID;       // Nom de la carte affiché dans l'IDE (sans espace)
const char OTA_MDP[]    = OTA_PASS;         // Mot de passe demandé par l'IDE à chaque envoi OTA
                                              // -> à changer ! (« password » = valeur par défaut)

#define OTA_WIFI_TIMEOUT 20000UL      // Durée max d'une tentative de connexion WiFi (ms)
#define OTA_RECO_PERIODE 30000UL      // Intervalle entre deux tentatives de reconnexion (ms)
#define OTA_DEBUG        1            // 1 = messages sur le moniteur série, 0 = silencieux
#define OTA_LED_PIN      -1
// --- Variables internes (ne pas modifier) ----------------------------------
bool          otaActif       = false;  // true quand le service OTA écoute
IPAddress     otaIP;                   // Adresse IP utilisée par le service OTA
unsigned long otaDernierEssai = 0;     // Horodatage de la dernière tentative WiFi

#if OTA_DEBUG
  #define OTA_LOG(x)   Serial.print(x)
  #define OTA_LOGLN(x) Serial.println(x)
#else
  #define OTA_LOG(x)
  #define OTA_LOGLN(x)
#endif


/* ##########################################################################
 * ##  BLOC 2 — FONCTIONS OTA  (à placer avant setup())                    ##
 * ########################################################################## */

// Allume / éteint la LED témoin si elle est activée
void otaLed(bool etat) {
#if OTA_LED_PIN >= 0
  digitalWrite(OTA_LED_PIN, etat ? HIGH : LOW);
#endif
}

// --------------------------------------------------------------------------
// connecterWiFi() : se connecte au réseau, renvoie true si succès.
// Attention : WiFi.begin() est bloquant sur l'UNO R4 (plusieurs secondes).
// --------------------------------------------------------------------------
bool connecterWiFi(unsigned long timeoutMs) {
  if (WiFi.status() == WL_NO_MODULE) {             // Le module ESP32-S3 ne répond pas
    OTA_LOGLN(F("[OTA] Module WiFi absent !"));
    return false;
  }

  OTA_LOG(F("[OTA] Connexion a ")); OTA_LOGLN(WIFI_SSID);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);              // Tentative de connexion
    if (WiFi.status() != WL_CONNECTED) delay(1000);
  }
  if (WiFi.status() != WL_CONNECTED) {
    OTA_LOGLN(F("[OTA] Echec de connexion WiFi"));
    return false;
  }

  // Sur l'UNO R4, l'adresse IP peut rester à 0.0.0.0 quelques instants
  // après la connexion : on attend qu'elle soit attribuée par le DHCP.
  t0 = millis();
  while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && millis() - t0 < 5000) delay(100);

  OTA_LOG(F("[OTA] Connecte, IP = ")); OTA_LOGLN(WiFi.localIP());
  return WiFi.localIP() != IPAddress(0, 0, 0, 0);
}

// --------------------------------------------------------------------------
// Fonctions de rappel appelées par la bibliothèque pendant une mise à jour
// --------------------------------------------------------------------------
void otaAuDebut() {                 // Un téléversement OTA commence
  OTA_LOGLN(F("[OTA] Reception d'un nouveau programme..."));
  otaLed(true);
  // >>> Ici : mettre les moteurs / sorties dangereuses en sécurité <<<
}

void otaAvantApplication() {        // Programme reçu et vérifié, la carte va redémarrer
  OTA_LOGLN(F("[OTA] Ecriture du programme puis redemarrage"));
#if OTA_DEBUG
  Serial.flush();                   // Vider le tampon série avant le redémarrage
#endif
}

// --------------------------------------------------------------------------
// activerOTA() : LE « VOID » PRINCIPAL — passe la carte en mode OTA.
// Connecte le WiFi si besoin, puis démarre le service qui écoute l'IDE
// (port TCP 65280 + annonce mDNS pour apparaître dans Outils > Port).
// Peut être appelé plusieurs fois sans risque.
// --------------------------------------------------------------------------
void activerOTA() {
  if (otaActif) return;                            // Déjà actif : rien à faire

#if OTA_LED_PIN >= 0
  pinMode(OTA_LED_PIN, OUTPUT);
#endif

  // Vérification (facultative) du firmware du module WiFi ESP32-S3
  String fw = WiFi.firmwareVersion();
  if (fw < WIFI_FIRMWARE_LATEST_VERSION) {
    OTA_LOG(F("[OTA] Firmware WiFi ")); OTA_LOG(fw);
    OTA_LOGLN(F(" : mise a jour conseillee (Outils > Firmware Updater)"));
  }

  if (WiFi.status() != WL_CONNECTED && !connecterWiFi(OTA_WIFI_TIMEOUT)) {
    otaDernierEssai = millis();                    // gererOTA() réessaiera plus tard
    return;
  }

  ArduinoOTA.onStart(otaAuDebut);                  // Rappel : début de réception
  ArduinoOTA.beforeApply(otaAvantApplication);     // Rappel : juste avant redémarrage

  otaIP = WiFi.localIP();
  ArduinoOTA.begin(otaIP, OTA_NOM, OTA_MDP, InternalStorage);  // Démarrage du service
  otaActif = true;

  OTA_LOG(F("[OTA] Pret ! Port reseau : ")); OTA_LOG(OTA_NOM);
  OTA_LOG(F(" at ")); OTA_LOGLN(otaIP);
}

// --------------------------------------------------------------------------
// gererOTA() : à appeler à CHAQUE tour de loop().
// - écoute les demandes de téléversement de l'IDE
// - relance le WiFi et le service si la connexion a été perdue
// Ne bloque pas (sauf pendant une reconnexion WiFi, toutes les 30 s max).
// --------------------------------------------------------------------------
void gererOTA() {
  // 1) WiFi perdu : on coupe le service et on retente périodiquement
  if (WiFi.status() != WL_CONNECTED) {
    if (otaActif) {
      OTA_LOGLN(F("[OTA] WiFi perdu"));
      ArduinoOTA.end();                            // Arrêt propre du service
      otaActif = false;
    }
    if (millis() - otaDernierEssai >= OTA_RECO_PERIODE) {
      otaDernierEssai = millis();
      activerOTA();                                // Reconnexion + redémarrage du service
    }
    return;
  }

  // 2) WiFi présent mais service pas encore démarré (ex. échec au setup)
  if (!otaActif) {
    if (millis() - otaDernierEssai >= OTA_RECO_PERIODE) {
      otaDernierEssai = millis();
      activerOTA();
    }
    return;
  }

  // 3) L'adresse IP a changé (bail DHCP renouvelé) : on relance le service
  if (WiFi.localIP() != otaIP) {
    ArduinoOTA.end();
    otaActif = false;
    activerOTA();
    return;
  }

  // 4) Cas normal : traiter une éventuelle demande de l'IDE
  ArduinoOTA.poll();
}

// --------------------------------------------------------------------------
// modeOTA(dureeMs) : « mode OTA dédié », BLOQUANT.
// Le programme principal est suspendu, la LED clignote, la carte attend
// uniquement un téléversement. dureeMs = 0 -> attente infinie.
// Utile si l'on ne veut PAS laisser l'OTA actif en permanence
// (ex. déclenché par un bouton au démarrage).
// --------------------------------------------------------------------------
void modeOTA(unsigned long dureeMs) {
  activerOTA();
  OTA_LOGLN(F("[OTA] Mode OTA dedie : en attente d'un televersement..."));

  unsigned long t0 = millis();
  while (dureeMs == 0 || millis() - t0 < dureeMs) {
    gererOTA();                                    // Si un envoi arrive, la carte redémarre ici
    otaLed((millis() / 250) % 2);                  // Clignotement rapide = mode OTA
    delay(5);
  }
  otaLed(false);
  OTA_LOGLN(F("[OTA] Fin du mode OTA dedie, reprise du programme"));
}


/* ##########################################################################
 * ##  EXEMPLE DE PROGRAMME QUELCONQUE UTILISANT LES BLOCS CI-DESSUS       ##
 * ########################################################################## */

#define BOUTON_OTA 2          // Exemple : bouton entre D2 et GND (facultatif)

void setup() {
  Serial.begin(115200);
  unsigned long t = millis();
  while (!Serial && millis() - t < 3000);   // Attend le moniteur série 3 s max

  // ---- BLOC 3 : UNE ligne dans setup() --------------------------------
  activerOTA();                // OTA toujours disponible pendant le fonctionnement
  // ---------------------------------------------------------------------

  // Variante « à la demande » (à la place de activerOTA()) :
  //   pinMode(BOUTON_OTA, INPUT_PULLUP);
  //   if (digitalRead(BOUTON_OTA) == LOW) modeOTA(120000UL);  // 2 min d'attente

  // ... votre setup() habituel ...

  pinMode(LED_BUILTIN, OUTPUT);

}

void loop() {
  // ---- BLOC 4 : UNE ligne au début de loop() --------------------------
  gererOTA();
  // ---------------------------------------------------------------------

  // ... votre loop() habituel ...
  // Évitez les delay() longs : pendant un delay(), l'OTA n'est pas écouté.
  // Exemple non bloquant : un message toutes les 5 s.
/*  static unsigned long dernier = 0;
  if (millis() - dernier >= 5000) {
    dernier = millis();
    Serial.println(F("Programme en cours... (version 1)"));
  }*/

// Clignotement sans delay() : l'OTA reste écouté en permanence
  static unsigned long dernier = 0;
  static bool etat = false;
  if (millis() - dernier >= 1000) {
    dernier = millis();
    etat = !etat;
    digitalWrite(LED_BUILTIN, etat ? HIGH : LOW);
    Serial.print("LED_BUILTIN = ");
    Serial.println(LED_BUILTIN);
}
}
