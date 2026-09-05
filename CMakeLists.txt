cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0077 NEW)

project(
  clay
  VERSION 0.1.0
  DESCRIPTION "A bespoke, self-contained game engine. C23 core, C++23 engine."
  LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Language standards — the clay divide, enforced here:
#   src/core  -> plain C23, exposed through one C ABI header (include/clay/clay.h)
#   src/engine+ -> C++23, consumes clay.h across `extern "C"`
# Nothing in src/core may include a C++ header; nothing in test/ may include
# a core .c file directly (tests link clay_core).
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

# GCC/Clang warning flags; MSVC rejects these spellings, so guard them off.
if(NOT MSVC)
  add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-function)
else()
  add_compile_options(/W4)
endif()

option(CLAY_BUILD_TESTS "Build unit tests" ON)
option(CLAY_BUILD_INTERACTIVE "Build the interactive `clay_player` app" ON)
option(CLAY_BUILD_EXAMPLES "Build standalone host examples" ON)
option(CLAY_BUILD_SHARED "Build clay_engine as a shared library for host bindings" ON)
option(CLAY_BUILD_AUDIO_DEVICE
       "Build the optional miniaudio playback device backend" ON)
option(CLAY_BUILD_AUDIO_VORBIS
       "Enable Ogg Vorbis decoding through the pinned stb_vorbis source" ON)
option(CLAY_BUILD_GDEXTENSION
       "Build the optional Godot GDExtension bootstrap library" OFF)
option(CLAY_INSTALL_GODOT_NATIVE
       "Install the shared Clay library into the Godot native layout" ON)

# ------------------------------------------------------------------ third party
# Deliberate, minimal dependency set (see third_party/README.md):
#   - GLFW          windowing + input presentation (bus-tested, from glfw.org)
#   - miniaudio     optional realtime audio device (public domain / MIT-0)
#   - stb_image     PNG decode (imageio)
#   - stb_image_write PNG encode (imageio, framing dumps)
#   - doctest       C++ unit test harness (the *test/*.cpp executables)
# The C23 core stays std-only by design; these all live in the C++ layer.

add_library(clay_third_party INTERFACE)
# SYSTEM include dir: vendored headers (stb, doctest) are external — warnings
# emitted from inside them are not ours to fix, so treat them as system headers.
target_include_directories(
  clay_third_party SYSTEM INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party>)

if(CLAY_BUILD_AUDIO_VORBIS)
  include(FetchContent)
  FetchContent_Declare(stb_vorbis
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
    GIT_SHALLOW FALSE)
  FetchContent_GetProperties(stb_vorbis)
  if(NOT stb_vorbis_POPULATED)
    cmake_policy(PUSH)
    if(POLICY CMP0169)
      cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(stb_vorbis)
    cmake_policy(POP)
  endif()
  target_include_directories(clay_third_party SYSTEM INTERFACE
    $<BUILD_INTERFACE:${stb_vorbis_SOURCE_DIR}>)
  target_compile_definitions(clay_third_party INTERFACE CLAY_HAS_STB_VORBIS=1)
endif()

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
    include(FetchContent)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    # Wayland requires wayland-scanner which isn't always installed on CI runners.
    # X11 via XCB is the portable fallback on Linux.
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
      GIT_REPOSITORY https://github.com/glfw/glfw.git
      GIT_TAG 3.4
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(glfw)
    if(TARGET glfw)
      set(CLAY_HAS_GLFW ON)
      message(STATUS "clay: built GLFW 3.4 from source")
      # GLFW's own sources are third-party; don't gate our strict flags on them.
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
# When the engine is a shared library it links the static core archive, so the
# core objects must be position-independent or the Linux/Windows link fails.
if(CLAY_BUILD_SHARED)
  set_target_properties(clay_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
# On glibc, math functions (sqrtf, cosf, sinf) live in libm, not libc.
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
if(CLAY_BUILD_AUDIO_DEVICE)
  list(APPEND CLAY_ENGINE_SOURCES src/engine/audio/audio_device.cpp)
else()
  # Keep miniaudio's file decoders available to the headless runtime without
  # pulling in platform audio device backends.
  list(APPEND CLAY_ENGINE_SOURCES src/engine/audio/audio_decoder.cpp)
endif()
if(CLAY_HAS_GLFW)
  list(APPEND CLAY_ENGINE_SOURCES src/engine/platform/window_glfw.cpp)
endif()

if(CLAY_BUILD_GDEXTENSION)
  add_library(clay_gdextension SHARED
    integrations/godot-gdextension/clay_gdextension.cpp)
  set_target_properties(clay_gdextension PROPERTIES
    OUTPUT_NAME clay_gdextension)
  target_include_directories(clay_gdextension PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/integrations/godot-gdextension)
  target_link_libraries(clay_gdextension PRIVATE clay_engine)
  install(TARGETS clay_gdextension
    LIBRARY DESTINATION integrations/godot-gdextension/bin
    RUNTIME DESTINATION integrations/godot-gdextension/bin
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES integrations/godot-gdextension/clay.gdextension
    integrations/godot-gdextension/README.md
    integrations/godot-gdextension/project.godot
    integrations/godot-gdextension/main.tscn
    integrations/godot-gdextension/main.gd
    DESTINATION integrations/godot-gdextension)
endif()

if(CLAY_BUILD_SHARED)
  add_library(clay_engine SHARED ${CLAY_ENGINE_SOURCES})
  target_compile_definitions(clay_engine PRIVATE CLAY_BUILDING_SHARED=1)
  target_compile_definitions(clay_engine INTERFACE CLAY_USING_SHARED=1)
  # MSVC does not export C++ symbols from a DLL unless they are explicitly
  # annotated. Clay's public C++ API predates export annotations, so have CMake
  # generate the DLL export table from the object files for Windows builds.
  if(WIN32)
    set_target_properties(clay_engine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  endif()
else()
  add_library(clay_engine STATIC ${CLAY_ENGINE_SOURCES})
endif()
target_include_directories(clay_engine PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/engine>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/clay/engine>)
target_link_libraries(clay_engine PUBLIC clay_core clay_third_party)
# A C host may link the static engine through the C ABI. Propagate the C++
# runtime explicitly because CMake otherwise chooses the C linker for that
# consumer and leaves libc++/libstdc++ symbols unresolved.
if(NOT CLAY_BUILD_SHARED AND NOT MSVC)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(clay_engine PUBLIC
      "$<$<LINK_LANGUAGE:C>:stdc++>")
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
# Keep the source tree private: consumers use the stable umbrella header and
# the installed C++ engine headers, which makes Clay usable as a standalone
# library or as the native side of a host integration (e.g. Godot Mono).
install(TARGETS clay_third_party clay_core clay_engine
  EXPORT clayTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES include/clay/clay.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/clay)
install(FILES include/clay/engine_c.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/clay)
install(FILES include/clay/engine.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/clay)
install(FILES third_party/miniaudio.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/clay/engine)
install(DIRECTORY src/core/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/clay/core
  FILES_MATCHING PATTERN "*.h")
install(DIRECTORY src/engine/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/clay/engine
  FILES_MATCHING PATTERN "*.hpp")
install(EXPORT clayTargets
  FILE clayTargets.cmake
  NAMESPACE clay::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/clay)
configure_package_config_file(
  cmake/clayConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/clayConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/clay)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/clayConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/clayConfig.cmake
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
  # Keep a copy beside the sample project so DllImport("clay_engine") resolves
  # immediately after installing the SDK, without a manual staging step.
  install(FILES $<TARGET_FILE:clay_engine>
    DESTINATION integrations/godot-mono)
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
