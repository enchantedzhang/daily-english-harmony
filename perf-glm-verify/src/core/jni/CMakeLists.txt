set(DEPEND_LIBS
        -Wl,--start-group
        pthread
        dl
        bss_common
        bss_compress
        bss_executor
        bss_fresh_table
        slice_table
        bss_memory
        bss_db
        blob_store
        bss_kv_table
        snapshot
        lsm_store
        ${LIB_3RD_SECURE}
        ${LIB_3RD_LZ4}
)

list(APPEND DEPEND_LIBS -Wl,--end-group)

file(GLOB_RECURSE BSS_JNI_SRCS ./*.cpp ./*.h)
include_directories(${JAVA_HOME}/include)
include_directories(${JAVA_HOME}/include/linux)
add_library(ockdbjni-linux64 SHARED ${BSS_JNI_SRCS})
target_include_directories(ockdbjni-linux64 PRIVATE ./)

target_link_libraries(ockdbjni-linux64 PRIVATE ${DEPEND_LIBS})