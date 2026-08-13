#include "libparam_bridge.h"

#include <limits.h>
#include <stddef.h>

#include <param/param_client.h>
#include <param/param_list.h>
#include <param/param_server.h>

_Static_assert(FPRIMECSP_PARAM_SERVER_PORT == PARAM_PORT_SERVER,
               "F Prime CSP library port must match Space Inventor libparam");

int fprimecsp_param_list_add(param_t* parameter) {
    if (parameter == NULL) {
        return -1;
    }
    return param_list_add(parameter);
}

void fprimecsp_param_serve(csp_packet_t* packet) {
    if (packet != NULL) {
        param_serve(packet);
    }
}

int fprimecsp_param_pull_u32(param_t* parameter,
                             const uint16_t peer_node,
                             const uint32_t timeout_ms,
                             const int protocol_version,
                             uint32_t* value) {
    if ((parameter == NULL) || (value == NULL)) {
        return -1;
    }
    const int status = param_pull_single(parameter,
                                         -1,
                                         CSP_PRIO_NORM,
                                         0,
                                         peer_node,
                                         (int)timeout_ms,
                                         protocol_version);
    if (status == 0) {
        *value = param_get_uint32(parameter);
    }
    return status;
}

int fprimecsp_param_push_u32(param_t* parameter,
                             const uint16_t peer_node,
                             const uint32_t timeout_ms,
                             const int protocol_version,
                             const uint32_t value) {
    if (parameter == NULL) {
        return -1;
    }
    uint32_t mutable_value = value;
    return param_push_single(parameter,
                             -1,
                             CSP_PRIO_NORM,
                             &mutable_value,
                             0,
                             peer_node,
                             (int)timeout_ms,
                             protocol_version,
                             true);
}

static int integer_type_size(const param_type_e type) {
    switch (type) {
        case PARAM_TYPE_UINT8:
        case PARAM_TYPE_INT8:
            return 1;
        case PARAM_TYPE_UINT16:
        case PARAM_TYPE_INT16:
            return 2;
        case PARAM_TYPE_UINT32:
        case PARAM_TYPE_INT32:
            return 4;
        default:
            return 0;
    }
}

static size_t integer_type_alignment(const param_type_e type) {
    switch (type) {
        case PARAM_TYPE_UINT8:
            return _Alignof(uint8_t);
        case PARAM_TYPE_INT8:
            return _Alignof(int8_t);
        case PARAM_TYPE_UINT16:
            return _Alignof(uint16_t);
        case PARAM_TYPE_INT16:
            return _Alignof(int16_t);
        case PARAM_TYPE_UINT32:
            return _Alignof(uint32_t);
        case PARAM_TYPE_INT32:
            return _Alignof(int32_t);
        default:
            return 0;
    }
}

static int validate_integer_parameter(const param_t* parameter,
                                      const uint16_t peer_node,
                                      const uint32_t timeout_ms,
                                      const uint8_t index) {
    if ((parameter == NULL) || (parameter->node == NULL) ||
        (parameter->timestamp == NULL) ||
        ((parameter->addr == NULL) && (parameter->vmem == NULL)) ||
        (*parameter->node != peer_node) || (timeout_ms > INT_MAX)) {
        return -1;
    }
    const int type_size = integer_type_size(parameter->type);
    if (type_size == 0) {
        return -2;
    }
    if ((parameter->array_size <= 0) || ((int)index >= parameter->array_size) ||
        (index > INT8_MAX)) {
        return -3;
    }
    if (parameter->array_step < type_size) {
        return -1;
    }
    if (parameter->addr != NULL) {
        const size_t type_alignment = integer_type_alignment(parameter->type);
        if ((type_alignment == 0U) ||
            (((uintptr_t)parameter->addr % type_alignment) != 0U) ||
            (((size_t)parameter->array_step % type_alignment) != 0U)) {
            return -1;
        }
    }
    return 0;
}

int fprimecsp_param_pull_integer(param_t* parameter,
                                 const uint16_t peer_node,
                                 const uint32_t timeout_ms,
                                 const int protocol_version,
                                 const uint8_t index,
                                 int64_t* value) {
    if (value == NULL) {
        return -1;
    }
    const int validation = validate_integer_parameter(parameter, peer_node, timeout_ms, index);
    if (validation != 0) {
        return validation;
    }
    const int status = param_pull_single(parameter,
                                         (int)index,
                                         CSP_PRIO_NORM,
                                         0,
                                         peer_node,
                                         (int)timeout_ms,
                                         protocol_version);
    if (status != 0) {
        return status;
    }

    switch (parameter->type) {
        case PARAM_TYPE_UINT8:
            *value = param_get_uint8_array(parameter, index);
            break;
        case PARAM_TYPE_UINT16:
            *value = param_get_uint16_array(parameter, index);
            break;
        case PARAM_TYPE_UINT32:
            *value = param_get_uint32_array(parameter, index);
            break;
        case PARAM_TYPE_INT8:
            *value = param_get_int8_array(parameter, index);
            break;
        case PARAM_TYPE_INT16:
            *value = param_get_int16_array(parameter, index);
            break;
        case PARAM_TYPE_INT32:
            *value = param_get_int32_array(parameter, index);
            break;
        default:
            return -2;
    }
    return 0;
}

int fprimecsp_param_push_integer(param_t* parameter,
                                 const uint16_t peer_node,
                                 const uint32_t timeout_ms,
                                 const int protocol_version,
                                 const uint8_t index,
                                 const int64_t value) {
    const int validation = validate_integer_parameter(parameter, peer_node, timeout_ms, index);
    if (validation != 0) {
        return validation;
    }
#define PUSH_INTEGER(c_type, minimum, maximum)                                   \
    do {                                                                         \
        if ((value < (minimum)) || (value > (maximum))) {                        \
            return -4;                                                           \
        }                                                                        \
        c_type typed_value = (c_type)value;                                      \
        return param_push_single(parameter,                                      \
                                 (int)index,                                      \
                                 CSP_PRIO_NORM,                                   \
                                 &typed_value,                                    \
                                 0,                                               \
                                 peer_node,                                       \
                                 (int)timeout_ms,                                 \
                                 protocol_version,                                \
                                 true);                                           \
    } while (0)

    switch (parameter->type) {
        case PARAM_TYPE_UINT8:
            PUSH_INTEGER(uint8_t, 0, UINT8_MAX);
        case PARAM_TYPE_UINT16:
            PUSH_INTEGER(uint16_t, 0, UINT16_MAX);
        case PARAM_TYPE_UINT32:
            PUSH_INTEGER(uint32_t, 0, UINT32_MAX);
        case PARAM_TYPE_INT8:
            PUSH_INTEGER(int8_t, INT8_MIN, INT8_MAX);
        case PARAM_TYPE_INT16:
            PUSH_INTEGER(int16_t, INT16_MIN, INT16_MAX);
        case PARAM_TYPE_INT32:
            PUSH_INTEGER(int32_t, INT32_MIN, INT32_MAX);
        default:
            return -2;
    }

#undef PUSH_INTEGER
}
