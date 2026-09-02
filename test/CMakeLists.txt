# Clay test suite: one executable per module, wired into ctest.
#
# C core tests (test_c.h harness, strict C23, link clay_core only):
#   test_math/test_arena/test_json/test_hmap/test_variant/test_rng
#   test_input/test_event/test_input_log
#
# C++ engine tests (doctest, link clay_engine):
#   test_ecs/test_raster/test_actions/test_command
#   test_collision
#   test_reactions/test_runtime   (the pipeline + replay determinism)

function(clay_c_test name)
  add_executable(${name} ${name}.c)
  target_include_directories(${name} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(${name} PRIVATE clay_core)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

function(clay_cpp_test name)
  add_executable(${name} ${name}.cpp)
  target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/src/engine)
  target_link_libraries(${name} PRIVATE clay_engine)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

clay_c_test(test_math)
clay_c_test(test_arena)
clay_c_test(test_json)
clay_c_test(test_hmap)
clay_c_test(test_variant)
clay_c_test(test_rng)
clay_c_test(test_input)
clay_c_test(test_event)
clay_c_test(test_input_log)

clay_cpp_test(test_ecs)
clay_cpp_test(test_raster)
clay_cpp_test(test_raster3d)
clay_cpp_test(test_scene3d)
clay_cpp_test(test_text)
clay_cpp_test(test_actions)
clay_cpp_test(test_command)
clay_cpp_test(test_collision)
clay_cpp_test(test_reactions)
clay_cpp_test(test_runtime)
clay_cpp_test(test_clay_file)
clay_cpp_test(test_engine_c_api)

# The garden itself must survive a real headless run, not just unit tests:
# every frame it renders deterministically and the dump is a non-degenerate
# PNG. Guards the CLI + reaction-rules + presenter plumbing end to end.
include(${PROJECT_SOURCE_DIR}/cmake/headless_smoke.cmake)

# A real .clay scene must also survive the CLI path, and two independent runs
# must produce byte-identical PNGs. CI runs this on Linux and macOS.
set(_scene_smoke_dir "${CMAKE_BINARY_DIR}/scene-smoke")
file(MAKE_DIRECTORY "${_scene_smoke_dir}")
set(_scene_file "${PROJECT_SOURCE_DIR}/demo/scenes/tabletop.clay")

add_test(
  NAME headless_scene_smoke_a
  COMMAND clay_player --headless --scene ${_scene_file} --frames 1
                      --dump ${_scene_smoke_dir}/scene-a.png)
set_tests_properties(headless_scene_smoke_a PROPERTIES
  PASS_REGULAR_EXPRESSION "clay_player: dumped")

add_test(
  NAME headless_scene_smoke_b
  COMMAND clay_player --headless --scene ${_scene_file} --frames 1
                      --dump ${_scene_smoke_dir}/scene-b.png)
set_tests_properties(headless_scene_smoke_b PROPERTIES
  PASS_REGULAR_EXPRESSION "clay_player: dumped")

add_test(
  NAME headless_scene_determinism
  COMMAND ${CMAKE_COMMAND} -E compare_files
          ${_scene_smoke_dir}/scene-a.png
          ${_scene_smoke_dir}/scene-b.png)
set_tests_properties(headless_scene_determinism PROPERTIES
  DEPENDS "headless_scene_smoke_a;headless_scene_smoke_b")
