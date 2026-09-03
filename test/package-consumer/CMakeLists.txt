cmake_minimum_required(VERSION 3.20)

project(clay_package_consumer LANGUAGES C CXX)

enable_testing()

find_package(clay CONFIG REQUIRED)

add_executable(clay_package_consumer main.c)
target_link_libraries(clay_package_consumer PRIVATE clay::clay_engine)
add_test(NAME clay_package_consumer COMMAND clay_package_consumer)

add_executable(clay_cpp_package_consumer main.cpp)
target_compile_features(clay_cpp_package_consumer PRIVATE cxx_std_23)
target_link_libraries(clay_cpp_package_consumer PRIVATE clay::clay_engine)
add_test(NAME clay_cpp_package_consumer COMMAND clay_cpp_package_consumer)

get_target_property(_clay_engine_type clay::clay_engine TYPE)
if(WIN32 AND _clay_engine_type STREQUAL "SHARED_LIBRARY")
  foreach(_consumer clay_package_consumer clay_cpp_package_consumer)
    add_custom_command(TARGET ${_consumer} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              $<TARGET_FILE:clay::clay_engine> $<TARGET_FILE_DIR:${_consumer}>
      VERBATIM)
  endforeach()
endif()
