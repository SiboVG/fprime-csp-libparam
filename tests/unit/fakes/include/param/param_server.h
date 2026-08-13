#pragma once

#include <csp/csp_types.h>

#define PARAM_PORT_SERVER 10

#ifdef __cplusplus
extern "C" {
#endif

void param_serve(csp_packet_t* packet);

#ifdef __cplusplus
}
#endif
