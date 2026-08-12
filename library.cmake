# Reusable F Prime integration for Space Inventor libcsp and libparam.

include("${CMAKE_CURRENT_LIST_DIR}/cmake/Dependencies.cmake")

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FprimeCsp/Runtime")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FprimeCsp/SocketCan")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FprimeCsp/Libparam")
