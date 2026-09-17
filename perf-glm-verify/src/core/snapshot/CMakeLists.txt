file(GLOB_RECURSE SNAPSHOT_SRCS *.cpp *.h)
add_library(snapshot STATIC ${SNAPSHOT_SRCS})