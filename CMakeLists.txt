cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

project(mvoice LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

option(AUDIO "audio API")
if(NOT AUDIO)
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set(AUDIO alsa)
    endif()
endif()

option(BASEDIR "base directory")
if(NOT BASEDIR)
    set(BASEDIR $ENV{HOME})
endif()

option(CFGDIR "directory for user configuration files")
if(NOT CFGDIR)
    set(CFGDIR ".config/mvoice")
endif()

option(USE44100 "use 44100Hz sampling instead of 8000Hz" OFF)
option(DISABLE_OPENDHT "disable OpenDHT support" OFF)
option(DEBUG "debug build" OFF)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

find_package(X11 REQUIRED)
include_directories(${X11_INCLUDE_DIR})

set(FLTK_SKIP_OPENGL ON)
set(FLTK_SKIP_FORMS ON)
set(FLTK_SKIP_FLUID ON)
find_package(FLTK REQUIRED)
include_directories(${FLTK_INCLUDE_DIRS})

find_package(Intl REQUIRED)
include_directories(${Intl_INCLUDE_DIRS})

pkg_check_modules(LIBCURL REQUIRED libcurl)
include_directories(${LIBCURL_INCLUDE_DIRS})
link_directories(${LIBCURL_LIBRARY_DIRS})

set(CMAKE_DEPFILE_FLAGS_CXX "-MMD")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -I${CMAKE_SOURCE_DIR}/codec2 -DCFGDIR=\\\"${CFGDIR}\\\" -DBASEDIR=\\\"${BASEDIR}\\\"")
if(DEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb")
endif()
if(USE44100)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE44100")
    set(RESAMPLER_SRC Resampler.cpp)
endif()

if(NOT(DISABLE_OPENDHT))
    pkg_check_modules(LIBOPENDHT opendht)
endif()
if(NOT(DISABLE_OPENDHT) AND LIBOPENDHT_FOUND)
    include_directories(${LIBOPENDHT_INCLUDE_DIRS})
    link_directories(${LIBOPENDHT_LIBRARY_DIRS})
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNO_DHT")
endif()

if(${AUDIO} STREQUAL "alsa")
    pkg_check_modules(AUDIO_API REQUIRED alsa)
else()
    message(FATAL_ERROR "unsupported audio API")
endif()
include_directories(${AUDIO_API_INCLUDE_DIRS})
link_directories(${AUDIO_API_LIBRARY_DIRS})

file(GLOB SRC
	codec2/*.cpp
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
	TransmitButton.cpp
	UDPSocket.cpp
	UnixDgramSocket.cpp
	${RESAMPLER_SRC}
)

add_executable(${PROJECT_NAME} ${SRC})
target_link_libraries(${PROJECT_NAME} Threads::Threads ${FLTK_LIBRARIES} ${AUDIO_API_LIBRARIES} ${LIBCURL_LIBRARIES} ${LIBOPENDHT_LIBRARIES} ${Intl_LIBRARY})

install(TARGETS ${PROJECT_NAME} DESTINATION ${BASEDIR}/bin)
