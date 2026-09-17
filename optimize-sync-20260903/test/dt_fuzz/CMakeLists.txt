set(DEPEND_LIBS
        -Wl,--start-group
        pthread
        dl
        gcov
        bss_common
        bss_compress
        bss_executor
        bss_fresh_table
        slice_table
        bss_memory
        bss_db
        bss_kv_table
        snapshot
        lsm_store
        ${LIB_3RD_SECURE}
        ${LIB_3RD_LZ4}
        aio
        ${LIB_3RD_GTEST}
        ${LIB_3RD_GMOCK}
        ${LIB_3RD_FUZZ}
)
if (NOT DEFINED ${PROJECT_3RDPARTY_BIN_DIR})
    SET(PROJECT_3RDPARTY_BIN_DIR ${PROJECT_OUTPUT_PATH}/3rdparty)
endif(NOT DEFINED ${PROJECT_3RDPARTY_BIN_DIR})

list(APPEND DEPEND_LIBS -Wl,--end-group)


file(GLOB_RECURSE FUZZ_SOURCE_FILES *.cpp *.h testcase/*)
add_executable(bss_fuzz ${FUZZ_SOURCE_FILES})

add_compile_options(-fsanitize=leak -fsanitize=address -fno-omit-frame-pointer -fsanitize-recover=address -fprofile-arcs -ftest-coverage -fsanitize-coverage=trace-pc)
include_directories(
        ${PROJECT_3RDPARTY_BIN_DIR}/secodefuzz/include
)
link_libraries(asan)

target_compile_options(bss_fuzz PUBLIC -D_GNU_SOURCE -g)
target_compile_definitions(bss_fuzz PRIVATE BUILD_PATH="${OCK_FUZZ_PROJECT_BINARY}")

set_target_properties(bss_fuzz PROPERTIES OUTPUT_NAME "bss_fuzz")
set_target_properties(bss_fuzz PROPERTIES CLEAN_DIRECT_OUTPUT 1)

target_link_libraries(bss_fuzz ${DEPEND_LIBS})
target_link_directories(bss_fuzz PRIVATE ${LIB_3RD_FUZZ})
target_include_directories(bss_fuzz PRIVATE testcase/)
#target_include_directories(bss_fuzz PRIVATE testcase/util)
#target_include_directories(bss_fuzz PRIVATE testcase/kv_store)
#target_include_directories(bss_fuzz PRIVATE testcase/kv_helper)

SET(LIBZK_SE_CODE_FUZZ        ${PROJECT_3RDPARTY_BIN_DIR}/fuzz/secodefuzz/lib/libSecodefuzz.a)