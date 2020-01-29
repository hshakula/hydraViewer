set(PXR_PREFIX rprEngine)
set(PXR_PACKAGE rprEngine)

if(${USD_LIBRARY_MONOLITHIC})
    set(USD_LIBRARIES usd_ms)
else()
    set(USD_LIBRARIES
        hd
        hf
        hdx
        gf
        tf
        trace
        sdf
        usd
        usdImaging
        )
endif()

pxr_library(rprEngine
    LIBRARIES
        ${USD_LIBRARIES}
        ${TBB_tbb_LIBRARY}
        ${Boost_LIBRARIES}
        ${PYTHON_LIBRARY}

    INCLUDE_DIRS
        ${USD_INCLUDE_DIR}
        ${TBB_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}

    PUBLIC_CLASSES
        engine

    PUBLIC_HEADERS
        api.h
        version.h
        renderParams.h
)

pxr_build_test(testHdRprEngine
    LIBRARIES
        rprEngine
        usd
    CPPFILES
        test/testHdRprEngine.cpp
)

add_subdirectory(tinySample)
