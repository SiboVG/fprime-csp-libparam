cmake_minimum_required(VERSION 3.20)
project(FprimeCspHostUnitTests LANGUAGES C CXX)

include(CTest)
find_package(Threads REQUIRED)

set(FPRIMECSP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(TEST_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/fakes/include")

add_library(test_csp_fakes STATIC fakes/FakeCsp.cpp)
target_include_directories(test_csp_fakes PUBLIC "${TEST_INCLUDE}")
target_compile_features(test_csp_fakes PUBLIC cxx_std_17)

add_library(test_os_fakes INTERFACE)
target_include_directories(test_os_fakes INTERFACE "${TEST_INCLUDE}")
target_link_libraries(test_os_fakes INTERFACE Threads::Threads)

add_library(test_param_fakes STATIC fakes/FakeParam.cpp)
target_include_directories(test_param_fakes PUBLIC "${TEST_INCLUDE}")
target_compile_features(test_param_fakes PUBLIC cxx_std_17)

function(fprimecsp_test_options target)
    target_include_directories(${target} PRIVATE "${FPRIMECSP_ROOT}" "${TEST_INCLUDE}")
    target_compile_features(${target} PRIVATE cxx_std_17)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

add_executable(stack_tests
    StackTests.cpp
    "${FPRIMECSP_ROOT}/FprimeCsp/Runtime/Stack.cpp"
)
fprimecsp_test_options(stack_tests)
target_link_libraries(stack_tests PRIVATE test_csp_fakes test_os_fakes)

add_executable(socketcan_tests
    SocketCanTests.cpp
    "${FPRIMECSP_ROOT}/FprimeCsp/Runtime/Stack.cpp"
    "${FPRIMECSP_ROOT}/FprimeCsp/SocketCan/SocketCanTransport.cpp"
)
fprimecsp_test_options(socketcan_tests)
target_link_libraries(socketcan_tests PRIVATE test_csp_fakes test_os_fakes)

add_executable(libparam_tests
    LibparamTests.cpp
    "${FPRIMECSP_ROOT}/FprimeCsp/Runtime/Stack.cpp"
    "${FPRIMECSP_ROOT}/FprimeCsp/Libparam/Libparam.cpp"
    "${FPRIMECSP_ROOT}/FprimeCsp/Libparam/libparam_bridge.c"
)
fprimecsp_test_options(libparam_tests)
set_source_files_properties(
    "${FPRIMECSP_ROOT}/FprimeCsp/Libparam/libparam_bridge.c"
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic;-Werror"
)
target_link_libraries(libparam_tests PRIVATE test_csp_fakes test_param_fakes test_os_fakes)

function(add_scenarios executable)
    foreach(scenario IN LISTS ARGN)
        add_test(NAME "${executable}.${scenario}" COMMAND ${executable} "${scenario}")
    endforeach()
endfunction()

add_scenarios(stack_tests
    initialize_configures_csp_and_transport
    invalid_config_is_rejected_before_csp_init
    transport_error_is_preserved
    callback_binding_forwards_and_preserves_errors
    service_handler_binding_is_idempotent
    router_runs_stops_and_joins
    router_start_error_is_reported
    ping_uses_crc32_and_requires_initialization
)

add_scenarios(socketcan_tests
    open_applies_socketcan_configuration
    empty_interface_name_uses_libcsp_default
    driver_failure_is_preserved
    success_without_interface_is_rejected
    close_removes_then_stops_interface
    stop_failure_keeps_interface_for_retry
    empty_device_is_rejected_before_driver_call
)

add_scenarios(libparam_tests
    registry_forwards_descriptor_and_status
    server_binds_standard_callback_once
    pull_u32_forwards_request_and_reads_on_success
    pull_u32_preserves_output_on_failure
    push_u32_forwards_value_and_ack_policy
    pull_integer_forwards_indexed_typed_requests
    pull_integer_preserves_output_on_failure
    push_integer_forwards_indexed_typed_values
    integer_operations_reject_unsupported_types_before_io
    integer_operations_reject_invalid_indexes_before_io
    integer_operations_reject_invalid_descriptor_storage_before_io
    integer_operations_reject_misaligned_ram_base_before_io
    integer_operations_reject_misaligned_ram_stride_before_io
    integer_operations_allow_byte_addressed_vmem_descriptors
    integer_operations_require_descriptor_node_to_match_peer
    integer_operations_reject_timeouts_that_cannot_reach_the_c_api
    push_integer_rejects_out_of_range_values_before_io
    invalid_protocol_version_is_rejected
    bridge_guards_null_inputs
    bridge_serves_non_null_packets
)
