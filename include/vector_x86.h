#ifndef __VECTOR_X86
#define __VECTOR_X86

#include "common.h"

#ifdef HAVE_EMMINTRIN_H
#  include <emmintrin.h>
#endif

//#define ALIGN16(_m) (double*)((((long)(_m)) + 0x10L) & ~0xFL);
#undef VEC_ALIGN
#define VEC_ALIGN __attribute__((aligned(16)))

typedef double v2df __attribute__((vector_size(16)));

template<int LEN>
struct vec_internal {
  v2df v[LEN/2];
};

// inline dvec4::dvec4(double a, double b, double c, double d)
//     : dvec<4>(
// {
//     double t[4] VEC_ALIGN = {a, b, c, d};
    
// }

template<int LEN>
inline dvec<LEN>::dvec(double s)
{
  double t[LEN] VEC_ALIGN;
  for (int i = 0; i < LEN/2; i++) {
    t[i*2]   = s;
    t[i*2+1] = s;
    data.v[i] = _mm_load_pd(&t[i*2]);
  }
}

template<int LEN>
inline dvec<LEN>::dvec(const double* vals, bool aligned)
{
  if (aligned) {
    for (int i = 0; i < LEN/2; i++) {
      data.v[i] = _mm_load_pd(&vals[i*2]);
    }
  } else {
    for (int i = 0; i < LEN/2; i++) {
      data.v[i] = _mm_loadu_pd(&vals[i*2]);
    }
  }
}

template<int LEN>
inline dvec<LEN>::dvec(const dvec<LEN>& p)
{
  for (int i = 0; i < LEN/2; i++) {
    data.v[i] = p.data.v[i];
  }
}

template<int LEN>
inline dvec<LEN>::dvec(const vec_internal<LEN>& d)
{
  for (int i = 0; i < LEN/2; i++) data.v[i] = d.v[i];
}

template<int LEN>
inline dvec<LEN>& 
dvec<LEN>::operator=(const dvec<LEN>& p)
{
  for (int i = 0; i < LEN/2; i++) {
    data.v[i] = p.data.v[i];
  }
  return *this;
}

template<int LEN>
inline double 
dvec<LEN>::operator[](const int index) const
{
  double t[2] __attribute__((aligned(16)));
  _mm_store_pd(t, data.v[index/2]);
  return t[index%2];
}

template<int LEN>
inline void 
dvec<LEN>::u_store(double* arr) const
{
  for (int i = 0; i < LEN/2; i++) {
    _mm_storeu_pd(&arr[i*2], data.v[i]);
  }
}

template<int LEN>
inline void 
dvec<LEN>::a_store(double* arr) const
{
  for (int i = 0; i < LEN/2; i++) {
    _mm_store_pd(&arr[i*2], data.v[i]);
  }
}

template<int LEN>
inline bool
dvec<LEN>::operator==(const dvec<LEN>& b) const 
{
  double ta[LEN] VEC_ALIGN;
  double tb[LEN] VEC_ALIGN;
  a_store(ta);
  b.a_store(tb);
  for (int i = 0; i < LEN; i++) 
    if (fabs(ta[i]-tb[i]) > VEQUALITY) return false;
  return true;
}

#define OP_IMPL(__op__) {                         \
  struct vec_internal<LEN> result;                \
  for (int i = 0; i < LEN/2; i++) {               \
    result.v[i] = __op__(data.v[i], b.data.v[i]); \
  }                                               \
 return dvec<LEN>(result);                        \
}
   
