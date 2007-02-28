#ifndef __VECTOR
#define __VECTOR

extern "C++" {
#include <iostream>
  
  const double VEQUALITY = 0.0000001;

  template<int LEN>
  class dvec {
  public:
    dvec(double s);
    dvec(const double* vals);
    dvec(const dvec<LEN>& p);
    
    dvec<LEN>& operator=(const dvec<LEN>& p);
    double operator[](int index) const;
    void u_store(double* arr) const;
    void a_store(double* arr) const;
    
    dvec<LEN> operator+(const dvec<LEN>& b);
    dvec<LEN> operator-(const dvec<LEN>& b);
    dvec<LEN> operator*(const dvec<LEN>& b);
    dvec<LEN> operator/(const dvec<LEN>& b);
    
    dvec<LEN> madd(const dvec<LEN>& s, const dvec<LEN>& b);
    dvec<LEN> madd(const double s, const dvec<LEN>& b);
  };


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
