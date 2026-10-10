cmake_minimum_required(VERSION 3.24)
project(gragarda-voice VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# libobs : pointe CMAKE_PREFIX_PATH vers ton build d'obs-studio
# ex: -DCMAKE_PREFIX_PATH="C:/dev/obs-studio/build_x64"
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

add_library(gv_dsp STATIC dsp/VoiceChain.cpp dsp/Calibration.cpp)
target_include_directories(gv_dsp PUBLIC dsp)
target_link_libraries(gv_dsp PRIVATE rnnoise
  PUBLIC "${DFN_DIR}/target/release/df.lib" ws2_32 userenv ntdll bcrypt advapi32)

# ---- Auto-test DSP (sans OBS) : cmake -DGV_DSP_ONLY=ON ... puis ctest
enable_testing()
add_executable(test_chain tests/test_chain.cpp)
target_link_libraries(test_chain PRIVATE gv_dsp)
add_test(NAME test_chain COMMAND test_chain "${DFN_MODEL}")
add_executable(test_calibration tests/test_calibration.cpp)
target_link_libraries(test_calibration PRIVATE gv_dsp)
add_test(NAME test_calibration COMMAND test_calibration)

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
  target_compile_options(gv_dsp PRIVATE /W4 /fp:fast)
endif()

# Installation dans OBS (cmake --install en terminal admin)
set(OBS_DIR "C:/Program Files/obs-studio" CACHE PATH "Dossier d'installation d'OBS")
install(TARGETS gragarda-voice LIBRARY DESTINATION "${OBS_DIR}/obs-plugins/64bit")
install(DIRECTORY data/ DESTINATION "${OBS_DIR}/data/obs-plugins/gragarda-voice")
install(DIRECTORY licenses/ DESTINATION "${OBS_DIR}/data/obs-plugins/gragarda-voice/licenses")
install(FILES "${DFN_MODEL}" DESTINATION "${OBS_DIR}/data/obs-plugins/gragarda-voice/models")
