#ifndef __VECTOR_FPU
#define __VECTOR_FPU

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
    _init(proto.v[0],proto.v[1]);
  }

  vec2d& operator=(const vec2d& b) 
  {
    v[0] = b.v[0];
    v[1] = b.v[1];
    return *this;
  }

  double operator[](int index) const { return v[index]; }

  double x() const { return v[0]; }
  double y() const { return v[1]; }

  vec2d operator+(const vec2d& b) const
  {
    return vec2d(v[0] + b.v[0], v[1] + b.v[1]);
  }
  
  vec2d operator-(const vec2d& b) const
  {
    return vec2d(v[0] - b.v[0], v[1] - b.v[1]);
  }
  
  vec2d operator*(const vec2d& b) const
  {
    return vec2d(v[0] * b.v[0], v[1] * b.v[1]);
  }
  
  vec2d operator/(const vec2d& b) const
  {
    return vec2d(v[0] / b.v[0], v[1] / b.v[1]);
  }

  vec2d madd(const double& scalar, const vec2d& b) const
  {
    return vec2d(v[0]*scalar+b.v[0],v[1]*scalar+b.v[1]);
  }

  vec2d madd(const vec2d& s, const vec2d& b) const
  {
    return vec2d(v[0]*s.v[0]+b.v[0],v[1]*s.v[1]+b.v[1]);
  }

private:
  double* v;
  double  m[4];

  void _init(double x, double y) {
    // align to 16-byte boundary
    v = (double*)((((long)m) + 0x10L) & ~0xFL);
    v[0] = x;
    v[1] = y;
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
