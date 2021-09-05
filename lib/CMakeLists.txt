add_library(linenoise linenoise/linenoise.h linenoise/linenoise.c)

target_include_directories(linenoise PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
