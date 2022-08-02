/*                           D V E C . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#ifndef BN_DVEC_H
#define BN_DVEC_H

#include "common.h"

#include <math.h>
extern "C++" {
/* Hide iostream from Doxygen with cond */
/** @cond */
#include <iostream>
#include <ostream>
/** @endcond */
}

/* Needed for fastf_t definition */
#include "bu/defines.h"

/* Needed for VUNITIZE_TOL and NEAR_ZERO */
#include "vmath.h"

/** @addtogroup bn_vectormath */
/** @{ */
/** @file bn/dvec.h */

extern "C++" {

const double VEQUALITY = 0.0000001;

template<int LEN>
struct dvec_internal;

template<int LEN>
struct fvec_internal;

template<int LEN>
class dvec;

template<int LEN>
std::ostream& operator<<(std::ostream& out, const dvec<LEN>& v);

class dvec_unop {
public:
    virtual double operator()(double a) const = 0;
    virtual ~dvec_unop() {}
};

class dvec_op {
public:
    virtual double operator()(double a, double b) const = 0;
    virtual ~dvec_op() {}
};

template<int LEN>
class dvec {
public:
    dvec(double s);
    dvec(const double* vals);
    dvec(const float* vals);
    dvec(const dvec<LEN>& p);

    dvec<LEN>& operator=(const dvec<LEN>& p);
    double operator[](int index) const;
    void u_store(double* arr) const;
    void u_store(float* arr) const;
    void a_store(double* arr) const;
    void a_store(float* arr) const;

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
	double operator()(double a) const { return ::sqrt(a); }
    };
private:
    dvec_internal<LEN> data;

    dvec(const dvec_internal<LEN>& d);
    dvec(const fvec_internal<LEN>& f);
};

//#define DVEC4(V, t, a, b, c, d) double v#t[4] VEC_ALIGN = {(a), (b), (c), (d)}; V(v#t)

// use this to create 16-byte aligned memory on platforms that support it
#define VEC_ALIGN

/* Doxygen doesn't like these includes... */
/** @cond */
/*#undef __SSE2__*/ // Test FPU version
#if defined(__SSE2__) && defined(__GNUC__) && defined(HAVE_EMMINTRIN_H) && defined(HAVE_EMMINTRIN)
#  define __x86_vector__

#ifdef HAVE_EMMINTRIN_H
#  include <emmintrin.h>
#endif

#undef VEC_ALIGN
#define VEC_ALIGN __attribute__((aligned(16)))

typedef double v2df __attribute__((vector_size(16)));
typedef double v2f __attribute__((vector_size(8)));

template<int LEN>
struct dvec_internal {
    v2df v[LEN/2];
};

