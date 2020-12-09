#ifndef NULLPTR_H
#define NULLPTR_H

#include <sc_cf.h>

#ifdef HAVE_NULLPTR
#include <cstddef>
#else
#  define nullptr_t void*
#  define nullptr NULL
#endif //HAVE_NULLPTR

#endif //NULLPTR_H
