cmake_minimum_required(VERSION 3.14.1)
project(boost-ss CXX C)
set(CMAKE_CXX_STANDARD 14)

option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_SVE "Build sve" OFF)

set(CMAKE_SKIP_RPATH TRUE)
#cmake -DCMAKE_BUILD_TYPE=Debug or Release or Blend
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "BUILD_TESTS = ${BUILD_TESTS}")
message(STATUS "BUILD_SVE = ${BUILD_SVE}")

set(CMAKE_AR "/usr/bin/gcc-ar")
set(CMAKE_RANLIB "/usr/bin/gcc-ranlib")

add_compile_options(-faligned-new)
add_compile_options(-fsigned-char)
add_compile_options(-fstack-protector-all)
add_compile_options(-fstack-protector-strong)
add_compile_options(-Wl,-z,relro,-z,now,-z,noexecstack)
add_compile_options(-fPIC)
add_compile_options(-Wall -fms-extensions)
add_compile_options(-Wno-unused-parameter -Wno-unused-function -Wunused-variable -Wunused-value -Wcast-align)
#add_compile_options(-Wcast-qual -Winvalid-pch -Wwrite-strings -Wsign-compare -Wfloat-equal -Wextra)
add_compile_options(-fno-omit-frame-pointer)
if (${CMAKE_BUILD_TYPE} MATCHES "Release")
    add_compile_options(-flto -flto-partition=none)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,relro,-z,now,-z,noexecstack -s -flto -flto-partition=none")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,relro,-z,now,-z,noexecstack -s -fPIE -flto -flto-partition=none")
    add_compile_options(-s)
    add_compile_options(-D_FORTIFY_SOURCE=2 -O2)
elseif (${CMAKE_BUILD_TYPE} MATCHES "Blend")
    add_compile_options(-flto -flto-partition=one)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,relro,-z,now,-z,noexecstack -flto -flto-partition=one")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,relro,-z,now,-z,noexecstack -fPIE -flto -flto-partition=one")
    add_compile_options(-g -rdynamic)
    add_compile_options(-D_FORTIFY_SOURCE=2 -O2)
else ()
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,relro,-z,now,-z,noexecstack")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,relro,-z,now,-z,noexecstack -fPIE")
    add_compile_options(-g -rdynamic)
endif ()


if (BUILD_SVE STREQUAL "ON")
    add_definitions(-DBUILD_SVE)
    # 禁用 LTO
    add_compile_options(-fno-lto)

    if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        # 检测编译器是否支持 SVE
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-march=armv8.5-a+sve" COMPILER_SUPPORTS_SVE)

        if (COMPILER_SUPPORTS_SVE)
            # 开启sve优化
            add_compile_options(-march=armv8.5-a+sve)
            message(STATUS "SVE support enabled")
        else ()
            message(WARNING "Compiler does not support SVE extensions")
        endif ()
    endif ()
endif ()

if (BUILD_TESTS STREQUAL "ON")
    add_compile_options(-fprofile-arcs -ftest-coverage)
    add_compile_definitions(DEBUG_UT)
endif ()

set(SRC_DIR ${PROJECT_SOURCE_DIR})
set(3RDPARTY_SRC_DIR ${PROJECT_SOURCE_DIR}/3rdparty)
set(OUTPUT_PATH ${PROJECT_SOURCE_DIR}/dist)
set(BUILD_PATH ${PROJECT_BINARY_DIR}/)

set(PROJECT_3RDPARTY_SRC_DIR ${PROJECT_SOURCE_DIR}/3rdparty)
set(PROJECT_BUILD_PATH ${PROJECT_SOURCE_DIR}/build)
set(PROJECT_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/dist)
set(PROJECT_3RDPARTY_BIN_DIR ${PROJECT_OUTPUT_PATH}/3rdparty)
set(ENV{3RDPARTY_BIN_DIR} ${PROJECT_3RDPARTY_BIN_DIR})

