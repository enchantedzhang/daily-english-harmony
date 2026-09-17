file(GLOB_RECURSE BSS_KV_TABLE_SRCS *.cpp *.h)
add_library(bss_kv_table STATIC ${BSS_KV_TABLE_SRCS})