file(GLOB_RECURSE LSM_STORE_SRCS *.cpp *.h)
add_library(lsm_store STATIC ${LSM_STORE_SRCS})