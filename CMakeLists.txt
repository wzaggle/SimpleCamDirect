cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version} LANGUAGES C CXX)

option(ENABLE_FRONTEND_API "Use the OBS frontend API for docks and scene selection" ON)
option(ENABLE_QT "Use Qt widgets for OBS docks" ON)

include(compilerconfig)
include(defaults)
include(helpers)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(${CMAKE_PROJECT_NAME} MODULE
  src/plugin-main.cpp
  src/camera_runtime.cpp
  src/camera_docks.cpp
  src/preset_source.cpp
)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  OBS::libobs
  OBS::obs-frontend-api
  Qt6::Core
  Qt6::Widgets
  Qt6::Network
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
  AUTOMOC ON
  AUTOUIC ON
  AUTORCC ON
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE QT_NO_KEYWORDS)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

include(CTest)
if(BUILD_TESTING)
  add_executable(simplecamdirect_packet_tests tests/visca_packets_test.cpp)
  target_include_directories(simplecamdirect_packet_tests PRIVATE src)
  target_compile_features(simplecamdirect_packet_tests PRIVATE cxx_std_17)
  add_test(NAME simplecamdirect_packet_tests COMMAND simplecamdirect_packet_tests)
endif()
