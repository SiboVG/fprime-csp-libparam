#pragma once

#include <stdint.h>
#include <csp/csp_types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PARAM_TYPE_UINT8,
    PARAM_TYPE_UINT16,
    PARAM_TYPE_UINT32,
    PARAM_TYPE_UINT64,
    PARAM_TYPE_INT8,
    PARAM_TYPE_INT16,
    PARAM_TYPE_INT32,
    PARAM_TYPE_INT64,
    PARAM_TYPE_XINT8,
    PARAM_TYPE_XINT16,
    PARAM_TYPE_XINT32,
    PARAM_TYPE_XINT64,
    PARAM_TYPE_FLOAT,
    PARAM_TYPE_DOUBLE,
    PARAM_TYPE_STRING,
    PARAM_TYPE_DATA,
    PARAM_TYPE_INVALID,
} param_type_e;

struct vmem_s;
typedef struct param_s {
    uint16_t id;
    uint16_t* node;
    param_type_e type;
    uint32_t mask;
    char* name;
    char* unit;
    char* docstr;
    void* addr;
    uint64_t vaddr;
    struct vmem_s* vmem;
    int array_size;
    int array_step;
    void (*callback)(struct param_s*, int);
    csp_timestamp_t* timestamp;
    struct {
        struct param_s* sle_next;
    } next;
} param_t;

uint32_t param_get_uint32(param_t* parameter);
uint8_t param_get_uint8_array(param_t* parameter, unsigned int index);
uint16_t param_get_uint16_array(param_t* parameter, unsigned int index);
uint32_t param_get_uint32_array(param_t* parameter, unsigned int index);
int8_t param_get_int8_array(param_t* parameter, unsigned int index);
int16_t param_get_int16_array(param_t* parameter, unsigned int index);
int32_t param_get_int32_array(param_t* parameter, unsigned int index);

#ifdef __cplusplus
}
#endif
