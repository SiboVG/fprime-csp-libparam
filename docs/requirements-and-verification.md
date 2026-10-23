# Requirements and verification

This matrix is the release traceability record for the reusable integration
layer. “Verified in CI” means that the named automated evidence exists and is
run by the repository workflow. It does not imply hardware qualification.

| ID | Requirement | Implementation | Verification evidence | Status |
|---|---|---|---|---|
| `FCL-ARCH-001` | The library shall allow only one owner of process-global CSP state at a time. | `Stack::initialize` process claim | `StackTests`: second-owner and lifecycle scenarios; repository contract | Verified in CI |
| `FCL-RUN-001` | Stack addressing and transport configuration shall be validated before process-global state is claimed. | `Stack::initialize` | `invalid_config_is_rejected_before_csp_init`; `test_addressing_is_validated_before_global_state_is_claimed` | Verified in CI |
| `FCL-RUN-002` | Router start, stop request, join, and shutdown shall be explicit application-managed phases. | `Stack` lifecycle API | `router_runs_stops_and_joins`; runtime smoke consumer | Verified in CI |
| `FCL-RUN-003` | Shutdown shall not close or release the transport unless router join succeeds, and a close failure shall remain retryable. | `ShutdownStatus`, `Stack::shutdown` | `shutdown_defers_transport_close_until_router_join_succeeds`; `shutdown_retries_transport_close_failure` | Verified in CI |
| `FCL-CSP-001` | Ping shall use the configured peer and CSP CRC32 option and reject calls before initialization. | `Stack::ping` | `ping_uses_crc32_and_requires_initialization` | Verified in CI |
| `FCL-CSP-002` | A service callback shall bind to the requested CSP port and preserve upstream errors. | `Stack::bindCallback` | `callback_binding_forwards_and_preserves_errors`; `service_handler_binding_is_idempotent` | Verified in CI |
| `FCL-CAN-001` | SocketCAN shutdown shall unregister the CSP interface before stopping its driver; a failed phase shall be retryable without repeating a completed phase. | `SocketCanTransport::close` two-phase state | `close_removes_then_stops_interface`; `remove_failure_defers_driver_stop_until_retry`; `stop_failure_keeps_interface_for_retry` | Verified in CI |
| `FCL-PAR-001` | Typed client operations shall validate protocol version, descriptor node, storage, alignment, array bounds, type, and C-API timeout conversion before I/O. | `libparam_bridge.c` | `integer_operations_reject_*`; `bridge_rejects_invalid_protocol_versions` | Verified in CI |
| `FCL-PAR-002` | Typed integer pull and push shall support indexed 8-, 16-, and 32-bit signed and unsigned descriptors without changing output on failed reads. | `Client::pullInteger`, `Client::pushInteger` | typed forwarding, range, failure-preservation, and `vcan` type-matrix tests | Verified in CI and Linux `vcan` |
| `FCL-LEG-001` | Legacy scalar `pullU32` and `pushU32` shall accept only safe scalar `UINT32` descriptors and representable timeouts. | legacy bridge entry points | `legacy_u32_operations_reject_unsafe_descriptors_and_timeouts_before_io`, including non-scalar rejection before I/O | Host verified; CI rerun required |
| `FCL-INT-001` | A standalone F Prime consumer shall discover the manifest and build the runtime modules against pinned compatible dependencies. | `library.cmake`, smoke consumer | repository contracts and `run_runtime_smoke.sh` | Verified in CI |
| `FCL-HIL-001` | The SocketCAN adapter shall exchange CSP/libparam traffic over a supported physical CAN interface without resource or teardown faults. | `SocketCanTransport` and consuming application | Consumer Kvaser loopback procedure and retained as-run record | **Pending hardware execution** |

## CSP over UDP

A UDP transport is planned so that a Zephyr target (the TE0715) and a Linux host can
carry CSP over Ethernet. libcsp csh/2.10's own UDP interface cannot be used: the
Zephyr module does not build it, and it creates its socket with protocol `PF_PACKET`,
which works on Linux only because that value equals `IPPROTO_UDP` there. It also
ignores bind errors and cannot be closed. The transport below keeps its wire format,
so the same traffic interoperates with csh's `csp add udp` (default port 9220). The
requirements were stated before the implementation and the tests written from them first.
The unit tests run the transport against fakes of libcsp and of the socket layer, so
failures can be injected; the loopback tests run the platform's sockets themselves.
Coverage of `FprimeCsp/Udp/`: 100% of the lines of `UdpSockets.cpp` and all of
`UdpTransport.cpp` but its destructor's `std::terminate()` path, reached only if closing
fails in the destructor (as for the CAN transports, justified by analysis), and the
default virtual destructor of `UdpSocketApi`, never called through the interface.

