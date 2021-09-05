add_library(clocks_vm)

target_include_directories(
  clocks_vm
  PUBLIC ${clocks_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_sources(clocks_vm PUBLIC chunk.c memory.c debug.c value.c vm.c scanner.c)

add_executable(clocks_repl)
set_target_properties(clocks_repl PROPERTIES OUTPUT_NAME "clocks")

target_include_directories(
  clocks_repl
  PUBLIC ${clocks_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_sources(clocks_repl PRIVATE main.c)

target_link_libraries(clocks_repl PRIVATE clocks_vm)
