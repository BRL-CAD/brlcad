#ifndef __VECTOR
#define __VECTOR

extern "C++" {
#include <iostream>
  
  const double VEQUALITY = 0.0000001;

#if defined(__SSE2__) && defined(__GNUC__)
#define __x86_vector__
#include "vector_x86.h"
#else
#define __fpu_vector__
#include "vector_fpu.h"
#endif
  
  inline bool vequals(const vec2d& a, const vec2d& b) {
    return 
      (fabs(a.x()-b.x()) < VEQUALITY) &&
      (fabs(a.y()-b.y()) < VEQUALITY);
  }
}

#endif
