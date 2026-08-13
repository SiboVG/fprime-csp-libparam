#pragma once

#include <stdint.h>

#include <csp/csp_types.h>
#include <param/param.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FPRIMECSP_PARAM_SERVER_PORT = 10,
    FPRIMECSP_PARAM_PROTOCOL_VERSION = 2,
};

int fprimecsp_param_list_add(param_t* parameter);
void fprimecsp_param_serve(csp_packet_t* packet);
int fprimecsp_param_pull_u32(param_t* parameter,
                             uint16_t peer_node,
                             uint32_t timeout_ms,
                             int protocol_version,
                             uint32_t* value);
int fprimecsp_param_push_u32(param_t* parameter,
                             uint16_t peer_node,
                             uint32_t timeout_ms,
                             int protocol_version,
                             uint32_t value);
int fprimecsp_param_pull_integer(param_t* parameter,
                                 uint16_t peer_node,
                                 uint32_t timeout_ms,
                                 int protocol_version,
                                 uint8_t index,
                                 int64_t* value);
int fprimecsp_param_push_integer(param_t* parameter,
                                 uint16_t peer_node,
                                 uint32_t timeout_ms,
                                 int protocol_version,
                                 uint8_t index,
                                 int64_t value);

#ifdef __cplusplus
}
#endif
