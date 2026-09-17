file(GLOB_RECURSE BLOB_STORE_SRCS *.cpp *.h)
add_library(blob_store STATIC ${BLOB_STORE_SRCS})