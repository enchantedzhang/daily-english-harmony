#build spdlog
set(SOURCE_DIR ${PROJECT_3RDPARTY_SRC_DIR}/spdlog/spdlog)
set(BUILD_DIR ${PROJECT_BUILD_PATH}/spdlog)
set(INSTALL_DIR ${PROJECT_3RDPARTY_BIN_DIR}/spdlog)

message("-- spdlog: check if need to build ")

# build the lib if not built yet
find_file(LIBS_BUILT_SPDLOG libspdlog.a ${INSTALL_DIR}/lib64/ ${INSTALL_DIR}/lib)
if(NOT EXISTS ${LIBS_BUILT_SPDLOG})

    # create build dir
    exec_program(mkdir ${BUILD_PATH} ARGS -p ${BUILD_DIR})

    # configure
    exec_program(cmake ${BUILD_DIR} ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ${SOURCE_DIR})

    # execute make && make install
    exec_program(make ${BUILD_DIR} ARGS clean)
    exec_program(make ${BUILD_DIR} ARGS -j8)
    exec_program("make install" ${BUILD_DIR})

else()
    message("-- spdlog: ${LIBS_BUILT_SPDLOG}")
    message("-- spdlog: has been built, ignored")
endif(NOT EXISTS ${LIBS_BUILT_SPDLOG})
