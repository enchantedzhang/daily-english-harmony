add_subdirectory(secure)
add_subdirectory(spdlog)
add_subdirectory(lz4)

if (NOT ${CMAKE_BUILD_TYPE} STREQUAL "Release")
    add_subdirectory(googletest)
endif (NOT ${CMAKE_BUILD_TYPE} STREQUAL "Release")