template<int LEN>
struct fvec_internal {
    v2f v[LEN/2];
};

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
inline dvec<LEN>::dvec(const double* vals)
{
    for (int i = 0; i < LEN/2; i++) {
	/* NOTE: assumes that vals are 16-byte aligned */
	data.v[i] = _mm_load_pd(&vals[i*2]);
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
inline dvec<LEN>::dvec(const dvec_internal<LEN>& d)
{
    for (int i = 0; i < LEN/2; i++) data.v[i] = d.v[i];
}

template<int LEN>
inline dvec<LEN>::dvec(const fvec_internal<LEN>& f)
{
    for (int i = 0; i < LEN/2; i++) data.v[i] = f.v[i];
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

#define DOP_IMPL(__op__) {                             \
	dvec_internal<LEN> result;                         \
	for (int i = 0; i < LEN/2; i++) {                 \
	    result.v[i] = __op__(data.v[i], b.data.v[i]); \
	}                                                 \
	return dvec<LEN>(result);                         \
    }

template<int LEN>
inline dvec<LEN>
dvec<LEN>::operator+(const dvec<LEN>& b)
{
    DOP_IMPL(_mm_add_pd);
}

template<int LEN>
inline dvec<LEN>
dvec<LEN>::operator-(const dvec<LEN>& b)
{
    DOP_IMPL(_mm_sub_pd);
}

template<int LEN>
inline dvec<LEN>
dvec<LEN>::operator*(const dvec<LEN>& b)
{
    DOP_IMPL(_mm_mul_pd);
}

template<int LEN>
inline dvec<LEN>
dvec<LEN>::operator/(const dvec<LEN>& b)
{
    DOP_IMPL(_mm_div_pd);
}

template<int LEN>
inline dvec<LEN>
dvec<LEN>::madd(const dvec<LEN>& s, const dvec<LEN>& b)
{
    dvec_internal<LEN> r;
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
    dvec<LEN> t(_t);
    return madd(t, b);
}

template<int LEN>
inline double
dvec<LEN>::foldr(double identity, const dvec_op& op, int limit)
{
    double _t[LEN] VEC_ALIGN;
    a_store(_t);
    double val = identity;
    for (int i = limit-1; i >= 0; i--) {
	val = op(_t[i], val);
    }
    return val;
}

template<int LEN>
inline double
dvec<LEN>::foldl(double identity, const dvec_op& op, int limit)
{
    double _t[LEN] VEC_ALIGN;
    a_store(_t);
    double val = identity;
    for (int i = 0; i < limit; i++) {
	val = op(val, _t[i]);
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
	_init(0, 0);
    }

    vec2d(double x_, double y_) {
	_init(x_, y_);
    }

    vec2d(const vec2d& proto) {
	_vec = proto._vec;
    }

    vec2d& operator=(const vec2d& b) {
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

    vec2d operator+(const vec2d& b) const {
	return vec2d(_mm_add_pd(_vec, b._vec));
    }

    vec2d operator-(const vec2d& b) const {
	return vec2d(_mm_sub_pd(vec(), b.vec()));
    }

    vec2d operator*(const vec2d& b) const {
	return vec2d(_mm_mul_pd(vec(), b.vec()));
    }

    vec2d operator/(const vec2d& b) const {
	return vec2d(_mm_div_pd(vec(), b.vec()));
    }

    vec2d madd(const double& scalar, const vec2d& b) const {
	return madd(vec2d(scalar, scalar), b);
    }

    vec2d madd(const vec2d& s, const vec2d& b) const {
	return vec2d(_mm_add_pd(_mm_mul_pd(vec(), s.vec()), b.vec()));
    }

private:
    //double  v[2] __attribute__((aligned(16)));
    v2df _vec;

    vec2d(const v2df& result) {
	_vec = result;
    }

    v2df vec() const { return _vec; }

    void _init(double x_, double y_) {
	double  v[2] __attribute__((aligned(16)));
	v[0] = x_;
	v[1] = y_;
	_vec = _mm_load_pd(v);
    }

};

inline std::ostream&
operator<<(std::ostream& out, const vec2d& v)
{
    out << "<" << v.x() << "," << v.y() << ">";
    return out;
}

#else
#  define __fpu_vector__

#ifdef __GNUC__
#undef VEC_ALIGN
#define VEC_ALIGN __attribute__((aligned(16)))
#endif

template<int LEN>
struct dvec_internal {
    double v[LEN] VEC_ALIGN;
};

template<int LEN>
struct fvec_internal {
    float v[LEN] VEC_ALIGN;
};

template<int LEN>
inline dvec<LEN>::dvec(double s)
{
    for (int i = 0; i < LEN; i++)
	data.v[i] = s;
}

template<int LEN>
inline dvec<LEN>::dvec(const float* vals)
{
    for (int i = 0; i < LEN; i++)
	data.v[i] = vals[i];
}

template<int LEN>
inline dvec<LEN>::dvec(const double* vals)
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
inline dvec<LEN>::dvec(const dvec_internal<LEN>& d)
{
    for (int i = 0; i < LEN; i++)
	data.v[i] = d.v[i];
}

template<int LEN>
inline dvec<LEN>::dvec(const fvec_internal<LEN>& f)
{
    for (int i = 0; i < LEN; i++)
	data.v[i] = f.v[i];
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
dvec<LEN>::u_store(float* arr) const
{
    a_store(arr);
}

template<int LEN>
inline void
dvec<LEN>::u_store(double* arr) const
{
    a_store(arr);
}

template<int LEN>
inline void
dvec<LEN>::a_store(float* arr) const
{
    for (int i = 0; i < LEN; i++)
	arr[i] = data.v[i];
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
    dvec_internal<LEN> r;
    for (int i = 0; i < LEN; i++)
	r.v[i] = data.v[i] + b.data.v[i];
    return dvec<LEN>(r);
}

template<int LEN>
inline dvec<LEN>
dvec<LEN>::operator-(const dvec<LEN>& b)
{
    dvec_internal<LEN> r;
    for (int i = 0; i < LEN; i++)
	r.v[i] = data.v[i] - b.data.v[i];
    return dvec<LEN>(r);
}

template<int LEN>
inline dvec<LEN>
dvec<LEN>::operator*(const dvec<LEN>& b)
{
    dvec_internal<LEN> r;
    for (int i = 0; i < LEN; i++)
	r.v[i] = data.v[i] * b.data.v[i];
    return dvec<LEN>(r);
}

template<int LEN>
inline dvec<LEN>
dvec<LEN>::operator/(const dvec<LEN>& b)
{
    dvec_internal<LEN> r;
    for (int i = 0; i < LEN; i++)
	r.v[i] = data.v[i] / b.data.v[i];
    return dvec<LEN>(r);
}

template<int LEN>
inline dvec<LEN>
dvec<LEN>::madd(const dvec<LEN>& s, const dvec<LEN>& b)
{
    dvec_internal<LEN> r;
    for (int i = 0; i < LEN; i++)
	r.v[i] = data.v[i] * s.data.v[i] + b.data.v[i];
    return dvec<LEN>(r);
}

template<int LEN>
inline dvec<LEN>
dvec<LEN>::madd(const double s, const dvec<LEN>& b)
{
    dvec_internal<LEN> r;
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
	val = op(data.v[i], val);
    }
    return val;
}
template<int LEN>
inline double
dvec<LEN>::foldl(double identity, const dvec_op& op, int limit)
{
    double val = identity;
    for (int i = 0; i < limit; i++) {
	val = op(val, data.v[i]);
    }
    return val;
}

template<int LEN>
inline dvec<LEN>
dvec<LEN>::map(const dvec_unop& op, int limit)
{
    dvec_internal<LEN> r;
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
	_init(0, 0);
    }

    vec2d(double xin, double yin) {
	_init(xin, yin);
    }

    vec2d(const vec2d& proto) {
	_init(proto.v[0], proto.v[1]);
    }

    vec2d& operator=(const vec2d& b) {
	v[0] = b.v[0];
	v[1] = b.v[1];
	return *this;
    }

    double operator[](int index) const { return v[index]; }

    double x() const { return v[0]; }
    double y() const { return v[1]; }

    vec2d operator+(const vec2d& b) const {
	return vec2d(v[0] + b.v[0], v[1] + b.v[1]);
    }

    vec2d operator-(const vec2d& b) const {
	return vec2d(v[0] - b.v[0], v[1] - b.v[1]);
    }

    vec2d operator*(const vec2d& b) const {
	return vec2d(v[0] * b.v[0], v[1] * b.v[1]);
    }

    vec2d operator/(const vec2d& b) const {
	return vec2d(v[0] / b.v[0], v[1] / b.v[1]);
    }

    vec2d madd(const double& scalar, const vec2d& b) const {
	return vec2d(v[0]*scalar+b.v[0], v[1]*scalar+b.v[1]);
    }

    vec2d madd(const vec2d& s, const vec2d& b) const {
	return vec2d(v[0]*s.v[0]+b.v[0], v[1]*s.v[1]+b.v[1]);
    }

private:
    double* v;
    double  m[4];

    void _init(double xin, double yin) {
	// align to 16-byte boundary
	v = (double*)((((uintptr_t)m) + 0x10L) & ~0xFL);
	v[0] = xin;
	v[1] = yin;
    }
};

inline std::ostream&
operator<<(std::ostream& out, const vec2d& v)
{
    out << "<" << v.x() << "," << v.y() << ">";
    return out;
}

#endif
/** @endcond */

inline bool vequals(const vec2d& a, const vec2d& b) {
    return
    (fabs(a.x()-b.x()) < VEQUALITY) &&
    (fabs(a.y()-b.y()) < VEQUALITY);
}

// 2x2 row-major matrix
typedef fastf_t mat2d_t[4] VEC_ALIGN;

/* Hide from Doxygen with cond */
/** @cond */

// 2d point
typedef fastf_t pt2d_t[2] VEC_ALIGN;

/** @endcond */

//--------------------------------------------------------------------------------
// MATH / VECTOR ops
inline
bool mat2d_inverse(mat2d_t inv, mat2d_t m) {
    pt2d_t _a = {m[0], m[1]};
    pt2d_t _b = {m[3], m[2]};
    dvec<2> a(_a);
    dvec<2> b(_b);
    dvec<2> c = a*b;
    fastf_t det = c.foldr(0, dvec<2>::sub());
    if (NEAR_ZERO(det, VUNITIZE_TOL)) return false;
    fastf_t scale = 1.0 / det;
    double tmp[4] VEC_ALIGN = {m[3], -m[1], -m[2], m[0]};
    dvec<4> iv(tmp);
    dvec<4> sv(scale);
    dvec<4> r = iv * sv;
    r.a_store(inv);
    return true;
}
inline
void mat2d_pt2d_mul(pt2d_t r, mat2d_t m, pt2d_t p) {
    pt2d_t _a = {m[0], m[2]};
    pt2d_t _b = {m[1], m[3]};
    dvec<2> x(p[0]);
    dvec<2> y(p[1]);
    dvec<2> a(_a);
    dvec<2> b(_b);
    dvec<2> c = a*x + b*y;
    c.a_store(r);
}
inline
void pt2dsub(pt2d_t r, pt2d_t a, pt2d_t b) {
    dvec<2> va(a);
    dvec<2> vb(b);
    dvec<2> vr = va - vb;
    vr.a_store(r);
}

inline
fastf_t v2mag(pt2d_t p) {
    dvec<2> a(p);
    dvec<2> sq = a*a;
    return sqrt(sq.foldr(0, dvec<2>::add()));
}

inline
void move(pt2d_t a, const double *b) {
    a[0] = b[0];
    a[1] = b[1];
}
}

/** @} */

#endif /* BN_DVEC_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
