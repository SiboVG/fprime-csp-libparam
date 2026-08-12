include_guard(GLOBAL)

get_filename_component(
    FPRIMECSP_LIBRARY_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/.."
    ABSOLUTE
)

set(
    FPRIMECSP_LIBCSP_SOURCE
    "${FPRIMECSP_LIBRARY_ROOT}/lib/libcsp"
    CACHE PATH
    "Path to the Space Inventor libcsp csh/2.10 source tree"
)
set(
    FPRIMECSP_LIBPARAM_SOURCE
    "${FPRIMECSP_LIBRARY_ROOT}/lib/libparam"
    CACHE PATH
    "Path to the Space Inventor libparam csh/2.10 source tree"
)
set(
    FPRIMECSP_LIBPARAM_CONFIG_DIR
    "${FPRIMECSP_LIBRARY_ROOT}/Config"
    CACHE PATH
    "Directory containing the target-qualified generated libparam.h"
)
option(
    FPRIMECSP_ENABLE_SOCKETCAN
    "Build the Linux SocketCAN transport"
    OFF
)

set(FPRIMECSP_LIBCSP_COMMIT "5a95e5c48826c3602bf4a34aa5d60e2af9e4a004")
set(FPRIMECSP_LIBPARAM_COMMIT "d78da56e7cbc8b6be30d8890b6f01cdf80d0b9b9")

# Stack::requestStop() relies on csp_route_work() waking periodically. In this
# libcsp release that bounded wait is compiled only when RDP support is on.
set(FPRIMECSP_REQUIRE_BOUNDED_ROUTE_WORK ON)

function(fprimecsp_verify_revision source_dir expected_commit dependency_name)
    if(NOT EXISTS "${source_dir}/.git")
        return()
    endif()

    find_package(Git REQUIRED)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${source_dir}" rev-parse HEAD
        RESULT_VARIABLE revision_status
        OUTPUT_VARIABLE actual_commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT revision_status EQUAL 0 OR NOT actual_commit STREQUAL expected_commit)
        message(FATAL_ERROR
            "${dependency_name} is not at the tested csh/2.10 revision. "
            "Expected ${expected_commit}, found '${actual_commit}'.")
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${source_dir}" status --porcelain --untracked-files=normal
        RESULT_VARIABLE worktree_status
        OUTPUT_VARIABLE worktree_changes
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT worktree_status EQUAL 0 OR NOT worktree_changes STREQUAL "")
        message(FATAL_ERROR
            "${dependency_name} has local changes. "
            "Pinned-source builds require a clean checkout.")
    endif()
endfunction()

find_package(Threads REQUIRED)

if(TARGET csp)
    if(NOT DEFINED CSP_USE_RDP)
        message(FATAL_ERROR
            "A pre-existing 'csp' target must declare CSP_USE_RDP=ON. "
            "The FprimeCsp router cannot guarantee bounded shutdown otherwise.")
    elseif(NOT CSP_USE_RDP)
        message(FATAL_ERROR
            "CSP_USE_RDP must be ON for bounded FprimeCsp router shutdown.")
    endif()
elseif(DEFINED CSP_USE_RDP AND NOT CSP_USE_RDP)
    message(FATAL_ERROR
        "CSP_USE_RDP must be ON for bounded FprimeCsp router shutdown.")
endif()

if(FPRIMECSP_ENABLE_SOCKETCAN)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "FPRIMECSP_ENABLE_SOCKETCAN requires a Linux target")
    endif()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSOCKETCAN REQUIRED libsocketcan)
endif()

if(NOT TARGET csp)
    if(NOT EXISTS "${FPRIMECSP_LIBCSP_SOURCE}/CMakeLists.txt")
        message(FATAL_ERROR
            "libcsp is missing at ${FPRIMECSP_LIBCSP_SOURCE}. "
            "Run fprime-csp-libparam/scripts/bootstrap_dependencies.sh or set "
            "FPRIMECSP_LIBCSP_SOURCE before F Prime setup.")
    endif()

    fprimecsp_verify_revision(
        "${FPRIMECSP_LIBCSP_SOURCE}"
        "${FPRIMECSP_LIBCSP_COMMIT}"
        "Space Inventor libcsp"
    )

    set(CSP_BUFFER_SIZE 512 CACHE STRING "CSP packet buffer size")
    set(CSP_BUFFER_COUNT 64 CACHE STRING "CSP packet buffer count")
    set(CSP_QFIFO_LEN 64 CACHE STRING "CSP router queue length")
    set(CSP_CONN_RXQUEUE_LEN 32 CACHE STRING "CSP connection receive queue length")
    set(CSP_USE_RDP ON CACHE BOOL "Keep route_work bounded for clean shutdown")
    set(CSP_USE_RTABLE OFF CACHE BOOL "Use subnet and default-interface routing")

    add_subdirectory(
        "${FPRIMECSP_LIBCSP_SOURCE}"
        "${CMAKE_BINARY_DIR}/fprime-csp-libparam/libcsp"
    )
