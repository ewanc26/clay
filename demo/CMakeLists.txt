add_executable(clay_player
  main.cpp
  garden.cpp
)
target_link_libraries(clay_player PRIVATE clay_engine)
target_include_directories(clay_player PRIVATE ${PROJECT_SOURCE_DIR}/src/engine)

target_compile_definitions(clay_player PRIVATE
  CLAY_PLAYER_VERSION="0.1.0"
  $<$<BOOL:${CLAY_HAS_GLFW}>:CLAY_PLAYER_INTERACTIVE=1>)