file(GLOB_RECURSE BSS_MEMORY *.cpp *.h)
add_library(bss_memory STATIC ${BSS_MEMORY})