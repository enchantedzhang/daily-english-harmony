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
        blob_store
        lsm_store
        ${LIB_3RD_SECURE}
        ${LIB_3RD_LZ4}
        aio
        ${LIB_3RD_GTEST}
        ${LIB_3RD_GMOCK}
)

list(APPEND DEPEND_LIBS -Wl,--end-group)


file(GLOB_RECURSE UT_SOURCE_FILES *.cpp *.h testcase/*)
add_executable(bss_ut ${UT_SOURCE_FILES})

add_compile_options(-fsanitize=leak -fsanitize=address -fno-omit-frame-pointer -fsanitize-recover=address)
link_libraries(asan)

target_compile_options(bss_ut PUBLIC -D_GNU_SOURCE -g)
target_compile_definitions(bss_ut PRIVATE BUILD_PATH="${OCK_TEST_PROJECT_BINARY}")

set_target_properties(bss_ut PROPERTIES OUTPUT_NAME "bss_ut")
set_target_properties(bss_ut PROPERTIES CLEAN_DIRECT_OUTPUT 1)

target_link_libraries(bss_ut ${DEPEND_LIBS})
target_include_directories(bss_ut PRIVATE testcase/)
target_include_directories(bss_ut PRIVATE testcase/util)
target_include_directories(bss_ut PRIVATE testcase/kv_store)
target_include_directories(bss_ut PRIVATE testcase/kv_helper)
add_definitions(-DFLAG_FOR_UT)