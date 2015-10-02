#ifndef MM_CPUCONFIG_H
#define MM_CPUCONFIG_H


#include "common.h"


#if defined(__x86_64__)
#define CPUCONF_ARCH_AMD64
#elif defined(__i386__)
#define CPUCONF_ARCH_IA32
#endif

/* #define CPUCONF_CACHE_LINE_SIZE (64) (optional)*/

#endif
