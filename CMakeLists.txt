cmake_minimum_required(VERSION 3.24)
project(gragarda-voice VERSION 0.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # libs statiques liees dans un module partage (Linux)

# ---- Coeur DSP (submodule github.com/Gragarda/gragarda-voice-core) : gv_dsp, ${DFN_MODEL}
# git submodule update --init ; tests du coeur : cmake -DGV_DSP_ONLY=ON (active GV_CORE_TESTS)
option(GV_DSP_ONLY "Ne compiler que le coeur DSP et ses tests (pas besoin de libobs)" OFF)
set(GV_CORE_TESTS ${GV_DSP_ONLY})
enable_testing()
add_subdirectory(core)

# libobs : pointe CMAKE_PREFIX_PATH vers ton build d'obs-studio (voir README)
if(GV_DSP_ONLY)
  return()
endif()
find_package(libobs REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# ---- Filtre OBS
add_library(gragarda-voice MODULE
  src/plugin-main.cpp
  src/voice-filter.cpp
  src/calibration-dialog.cpp
  src/obs-tools.cpp
)
target_link_libraries(gragarda-voice PRIVATE OBS::libobs gv_dsp Qt6::Widgets)
set_target_properties(gragarda-voice PROPERTIES PREFIX "")
if(MSVC)
  target_compile_options(gragarda-voice PRIVATE /W4 /fp:fast)
  # Nom du .pdb seul dans la DLL publiee, pas le chemin du PC de build
  target_link_options(gragarda-voice PRIVATE /PDBALTPATH:%_PDB%)
elseif(UNIX AND NOT APPLE)
  # Un objet statique (libDF Rust / asm) sans .note.GNU-stack rend la pile executable :
  # glibc >= 2.41 refuse alors le dlopen ("cannot enable executable stack")
  # --exclude-libs : symboles des libs statiques (rnnoise_*, df_*) non exportes. Sinon le
  # rnnoise_create d'obs-filters (autre version, deja charge) remplace le notre -> crash
  target_link_options(gragarda-voice PRIVATE -Wl,-z,noexecstack -Wl,--exclude-libs,ALL)
endif()

# ---- Installation / paquet, par OS
set(PLUGIN_DATA data/ licenses/)
if(WIN32)
  # cmake --install directement dans OBS (dossier de l'OBS installe)
  set(OBS_DIR "C:/Program Files/obs-studio" CACHE PATH "Dossier d'installation d'OBS")
  install(TARGETS gragarda-voice LIBRARY DESTINATION "${OBS_DIR}/obs-plugins/64bit")
  install(DIRECTORY data/ DESTINATION "${OBS_DIR}/data/obs-plugins/gragarda-voice")
  install(DIRECTORY licenses/ DESTINATION "${OBS_DIR}/data/obs-plugins/gragarda-voice/licenses")
  install(FILES "${DFN_MODEL}" DESTINATION "${OBS_DIR}/data/obs-plugins/gragarda-voice/models")
elseif(APPLE)
  # Bundle gragarda-voice.plugin : binaire dans Contents/MacOS, donnees dans Contents/Resources
  # (a copier dans ~/Library/Application Support/obs-studio/plugins/)
  set_target_properties(gragarda-voice PROPERTIES
    BUNDLE TRUE
    BUNDLE_EXTENSION plugin
    MACOSX_BUNDLE_GUI_IDENTIFIER fr.gragarda.voice
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
  install(TARGETS gragarda-voice LIBRARY DESTINATION . BUNDLE DESTINATION .)
  set(RES "gragarda-voice.plugin/Contents/Resources")
  install(DIRECTORY data/ DESTINATION "${RES}")
  install(DIRECTORY licenses/ DESTINATION "${RES}/licenses")
  install(FILES LICENSE DESTINATION "${RES}/licenses" RENAME gragarda-voice-GPL-2.0.txt)
  install(FILES "${DFN_MODEL}" DESTINATION "${RES}/models")
else()
  # Arborescence d'un plugin utilisateur : ~/.config/obs-studio/plugins/gragarda-voice/{bin/64bit,data}
  install(TARGETS gragarda-voice LIBRARY DESTINATION gragarda-voice/bin/64bit)
  install(DIRECTORY data/ DESTINATION gragarda-voice/data)
  install(DIRECTORY licenses/ DESTINATION gragarda-voice/data/licenses)
  install(FILES LICENSE DESTINATION gragarda-voice/data/licenses RENAME gragarda-voice-GPL-2.0.txt)
  install(FILES "${DFN_MODEL}" DESTINATION gragarda-voice/data/models)
endif()
