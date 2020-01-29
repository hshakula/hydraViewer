find_library(HDRPR_PLUGIN_LIB
    NAMES hdRpr
    PATHS "${USD_ROOT}/plugin/usd"
    NO_DEFAULT_PATH NO_SYSTEM_ENVIRONMENT_PATH
)

pxr_cpp_bin(tinySample
    LIBRARIES
        hd
        gf
        glf
        tf
        vt
        sdf
        trace
        usd
        usdImaging
        ${HDRPR_PLUGIN_LIB}
    INCLUDE_DIRS
        ${USD_INCLUDE_DIR}
        ${TBB_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS})

target_sources(tinySample PRIVATE
    renderTask.cpp
    renderTask.h)
