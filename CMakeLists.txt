cmake_minimum_required(VERSION 3.28...3.30)

# Structure reprise d'obsproject/obs-plugintemplate : nom/version/dependances OBS dans buildspec.json
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # libs statiques liees dans un module partage (Linux)

# ---- Coeur DSP (submodule github.com/Gragarda/gragarda-voice-core) : gv_dsp, ${DFN_MODEL}
# git submodule update --init ; tests du coeur : cmake -DGV_DSP_ONLY=ON (active GV_CORE_TESTS)
# libDF (DeepFilterNet, Rust) doit etre compilee avant, dans ../DeepFilterNet (voir README)
option(GV_DSP_ONLY "Ne compiler que le coeur DSP et ses tests (pas besoin de libobs)" OFF)
set(GV_CORE_TESTS ${GV_DSP_ONLY})
enable_testing()
add_subdirectory(core)
# ponytail: code tiers (rnnoise) / depot separe, hors du -Werror des presets CI du template
set_target_properties(rnnoise gv_dsp PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
if(GV_DSP_ONLY)
  return()
endif()

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs gv_dsp Qt6::Core Qt6::Widgets)
target_compile_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/plugin-main.cpp src/voice-filter.cpp src/calibration-dialog.cpp src/obs-tools.cpp
)

if(MSVC)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /fp:fast)
  # Nom du .pdb seul dans la DLL publiee, pas le chemin du PC de build
  target_link_options(${CMAKE_PROJECT_NAME} PRIVATE /PDBALTPATH:%_PDB%)
elseif(UNIX AND NOT APPLE)
  # Un objet statique (libDF Rust / asm) sans .note.GNU-stack rend la pile executable :
  # glibc >= 2.41 refuse alors le dlopen ("cannot enable executable stack")
  # --exclude-libs : symboles des libs statiques (rnnoise_*, df_*) non exportes. Sinon le
  # rnnoise_create d'obs-filters (autre version, deja charge) remplace le notre -> crash
  target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -Wl,-z,noexecstack -Wl,--exclude-libs,ALL)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# ---- Donnees hors data/ : modele DeepFilterNet + licences (obs_module_file("models/..."))
file(GLOB _licenses "${CMAKE_CURRENT_SOURCE_DIR}/licenses/*")
configure_file(LICENSE "${CMAKE_CURRENT_BINARY_DIR}/gragarda-voice-GPL-2.0.txt" COPYONLY)
list(APPEND _licenses "${CMAKE_CURRENT_BINARY_DIR}/gragarda-voice-GPL-2.0.txt")
if(APPLE)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${DFN_MODEL}" ${_licenses})
  set_property(SOURCE "${DFN_MODEL}" PROPERTY MACOSX_PACKAGE_LOCATION Resources/models)
  set_property(SOURCE ${_licenses} PROPERTY MACOSX_PACKAGE_LOCATION Resources/licenses)
else()
  if(WIN32)
    set(_data_dir "${CMAKE_PROJECT_NAME}/data")
  else()
    set(_data_dir "${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}")
  endif()
  install(FILES "${DFN_MODEL}" DESTINATION "${_data_dir}/models")
  install(FILES ${_licenses} DESTINATION "${_data_dir}/licenses")
endif()
