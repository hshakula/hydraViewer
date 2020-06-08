add_library(rprEngine SHARED
    api.h
    engine.h
    engine.cpp
    renderParams.h)
target_link_libraries(rprEngine PUBLIC
    hd
    hf
    hdx
    gf
    tf
    trace
    sdf
    usd
    usdImaging)

function(disable_warning target flag)
    if(MSVC)
        target_compile_options(${target} PUBLIC "/wd${flag}")
    else()
        target_compile_options(${target} PUBLIC "-Wno-${flag}")
    endif()
endfunction()

if(MSVC)
    # truncation from 'double' to 'float' due to matrix and vector classes in `Gf`
    target_compile_options(rprEngine PUBLIC "/wd4244")
    target_compile_options(rprEngine PUBLIC "/wd4305")

    target_compile_definitions(rprEngine PUBLIC "-DNOMINMAX")
endif()

target_compile_definitions(rprEngine PRIVATE "-DHDRPR_EXPORTS")

add_subdirectory(tinySample)

install(TARGETS rprEngine)
