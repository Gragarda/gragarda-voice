# Gragarda Voice — contexte projet

Filtre audio OBS natif (Windows uniquement) qui applique toute la chaine de traitement voix
d'un studio pro en un seul filtre, utilisable par n'importe qui (assistant de calibration + 4 macros).

## Decisions prises
- Cible : plugin OBS natif (libobs, C++17, CMake, MSVC). Pas de VST2 (licence), pas de VST3 pour l'instant.
- Le coeur DSP (dsp/) ne doit JAMAIS dependre d'OBS : il sera reutilise pour un VST3 plus tard.
- Latence : acceptee (l'utilisateur compense avec un Retard de rendu sur la camera), mais elle doit etre
  FIXE et exposee via VoiceChain::latencySamples(), affichee dans le panneau en ms.
- Deux modes prevus : "Qualite max" (DeepFilterNet, ~50-80 ms) et "Rapide" (RNNoise, ~15 ms).
- Audio OBS : float planaire, 48 kHz, blocs de taille variable -> aucun etage ne doit supposer une taille de bloc.
- Pas d'allocation ni de lock dans filter_audio (thread audio temps reel).

## Chaine cible (ordre)
1. Trim d'entree  2. Passe-haut 70-100 Hz  3. Debruitage (RNNoise / DeepFilterNet)
4. De-click (optionnel)  5. EQ soustractif (boue 150-250 Hz, carton 200-400 Hz)
6. Compresseur (2:1-4:1, att 10-30 ms, rel 50-150 ms)  7. De-esser (5-9 kHz)
8. EQ additif (chaleur 100-150 Hz, presence 3-5 kHz, air 10-12 kHz)
9. Leveler / AGC lent (cible short-term, ex. -16 LUFS)  10. Limiteur a anticipation, plafond -1 dBTP

## UX cible
- Panneau OBS : 4 macros (Nettoyage, Chaleur, Clarte, Punch), destination (Stream/Podcast/Voix-off),
  bouton "Lancer la calibration", ligne de latence.
- Fenetre Qt : assistant (5 s de silence -> bruit de fond ; 15 s de lecture -> niveau, spectre,
  dynamique, sifflantes), 4 voyants (Bruit, Niveau, Sifflantes, Saturation), Avant/Apres a volume compense,
  bouton "Synchroniser automatiquement" (ajoute Retard de rendu sur les sources video + decalage sur les autres audios,
  apres confirmation), detection des filtres OBS doublons (compresseur, suppression de bruit).

## Feuille de route
1. [x] Squelette : filtre charge, gain lisse, bypass, affichage latence (valide dans OBS 32.2.2)
2. [x] Chaine DSP de base avec reglages en dur : HPF, EQ, compresseur, de-esser, limiteur (valide a l'ecoute, reglages actuels = reference)
3. [x] Integration RNNoise (21,5 ms) + DeepFilterNet 3 via libDF statique (41,5 ms, defaut) — valide a l'ecoute sur tous les micros
4. [x] Macros (0-100, 50 = reference) + destination (leveler : Stream -14 / Podcast -16 / Voix-off -18 LUFS) + panneau — valide
5. [x] (correction micro cachee : EQ grave/presence/aigus pose par la calibration) Analyseur (dsp/Calibration) + assistant Qt (src/calibration-dialog.cpp) + 4 voyants — profil LTASS de reference a recaler avec les rapports reels (log OBS "[gragarda-voice] calibration")
6. [x] Avant/apres a volume compense (case Bypass), sync auto (bouton, confirmation Qt), detection des doublons (avertissement + bouton), installeur Inno Setup (installer/) — a valider a l'ecoute/usage
7. [x] Presets : liste "Preset" + Enregistrer/Supprimer dans le filtre (cle "presets" = tableau {name, values}).
   Charge quand le nom change (UI ou obs-websocket SetSourceFilterSettings {"preset": "Nom"}) — valide

## Publication
- Depot public : https://github.com/Gragarda/gragarda-voice (GPL-2.0-or-later). v0.1.0 publiee (installeur en Release).
- Nouvelle version : bump AppVersion (installer/*.iss) + version dans plugin-main.cpp/CMakeLists, build, ISCC,
  `gh release create vX.Y.Z build/installer/...setup.exe`. Regenerer licenses/rust-crates.txt si DeepFilterNet change.

## Build
Voir README.md (libobs compile depuis les sources d'OBS, meme version que l'OBS installe).
