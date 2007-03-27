#ifndef __VECTOR
#define __VECTOR

#include "common.h"
#include <math.h>

extern "C++" {
#include <iostream>
  
  const double VEQUALITY = 0.0000001;

  template<int LEN>
  struct vec_internal;
  
  template<int LEN>
  class dvec;

  template<int LEN>
  std::ostream& operator<<(std::ostream& out, const dvec<LEN>& v);

  class dvec_unop {
  public:
    virtual double operator()(double a) const = 0;
  };

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

    dvec<LEN> map(const dvec_unop& operation, int limit = LEN);
    double foldr(double proto, const dvec_op& operation, int limit = LEN);
    double foldl(double proto, const dvec_op& operation, int limit = LEN);

    friend std::ostream& operator<< <LEN>(std::ostream& out, const dvec<LEN>& v);   

    class mul : public dvec_op {
    public: 
      double operator()(double a, double b) const { return a * b; }
    };

    class add : public dvec_op {
    public:
      double operator()(double a, double b) const { return a + b; }
    };

    class sub : public dvec_op {
    public:
      double operator()(double a, double b) const { return a - b; }
    };

    class sqrt : public dvec_unop {
    public:
      double operator()(double a) const { return sqrt(a); }
    };
  private:
    struct vec_internal<LEN> data;

    dvec(const vec_internal<LEN>& d);
  };  

  //#define DVEC4(V,t,a,b,c,d) double v#t[4] VEC_ALIGN = {(a),(b),(c),(d)}; V(v#t)
  
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