endif()

if(NOT TARGET csp)
    message(FATAL_ERROR "libcsp did not provide the expected 'csp' target")
endif()
if(FPRIMECSP_REQUIRE_BOUNDED_ROUTE_WORK AND (NOT DEFINED CSP_USE_RDP OR NOT CSP_USE_RDP))
    message(FATAL_ERROR
        "CSP_USE_RDP must be ON for bounded FprimeCsp router shutdown.")
endif()
if(FPRIMECSP_ENABLE_SOCKETCAN AND NOT TARGET driver_can)
    message(FATAL_ERROR
        "libcsp did not enable its SocketCAN driver; install libsocketcan-dev")
endif()

set(FPRIMECSP_LIBPARAM_C_TARGET fprimecsp_libparam_c)
if(NOT TARGET ${FPRIMECSP_LIBPARAM_C_TARGET})
    if(NOT EXISTS "${FPRIMECSP_LIBPARAM_SOURCE}/meson.build")
        message(FATAL_ERROR
            "libparam is missing at ${FPRIMECSP_LIBPARAM_SOURCE}. "
            "Run fprime-csp-libparam/scripts/bootstrap_dependencies.sh or set "
            "FPRIMECSP_LIBPARAM_SOURCE before F Prime setup.")
    endif()
    if(NOT EXISTS "${FPRIMECSP_LIBPARAM_CONFIG_DIR}/libparam.h")
        message(FATAL_ERROR
            "libparam configuration is missing at "
            "${FPRIMECSP_LIBPARAM_CONFIG_DIR}/libparam.h. Set "
            "FPRIMECSP_LIBPARAM_CONFIG_DIR to a target-qualified profile.")
    endif()

    fprimecsp_verify_revision(
        "${FPRIMECSP_LIBPARAM_SOURCE}"
        "${FPRIMECSP_LIBPARAM_COMMIT}"
        "Space Inventor libparam"
    )

    # libparam csh/2.10 is Meson-only. This is the complete default Meson
    # source set, exposed as a regular CMake target for F Prime consumers.
    set(fprimecsp_libparam_sources
        "${FPRIMECSP_LIBPARAM_SOURCE}/src/param/list/param_list.c"
        "${FPRIMECSP_LIBPARAM_SOURCE}/src/param/param_client.c"
        "${FPRIMECSP_LIBPARAM_SOURCE}/src/param/param_serializer.c"
        "${FPRIMECSP_LIBPARAM_SOURCE}/src/param/param_server.c"
        "${FPRIMECSP_LIBPARAM_SOURCE}/src/param/param_string.c"
        "${FPRIMECSP_LIBPARAM_SOURCE}/src/param/param_queue.c"
        "${FPRIMECSP_LIBPARAM_SOURCE}/src/param/param_wildcard.c"
        "${FPRIMECSP_LIBPARAM_SOURCE}/src/param/param.c"
        "${FPRIMECSP_LIBPARAM_SOURCE}/src/mpack/mpack.c"
        "${FPRIMECSP_LIBPARAM_SOURCE}/src/vmem/vmem_client.c"
        "${FPRIMECSP_LIBPARAM_SOURCE}/src/vmem/vmem_crc32.c"
        "${FPRIMECSP_LIBPARAM_SOURCE}/src/vmem/vmem_server.c"
        "${FPRIMECSP_LIBPARAM_SOURCE}/src/vmem/vmem.c"
        "${FPRIMECSP_LIBPARAM_SOURCE}/src/vmem/vmem_block.c"
        "${FPRIMECSP_LIBPARAM_SOURCE}/src/objstore/objstore.c"
    )

    add_library(${FPRIMECSP_LIBPARAM_C_TARGET} STATIC ${fprimecsp_libparam_sources})
    set_target_properties(
        ${FPRIMECSP_LIBPARAM_C_TARGET}
        PROPERTIES
            C_STANDARD 11
            C_STANDARD_REQUIRED ON
            C_EXTENSIONS ON
    )
    target_include_directories(
        ${FPRIMECSP_LIBPARAM_C_TARGET}
        PUBLIC
            "${FPRIMECSP_LIBPARAM_CONFIG_DIR}"
            "${FPRIMECSP_LIBPARAM_SOURCE}/include"
            "${FPRIMECSP_LIBPARAM_SOURCE}"
        PRIVATE
            "${FPRIMECSP_LIBPARAM_SOURCE}/src"
            "${FPRIMECSP_LIBPARAM_SOURCE}/src/param/list"
    )
    target_compile_options(${FPRIMECSP_LIBPARAM_C_TARGET} PRIVATE -Wall -Wextra)
    target_link_libraries(${FPRIMECSP_LIBPARAM_C_TARGET} PUBLIC csp Threads::Threads)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(LIBBSD REQUIRED IMPORTED_TARGET libbsd-overlay)
        target_link_libraries(${FPRIMECSP_LIBPARAM_C_TARGET} PUBLIC PkgConfig::LIBBSD)
    endif()
endif()
