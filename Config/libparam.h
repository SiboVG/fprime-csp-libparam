#ifndef FPRIMECSP_CONFIG_LIBPARAM_H
#define FPRIMECSP_CONFIG_LIBPARAM_H

/* Linux dynamic-list profile for libparam csh/2.10.
 * Embedded consumers must supply and qualify a target-specific profile. */
#define PARAM_HAVE_SYS_QUEUE 1
#define PARAM_LIST_DYNAMIC 1
#define PARAM_LIST_POOL 0
#define PARAM_VMEM_64_BITS_API 1
#define PARAM_NUM_PUBLISHQUEUES 0

#endif /* FPRIMECSP_CONFIG_LIBPARAM_H */
