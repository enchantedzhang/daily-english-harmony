# 构建 LZ4
set(SOURCE_DIR ${PROJECT_3RDPARTY_SRC_DIR}/lz4/lz4)
set(INSTALL_DIR ${PROJECT_3RDPARTY_BIN_DIR}/lz4)

message("-- lz4: Check if a build is required ")

# 检查是否已经构建
find_file(LIBS_BUILT_LZ4 liblz4.a ${INSTALL_DIR}/lib/)
if(NOT LIBS_BUILT_LZ4)
    message("-- lz4: Build with Makefile")

    # 创建安装目录
    file(MAKE_DIRECTORY ${INSTALL_DIR}/lib ${INSTALL_DIR}/include)

    # 定义目标架构变量
    if(NOT DEFINED TARGET_ARCH)
        # 检测架构
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64|ARM64")
            set(TARGET_ARCH "arm")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|i[3-6]86|AMD64")
            set(TARGET_ARCH "x86")
        else()
            set(TARGET_ARCH "generic")
        endif()
    endif()

    # 使用 Makefile 构建
    if(TARGET_ARCH STREQUAL "arm")
        message(STATUS "Configuring for ARM architecture")
        execute_process(
                COMMAND make -C ${SOURCE_DIR} liblz4.a
                WORKING_DIRECTORY ${SOURCE_DIR}
                RESULT_VARIABLE build_result
                OUTPUT_VARIABLE build_output
                ERROR_VARIABLE build_error
        )
    else()
        message(STATUS "Configuring for X86 architecture")
        execute_process(
                COMMAND make -C ${SOURCE_DIR} CFLAGS="-fPIC" liblz4.a
                WORKING_DIRECTORY ${SOURCE_DIR}
                RESULT_VARIABLE build_result
                OUTPUT_VARIABLE build_output
                ERROR_VARIABLE build_error
        )
    endif()

    # 检查构建结果
    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR "LZ4 BUILD FAILED: \n${build_output}\n${build_error}")
    endif()

    # 安装库文件
    file(GLOB LZ4_LIBRARIES ${SOURCE_DIR}/lib/*.a)
    file(COPY ${LZ4_LIBRARIES} DESTINATION ${INSTALL_DIR}/lib)

    # 安装头文件
    file(GLOB LZ4_HEADERS ${SOURCE_DIR}/lib/*.h)
    file(COPY ${LZ4_HEADERS} DESTINATION ${INSTALL_DIR}/include)

    message("-- lz4: Build and installation is complete")
else()
    message("-- lz4: ${LIBS_BUILT_LZ4}")
    message("-- lz4: Built, skipped")
endif()