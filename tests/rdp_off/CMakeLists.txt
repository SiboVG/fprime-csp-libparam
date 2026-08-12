cmake_minimum_required(VERSION 3.24.2)
project(FprimeCspRdpOffContract LANGUAGES C CXX)

set(CSP_USE_RDP OFF CACHE BOOL "Exercise the rejected shutdown configuration")
add_library(csp INTERFACE)
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/Dependencies.cmake")
