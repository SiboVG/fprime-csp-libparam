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

## CSP over UDP (specified, not yet implemented)

A UDP transport is planned so that a Zephyr target (the TE0715) and a Linux host can
carry CSP over Ethernet. libcsp csh/2.10's own UDP interface cannot be used: the
Zephyr module does not build it, and it creates its socket with protocol `PF_PACKET`,
which works on Linux only because that value equals `IPPROTO_UDP` there. It also
ignores bind errors and cannot be closed. The transport below keeps its wire format,
so the same traffic interoperates with csh's `csp add udp` (default port 9220). These
requirements are stated before the implementation. Their unit tests are written first,
from the requirements and the libcsp interface contract, and named in the Verification
column once they exist.

| ID | Requirement | Implementation | Verification evidence | Status |
|---|---|---|---|---|
| `FCL-UDP-001` | The UDP transport shall carry each CSP packet as one UDP datagram: the 6-byte CSP v2 header in network byte order (priority 2 bits, destination 14, source 14, destination port 6, source port 6, flags 6), followed by the packet data, with nothing else added. | Planned UDP transport | Unit tests against fakes, with header bytes computed by hand from the field layout; interoperability with csh on the TE0715 bench | To do: not implemented |
| `FCL-UDP-002` | A received datagram shall be dropped and counted as an interface receive error, without reaching the router, if it is shorter than the CSP v2 header, longer than the header plus the CSP buffer's data size, or sent from an address other than the configured peer. | Planned UDP transport | Unit tests against fakes: 0, 5, 6 bytes; the largest valid size and one byte more; another sender | To do: not implemented |
| `FCL-UDP-003` | The transport shall send each packet to the configured peer address and port. On success it shall release the packet; on a send failure it shall return an error and leave the packet to libcsp, which releases it and counts a transmit error. | Planned UDP transport | Unit tests against fakes with an injected send failure: the packet is released exactly once | To do: not implemented |
| `FCL-UDP-004` | Opening shall reject, before any socket or CSP interface is created, a peer that is not a unicast IPv4 address, a local or remote port of 0, and an interface name longer than libcsp's CSP_IFLIST_NAME_MAX (10) characters. | Planned UDP transport | Unit tests against fakes: boundary and invalid configurations, no socket or interface created | To do: not implemented |
| `FCL-UDP-005` | If the socket cannot be created or bound, opening shall fail with the error, leaving no socket open and no CSP interface registered. | Planned UDP transport | Unit tests against fakes with injected socket and bind failures | To do: not implemented |
| `FCL-UDP-006` | Closing shall unregister the CSP interface before it stops receiving, then stop the receive task and close the socket within one receive timeout of at most 1 s; no datagram received after close shall reach the router, and a failed phase shall be retryable without repeating a completed one, as for SocketCAN (`FCL-CAN-001`). | Planned UDP transport | Unit tests against fakes: close order, injected close failures and retry | To do: not implemented |
| `FCL-UDP-007` | The same transport shall build for Linux and for the fprime-zephyr platform, behind a CMake option that is off by default, as the CAN transports are. | Planned UDP transport, `library.cmake` | Linux smoke-consumer build with the option on; the TE0715 image build | To do: not implemented |

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
