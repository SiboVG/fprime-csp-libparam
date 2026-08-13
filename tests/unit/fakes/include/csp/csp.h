#pragma once

#include <csp/csp_error.h>
#include <csp/csp_types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum csp_dedup_types {
    CSP_DEDUP_OFF,
    CSP_DEDUP_FWD,
    CSP_DEDUP_INCOMING,
    CSP_DEDUP_ALL,
};

typedef struct csp_conf_s {
    uint8_t version;
    const char* hostname;
    const char* model;
    const char* revision;
    uint32_t conn_dfl_so;
    uint8_t dedup;
} csp_conf_t;

extern csp_conf_t csp_conf;
void csp_init(void);
int csp_bind_callback(csp_callback_t callback, uint8_t port);
int csp_route_work(void);
int csp_ping(uint16_t node, uint32_t timeout, unsigned int size, uint8_t opts);
void csp_service_handler(csp_packet_t* packet);

#ifdef __cplusplus
}
#endif
