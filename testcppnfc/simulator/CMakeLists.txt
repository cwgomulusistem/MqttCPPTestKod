cmake_minimum_required(VERSION 3.16)
project(funtoria_sdl_sim LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 CONFIG QUIET)
if(SDL2_FOUND)
  if(TARGET SDL2::SDL2)
    set(SDL2_LINK_TARGET SDL2::SDL2)
  elseif(TARGET SDL2::SDL2-static)
    set(SDL2_LINK_TARGET SDL2::SDL2-static)
  endif()
endif()

if(NOT SDL2_LINK_TARGET)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  set(SDL2_LINK_TARGET PkgConfig::SDL2)
endif()

# Use dedicated LVGL config for desktop simulator.
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf_sim.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(LV_BUILD_SET_CONFIG_OPTS OFF CACHE BOOL "" FORCE)
set(LV_BUILD_LVGL_H_SIMPLE_INCLUDE ON CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../lib/lvgl" "${CMAKE_CURRENT_BINARY_DIR}/lvgl")

add_executable(funtoria_sdl_sim
  main.cpp
  ../src/screens/funtoria_screen_custom_text.c
  ../src/screens/funtoria_screen_error.c
  ../src/screens/funtoria_screen_nfc_read.c
  ../src/screens/funtoria_screen_payment_fail.c
  ../src/screens/funtoria_screen_payment_ok.c
  ../src/screens/funtoria_screen_phone_detected.c
  ../src/screens/funtoria_screen_setup_required.c
  ../src/screens/funtoria_screen_waiting.c
)

target_include_directories(funtoria_sdl_sim PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/screens"
)

target_link_libraries(funtoria_sdl_sim PRIVATE
  lvgl::lvgl
  ${SDL2_LINK_TARGET}
)
