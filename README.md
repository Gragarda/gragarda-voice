# Gragarda Voice — all-in-one voice filter for OBS (Windows)

One OBS filter that runs a full pro voice-studio chain, set up in 20 seconds by a calibration wizard.
The interface follows OBS's language (English by default, French included).

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

## Français
Un seul filtre OBS avec toute la chaîne d'un studio voix pro (débruitage IA, EQ, compresseur, de-esser, leveler,
limiteur), réglé en 20 secondes par un assistant de calibration. L'interface est en français si OBS est en français.
Installation : télécharger l'installeur dans [Releases](../../releases), fermer OBS, lancer l'installeur, puis
clic droit sur le micro → Filtres → + → **Gragarda Voice** → **Lancer la calibration**.

## License
GPL-2.0-or-later (same as OBS). Third-party components: RNNoise (BSD), DeepFilterNet (MIT/Apache-2.0) and its
Rust dependencies — see `licenses/`.

---

# Development (notes in French)

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