| ID | Requirement | Implementation | Verification evidence | Status |
|---|---|---|---|---|
| `FCL-UDP-001` | The UDP transport shall carry each CSP packet as one UDP datagram: the 6-byte CSP v2 header in network byte order (priority 2 bits, destination 14, source 14, destination port 6, source port 6, flags 6), followed by the packet data, with nothing else added; a received datagram in this format shall reach the router as the CSP packet it carries. | `UdpTransport`, `UdpSockets.cpp` | Unit: `udp001_send_is_header_and_data_only`, `udp001_empty_packet_is_the_header_alone`, `udp001_received_datagram_reaches_the_router`; loopback: `udp_socket_datagram_arrives_with_its_sender`. Interoperability with the real libcsp and csh on the TE0715 bench | Partial: host verified; interoperability on hardware pending |
| `FCL-UDP-002` | A received datagram shall be dropped and counted as an interface receive error, without reaching the router, if it is shorter than the CSP v2 header, longer than the header plus the CSP buffer's data size, or sent from an address other than the configured peer; a datagram for which no CSP packet buffer is free shall be dropped and counted as a drop; a failed receive on the socket shall be recorded and shall not stop the transport from receiving. | `UdpTransport` | Unit: `udp002_empty_datagram_is_rejected`, `udp002_datagram_one_byte_shorter_than_the_header_is_rejected`, `udp002_boundary_sizes_are_delivered`, `udp002_datagram_one_byte_longer_than_the_largest_packet_is_rejected`, `udp002_much_longer_datagram_is_rejected`, `udp002_datagram_from_another_sender_is_rejected`, `udp002_header_the_stack_refuses_is_rejected`, `udp002_datagram_without_a_free_buffer_is_dropped`, `udp002_receive_error_does_not_stop_receiving`; loopback: `udp_socket_longer_datagram_is_cut_to_the_buffer` | Host verified; CI rerun required |
| `FCL-UDP-003` | The transport shall send each packet to the configured peer address and port. On success it shall release the packet; on a send failure it shall return an error and leave the packet to libcsp, which releases it and counts a transmit error. | `UdpTransport` | Unit: `udp003_send_goes_to_the_peer_address_and_port`, `udp003_sent_packet_is_released_once`, `udp003_send_error_leaves_the_packet_to_libcsp`, `udp003_partial_send_is_a_failure`; loopback: `udp_socket_send_on_an_invalid_socket_fails` | Host verified; CI rerun required |
| `FCL-UDP-004` | Opening shall reject, before any socket or CSP interface is created, a peer that is not a unicast IPv4 address, a local or remote port of 0, and an interface name that is empty, longer than libcsp's CSP_IFLIST_NAME_MAX (10) characters, or already registered with libcsp (whose interface list silently ignores a duplicate name). | `UdpTransport` (`UdpTransport::validate`) | Unit: `udp004_peer_that_is_not_an_address_is_invalid`, `udp004_peer_that_is_not_unicast_is_invalid`, `udp004_zero_ports_are_invalid`, `udp004_interface_names_out_of_bounds_are_invalid`, `udp004_registered_interface_name_is_invalid`, `udp004_valid_boundaries_are_accepted`, `udp004_open_refuses_an_invalid_peer_before_any_socket`, `udp004_open_refuses_a_registered_name_before_any_socket`, `udp004_boundary_configuration_is_accepted`, `udp004_open_registers_the_configured_interface` | Host verified; CI rerun required |
| `FCL-UDP-005` | If the socket cannot be created or bound, opening shall fail with the error, leaving no socket open and no CSP interface registered. | `UdpTransport`, `UdpSockets.cpp` | Unit: `udp005_socket_failure_leaves_no_interface`, `udp005_task_failure_closes_the_socket`, `udp005_task_failure_with_a_failing_socket_close_is_reported`; loopback: `udp_socket_bound_port_cannot_be_opened_twice`, `udp_socket_open_failure_reports_the_errno` | Host verified; CI rerun required |
| `FCL-UDP-006` | Closing shall unregister the CSP interface before it stops receiving, then stop the receive task and close the socket within one receive timeout of at most 1 s; no datagram received after close shall reach the router, and a failed phase shall be retryable without repeating a completed one, as for SocketCAN (`FCL-CAN-001`). | `UdpTransport`, `UdpSockets.cpp` | Unit: `udp006_close_unregisters_then_stops_receiving_then_closes_the_socket`, `udp006_close_is_bounded_by_the_receive_timeout`, `udp006_nothing_reaches_the_router_after_close`, `udp006_failed_task_join_is_retried_without_unregistering_twice`, `udp006_failed_socket_close_is_retried_without_stopping_twice`; loopback: `udp_socket_receive_waits_at_most_the_timeout`, `udp_socket_close_reports_an_invalid_socket` | Host verified; CI rerun required |
| `FCL-UDP-007` | The same transport shall build for Linux and for the fprime-zephyr platform, behind a CMake option that is off by default, as the CAN transports are. | `FprimeCsp/Udp/CMakeLists.txt`, `library.cmake`, `cmake/Dependencies.cmake` | Repository contract `test_udp_is_an_opt_in_module_built_by_the_smoke_consumer`; Linux smoke-consumer build (`run_runtime_smoke.sh`, builds `FprimeCsp_Udp`); the TE0715 image build | Partial: Linux smoke build verified; Zephyr build pending |

## Verification levels

- Unit tests isolate libcsp, libparam, SocketCAN, and task boundaries with
  deterministic fakes.
- Sanitizers exercise the same host suite for memory and undefined behavior.
- Coverage and static-analysis jobs are enforced independently of the tests;
  the static-analysis gate runs pinned cppcheck and clang-tidy over the
  production sources and fails on any finding.
- The downstream Linux `vcan` consumer verifies the real pinned protocol
  libraries without physical hardware.
- `FCL-HIL-001` remains open until an operator runs the physical procedure,
  identifies the test assets, and retains the resulting logs and checksums.

See [Verification evidence](verification-evidence.md) for the distinction
between automated evidence and an as-run hardware record.
