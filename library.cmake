# Reusable F Prime integration for Space Inventor libcsp and libparam.

include("${CMAKE_CURRENT_LIST_DIR}/cmake/Dependencies.cmake")

function(fprimecsp_enable_strict_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    endif()
endfunction()

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FprimeCsp/Runtime")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FprimeCsp/SocketCan")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FprimeCsp/ZephyrCan")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FprimeCsp/Libparam")