set(PROJECT_TOP_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
# set install target top dir
if (${CMAKE_INSTALL_PREFIX} MATCHES "/usr/local")
    set(TARGET_INSTALL_DIR ${OUTPUT_PATH}/boost-ss)
elseif (NOT CMAKE_INSTALL_PREFIX)
    set(TARGET_INSTALL_DIR ${OUTPUT_PATH}/boost-ss)
else ()
    set(TARGET_INSTALL_DIR ${OUTPUT_PATH}/boost-ss)
endif ()

message(STATUS "3rdparty src dir ${PROJECT_3RDPARTY_SRC_DIR}")
message(STATUS "3rdparty bin dir ${PROJECT_3RDPARTY_BIN_DIR}")
message(STATUS "target install dir ${TARGET_INSTALL_DIR}")

include_directories(
        ${SRC_DIR}/src/core
        ${SRC_DIR}/src/binary
		${SRC_DIR}/src/core/common
		${SRC_DIR}/src/core/compress
		${SRC_DIR}/src/core/db
        ${SRC_DIR}/src/core/blob_store
		${SRC_DIR}/src/core/executor
		${SRC_DIR}/src/core/fresh_table
        ${SRC_DIR}/src/core/include
		${SRC_DIR}/src/core/kv_table
		${SRC_DIR}/src/core/lsm_store
        ${SRC_DIR}/src/core/memory
		${SRC_DIR}/src/core/slice_table
        ${SRC_DIR}/src/core/snapshot
        ${PROJECT_3RDPARTY_BIN_DIR}/libboundscheck/include/
        ${PROJECT_3RDPARTY_BIN_DIR}/lz4/include/
        ${PROJECT_3RDPARTY_BIN_DIR}/spdlog/include
        ${PROJECT_3RDPARTY_BIN_DIR}/googletest/include
)
SET(JAVA_HOME $ENV{JAVA_HOME})

include_directories(${JAVA_HOME}/include)
include_directories(${JAVA_HOME}/include/linux)

link_directories(
        ${PROJECT_3RDPARTY_BIN_DIR}/libboundscheck/lib/
        ${PROJECT_3RDPARTY_BIN_DIR}/lz4/lib/
        ${PROJECT_3RDPARTY_BIN_DIR}/spdlog/lib/
        ${PROJECT_3RDPARTY_BIN_DIR}/googletest/lib/
)

SET(LIB_3RD_GTEST ${PROJECT_3RDPARTY_BIN_DIR}/googletest/lib/libgtest.a)
SET(LIB_3RD_SECURE ${PROJECT_3RDPARTY_BIN_DIR}/libboundscheck/lib/libboundscheck.a)
SET(LIB_3RD_LZ4 ${PROJECT_3RDPARTY_BIN_DIR}/lz4/lib/liblz4.a)

add_subdirectory(3rdparty)
add_subdirectory(src)
add_subdirectory(test)

add_custom_target(build_cpp
        COMMAND make -j8
        WORKING_DIRECTORY ${PROJECT_TOP_BUILD_DIR}
)

add_custom_target(build_version
        COMMAND make -j8
        COMMAND make build_bss_java_version_${FLINK_VERSION}
        COMMAND make build_bss_install_java_version_${FLINK_VERSION}

        WORKING_DIRECTORY ${PROJECT_TOP_BUILD_DIR}
)

add_custom_target(build_all
        COMMAND make -j8
        COMMAND make build_bss_java_version_1.16.3
        COMMAND make build_bss_install_java_version_1.16.3
        COMMAND make build_bss_java_version_1.17.1
        COMMAND make build_bss_install_java_version_1.17.1
        COMMAND make build_bss_java_version_1.16.1
        COMMAND make build_bss_install_java_version_1.16.1
        COMMAND make build_bss_java_version_1.20.0
        COMMAND make build_bss_install_java_version_1.20.0

        WORKING_DIRECTORY ${PROJECT_TOP_BUILD_DIR}
)
