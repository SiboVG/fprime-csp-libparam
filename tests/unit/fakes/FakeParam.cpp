#include "TestDoubles.hpp"

namespace TestDoubles {
namespace {
ParamState state;
}

ParamState& paramState() {
    return state;
}

void resetParam() {
    state = {};
}
}  // namespace TestDoubles

extern "C" {

int param_list_add(param_t* parameter) {
    auto& state = TestDoubles::paramState();
    ++state.listAddCalls;
    state.listParameter = parameter;
    return state.listAddStatus;
}

void param_serve(csp_packet_t* packet) {
    auto& state = TestDoubles::paramState();
    ++state.serveCalls;
    state.servedPacket = packet;
}

int param_pull_single(param_t* parameter,
                      int offset,
                      int priority,
                      int verbose,
                      int host,
                      int timeout,
                      int version) {
    auto& state = TestDoubles::paramState();
    ++state.pullCalls;
    state.pullParameter = parameter;
    state.pullOffset = offset;
    state.pullPriority = priority;
    state.pullVerbose = verbose;
    state.pullHost = host;
    state.pullTimeout = timeout;
    state.pullVersion = version;
    return state.pullStatus;
}

std::uint32_t param_get_uint32(param_t*) {
    auto& state = TestDoubles::paramState();
    ++state.getU32Calls;
    return state.getU32Value;
}

int param_push_single(param_t* parameter,
                      int offset,
                      int priority,
                      void* value,
                      int verbose,
                      int host,
                      int timeout,
                      int version,
                      bool ackWithPull) {
    auto& state = TestDoubles::paramState();
    ++state.pushCalls;
    state.pushParameter = parameter;
    state.pushOffset = offset;
    state.pushPriority = priority;
    state.pushVerbose = verbose;
    state.pushHost = host;
    state.pushTimeout = timeout;
    state.pushVersion = version;
    state.pushAckWithPull = ackWithPull;
    state.pushedValue = value == nullptr ? 0U : *static_cast<std::uint32_t*>(value);
    return state.pushStatus;
}

}  // extern "C"
