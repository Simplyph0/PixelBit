# Builds the PC emulator and the firmware tests on Linux, macOS and Windows.
#
#   cmake -S tools -B build/tools            (add -DGFX=/path/to/Adafruit_GFX_Library to use a copy you have)
#   cmake --build build/tools
#   ctest --test-dir build/tools --output-on-failure
#
# Compiler: GCC or Clang. On Windows use MinGW-w64 (MSYS2 UCRT64: pacman -S
# mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,SDL2,python}); the firmware sources use
# GCC extensions, so Visual Studio's compiler is not supported.
# SDL2 is optional: with it the emulator gets a window, without it only the terminal.
cmake_minimum_required(VERSION 3.16)
project(PixelBitTools CXX)

if(MSVC)
  message(FATAL_ERROR "Use MinGW-w64 (MSYS2) or Clang on Windows: the firmware sources need GCC extensions.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)           # gnu++17, as the Arduino toolchain
set(CMAKE_CXX_FLAGS_RELEASE "-O1")    # as the Makefiles: same code generation as before
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(FW ${CMAKE_CURRENT_SOURCE_DIR}/../GameSystem)
set(EM ${CMAKE_CURRENT_SOURCE_DIR}/emulator)
set(TESTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)

# ---- Adafruit GFX: the one the Arduino IDE installed, or download it ----
set(GFX "" CACHE PATH "Folder with Adafruit_GFX.cpp (default: the Arduino IDE's copy, else downloaded)")
if(NOT GFX)
  foreach(dir "$ENV{HOME}/Arduino/libraries/Adafruit_GFX_Library"
              "$ENV{HOME}/Documents/Arduino/libraries/Adafruit_GFX_Library"
              "$ENV{USERPROFILE}/Documents/Arduino/libraries/Adafruit_GFX_Library")
    if(EXISTS "${dir}/Adafruit_GFX.cpp")
      set(GFX "${dir}")
      break()
    endif()
  endforeach()
endif()
if(NOT GFX)
  include(FetchContent)
  FetchContent_Declare(adafruit_gfx
    GIT_REPOSITORY https://github.com/adafruit/Adafruit-GFX-Library.git
    GIT_TAG 1.11.9)
  FetchContent_GetProperties(adafruit_gfx)
  if(NOT adafruit_gfx_POPULATED)
    FetchContent_Populate(adafruit_gfx)
  endif()
  set(GFX "${adafruit_gfx_SOURCE_DIR}")
endif()
message(STATUS "Adafruit GFX: ${GFX}")

add_compile_options(-w)
add_compile_definitions(ARDUINO=100)
include_directories(${EM}/stubs ${EM} ${GFX} ${FW})

# ---- operating system layer ----
set(PLATFORM_SRC ${EM}/platform_common.cpp)
if(WIN32)
  list(APPEND PLATFORM_SRC ${EM}/platform_win.cpp)
else()
  list(APPEND PLATFORM_SRC ${EM}/platform_posix.cpp)
endif()
add_library(plat STATIC ${PLATFORM_SRC})
if(WIN32)
  target_link_libraries(plat PUBLIC ws2_32 winmm)
endif()
find_package(Threads)
if(Threads_FOUND)
  target_link_libraries(plat PUBLIC Threads::Threads)
endif()

# ---- the firmware and Adafruit GFX: compiled ONCE, shared by the emulator and every test ----
# A test links the library and the linker takes only the files that test needs, so nothing is
# compiled twice. (Each test used to compile its own copy of every file it used.)
file(GLOB FW_ALL_CPP ${FW}/*.cpp)
add_library(gfx STATIC ${GFX}/Adafruit_GFX.cpp)
add_library(fwlib STATIC ${FW_ALL_CPP})
target_link_libraries(fwlib PUBLIC gfx plat)

# ---- emulator ----
find_package(SDL2 CONFIG QUIET)
add_executable(emulator ${EM}/host.cpp)
target_link_libraries(emulator PRIVATE fwlib plat)
if(TARGET SDL2::SDL2)
  set_source_files_properties(${EM}/host.cpp PROPERTIES COMPILE_DEFINITIONS USE_SDL)   # only the window code needs it
  target_link_libraries(emulator PRIVATE SDL2::SDL2)
  message(STATUS "Emulator: with an SDL2 window")
else()
  message(STATUS "Emulator: terminal only (SDL2 not found)")
endif()

# ---- tests ----
# The test environment (the clock and the stand-in Arduino core) comes in two flavours: with a
# controllable clock, or with the simulated radio and a real clock (USE_AIR). Built once each.
add_library(env_host OBJECT ${TESTS}/hostenv.cpp)
add_library(env_air OBJECT ${TESTS}/hostenv.cpp ${TESTS}/air.cpp)
target_compile_definitions(env_air PRIVATE USE_AIR)

function(pb_test name env)   # pb_test(name host|air sources...)
  add_executable(${name} ${ARGN} $<TARGET_OBJECTS:env_${env}>)
  target_link_libraries(${name} PRIVATE fwlib plat)
  if(env STREQUAL "air")
    target_compile_definitions(${name} PRIVATE USE_AIR)
  endif()
endfunction()

pb_test(logic_test host ${TESTS}/logic_test.cpp)
pb_test(ttt_test host ${TESTS}/ttt_test.cpp)
pb_test(rps_test host ${TESTS}/rps_test.cpp)
pb_test(settings_test host ${TESTS}/settings_test.cpp)
pb_test(vs_test air ${TESTS}/vs_test.cpp)
pb_test(pbv_test host ${TESTS}/pbv_test.cpp)
pb_test(chess_test host ${TESTS}/chess_test.cpp)
pb_test(chess_vs_test air ${TESTS}/chess_vs_test.cpp)
pb_test(checkers_test host ${TESTS}/checkers_test.cpp)
pb_test(checkers_vs_test air ${TESTS}/checkers_vs_test.cpp)
pb_test(mines_test host ${TESTS}/mines_test.cpp)
pb_test(chat_test air ${TESTS}/chat_test.cpp)

# the firmware update: three builds with different version numbers talk to each other.
# All three have fixed versions, so raising OS_VERSION in config.h never changes what the test means.
# Only fwupdate.cpp knows its version, so only that file is built three times.
function(pb_update_test name version number)
  add_library(${name}_fwupdate OBJECT ${FW}/fwupdate.cpp)
  target_compile_definitions(${name}_fwupdate PRIVATE "OS_VERSION=\"${version}\"" OS_VERSION_NUM=${number})
  pb_test(${name} air ${TESTS}/update_test.cpp $<TARGET_OBJECTS:${name}_fwupdate>)
  target_compile_definitions(${name} PRIVATE "OS_VERSION=\"${version}\"" OS_VERSION_NUM=${number})
endfunction()
pb_update_test(update_test v4.2 402)
pb_update_test(update_test_new v4.3 403)
pb_update_test(update_test_old v4.0 400)

enable_testing()
find_package(Python3 COMPONENTS Interpreter)
add_test(NAME logic COMMAND logic_test)
add_test(NAME settings COMMAND settings_test)
add_test(NAME tictactoe COMMAND ttt_test)
add_test(NAME rps COMMAND rps_test)
add_test(NAME chess COMMAND chess_test)
add_test(NAME checkers COMMAND checkers_test)
add_test(NAME mines COMMAND mines_test)
add_test(NAME chat_unit COMMAND chat_test unit)
if(Python3_FOUND)
  add_test(NAME persistence COMMAND ${Python3_EXECUTABLE} ${TESTS}/persistence_check.py $<TARGET_FILE:emulator>)
  add_test(NAME chat COMMAND ${Python3_EXECUTABLE} ${TESTS}/chat_check.py $<TARGET_FILE_DIR:chat_test>)
  add_test(NAME chess_play COMMAND ${Python3_EXECUTABLE} ${TESTS}/chess_check.py $<TARGET_FILE:chess_vs_test>)
  add_test(NAME checkers_play COMMAND ${Python3_EXECUTABLE} ${TESTS}/checkers_check.py $<TARGET_FILE:checkers_vs_test>)
  add_test(NAME versus COMMAND ${Python3_EXECUTABLE} ${TESTS}/vs_check.py $<TARGET_FILE_DIR:vs_test>)
  add_test(NAME firmware_update COMMAND ${Python3_EXECUTABLE} ${TESTS}/update_check.py $<TARGET_FILE_DIR:update_test>)
  add_test(NAME video COMMAND ${Python3_EXECUTABLE} ${TESTS}/video_check.py $<TARGET_FILE:pbv_test> $<TARGET_FILE:emulator>)
  set_tests_properties(persistence video versus firmware_update chat chess_play checkers_play PROPERTIES TIMEOUT 600)
  # The radio tests can run side by side: each uses its own UDP ports (versus 42000+, chat 43000+,
  # chess 44000+, checkers 45000+, the firmware update the default 41000).
endif()
