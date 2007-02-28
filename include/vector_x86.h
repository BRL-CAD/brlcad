#ifndef __VECTOR_X86
#define __VECTOR_X86

#include <emmintrin.h>

#define ALIGN16(_m) (double*)((((long)(_m)) + 0x10L) & ~0xFL);

typedef double v2df __attribute__((vector_size(16)));


template<int LEN>
inline dvec<LEN>::dvec(double s)
{
  
}

template<int LEN>
inline dvec<LEN>::dvec(const double* vals)
{

}

template<int LEN>
inline dvec<LEN>::dvec(const dvec<LEN>& p)
{

}

template<int LEN>
inline dvec<LEN>& 
dvec<LEN>::operator=(const dvec<LEN>& p)
{
  return *this;
}

template<int LEN>
inline double 
dvec<LEN>::operator[](int index) const
{
  return 0.0;
}

template<int LEN>
inline void 
dvec<LEN>::u_store(double* arr) const
{
  return 0.0;
}

template<int LEN>
inline void 
dvec<LEN>::a_store(double* arr) const
{

}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::operator+(const dvec<LEN>& b)
{
  return dvec<LEN>(0.0);
}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::operator-(const dvec<LEN>& b)
{
  return dvec<LEN>(0.0);
}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::operator*(const dvec<LEN>& b)
{
  return dvec<LEN>(0.0);
}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::operator/(const dvec<LEN>& b)
{
  return dvec<LEN>(0.0);
}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::madd(const dvec<LEN>& s, const dvec<LEN>& b)
{
  return dvec<LEN>(0.0);
}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::madd(const double s, const dvec<LEN>& b)
{  
  return dvec<LEN>(0.0);
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
  
  vec2d::vec2d(const v2df& result) 
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
