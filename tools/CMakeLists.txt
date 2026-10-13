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

# ---- the firmware, as the emulator and the tests use it ----
file(GLOB FW_ALL_CPP ${FW}/*.cpp)

# ---- emulator ----
find_package(SDL2 CONFIG QUIET)
add_executable(emulator ${EM}/host.cpp ${FW_ALL_CPP} ${GFX}/Adafruit_GFX.cpp)
target_link_libraries(emulator PRIVATE plat)
if(TARGET SDL2::SDL2)
  target_compile_definitions(emulator PRIVATE USE_SDL)
  target_link_libraries(emulator PRIVATE SDL2::SDL2)
  message(STATUS "Emulator: with an SDL2 window")
else()
  message(STATUS "Emulator: terminal only (SDL2 not found)")
endif()

# ---- tests ----
set(CORE_SRC ${TESTS}/hostenv.cpp ${GFX}/Adafruit_GFX.cpp
    ${FW}/display.cpp ${FW}/input.cpp ${FW}/ui.cpp ${FW}/fx.cpp)
set(LINK_SRC ${FW}/lobby.cpp ${FW}/gamelink.cpp ${FW}/crypto.cpp)
set(GAME_SRC)
foreach(g snake tetris flappy game2048 breakout frogger helicopter pacman pong invaders dino)
  list(APPEND GAME_SRC ${FW}/${g}.cpp)
endforeach()

function(pb_test name)   # pb_test(name sources...)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE plat)
endfunction()

pb_test(logic_test ${TESTS}/logic_test.cpp ${CORE_SRC} ${LINK_SRC} ${GAME_SRC})
pb_test(ttt_test ${TESTS}/ttt_test.cpp ${CORE_SRC} ${FW}/tictactoe.cpp ${LINK_SRC})
pb_test(rps_test ${TESTS}/rps_test.cpp ${CORE_SRC} ${FW}/rps.cpp ${LINK_SRC})
pb_test(settings_test ${TESTS}/settings_test.cpp ${CORE_SRC} ${FW}/settings.cpp)
pb_test(vs_test ${TESTS}/vs_test.cpp ${TESTS}/air.cpp ${CORE_SRC} ${LINK_SRC}
        ${FW}/pong.cpp ${FW}/tictactoe.cpp ${FW}/rps.cpp ${FW}/connect4.cpp)
target_compile_definitions(vs_test PRIVATE USE_AIR)

# the firmware update: three builds with different version numbers talk to each other
function(pb_update_test name)   # pb_update_test(name [version number])
  pb_test(${name} ${TESTS}/update_test.cpp ${TESTS}/air.cpp ${CORE_SRC} ${LINK_SRC} ${FW}/fwupdate.cpp)
  target_compile_definitions(${name} PRIVATE USE_AIR)
  if(ARGC GREATER 1)
    target_compile_definitions(${name} PRIVATE "OS_VERSION=\"${ARGV1}\"" OS_VERSION_NUM=${ARGV2})
  endif()
endfunction()
pb_update_test(update_test)
pb_update_test(update_test_new v4.3 403)
pb_update_test(update_test_old v4.0 400)

enable_testing()
find_package(Python3 COMPONENTS Interpreter)
add_test(NAME logic COMMAND logic_test)
add_test(NAME settings COMMAND settings_test)
add_test(NAME tictactoe COMMAND ttt_test)
add_test(NAME rps COMMAND rps_test)
if(Python3_FOUND)
  add_test(NAME persistence COMMAND ${Python3_EXECUTABLE} ${TESTS}/persistence_check.py $<TARGET_FILE:emulator>)
  add_test(NAME versus COMMAND ${Python3_EXECUTABLE} ${TESTS}/vs_check.py $<TARGET_FILE_DIR:vs_test>)
  add_test(NAME firmware_update COMMAND ${Python3_EXECUTABLE} ${TESTS}/update_check.py $<TARGET_FILE_DIR:update_test>)
  set_tests_properties(persistence versus firmware_update PROPERTIES TIMEOUT 600)
  # versus and firmware_update talk over the same UDP ports: never at the same time, even with ctest -j
  set_tests_properties(versus firmware_update PROPERTIES RESOURCE_LOCK radio)
endif()
