# Gragarda Voice — all-in-one voice filter for OBS (Windows)

🇫🇷 [Version française](#version-française)

[![Support me on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/gragarda)

Gragarda Voice is free and open source. If it helps you, you can support its development on [Ko-fi](https://ko-fi.com/gragarda).

One OBS filter that runs a full pro voice-studio chain, set up in 20 seconds by a calibration wizard.
The interface follows OBS's language: 34 languages included (English by default for the others).

Translations: English, French, German, Spanish, Italian, Portuguese (Brazil and Portugal), Dutch, Polish,
Russian, Ukrainian, Turkish, Japanese, Korean, Chinese (Simplified and Traditional), Swedish, Danish, Norwegian,
Finnish, Czech, Slovak, Hungarian, Romanian, Greek, Bulgarian, Croatian, Catalan, Indonesian, Vietnamese, Thai,
Arabic, Hebrew, Hindi. Fixes welcome: edit `data/locale/<lang>.ini` (check with `python tests/check_locales.py`).

## Made with AI
To be transparent: **most of this plugin's code was written by an AI** (Claude, by Anthropic), under my direction.
I chose the processing chain, the settings and the features, and tested every version by ear in OBS
on several microphones. The translations were also produced with AI. The noise-removal engines it uses
(DeepFilterNet, RNNoise) are third-party open-source projects not written by AI.
Bugs or doubts? Please open an [issue](../../issues).

## Features
- **AI noise removal**: DeepFilterNet 3 (Max quality, 41.5 ms) or RNNoise (Fast, 21.5 ms). Fixed, displayed latency.
- **Full chain**: high-pass, corrective EQ, compressor, de-esser, tone EQ, leveler (LUFS target), −1 dBFS look-ahead limiter.
- **4 macros** (Cleanup, Warmth, Clarity, Punch) + **destination** (Stream −14, Podcast −16, Voice-over −18 LUFS).
- **Calibration wizard**: 5 s of silence + 15 s of reading → gain, automatic mic correction,
  4 indicators (noise, level, sibilance, clipping).
- **Presets**: one per mic, switchable live from Streamer.bot / Stream Deck (obs-websocket).
- **Before/After** at matched loudness with no sync jump, **auto-sync** of cameras and other mics,
  **duplicate filter detection** (OBS filters and common VSTs).

## Install
1. Download `gragarda-voice-<version>-setup.exe` from [Releases](../../releases).
2. Close OBS and run the installer (it finds your OBS folder automatically).
3. In OBS: right-click your mic → Filters → + → **Gragarda Voice** → **Run calibration**.
4. Set your camera's **Render Delay** to the displayed latency, or click **Auto-sync**.

Requires OBS Studio 32 or newer, Windows 10/11 64-bit.
The installer is not code-signed: if SmartScreen warns you, click *More info* → *Run anyway*.

## Presets from Streamer.bot / Stream Deck
Save one preset per mic (*Save as preset*), then switch with an obs-websocket `SetSourceFilterSettings` request
(Streamer.bot: *OBS Raw* action; Stream Deck: a button that triggers that Streamer.bot action):
```json
{"sourceName": "Mic/Aux", "filterName": "Gragarda Voice", "filterSettings": {"preset": "My mic"}}
```
A preset is reloaded only when its name changes: after tweaking, switch to another preset (or "(none)") and back.

## Development

Chain: trim, high-pass, noise removal, subtractive EQ, compressor, de-esser, additive EQ, leveler, 1.5 ms look-ahead limiter.
Fixed latency per mode: Max quality 1992 samples (41.5 ms), Fast 1032 (21.5 ms).
RNNoise is copied into `third_party/rnnoise` from obs-studio. DeepFilterNet (libDF) is statically linked;
its model is installed in `data/obs-plugins/gragarda-voice/models`.
Qt 6 comes from the obs-deps (`.deps/obs-deps-qt6-*`), add it to CMAKE_PREFIX_PATH.
The DSP core (`dsp/`) does not depend on OBS.

### Prerequisites
- Visual Studio 2022 ("Desktop development with C++" workload); its CMake is enough (run from an "x64 Native Tools Command Prompt")
- OBS built from source (libobs only), same version as the installed OBS (here 32.2.2),
  cloned next to the project in `../obs-studio`:
  ```
  git clone --depth 1 --branch 32.2.2 --recurse-submodules --shallow-submodules https://github.com/obsproject/obs-studio.git
  cd obs-studio
  cmake --preset windows-x64 -G "Visual Studio 17 2022" -DENABLE_BROWSER=OFF -DENABLE_WEBSOCKET=OFF -DENABLE_SCRIPTING=OFF
  cmake --build --preset windows-x64 --config RelWithDebInfo --target libobs
  ```
  (the preset targets VS 2026 by default, hence the `-G`)
- Rust (rustup, msvc toolchain) and DeepFilterNet cloned next to the project in `../DeepFilterNet`:
  ```
  git clone https://github.com/Rikorose/DeepFilterNet.git
  cd DeepFilterNet
  git checkout d375b2d   # version used for v0.1.0
  set RUSTFLAGS=--remap-path-prefix=%USERPROFILE%=~
  cargo build --release -p deep_filter --no-default-features --features capi --lib
  ```

### Build
OBS_DIR defaults to `C:/Program Files/obs-studio` (pass `-DOBS_DIR=...` if OBS is elsewhere).
```
set OBS=%CD%/../obs-studio
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_PREFIX_PATH="%OBS%/build_x64;%OBS%/build_x64/deps/w32-pthreads;%OBS%/.deps/obs-deps-2026-07-15-x64;%OBS%/.deps/obs-deps-qt6-2026-07-15-x64" ^
  -DCMAKE_MODULE_PATH="%OBS%/cmake/finders"
cmake --build build --config RelWithDebInfo
ctest --test-dir build -C RelWithDebInfo
cmake --install build --config RelWithDebInfo     (admin terminal, OBS closed)
```

### Installer
Inno Setup 6 (`winget install JRSoftware.InnoSetup`), after the build:
```
ISCC installer\gragarda-voice.iss
```
→ `build/installer/gragarda-voice-<version>-setup.exe`. It finds OBS through the registry and refuses to run while OBS is open.
Rust crate licenses (regenerate if DeepFilterNet changes): `cargo install cargo-about --features cli`, then in
`../DeepFilterNet/libDF`: `cargo about generate --no-default-features --features capi <template.hbs> -o licenses/rust-crates.txt`.

### DSP tests (without OBS)
```
cmake -S . -B build-dsp -DGV_DSP_ONLY=ON
cmake --build build-dsp --config Release
ctest --test-dir build-dsp -C Release
```

### Testing in OBS
OBS → right-click the mic → Filters → + → "Gragarda Voice".
Speak: the voice should sound denser, the "s" softer, never clipping even with gain at +12 dB.
The latency line should show 42 ms in Max quality, 22 ms in Fast. Remove the OBS "Noise Suppression" filter: it is a duplicate.
The OBS log (Help → Log Files) should contain "[gragarda-voice] charge".

### Structure
- dsp/  : DSP core without OBS (reusable as a VST3)
- src/  : OBS glue (filter registration, properties)
- data/ : translations

## License
GPL-2.0-or-later (same as OBS). Third-party components: RNNoise (BSD), DeepFilterNet (MIT/Apache-2.0) and its
Rust dependencies — see `licenses/`.

---

# Version française

🇬🇧 [English version](#gragarda-voice--all-in-one-voice-filter-for-obs-windows)

[![Me soutenir sur Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/gragarda)

Gragarda Voice est gratuit et open source. S'il vous est utile, vous pouvez soutenir son développement sur [Ko-fi](https://ko-fi.com/gragarda).

Un seul filtre OBS qui applique toute la chaîne d'un studio voix pro, réglé en 20 secondes par un assistant de calibration.
L'interface suit la langue d'OBS : 34 langues incluses (anglais par défaut pour les autres).

Traductions : anglais, français, allemand, espagnol, italien, portugais (Brésil et Portugal), néerlandais, polonais,
russe, ukrainien, turc, japonais, coréen, chinois (simplifié et traditionnel), suédois, danois, norvégien,
finnois, tchèque, slovaque, hongrois, roumain, grec, bulgare, croate, catalan, indonésien, vietnamien, thaï,
arabe, hébreu, hindi. Corrections bienvenues : modifier `data/locale/<langue>.ini` (vérifier avec `python tests/check_locales.py`).

## Fait avec l'IA
Par transparence : **l'essentiel du code de ce plugin a été écrit par une IA** (Claude, d'Anthropic), sous ma direction.
J'ai choisi la chaîne de traitement, les réglages et les fonctionnalités, et j'ai testé chaque version à l'écoute
dans OBS sur plusieurs micros. Les traductions ont aussi été produites avec l'IA. Les moteurs de débruitage utilisés
(DeepFilterNet, RNNoise) sont des projets open source tiers, non écrits par une IA.
Un bug ou un doute ? Ouvrez une [issue](../../issues).

## Fonctionnalités
- **Débruitage IA** : DeepFilterNet 3 (Qualité max, 41,5 ms) ou RNNoise (Rapide, 21,5 ms). Latence fixe et affichée.
- **Chaîne complète** : passe-haut, EQ correctif, compresseur, de-esser, EQ de couleur, leveler (cible LUFS), limiteur à anticipation −1 dBFS.
- **4 macros** (Nettoyage, Chaleur, Clarté, Punch) + **destination** (Stream −14, Podcast −16, Voix-off −18 LUFS).
- **Assistant de calibration** : 5 s de silence + 15 s de lecture → gain, correction automatique du micro,
  4 voyants (bruit, niveau, sifflantes, saturation).
- **Presets** : un par micro, changeables en direct depuis Streamer.bot / Stream Deck (obs-websocket).
- **Avant/Après** à volume compensé sans saut de synchro, **synchronisation automatique** des caméras et autres micros,
  **détection des filtres en double** (filtres OBS et VST courants).

## Installation
1. Télécharger `gragarda-voice-<version>-setup.exe` dans les [Releases](../../releases).
2. Fermer OBS et lancer l'installeur (il trouve le dossier d'OBS tout seul).
3. Dans OBS : clic droit sur le micro → Filtres → + → **Gragarda Voice** → **Lancer la calibration**.
4. Régler le **Retard de rendu** de la caméra sur la latence affichée, ou cliquer sur **Synchroniser automatiquement**.

Nécessite OBS Studio 32 ou plus récent, Windows 10/11 64 bits.
L'installeur n'est pas signé : si SmartScreen avertit, cliquer sur *Informations complémentaires* → *Exécuter quand même*.

## Presets depuis Streamer.bot / Stream Deck
Enregistrer un preset par micro (*Enregistrer comme preset*), puis changer avec une requête obs-websocket `SetSourceFilterSettings`
(Streamer.bot : action *OBS Raw* ; Stream Deck : un bouton qui déclenche cette action Streamer.bot) :
```json
{"sourceName": "Mic/Aux", "filterName": "Gragarda Voice", "filterSettings": {"preset": "Mon micro"}}
```
Un preset n'est rechargé que si son nom change : après des retouches, passer sur un autre preset (ou « (aucun) ») puis revenir.

## Développement

Chaîne : trim, passe-haut, débruitage, EQ soustractif, compresseur, de-esser, EQ additif, leveler, limiteur à anticipation 1,5 ms.
Latence fixe par mode : Qualité max 1992 échantillons (41,5 ms), Rapide 1032 (21,5 ms).
RNNoise est copié dans `third_party/rnnoise` depuis obs-studio. DeepFilterNet (libDF) est lié statiquement ;
son modèle est installé dans `data/obs-plugins/gragarda-voice/models`.
Qt 6 vient des obs-deps (`.deps/obs-deps-qt6-*`), à ajouter à CMAKE_PREFIX_PATH.
Le cœur DSP (`dsp/`) ne dépend pas d'OBS.

### Prérequis
- Visual Studio 2022 (charge « Développement desktop C++ ») ; son CMake suffit (lancer depuis un « x64 Native Tools Command Prompt »)
- Sources d'OBS compilées (libobs seulement), même version que l'OBS installé (ici 32.2.2),
  clonées à côté du projet dans `../obs-studio` :
  ```
  git clone --depth 1 --branch 32.2.2 --recurse-submodules --shallow-submodules https://github.com/obsproject/obs-studio.git
  cd obs-studio
  cmake --preset windows-x64 -G "Visual Studio 17 2022" -DENABLE_BROWSER=OFF -DENABLE_WEBSOCKET=OFF -DENABLE_SCRIPTING=OFF
  cmake --build --preset windows-x64 --config RelWithDebInfo --target libobs
  ```
  (le preset vise VS 2026 par défaut, d'où le `-G`)
- Rust (rustup, toolchain msvc) et DeepFilterNet cloné à côté du projet dans `../DeepFilterNet` :
  ```
  git clone https://github.com/Rikorose/DeepFilterNet.git
  cd DeepFilterNet
  git checkout d375b2d   # version utilisée pour v0.1.0
  set RUSTFLAGS=--remap-path-prefix=%USERPROFILE%=~
  cargo build --release -p deep_filter --no-default-features --features capi --lib
  ```

### Build
OBS_DIR vaut `C:/Program Files/obs-studio` par défaut (passer `-DOBS_DIR=...` si OBS est ailleurs).
```
set OBS=%CD%/../obs-studio
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_PREFIX_PATH="%OBS%/build_x64;%OBS%/build_x64/deps/w32-pthreads;%OBS%/.deps/obs-deps-2026-07-15-x64;%OBS%/.deps/obs-deps-qt6-2026-07-15-x64" ^
  -DCMAKE_MODULE_PATH="%OBS%/cmake/finders"
cmake --build build --config RelWithDebInfo
ctest --test-dir build -C RelWithDebInfo
cmake --install build --config RelWithDebInfo     (terminal admin, OBS fermé)
```

### Installeur
Inno Setup 6 (`winget install JRSoftware.InnoSetup`), après le build :
```
ISCC installer\gragarda-voice.iss
```
→ `build/installer/gragarda-voice-<version>-setup.exe`. Il trouve OBS via le registre et refuse de s'exécuter si OBS est ouvert.
Licences des crates Rust (à régénérer si DeepFilterNet change) : `cargo install cargo-about --features cli`, puis dans
`../DeepFilterNet/libDF` : `cargo about generate --no-default-features --features capi <template.hbs> -o licenses/rust-crates.txt`.

### Tests DSP (sans OBS)
```
cmake -S . -B build-dsp -DGV_DSP_ONLY=ON
cmake --build build-dsp --config Release
ctest --test-dir build-dsp -C Release
```

### Test dans OBS
OBS → clic droit sur le micro → Filtres → + → « Gragarda Voice ».
Parler : la voix doit être plus dense, les « s » adoucis, jamais de saturation même avec le gain à +12 dB.
La ligne de latence doit afficher 42 ms en Qualité max, 22 ms en Rapide. Retirer le filtre OBS « Suppression du bruit » : il fait doublon.
Le log OBS (Aide → Fichiers journaux) doit contenir « [gragarda-voice] charge ».

### Structure
- dsp/  : cœur DSP sans OBS (réutilisable en VST3)
- src/  : colle OBS (enregistrement du filtre, propriétés)
- data/ : traductions

## Licence
GPL-2.0-or-later (comme OBS). Composants tiers : RNNoise (BSD), DeepFilterNet (MIT/Apache-2.0) et ses
dépendances Rust — voir `licenses/`.
