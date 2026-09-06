register_fprime_library(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Libparam.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/libparam_bridge.c"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/Libparam.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/libparam_bridge.h"
    DEPENDS
        FprimeCsp_Runtime
)

set_target_properties(
    ${FPRIME_CURRENT_MODULE}
    PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
)
target_compile_features(${FPRIME_CURRENT_MODULE} PUBLIC c_std_11 cxx_std_17)
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    # F Prime's default configuration supplies -std=c99 as a raw flag. Place
    # the module's stronger requirement later so the C11 bridge is portable.
    target_compile_options(${FPRIME_CURRENT_MODULE} PRIVATE "$<$<COMPILE_LANGUAGE:C>:-std=c11>")
endif()
target_include_directories(${FPRIME_CURRENT_MODULE} PUBLIC "${FPRIMECSP_LIBRARY_ROOT}")
target_link_libraries(${FPRIME_CURRENT_MODULE} PUBLIC ${FPRIMECSP_LIBPARAM_C_TARGET})
fprimecsp_enable_strict_warnings(${FPRIME_CURRENT_MODULE})
