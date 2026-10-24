#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CSP_ANY 255
#define CSP_PACKET_PADDING_BYTES 8
#define CSP_BUFFER_SIZE 256

typedef enum {
    CSP_PRIO_CRITICAL = 0,
    CSP_PRIO_HIGH = 1,
    CSP_PRIO_NORM = 2,
    CSP_PRIO_LOW = 3,
} csp_prio_t;

#define CSP_SO_NONE 0x0000
#define CSP_SO_CRC32REQ 0x0040
#define CSP_O_NONE CSP_SO_NONE
#define CSP_O_CRC32 CSP_SO_CRC32REQ

typedef struct {
    uint32_t tv_sec;
    uint32_t tv_nsec;
} csp_timestamp_t;

typedef struct {
    uint8_t pri;
    uint8_t flags;
    uint16_t src;
    uint16_t dst;
    uint8_t dport;
    uint8_t sport;
} csp_id_t;

/* The layout of libcsp csh/2.10 (layer 2 fields, length, id, header, data); __extension__
 * keeps -Wpedantic quiet about its anonymous structs, as libcsp is a system header in the build */
typedef struct csp_packet_s {
    union {
        __extension__ struct {
            uint32_t timestamp_tx;
            uint32_t timestamp_rx;
            struct csp_conn_s* conn;
        };
        __extension__ struct {
            uint16_t rx_count;
            uint16_t remain;
            uint32_t cfpid;
            uint32_t last_used;
            uint8_t* frame_begin;
            uint16_t frame_length;
        };
    };
    uint16_t length;
    csp_id_t id;
    struct csp_packet_s* next;
    uint8_t header[CSP_PACKET_PADDING_BYTES];
    union {
        uint8_t data[CSP_BUFFER_SIZE];
        uint16_t data16[CSP_BUFFER_SIZE / 2];
        uint32_t data32[CSP_BUFFER_SIZE / 4];
    };
} csp_packet_t;

typedef struct csp_iface_s csp_iface_t;
typedef void (*csp_callback_t)(csp_packet_t* packet);

#ifdef __cplusplus
}
#endif
