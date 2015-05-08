/*                           D V E C . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file dvec.h
 *
 *
 */

#ifndef DVEC_H
#define DVEC_H

#include "common.h"

#include <math.h>

/* Needed for fastf_t definition */
#include "bu/defines.h"

/* Needed for VUNITIZE_TOL and NEAR_ZERO */
#include "vmath.h"

extern "C++" {
#include <iostream>

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

/*#undef __SSE2__*/ // Test FPU version
#if defined(__SSE2__) && defined(__GNUC__) && defined(HAVE_EMMINTRIN_H) && defined(HAVE_EMMINTRIN)
#  define __x86_vector__
#  include "vector_x86.h"
#else
#  define __fpu_vector__
#  include "vector_fpu.h"
#endif

    inline bool vequals(const vec2d& a, const vec2d& b) {
	return
	    (fabs(a.x()-b.x()) < VEQUALITY) &&
	    (fabs(a.y()-b.y()) < VEQUALITY);
    }

    // 2x2 row-major matrix
    typedef fastf_t mat2d_t[4] VEC_ALIGN;

    // 2d point
    typedef fastf_t pt2d_t[2] VEC_ALIGN;

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

#endif /* DVEC_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
