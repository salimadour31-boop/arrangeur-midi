cmake_minimum_required(VERSION 3.15)
project(arrangeur-midi VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

# MSVC ships no usable GL/glext.h: use the official Khronos headers (MIT) on Windows
if(WIN32)
  include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/khronos)
endif()

add_subdirectory(dpf)

# Formats: VST3 + VST2 (FL Studio: the VST2 version is the most reliable for MIDI out), CLAP, LV2
option(AM_STANDALONE "Also build the standalone (JACK) app, used for screenshots" OFF)
set(AM_TARGETS vst3 vst2 clap lv2)
if(AM_STANDALONE)
  list(APPEND AM_TARGETS jack)
endif()
dpf_add_plugin(arrangeur-midi
  TARGETS ${AM_TARGETS}
  FILES_DSP plugin/ArrangeurPlugin.cpp
  FILES_UI  plugin/ArrangeurUI.cpp)
target_include_directories(arrangeur-midi PUBLIC plugin src)

# Offline tests of the music logic (no host needed)
enable_testing()
add_executable(test_music tests/test_music.cpp)
target_include_directories(test_music PRIVATE src)
if(MSVC)
  target_compile_definitions(test_music PRIVATE _USE_MATH_DEFINES)
  target_compile_options(test_music PRIVATE /utf-8)
  target_compile_options(arrangeur-midi PUBLIC /utf-8)
endif()
add_test(NAME music COMMAND test_music)
