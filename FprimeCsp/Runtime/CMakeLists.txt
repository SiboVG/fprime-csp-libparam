register_fprime_library(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Stack.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/Stack.hpp"
    DEPENDS
        Os
        csp
        Threads::Threads
)

target_compile_features(${FPRIME_CURRENT_MODULE} PUBLIC cxx_std_17)
target_include_directories(${FPRIME_CURRENT_MODULE} PUBLIC "${FPRIMECSP_LIBRARY_ROOT}")
