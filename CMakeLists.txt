cmake_minimum_required(VERSION 3.1.1)

project(hydraViewer)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/usd_cmake/defaults
                      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/usd_cmake/modules
                      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/usd_cmake/macros
                      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

include(Options)
include(ProjectDefaults)

include(CXXDefaults)
add_definitions(${_PXR_CXX_DEFINITIONS})
set(CMAKE_CXX_FLAGS "${_PXR_CXX_FLAGS} ${CMAKE_CXX_FLAGS}")

include(Public)
include(Packages)

set(CMAKE_CXX_STANDARD 11)

find_package(USD REQUIRED)
link_directories(${USD_LIBRARY_DIR})

add_subdirectory(pxr/rprImaging/rprEngine)
