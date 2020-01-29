set(PXR_PREFIX viewer)
set(PXR_PACKAGE viewer)

if(${USD_LIBRARY_MONOLITHIC})
    set(USD_LIBRARIES usd_ms)
else()
    set(USD_LIBRARIES
        hd)
endif()

pxr_cpp_bin(viewer
    INCLUDE_DIRS
        ${USD_INCLUDE_DIR}
 
    LIBRARIES
        ${USD_LIBRARIES}
)

target_sources(viewer PRIVATE
    viewer.h
    viewer.cpp)
