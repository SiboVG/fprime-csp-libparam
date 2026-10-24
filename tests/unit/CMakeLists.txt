cmake_minimum_required(VERSION 3.20)
project(FprimeCspHostUnitTests LANGUAGES C CXX)

include(CTest)
find_package(Threads REQUIRED)

option(FPRIMECSP_ENABLE_COVERAGE "Instrument test targets for gcov coverage" OFF)

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
    if(FPRIMECSP_ENABLE_COVERAGE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target} PRIVATE --coverage)
            target_link_options(${target} PRIVATE --coverage)
        else()
            message(FATAL_ERROR "FPRIMECSP_ENABLE_COVERAGE requires Clang or GCC")
        endif()
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

add_executable(udp_tests
    UdpTests.cpp
    "${FPRIMECSP_ROOT}/FprimeCsp/Runtime/Stack.cpp"
    "${FPRIMECSP_ROOT}/FprimeCsp/Udp/UdpTransport.cpp"
)
fprimecsp_test_options(udp_tests)
target_link_libraries(udp_tests PRIVATE test_csp_fakes test_os_fakes)

# The platform's sockets themselves, over the loopback interface (POSIX hosts)
add_executable(udp_socket_tests
    UdpSocketTests.cpp
    "${FPRIMECSP_ROOT}/FprimeCsp/Udp/UdpSockets.cpp"
)
fprimecsp_test_options(udp_socket_tests)
target_link_libraries(udp_socket_tests PRIVATE test_os_fakes)
target_include_directories(udp_socket_tests PRIVATE "${TEST_INCLUDE}")

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
    shutdown_defers_transport_close_until_router_join_succeeds
    shutdown_retries_transport_close_failure
    ping_uses_crc32_and_requires_initialization
)

add_scenarios(socketcan_tests
    open_applies_socketcan_configuration
    empty_interface_name_uses_libcsp_default
    driver_failure_is_preserved
    success_without_interface_is_rejected
    close_removes_then_stops_interface
    stop_failure_keeps_interface_for_retry
    remove_failure_defers_driver_stop_until_retry
    empty_device_is_rejected_before_driver_call
)

add_scenarios(libparam_tests
    registry_forwards_descriptor_and_status
    server_binds_standard_callback_once
    pull_u32_forwards_request_and_reads_on_success
    pull_u32_preserves_output_on_failure
    push_u32_forwards_value_and_ack_policy
    legacy_u32_operations_reject_unsafe_descriptors_and_timeouts_before_io
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
    bridge_rejects_invalid_protocol_versions
    bridge_serves_non_null_packets
)

add_scenarios(udp_tests
    udp001_send_is_header_and_data_only
    udp001_empty_packet_is_the_header_alone
    udp001_received_datagram_reaches_the_router
    udp002_empty_datagram_is_rejected
    udp002_datagram_one_byte_shorter_than_the_header_is_rejected
    udp002_datagram_one_byte_longer_than_the_largest_packet_is_rejected
    udp002_much_longer_datagram_is_rejected
    udp002_datagram_from_another_sender_is_rejected
    udp002_header_the_stack_refuses_is_rejected
    udp002_boundary_sizes_are_delivered
    udp002_datagram_without_a_free_buffer_is_dropped
    udp002_receive_error_does_not_stop_receiving
    udp003_send_goes_to_the_peer_address_and_port
    udp003_sent_packet_is_released_once
    udp003_send_error_leaves_the_packet_to_libcsp
    udp003_partial_send_is_a_failure
    udp004_peer_that_is_not_an_address_is_invalid
    udp004_peer_that_is_not_unicast_is_invalid
    udp004_zero_ports_are_invalid
    udp004_interface_names_out_of_bounds_are_invalid
    udp004_registered_interface_name_is_invalid
    udp004_valid_boundaries_are_accepted
    udp004_open_refuses_an_invalid_peer_before_any_socket
    udp004_open_refuses_a_registered_name_before_any_socket
    udp004_boundary_configuration_is_accepted
    udp004_open_registers_the_configured_interface
    udp005_socket_failure_leaves_no_interface
    udp005_task_failure_closes_the_socket
    udp005_task_failure_with_a_failing_socket_close_is_reported
    udp006_close_unregisters_then_stops_receiving_then_closes_the_socket
    udp006_close_is_bounded_by_the_receive_timeout
    udp006_nothing_reaches_the_router_after_close
    udp006_failed_task_join_is_retried_without_unregistering_twice
    udp006_failed_socket_close_is_retried_without_stopping_twice
)

add_scenarios(udp_socket_tests
    udp_socket_datagram_arrives_with_its_sender
    udp_socket_receive_waits_at_most_the_timeout
    udp_socket_bound_port_cannot_be_opened_twice
    udp_socket_longer_datagram_is_cut_to_the_buffer
    udp_socket_close_reports_an_invalid_socket
    udp_socket_send_on_an_invalid_socket_fails
    udp_socket_open_failure_reports_the_errno
)
