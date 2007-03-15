#ifndef __VECTOR
#define __VECTOR

#include "common.h"

extern "C++" {
#include <iostream>
  
  const double VEQUALITY = 0.0000001;

  template<int LEN>
  struct vec_internal;
  
  template<int LEN>
  class dvec;

  template<int LEN>
  std::ostream& operator<<(std::ostream& out, const dvec<LEN>& v);

  class dvec_op {
  public:   
    virtual double operator()(double a, double b) const = 0;
  };
  
  template<int LEN>
  class dvec {
  public:
    dvec(double s);
    dvec(const double* vals, bool aligned=true);
    dvec(const dvec<LEN>& p);
    
    dvec<LEN>& operator=(const dvec<LEN>& p);
    double operator[](int index) const;
    void u_store(double* arr) const;
    void a_store(double* arr) const;
    
    bool operator==(const dvec<LEN>& b) const;

    dvec<LEN> operator+(const dvec<LEN>& b);
    dvec<LEN> operator-(const dvec<LEN>& b);
    dvec<LEN> operator*(const dvec<LEN>& b);
    dvec<LEN> operator/(const dvec<LEN>& b);
    
    dvec<LEN> madd(const dvec<LEN>& s, const dvec<LEN>& b);
    dvec<LEN> madd(const double s, const dvec<LEN>& b);

    double fold(double proto, const dvec_op& operation, int limit = LEN);

    friend std::ostream& operator<< <LEN>(std::ostream& out, const dvec<LEN>& v);   

    class mul : public dvec_op {
    public: 
      double operator()(double a, double b) const { return a * b; }
    };
  private:
    struct vec_internal<LEN> data;

    dvec(const vec_internal<LEN>& d);
  };  

  // use this to create 16-byte aligned memory on platforms that support it
#define VEC_ALIGN

  /*#undef __SSE2__*/ // Test FPU version
#if defined(__SSE2__) && defined(__GNUC__) && defined(HAVE_EMMINTRIN_H)
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
