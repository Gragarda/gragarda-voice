# Gragarda Voice — filtre voix tout-en-un pour OBS (Windows)

Un seul filtre OBS qui applique toute la chaîne de traitement d'un studio voix pro, réglé en 20 secondes
par un assistant de calibration.

*One OBS filter with a full pro voice chain (denoise, EQ, compressor, de-esser, leveler, limiter),
set up in 20 seconds by a calibration wizard. Windows, OBS 32+.*

## Fonctionnalités
- **Débruitage IA** : DeepFilterNet 3 (Qualité max, 41,5 ms) ou RNNoise (Rapide, 21,5 ms). Latence fixe, affichée.
- **Chaîne complète** : passe-haut, EQ soustractif, compresseur, de-esser, EQ additif, leveler (cible LUFS), limiteur −1 dBFS.
- **4 macros** (Nettoyage, Chaleur, Clarté, Punch) + **destination** (Stream −14, Podcast −16, Voix-off −18 LUFS).
- **Calibration** : 5 s de silence + 15 s de lecture → gain, correction du micro, 4 voyants (bruit, niveau, sifflantes, saturation).
- **Presets** : un par micro, changeables en direct depuis Streamer.bot / Stream Deck (obs-websocket).
- **Avant/Après** à volume égalisé et sans saut de synchro, **synchronisation automatique** de la caméra,
  **détection des filtres en double** (filtres OBS et VST courants).

## Installation
1. Télécharger `gragarda-voice-<version>-setup.exe` dans [Releases](../../releases).
2. Fermer OBS, lancer l'installeur (il trouve le dossier d'OBS tout seul).
3. Dans OBS : clic droit sur le micro → Filtres → + → **Gragarda Voice** → **Lancer la calibration**.
4. Mettre le **Retard de rendu** de la caméra à la latence affichée (ou bouton **Synchroniser automatiquement**).

Nécessite OBS Studio 32 ou plus récent, Windows 10/11 64 bits.

## Licence
GPL-2.0-or-later (comme OBS). Composants tiers : RNNoise (BSD), DeepFilterNet (MIT/Apache-2.0) et ses
dépendances Rust — voir `licenses/`.

---

# Développement

Chaîne : trim, passe-haut, débruitage, EQ soustractif, compresseur, de-esser, EQ additif, leveler, limiteur à anticipation 1,5 ms.
Latence fixe par mode : Qualité max 1992 échantillons (41,5 ms), Rapide 1032 (21,5 ms).
RNNoise est copié dans `third_party/rnnoise` depuis obs-studio. DeepFilterNet (libDF) est lié statiquement ;
son modèle est installé dans `data/obs-plugins/gragarda-voice/models`.
Qt 6 vient des obs-deps (`.deps/obs-deps-qt6-*`), à ajouter à CMAKE_PREFIX_PATH.
Le cœur DSP (`dsp/`) ne dépend pas d'OBS.

## Prerequis
- Visual Studio 2022 (charge "Developpement desktop C++") ; son CMake suffit (lancer depuis un "x64 Native Tools Command Prompt")
- Sources d'OBS compilees (libobs seulement), meme version que l'OBS installe (ici 32.2.2),
  clonees a cote du projet dans `../obs-studio` :
  ```
  git clone --depth 1 --branch 32.2.2 --recurse-submodules --shallow-submodules https://github.com/obsproject/obs-studio.git
  cd obs-studio
  cmake --preset windows-x64 -G "Visual Studio 17 2022" -DENABLE_BROWSER=OFF -DENABLE_WEBSOCKET=OFF -DENABLE_SCRIPTING=OFF
  cmake --build --preset windows-x64 --config RelWithDebInfo --target libobs
  ```
  (le preset vise VS 2026 par defaut, d'ou le `-G`)
- Rust (rustup, toolchain msvc) et DeepFilterNet clone a cote du projet dans `../DeepFilterNet` :
  ```
  git clone https://github.com/Rikorose/DeepFilterNet.git
  cd DeepFilterNet
  git checkout d375b2d   # version utilisee pour v0.1.0
  cargo build --release -p deep_filter --no-default-features --features capi --lib
  ```

## Build
OBS installe ici : `C:/Program Files/obs-studio` (valeur par defaut de OBS_DIR).
```
set OBS=%CD%/../obs-studio
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_PREFIX_PATH="%OBS%/build_x64;%OBS%/build_x64/deps/w32-pthreads;%OBS%/.deps/obs-deps-2026-07-15-x64;%OBS%/.deps/obs-deps-qt6-2026-07-15-x64" ^
  -DCMAKE_MODULE_PATH="%OBS%/cmake/finders"
cmake --build build --config RelWithDebInfo
ctest --test-dir build -C RelWithDebInfo
cmake --install build --config RelWithDebInfo     (terminal admin, OBS ferme)
```

## Presets et Streamer.bot / Stream Deck
Un seul filtre, plusieurs presets (un par micro, par ex.). Pour changer de preset depuis Streamer.bot :
action "OBS Raw" -> requete `SetSourceFilterSettings` :
```json
{"sourceName": "01-Mic/Aux", "filterName": "Gragarda Voice - sport", "filterSettings": {"preset": "Vocaster"}}
```
Stream Deck : bouton qui declenche cette action Streamer.bot (plugin Streamer.bot pour Stream Deck).
Le preset n'est recharge que si son nom change ; pour revenir aux valeurs d'un preset apres l'avoir retouche,
passer par un autre preset ou "(aucun)".

## Installeur
Inno Setup 6 (`winget install JRSoftware.InnoSetup`), apres le build :
```
ISCC installer\gragarda-voice.iss
```
-> `build/installer/gragarda-voice-<version>-setup.exe`.
Licences des crates Rust (a regenerer si DeepFilterNet change) : `cargo install cargo-about --features cli`, puis dans
`../DeepFilterNet/libDF` : `cargo about generate --no-default-features --features capi <template.hbs> -o licenses/rust-crates.txt`. Trouve OBS via le registre, refuse si OBS est ouvert.

## Tests DSP (sans OBS)
```
cmake -S . -B build-dsp -DGV_DSP_ONLY=ON
cmake --build build-dsp --config Release
ctest --test-dir build-dsp -C Release
```

## Test dans OBS
OBS -> clic droit sur le micro -> Filtres -> + -> "Gragarda Voice".
Parle : la voix doit etre plus dense, les "s" adoucis, jamais de saturation meme gain a +12 dB.
La ligne de latence doit afficher 42 ms en Qualite max, 22 ms en Rapide. Retirer le filtre OBS "Suppression du bruit" : il fait doublon.
Le log OBS (Aide -> Fichiers journaux) doit contenir "[gragarda-voice] charge".

## Structure
- dsp/  : coeur DSP sans OBS (reutilisable en VST3)
- src/  : colle OBS (enregistrement du filtre, proprietes)
- data/ : traductions
