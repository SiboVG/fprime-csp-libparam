register_fprime_library(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Libparam.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/libparam_bridge.c"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/Libparam.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/libparam_bridge.h"
    DEPENDS
        FprimeCsp_Runtime
        ${FPRIMECSP_LIBPARAM_C_TARGET}
)

target_compile_features(${FPRIME_CURRENT_MODULE} PUBLIC cxx_std_17)
target_include_directories(${FPRIME_CURRENT_MODULE} PUBLIC "${FPRIMECSP_LIBRARY_ROOT}")
