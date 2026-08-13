#pragma once

#include <stdbool.h>
#include <csp/interfaces/csp_if_can.h>

#ifdef __cplusplus
extern "C" {
#endif

int csp_can_socketcan_open_and_add_interface(const char* device,
                                             const char* ifname,
                                             unsigned int node_id,
                                             int bitrate,
                                             bool promisc,
                                             csp_iface_t** return_iface);
int csp_can_socketcan_stop(csp_iface_t* iface);

#ifdef __cplusplus
}
#endif
