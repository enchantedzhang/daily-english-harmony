file(GLOB_RECURSE BSS_COMPRESS_SRCS *.cpp *.h)
add_library(bss_compress STATIC ${BSS_COMPRESS_SRCS})