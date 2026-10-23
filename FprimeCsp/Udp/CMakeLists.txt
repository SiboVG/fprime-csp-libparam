if(NOT FPRIMECSP_ENABLE_UDP)
    return()
endif()

register_fprime_library(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/UdpTransport.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/UdpSockets.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/UdpTransport.hpp"
    DEPENDS
        FprimeCsp_Runtime
        csp
)

target_compile_features(${FPRIME_CURRENT_MODULE} PUBLIC cxx_std_17)
target_include_directories(${FPRIME_CURRENT_MODULE} PUBLIC "${FPRIMECSP_LIBRARY_ROOT}")
# Lets consumers choose this transport at compile time
target_compile_definitions(${FPRIME_CURRENT_MODULE} PUBLIC FPRIMECSP_HAVE_UDP=1)
fprimecsp_enable_strict_warnings(${FPRIME_CURRENT_MODULE})
