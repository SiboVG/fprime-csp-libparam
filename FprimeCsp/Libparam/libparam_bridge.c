#include "libparam_bridge.h"

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
