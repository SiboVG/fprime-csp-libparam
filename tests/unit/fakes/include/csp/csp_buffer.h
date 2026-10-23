#pragma once

#include <csp/csp_types.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

csp_packet_t* csp_buffer_get(size_t unused);
void csp_buffer_free(void* buffer);

#ifdef __cplusplus
}
#endif
