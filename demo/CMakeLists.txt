add_executable(clay_player
  main.cpp
  garden.cpp
)
target_link_libraries(clay_player PRIVATE clay_engine)
target_include_directories(clay_player PRIVATE ${PROJECT_SOURCE_DIR}/src/engine)

if(WIN32 AND CLAY_BUILD_SHARED)
  add_custom_command(TARGET clay_player POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:clay_engine> $<TARGET_FILE_DIR:clay_player>
    VERBATIM)
endif()

target_compile_definitions(clay_player PRIVATE
  CLAY_PLAYER_VERSION="0.1.0"
  $<$<BOOL:${CLAY_HAS_GLFW}>:CLAY_PLAYER_INTERACTIVE=1>)
