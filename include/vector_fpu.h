#ifndef __VECTOR_FPU
#define __VECTOR_FPU

#ifdef __GNUC__
#undef VEC_ALIGN
#define VEC_ALIGN __attribute__((aligned(16)))
#endif

template<int LEN>
struct vec_internal {
  double v[LEN] VEC_ALIGN;
};

template<int LEN>
inline dvec<LEN>::dvec(double s)
{
  for (int i = 0; i < LEN; i++) 
    data.v[i] = s;
}

template<int LEN>
inline dvec<LEN>::dvec(const double* vals, bool aligned)
{
  for (int i = 0; i < LEN; i++) 
    data.v[i] = vals[i];
}

template<int LEN>
inline dvec<LEN>::dvec(const dvec<LEN>& p)
{
  for (int i = 0; i < LEN; i++) 
    data.v[i] = p.data.v[i];
}

template<int LEN>
inline dvec<LEN>::dvec(const vec_internal<LEN>& d)
{
  for (int i = 0; i < LEN; i++) 
    data.v[i] = d.v[i];
}

template<int LEN>
inline dvec<LEN>& 
dvec<LEN>::operator=(const dvec<LEN>& p)
{
  for (int i = 0; i < LEN; i++) 
    data.v[i] = p.data.v[i];
  return *this;
}

template<int LEN>
inline double 
dvec<LEN>::operator[](int index) const
{
  return data.v[index];
}

template<int LEN>
inline void 
dvec<LEN>::u_store(double* arr) const
{
  a_store(arr);
}

template<int LEN>
inline void 
dvec<LEN>::a_store(double* arr) const
{
  for (int i = 0; i < LEN; i++) 
    arr[i] = data.v[i];
}

template<int LEN>
inline bool 
dvec<LEN>::operator==(const dvec<LEN>& b) const
{
  for (int i = 0; i < LEN; i++)
    if (fabs(data.v[i]-b.data.v[i]) > VEQUALITY) return false;
  return true;
}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::operator+(const dvec<LEN>& b)
{
  struct vec_internal<LEN> r;
  for (int i = 0; i < LEN; i++) 
    r.v[i] = data.v[i] + b.data.v[i];
  return dvec<LEN>(r);
}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::operator-(const dvec<LEN>& b)
{
  struct vec_internal<LEN> r;
  for (int i = 0; i < LEN; i++) 
    r.v[i] = data.v[i] - b.data.v[i];
  return dvec<LEN>(r);
}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::operator*(const dvec<LEN>& b)
{
  struct vec_internal<LEN> r;
  for (int i = 0; i < LEN; i++) 
    r.v[i] = data.v[i] * b.data.v[i];
  return dvec<LEN>(r);
}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::operator/(const dvec<LEN>& b)
{
  struct vec_internal<LEN> r;
  for (int i = 0; i < LEN; i++) 
    r.v[i] = data.v[i] / b.data.v[i];
  return dvec<LEN>(r);
}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::madd(const dvec<LEN>& s, const dvec<LEN>& b)
{
  struct vec_internal<LEN> r;
  for (int i = 0; i < LEN; i++) 
    r.v[i] = data.v[i] * s.data.v[i] + b.data.v[i];
  return dvec<LEN>(r);
}

template<int LEN>
inline dvec<LEN> 
dvec<LEN>::madd(const double s, const dvec<LEN>& b)
{  
  struct vec_internal<LEN> r;
  for (int i = 0; i < LEN; i++) 
    r.v[i] = data.v[i] * s +  b.data.v[i];
  return dvec<LEN>(r);
}

template<int LEN>
inline double 
dvec<LEN>::foldr(double identity, const dvec_op& op, int limit)
{
    double val = identity;
    for (int i = limit-1; i >= 0; i--) {
	val = op(data.v[i],val);
    }
    return val;
}
template<int LEN>
inline double 
dvec<LEN>::foldl(double identity, const dvec_op& op, int limit)
{
    double val = identity;
    for (int i = 0; i < limit; i++) {
	val = op(val,data.v[i]);
    }
    return val;
}

template<int LEN>
inline dvec<LEN>
dvec<LEN>::map(const dvec_unop& op, int limit)
{
    struct vec_internal<LEN> r;
    for (int i = 0; i < limit; i++) {
	r.v[i] = op(data.v[i]);
    }
    return dvec<LEN>(r);
}


template <int LEN>
inline std::ostream&
operator<<(std::ostream& out, const dvec<LEN>& v)
{
  out << "<";
  for (int i = 0; i < LEN; i++) {
    out << v.data.v[i];
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
