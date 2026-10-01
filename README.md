# OTA_R4_WiFi – Mise à jour sans fil pour Arduino UNO R4 WiFi

Modèle permettant d'ajouter le **téléversement par WiFi (OTA)** depuis l'IDE Arduino à n'importe quel programme pour **Arduino UNO R4 WiFi**.

Le code est découpé en **4 blocs** à recopier dans votre propre programme. Il gère aussi la reconnexion WiFi automatique et propose un « mode OTA dédié » activable par bouton.

---

## Fonctionnalités

- Téléversement par WiFi depuis l'IDE Arduino (la carte apparaît dans **Outils > Port**).
- Mises à jour **protégées par mot de passe**.
- **Reconnexion automatique** si le WiFi est perdu (nouvel essai toutes les 30 s).
- Redémarrage du service si l'**adresse IP change** (renouvellement DHCP).
- Rappels pour **mettre le montage en sécurité** avant la mise à jour (moteurs, sorties…).
- **Mode OTA dédié** bloquant, optionnel (ex. déclenché par un bouton au démarrage).
- LED témoin optionnelle et messages de débogage désactivables.
- Vérification de la version du firmware du module WiFi.
- Identifiants séparés du code dans `arduino_secrets.h`.

---

## Prérequis

### Matériel
- Une carte **Arduino UNO R4 WiFi**.
- Un réseau WiFi **2,4 GHz** (le 5 GHz n'est pas supporté).
- Un ordinateur sur **le même réseau local** que la carte.

### Logiciels
1. **Arduino IDE** avec le paquet de cartes **Arduino UNO R4 Boards** (`renesas_uno`).
2. Bibliothèque **ArduinoOTA** de **Juraj Andrassy** (Gestionnaire de bibliothèques).
3. ⚠️ **Étape indispensable** : copier le fichier **`platform.local.txt`** situé dans le dossier `extras/renesas` de la bibliothèque ArduinoOTA vers le dossier du paquet de cartes `renesas_uno`.

   Emplacement habituel sous Windows :
   ```
   C:\Users\<utilisateur>\AppData\Local\Arduino15\packages\arduino\hardware\renesas_uno\<version>\
   ```
   Redémarrez ensuite l'IDE. Sans ce fichier, l'IDE ne sait pas envoyer le programme par le réseau.
   > À refaire après chaque mise à jour du paquet de cartes `renesas_uno` (nouveau dossier `<version>`).

4. Conseillé : mettre à jour le firmware du module WiFi via **Outils > Firmware Updater**.

---

## Structure du projet

```
OTA_R4_WiFi/
├── OTA_R4_WiFi.ino      # Modèle + programme d'exemple
├── arduino_secrets.h    # Identifiants (à créer, ne pas partager)
└── README.md
```

---

## Configuration

### `arduino_secrets.h`

Créez cet onglet dans le dossier du croquis :

```cpp
#define SECRET_SSID  "NomDeVotreWiFi"   // Réseau 2,4 GHz
#define SECRET_PASS  "MotDePasseWiFi"
#define OTA_SSID     "UNO_R4_Projet"    // Nom affiché dans l'IDE (sans espace)
#define OTA_PASS     "MotDePasseOTA"    // Ne pas laisser « password »
```

| Constante     | Description                                             |
|---------------|---------------------------------------------------------|
| `SECRET_SSID` | Nom du réseau WiFi                                      |
| `SECRET_PASS` | Mot de passe du WiFi                                    |
| `OTA_SSID`    | Nom de la carte dans **Outils > Port** (sans espace)    |
| `OTA_PASS`    | Mot de passe demandé par l'IDE à chaque envoi OTA       |

> Si le projet est publié (GitHub…), ajoutez `arduino_secrets.h` au `.gitignore`.

### Réglages dans le BLOC 1

| Réglage            | Défaut   | Rôle                                                       |
|--------------------|----------|------------------------------------------------------------|
| `OTA_WIFI_TIMEOUT` | 20000 ms | Durée maximale d'une tentative de connexion WiFi           |
| `OTA_RECO_PERIODE` | 30000 ms | Intervalle entre deux tentatives de reconnexion            |
| `OTA_DEBUG`        | 1        | 1 = messages sur le moniteur série, 0 = silencieux         |
| `OTA_LED_PIN`      | -1       | Broche de la LED témoin OTA (-1 = désactivée, ex. `LED_BUILTIN`) |

---

## Intégrer l'OTA dans votre programme

| Bloc | Contenu                       | Emplacement                  |
|------|-------------------------------|------------------------------|
| 1    | Includes + réglages           | Tout en haut du programme    |
| 2    | Fonctions OTA                 | Avant `setup()`              |
| 3    | `activerOTA();`               | Dans `setup()`               |
| 4    | `gererOTA();`                 | Au début de `loop()`         |

```cpp
// BLOC 1 et BLOC 2 recopiés ici

void setup() {
  Serial.begin(115200);
  activerOTA();          // BLOC 3
  // ... votre setup()
}

void loop() {
  gererOTA();            // BLOC 4
  // ... votre loop(), sans delay() long
}
```

> ⚠️ `#include <WiFiS3.h>` doit **toujours** être placé **avant** `#include <ArduinoOTA.h>`.

> 🚨 **Chaque programme envoyé par OTA doit contenir ces 4 blocs**, sinon la carte ne pourra plus être mise à jour par WiFi et il faudra revenir au câble USB.

---

## Utilisation

### 1. Premier téléversement – par câble USB (obligatoire)
1. Branchez la carte en USB, sélectionnez **Arduino UNO R4 WiFi** et le port série.
2. Téléversez le programme.
3. Ouvrez le moniteur série à **115200 bauds** ; vous devez lire :
   ```
   [OTA] Connecte, IP = 192.168.x.x
   [OTA] Pret ! Port reseau : UNO_R4_Projet at 192.168.x.x
   ```

### 2. Téléversements suivants – par WiFi
1. Dans **Outils > Port**, choisissez le **port réseau** portant le nom `OTA_SSID`.
2. Téléversez normalement et saisissez le **mot de passe OTA** si l'IDE le demande.
3. La carte écrit le nouveau programme puis redémarre toute seule.

---

## Fonctions disponibles

| Fonction                     | Rôle                                                                                     |
|------------------------------|------------------------------------------------------------------------------------------|
| `activerOTA()`               | Connecte le WiFi si besoin et démarre le service OTA. Peut être appelée plusieurs fois. |
| `gererOTA()`                 | À appeler à chaque tour de `loop()` : écoute l'IDE, gère la perte de WiFi et le changement d'IP. |
| `modeOTA(dureeMs)`           | Mode OTA dédié **bloquant** : le programme est suspendu en attente d'un envoi. `0` = attente infinie. |
| `connecterWiFi(timeoutMs)`   | Connexion au WiFi, attend l'attribution de l'adresse IP. Renvoie `true` si succès.       |
| `otaAuDebut()`               | Appelée au début d'une réception : **mettez ici vos sorties en sécurité** (moteurs…).    |
| `otaAvantApplication()`      | Appelée juste avant l'écriture du programme et le redémarrage.                           |

### Variante : OTA uniquement à la demande (bouton)

Pour ne pas laisser l'OTA actif en permanence, remplacez `activerOTA();` dans `setup()` par :

```cpp
pinMode(BOUTON_OTA, INPUT_PULLUP);                       // bouton entre D2 et GND
if (digitalRead(BOUTON_OTA) == LOW) modeOTA(120000UL);   // 2 min d'attente
```

Maintenez le bouton appuyé au démarrage : la carte attend un téléversement pendant 2 minutes (LED clignotante si `OTA_LED_PIN` est défini), puis reprend le programme normal.

> Dans cette variante, `gererOTA()` dans `loop()` réactiverait l'OTA au bout de 30 s : retirez-le si vous voulez un OTA **uniquement** sur bouton.

---

## Règles importantes

- **Pas de `delay()` long** dans `loop()` : pendant un `delay()`, l'OTA n'est pas écouté. Utilisez `millis()` (voir l'exemple du clignotement de LED dans le programme).
- **Taille limitée** : avec le stockage interne, le programme ne doit pas dépasser la **moitié de la flash**, soit environ **128 Ko** (moins de ~48 % affichés par l'IDE à la compilation).
- `WiFi.begin()` est **bloquant** sur l'UNO R4 (plusieurs secondes) : une reconnexion peut figer brièvement le programme (au plus toutes les 30 s).

---

## Messages du moniteur série

| Message                                         | Signification                                    |
|-------------------------------------------------|--------------------------------------------------|
| `[OTA] Connexion a ...`                         | Tentative de connexion au WiFi                   |
| `[OTA] Connecte, IP = ...`                      | WiFi connecté                                    |
| `[OTA] Pret ! Port reseau : ... at ...`         | Service OTA actif, la carte est visible dans l'IDE |
| `[OTA] Echec de connexion WiFi`                 | Pas de connexion, nouvel essai dans 30 s         |
| `[OTA] Module WiFi absent !`                    | Le module ESP32-S3 de la carte ne répond pas     |
| `[OTA] Firmware WiFi ... : mise a jour conseillee` | Firmware du module WiFi ancien                |
| `[OTA] WiFi perdu`                              | Connexion perdue, service arrêté                 |
| `[OTA] Reception d'un nouveau programme...`     | Téléversement OTA en cours                       |
| `[OTA] Ecriture du programme puis redemarrage`  | Programme reçu, la carte redémarre               |
| `[OTA] Mode OTA dedie : en attente...`          | Mode dédié actif (`modeOTA`)                     |

---

## Dépannage

- **Le port réseau n'apparaît pas dans l'IDE** : vérifiez que le PC est sur le même réseau, redémarrez l'IDE, autorisez-le dans le pare-feu (port TCP **65280** et mDNS). Les réseaux « invités » isolent souvent les appareils. Une fois la carte visible au moins une fois, l'IDE peut mettre quelques secondes à l'afficher.
- **Le téléversement OTA échoue immédiatement** : `platform.local.txt` n'a pas été copié dans le dossier `renesas_uno` (ou le paquet de cartes a été mis à jour depuis).
- **Erreur de taille / échec d'écriture** : programme trop gros (> ~128 Ko).
- **Mot de passe refusé** : vérifiez `OTA_PASS` dans `arduino_secrets.h`.
- **L'OTA marche une fois puis plus jamais** : le nouveau programme ne contient pas les 4 blocs, ou utilise des `delay()` trop longs.
- **Pas de connexion WiFi** : identifiants erronés ou réseau 5 GHz uniquement.

---

## Paramètres techniques

| Paramètre              | Valeur                            |
|------------------------|-----------------------------------|
| Carte                  | Arduino UNO R4 WiFi (module ESP32-S3) |
| Bibliothèque           | ArduinoOTA (J. Andrassy)          |
| Stockage               | `InternalStorage`                 |
| Port OTA               | TCP 65280 + annonce mDNS          |
| Vitesse série          | 115200 bauds                      |
| Taille max programme   | ~128 Ko                           |
