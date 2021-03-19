#ifndef SC_STRTOULL_H
#define SC_STRTOULL_H

#include <limits.h>

#ifdef _WIN32
#  define strtoull _strtoui64
#  define ULLONG_MAX _UI64_MAX
#endif

#endif /* SC_STRTOULL_H */