template<int LEN>
inline dvec<LEN> 
dvec<LEN>::operator+(const dvec<LEN>& b)
{
  OP_IMPL(_mm_add_pd)
}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::operator-(const dvec<LEN>& b)
{
  OP_IMPL(_mm_sub_pd)
}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::operator*(const dvec<LEN>& b)
{
  OP_IMPL(_mm_mul_pd)
}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::operator/(const dvec<LEN>& b)
{
  OP_IMPL(_mm_div_pd)
}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::madd(const dvec<LEN>& s, const dvec<LEN>& b)
{
  struct vec_internal<LEN> r;
  for (int i = 0; i < LEN/2; i++) {
    r.v[i] = _mm_mul_pd(data.v[i], s.data.v[i]);
  }
  for (int i = 0; i < LEN/2; i++) {
    r.v[i] = _mm_add_pd(r.v[i], b.data.v[i]);
  }
  return dvec<LEN>(r);
}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::madd(const double s, const dvec<LEN>& b)
{  
  double _t[LEN] VEC_ALIGN;
  for (int i = 0; i < LEN; i++) _t[i] = s;
  dvec<LEN> t(_t,true);
  return madd(t, b);
}

template<int LEN>
inline double 
dvec<LEN>::fold(double identity, const dvec_op& op, int limit)
{
    double _t[LEN] VEC_ALIGN;
    a_store(_t);
    double val = identity;
    for (int i = 0; i < limit; i++) {
	val = op(val,_t[i]);
    }
    return val;
}

template<int LEN>
inline dvec<LEN>
dvec<LEN>::map(const dvec_unop& op, int limit)
{
    double _t[LEN] VEC_ALIGN;
    a_store(_t);
    for (int i = 0; i < limit; i++) {
	_t[i] = op(_t[i]);
    }
    return dvec<LEN>(_t);
}


template <int LEN>
inline std::ostream&
operator<<(std::ostream& out, const dvec<LEN>& v)
{
  double _t[LEN] VEC_ALIGN;
  v.a_store(_t);
  out << "<";
  for (int i = 0; i < LEN; i++) {
    out << _t[i];
    if (i != LEN-1) 
      out << ",";
  }
  out << ">";
  return out;
}

class vec2d {
public:
  
  vec2d() { 
    _init(0,0); 
  }
  
  vec2d(double x, double y) {
    _init(x,y);
  }

  vec2d(const vec2d& proto)
  {
    _vec = proto._vec;
  }

  vec2d& operator=(const vec2d& b) 
  {
    _vec = b._vec;
    return *this;
  }

  double operator[](int index) const {
    double  v[2] __attribute__((aligned(16)));
    _mm_store_pd(v, _vec);
    return v[index];
  }

  void ustore(double* arr) const {
    // assume nothing about the alignment of arr
    double  v[2] __attribute__((aligned(16)));    
    _mm_store_pd(v, _vec);
    arr[0] = v[0];
    arr[1] = v[1];
  }

  double x() const { return (*this)[0]; }
  double y() const { return (*this)[1]; }

  vec2d operator+(const vec2d& b) const
  {
    return vec2d(_mm_add_pd(_vec,b._vec));
  }
  
  vec2d operator-(const vec2d& b) const
  {
    return vec2d(_mm_sub_pd(vec(),b.vec()));
  }
  
  vec2d operator*(const vec2d& b) const
  {
    return vec2d(_mm_mul_pd(vec(),b.vec()));
  }
  
  vec2d operator/(const vec2d& b) const
  {
    return vec2d(_mm_div_pd(vec(),b.vec()));
  }

  vec2d madd(const double& scalar, const vec2d& b) const
  {
    return madd(vec2d(scalar,scalar), b);
  }

  vec2d madd(const vec2d& s, const vec2d& b) const
  {
    return vec2d(_mm_add_pd(_mm_mul_pd(vec(),s.vec()),b.vec()));
  }

private:
  //double  v[2] __attribute__((aligned(16)));
  v2df _vec;
  
  vec2d(const v2df& result) 
  {
    _vec = result;
  }
   
  v2df vec() const { return _vec; }
 
  void _init(double x, double y) 
  {
    double  v[2] __attribute__((aligned(16)));
    v[0] = x;
    v[1] = y;
    _vec = _mm_load_pd(v);
  }    

  friend std::ostream& operator<<(std::ostream& out, const vec2d& v);
};
  
inline std::ostream& 
operator<<(std::ostream& out, const vec2d& v)
{
  out << "<" << v.x() << "," << v.y() << ">";
  return out;
}

#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
