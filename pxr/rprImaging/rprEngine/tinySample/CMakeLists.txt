add_executable(tinySample
    tinySample.cpp
    renderTask.cpp
    renderTask.h)
target_link_libraries(tinySample PRIVATE
    rprEngine)

install(TARGETS tinySample)