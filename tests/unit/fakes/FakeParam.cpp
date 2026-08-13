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

#define PARAM_GET_INTEGER_FAKE(c_type, name, param_type)                         \
    c_type param_get_##name##_array(param_t*, unsigned int index) {              \
        auto& state = TestDoubles::paramState();                                 \
        ++state.getIntegerCalls;                                                 \
        state.getIntegerType = param_type;                                       \
        state.getIntegerIndex = index;                                           \
        return static_cast<c_type>(state.getIntegerValue);                       \
    }

PARAM_GET_INTEGER_FAKE(std::uint8_t, uint8, PARAM_TYPE_UINT8)
PARAM_GET_INTEGER_FAKE(std::uint16_t, uint16, PARAM_TYPE_UINT16)
PARAM_GET_INTEGER_FAKE(std::uint32_t, uint32, PARAM_TYPE_UINT32)
PARAM_GET_INTEGER_FAKE(std::int8_t, int8, PARAM_TYPE_INT8)
PARAM_GET_INTEGER_FAKE(std::int16_t, int16, PARAM_TYPE_INT16)
PARAM_GET_INTEGER_FAKE(std::int32_t, int32, PARAM_TYPE_INT32)

#undef PARAM_GET_INTEGER_FAKE

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
    if (value != nullptr) {
        switch (parameter->type) {
            case PARAM_TYPE_UINT8:
                state.pushedIntegerValue = *static_cast<std::uint8_t*>(value);
                break;
            case PARAM_TYPE_UINT16:
                state.pushedIntegerValue = *static_cast<std::uint16_t*>(value);
                break;
            case PARAM_TYPE_UINT32:
                state.pushedIntegerValue = *static_cast<std::uint32_t*>(value);
                break;
            case PARAM_TYPE_INT8:
                state.pushedIntegerValue = *static_cast<std::int8_t*>(value);
                break;
            case PARAM_TYPE_INT16:
                state.pushedIntegerValue = *static_cast<std::int16_t*>(value);
                break;
            case PARAM_TYPE_INT32:
                state.pushedIntegerValue = *static_cast<std::int32_t*>(value);
                break;
            default:
                state.pushedIntegerValue = 0;
                break;
        }
        state.pushedValue = static_cast<std::uint32_t>(state.pushedIntegerValue);
    }
    return state.pushStatus;
}

}  // extern "C"
