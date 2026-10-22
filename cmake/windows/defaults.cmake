# CMake Windows defaults module

include_guard(GLOBAL)

# Enable find_package targets to become globally available targets
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)

include(buildspec)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(
    CMAKE_INSTALL_PREFIX
    "$ENV{ALLUSERSPROFILE}/obs-studio/plugins"
    CACHE PATH
    "Default plugin installation directory"
    FORCE
  )
endif()

# Keep the prefix in CMake path syntax in the generated cmake_install.cmake.
# A native Windows path such as C:\ProgramData can trigger an invalid \P escape
# warning when CMake writes that value into its generated script.
if(WIN32)
  file(TO_CMAKE_PATH "${CMAKE_INSTALL_PREFIX}" _install_prefix_cmake_path)
  set(
    CMAKE_INSTALL_PREFIX
    "${_install_prefix_cmake_path}"
    CACHE PATH
    "Installation directory"
    FORCE
  )
  unset(_install_prefix_cmake_path)
endif()
