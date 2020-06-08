cmake_minimum_required(VERSION 3.1.1)

project(hydraViewer)

find_package(pxr REQUIRED)

if(MSVC)
    STRING(REPLACE "/O2" "/Od" CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} CACHE STRING "compile flags" FORCE)

    set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD 1)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(pxr/rprImaging/rprEngine)
