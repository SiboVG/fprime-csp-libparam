#pragma once

#include <csp/csp.h>

#ifdef __cplusplus
extern "C" {
#endif

void csp_id_prepend(csp_packet_t* packet);
int csp_id_strip(csp_packet_t* packet);
int csp_id_setup_rx(csp_packet_t* packet);

#ifdef __cplusplus
}
#endif
