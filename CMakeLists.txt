cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0077 NEW)
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

project(
  clay
  VERSION 0.1.0
  DESCRIPTION "A bespoke, self-contained game engine. C23 core, C++23 engine."
  LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(FetchContent)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

if(NOT MSVC)
  add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-function)
else()
  add_compile_options(/W4)
endif()

option(CLAY_BUILD_TESTS "Build unit tests" ON)
option(CLAY_BUILD_INTERACTIVE "Build the interactive `clay_player` app" ON)
option(CLAY_BUILD_EXAMPLES "Build standalone host examples" ON)
option(CLAY_BUILD_SHARED "Build clay_engine as a shared library for host bindings" ON)
option(CLAY_INSTALL_GODOT_NATIVE
       "Install the shared Clay library into the Godot native layout" ON)

# ------------------------------------------------------------------ third party
# The C23 core stays std-only. The C++ layer uses vendored stb/doctest plus a
# pinned miniaudio source checkout for WAV/OGG decoding, resampling and native
# device output. miniaudio's extras/stb_vorbis.c supplies Vorbis decoding.
add_library(clay_third_party INTERFACE)
target_include_directories(
  clay_third_party SYSTEM INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party>)

FetchContent_Declare(miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio.git
  GIT_TAG 0.11.25
  GIT_SHALLOW TRUE)
FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
  FetchContent_Populate(miniaudio)
endif()
target_include_directories(clay_third_party SYSTEM INTERFACE
  $<BUILD_INTERFACE:${miniaudio_SOURCE_DIR}>
  $<BUILD_INTERFACE:${miniaudio_SOURCE_DIR}/extras>)

option(CLAY_USE_SYSTEM_GLFW
       "Prefer the system glfw3 instead of fetching and building one"
       AUTO)

if(CLAY_BUILD_INTERACTIVE)
  set(CLAY_HAS_GLFW OFF)
  if(CLAY_USE_SYSTEM_GLFW)
    find_package(glfw3 QUIET)
  endif()
  if(glfw3_FOUND)
    set(CLAY_HAS_GLFW ON)
    message(STATUS "clay: using system glfw3")
  elseif(NOT CLAY_USE_SYSTEM_GLFW STREQUAL "OFF")
    message(STATUS "clay: glfw3 not found — fetching GLFW 3.4 from GitHub")
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
      GIT_REPOSITORY https://github.com/glfw/glfw.git
      GIT_TAG 3.4
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(glfw)
    if(TARGET glfw)
      set(CLAY_HAS_GLFW ON)
      message(STATUS "clay: built GLFW 3.4 from source")
      if(NOT MSVC)
        target_compile_options(glfw PRIVATE -w)
      endif()
    endif()
  endif()
  if(NOT CLAY_HAS_GLFW)
    message(WARNING "clay: no GLFW available — clay_player stays headless")
  endif()
endif()

# ---------------------------------------------------------------- clay_core (C23)
set(CLAY_CORE_SOURCES
  src/core/common.c
  src/core/log.c
  src/core/arena.c
  src/core/math.c
  src/core/variant.c
  src/core/hmap.c
  src/core/json.c
  src/core/rng.c
  src/core/time.c
  src/core/input.c
  src/core/event.c
  src/core/input_log.c
)

