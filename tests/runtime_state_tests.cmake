# Included after the project Qt/OBS/plugin-support targets exist.
add_executable(simplecamptz_runtime_state_tests EXCLUDE_FROM_ALL
  tests/camera_runtime_state_test.cpp
  src/camera_runtime.cpp src/panasonic_aw.cpp
  src/visca_udp_transport.cpp
  src/camera_profile.cpp src/panasonic_aw_protocol.cpp
)
target_include_directories(simplecamptz_runtime_state_tests PRIVATE src)
target_compile_features(simplecamptz_runtime_state_tests PRIVATE cxx_std_17)
target_compile_definitions(simplecamptz_runtime_state_tests PRIVATE QT_NO_KEYWORDS)
target_link_libraries(simplecamptz_runtime_state_tests PRIVATE
  Qt6::Core Qt6::Network OBS::libobs plugin-support)
add_test(NAME simplecamptz_runtime_state_tests
  COMMAND simplecamptz_runtime_state_tests "${CMAKE_CURRENT_SOURCE_DIR}/data/cameras/avipas-av-1280w.visca.json")

# Explicit opt-in: does not add test executables to a normal Install build.
add_executable(simplecamptz_camera_concurrency_tests EXCLUDE_FROM_ALL
  tests/camera_concurrency_test.cpp
  src/camera_runtime.cpp src/panasonic_aw.cpp
  src/visca_udp_transport.cpp
  src/camera_profile.cpp src/panasonic_aw_protocol.cpp
)
target_include_directories(simplecamptz_camera_concurrency_tests PRIVATE src)
target_compile_features(simplecamptz_camera_concurrency_tests PRIVATE cxx_std_17)
target_compile_definitions(simplecamptz_camera_concurrency_tests PRIVATE QT_NO_KEYWORDS)
target_link_libraries(simplecamptz_camera_concurrency_tests PRIVATE
  Qt6::Core Qt6::Network OBS::libobs plugin-support)
add_test(NAME simplecamptz_camera_concurrency_tests
  COMMAND simplecamptz_camera_concurrency_tests "${CMAKE_CURRENT_SOURCE_DIR}/data/cameras/avipas-av-1280w.visca.json")

