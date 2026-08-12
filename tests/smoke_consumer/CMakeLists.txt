cmake_minimum_required(VERSION 3.24.2)
project(FprimeCspRuntimeSmoke LANGUAGES C CXX)

set(SMOKE_FPRIME_FRAMEWORK_PATH "" CACHE PATH "Caller-supplied F Prime source tree")
set(SMOKE_LIBCSP_SOURCE "" CACHE PATH "Caller-supplied libcsp csh/2.10 source tree")

if(NOT EXISTS "${SMOKE_FPRIME_FRAMEWORK_PATH}/cmake/FPrime.cmake")
    message(FATAL_ERROR
        "Set SMOKE_FPRIME_FRAMEWORK_PATH to an F Prime source tree")
endif()
if(NOT EXISTS "${SMOKE_LIBCSP_SOURCE}/include/csp/csp.h")
    message(FATAL_ERROR
        "Set SMOKE_LIBCSP_SOURCE to a libcsp csh/2.10 source tree")
endif()

get_filename_component(
    FPRIMECSP_LIBRARY_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE
)
if(NOT EXISTS "${FPRIMECSP_LIBRARY_ROOT}/library.cmake")
    message(FATAL_ERROR "The smoke consumer cannot find the library.cmake manifest")
endif()

set(FPRIME_FRAMEWORK_PATH "${SMOKE_FPRIME_FRAMEWORK_PATH}" CACHE PATH "" FORCE)
set(FPRIME_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "" FORCE)
set(FPRIME_LIBRARY_LOCATIONS "${FPRIMECSP_LIBRARY_ROOT}" CACHE STRING "" FORCE)
set(FPRIMECSP_ENABLE_SOCKETCAN OFF CACHE BOOL "" FORCE)
set(CSP_USE_RDP ON CACHE BOOL "" FORCE)
set(FPRIME_ENABLE_FRAMEWORK_UTS OFF CACHE BOOL "" FORCE)
set(FPRIME_ENABLE_AUTOCODER_UTS OFF CACHE BOOL "" FORCE)

# This smoke test compiles only the Runtime wrapper. Its interface targets
# provide the dependency headers without building either third-party library.
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/csp_autoconfig.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/csp/autoconfig.h"
    @ONLY
)
add_library(csp INTERFACE)
target_include_directories(
    csp
    INTERFACE
        "${CMAKE_CURRENT_BINARY_DIR}/include"
        "${SMOKE_LIBCSP_SOURCE}/include"
)
add_library(fprimecsp_libparam_c INTERFACE)
target_link_libraries(fprimecsp_libparam_c INTERFACE csp)

include("${FPRIME_FRAMEWORK_PATH}/cmake/FPrime.cmake")
fprime_setup_included_code()

if(NOT TARGET FprimeCsp_Runtime)
    message(FATAL_ERROR
        "F Prime did not discover FprimeCsp_Runtime through library.cmake")
endif()
