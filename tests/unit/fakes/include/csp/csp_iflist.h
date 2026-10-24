#pragma once

#include <csp/csp_interface.h>

#ifdef __cplusplus
extern "C" {
#endif

void csp_iflist_add(csp_iface_t* iface);
void csp_iflist_remove(csp_iface_t* ifc);
csp_iface_t* csp_iflist_get_by_name(const char* name);

#ifdef __cplusplus
}
#endif
