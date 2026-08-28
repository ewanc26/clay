# Real headless run of The Clay Garden as an integration test. Guards the
# entire pipeline: CLI parsing, reaction rules, the reactive loop, the
# recorded .clayrec transcript, replay, PNG encoding, and a non-degenerate
# dump.

set(_smoke_dir "${CMAKE_BINARY_DIR}/smoke")
file(MAKE_DIRECTORY "${_smoke_dir}")

add_test(
  NAME headless_smoke
  COMMAND clay_player --headless --frames 90 --seed 0xCAFEBABE
                      --dump ${_smoke_dir}/garden.png
                      --record ${_smoke_dir}/smoke.clayrec)
set_tests_properties(headless_smoke PROPERTIES
  PASS_REGULAR_EXPRESSION "clay_player: dumped")

add_test(
  NAME headless_smoke_replay
  COMMAND clay_player --headless --frames 90 --seed 0xCAFEBABE
                      --replay ${_smoke_dir}/smoke.clayrec
                      --dump ${_smoke_dir}/garden_replay.png)
set_tests_properties(headless_smoke_replay PROPERTIES
  PASS_REGULAR_EXPRESSION "clay_player: dumped"
  DEPENDS headless_smoke)

add_test(
  NAME headless_smoke_png
  COMMAND ${CMAKE_COMMAND}
          -DIN=${_smoke_dir}/garden.png
          -DEXPECTED_MIN=8000
          -P ${PROJECT_SOURCE_DIR}/cmake/verify_png.cmake)
set_tests_properties(headless_smoke_png PROPERTIES
  DEPENDS headless_smoke)