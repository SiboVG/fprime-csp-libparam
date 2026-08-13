#pragma once

#include <csp/csp_types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*nexthop_t)(csp_iface_t*, uint16_t, csp_packet_t*, int);
typedef int (*csp_alias_add_t)(void*, uint16_t);

struct csp_iface_s {
    uint16_t addr;
    uint16_t netmask;
    const char* name;
    void* interface_data;
    void* driver_data;
    nexthop_t nexthop;
    csp_alias_add_t add_alias;
    uint8_t is_default;
    uint32_t tx;
    uint32_t rx;
    uint32_t tx_error;
    uint32_t rx_error;
    uint32_t drop;
    uint32_t autherr;
    uint32_t frame;
    uint32_t txbytes;
    uint32_t rxbytes;
    uint32_t irq;
    struct csp_iface_s* next;
};

#ifdef __cplusplus
}
#endif
