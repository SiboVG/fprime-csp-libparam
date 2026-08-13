#pragma once

#include <stdbool.h>
#include <param/param.h>

#ifdef __cplusplus
extern "C" {
#endif

int param_pull_single(param_t* param, int offset, int prio, int verbose, int host, int timeout, int version);
int param_push_single(param_t* param,
                      int offset,
                      int prio,
                      void* value,
                      int verbose,
                      int host,
                      int timeout,
                      int version,
                      bool ack_with_pull);

#ifdef __cplusplus
}
#endif
