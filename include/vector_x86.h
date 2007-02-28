#ifndef __VECTOR_X86
#define __VECTOR_X86

#define ALIGN16(_m) (double*)((((long)(_m)) + 0x10L) & ~0xFL);

typedef double v2df __attribute__((vector_size(16)));

#if !defined(__builtin_ia32_storeapd) 
static inline void __builtin_ia32_storeapd(double* v, v2df vector) 
{
    __builtin_ia32_storeupd(v, vector);
}
#endif

#if !defined(__builtin_ia32_loadapd)
static inline v2df __builtin_ia32_loadapd(double* v)
{
    return __builtin_ia32_loadupd(v);
}
#endif

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
    v = ALIGN16(m);
    __builtin_ia32_storeapd(v,proto.vec());
  }

  vec2d& operator=(const vec2d& b) 
  {
    __builtin_ia32_storeapd(v, b.vec());
    return *this;
  }

  double operator[](int index) const {
    return v[index];
  }

  double x() const { return v[0]; }
  double y() const { return v[1]; }

  vec2d operator+(const vec2d& b) const
  {
    return vec2d(__builtin_ia32_addpd(vec(),b.vec()));
  }
  
  vec2d operator-(const vec2d& b) const
  {
    return vec2d(__builtin_ia32_subpd(vec(),b.vec()));
  }
  
  vec2d operator*(const vec2d& b) const
  {
    return vec2d(__builtin_ia32_mulpd(vec(),b.vec()));
  }
  
  vec2d operator/(const vec2d& b) const
  {
    return vec2d(__builtin_ia32_divpd(vec(),b.vec()));
  }

  vec2d madd(const double& scalar, const vec2d& b) const
  {
    return madd(vec2d(scalar,scalar), b);
  }

  vec2d madd(const vec2d& s, const vec2d& b) const
  {
    
    return vec2d(__builtin_ia32_addpd(__builtin_ia32_mulpd(vec(),s.vec()),b.vec()));
  }

private:
  double  m[4];
  double* v;
  
    vec2d::vec2d(const v2df& result) 
    {
	v = ALIGN16(m);
	__builtin_ia32_storeapd(v, result);
    }
    
  v2df vec() const {
    return __builtin_ia32_loadapd(v);
  }

  void _init(double x, double y) 
  {
    v = ALIGN16(m);
    v[0] = x;
    v[1] = y;
  }    
  friend std::ostream& operator<<(std::ostream& out, const vec2d& v);
};
  
std::ostream& 
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