add_library(clay_core STATIC ${CLAY_CORE_SOURCES})
target_include_directories(clay_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
set_target_properties(clay_core PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED ON)
if(CLAY_BUILD_SHARED)
  set_target_properties(clay_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
if(NOT APPLE AND NOT MSVC)
  target_link_libraries(clay_core PUBLIC m)
endif()

# ------------------------------------------------------------------- clay_engine (C++23)
set(CLAY_ENGINE_SOURCES
  src/engine/event.cpp
  src/engine/action.cpp
  src/engine/command.cpp
  src/engine/replay.cpp
  src/engine/input_system.cpp
  src/engine/ecs/world.cpp
  src/engine/audio/audio_system.cpp
  src/engine/render/raster.cpp
  src/engine/render/raster3d.cpp
  src/engine/render/scene3d.cpp
  src/engine/render/renderer_sw.cpp
  src/engine/render/render_system.cpp
  src/engine/render/text.cpp
  src/engine/systems/system_graph.cpp
  src/engine/systems/reaction.cpp
  src/engine/systems/builtin.cpp
  src/engine/systems/collision.cpp
  src/engine/ui/imgui.cpp
  src/engine/runtime.cpp
  src/engine/imageio.cpp
  src/engine/engine_c.cpp
)
if(CLAY_HAS_GLFW)
  list(APPEND CLAY_ENGINE_SOURCES src/engine/platform/window_glfw.cpp)
endif()

if(CLAY_BUILD_SHARED)
  add_library(clay_engine SHARED ${CLAY_ENGINE_SOURCES})
  target_compile_definitions(clay_engine PRIVATE CLAY_BUILDING_SHARED=1)
  target_compile_definitions(clay_engine INTERFACE CLAY_USING_SHARED=1)
  if(WIN32)
    set_target_properties(clay_engine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  endif()
else()
  add_library(clay_engine STATIC ${CLAY_ENGINE_SOURCES})
endif()
if(WIN32)
  # miniaudio includes windows.h; prevent its legacy min/max macros from
  # rewriting std::min/std::max and numeric_limits::max in engine code.
  target_compile_definitions(clay_engine PRIVATE NOMINMAX)
endif()
target_include_directories(clay_engine PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/engine>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/clay/engine>)
target_link_libraries(clay_engine PUBLIC clay_core clay_third_party)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(clay_engine PRIVATE ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})
if(NOT CLAY_BUILD_SHARED AND NOT MSVC)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(clay_engine PUBLIC "$<$<LINK_LANGUAGE:C>:stdc++>")
  else()
    target_link_libraries(clay_engine PUBLIC "$<$<LINK_LANGUAGE:C>:c++>")
  endif()
endif()

if(CLAY_HAS_GLFW)
  target_compile_definitions(clay_engine PUBLIC CLAY_HAS_GLFW=1)
  find_package(OpenGL REQUIRED)
  target_link_libraries(clay_engine PUBLIC glfw OpenGL::GL)
endif()

# ---------------------------------------------------------------------- tests
if(CLAY_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

# ----------------------------------------------------------------------- demo
add_subdirectory(demo)
if(CLAY_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# --------------------------------------------------------------------- install
install(TARGETS clay_third_party clay_core clay_engine
  EXPORT clayTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES include/clay/clay.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/clay)
install(FILES include/clay/engine_c.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/clay)
install(FILES include/clay/engine.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/clay)
install(DIRECTORY src/core/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/clay/core
  FILES_MATCHING PATTERN "*.h")
install(DIRECTORY src/engine/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/clay/engine
  FILES_MATCHING PATTERN "*.hpp")
install(EXPORT clayTargets FILE clayTargets.cmake NAMESPACE clay::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/clay)
configure_package_config_file(cmake/clayConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/clayConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/clay)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/clayConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/clayConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/clayConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/clay)
install(FILES LICENSE third_party/README.md third_party/doctest/LICENSE.txt
  DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/clay)
if(CLAY_INSTALL_GODOT_NATIVE AND CLAY_BUILD_SHARED)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CLAY_GODOT_PLATFORM "macos")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CLAY_GODOT_PLATFORM "windows")
  else()
    set(CLAY_GODOT_PLATFORM "linux")
  endif()
  install(FILES $<TARGET_FILE:clay_engine>
    DESTINATION integrations/godot-mono/native/${CLAY_GODOT_PLATFORM})
  install(FILES $<TARGET_FILE:clay_engine> DESTINATION integrations/godot-mono)
endif()
install(FILES
  integrations/godot-mono/ClayRuntime.cs
  integrations/godot-mono/ClayKey.cs
  integrations/godot-mono/ClayModifiers.cs
  integrations/godot-mono/ClayGodotSample.csproj
  integrations/godot-mono/ClayGodotSample.sln
  integrations/godot-mono/ClayDemo.cs
  integrations/godot-mono/ClayDemo.tscn
  integrations/godot-mono/export_presets.cfg
  integrations/godot-mono/project.godot
  integrations/godot-mono/README.md
  DESTINATION integrations/godot-mono)
install(FILES integrations/godot-mono/managed/ClayRuntime.csproj
  DESTINATION integrations/godot-mono/managed)
install(PROGRAMS scripts/godot_mono_stage_native.sh
  DESTINATION integrations/godot-mono)
