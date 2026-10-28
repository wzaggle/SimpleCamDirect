cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(SimpleCamPTZ VERSION ${_version} LANGUAGES C CXX)

option(ENABLE_FRONTEND_API "Use the OBS frontend API for docks and scene selection" ON)
option(ENABLE_QT "Use Qt widgets for OBS docks" ON)

include(compilerconfig)
include(defaults)
include(helpers)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(simplecamptz MODULE
  src/plugin-main.cpp
  src/camera_runtime.cpp
  src/camera_profile.cpp
  src/profile_test_tool.cpp
  src/camera_docks.cpp
  src/preset_source.cpp
  resources/simplecamdirect.qrc
)

if(APPLE)
  target_sources(simplecamptz PRIVATE src/mac_dock_support.cpp)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources(simplecamptz PRIVATE src/linux_dock_support.cpp)
endif()

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# plugin-support.c uses OBS's canonical logging declarations from util/base.h.
if(TARGET plugin-support)
  target_link_libraries(plugin-support PRIVATE OBS::libobs)
endif()

target_link_libraries(simplecamptz PRIVATE
  OBS::libobs
  OBS::obs-frontend-api
  Qt6::Core
  Qt6::Widgets
  Qt6::Network
)

set_target_properties(simplecamptz PROPERTIES
  AUTOMOC ON
  AUTOUIC ON
  AUTORCC ON
)

target_include_directories(simplecamptz PRIVATE src)
target_compile_definitions(simplecamptz PRIVATE QT_NO_KEYWORDS)

set_target_properties_plugin(simplecamptz PROPERTIES OUTPUT_NAME ${_name})

# Visual Studio's Open Folder CMake view does not always expose CMake's
# generated INSTALL target consistently. Keep an explicit, discoverable target
# for the normal build-then-install workflow.
add_custom_target(install-simplecamptz
  COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --config "$<CONFIG>"
  COMMENT "Install SimpleCamPTZ and its resources"
  USES_TERMINAL
  VERBATIM
)
add_dependencies(install-simplecamptz simplecamptz)

if(WIN32)
  add_custom_target(package-simplecamptz-installer
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
      -File "${CMAKE_CURRENT_SOURCE_DIR}/build-installer.ps1"
      -BuildDir "${CMAKE_BINARY_DIR}" -Configuration "$<CONFIG>"
    COMMENT "Stage and package the SimpleCamPTZ Windows installer"
    USES_TERMINAL
    VERBATIM
  )
  add_dependencies(package-simplecamptz-installer simplecamptz)
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(simplecamptz_packet_tests tests/visca_packets_test.cpp)
  target_include_directories(simplecamptz_packet_tests PRIVATE src)
  target_compile_features(simplecamptz_packet_tests PRIVATE cxx_std_17)
  # These fixtures use assert(); keep their checks enabled in release CI builds.
  if(MSVC)
    target_compile_options(simplecamptz_packet_tests PRIVATE /UNDEBUG)
  else()
    target_compile_options(simplecamptz_packet_tests PRIVATE -UNDEBUG)
  endif()
  add_test(NAME simplecamptz_packet_tests COMMAND simplecamptz_packet_tests)

  add_executable(simplecamptz_profile_tests
    tests/camera_profile_test.cpp
    src/camera_profile.cpp
  )
  target_include_directories(simplecamptz_profile_tests PRIVATE src)
  target_link_libraries(simplecamptz_profile_tests PRIVATE Qt6::Core)
  target_compile_features(simplecamptz_profile_tests PRIVATE cxx_std_17)
  add_test(NAME simplecamptz_profile_tests
    COMMAND simplecamptz_profile_tests "${CMAKE_CURRENT_SOURCE_DIR}/data/cameras/avipas-av-1280w.visca.json")
endif()

if(BUILD_TESTING)
  include("${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime_state_tests.cmake")
endif()

if(WIN32)
  add_custom_target(build-all-platforms
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
      -File "${CMAKE_CURRENT_SOURCE_DIR}/build-all-platforms.ps1"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Run GitHub Actions for the current committed revision and download all packages"
    USES_TERMINAL
    VERBATIM
  )
endif()

if(BUILD_TESTING)
  add_executable(simplecamptz_program_output_tests EXCLUDE_FROM_ALL tests/program_output_test.cpp)
  target_include_directories(simplecamptz_program_output_tests PRIVATE src)
  target_compile_features(simplecamptz_program_output_tests PRIVATE cxx_std_17)
  target_link_libraries(simplecamptz_program_output_tests PRIVATE OBS::libobs)
  add_test(NAME simplecamptz_program_output_tests COMMAND simplecamptz_program_output_tests)
endif()
