set(SECURE_PACKAGE_DIR ${PROJECT_3RDPARTY_SRC_DIR}/secure/libboundscheck)
set(BUILD_DIR          ${PROJECT_BUILD_PATH}/secure_build)
set(SECURE_SRC_PATH    ${BUILD_DIR}/libboundscheck-v1.1.16)
set(SECURE_INSTALL_DIR ${PROJECT_3RDPARTY_BIN_DIR}/libboundscheck)

message(${SECURE_INSTALL_DIR}/lib/)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

set(SECURE_STATIC_LIB ${SECURE_INSTALL_DIR}/lib/libboundscheck.a)
if(NOT EXISTS ${SECURE_STATIC_LIB})
    file(REMOVE_RECURSE ${SECURE_SRC_PATH})
    file(MAKE_DIRECTORY ${BUILD_DIR} ${SECURE_INSTALL_DIR}/lib ${SECURE_INSTALL_DIR}/include)
    execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xzf ${SECURE_PACKAGE_DIR}/libboundscheck-v1.1.16.tar.gz
            WORKING_DIRECTORY ${BUILD_DIR}
            RESULT_VARIABLE extract_result
    )
    if(NOT extract_result EQUAL 0)
        message(FATAL_ERROR "libboundscheck source extraction failed")
    endif()

    execute_process(
            COMMAND make -C ${SECURE_SRC_PATH} -j4
            RESULT_VARIABLE build_result
    )
    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR "libboundscheck build failed")
    endif()

    file(COPY ${SECURE_SRC_PATH}/lib/ DESTINATION ${SECURE_INSTALL_DIR}/lib)
    file(COPY ${SECURE_SRC_PATH}/include/ DESTINATION ${SECURE_INSTALL_DIR}/include)
    file(GLOB SECURE_OBJECTS ${SECURE_SRC_PATH}/obj/*.o)
    if(NOT SECURE_OBJECTS)
        message(FATAL_ERROR "No libboundscheck object files were generated")
    endif()
    execute_process(
            COMMAND ${CMAKE_AR} crs ${SECURE_STATIC_LIB} ${SECURE_OBJECTS}
            RESULT_VARIABLE archive_result
    )
    if(NOT archive_result EQUAL 0)
        message(FATAL_ERROR "Failed to create static libboundscheck archive")
    endif()
else()
    message("-- libboundscheck: has been built, ignored")
endif()
