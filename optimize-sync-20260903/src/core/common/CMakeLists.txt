file(GLOB_RECURSE BSS_COMMON_SRCS *.cpp *.h)
add_library(bss_common STATIC ${BSS_COMMON_SRCS})
