file(GLOB_RECURSE SLICE_TABLE_SRCS *.cpp *.h)
add_library(slice_table STATIC ${SLICE_TABLE_SRCS})