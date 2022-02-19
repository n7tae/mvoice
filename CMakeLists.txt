cmake_minimum_required(VERSION 3.7.2)
project(mvoice LANGUAGES CXX VERSION 0.0.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(GNUInstallDirs)

#select the release build type by default to get optimization flags
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: defaulting to release.")
endif()

#option (PREFER_CONVENIENCE_COPIES
#        "Use local copies instead of system-wide version" ON)
#find_package(Codec2)

find_package(ALSA)
find_package(PkgConfig)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0 IMPORTED_TARGET)
pkg_check_modules(Curl REQUIRED libcurl IMPORTED_TARGET)
find_package(Threads REQUIRED)

add_custom_command(OUTPUT resources.cpp
  COMMAND glib-compile-resources
          --sourcedir="${CMAKE_CURRENT_SOURCE_DIR}"
          "${CMAKE_CURRENT_SOURCE_DIR}/mvoice.gresources.xml"
          --target=resources.cpp --generate-source
  COMMENT "compile resources")

add_executable (mvoice
  codec2/codebooks.cpp
  codec2/codec2.cpp
  codec2/kiss_fft.cpp
  codec2/lpc.cpp
  codec2/nlp.cpp
  codec2/pack.cpp
  codec2/qbase.cpp
  codec2/quantise.cpp
  AboutDlg.cpp
  AudioManager.cpp
  Base.cpp
  Callsign.cpp
  Configure.cpp
  CRC.cpp
  M17Gateway.cpp
  M17RouteMap.cpp
  MainWindow.cpp
  SettingsDlg.cpp
  UDPSocket.cpp
  UnixDgramSocket.cpp
  WaitCursor.cpp
  resources.cpp
  )

target_include_directories(mvoice PRIVATE codec2)
target_link_libraries(mvoice
  PkgConfig::GTKMM ALSA::ALSA PkgConfig::Curl Threads::Threads)
install(TARGETS mvoice RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
