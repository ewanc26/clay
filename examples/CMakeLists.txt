add_executable(clay_host_c main.c)
set_target_properties(clay_host_c PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(clay_host_c PRIVATE clay_engine)
