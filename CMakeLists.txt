cmake_minimum_required(VERSION 3.24)
project(gragarda-voice VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # libs statiques liees dans un module partage (Linux)

# libobs : pointe CMAKE_PREFIX_PATH vers ton build d'obs-studio (voir README)
# ---- Coeur DSP : aucune dependance a OBS (reutilisable pour un VST3 plus tard)
# RNNoise (BSD, copie de obs-studio/plugins/obs-filters/rnnoise, modele integre)
file(GLOB RNNOISE_SRC third_party/rnnoise/src/*.c)
add_library(rnnoise STATIC ${RNNOISE_SRC})
target_include_directories(rnnoise PUBLIC third_party/rnnoise/include)
target_compile_definitions(rnnoise PRIVATE _USE_MATH_DEFINES COMPILE_OPUS)

# DeepFilterNet 3 (libDF, Rust, MIT/Apache) : clone de github.com/Rikorose/DeepFilterNet a cote du projet,
# puis : cargo build --release -p deep_filter --no-default-features --features capi --lib
set(DFN_DIR "${CMAKE_SOURCE_DIR}/../DeepFilterNet" CACHE PATH "Sources DeepFilterNet (libDF compilee)")
set(DFN_MODEL "${DFN_DIR}/models/DeepFilterNet3_onnx.tar.gz")
if(WIN32)
  set(DFN_LIB_DEFAULT "${DFN_DIR}/target/release/df.lib")
  set(DFN_SYS_LIBS ws2_32 userenv ntdll bcrypt advapi32)
elseif(APPLE)
  set(DFN_LIB_DEFAULT "${DFN_DIR}/target/release/libdf.a") # universel : lipo des deux cibles (voir CI)
  set(DFN_SYS_LIBS "-framework CoreFoundation" "-framework Security" "-framework SystemConfiguration" iconv)
else()
  set(DFN_LIB_DEFAULT "${DFN_DIR}/target/release/libdf.a")
  set(DFN_SYS_LIBS dl pthread m)
endif()
set(DFN_LIB "${DFN_LIB_DEFAULT}" CACHE FILEPATH "Bibliotheque statique libDF")

add_library(gv_dsp STATIC dsp/VoiceChain.cpp dsp/Calibration.cpp)
target_include_directories(gv_dsp PUBLIC dsp)
target_link_libraries(gv_dsp PRIVATE rnnoise PUBLIC "${DFN_LIB}" ${DFN_SYS_LIBS})

# ---- Auto-test DSP (sans OBS) : cmake -DGV_DSP_ONLY=ON ... puis ctest
enable_testing()
add_executable(test_chain tests/test_chain.cpp)
target_link_libraries(test_chain PRIVATE gv_dsp)
add_test(NAME test_chain COMMAND test_chain "${DFN_MODEL}")
add_executable(test_calibration tests/test_calibration.cpp)
target_link_libraries(test_calibration PRIVATE gv_dsp)
add_test(NAME test_calibration COMMAND test_calibration)

if(MSVC)
  target_compile_options(gv_dsp PRIVATE /W4 /fp:fast)
else()
  target_compile_options(gv_dsp PRIVATE -Wall -Wextra)
endif()

option(GV_DSP_ONLY "Ne compiler que le coeur DSP et ses tests (pas besoin de libobs)" OFF)
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
