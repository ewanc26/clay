# Verifies a generated PNG: exists, is a real PNG signature, and is larger
# than EXPECTED_MIN bytes (catches degenerate/blank dumps in headless CI).

if(IN STREQUAL "" OR NOT EXISTS "${IN}")
  message(FATAL_ERROR "missing expected file: ${IN}")
endif()

file(SIZE "${IN}" _size)
if(_size LESS EXPECTED_MIN)
  message(FATAL_ERROR "${IN} is only ${_size} bytes (expected >= ${EXPECTED_MIN})")
endif()

file(READ "${IN}" _head HEX OFFSET 0 LIMIT 8)
# 89 50 4E 47 0D 0A 1A 0A
if(NOT _head STREQUAL "89504e470d0a1a0a")
  message(FATAL_ERROR "${IN} does not start with a PNG signature (${_head})")
endif()

message(STATUS "png ok: ${IN} (${_size} bytes)")