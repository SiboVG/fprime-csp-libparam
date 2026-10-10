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
