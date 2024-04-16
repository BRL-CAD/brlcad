// detria - a delaunay triangulation library
//
// Copyright (c) 2024 Kimbatt (https://github.com/Kimbatt)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
// (Note - official licenses are at the bottom of the file - this copy is
// placed here to indiciate which one BRL-CAD is using detria under and to
// make it easier for our license scanner to find.)

//
// Example usage:
/*

// Create a square, and triangulate it

// List of points (positions)
std::vector<detria::PointD> points =
{
    { 0.0, 0.0 },
    { 1.0, 0.0 },
    { 1.0, 1.0 },
    { 0.0, 1.0 }
};

// List of point indices
std::vector<uint32_t> outline = { 0, 1, 2, 3 };

bool delaunay = true;

detria::Triangulation tri;
tri.setPoints(points);
tri.addOutline(outline);

bool success = tri.triangulate(delaunay);

if (success)
{
    bool cwTriangles = true;

    tri.forEachTriangle([&](detria::Triangle<uint32_t> triangle)
    {
        // `triangle` contains the point indices

        detria::PointD firstPointOfTriangle = points[triangle.x];
        detria::PointD secondPointOfTriangle = points[triangle.y];
        detria::PointD thirdPointOfTriangle = points[triangle.z];
    }, cwTriangles);
}

*/
// See https://github.com/Kimbatt/detria for more information and full documentation.
// License: WTFPL or MIT, at your choice. You can find the license texts at the bottom of this file.

#ifndef DETRIA_HPP_INCLUDED
#define DETRIA_HPP_INCLUDED

#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <optional>
#include <variant>
#include <string>
#include <sstream>

#if !defined(NDEBUG)
#include <iostream>

#if !defined(_WIN32) || !_WIN32
#include <csignal>
#endif

#endif

namespace detria
{
    template <typename T>
    struct Vec2
    {
        T x;
        T y;
    };

    using PointF = Vec2<float>;
    using PointD = Vec2<double>;

    template <typename Idx>
    struct Triangle
    {
        Idx x;
        Idx y;
        Idx z;
    };

    template <typename Idx>
    struct Edge
    {
        Idx x;
        Idx y;
    };

#if (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L) || __cplusplus >= 202002L
#define DETRIA_LIKELY [[likely]]
#define DETRIA_UNLIKELY [[unlikely]]
#else
#define DETRIA_LIKELY
#define DETRIA_UNLIKELY
#endif

    namespace predicates
    {
        // http://www.cs.cmu.edu/~quake/robust.html
        // Routines for Arbitrary Precision Floating-point Arithmetic and Fast Robust Geometric Predicates
        // Placed in the public domain by Jonathan Richard Shewchuk

        template <typename Scalar>
        struct ErrorBounds
        {
            Scalar splitter;
            Scalar epsilon;
            Scalar resulterrbound;
            Scalar ccwerrboundA, ccwerrboundB, ccwerrboundC;
            Scalar o3derrboundA, o3derrboundB, o3derrboundC;
            Scalar iccerrboundA, iccerrboundB, iccerrboundC;
            Scalar isperrboundA, isperrboundB, isperrboundC;
        };

        template <typename Scalar>
        constexpr ErrorBounds<Scalar> calculateErrorBounds()
        {
            Scalar check = Scalar(1);
            bool everyOther = true;

            Scalar epsilon = Scalar(1);
            Scalar splitter = Scalar(1);

            Scalar lastcheck{ };
            do
            {
                lastcheck = check;
                epsilon *= Scalar(0.5);
                if (everyOther)
                {
                    splitter *= Scalar(2);
                }
                everyOther = !everyOther;
                check = Scalar(1) + epsilon;
            }
            while (check != Scalar(1) && (check != lastcheck));

            splitter += Scalar(1);

            ErrorBounds<Scalar> bounds{ };
            bounds.splitter = splitter;
            bounds.epsilon = epsilon;
            bounds.resulterrbound = (Scalar(3) + Scalar(8) * epsilon) * epsilon;
            bounds.ccwerrboundA = (Scalar(3) + Scalar(16) * epsilon) * epsilon;
            bounds.ccwerrboundB = (Scalar(2) + Scalar(12) * epsilon) * epsilon;
            bounds.ccwerrboundC = (Scalar(9) + Scalar(64) * epsilon) * epsilon * epsilon;
            bounds.o3derrboundA = (Scalar(7) + Scalar(56) * epsilon) * epsilon;
            bounds.o3derrboundB = (Scalar(3) + Scalar(28) * epsilon) * epsilon;
            bounds.o3derrboundC = (Scalar(26) + Scalar(288) * epsilon) * epsilon * epsilon;
            bounds.iccerrboundA = (Scalar(10) + Scalar(96) * epsilon) * epsilon;
            bounds.iccerrboundB = (Scalar(4) + Scalar(48) * epsilon) * epsilon;
            bounds.iccerrboundC = (Scalar(44) + Scalar(576) * epsilon) * epsilon * epsilon;
            bounds.isperrboundA = (Scalar(16) + Scalar(224) * epsilon) * epsilon;
            bounds.isperrboundB = (Scalar(5) + Scalar(72) * epsilon) * epsilon;
            bounds.isperrboundC = (Scalar(71) + Scalar(1408) * epsilon) * epsilon * epsilon;

            return bounds;
        }

        template <typename Scalar>
        static constexpr predicates::ErrorBounds<Scalar> errorBounds = predicates::calculateErrorBounds<Scalar>();

#define DETRIA_INEXACT

#define DETRIA_Fast_Two_Sum_Tail(a, b, x, y) \
  bvirt = x - a; \
  y = b - bvirt

#define DETRIA_Fast_Two_Sum(a, b, x, y) \
  x = Scalar(a + b); \
  DETRIA_Fast_Two_Sum_Tail(a, b, x, y)

#define DETRIA_Two_Sum_Tail(a, b, x, y) \
  bvirt = Scalar(x - a); \
  avirt = x - bvirt; \
  bround = b - bvirt; \
  around = a - avirt; \
  y = around + bround

#define DETRIA_Two_Sum(a, b, x, y) \
  x = Scalar(a + b); \
  DETRIA_Two_Sum_Tail(a, b, x, y)

#define DETRIA_Two_Diff_Tail(a, b, x, y) \
  bvirt = Scalar(a - x); \
  avirt = x + bvirt; \
  bround = bvirt - b; \
  around = a - avirt; \
  y = around + bround

#define DETRIA_Two_Diff(a, b, x, y) \
  x = Scalar(a - b); \
  DETRIA_Two_Diff_Tail(a, b, x, y)

#define DETRIA_Split(a, ahi, alo) \
  c = Scalar(errorBounds<Scalar>.splitter * a); \
  abig = Scalar(c - a); \
  ahi = c - abig; \
  alo = a - ahi

#define DETRIA_Two_Product_Tail(a, b, x, y) \
  DETRIA_Split(a, ahi, alo); \
  DETRIA_Split(b, bhi, blo); \
  err1 = x - (ahi * bhi); \
  err2 = err1 - (alo * bhi); \
  err3 = err2 - (ahi * blo); \
  y = (alo * blo) - err3

#define DETRIA_Two_Product(a, b, x, y) \
  x = Scalar(a * b); \
  DETRIA_Two_Product_Tail(a, b, x, y)

#define DETRIA_Two_Product_Presplit(a, b, bhi, blo, x, y) \
  x = Scalar(a * b); \
  DETRIA_Split(a, ahi, alo); \
  err1 = x - (ahi * bhi); \
  err2 = err1 - (alo * bhi); \
  err3 = err2 - (ahi * blo); \
  y = (alo * blo) - err3

#define DETRIA_Square_Tail(a, x, y) \
  DETRIA_Split(a, ahi, alo); \
  err1 = x - (ahi * ahi); \
  err3 = err1 - ((ahi + ahi) * alo); \
  y = (alo * alo) - err3

#define DETRIA_Square(a, x, y) \
  x = Scalar(a * a); \
  DETRIA_Square_Tail(a, x, y)

#define DETRIA_Two_One_Sum(a1, a0, b, x2, x1, x0) \
  DETRIA_Two_Sum(a0, b , _i, x0); \
  DETRIA_Two_Sum(a1, _i, x2, x1)

#define DETRIA_Two_One_Diff(a1, a0, b, x2, x1, x0) \
  DETRIA_Two_Diff(a0, b , _i, x0); \
  DETRIA_Two_Sum( a1, _i, x2, x1)

#define DETRIA_Two_Two_Sum(a1, a0, b1, b0, x3, x2, x1, x0) \
  DETRIA_Two_One_Sum(a1, a0, b0, _j, _0, x0); \
  DETRIA_Two_One_Sum(_j, _0, b1, x3, x2, x1)

#define DETRIA_Two_Two_Diff(a1, a0, b1, b0, x3, x2, x1, x0) \
  DETRIA_Two_One_Diff(a1, a0, b0, _j, _0, x0); \
  DETRIA_Two_One_Diff(_j, _0, b1, x3, x2, x1)

        template <typename Scalar>
        inline Scalar estimate(int elen, const Scalar* e)
        {
            Scalar Q = e[0];
            for (int eindex = 1; eindex < elen; eindex++)
            {
                Q += e[eindex];
            }
            return Q;
        }

        template <typename Scalar>
        inline constexpr Scalar Absolute(Scalar a)
        {
            return a >= Scalar(0) ? a : -a;
        }

        template <typename Scalar>
        int fast_expansion_sum_zeroelim(int elen, const Scalar* e, int flen, const Scalar* f, Scalar* h)
        {
            Scalar Q;
            DETRIA_INEXACT Scalar Qnew;
            DETRIA_INEXACT Scalar hh;
            DETRIA_INEXACT Scalar bvirt;
            Scalar avirt, bround, around;
            int eindex, findex, hindex;
            Scalar enow, fnow;

            enow = e[0];
            fnow = f[0];
            eindex = findex = 0;
            if ((fnow > enow) == (fnow > -enow))
            {
                Q = enow;
                enow = e[++eindex];
            }
            else
            {
                Q = fnow;
                fnow = f[++findex];
            }
            hindex = 0;
            if ((eindex < elen) && (findex < flen))
            {
                if ((fnow > enow) == (fnow > -enow))
                {
                    DETRIA_Fast_Two_Sum(enow, Q, Qnew, hh);
                    enow = e[++eindex];
                }
                else
                {
                    DETRIA_Fast_Two_Sum(fnow, Q, Qnew, hh);
                    fnow = f[++findex];
                }
                Q = Qnew;
                if (hh != Scalar(0))
                {
                    h[hindex++] = hh;
                }
                while ((eindex < elen) && (findex < flen))
                {
                    if ((fnow > enow) == (fnow > -enow))
                    {
                        DETRIA_Two_Sum(Q, enow, Qnew, hh);
                        enow = e[++eindex];
                    }
                    else
                    {
                        DETRIA_Two_Sum(Q, fnow, Qnew, hh);
                        fnow = f[++findex];
                    }
                    Q = Qnew;
                    if (hh != Scalar(0))
                    {
                        h[hindex++] = hh;
                    }
                }
            }
            while (eindex < elen)
            {
                DETRIA_Two_Sum(Q, enow, Qnew, hh);
                enow = e[++eindex];
                Q = Qnew;
                if (hh != Scalar(0))
                {
                    h[hindex++] = hh;
                }
            }
            while (findex < flen)
            {
                DETRIA_Two_Sum(Q, fnow, Qnew, hh);
                fnow = f[++findex];
                Q = Qnew;
                if (hh != Scalar(0))
                {
                    h[hindex++] = hh;
                }
            }
            if ((Q != Scalar(0)) || (hindex == 0))
            {
                h[hindex++] = Q;
            }
            return hindex;
        }

        template <typename Scalar>
        Scalar orient2dadapt(Scalar pa[2], Scalar pb[2], Scalar pc[2], Scalar detsum)
        {
            DETRIA_INEXACT Scalar acx, acy, bcx, bcy;
            Scalar acxtail, acytail, bcxtail, bcytail;
            DETRIA_INEXACT Scalar detleft, detright;
            Scalar detlefttail, detrighttail;
            Scalar det, errbound;
            Scalar B[4]{ }, C1[8]{ }, C2[12]{ }, D[16]{ };
            DETRIA_INEXACT Scalar B3;
            int C1length, C2length, Dlength;
            Scalar u[4]{ };
            DETRIA_INEXACT Scalar u3;
            DETRIA_INEXACT Scalar s1, t1;
            Scalar s0, t0;

            DETRIA_INEXACT Scalar bvirt;
            Scalar avirt, bround, around;
            DETRIA_INEXACT Scalar c;
            DETRIA_INEXACT Scalar abig;
            Scalar ahi, alo, bhi, blo;
            Scalar err1, err2, err3;
            DETRIA_INEXACT Scalar _i, _j;
            Scalar _0;

            acx = pa[0] - pc[0];
            bcx = pb[0] - pc[0];
            acy = pa[1] - pc[1];
            bcy = pb[1] - pc[1];

            DETRIA_Two_Product(acx, bcy, detleft, detlefttail);
            DETRIA_Two_Product(acy, bcx, detright, detrighttail);

            DETRIA_Two_Two_Diff(detleft, detlefttail, detright, detrighttail, B3, B[2], B[1], B[0]);
            B[3] = B3;

            det = estimate(4, B);
            errbound = errorBounds<Scalar>.ccwerrboundB * detsum;
            if ((det >= errbound) || (-det >= errbound))
            {
                return det;
            }

            DETRIA_Two_Diff_Tail(pa[0], pc[0], acx, acxtail);
            DETRIA_Two_Diff_Tail(pb[0], pc[0], bcx, bcxtail);
            DETRIA_Two_Diff_Tail(pa[1], pc[1], acy, acytail);
            DETRIA_Two_Diff_Tail(pb[1], pc[1], bcy, bcytail);

            if ((acxtail == Scalar(0)) && (acytail == Scalar(0)) && (bcxtail == Scalar(0)) && (bcytail == Scalar(0)))
            {
                return det;
            }

            errbound = errorBounds<Scalar>.ccwerrboundC * detsum + errorBounds<Scalar>.resulterrbound * Absolute(det);
            det += (acx * bcytail + bcy * acxtail) - (acy * bcxtail + bcx * acytail);
            if ((det >= errbound) || (-det >= errbound))
            {
                return det;
            }

            DETRIA_Two_Product(acxtail, bcy, s1, s0);
            DETRIA_Two_Product(acytail, bcx, t1, t0);
            DETRIA_Two_Two_Diff(s1, s0, t1, t0, u3, u[2], u[1], u[0]);
            u[3] = u3;
            C1length = fast_expansion_sum_zeroelim(4, B, 4, u, C1);

            DETRIA_Two_Product(acx, bcytail, s1, s0);
            DETRIA_Two_Product(acy, bcxtail, t1, t0);
            DETRIA_Two_Two_Diff(s1, s0, t1, t0, u3, u[2], u[1], u[0]);
            u[3] = u3;
            C2length = fast_expansion_sum_zeroelim(C1length, C1, 4, u, C2);

            DETRIA_Two_Product(acxtail, bcytail, s1, s0);
            DETRIA_Two_Product(acytail, bcxtail, t1, t0);
            DETRIA_Two_Two_Diff(s1, s0, t1, t0, u3, u[2], u[1], u[0]);
            u[3] = u3;
            Dlength = fast_expansion_sum_zeroelim(C2length, C2, 4, u, D);

            return(D[Dlength - 1]);
        }

        template <typename Scalar>
        int scale_expansion_zeroelim(int elen, const Scalar* e, Scalar b, Scalar* h)
        {
            DETRIA_INEXACT Scalar Q, sum;
            Scalar hh;
            DETRIA_INEXACT Scalar product1;
            Scalar product0;
            int eindex, hindex;
            Scalar enow;
            DETRIA_INEXACT Scalar bvirt;
            Scalar avirt, bround, around;
            DETRIA_INEXACT Scalar c;
            DETRIA_INEXACT Scalar abig;
            Scalar ahi, alo, bhi, blo;
            Scalar err1, err2, err3;

            DETRIA_Split(b, bhi, blo);
            DETRIA_Two_Product_Presplit(e[0], b, bhi, blo, Q, hh);
            hindex = 0;
            if (hh != Scalar(0))
            {
                h[hindex++] = hh;
            }

            for (eindex = 1; eindex < elen; eindex++)
            {
                enow = e[eindex];
                DETRIA_Two_Product_Presplit(enow, b, bhi, blo, product1, product0);
                DETRIA_Two_Sum(Q, product0, sum, hh);
                if (hh != Scalar(0))
                {
                    h[hindex++] = hh;
                }

                DETRIA_Fast_Two_Sum(product1, sum, Q, hh);
                if (hh != Scalar(0))
                {
                    h[hindex++] = hh;
                }
            }

            if (Q != Scalar(0) || hindex == 0)
            {
                h[hindex++] = Q;
            }

            return hindex;
        }

        template <typename Scalar>
        Scalar incircleadapt(Scalar pa[2], Scalar pb[2], Scalar pc[2], Scalar pd[2], Scalar permanent)
        {
            DETRIA_INEXACT Scalar adx, bdx, cdx, ady, bdy, cdy;
            Scalar det, errbound;

            DETRIA_INEXACT Scalar bdxcdy1, cdxbdy1, cdxady1, adxcdy1, adxbdy1, bdxady1;
            Scalar bdxcdy0, cdxbdy0, cdxady0, adxcdy0, adxbdy0, bdxady0;
            Scalar bc[4], ca[4], ab[4];
            DETRIA_INEXACT Scalar bc3, ca3, ab3;
            Scalar axbc[8], axxbc[16], aybc[8], ayybc[16], adet[32];
            int axbclen, axxbclen, aybclen, ayybclen, alen;
            Scalar bxca[8], bxxca[16], byca[8], byyca[16], bdet[32];
            int bxcalen, bxxcalen, bycalen, byycalen, blen;
            Scalar cxab[8], cxxab[16], cyab[8], cyyab[16], cdet[32];
            int cxablen, cxxablen, cyablen, cyyablen, clen;
            Scalar abdet[64];
            int ablen;
            Scalar fin1[1152], fin2[1152];
            Scalar* finnow, * finother, * finswap;
            int finlength;

            Scalar adxtail, bdxtail, cdxtail, adytail, bdytail, cdytail;
            DETRIA_INEXACT Scalar adxadx1, adyady1, bdxbdx1, bdybdy1, cdxcdx1, cdycdy1;
            Scalar adxadx0, adyady0, bdxbdx0, bdybdy0, cdxcdx0, cdycdy0;
            Scalar aa[4], bb[4], cc[4];
            DETRIA_INEXACT Scalar aa3, bb3, cc3;
            DETRIA_INEXACT Scalar ti1, tj1;
            Scalar ti0, tj0;
            Scalar u[4], v[4];
            DETRIA_INEXACT Scalar u3, v3;
            Scalar temp8[8], temp16a[16], temp16b[16], temp16c[16];
            Scalar temp32a[32], temp32b[32], temp48[48], temp64[64];
            int temp8len, temp16alen, temp16blen, temp16clen;
            int temp32alen, temp32blen, temp48len, temp64len;
            Scalar axtbb[8], axtcc[8], aytbb[8], aytcc[8];
            int axtbblen, axtcclen, aytbblen, aytcclen;
            Scalar bxtaa[8], bxtcc[8], bytaa[8], bytcc[8];
            int bxtaalen, bxtcclen, bytaalen, bytcclen;
            Scalar cxtaa[8], cxtbb[8], cytaa[8], cytbb[8];
            int cxtaalen, cxtbblen, cytaalen, cytbblen;
            Scalar axtbc[8], aytbc[8], bxtca[8], bytca[8], cxtab[8], cytab[8];
            int axtbclen, aytbclen, bxtcalen, bytcalen, cxtablen, cytablen;
            Scalar axtbct[16], aytbct[16], bxtcat[16], bytcat[16], cxtabt[16], cytabt[16];
            int axtbctlen, aytbctlen, bxtcatlen, bytcatlen, cxtabtlen, cytabtlen;
            Scalar axtbctt[8], aytbctt[8], bxtcatt[8];
            Scalar bytcatt[8], cxtabtt[8], cytabtt[8];
            int axtbcttlen, aytbcttlen, bxtcattlen, bytcattlen, cxtabttlen, cytabttlen;
            Scalar abt[8], bct[8], cat[8];
            int abtlen, bctlen, catlen;
            Scalar abtt[4], bctt[4], catt[4];
            int abttlen, bcttlen, cattlen;
            DETRIA_INEXACT Scalar abtt3, bctt3, catt3;
            Scalar negate;

            DETRIA_INEXACT Scalar bvirt;
            Scalar avirt, bround, around;
            DETRIA_INEXACT Scalar c;
            DETRIA_INEXACT Scalar abig;
            Scalar ahi, alo, bhi, blo;
            Scalar err1, err2, err3;
            DETRIA_INEXACT Scalar _i, _j;
            Scalar _0;

            axtbclen = 0;
            aytbclen = 0;
            bxtcalen = 0;
            bytcalen = 0;
            cxtablen = 0;
            cytablen = 0;

            adx = Scalar(pa[0] - pd[0]);
            bdx = Scalar(pb[0] - pd[0]);
            cdx = Scalar(pc[0] - pd[0]);
            ady = Scalar(pa[1] - pd[1]);
            bdy = Scalar(pb[1] - pd[1]);
            cdy = Scalar(pc[1] - pd[1]);

            DETRIA_Two_Product(bdx, cdy, bdxcdy1, bdxcdy0);
            DETRIA_Two_Product(cdx, bdy, cdxbdy1, cdxbdy0);
            DETRIA_Two_Two_Diff(bdxcdy1, bdxcdy0, cdxbdy1, cdxbdy0, bc3, bc[2], bc[1], bc[0]);
            bc[3] = bc3;
            axbclen = scale_expansion_zeroelim(4, bc, adx, axbc);
            axxbclen = scale_expansion_zeroelim(axbclen, axbc, adx, axxbc);
            aybclen = scale_expansion_zeroelim(4, bc, ady, aybc);
            ayybclen = scale_expansion_zeroelim(aybclen, aybc, ady, ayybc);
            alen = fast_expansion_sum_zeroelim(axxbclen, axxbc, ayybclen, ayybc, adet);

            DETRIA_Two_Product(cdx, ady, cdxady1, cdxady0);
            DETRIA_Two_Product(adx, cdy, adxcdy1, adxcdy0);
            DETRIA_Two_Two_Diff(cdxady1, cdxady0, adxcdy1, adxcdy0, ca3, ca[2], ca[1], ca[0]);
            ca[3] = ca3;
            bxcalen = scale_expansion_zeroelim(4, ca, bdx, bxca);
            bxxcalen = scale_expansion_zeroelim(bxcalen, bxca, bdx, bxxca);
            bycalen = scale_expansion_zeroelim(4, ca, bdy, byca);
            byycalen = scale_expansion_zeroelim(bycalen, byca, bdy, byyca);
            blen = fast_expansion_sum_zeroelim(bxxcalen, bxxca, byycalen, byyca, bdet);

            DETRIA_Two_Product(adx, bdy, adxbdy1, adxbdy0);
            DETRIA_Two_Product(bdx, ady, bdxady1, bdxady0);
            DETRIA_Two_Two_Diff(adxbdy1, adxbdy0, bdxady1, bdxady0, ab3, ab[2], ab[1], ab[0]);
            ab[3] = ab3;
            cxablen = scale_expansion_zeroelim(4, ab, cdx, cxab);
            cxxablen = scale_expansion_zeroelim(cxablen, cxab, cdx, cxxab);
            cyablen = scale_expansion_zeroelim(4, ab, cdy, cyab);
            cyyablen = scale_expansion_zeroelim(cyablen, cyab, cdy, cyyab);
            clen = fast_expansion_sum_zeroelim(cxxablen, cxxab, cyyablen, cyyab, cdet);

            ablen = fast_expansion_sum_zeroelim(alen, adet, blen, bdet, abdet);
            finlength = fast_expansion_sum_zeroelim(ablen, abdet, clen, cdet, fin1);

            det = estimate(finlength, fin1);
            errbound = errorBounds<Scalar>.iccerrboundB * permanent;
            if ((det >= errbound) || (-det >= errbound))
            {
                return det;
            }

            DETRIA_Two_Diff_Tail(pa[0], pd[0], adx, adxtail);
            DETRIA_Two_Diff_Tail(pa[1], pd[1], ady, adytail);
            DETRIA_Two_Diff_Tail(pb[0], pd[0], bdx, bdxtail);
            DETRIA_Two_Diff_Tail(pb[1], pd[1], bdy, bdytail);
            DETRIA_Two_Diff_Tail(pc[0], pd[0], cdx, cdxtail);
            DETRIA_Two_Diff_Tail(pc[1], pd[1], cdy, cdytail);

            if (adxtail == Scalar(0) && bdxtail == Scalar(0) && cdxtail == Scalar(0) &&
                adytail == Scalar(0) && bdytail == Scalar(0) && cdytail == Scalar(0))
            {
                return det;
            }

            errbound = errorBounds<Scalar>.iccerrboundC * permanent + errorBounds<Scalar>.resulterrbound * Absolute(det);
            det += ((adx * adx + ady * ady) * ((bdx * cdytail + cdy * bdxtail)
                - (bdy * cdxtail + cdx * bdytail))
                + Scalar(2) * (adx * adxtail + ady * adytail) * (bdx * cdy - bdy * cdx))
                + ((bdx * bdx + bdy * bdy) * ((cdx * adytail + ady * cdxtail)
                    - (cdy * adxtail + adx * cdytail))
                    + Scalar(2) * (bdx * bdxtail + bdy * bdytail) * (cdx * ady - cdy * adx))
                + ((cdx * cdx + cdy * cdy) * ((adx * bdytail + bdy * adxtail)
                    - (ady * bdxtail + bdx * adytail))
                    + Scalar(2) * (cdx * cdxtail + cdy * cdytail) * (adx * bdy - ady * bdx));

            if (det >= errbound || -det >= errbound)
            {
                return det;
            }

            finnow = fin1;
            finother = fin2;

            if (bdxtail != Scalar(0) || bdytail != Scalar(0) || cdxtail != Scalar(0) || cdytail != Scalar(0))
            {
                DETRIA_Square(adx, adxadx1, adxadx0);
                DETRIA_Square(ady, adyady1, adyady0);
                DETRIA_Two_Two_Sum(adxadx1, adxadx0, adyady1, adyady0, aa3, aa[2], aa[1], aa[0]);
                aa[3] = aa3;
            }
            if (cdxtail != Scalar(0) || cdytail != Scalar(0) || adxtail != Scalar(0) || adytail != Scalar(0))
            {
                DETRIA_Square(bdx, bdxbdx1, bdxbdx0);
                DETRIA_Square(bdy, bdybdy1, bdybdy0);
                DETRIA_Two_Two_Sum(bdxbdx1, bdxbdx0, bdybdy1, bdybdy0, bb3, bb[2], bb[1], bb[0]);
                bb[3] = bb3;
            }
            if (adxtail != Scalar(0) || adytail != Scalar(0) || bdxtail != Scalar(0) || bdytail != Scalar(0))
            {
                DETRIA_Square(cdx, cdxcdx1, cdxcdx0);
                DETRIA_Square(cdy, cdycdy1, cdycdy0);
                DETRIA_Two_Two_Sum(cdxcdx1, cdxcdx0, cdycdy1, cdycdy0, cc3, cc[2], cc[1], cc[0]);
                cc[3] = cc3;
            }

            if (adxtail != Scalar(0))
            {
                axtbclen = scale_expansion_zeroelim(4, bc, adxtail, axtbc);
                temp16alen = scale_expansion_zeroelim(axtbclen, axtbc, Scalar(2) * adx, temp16a);

                axtcclen = scale_expansion_zeroelim(4, cc, adxtail, axtcc);
                temp16blen = scale_expansion_zeroelim(axtcclen, axtcc, bdy, temp16b);

                axtbblen = scale_expansion_zeroelim(4, bb, adxtail, axtbb);
                temp16clen = scale_expansion_zeroelim(axtbblen, axtbb, -cdy, temp16c);

                temp32alen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32a);
                temp48len = fast_expansion_sum_zeroelim(temp16clen, temp16c, temp32alen, temp32a, temp48);
                finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
                finswap = finnow; finnow = finother; finother = finswap;
            }

            if (adytail != Scalar(0))
            {
                aytbclen = scale_expansion_zeroelim(4, bc, adytail, aytbc);
                temp16alen = scale_expansion_zeroelim(aytbclen, aytbc, Scalar(2) * ady, temp16a);

                aytbblen = scale_expansion_zeroelim(4, bb, adytail, aytbb);
                temp16blen = scale_expansion_zeroelim(aytbblen, aytbb, cdx, temp16b);

                aytcclen = scale_expansion_zeroelim(4, cc, adytail, aytcc);
                temp16clen = scale_expansion_zeroelim(aytcclen, aytcc, -bdx, temp16c);

                temp32alen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32a);
                temp48len = fast_expansion_sum_zeroelim(temp16clen, temp16c, temp32alen, temp32a, temp48);
                finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
                finswap = finnow; finnow = finother; finother = finswap;
            }

            if (bdxtail != Scalar(0))
            {
                bxtcalen = scale_expansion_zeroelim(4, ca, bdxtail, bxtca);
                temp16alen = scale_expansion_zeroelim(bxtcalen, bxtca, Scalar(2) * bdx, temp16a);

                bxtaalen = scale_expansion_zeroelim(4, aa, bdxtail, bxtaa);
                temp16blen = scale_expansion_zeroelim(bxtaalen, bxtaa, cdy, temp16b);

                bxtcclen = scale_expansion_zeroelim(4, cc, bdxtail, bxtcc);
                temp16clen = scale_expansion_zeroelim(bxtcclen, bxtcc, -ady, temp16c);

                temp32alen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32a);
                temp48len = fast_expansion_sum_zeroelim(temp16clen, temp16c, temp32alen, temp32a, temp48);
                finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
                finswap = finnow; finnow = finother; finother = finswap;
            }

            if (bdytail != Scalar(0))
            {
                bytcalen = scale_expansion_zeroelim(4, ca, bdytail, bytca);
                temp16alen = scale_expansion_zeroelim(bytcalen, bytca, Scalar(2) * bdy, temp16a);

                bytcclen = scale_expansion_zeroelim(4, cc, bdytail, bytcc);
                temp16blen = scale_expansion_zeroelim(bytcclen, bytcc, adx, temp16b);

                bytaalen = scale_expansion_zeroelim(4, aa, bdytail, bytaa);
                temp16clen = scale_expansion_zeroelim(bytaalen, bytaa, -cdx, temp16c);

                temp32alen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32a);
                temp48len = fast_expansion_sum_zeroelim(temp16clen, temp16c, temp32alen, temp32a, temp48);
                finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
                finswap = finnow; finnow = finother; finother = finswap;
            }

            if (cdxtail != Scalar(0))
            {
                cxtablen = scale_expansion_zeroelim(4, ab, cdxtail, cxtab);
                temp16alen = scale_expansion_zeroelim(cxtablen, cxtab, Scalar(2) * cdx, temp16a);

                cxtbblen = scale_expansion_zeroelim(4, bb, cdxtail, cxtbb);
                temp16blen = scale_expansion_zeroelim(cxtbblen, cxtbb, ady, temp16b);

                cxtaalen = scale_expansion_zeroelim(4, aa, cdxtail, cxtaa);
                temp16clen = scale_expansion_zeroelim(cxtaalen, cxtaa, -bdy, temp16c);

                temp32alen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32a);
                temp48len = fast_expansion_sum_zeroelim(temp16clen, temp16c, temp32alen, temp32a, temp48);
                finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
                finswap = finnow; finnow = finother; finother = finswap;
            }

            if (cdytail != Scalar(0))
            {
                cytablen = scale_expansion_zeroelim(4, ab, cdytail, cytab);
                temp16alen = scale_expansion_zeroelim(cytablen, cytab, Scalar(2) * cdy, temp16a);

                cytaalen = scale_expansion_zeroelim(4, aa, cdytail, cytaa);
                temp16blen = scale_expansion_zeroelim(cytaalen, cytaa, bdx, temp16b);

                cytbblen = scale_expansion_zeroelim(4, bb, cdytail, cytbb);
                temp16clen = scale_expansion_zeroelim(cytbblen, cytbb, -adx, temp16c);

                temp32alen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32a);
                temp48len = fast_expansion_sum_zeroelim(temp16clen, temp16c, temp32alen, temp32a, temp48);
                finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
                finswap = finnow; finnow = finother; finother = finswap;
            }

            if (adxtail != Scalar(0) || adytail != Scalar(0))
            {
                if (bdxtail != Scalar(0) || bdytail != Scalar(0) || cdxtail != Scalar(0) || cdytail != Scalar(0))
                {
                    DETRIA_Two_Product(bdxtail, cdy, ti1, ti0);
                    DETRIA_Two_Product(bdx, cdytail, tj1, tj0);
                    DETRIA_Two_Two_Sum(ti1, ti0, tj1, tj0, u3, u[2], u[1], u[0]);
                    u[3] = u3;
                    negate = -bdy;
                    DETRIA_Two_Product(cdxtail, negate, ti1, ti0);
                    negate = -bdytail;
                    DETRIA_Two_Product(cdx, negate, tj1, tj0);
                    DETRIA_Two_Two_Sum(ti1, ti0, tj1, tj0, v3, v[2], v[1], v[0]);
                    v[3] = v3;
                    bctlen = fast_expansion_sum_zeroelim(4, u, 4, v, bct);

                    DETRIA_Two_Product(bdxtail, cdytail, ti1, ti0);
                    DETRIA_Two_Product(cdxtail, bdytail, tj1, tj0);
                    DETRIA_Two_Two_Diff(ti1, ti0, tj1, tj0, bctt3, bctt[2], bctt[1], bctt[0]);
                    bctt[3] = bctt3;
                    bcttlen = 4;
                }
                else
                {
                    bct[0] = Scalar(0);
                    bctlen = 1;
                    bctt[0] = Scalar(0);
                    bcttlen = 1;
                }

                if (adxtail != Scalar(0))
                {
                    temp16alen = scale_expansion_zeroelim(axtbclen, axtbc, adxtail, temp16a);
                    axtbctlen = scale_expansion_zeroelim(bctlen, bct, adxtail, axtbct);
                    temp32alen = scale_expansion_zeroelim(axtbctlen, axtbct, Scalar(2) * adx, temp32a);
                    temp48len = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp32alen, temp32a, temp48);
                    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
                    finswap = finnow; finnow = finother; finother = finswap;

                    if (bdytail != Scalar(0))
                    {
                        temp8len = scale_expansion_zeroelim(4, cc, adxtail, temp8);
                        temp16alen = scale_expansion_zeroelim(temp8len, temp8, bdytail, temp16a);
                        finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp16alen, temp16a, finother);
                        finswap = finnow; finnow = finother; finother = finswap;
                    }

                    if (cdytail != Scalar(0))
                    {
                        temp8len = scale_expansion_zeroelim(4, bb, -adxtail, temp8);
                        temp16alen = scale_expansion_zeroelim(temp8len, temp8, cdytail, temp16a);
                        finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp16alen, temp16a, finother);
                        finswap = finnow; finnow = finother; finother = finswap;
                    }

                    temp32alen = scale_expansion_zeroelim(axtbctlen, axtbct, adxtail, temp32a);
                    axtbcttlen = scale_expansion_zeroelim(bcttlen, bctt, adxtail, axtbctt);
                    temp16alen = scale_expansion_zeroelim(axtbcttlen, axtbctt, Scalar(2) * adx, temp16a);
                    temp16blen = scale_expansion_zeroelim(axtbcttlen, axtbctt, adxtail, temp16b);
                    temp32blen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32b);
                    temp64len = fast_expansion_sum_zeroelim(temp32alen, temp32a, temp32blen, temp32b, temp64);
                    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp64len, temp64, finother);
                    finswap = finnow; finnow = finother; finother = finswap;
                }

                if (adytail != Scalar(0))
                {
                    temp16alen = scale_expansion_zeroelim(aytbclen, aytbc, adytail, temp16a);
                    aytbctlen = scale_expansion_zeroelim(bctlen, bct, adytail, aytbct);
                    temp32alen = scale_expansion_zeroelim(aytbctlen, aytbct, Scalar(2) * ady, temp32a);
                    temp48len = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp32alen, temp32a, temp48);
                    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
                    finswap = finnow; finnow = finother; finother = finswap;

                    temp32alen = scale_expansion_zeroelim(aytbctlen, aytbct, adytail, temp32a);
                    aytbcttlen = scale_expansion_zeroelim(bcttlen, bctt, adytail, aytbctt);
                    temp16alen = scale_expansion_zeroelim(aytbcttlen, aytbctt, Scalar(2) * ady, temp16a);
                    temp16blen = scale_expansion_zeroelim(aytbcttlen, aytbctt, adytail, temp16b);
                    temp32blen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32b);
                    temp64len = fast_expansion_sum_zeroelim(temp32alen, temp32a, temp32blen, temp32b, temp64);
                    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp64len, temp64, finother);
                    finswap = finnow; finnow = finother; finother = finswap;
                }
            }

            if (bdxtail != Scalar(0) || bdytail != Scalar(0))
            {
                if (cdxtail != Scalar(0) || cdytail != Scalar(0) || adxtail != Scalar(0) || adytail != Scalar(0))
                {
                    DETRIA_Two_Product(cdxtail, ady, ti1, ti0);
                    DETRIA_Two_Product(cdx, adytail, tj1, tj0);
                    DETRIA_Two_Two_Sum(ti1, ti0, tj1, tj0, u3, u[2], u[1], u[0]);
                    u[3] = u3;
                    negate = -cdy;
                    DETRIA_Two_Product(adxtail, negate, ti1, ti0);
                    negate = -cdytail;
                    DETRIA_Two_Product(adx, negate, tj1, tj0);
                    DETRIA_Two_Two_Sum(ti1, ti0, tj1, tj0, v3, v[2], v[1], v[0]);
                    v[3] = v3;
                    catlen = fast_expansion_sum_zeroelim(4, u, 4, v, cat);

                    DETRIA_Two_Product(cdxtail, adytail, ti1, ti0);
                    DETRIA_Two_Product(adxtail, cdytail, tj1, tj0);
                    DETRIA_Two_Two_Diff(ti1, ti0, tj1, tj0, catt3, catt[2], catt[1], catt[0]);
                    catt[3] = catt3;
                    cattlen = 4;
                }
                else
                {
                    cat[0] = Scalar(0);
                    catlen = 1;
                    catt[0] = Scalar(0);
                    cattlen = 1;
                }

                if (bdxtail != Scalar(0))
                {
                    temp16alen = scale_expansion_zeroelim(bxtcalen, bxtca, bdxtail, temp16a);
                    bxtcatlen = scale_expansion_zeroelim(catlen, cat, bdxtail, bxtcat);
                    temp32alen = scale_expansion_zeroelim(bxtcatlen, bxtcat, Scalar(2) * bdx, temp32a);
                    temp48len = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp32alen, temp32a, temp48);
                    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
                    finswap = finnow; finnow = finother; finother = finswap;

                    if (cdytail != Scalar(0))
                    {
                        temp8len = scale_expansion_zeroelim(4, aa, bdxtail, temp8);
                        temp16alen = scale_expansion_zeroelim(temp8len, temp8, cdytail, temp16a);
                        finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp16alen, temp16a, finother);
                        finswap = finnow; finnow = finother; finother = finswap;
                    }

                    if (adytail != Scalar(0))
                    {
                        temp8len = scale_expansion_zeroelim(4, cc, -bdxtail, temp8);
                        temp16alen = scale_expansion_zeroelim(temp8len, temp8, adytail, temp16a);
                        finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp16alen, temp16a, finother);
                        finswap = finnow; finnow = finother; finother = finswap;
                    }

                    temp32alen = scale_expansion_zeroelim(bxtcatlen, bxtcat, bdxtail, temp32a);
                    bxtcattlen = scale_expansion_zeroelim(cattlen, catt, bdxtail, bxtcatt);
                    temp16alen = scale_expansion_zeroelim(bxtcattlen, bxtcatt, Scalar(2) * bdx, temp16a);
                    temp16blen = scale_expansion_zeroelim(bxtcattlen, bxtcatt, bdxtail, temp16b);
                    temp32blen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32b);
                    temp64len = fast_expansion_sum_zeroelim(temp32alen, temp32a, temp32blen, temp32b, temp64);
                    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp64len, temp64, finother);
                    finswap = finnow; finnow = finother; finother = finswap;
                }

                if (bdytail != Scalar(0))
                {
                    temp16alen = scale_expansion_zeroelim(bytcalen, bytca, bdytail, temp16a);
                    bytcatlen = scale_expansion_zeroelim(catlen, cat, bdytail, bytcat);
                    temp32alen = scale_expansion_zeroelim(bytcatlen, bytcat, Scalar(2) * bdy, temp32a);
                    temp48len = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp32alen, temp32a, temp48);
                    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
                    finswap = finnow; finnow = finother; finother = finswap;

                    temp32alen = scale_expansion_zeroelim(bytcatlen, bytcat, bdytail, temp32a);
                    bytcattlen = scale_expansion_zeroelim(cattlen, catt, bdytail, bytcatt);
                    temp16alen = scale_expansion_zeroelim(bytcattlen, bytcatt, Scalar(2) * bdy, temp16a);
                    temp16blen = scale_expansion_zeroelim(bytcattlen, bytcatt, bdytail, temp16b);
                    temp32blen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32b);
                    temp64len = fast_expansion_sum_zeroelim(temp32alen, temp32a, temp32blen, temp32b, temp64);
                    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp64len, temp64, finother);
                    finswap = finnow; finnow = finother; finother = finswap;
                }
            }

            if (cdxtail != Scalar(0) || cdytail != Scalar(0))
            {
                if (adxtail != Scalar(0) || adytail != Scalar(0) || bdxtail != Scalar(0) || bdytail != Scalar(0))
                {
                    DETRIA_Two_Product(adxtail, bdy, ti1, ti0);
                    DETRIA_Two_Product(adx, bdytail, tj1, tj0);
                    DETRIA_Two_Two_Sum(ti1, ti0, tj1, tj0, u3, u[2], u[1], u[0]);
                    u[3] = u3;
                    negate = -ady;
                    DETRIA_Two_Product(bdxtail, negate, ti1, ti0);
                    negate = -adytail;
                    DETRIA_Two_Product(bdx, negate, tj1, tj0);
                    DETRIA_Two_Two_Sum(ti1, ti0, tj1, tj0, v3, v[2], v[1], v[0]);
                    v[3] = v3;
                    abtlen = fast_expansion_sum_zeroelim(4, u, 4, v, abt);

                    DETRIA_Two_Product(adxtail, bdytail, ti1, ti0);
                    DETRIA_Two_Product(bdxtail, adytail, tj1, tj0);
                    DETRIA_Two_Two_Diff(ti1, ti0, tj1, tj0, abtt3, abtt[2], abtt[1], abtt[0]);
                    abtt[3] = abtt3;
                    abttlen = 4;
                }
                else
                {
                    abt[0] = Scalar(0);
                    abtlen = 1;
                    abtt[0] = Scalar(0);
                    abttlen = 1;
                }

                if (cdxtail != Scalar(0))
                {
                    temp16alen = scale_expansion_zeroelim(cxtablen, cxtab, cdxtail, temp16a);
                    cxtabtlen = scale_expansion_zeroelim(abtlen, abt, cdxtail, cxtabt);
                    temp32alen = scale_expansion_zeroelim(cxtabtlen, cxtabt, Scalar(2) * cdx, temp32a);
                    temp48len = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp32alen, temp32a, temp48);
                    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
                    finswap = finnow; finnow = finother; finother = finswap;

                    if (adytail != Scalar(0))
                    {
                        temp8len = scale_expansion_zeroelim(4, bb, cdxtail, temp8);
                        temp16alen = scale_expansion_zeroelim(temp8len, temp8, adytail, temp16a);
                        finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp16alen, temp16a, finother);
                        finswap = finnow; finnow = finother; finother = finswap;
                    }

                    if (bdytail != Scalar(0))
                    {
                        temp8len = scale_expansion_zeroelim(4, aa, -cdxtail, temp8);
                        temp16alen = scale_expansion_zeroelim(temp8len, temp8, bdytail, temp16a);
                        finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp16alen, temp16a, finother);
                        finswap = finnow; finnow = finother; finother = finswap;
                    }

                    temp32alen = scale_expansion_zeroelim(cxtabtlen, cxtabt, cdxtail, temp32a);
                    cxtabttlen = scale_expansion_zeroelim(abttlen, abtt, cdxtail, cxtabtt);
                    temp16alen = scale_expansion_zeroelim(cxtabttlen, cxtabtt, Scalar(2) * cdx, temp16a);
                    temp16blen = scale_expansion_zeroelim(cxtabttlen, cxtabtt, cdxtail, temp16b);
                    temp32blen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32b);
                    temp64len = fast_expansion_sum_zeroelim(temp32alen, temp32a, temp32blen, temp32b, temp64);
                    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp64len, temp64, finother);
                    finswap = finnow; finnow = finother; finother = finswap;
                }

                if (cdytail != Scalar(0))
                {
                    temp16alen = scale_expansion_zeroelim(cytablen, cytab, cdytail, temp16a);
                    cytabtlen = scale_expansion_zeroelim(abtlen, abt, cdytail, cytabt);
                    temp32alen = scale_expansion_zeroelim(cytabtlen, cytabt, Scalar(2) * cdy, temp32a);
                    temp48len = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp32alen, temp32a, temp48);
                    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp48len, temp48, finother);
                    finswap = finnow; finnow = finother; finother = finswap;

                    temp32alen = scale_expansion_zeroelim(cytabtlen, cytabt, cdytail, temp32a);
                    cytabttlen = scale_expansion_zeroelim(abttlen, abtt, cdytail, cytabtt);
                    temp16alen = scale_expansion_zeroelim(cytabttlen, cytabtt, Scalar(2) * cdy, temp16a);
                    temp16blen = scale_expansion_zeroelim(cytabttlen, cytabtt, cdytail, temp16b);
                    temp32blen = fast_expansion_sum_zeroelim(temp16alen, temp16a, temp16blen, temp16b, temp32b);
                    temp64len = fast_expansion_sum_zeroelim(temp32alen, temp32a, temp32blen, temp32b, temp64);
                    finlength = fast_expansion_sum_zeroelim(finlength, finnow, temp64len, temp64, finother);
                    finswap = finnow; finnow = finother; finother = finswap;
                }
            }

            return finnow[finlength - 1];
        }

#undef DETRIA_INEXACT
#undef DETRIA_Fast_Two_Sum_Tail
#undef DETRIA_Fast_Two_Sum
#undef DETRIA_Two_Sum_Tail
#undef DETRIA_Two_Sum
#undef DETRIA_Two_Diff_Tail
#undef DETRIA_Two_Diff
#undef DETRIA_Split
#undef DETRIA_Two_Product_Tail
#undef DETRIA_Two_Product
#undef DETRIA_Two_Product_Presplit
#undef DETRIA_Square_Tail
#undef DETRIA_Square
#undef DETRIA_Two_One_Sum
#undef DETRIA_Two_One_Diff
#undef DETRIA_Two_Two_Sum
#undef DETRIA_Two_Two_Diff
    }

    namespace detail
    {
        // Forward declare

        bool detriaAssert(bool condition, const char* message = nullptr);
    }

    namespace math
    {
#ifndef NDEBUG
        // Debug functions for integer overflow checking (from https://stackoverflow.com/a/1514309)

        template <typename Scalar>
        inline constexpr bool checkOverflowAdd(const Scalar& a, const Scalar& b)
        {
            if constexpr (std::is_floating_point_v<Scalar>)
            {
                return false;
            }
            else
            {
                return (b > Scalar(0) && a > std::numeric_limits<Scalar>::max() - b)
                    || (b < Scalar(0) && a < std::numeric_limits<Scalar>::min() - b);
            }
        }

        template <typename Scalar>
        inline constexpr bool checkOverflowSubtract(const Scalar& a, const Scalar& b)
        {
            if constexpr (std::is_floating_point_v<Scalar>)
            {
                return false;
            }
            else
            {
                return (b < Scalar(0) && a > std::numeric_limits<Scalar>::max() + b)
                    || (b > Scalar(0) && a < std::numeric_limits<Scalar>::min() + b);
            }
        }

        template <typename Scalar>
        inline constexpr bool checkOverflowMultiply(const Scalar& a, const Scalar& b)
        {
            if constexpr (std::is_floating_point_v<Scalar>)
            {
                return false;
            }
            else
            {
                return (a > 0 && b > 0 && a > std::numeric_limits<Scalar>::max() / b)
                    || (a > 0 && b < 0 && b < std::numeric_limits<Scalar>::min() / a)
                    || (a < 0 && b > 0 && a < std::numeric_limits<Scalar>::min() / b)
                    || (a < 0 && b < 0 && b < std::numeric_limits<Scalar>::max() / a);
            }
        }
#endif

        enum class Orientation : uint8_t
        {
            CW = 0, CCW = 1, Collinear = 2
        };

        template <bool Robust, typename Vec2>
        inline Orientation orient2d(const Vec2& a, const Vec2& b, const Vec2& c)
        {
            // The robust tests are only required for floating point types
            // For floating point types, VecComponent is always the same type as Scalar, so no actual conversions happen
            // For integer types, the values are converted to 64-bit signed integers, which can avoid overflows

            using VecComponent = decltype(Vec2::x);
            constexpr bool IsInteger = std::is_integral_v<VecComponent>;
            using Scalar = std::conditional_t<IsInteger, int64_t, VecComponent>;

            Scalar pa0 = static_cast<Scalar>(a.x);
            Scalar pa1 = static_cast<Scalar>(a.y);
            Scalar pb0 = static_cast<Scalar>(b.x);
            Scalar pb1 = static_cast<Scalar>(b.y);
            Scalar pc0 = static_cast<Scalar>(c.x);
            Scalar pc1 = static_cast<Scalar>(c.y);

#ifndef NDEBUG
            detail::detriaAssert(
                !checkOverflowSubtract(pa0, pc0) &&
                !checkOverflowSubtract(pb1, pc1) &&
                !checkOverflowSubtract(pa1, pc1) &&
                !checkOverflowSubtract(pb0, pc0) &&
                !checkOverflowMultiply(pa0 - pc0, pb1 - pc1) &&
                !checkOverflowMultiply(pa1 - pc1, pb0 - pc0) &&
                !checkOverflowSubtract((pa0 - pc0) * (pb1 - pc1), (pa1 - pc1) * (pb0 - pc0)),
                "Integer overflow"
            );
#endif

            Scalar detleft = (pa0 - pc0) * (pb1 - pc1);
            Scalar detright = (pa1 - pc1) * (pb0 - pc0);
            Scalar det = detleft - detright;

            if constexpr (Robust && !IsInteger)
            {
                do
                {
                    Scalar detsum = Scalar(0);

                    if (detleft > Scalar(0))
                    {
                        if (detright <= Scalar(0))
                        {
                            break;
                        }
                        else
                        {
                            detsum = detleft + detright;
                        }
                    }
                    else if (detleft < Scalar(0))
                    {
                        if (detright >= Scalar(0))
                        {
                            break;
                        }
                        else
                        {
                            detsum = -detleft - detright;
                        }
                    }
                    else
                    {
                        break;
                    }

                    Scalar errbound = predicates::errorBounds<Scalar>.ccwerrboundA * detsum;
                    if (predicates::Absolute(det) < errbound) DETRIA_UNLIKELY
                    {
                        Scalar pa[2]{ a.x, a.y };
                        Scalar pb[2]{ b.x, b.y };
                        Scalar pc[2]{ c.x, c.y };
                        det = predicates::orient2dadapt(pa, pb, pc, detsum);
                    }
                }
                while (false);
            }

            if (det < Scalar(0))
            {
                return Orientation::CW;
            }
            else if (det > Scalar(0))
            {
                return Orientation::CCW;
            }
            else
            {
                return Orientation::Collinear;
            }
        }

        enum class CircleLocation : uint8_t
        {
            Inside = 0, Outside = 1, Cocircular = 2
        };

        template <bool Robust, typename Vec2>
        inline CircleLocation incircle(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d)
        {
#ifndef NDEBUG
            // The points pa, pb, and pc must be in counterclockwise order, or the sign of the result will be reversed.
            detail::detriaAssert(math::orient2d<Robust, Vec2>(a, b, c) == math::Orientation::CCW);
#endif

            // See comments in orient2d for additional info about integer types

            using VecComponent = decltype(Vec2::x);
            constexpr bool IsInteger = std::is_integral_v<VecComponent>;
            using Scalar = std::conditional_t<IsInteger, int64_t, VecComponent>;

            Scalar pa0 = static_cast<Scalar>(a.x);
            Scalar pa1 = static_cast<Scalar>(a.y);
            Scalar pb0 = static_cast<Scalar>(b.x);
            Scalar pb1 = static_cast<Scalar>(b.y);
            Scalar pc0 = static_cast<Scalar>(c.x);
            Scalar pc1 = static_cast<Scalar>(c.y);
            Scalar pd0 = static_cast<Scalar>(d.x);
            Scalar pd1 = static_cast<Scalar>(d.y);

#ifndef NDEBUG
            detail::detriaAssert(
                !checkOverflowSubtract(pa0, pd0) &&
                !checkOverflowSubtract(pb0, pd0) &&
                !checkOverflowSubtract(pc0, pd0) &&
                !checkOverflowSubtract(pa1, pd1) &&
                !checkOverflowSubtract(pb1, pd1) &&
                !checkOverflowSubtract(pc1, pd1),
                "Integer overflow"
            );
#endif

            Scalar adx = pa0 - pd0;
            Scalar bdx = pb0 - pd0;
            Scalar cdx = pc0 - pd0;
            Scalar ady = pa1 - pd1;
            Scalar bdy = pb1 - pd1;
            Scalar cdy = pc1 - pd1;

#ifndef NDEBUG
            detail::detriaAssert(
                !checkOverflowMultiply(bdx, cdy) &&
                !checkOverflowMultiply(cdx, bdy) &&
                !checkOverflowMultiply(adx, adx) &&
                !checkOverflowMultiply(ady, ady) &&
                !checkOverflowAdd(adx * adx, ady * ady) &&

                !checkOverflowMultiply(cdx, ady) &&
                !checkOverflowMultiply(adx, cdy) &&
                !checkOverflowMultiply(bdx, bdx) &&
                !checkOverflowMultiply(bdy, bdy) &&
                !checkOverflowAdd(bdx * bdx, bdy * bdy) &&

                !checkOverflowMultiply(adx, bdy) &&
                !checkOverflowMultiply(bdx, ady) &&
                !checkOverflowMultiply(cdx, cdx) &&
                !checkOverflowMultiply(cdy, cdy) &&
                !checkOverflowAdd(cdx * cdx, cdy * cdy),
                "Integer overflow"
            );
#endif

            Scalar bdxcdy = bdx * cdy;
            Scalar cdxbdy = cdx * bdy;
            Scalar alift = adx * adx + ady * ady;

            Scalar cdxady = cdx * ady;
            Scalar adxcdy = adx * cdy;
            Scalar blift = bdx * bdx + bdy * bdy;

            Scalar adxbdy = adx * bdy;
            Scalar bdxady = bdx * ady;
            Scalar clift = cdx * cdx + cdy * cdy;

#ifndef NDEBUG
            detail::detriaAssert(
                !checkOverflowSubtract(bdxcdy, cdxbdy) &&
                !checkOverflowSubtract(cdxady, adxcdy) &&
                !checkOverflowSubtract(adxbdy, bdxady) &&
                !checkOverflowMultiply(alift, bdxcdy - cdxbdy) &&
                !checkOverflowMultiply(blift, cdxady - adxcdy) &&
                !checkOverflowMultiply(clift, adxbdy - bdxady) &&
                !checkOverflowAdd(alift * (bdxcdy - cdxbdy), blift * (cdxady - adxcdy)) &&
                !checkOverflowAdd(alift * (bdxcdy - cdxbdy) + blift * (cdxady - adxcdy), clift * (adxbdy - bdxady)),
                "Integer overflow"
            );
#endif

            Scalar det = alift * (bdxcdy - cdxbdy) + blift * (cdxady - adxcdy) + clift * (adxbdy - bdxady);

            if constexpr (Robust && !IsInteger)
            {
                Scalar permanent =
                    (predicates::Absolute(bdxcdy) + predicates::Absolute(cdxbdy)) * alift +
                    (predicates::Absolute(cdxady) + predicates::Absolute(adxcdy)) * blift +
                    (predicates::Absolute(adxbdy) + predicates::Absolute(bdxady)) * clift;

                Scalar errbound = predicates::errorBounds<Scalar>.iccerrboundA * permanent;
                if (predicates::Absolute(det) <= errbound) DETRIA_UNLIKELY
                {
                    Scalar pa[2]{ a.x, a.y };
                    Scalar pb[2]{ b.x, b.y };
                    Scalar pc[2]{ c.x, c.y };
                    Scalar pd[2]{ d.x, d.y };
                    det = predicates::incircleadapt(pa, pb, pc, pd, permanent);
                }
            }

            if (det > Scalar(0))
            {
                return CircleLocation::Inside;
            }
            else if (det < Scalar(0))
            {
                return CircleLocation::Outside;
            }
            else
            {
                return CircleLocation::Cocircular;
            }
        }
    }

    namespace memory
    {
        struct DefaultAllocator
        {
            template <typename T>
            struct StlAllocator
            {
                using value_type = T;
                using size_type = size_t;
                using difference_type = ptrdiff_t;

                StlAllocator()
                {
                }

                template <typename U>
                StlAllocator(const StlAllocator<U>&)
                {
                }

                inline T* allocate(size_t count)
                {
                    return new T[count];
                }

                inline void deallocate(T* ptr, size_t)
                {
                    delete[] ptr;
                }
            };

            template <typename T>
            inline StlAllocator<T> createStlAllocator()
            {
                return StlAllocator<T>();
            }
        };
    }

#define DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(collection) collection(allocator.template createStlAllocator<typename decltype(collection)::value_type>())

    namespace detail
    {
        // Simple zero-sized struct
        struct Empty { };

        template <typename T, typename Idx, template <typename, typename> typename Collection, typename Allocator>
        struct FlatLinkedList
        {
            // Simple cyclic doubly-linked list, but the elements are not allocated one-by-one
            // The indices of the deleted elements are stored in `free`

            struct Node
            {
                Idx prevId;
                Idx nextId;
                T data;
            };

            FlatLinkedList(Allocator allocator) :
                DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_nodes),
                DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_free)
            {
            }

            inline Idx create(const T& data)
            {
                Idx newId{ };
                Node* newNode = nullptr;
                if (_free.empty())
                {
                    newId = Idx(_nodes.size());
                    newNode = &_nodes.emplace_back();
                }
                else
                {
                    newId = _free.back();
                    _free.pop_back();
                    newNode = &_nodes[size_t(newId)];
                }

                newNode->data = data;
                newNode->prevId = newId;
                newNode->nextId = newId;

                return newId;
            }

            inline Idx addAfter(const Idx& afterId, const T& data)
            {
                Idx nodeId = create(data);
                Node& node = _nodes[size_t(nodeId)];
                Node& after = _nodes[size_t(afterId)];

                _nodes[size_t(after.nextId)].prevId = nodeId;
                node.nextId = after.nextId;
                after.nextId = nodeId;
                node.prevId = afterId;

                return nodeId;
            }

            inline Idx addBefore(const Idx& beforeId, const T& data)
            {
                Idx nodeId = create(data);
                Node& node = _nodes[size_t(nodeId)];
                Node& before = _nodes[size_t(beforeId)];

                _nodes[size_t(before.prevId)].nextId = nodeId;
                node.prevId = before.prevId;
                before.prevId = nodeId;
                node.nextId = before;

                return nodeId;
            }

            inline void remove(const Idx& nodeId)
            {
                Node& node = _nodes[size_t(nodeId)];
                _nodes[size_t(node.prevId)].nextId = node.nextId;
                _nodes[size_t(node.nextId)].prevId = node.prevId;

                _free.push_back(nodeId);
            }

            inline const Node& getNode(const Idx& nodeId) const
            {
                return _nodes[size_t(nodeId)];
            }

            inline size_t size() const
            {
                return _nodes.size();
            }

            inline void clear()
            {
                _nodes.clear();
                _free.clear();
            }

        private:
            Collection<Node, typename Allocator::template StlAllocator<Node>> _nodes;
            Collection<Idx, typename Allocator::template StlAllocator<Idx>> _free;
        };

        inline bool detriaAssert(bool condition, const char* message)
        {
            if (condition) DETRIA_LIKELY
            {
                return true;
            }

#ifdef NDEBUG
            (void)message;
#else
            // Only in debug mode

            std::cerr << "Assertion failed";
            if (message)
            {
                std::cerr << ": " << message;
            }

            std::cerr << std::endl;

#if defined(_WIN32) && _WIN32
            __debugbreak();
#else
#if defined(SIGTRAP)
            std::raise(SIGTRAP);
#endif
#endif

#endif
            return false;
        }

        // Similar to std::span, but without C++20, also much simpler
        template <typename T>
        struct ReadonlySpan
        {
            ReadonlySpan() : _ptr(nullptr), _count(0)
            {
            }

            ReadonlySpan(const T* ptr, size_t count) : _ptr(ptr), _count(count)
            {
            }

            template <template <typename, typename> typename Collection, typename Allocator>
            ReadonlySpan(const Collection<T, Allocator>& collection) : _ptr(collection.data()), _count(collection.size())
            {
            }

            inline size_t size() const
            {
                return _count;
            }

            inline void reset()
            {
                _ptr = nullptr;
                _count = 0;
            }

            inline const T& front() const
            {
                return _ptr[0];
            }

            inline const T& back() const
            {
                return _ptr[_count - 1];
            }

            inline const T& operator[](size_t index) const
            {
                return _ptr[index];
            }

        private:
            const T* _ptr;
            size_t _count;
        };

        template <typename T, size_t MaxSize>
        struct FixedSizeStack
        {
            inline void push(T elem)
            {
#ifndef NDEBUG
                detriaAssert(_size < MaxSize);
#endif
                _stack[_size++] = elem;
            }

            inline T pop()
            {
#ifndef NDEBUG
                detriaAssert(_size != 0);
#endif
                return _stack[--_size];
            }

            inline bool empty() const
            {
                return _size == 0;
            }

        private:
            std::array<T, MaxSize> _stack;
            size_t _size = 0;
        };

        struct DefaultSorter
        {
            template <typename Iter, typename Cmp>
            inline static void sort(Iter begin_, Iter end_, Cmp cmp)
            {
                using Ty = typename Iter::value_type;
                using Diff = std::ptrdiff_t;

                constexpr Diff insertionSortThreshold = 48;

                // Store sort ranges in a fixed-size stack to avoid recursion

                constexpr size_t sizeInBits = sizeof(Diff) * 8;
                constexpr size_t maxStackSize = sizeInBits * 2 + 2;

                using StackElement = std::pair<Diff, Diff>;
                FixedSizeStack<StackElement, maxStackSize> stack;

                stack.push(std::make_pair(0, std::distance(begin_, end_)));

                while (!stack.empty())
                {
                    auto [beginIndex, endIndex] = stack.pop();
                    Iter begin = std::next(begin_, beginIndex);
                    Iter end = std::next(begin_, endIndex);

                    Diff count = std::distance(begin, end);
                    if (count < insertionSortThreshold)
                    {
                        if (count >= 2)
                        {
                            insertionSort(begin, end, cmp);
                        }

                        continue;
                    }

                    // Find a good pivot

                    std::array<Ty, 5> possiblePivotValues{ };
                    possiblePivotValues[0] = *begin;
                    possiblePivotValues[1] = *std::next(begin, count / 4 * 1);
                    possiblePivotValues[2] = *std::next(begin, count / 4 * 2);
                    possiblePivotValues[3] = *std::next(begin, count / 4 * 3);
                    possiblePivotValues[4] = *std::prev(end);

                    insertionSort(possiblePivotValues.begin(), possiblePivotValues.end(), cmp);
                    Ty pivotValue = possiblePivotValues[2];

                    // Partition
                    // Using branchless Lomuto partitioning - https://orlp.net/blog/branchless-lomuto-partitioning/

                    Ty temp = *begin;
                    Diff otherIndex = 0;
                    for (Diff i = 0; i < count - 1; ++i)
                    {
                        Iter current = std::next(begin, i);
                        Iter other = std::next(begin, otherIndex);
                        *current = *other;
                        *other = *std::next(current);
                        bool cmpResult = cmp(*other, pivotValue);
                        otherIndex += cmpResult ? 1 : 0;
                    }

                    Iter other2 = std::next(begin, otherIndex);
                    *std::prev(end) = *other2;
                    *other2 = temp;

                    Diff pivotIndex = beginIndex + otherIndex + (cmp(temp, pivotValue) ? 1 : 0);

                    // Add partitioned ranges to the stack
                    // Add the longer range first, so the shorter range is processed first, which ensures that the stack is always large enough

                    std::pair firstRange = std::make_pair(beginIndex, pivotIndex);
                    std::pair secondRange = std::make_pair(pivotIndex, endIndex);
                    Diff firstRangeSize = pivotIndex - beginIndex;
                    Diff secondRangeSize = endIndex - pivotIndex;

                    if (firstRangeSize == 0 || secondRangeSize == 0) DETRIA_UNLIKELY
                    {
                        // When all values are equal (the comparer returns the same value for all elements), then one of the ranges can become empty
                        // This means that the values are already sorted
                        continue;
                    }

                    if (firstRangeSize < secondRangeSize)
                    {
                        stack.push(secondRange);
                        stack.push(firstRange);
                    }
                    else
                    {
                        stack.push(firstRange);
                        stack.push(secondRange);
                    }
                }
            }

        private:
            template <typename Iter, typename Cmp>
            inline static void insertionSort(Iter begin, Iter end, Cmp cmp)
            {
                using Diff = std::ptrdiff_t;

                // Assume that there are at least two elements
                // This is checked before calling this function
                Diff count = std::distance(begin, end);

                for (Diff i = 0; i < count - 1; ++i)
                {
                    for (Diff j = i + 1; j > 0; --j)
                    {
                        Iter a = std::next(begin, j - 1);
                        Iter b = std::next(begin, j);
                        if (!cmp(*a, *b))
                        {
                            std::swap(*a, *b);
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
        };

        // Helper to decide if a type has a `reserve` function
        template <template <typename, typename> typename, typename = void>
        struct HasReserve
        {
            static constexpr bool value = false;
        };

        template <template <typename, typename> typename T>
        struct HasReserve<T, std::void_t<decltype(T<char, memory::DefaultAllocator::StlAllocator<char>>().reserve(size_t()))>>
        {
            static constexpr bool value = true;
        };
    }

    template <typename Point, typename Idx>
    struct DefaultPointGetter
    {
        // PointGetter classes are used retrieve points using custom logic. They can also be lambda functions.
        // If a struct or a class is used, then it must have the following function:
        // `Point operator()(Idx) const`
        // 
        // The function must return a point type which has an `x` and an `y` field (e.g. detria::PointD or detria::PointF).
        // The function will be called with an index where 0 <= index < numPoints
        // 
        // For example, for a point class, which uses the index operator to get the components, a point getter class could look like this:
        /*
        struct PointGetter
        {
            detria::PointD operator()(uint32_t idx) const
            {
                const CustomPoint& p = _customPoints[idx];
                return detria::PointD{ .x = p[0], .y = p[1] };
            }

        private:
            std::vector<CustomPoint> _customPoints;
        };
        */
        // The returned point can be a const reference if you already have the points in a compatible format.


        // If you get an error here, then it's likely that the returned Point class has no field 'x'
        using Scalar = decltype(Point::x);

        // If you get an error here, then it's likely that the type of 'x' in the Point class is not a number type
        static_assert(std::is_arithmetic_v<Scalar>, "The Point type's x and y fields must be number types");

        // If you get an error here, then it's likely that the type of 'x' and 'y' in the Point class are different
        // (for example, if 'x' is float, then 'y' also must be a float, not a double)
        static_assert(std::is_same_v<Scalar, decltype(Point::y)>, "The Point type's x and y fields must be the same type");

        DefaultPointGetter(detail::ReadonlySpan<Point> points) : _points(points)
        {
        }

        const Point& operator()(Idx idx) const
        {
            return _points[size_t(idx)];
        }

    private:
        detail::ReadonlySpan<Point> _points;
    };

    namespace topology
    {
        template <
            typename Idx,
            typename VertexData,
            typename EdgeData,
            template <typename, typename> typename Collection,
            typename Allocator
        >
        struct Topology
        {
            // Using half-edges for storing vertex relations
            // Each vertex has outgoing half-edges (only the first edge is stored)
            // The rest of the edges going around a given vertex are stored in a doubly-linked list
            // The half-edges are also paired with an opposite edge, which references the other vertex
            // The triangles are not stored, since the topology is constructed in a way that the edges are not intersecting,
            // and the triangles can always be calculated from the stored data (the triangles are calculated at the final step of the triangulation)
            // For example:
            //
            //            v1  e13     e31  v3
            //             *------   ------*
            //            / \             /
            //       e10 /   \ e12       / e32
            //          /     \         /
            //
            //        /         \     /
            //   e01 /       e21 \   / e23
            //      /             \ /
            //  v0 *------   ------* v2
            //        e02     e20
            //
            // v0.firstEdge == e01
            // getOpposite(e01) == e10
            // getOpposite(e01).vertex == v1
            // e01.nextEdge == e02
            // e01.prevEdge == e02
            // e20.nextEdge == e21
            // e21.prevEdge == e23

            static constexpr Idx NullIndex = Idx(-1);

            static constexpr bool collectionHasReserve = detail::HasReserve<Collection>::value;

            struct VertexIndex
            {
                inline explicit VertexIndex(Idx index_) : index(index_)
                {
                }

                inline VertexIndex() : index(NullIndex)
                {
                }

                inline bool operator==(const VertexIndex& other) const
                {
                    return index == other.index;
                }

                inline bool operator!=(const VertexIndex& other) const
                {
                    return index != other.index;
                }

                inline bool isValid() const
                {
                    return index != NullIndex;
                }

                Idx index;
            };

            struct HalfEdgeIndex
            {
                inline explicit HalfEdgeIndex(Idx index_) : index(index_)
                {
                }

                inline HalfEdgeIndex() : index(NullIndex)
                {
                }

                inline bool operator==(const HalfEdgeIndex& other) const
                {
                    return index == other.index;
                }

                inline bool operator!=(const HalfEdgeIndex& other) const
                {
                    return index != other.index;
                }

                inline bool isValid() const
                {
                    return index != NullIndex;
                }

                Idx index;
            };

            struct Vertex
            {
                HalfEdgeIndex firstEdge = HalfEdgeIndex(NullIndex);

                VertexData data;
            };

            struct HalfEdge
            {
                VertexIndex vertex;
                HalfEdgeIndex prevEdge;
                HalfEdgeIndex nextEdge;

                EdgeData data;
            };

            Topology(Allocator allocator) :
                DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_vertices),
                DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_edges)
            {
            }

            inline void clear()
            {
                _vertices.clear();
                _edges.clear();
            }

            inline void reserveVertices(size_t capacity)
            {
                if constexpr (collectionHasReserve)
                {
                    _vertices.reserve(capacity);
                }
            }

            inline void reserveHalfEdges(size_t capacity)
            {
                if constexpr (collectionHasReserve)
                {
                    _edges.reserve(capacity);
                }
            }

            inline VertexIndex createVertex()
            {
                Idx idx = Idx(_vertices.size());

                _vertices.emplace_back();

                return VertexIndex(idx);
            }

            inline Vertex& getVertex(VertexIndex idx)
            {
                return _vertices[idx.index];
            }

            inline const Vertex& getVertex(VertexIndex idx) const
            {
                return _vertices[idx.index];
            }

            inline HalfEdge& getEdge(HalfEdgeIndex idx)
            {
                return _edges[idx.index];
            }

            inline const HalfEdge& getEdge(HalfEdgeIndex idx) const
            {
                return _edges[idx.index];
            }

            inline VertexData& getVertexData(VertexIndex idx)
            {
                return getVertex(idx).data;
            }

            inline const VertexData& getVertexData(VertexIndex idx) const
            {
                return getVertex(idx).data;
            }

            inline EdgeData& getEdgeData(HalfEdgeIndex idx)
            {
                return getEdge(idx).data;
            }

            inline const EdgeData& getEdgeData(HalfEdgeIndex idx) const
            {
                return getEdge(idx).data;
            }

            inline size_t vertexCount() const
            {
                return _vertices.size();
            }

            inline size_t halfEdgeCount() const
            {
                return _edges.size();
            }

            inline HalfEdgeIndex getOpposite(HalfEdgeIndex idx) const
            {
                return HalfEdgeIndex(idx.index ^ 1);
            }

            inline HalfEdgeIndex getEdgeBetween(VertexIndex a, VertexIndex b) const
            {
                HalfEdgeIndex existingEdge{ };

                forEachEdgeOfVertex(a, [&](HalfEdgeIndex edge)
                {
                    HalfEdgeIndex oppositeEdgeIndex = getOpposite(edge);
                    if (getEdge(oppositeEdgeIndex).vertex == b)
                    {
                        existingEdge = edge;
                        return false;
                    }

                    return true;
                });

                return existingEdge;
            }

            inline HalfEdgeIndex createNewEdge(VertexIndex a, VertexIndex b, HalfEdgeIndex addAAfterThisEdge, HalfEdgeIndex addBAfterThisEdge,
                HalfEdgeIndex reusedEdgeIndex = { })
            {
                // If the given edges are valid, the edge will be inserted after the given edges
                // Otherwise the new edge will be added as the first edge of the given vertex
                // This function should only be called if there is no edge between the given vertices

                // Half edges are always created in pairs, which means that the opposite index can always be retrieved by xor-ing the edge index with 1

                auto addHalfEdge = [this](HalfEdgeIndex newEdgeIndex, VertexIndex vertex, HalfEdgeIndex afterEdge)
                {
                    HalfEdge newEdge{ };

                    newEdge.vertex = vertex;

                    if (afterEdge.isValid())
                    {
                        // Valid index, insert between the edges

                        HalfEdge& originalEdge = getEdge(afterEdge);
                        HalfEdgeIndex beforeEdge = originalEdge.nextEdge;
                        HalfEdge& originalNextEdge = getEdge(beforeEdge);

                        newEdge.prevEdge = afterEdge;
                        newEdge.nextEdge = beforeEdge;

                        originalEdge.nextEdge = newEdgeIndex;
                        originalNextEdge.prevEdge = newEdgeIndex;
                    }
                    else
                    {
                        // Null index, set as first edge
                        Vertex& v = getVertex(vertex);
                        v.firstEdge = newEdgeIndex;

                        // Set references to self
                        newEdge.nextEdge = newEdgeIndex;
                        newEdge.prevEdge = newEdgeIndex;
                    }

                    return newEdge;
                };

                bool reusedEdge = reusedEdgeIndex.isValid();

                HalfEdgeIndex newEdgeIndexA{ };
                HalfEdgeIndex newEdgeIndexB{ };

                if (reusedEdge)
                {
                    newEdgeIndexA = reusedEdgeIndex;
                    newEdgeIndexB = getOpposite(reusedEdgeIndex);
                }
                else
                {
                    newEdgeIndexA = HalfEdgeIndex(Idx(_edges.size()));
                    newEdgeIndexB = HalfEdgeIndex(Idx(_edges.size() + 1));
                }

                HalfEdge newEdgeA = addHalfEdge(newEdgeIndexA, a, addAAfterThisEdge);
                HalfEdge newEdgeB = addHalfEdge(newEdgeIndexB, b, addBAfterThisEdge);

                if (reusedEdge)
                {
                    _edges[size_t(newEdgeIndexA.index)] = newEdgeA;
                    _edges[size_t(newEdgeIndexB.index)] = newEdgeB;
                }
                else
                {
                    _edges.emplace_back(std::move(newEdgeA));
                    _edges.emplace_back(std::move(newEdgeB));
                }

                return newEdgeIndexA;
            }

            inline HalfEdgeIndex createOrGetEdge(VertexIndex a, VertexIndex b, HalfEdgeIndex afterEdgeA, HalfEdgeIndex afterEdgeB)
            {
                HalfEdgeIndex existingEdge = getEdgeBetween(a, b);
                if (existingEdge.isValid())
                {
                    return *existingEdge;
                }

                return createNewEdge(a, b, afterEdgeA, afterEdgeB);
            }

            template <typename Callback>
            inline void forEachEdgeOfVertex(VertexIndex idx, Callback callback) const
            {
                const Vertex& v = getVertex(idx);
                if (!v.firstEdge.isValid())
                {
                    return;
                }

                HalfEdgeIndex firstEdge = v.firstEdge;
                HalfEdgeIndex currentEdgeIndex = firstEdge;

                constexpr bool callbackReturnsBoolean = std::is_same_v<std::invoke_result_t<Callback, HalfEdgeIndex>, bool>;

                do
                {
                    const HalfEdge& edge = getEdge(currentEdgeIndex);

                    if constexpr (callbackReturnsBoolean)
                    {
                        if (!callback(currentEdgeIndex))
                        {
                            return;
                        }
                    }
                    else
                    {
                        callback(currentEdgeIndex);
                    }

                    currentEdgeIndex = edge.nextEdge;
                }
                while (currentEdgeIndex != firstEdge);
            }

            inline void unlinkEdge(HalfEdgeIndex edgeIndex)
            {
                // Assuming the vertices of this edge have at least one other edge
                // The first edge of the vertices will not be set correctly otherwise

                HalfEdge& edge = getEdge(edgeIndex);
                HalfEdge& opposite = getEdge(getOpposite(edgeIndex));

                Vertex& v0 = getVertex(edge.vertex);
                Vertex& v1 = getVertex(opposite.vertex);
                v0.firstEdge = edge.nextEdge;
                v1.firstEdge = opposite.nextEdge;

                getEdge(edge.nextEdge).prevEdge = edge.prevEdge;
                getEdge(edge.prevEdge).nextEdge = edge.nextEdge;

                getEdge(opposite.nextEdge).prevEdge = opposite.prevEdge;
                getEdge(opposite.prevEdge).nextEdge = opposite.nextEdge;
            }

            inline void flipEdge(HalfEdgeIndex edgeIndex)
            {
                /*        0                 0
                 *        *                 *
                 *       /|\               / \
                 *      / | \             /   \
                 *   3 *  |  * 1  -->  3 *-----* 1
                 *      \ | /             \   /
                 *       \|/               \ /
                 *        *                 *
                 *        2                 2
		 */

                // Setup all references
                HalfEdge& edge = getEdge(edgeIndex);
                HalfEdgeIndex oppositeIndex = getOpposite(edgeIndex);
                HalfEdge& opposite = getEdge(oppositeIndex);

                HalfEdgeIndex e01i = edge.prevEdge;
                HalfEdgeIndex e03i = edge.nextEdge;
                HalfEdgeIndex e21i = opposite.nextEdge;
                HalfEdgeIndex e23i = opposite.prevEdge;
                HalfEdge& e01 = getEdge(e01i);
                HalfEdge& e03 = getEdge(e03i);
                HalfEdge& e21 = getEdge(e21i);
                HalfEdge& e23 = getEdge(e23i);

                HalfEdgeIndex e10i = getOpposite(e01i);
                HalfEdgeIndex e30i = getOpposite(e03i);
                HalfEdgeIndex e12i = getOpposite(e21i);
                HalfEdgeIndex e32i = getOpposite(e23i);
                HalfEdge& e10 = getEdge(e10i);
                HalfEdge& e30 = getEdge(e30i);
                HalfEdge& e12 = getEdge(e12i);
                HalfEdge& e32 = getEdge(e32i);

                Vertex& v0 = getVertex(edge.vertex);
                Vertex& v2 = getVertex(opposite.vertex);

                // Unlink the edge
                e01.nextEdge = e03i;
                e03.prevEdge = e01i;
                e23.nextEdge = e21i;
                e21.prevEdge = e23i;

                // Technically we only need to change firstEdge if it's the flipped edge, but we can avoid a branch if we always set it
                v0.firstEdge = e01i;
                v2.firstEdge = e23i;

                // Re-link edge as flipped
                edge.prevEdge = e12i;
                edge.nextEdge = e10i;
                e10.prevEdge = edgeIndex;
                e12.nextEdge = edgeIndex;

                opposite.prevEdge = e30i;
                opposite.nextEdge = e32i;
                e32.prevEdge = oppositeIndex;
                e30.nextEdge = oppositeIndex;

                edge.vertex = e10.vertex;
                opposite.vertex = e32.vertex;
            }

        private:
            Collection<Vertex, typename Allocator::template StlAllocator<Vertex>> _vertices;
            Collection<HalfEdge, typename Allocator::template StlAllocator<HalfEdge>> _edges;
        };
    }

    enum class TriangleLocation : uint8_t
    {
        // Inside an outline
        Interior = 1,

        // Part of a hole
        Hole = 2,

        // Not part of any outlines or holes, only part of the initial triangulation
        ConvexHull = 4,

        All = Interior | Hole | ConvexHull
    };

    enum class TriangulationError
    {
        // Triangulation was successful
        NoError,

        // The triangulation object was created, but no triangulation was performed yet
        TriangulationNotStarted,

        // Less than three points were added to the triangulation
        LessThanThreePoints,


        // Errors of the initial triangulation phase

        // The input points contained a NaN or infinite value
        NonFinitePositionFound,

        // The list of input points contained duplicates
        DuplicatePointsFound,

        // All of the input points were collinear, so no valid triangles could be created
        AllPointsAreCollinear,


        // Errors of constrained edge creation

        // A polyline (outline or hole) contained less than 3 points
        PolylineTooShort,

        // An index in a polyline was out-of-bounds (idx < 0 or idx >= number of point)
        PolylineIndexOutOfBounds,

        // Two consecutive points in a polyline were the same
        PolylineDuplicateConsecutivePoints,

        // An edge was part of both an outline and a hole
        EdgeWithDifferentConstrainedTypes,

        // A point was exactly on a constrained edge
        PointOnConstrainedEdge,

        // Two constrained edges were intersecting
        ConstrainedEdgeIntersection,


        // Errors of triangle classification

        // Found a hole which was not inside any outlines
        HoleNotInsideOutline,

        // An outline was directly inside another outline, or a hole was directly inside another hole
        StackedPolylines,


        // A condition that should always be true was false, this error indicates a bug in the code
        AssertionFailed
    };

#define DETRIA_ASSERT_MSG(cond, msg) do { if (!detail::detriaAssert((cond), msg)) DETRIA_UNLIKELY { return fail(TE_AssertionFailed{ }); } } while(0)
#define DETRIA_ASSERT(cond) DETRIA_ASSERT_MSG(cond, nullptr)

#ifndef NDEBUG
#define DETRIA_DEBUG_ASSERT(cond) detail::detriaAssert((cond))
#else
#define DETRIA_DEBUG_ASSERT(cond)
#endif

#define DETRIA_CHECK(cond) do { if (!(cond)) DETRIA_UNLIKELY { return false; } } while (0)

    template <typename Point, typename Idx>
    struct DefaultTriangulationConfig
    {
        // Default configuration for a triangulation.
        // To change the values below, you can inherit from this class, and override the things you want to change.
        // For example:
        /*

        struct MyTriangulationConfig : public DefaultTriangulationConfig<detria::PointD, uint32_t>
        {
            using Allocator = MyCustomAllocatorType;

            // The rest are used from the default configuration
        }

        void doStuff()
        {
            detria::Triangulation<detria::PointD, uint32_t, MyTriangulationConfig> tri(getMyCustomAllocator());
            ...
        }

        */

        // Used to retrieve points with possibly custom logic. See comment at `DefaultPointGetter` for more info.
        using PointGetter = DefaultPointGetter<Point, Idx>;

        // Custom allocator to use for collections.
        using Allocator = memory::DefaultAllocator;

        // Custom collection used during triangulation.
        template <typename T, typename Allocator>
        using Collection = std::vector<T, Allocator>;

        // Custom sorting algorithm.
        // To use a different algorithm, use a class which has a function with the following signature:
        /*
        template <typename Iter, typename Cmp>
        static void sort(Iter begin, Iter end, Cmp comparer);
        */
        // This signature is equivalent to `std::sort` with a custom comparer.
        using Sorter = detail::DefaultSorter;

        // Robust orientation and incircle tests guarantee correct results, even if the points are nearly collinear or cocircular.
        // Incircle tests are only used in delaunay triangulations.
        constexpr static bool UseRobustOrientationTests = true;
        constexpr static bool UseRobustIncircleTests = true;

        // If enabled, then all user-provided indices are checked, and if anything is invalid (out-of-bounds or negative indices,
        // or two consecutive duplicate indices), then an error is generated.
        // If disabled, then no checks are done, so if the input has any invalid indices, then it's undefined behavior (might crash).
        constexpr static bool IndexChecks = true;

        // If enabled, then all the input points are checked, and if any of the points have a NaN or infinity value, then an error is generated.
        // If there are no such values in the list of input points, then this can be disabled.
        constexpr static bool NaNChecks = true;
    };

    template <
        typename Point = PointD,
        typename Idx = uint32_t,
        typename Config = DefaultTriangulationConfig<Point, Idx>
    >
    class Triangulation
    {
    private:
        using PointGetter = typename Config::PointGetter;

        using Allocator = typename Config::Allocator;

        template <typename T, typename Allocator>
        using Collection = typename Config::template Collection<T, Allocator>;

        template <typename T>
        using CollectionWithAllocator = Collection<T, typename Allocator::template StlAllocator<T>>;

        using Vector2 = std::invoke_result_t<decltype(&PointGetter::operator()), const PointGetter&, Idx>; // Can be const reference

        using Scalar = decltype(std::decay_t<Vector2>::x);
        static_assert(std::is_same_v<Scalar, decltype(std::decay_t<Vector2>::y)>, "Point type must have `x` and `y` fields, and they must be the same type");

        static_assert(std::is_integral_v<Idx>, "Index type must be an integer type");

        static constexpr bool collectionHasReserve = detail::HasReserve<Collection>::value;

        using Tri = Triangle<Idx>;

        enum class EdgeType : uint8_t
        {
            NotConstrained = 0, ManuallyConstrained, AutoDetect, Outline, Hole, MAX_EDGE_TYPE
        };

        struct EdgeData
        {
            // An edge can be either:
            // - Not constrained
            // - Manually constrained
            // - Auto detected: will decide if it's an outline or a hole, depending on where it is
            // - Part of an outline or a hole; in this case, we need to store its index
            // Also store if the edge is delaunay and if it's boundary

            struct NotConstrainedEdgeTag {};
            struct ManuallyConstrainedEdgeTag {};

            struct OutlineOrHoleData
            {
                Idx polylineIndex;
                EdgeType type;
            };

            enum class Flags : uint8_t
            {
                None = 0,
                Delaunay = 1,
                Boundary = 2
            };

            inline bool isConstrained() const
            {
                return !std::holds_alternative<NotConstrainedEdgeTag>(data);
            }

            inline std::optional<Idx> getOutlineOrHoleIndex() const
            {
                if (const OutlineOrHoleData* holeData = std::get_if<OutlineOrHoleData>(&data))
                {
                    return holeData->polylineIndex;
                }
                else
                {
                    return { };
                }
            }

            inline EdgeType getEdgeType() const
            {
                EdgeType result{ };
                std::visit([&](const auto& value)
                {
                    using Ty = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Ty, NotConstrainedEdgeTag>)
                    {
                        result = EdgeType::NotConstrained;
                    }
                    else if constexpr (std::is_same_v<Ty, ManuallyConstrainedEdgeTag>)
                    {
                        result = EdgeType::ManuallyConstrained;
                    }
                    else
                    {
                        result = value.type;
                    }
                }, data);

                return result;
            }

            inline bool hasFlag(Flags flag) const
            {
                return Flags(uint8_t(flags) & uint8_t(flag)) != Flags::None;
            }

            inline void setFlag(Flags flag, bool value)
            {
                if (value)
                {
                    flags = Flags(uint8_t(flags) | uint8_t(flag));
                }
                else
                {
                    flags = Flags(uint8_t(flags) & ~uint8_t(flag));
                }
            }

            inline bool isDelaunay() const
            {
                return hasFlag(Flags::Delaunay);
            }

            inline void setDelaunay(bool value)
            {
                setFlag(Flags::Delaunay, value);
            }

            inline bool isBoundary() const
            {
                return hasFlag(Flags::Boundary);
            }

            inline void setBoundary(bool value)
            {
                setFlag(Flags::Boundary, value);
            }

            std::variant<NotConstrainedEdgeTag, ManuallyConstrainedEdgeTag, OutlineOrHoleData> data = NotConstrainedEdgeTag{ };
            Flags flags = Flags::None;
        };

        using Topology = topology::Topology<Idx, detail::Empty, EdgeData, Collection, Allocator>;
        using TVertex = typename Topology::VertexIndex;
        using THalfEdge = typename Topology::HalfEdgeIndex;
        using HalfEdge = typename Topology::HalfEdge;

        struct TriangleData
        {
            struct UnknownLocationTag {};

            struct KnownLocationData
            {
                // The index of the inner-most outline or hole that contains this triangle
                // nullopt if the triangle is outside of all polylines
                std::optional<Idx> parentPolylineIndex;
            };

            std::variant<UnknownLocationTag, KnownLocationData> locationData = UnknownLocationTag{ };
            THalfEdge firstEdge{ };
        };

        struct TriangleIndex
        {
            inline explicit TriangleIndex(Idx index_) : index(index_)
            {
            }

            inline TriangleIndex() : index(Topology::NullIndex)
            {
            }

            inline bool operator==(const TriangleIndex& other) const
            {
                return index == other.index;
            }

            inline bool operator!=(const TriangleIndex& other) const
            {
                return index != other.index;
            }

            inline bool isValid() const
            {
                return index != Topology::NullIndex;
            }

            Idx index;
        };

        struct PolylineData
        {
            detail::ReadonlySpan<Idx> pointIndices;
            EdgeType type{ };
        };

        using List = detail::FlatLinkedList<TVertex, Idx, Collection, Allocator>;

        // Triangulation error types

        struct TE_NoError
        {
            TriangulationError getError() const
            {
                return TriangulationError::NoError;
            }

            std::string getErrorMessage() const
            {
                return "The triangulation was successful";
            }
        };

        struct TE_NotStarted
        {
            TriangulationError getError() const
            {
                return TriangulationError::TriangulationNotStarted;
            }

            std::string getErrorMessage() const
            {
                return "The triangulation was not performed yet";
            }
        };

        struct TE_LessThanThreePoints
        {
            TriangulationError getError() const
            {
                return TriangulationError::LessThanThreePoints;
            }

            std::string getErrorMessage() const
            {
                return "At least 3 input points are required to perform the triangulation";
            }
        };

        struct TE_NonFinitePositionFound
        {
            TriangulationError getError() const
            {
                return TriangulationError::NonFinitePositionFound;
            }

            std::string getErrorMessage() const
            {
                std::stringstream ss;

                // This error can only happen with floating point types
                if constexpr (std::is_floating_point_v<Scalar>)
                {
                    ss << (std::isnan(value) ? "NaN" : "Infinite") << " value found at point index " << index;
                }

                return ss.str();
            }

            Idx index;
            Scalar value;
        };

        struct TE_DuplicatePointsFound
        {
            TriangulationError getError() const
            {
                return TriangulationError::DuplicatePointsFound;
            }

            std::string getErrorMessage() const
            {
                std::stringstream ss;
                ss << "Multiple input points had the same position (" << positionX << ", " << positionY
                    << "), at index " << idx0 << " and at index " << idx1;
                return ss.str();
            }

            Scalar positionX;
            Scalar positionY;
            Idx idx0;
            Idx idx1;
        };

        struct TE_AllPointsAreCollinear
        {
            TriangulationError getError() const
            {
                return TriangulationError::AllPointsAreCollinear;
            }

            std::string getErrorMessage() const
            {
                return "All input points were collinear";
            }
        };

        struct TE_PolylineTooShort
        {
            TriangulationError getError() const
            {
                return TriangulationError::PolylineTooShort;
            }

            std::string getErrorMessage() const
            {
                // Note: we don't really have an index to return, since the outlines and the holes are stored in the same vector
                return "An input polyline contained less than three points";
            }
        };

        struct TE_PolylineIndexOutOfBounds
        {
            TriangulationError getError() const
            {
                return TriangulationError::PolylineIndexOutOfBounds;
            }

            std::string getErrorMessage() const
            {
                std::stringstream ss;
                ss << "A polyline referenced a point at index " << pointIndex << ", which is not in range [0, " << (numPointsInPolyline - 1) << "]";
                return ss.str();
            }

            Idx pointIndex;
            Idx numPointsInPolyline;
        };

        struct TE_PolylineDuplicateConsecutivePoints
        {
            TriangulationError getError() const
            {
                return TriangulationError::PolylineDuplicateConsecutivePoints;
            }

            std::string getErrorMessage() const
            {
                std::stringstream ss;
                ss << "A polyline containes two duplicate consecutive points (index " << pointIndex << ")";
                return ss.str();
            }

            Idx pointIndex;
        };

        struct TE_EdgeWithDifferentConstrainedTypes
        {
            TriangulationError getError() const
            {
                return TriangulationError::EdgeWithDifferentConstrainedTypes;
            }

            std::string getErrorMessage() const
            {
                std::stringstream ss;
                ss << "The edge between points " << idx0 << " and " << idx1 << " is part of both an outline and a hole";
                return ss.str();
            }

            Idx idx0;
            Idx idx1;
        };

        struct TE_PointOnConstrainedEdge
        {
            TriangulationError getError() const
            {
                return TriangulationError::PointOnConstrainedEdge;
            }

            std::string getErrorMessage() const
            {
                std::stringstream ss;
                ss << "Point " << pointIndex << " is exactly on a constrained edge (between points "
                    << edgePointIndex0 << " and " << edgePointIndex1 << ")";
                return ss.str();
            }

            Idx pointIndex;
            Idx edgePointIndex0;
            Idx edgePointIndex1;
        };

        struct TE_ConstrainedEdgeIntersection
        {
            TriangulationError getError() const
            {
                return TriangulationError::ConstrainedEdgeIntersection;
            }

            std::string getErrorMessage() const
            {
                std::stringstream ss;
                ss << "Constrained edges are intersecting: first edge between points " << idx0 << " and " << idx1
                    << ", second edge between points " << idx2 << " and " << idx3;
                return ss.str();
            }

            Idx idx0;
            Idx idx1;
            Idx idx2;
            Idx idx3;
        };

        struct TE_HoleNotInsideOutline
        {
            TriangulationError getError() const
            {
                return TriangulationError::HoleNotInsideOutline;
            }

            std::string getErrorMessage() const
            {
                return "Found a hole which was not inside any outlines";
            }
        };

        struct TE_StackedPolylines
        {
            TriangulationError getError() const
            {
                return TriangulationError::StackedPolylines;
            }

            std::string getErrorMessage() const
            {
                return isHole
                    ? "A hole was directly inside another hole"
                    : "An outline was directly inside another outline";
            }

            bool isHole;
        };

        struct TE_AssertionFailed
        {
            TriangulationError getError() const
            {
                return TriangulationError::AssertionFailed;
            }

            std::string getErrorMessage() const
            {
                return "Internal error, this is a bug in the code";
            }
        };

        using TriangulationErrorData = std::variant<
            TE_NoError,
            TE_NotStarted,
            TE_LessThanThreePoints,
            TE_NonFinitePositionFound,
            TE_DuplicatePointsFound,
            TE_AllPointsAreCollinear,
            TE_PolylineTooShort,
            TE_PolylineIndexOutOfBounds,
            TE_PolylineDuplicateConsecutivePoints,
            TE_EdgeWithDifferentConstrainedTypes,
            TE_PointOnConstrainedEdge,
            TE_ConstrainedEdgeIntersection,
            TE_HoleNotInsideOutline,
            TE_StackedPolylines,
            TE_AssertionFailed
        >;

    public:

        Triangulation(Allocator allocator = { }) :
            _allocator(allocator),
            _pointGetter(),
            _numPoints(0),
            DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_polylines),
            DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_manuallyConstrainedEdges),
            DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_autoDetectedPolylineTypes),
            _topology(allocator),
            DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_initialTriangulation_SortedPoints),
            DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_constrainedEdgeVerticesCW),
            DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_constrainedEdgeVerticesCCW),
            DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_deletedConstrainedEdges),
            DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_constrainedEdgeReTriangulationStack),
            DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_classifyTriangles_TrianglesByHalfEdgeIndex),
            DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_classifyTriangles_CheckedHalfEdges),
            DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_classifyTriangles_CheckedTriangles),
            DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_classifyTriangles_TrianglesToCheck),
            DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_resultTriangles),
            _convexHullPoints(allocator),
            DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_parentPolylines),
            DETRIA_INIT_COLLECTION_WITH_ALLOCATOR(_delaunayCheckStack)
        {
        }

        Triangulation(const Triangulation&) = delete;
        Triangulation operator=(const Triangulation&) = delete;

        // If the triangulation failed, this function returns the type of the error that occured
        TriangulationError getError() const
        {
            TriangulationError err{ };
            std::visit([&](const auto& errorData)
            {
                err = errorData.getError();
            }, _error);
            return err;
        }

        // Returns a human-readable message about the last triangulation error (if any)
        std::string getErrorMessage() const
        {
            std::string msg;
            std::visit([&](const auto& errorData)
            {
                msg = std::move(errorData.getErrorMessage());
            }, _error);
            return msg;
        }

        // Clears all data of the triangulation, allowing the triangulation object to be reused
        void clear()
        {
            _pointGetter.reset();
            _numPoints = 0;
            _polylines.clear();
            _manuallyConstrainedEdges.clear();

            clearInternalData();
        }

        // Set all points which will be used for the triangulation
        // The points are not copied, so they must be valid for the duration of the triangulation
        // The result triangles will be indices of these points
        // If a point should be part of an outline, then use `addOutline`
        // If a point should be part of a hole, then use `addHole`
        // The rest of the points will be steiner points
        // If you're using a custom point getter which cannot be constructed from `points`, then use the `setPointGetter` function instead
        template <typename Points>
        void setPoints(const Points& points)
        {
            _pointGetter = PointGetter(points);
            _numPoints = points.size();
        }

        void setPointGetter(PointGetter pointGetter, size_t numPoints)
        {
            _pointGetter.emplace(pointGetter);
            _numPoints = numPoints;
        }

        // `addOutline`, `addHole`, and `addPolylineAutoDetectType` return the polyline's index,
        // which can be used to get its parent, using `getParentPolylineIndex`

        // Add an outline - regions surrounded by outlines are "solid", and will be part of the "inside" triangles
        Idx addOutline(detail::ReadonlySpan<Idx> outline)
        {
            Idx id = Idx(_polylines.size());

            _polylines.push_back(PolylineData
            {
                outline,
                EdgeType::Outline
            });

            return id;
        }

        // Add a hole - holes will be subtracted from the final "solid", and will be part of the "outside" triangles
        Idx addHole(detail::ReadonlySpan<Idx> hole)
        {
            Idx id = Idx(_polylines.size());

            _polylines.push_back(PolylineData
            {
                hole,
                EdgeType::Hole
            });

            return id;
        }

        // Add a polyline, and automatically decide if it's an outline or a hole
        Idx addPolylineAutoDetectType(detail::ReadonlySpan<Idx> polyline)
        {
            Idx id = Idx(_polylines.size());

            _polylines.push_back(PolylineData
            {
                polyline,
                EdgeType::AutoDetect
            });

            return id;
        }

        // Set a single constrained edge, which will be part of the final triangulation
        void setConstrainedEdge(const Idx& idxA, const Idx& idxB)
        {
            _manuallyConstrainedEdges.push_back({ idxA, idxB });
        }

        // Perform the triangulation
        // Returns true if the triangulation succeeded, false otherwise
        [[nodiscard]]
        bool triangulate(bool delaunay)
        {
            clearInternalData();

            if constexpr (collectionHasReserve)
            {
                // Guess capacity, 64 should be a good starting point
                constexpr Idx initialCapacity = 64;
                _constrainedEdgeVerticesCW.reserve(initialCapacity);
                _constrainedEdgeVerticesCCW.reserve(initialCapacity);
                _constrainedEdgeReTriangulationStack.reserve(initialCapacity);

                if (delaunay)
                {
                    _delaunayCheckStack.reserve(initialCapacity);
                }
            }

            if (!triangulateInternal(delaunay))
            {
                // If failed, then clear topology and convex hull points, so no invalid triangulation is returned
                _topology.clear();
                _convexHullPoints.clear();
                _resultTriangles.clear();
                return false;
            }

            _error = TE_NoError{ };
            return true;
        }

        // Iterate over every interior triangle of the triangulation
        template <typename Callback>
        void forEachTriangle(Callback&& callback, bool cwTriangles = true) const
        {
            forEachTriangleInternal(callback, cwTriangles, [&](size_t triIndex)
            {
                return getTriangleLocation(triIndex) == TriangleLocation::Interior;
            });
        }

        // Iterate over every hole triangle of the triangulation
        template <typename Callback>
        void forEachHoleTriangle(Callback&& callback, bool cwTriangles = true) const
        {
            forEachTriangleInternal(callback, cwTriangles, [&](size_t triIndex)
            {
                return getTriangleLocation(triIndex) == TriangleLocation::Hole;
            });
        }

        // Iterate over every single triangle of the triangulation, even convex hull triangles
        template <typename Callback>
        void forEachTriangleOfEveryLocation(Callback&& callback, bool cwTriangles = true) const
        {
            forEachTriangleInternal(callback, cwTriangles, [](size_t) { return true; });
        }

        // Iterate over every triangle of a given location
        // Locations can be combined, to iterate over e.g. both interior and hole triangles
        template <typename Callback>
        void forEachTriangleOfLocation(Callback&& callback, const TriangleLocation& locationMask, bool cwTriangles = true) const
        {
            uint8_t mask = static_cast<uint8_t>(locationMask);

            for (size_t i = 0; i < _resultTriangles.size(); ++i)
            {
                TriangleLocation location = getTriangleLocation(i);
                bool shouldProcess = (uint8_t(location) & mask) != 0;
                if (shouldProcess)
                {
                    if (cwTriangles)
                    {
                        callback(getTriangleOriginalIndices<false>(i), location);
                    }
                    else
                    {
                        callback(getTriangleOriginalIndices<true>(i), location);
                    }
                }
            }
        }

        // Iterate over the vertices (vertex indices) in the convex hull
        // The vertices are in clockwise order
        template <typename Callback>
        void forEachConvexHullVertex(Callback&& callback)
        {
            if (_convexHullPoints.size() == 0 || _convexHullStartIndex == Idx(-1))
            {
                return;
            }

            Idx nodeId = _convexHullStartIndex;
            do
            {
                const typename List::Node& currentNode = _convexHullPoints.getNode(nodeId);
                callback(currentNode.data.index);
                nodeId = currentNode.nextId;
            }
            while (nodeId != _convexHullStartIndex);
        }

        // Returns the index of the parent of this polyline (the parent directly contains this polyline)
        // Returns nullopt for top-level outlines without parent (and for out-of-range index)
        std::optional<Idx> getParentPolylineIndex(Idx polylineIndex)
        {
            if (polylineIndex >= 0 && size_t(polylineIndex) < _parentPolylines.size())
            {
                return _parentPolylines[size_t(polylineIndex)];
            }

            return { };
        }

    private:
        bool fail(const TriangulationErrorData& errorData)
        {
            _error = errorData;
            return false;
        }

        void clearInternalData()
        {
            _topology.clear();
            _resultTriangles.clear();
            _initialTriangulation_SortedPoints.clear();
            _constrainedEdgeVerticesCW.clear();
            _constrainedEdgeVerticesCCW.clear();
            _deletedConstrainedEdges.clear();
            _constrainedEdgeReTriangulationStack.clear();
            _classifyTriangles_TrianglesByHalfEdgeIndex.clear();
            _classifyTriangles_CheckedHalfEdges.clear();
            _classifyTriangles_CheckedTriangles.clear();
            _classifyTriangles_TrianglesToCheck.clear();
            _delaunayCheckStack.clear();
            _convexHullPoints.clear();
            _convexHullStartIndex = Idx(-1);
            _parentPolylines.clear();
            _autoDetectedPolylineTypes.clear();
        }

        inline Vector2 getPoint(Idx pointIndex) const
        {
            return (*_pointGetter)(pointIndex);
        }

        bool triangulateInternal(bool delaunay)
        {
            if (!_pointGetter.has_value() || _numPoints < 3) DETRIA_UNLIKELY
            {
                // Need at least 3 points to triangulate
                return fail(TE_LessThanThreePoints{ });
            }

            if constexpr (Config::NaNChecks && std::is_floating_point_v<Scalar>)
            {
                // Check NaN / inf values
                for (Idx i = 0; i < Idx(_numPoints); ++i)
                {
                    Vector2 p = getPoint(i);
                    if (!std::isfinite(p.x) || !std::isfinite(p.y)) DETRIA_UNLIKELY
                    {
                        return fail(TE_NonFinitePositionFound
                        {
                            i,
                            std::isfinite(p.x) ? p.y : p.x
                        });
                    }
                }
            }

            // Reserve topology capacity
            _topology.reserveVertices(_numPoints);
            size_t numTriangles = _numPoints * 2; // Guess number of triangles

            // Number of edges should be around number of vertices + number of triangles
            _topology.reserveHalfEdges((_numPoints + numTriangles) * 2);

            if constexpr (collectionHasReserve)
            {
                _resultTriangles.reserve(numTriangles);
            }

            // Only create the vertices for now
            for (size_t i = 0; i < _numPoints; ++i)
            {
                _topology.createVertex();
            }

            TVertex convexHullVertex0{ };
            TVertex convexHullVertex1{ };

            // Initial triangulation, will add the edges
            DETRIA_CHECK(createInitialTriangulation(delaunay, convexHullVertex0, convexHullVertex1));

            // Add constrained edges, which ensures that all required vertices have edges between them
            DETRIA_CHECK(createConstrainedEdges(delaunay));

            // Go through all triangles, and decide if they are inside or outside
            DETRIA_CHECK(classifyTriangles(_topology.getEdgeBetween(convexHullVertex0, convexHullVertex1)));
            
            return true;
        }

        bool createInitialTriangulation(bool delaunay, TVertex& convexHullVertex0, TVertex& convexHullVertex1)
        {
            // Initialize point data
            CollectionWithAllocator<Idx>& sortedPoints = _initialTriangulation_SortedPoints;
            sortedPoints.resize(_numPoints);
            for (size_t i = 0; i < _numPoints; ++i)
            {
                sortedPoints[i] = Idx(i);
            }

            // Initial triangulation - just a valid triangulation that includes all points

            // Sort all points by x coordinates; for points with the same x coordinate, sort by y
            // If there are two points (or more) that have the same x and y coordinate, then we can't triangulate it,
            // because it would create degenerate triangles

            Config::Sorter::sort(sortedPoints.begin(), sortedPoints.end(), [&](const Idx& idxA, const Idx& idxB)
            {
                Vector2 a = getPoint(idxA);
                Vector2 b = getPoint(idxB);
                bool differentX = a.x != b.x;
                return differentX ? a.x < b.x : a.y < b.y;
            });

            // Since the points are sorted, duplicates must be next to each other (if any)
            for (size_t i = 1; i < _numPoints; ++i)
            {
                Idx prevIndex = sortedPoints[i - 1];
                Idx currentIndex = sortedPoints[i];

                Vector2 prev = getPoint(prevIndex);
                Vector2 current = getPoint(currentIndex);

                if (prev.x == current.x && prev.y == current.y) DETRIA_UNLIKELY
                {
                    return fail(TE_DuplicatePointsFound
                    {
                        current.x,
                        current.y,
                        prevIndex,
                        currentIndex
                    });
                }
            }

            // Find an initial triangle
            // Since we have no duplicate points, we can always use the first two points as the triangle's points
            // For the third point, we need to find one so that the first three points are not collinear

            const Idx& p0Idx = sortedPoints[0];
            const Idx& p1Idx = sortedPoints[1];
            Vector2 p0Position = getPoint(p0Idx);
            Vector2 p1Position = getPoint(p1Idx);

            size_t firstNonCollinearIndex = 0; // Index to the list of sorted points, not the original points
            math::Orientation triangleOrientation = math::Orientation::Collinear;
            for (size_t i = 2; i < sortedPoints.size(); ++i)
            {
                const Idx& currentIdx = sortedPoints[i];
                Vector2 currentPosition = getPoint(currentIdx);

                triangleOrientation = orient2d(p0Position, p1Position, currentPosition);
                if (triangleOrientation != math::Orientation::Collinear)
                {
                    firstNonCollinearIndex = i;
                    break;
                }
            }

            if (triangleOrientation == math::Orientation::Collinear)
            {
                // All points are collinear, and cannot be triangulated
                return fail(TE_AllPointsAreCollinear{ });
            }

            const Idx& p2Idx = sortedPoints[firstNonCollinearIndex];

            // We have a triangle now, so start adding the remaining points to the triangulation
            // Also keep track of the convex hull

            using ListNode = typename List::Node;

            // Make sure that the triangles are clockwise

            TVertex v0{ };
            TVertex v1{ };
            TVertex v2(p2Idx);

            if (triangleOrientation == math::Orientation::CW)
            {
                v0 = TVertex(p0Idx);
                v1 = TVertex(p1Idx);
            }
            else
            {
                v0 = TVertex(p1Idx);
                v1 = TVertex(p0Idx);
            }

            // Create initial half-edges
            {
                //        v0
                //        *
                //
                //
                //
                // v2 *       * v1

                THalfEdge e01 = _topology.createNewEdge(v0, v1, { }, { });
                THalfEdge e10 = _topology.getOpposite(e01);

                //        v0
                //        *
                //         \ e01
                // 
                //           \ e10
                // v2 *       * v1

                THalfEdge e02 = _topology.createNewEdge(v0, v2, e01, { });
                THalfEdge e20 = _topology.getOpposite(e02);

                //        v0
                //        *
                //   e02 / \ e01
                // 
                // e20 /     \ e10
                // v2 *       * v1

                THalfEdge e12 = _topology.createNewEdge(v1, v2, e10, e20);
                THalfEdge e21 = _topology.getOpposite(e12);

                //        v0
                //        *
                //   e02 / \ e01
                // 
                // e20 /     \ e10
                // v2 *--- ---* v1
                //     e21 e12

                if (delaunay)
                {
                    // Mark initial edges as delaunay
                    // Mark outer part of half-edges as boundary

                    auto markEdge = [&](THalfEdge edge, bool isBoundary)
                    {
                        EdgeData& edgeData = _topology.getEdgeData(edge);
                        edgeData.setDelaunay(true);
                        edgeData.setBoundary(isBoundary);
                    };

                    markEdge(e01, false);
                    markEdge(e10, true);
                    markEdge(e02, true);
                    markEdge(e20, false);
                    markEdge(e12, false);
                    markEdge(e21, true);
                }
            }

            // Add indices to convex hull
            Idx firstPointId = _convexHullPoints.create(v0);
            Idx secondPointId = _convexHullPoints.addAfter(firstPointId, v1);
            Idx lastPointId = _convexHullPoints.addAfter(secondPointId, v2);

            auto addPoint = [&](size_t sortedPointIdx)
            {
                const Idx& originalIndex = sortedPoints[sortedPointIdx];
                Vector2 position = getPoint(originalIndex);

                auto isEdgeVisible = [&](const Idx& nodeId)
                {
                    // Decide if the edge (which is given by the current and the next vertex) is visible from the current point

                    const ListNode& node = _convexHullPoints.getNode(nodeId);
                    const ListNode& next = _convexHullPoints.getNode(node.nextId);

                    math::Orientation orientation = orient2d(
                        getPoint(node.data.index),
                        getPoint(next.data.index),
                        position
                    );

                    // Don't consider point as visible if exactly on the line
                    return orientation == math::Orientation::CCW;
                };

                // Start checking edges, and find the first and last one that is visible from the current point
                // `lastPointId` is guaranteed to be visible, so it's a good starting point

                Idx lastVisibleForwards = lastPointId;
                Idx lastVisibleBackwards = _convexHullPoints.getNode(lastPointId).prevId;

                // Check forwards
                while (true)
                {
                    if (isEdgeVisible(lastVisibleForwards))
                    {
                        lastVisibleForwards = _convexHullPoints.getNode(lastVisibleForwards).nextId;
                    }
                    else
                    {
                        break;
                    }
                }

                // Check backwards
                while (true)
                {
                    if (isEdgeVisible(lastVisibleBackwards))
                    {
                        lastVisibleBackwards = _convexHullPoints.getNode(lastVisibleBackwards).prevId;
                    }
                    else
                    {
                        lastVisibleBackwards = _convexHullPoints.getNode(lastVisibleBackwards).nextId;
                        break;
                    }
                }

                DETRIA_ASSERT_MSG(lastVisibleForwards != lastVisibleBackwards, "No visible edges found");

                TVertex pVertex(originalIndex);

                // Add new edges
                // If delaunay, then add edges that we are about to remove to the list of edges to check
                Idx current = lastVisibleBackwards;
                THalfEdge lastAddedEdge{ };

                while (current != lastVisibleForwards)
                {
                    const ListNode& currentNode = _convexHullPoints.getNode(current);

                    TVertex currentVertex = currentNode.data;
                    TVertex nextVertex = _convexHullPoints.getNode(currentNode.nextId).data;

                    THalfEdge edge0i = _topology.getEdgeBetween(currentVertex, nextVertex);
                    HalfEdge& edge0 = _topology.getEdge(edge0i);

                    THalfEdge edge1i = lastAddedEdge.isValid()
                        ? lastAddedEdge
                        : _topology.createNewEdge(pVertex, currentVertex, { }, edge0.prevEdge);
                    HalfEdge& edge1 = _topology.getEdge(edge1i);

                    THalfEdge oppositeEdge0i = _topology.getOpposite(edge0i);
                    THalfEdge edge2i = _topology.createNewEdge(pVertex, nextVertex, edge1.prevEdge, oppositeEdge0i);

                    // Update boundary status
                    _topology.getEdgeData(oppositeEdge0i).setBoundary(false); // Previously boundary edge, but it became interior now

                    if (!lastAddedEdge.isValid())
                    {
                        // Mark first edge of the newly added vertex as boundary
                        edge1.data.setBoundary(true);
                    }

                    lastAddedEdge = edge2i;

                    if (delaunay)
                    {
                        // Make sure the newly added triangle meets the delaunay criteria
                        DETRIA_CHECK(delaunayEdgeFlip(pVertex, edge0i));
                    }

                    current = currentNode.nextId;
                }

                // Finally, mark lastAddedEdge as boundary
                {
                    _topology.getEdgeData(_topology.getOpposite(lastAddedEdge)).setBoundary(true);
                }

                // Remove vertices from convex hull if needed
                current = _convexHullPoints.getNode(lastVisibleBackwards).nextId;
                while (current != lastVisibleForwards)
                {
                    Idx next = _convexHullPoints.getNode(current).nextId;
                    _convexHullPoints.remove(current);
                    current = next;
                }

                // Add new vertex to convex hull
                lastPointId = _convexHullPoints.addAfter(lastVisibleBackwards, pVertex);

                return true;
            };

            Idx rightMostConvexHullPointAtStart = lastPointId;

            // Add points up to `firstNonCollinearIndex`
            for (size_t i = 2; i < firstNonCollinearIndex; ++i)
            {
                DETRIA_CHECK(addPoint(i));
            }

            if (firstNonCollinearIndex != 2)
            {
                // If the first three (or more) points were collinear, then `lastPointId` might not be the right-most point
                // So make sure that it is the right-most

                auto getXCoord = [&](const Idx& id)
                {
                    return getPoint(_convexHullPoints.getNode(id).data.index).x;
                };

                if (getXCoord(lastPointId) < getXCoord(rightMostConvexHullPointAtStart))
                {
                    lastPointId = rightMostConvexHullPointAtStart;
                }
            }

            // Skip `firstNonCollinearIndex`, add the rest of the points
            for (size_t i = firstNonCollinearIndex + 1; i < sortedPoints.size(); ++i)
            {
                DETRIA_CHECK(addPoint(i));
            }

            // Store an edge of the outline for later
            // Also, don't store the edge, because it's possible that an edge is deleted and recreated with a different id later
            // Storing two vertices guarantees that the edge will be valid later too
            _convexHullStartIndex = lastPointId;
            const ListNode& firstConvexHullNode = _convexHullPoints.getNode(_convexHullStartIndex);
            const ListNode& secondConvexHullNode = _convexHullPoints.getNode(firstConvexHullNode.nextId);
            convexHullVertex0 = firstConvexHullNode.data;
            convexHullVertex1 = secondConvexHullNode.data;

            return true;
        }

        // Use `_delaunayCheckStack` in this function
        // It will always be empty when this function returns
        bool delaunayEdgeFlip(const TVertex& justAddedVertex, const THalfEdge& oppositeEdge)
        {
            auto addEdgeToCheck = [&](THalfEdge e)
            {
                HalfEdge& edge = _topology.getEdge(e);
                HalfEdge& opposite = _topology.getEdge(_topology.getOpposite(e));

                edge.data.setDelaunay(false);
                opposite.data.setDelaunay(false);

                _delaunayCheckStack.emplace_back(TopologyEdgeWithVertices
                {
                    edge.vertex,
                    opposite.vertex,
                    e
                });
            };

            addEdgeToCheck(oppositeEdge);

            // Flip edges
            while (!_delaunayCheckStack.empty())
            {
                TopologyEdgeWithVertices edgeWithVertices = _delaunayCheckStack.back();
                _delaunayCheckStack.pop_back();

                THalfEdge e01 = edgeWithVertices.edge;
                HalfEdge& edge01 = _topology.getEdge(e01);
                HalfEdge& edge10 = _topology.getEdge(_topology.getOpposite(e01));

                // Check if the edge still has the same vertices
                if (edge01.vertex != edgeWithVertices.v0 || edge10.vertex != edgeWithVertices.v1) DETRIA_UNLIKELY
                {
                    // Edge was flipped
                    continue;
                }

                EdgeData& edgeData = edge01.data;
                EdgeData& oppositeEdgeData = edge10.data;
                if (edgeData.isDelaunay() || edgeData.isConstrained() || oppositeEdgeData.isDelaunay() || oppositeEdgeData.isConstrained())
                {
                    // Don't flip edges that are already delaunay or constrained
                    continue;
                }

                if (edgeData.isBoundary() || oppositeEdgeData.isBoundary())
                {
                    // Also don't flip boundary edges
                    edgeData.setDelaunay(true);
                    oppositeEdgeData.setDelaunay(true);
                    continue;
                }

                TVertex vertex0 = edge01.vertex;
                TVertex vertex1 = edge10.vertex;
                TVertex otherVertex0 = _topology.getEdge(_topology.getOpposite(edge01.nextEdge)).vertex;
                TVertex otherVertex1 = _topology.getEdge(_topology.getOpposite(edge10.nextEdge)).vertex;

                if (otherVertex0 == otherVertex1) DETRIA_UNLIKELY
                {
                    // This can happen during re-triangulation around a constrained edge
                    // This just means that the edge is boundary, so skip it
                    continue;
                }

                /*

         vertex0     otherVertex1
                *---*
                |\  |
                | \ |
                |  \|
                *---*
    otherVertex0     vertex1

                */

                // Points of the current edge
                Vector2 vertex0Position = getPoint(vertex0.index);
                Vector2 vertex1Position = getPoint(vertex1.index);

                // Points of the edge that we'd get if a flip is needed
                Vector2 otherVertex0Position = getPoint(otherVertex0.index);
                Vector2 otherVertex1Position = getPoint(otherVertex1.index);

                // TODO?: maybe we could allow user-defined functions to decide if an edge should be flipped
                // That would enable other metrics, e.g. minimize edge length, flip based on the aspect ratio of the triangles, etc.
                // https://people.eecs.berkeley.edu/~jrs/papers/elemj.pdf
                // But we'd need to make sure that every edge is only processed once

                math::CircleLocation loc = incircle(vertex0Position, vertex1Position, otherVertex1Position, otherVertex0Position);
                if (loc == math::CircleLocation::Inside)
                {
                    // Flip edge
                    // The edge is always flippable if we get here, no need to do orientation checks

                    _topology.flipEdge(e01);

                    // Flipping an edge might require other edges to be flipped too
                    // Only check edges which can be non-delaunay
                    if (justAddedVertex.index == otherVertex0.index)
                    {
                        addEdgeToCheck(edge01.prevEdge);
                        addEdgeToCheck(edge01.nextEdge);
                    }
                    else
                    {
                        DETRIA_DEBUG_ASSERT(justAddedVertex.index == otherVertex1.index);
                        addEdgeToCheck(edge10.nextEdge);
                        addEdgeToCheck(edge10.prevEdge);
                    }
                }

                edgeData.setDelaunay(true);
                oppositeEdgeData.setDelaunay(true);
            }

            return true;
        }

        bool createConstrainedEdges(bool delaunay)
        {
            // Ensure that an edge exists between each consecutive vertex for all outlines and holes

            for (size_t i = 0; i < _polylines.size(); ++i)
            {
                const PolylineData& polylineData = _polylines[i];

                detail::ReadonlySpan<Idx> polyline = polylineData.pointIndices;
                EdgeType constrainedEdgeType = polylineData.type;

                if (polyline.size() < 3) DETRIA_UNLIKELY
                {
                    return fail(TE_PolylineTooShort{ });
                }

                Idx prevVertexIdx{ };
                size_t startingIndex{ };

                // We allow polylines to have the same start and end vertex, e.g. [0, 1, 2, 3, 0]

                if (polyline.front() == polyline.back())
                {
                    if (polyline.size() < 4)
                    {
                        // [0, 1, 0] is invalid, even though it has 3 elements
                        return fail(TE_PolylineTooShort{ });
                    }

                    prevVertexIdx = polyline.front();
                    startingIndex = 1;
                }
                else
                {
                    prevVertexIdx = polyline.back();
                    startingIndex = 0;
                }

                for (size_t j = startingIndex; j < polyline.size(); ++j)
                {
                    const Idx& currentIdx = polyline[j];

                    DETRIA_CHECK(constrainSingleEdge(prevVertexIdx, currentIdx, constrainedEdgeType, Idx(i), delaunay));

                    prevVertexIdx = currentIdx;
                }
            }

            for (const Vec2<Idx>& edge : _manuallyConstrainedEdges)
            {
                DETRIA_CHECK(constrainSingleEdge(edge.x, edge.y, EdgeType::ManuallyConstrained, Idx(-1), delaunay));
            }

            return true;
        }

        static constexpr std::array<uint8_t, size_t(EdgeType::MAX_EDGE_TYPE)> getConstrainedEdgeTypePriorities()
        {
            std::array<uint8_t, size_t(EdgeType::MAX_EDGE_TYPE)> priorities{ };
            priorities[size_t(EdgeType::NotConstrained)] = 0;
            priorities[size_t(EdgeType::ManuallyConstrained)] = 1;
            priorities[size_t(EdgeType::AutoDetect)] = 2;
            priorities[size_t(EdgeType::Outline)] = 3;
            priorities[size_t(EdgeType::Hole)] = 3;

            return priorities;
        }

        bool constrainSingleEdge(const Idx& idxA, const Idx& idxB, const EdgeType& constrainedEdgeType, const Idx& polylineIndex, bool delaunay)
        {
            if constexpr (Config::IndexChecks)
            {
                if (idxA < Idx(0) || idxA >= Idx(_numPoints) || idxB < Idx(0) || idxB >= Idx(_numPoints)) DETRIA_UNLIKELY
                {
                    return fail(TE_PolylineIndexOutOfBounds
                    {
                        (idxA < Idx(0) || idxA >= Idx(_numPoints)) ? idxA : idxB,
                        Idx(_numPoints)
                    });
                }

                if (idxA == idxB) DETRIA_UNLIKELY
                {
                    return fail(TE_PolylineDuplicateConsecutivePoints
                    {
                        idxA
                    });
                }
            }

            TVertex v0(idxA);
            TVertex v1(idxB);

            THalfEdge currentEdgeIndex = _topology.getEdgeBetween(v0, v1);
            if (currentEdgeIndex.isValid())
            {
                // There is already an edge between the vertices, which may or may not be constrained already
                // We have priorities to decide which edge types are overwritten by other types
                // If an edge is not yet constrained, then always overwrite it
                // If an edge is manually constrained, then only overwrite it if the new type is auto-detect, outline, or hole
                // If an edge is auto-detect, then only overwrite it with outline or hole
                // If an edge is already an outline or hole, then check if the new type is the same
                // If they are different, then it's an error

                EdgeData& currentEdgeData = _topology.getEdgeData(currentEdgeIndex);

                constexpr std::array<uint8_t, size_t(EdgeType::MAX_EDGE_TYPE)> edgeTypePriorities = getConstrainedEdgeTypePriorities();

                EdgeType currentEdgeType = currentEdgeData.getEdgeType();
                uint8_t currentPriority = edgeTypePriorities[size_t(currentEdgeType)];
                uint8_t newPriority = edgeTypePriorities[size_t(constrainedEdgeType)];

                if (newPriority > currentPriority)
                {
                    // New edge type is higher priority, overwrite

                    EdgeData& oppositeEdgeData = _topology.getEdgeData(_topology.getOpposite(currentEdgeIndex));

                    switch (constrainedEdgeType)
                    {
                        case EdgeType::ManuallyConstrained:
                            currentEdgeData.data = oppositeEdgeData.data = typename EdgeData::ManuallyConstrainedEdgeTag{ };
                            break;
                        case EdgeType::AutoDetect:
                        case EdgeType::Outline:
                        case EdgeType::Hole:
                            currentEdgeData.data = oppositeEdgeData.data = typename EdgeData::OutlineOrHoleData
                            {
                                polylineIndex,
                                constrainedEdgeType
                            };

                            break;
                        DETRIA_UNLIKELY case EdgeType::NotConstrained:
                        DETRIA_UNLIKELY case EdgeType::MAX_EDGE_TYPE:
                            DETRIA_ASSERT(false);
                            break;
                    }
                }
                else if (newPriority == currentPriority)
                {
                    // Same priority, so the types must also be the same

                    if (currentEdgeType != constrainedEdgeType) DETRIA_UNLIKELY
                    {
                        // Cannot have an edge that is both an outline and a hole
                        return fail(TE_EdgeWithDifferentConstrainedTypes
                        {
                            idxA,
                            idxB
                        });
                    }
                }
                // Else lower priority, don't do anything

                return true;
            }

            /*
            Example:

                 a         b
                 *---------*
                /|        /|\
               / |       / | \
              /  |      /  |  \
             /   |     /   |   \
         v0 *----|----/----|----* v1
             \   |   /     |   /
              \  |  /      |  /
               \ | /       | /
                \|/        |/
                 *---------*
                 c         d

            We want to create a constrained edge between v0 and v1
            Start from v0
            Since triangles are cw, there must be a triangle which contains v0, and has points pNext and pPrev,
            for which orient2d(v0, v1, pNext) == CCW and orient2d(v0, v1, pPrev) == CW
            In the current example, the starting triangle is "v0 a c"
            The next triangle is the other triangle of edge "a c"
            Check the third point of that next triangle (b), whether orient2d(v0, v1, b) is CW or CCW
            If CW, then the next edge is "a b", if CCW, then "c b"
            Repeat this until we find v1 in one of the triangles
            Also keep track of the outer vertices for both the CW and CCW side
            (in this example, the CW side is "v0 c d v1", CCW side is "v0 a b v1")
            Then re-triangulate the CW and CCW sides separately
            This will ensure that an edge exists between v0 and v1
            */

            Idx p0 = v0.index;
            Idx p1 = v1.index;

            THalfEdge initialTriangleEdge{ };
            TVertex vertexCW{ };
            TVertex vertexCCW{ };

            // Find initial triangle, which is in the direction of the other point
            DETRIA_CHECK(findInitialTriangleForConstrainedEdge(v0, v1, p0, p1, initialTriangleEdge, vertexCW, vertexCCW));

            _constrainedEdgeVerticesCW.clear();
            _constrainedEdgeVerticesCCW.clear();

            // Traverse adjacent triangles, until we reach v1
            // Store vertex indices of the triangles along the way
            // Also store the deleted edges, those will be reused later
            // Since the total number of edges doesn't change here, we will always exactly have the right amount of edges
            _deletedConstrainedEdges.clear();
            DETRIA_CHECK(removeInnerTrianglesAndGetOuterVertices(v0, v1, p0, p1, vertexCW, vertexCCW, initialTriangleEdge,
                _constrainedEdgeVerticesCW, _constrainedEdgeVerticesCCW));

            // Create new edge, and mark it constrained
            THalfEdge beforeV0 = _topology.getEdgeBetween(v0, _constrainedEdgeVerticesCCW[1]); // First is v0
            THalfEdge beforeV1 = _topology.getEdgeBetween(v1, _constrainedEdgeVerticesCW[_constrainedEdgeVerticesCW.size() - 2]); // Last is v1

            THalfEdge constrainedEdgeIndex = _topology.createNewEdge(v0, v1, beforeV0, beforeV1, _deletedConstrainedEdges.back());
            _deletedConstrainedEdges.pop_back();

            HalfEdge& constrainedEdge = _topology.getEdge(constrainedEdgeIndex);
            HalfEdge& oppositeConstrainedEdge = _topology.getEdge(_topology.getOpposite(constrainedEdgeIndex));

            if (constrainedEdgeType == EdgeType::ManuallyConstrained)
            {
                constrainedEdge.data = oppositeConstrainedEdge.data = EdgeData
                {
                    typename EdgeData::ManuallyConstrainedEdgeTag{ },
                    EdgeData::Flags::None
                };
            }
            else
            {
                constrainedEdge.data = oppositeConstrainedEdge.data = EdgeData
                {
                    typename EdgeData::OutlineOrHoleData
                    {
                        polylineIndex,
                        constrainedEdgeType
                    },
                    EdgeData::Flags::None
                };
            }

            // Re-triangulate deleted triangles

            DETRIA_CHECK(
                reTriangulateAroundConstrainedEdge(_constrainedEdgeVerticesCW, delaunay, true) &&
                reTriangulateAroundConstrainedEdge(_constrainedEdgeVerticesCCW, delaunay, false)
            );

            DETRIA_DEBUG_ASSERT(_deletedConstrainedEdges.empty());

            return true;
        }

        bool findInitialTriangleForConstrainedEdge(const TVertex& v0, const TVertex& v1, Idx p0, Idx p1,
            THalfEdge& initialTriangleEdge, TVertex& vertexCW, TVertex& vertexCCW)
        {
            bool found = false;
            bool error = false;

            Vector2 p0Position = getPoint(p0);
            Vector2 p1Position = getPoint(p1);

            _topology.forEachEdgeOfVertex(v0, [&](THalfEdge e)
            {
                const HalfEdge& edge = _topology.getEdge(e);

                TVertex prevVertex = _topology.getEdge(_topology.getOpposite(edge.nextEdge)).vertex;
                TVertex nextVertex = _topology.getEdge(_topology.getOpposite(e)).vertex;

                // We are looking for a triangle where "v0, v1, nextVertex" is ccw, and "v0, v1, prevVertex" is cw

                Vector2 prevVertexPosition = getPoint(prevVertex.index);
                Vector2 nextVertexPosition = getPoint(nextVertex.index);

                math::Orientation orientNext = orient2d(p0Position, p1Position, nextVertexPosition);
                math::Orientation orientPrev = orient2d(p0Position, p1Position, prevVertexPosition);

                if (orientPrev == math::Orientation::Collinear || orientNext == math::Orientation::Collinear) DETRIA_UNLIKELY
                {
                    // We found a point which is exactly on the line
                    // Check if it's between v0 and v1
                    // If yes, then this means that an edge would have to go 3 points, which would create degenerate triangles
                    // If this is a steiner point, then maybe we could remove it, but for now, this is an error
                    // If it's not between, then simply skip this triangle

                    auto isBetween = [&](Vector2 point)
                    {
                        Scalar xDiff = predicates::Absolute(p0Position.x - p1Position.x);
                        Scalar yDiff = predicates::Absolute(p0Position.y - p1Position.y);

                        Scalar p0Value{ };
                        Scalar p1Value{ };
                        Scalar pointValue{ };
                        if (xDiff > yDiff)
                        {
                            // Compare x coordinate
                            p0Value = p0Position.x;
                            p1Value = p1Position.x;
                            pointValue = point.x;
                        }
                        else
                        {
                            // Compare y coordinate
                            p0Value = p0Position.y;
                            p1Value = p1Position.y;
                            pointValue = point.y;
                        }

                        if (p0Value < p1Value)
                        {
                            return p0Value <= pointValue && pointValue <= p1Value;
                        }
                        else
                        {
                            return p1Value <= pointValue && pointValue <= p0Value;
                        }
                    };

                    if (orientPrev == math::Orientation::Collinear)
                    {
                        error = isBetween(prevVertexPosition);
                    }
                    else
                    {
                        error = isBetween(nextVertexPosition);
                    }

                    if (error)
                    {
                        found = true;
                        fail(TE_PointOnConstrainedEdge
                        {
                            orientPrev == math::Orientation::Collinear ? prevVertex.index : nextVertex.index,
                            v0.index,
                            v1.index
                        });

                        return false;
                    }

                    return true;
                }
                else if (orientPrev == math::Orientation::CW && orientNext == math::Orientation::CCW)
                {
                    // Correct orientation, triangle found
                    found = true;

                    initialTriangleEdge = e;
                    vertexCW = prevVertex;
                    vertexCCW = nextVertex;

                    return false;
                }

                return true;
            });

            DETRIA_ASSERT(found);

            return !error;
        }

        bool removeInnerTrianglesAndGetOuterVertices(const TVertex& v0, const TVertex& v1, Idx p0, Idx p1,
            TVertex vertexCW, TVertex vertexCCW, THalfEdge initialTriangleEdge,
            CollectionWithAllocator<TVertex>& verticesCW, CollectionWithAllocator<TVertex>& verticesCCW)
        {
            verticesCW.push_back(v0);
            verticesCW.push_back(vertexCW);
            verticesCCW.push_back(v0);
            verticesCCW.push_back(vertexCCW);

            THalfEdge edgeAcross = _topology.getEdge(_topology.getOpposite(initialTriangleEdge)).prevEdge;

            while (true)
            {
                const HalfEdge& edge = _topology.getEdge(edgeAcross);
                if (edge.data.getEdgeType() != EdgeType::NotConstrained) DETRIA_UNLIKELY
                {
                    // To constrain the current edge, we'd have to remove another edge that is already constrained
                    // This means that some outlines or holes are intersecting
                    return fail(TE_ConstrainedEdgeIntersection
                    {
                        v0.index,
                        v1.index,
                        vertexCW.index,
                        vertexCCW.index
                    });
                }

                THalfEdge indexOfEdgeOfThirdVertex = _topology.getOpposite(edge.prevEdge);
                const HalfEdge& edgeOfThirdVertex = _topology.getEdge(indexOfEdgeOfThirdVertex);
                TVertex thirdVertex = edgeOfThirdVertex.vertex;

                THalfEdge nextEdgeAcross{ };

                bool reachedOtherEnd = thirdVertex == v1;
                if (!reachedOtherEnd)
                {
                    // Find next direction

                    math::Orientation orientation = orient2d(getPoint(p0), getPoint(p1), getPoint(thirdVertex.index));
                    if (orientation == math::Orientation::Collinear) DETRIA_UNLIKELY
                    {
                        // Point on a constrained edge, this is not allowed
                        return fail(TE_PointOnConstrainedEdge
                        {
                            thirdVertex.index,
                            v0.index,
                            v1.index
                        });
                    }

                    if (orientation == math::Orientation::CW)
                    {
                        vertexCW = thirdVertex;
                        verticesCW.push_back(thirdVertex);
                        nextEdgeAcross = _topology.getOpposite(indexOfEdgeOfThirdVertex);
                    }
                    else
                    {
                        vertexCCW = thirdVertex;
                        verticesCCW.push_back(thirdVertex);
                        nextEdgeAcross = edgeOfThirdVertex.prevEdge;
                    }
                }

                // Remove edge
                _topology.unlinkEdge(edgeAcross);
                _deletedConstrainedEdges.push_back(edgeAcross);

                if (reachedOtherEnd)
                {
                    break;
                }

                edgeAcross = nextEdgeAcross;
            }

            verticesCW.push_back(v1);
            verticesCCW.push_back(v1);

            DETRIA_ASSERT(verticesCW.size() >= 3 && verticesCCW.size() >= 3);

            return true;
        }

        bool reTriangulateAroundConstrainedEdge(const CollectionWithAllocator<TVertex>& vertices, bool delaunay, bool isCW)
        {
            /*
            `requiredOrientation` depends on which side of the line we are on
            For example, let's say we have to re-triangulate this:

              v0    a     b
                *---*---*
               / \     /
            c *   \   /
              |    \ /
              *-----*
             d      v1

            We want to triangulate "v0 a b v1" and "v0 c d v1"
            The first polyline is CCW, the second is CW, because all points are on that side of the "v0 v1" line
            When triangulating the first polyline, we can only add a triangle if the vertices are CW
            So for example, we cannot add the "v0 a b" triangle, because the points are not CW, they are collinear; but we can add "a b v1"
            Similarly, for the second polyline, we can only add a triangle if the points are CCW, so we can add "v0 c d"
            So:
                CW side of the "v0 v1" line -> `requiredOrientation` == CCW
                CCW side -> `requiredOrientation` == CW
            */

            math::Orientation requiredOrientation = isCW ? math::Orientation::CCW : math::Orientation::CW;

            _constrainedEdgeReTriangulationStack.clear();

            _constrainedEdgeReTriangulationStack.push_back(vertices[0]);
            _constrainedEdgeReTriangulationStack.push_back(vertices[1]);

            auto getReusedEdge = [&]()
            {
                DETRIA_DEBUG_ASSERT(!_deletedConstrainedEdges.empty());
                THalfEdge reusedEdge = _deletedConstrainedEdges.back();
                _deletedConstrainedEdges.pop_back();
                return reusedEdge;
            };

            for (size_t i = 2; i < vertices.size(); ++i)
            {
                const TVertex& currentVertex = vertices[i];

                while (true)
                {
                    TVertex prevPrevVertex = _constrainedEdgeReTriangulationStack[_constrainedEdgeReTriangulationStack.size() - 2];
                    TVertex prevVertex = _constrainedEdgeReTriangulationStack[_constrainedEdgeReTriangulationStack.size() - 1];

                    math::Orientation orientation = orient2d(
                        getPoint(prevPrevVertex.index),
                        getPoint(prevVertex.index),
                        getPoint(currentVertex.index)
                    );

                    if (orientation == requiredOrientation)
                    {
                        // Add edge if needed

                        std::optional<THalfEdge> oppositeEdge{ };
                        if (isCW)
                        {
                            if (!_topology.getEdgeBetween(prevPrevVertex, currentVertex).isValid())
                            {
                                oppositeEdge = _topology.getEdgeBetween(prevPrevVertex, prevVertex);

                                _topology.createNewEdge(prevPrevVertex, currentVertex,
                                    _topology.getEdge(*oppositeEdge).prevEdge,
                                    _topology.getEdgeBetween(currentVertex, prevVertex),
                                    getReusedEdge()
                                );
                            }
                        }
                        else
                        {
                            if (!_topology.getEdgeBetween(currentVertex, prevPrevVertex).isValid())
                            {
                                oppositeEdge = _topology.getEdgeBetween(prevPrevVertex, prevVertex);

                                _topology.createNewEdge(currentVertex, prevPrevVertex,
                                    _topology.getEdge(_topology.getEdgeBetween(currentVertex, prevVertex)).prevEdge,
                                    *oppositeEdge,
                                    getReusedEdge()
                                );
                            }
                        }

                        // Update stack
                        _constrainedEdgeReTriangulationStack.pop_back();

                        if (delaunay && oppositeEdge.has_value())
                        {
                            // Ensure delaunay criteria for the newly inserted triangle
                            // Usually, the triangles are already delaunay, but sometimes not, so check it here

                            DETRIA_CHECK(delaunayEdgeFlip(currentVertex, *oppositeEdge));
                        }

                        // If there are more than one items left in the stack, then go back and try to triangulate again
                        // Otherwise just break
                        if (_constrainedEdgeReTriangulationStack.size() < 2)
                        {
                            DETRIA_ASSERT(_constrainedEdgeReTriangulationStack.size() == 1);
                            break;
                        }
                    }
                    else
                    {
                        // Cannot add triangle, will try again later, after adding a new vertex
                        break;
                    }
                }

                _constrainedEdgeReTriangulationStack.push_back(currentVertex);
            }

            DETRIA_ASSERT(_constrainedEdgeReTriangulationStack.size() == 2);
            return true;
        }

        bool classifyTriangles(THalfEdge startingConvexHullEdge)
        {
            DETRIA_ASSERT(startingConvexHullEdge.isValid());

            const EdgeData& startingEdgeData = _topology.getEdgeData(startingConvexHullEdge);
            EdgeType startingEdgeType = startingEdgeData.getEdgeType();
            if (startingEdgeType == EdgeType::Hole)
            {
                // If this happens, then it means that an outer polyline is a hole, which is invalid
                return fail(TE_HoleNotInsideOutline{ });
            }

            _resultTriangles.clear();

            // Euler characteristic: number of triangles == number of edges - number of vertices + 1
            // Since the outer "face" doesn't exist, only add 1 instead of 2
            size_t expectedTriangleCount = 1 + _topology.halfEdgeCount() / 2 - _topology.vertexCount();

            if constexpr (collectionHasReserve)
            {
                _resultTriangles.reserve(expectedTriangleCount);
            }

            // Create all triangles

            CollectionWithAllocator<TriangleIndex>& trianglesByHalfEdgeIndex = _classifyTriangles_TrianglesByHalfEdgeIndex;
            trianglesByHalfEdgeIndex.clear();
            trianglesByHalfEdgeIndex.resize(_topology.halfEdgeCount(), TriangleIndex{ });

            CollectionWithAllocator<bool>& checkedHalfEdges = _classifyTriangles_CheckedHalfEdges;
            checkedHalfEdges.clear();
            checkedHalfEdges.resize(_topology.halfEdgeCount(), false);

            for (size_t i = 0; i < _topology.halfEdgeCount(); ++i)
            {
                if (checkedHalfEdges[i])
                {
                    // Edge already checked
                    continue;
                }

                THalfEdge edge = THalfEdge(Idx(i));

                if (_topology.getEdgeData(edge).isBoundary()) DETRIA_UNLIKELY
                {
                    // Half-edge is boundary, don't add its triangle
                    checkedHalfEdges[i] = true;
                    continue;
                }

                TriangleIndex triangleIndex = TriangleIndex(Idx(_resultTriangles.size()));

                const HalfEdge& e0 = _topology.getEdge(edge);

                THalfEdge e1i = _topology.getEdge(_topology.getOpposite(edge)).prevEdge;
                const HalfEdge& e1 = _topology.getEdge(e1i);

                THalfEdge e2i = _topology.getEdge(_topology.getOpposite(e1i)).prevEdge;
                const HalfEdge& e2 = _topology.getEdge(e2i);

                checkedHalfEdges[i] = true;
                checkedHalfEdges[size_t(e1i.index)] = true;
                checkedHalfEdges[size_t(e2i.index)] = true;

                trianglesByHalfEdgeIndex[i] = triangleIndex;
                trianglesByHalfEdgeIndex[size_t(e1i.index)] = triangleIndex;
                trianglesByHalfEdgeIndex[size_t(e2i.index)] = triangleIndex;

                TriangleWithData& tri = _resultTriangles.emplace_back(TriangleWithData
                {
                    Tri
                    {
                        e0.vertex.index,
                        e1.vertex.index,
                        e2.vertex.index
                    },
                    TriangleData{ }
                });

                tri.data.firstEdge = edge;
            }

            // Outer edge, only has one triangle
            TriangleIndex startingTriangle = trianglesByHalfEdgeIndex[size_t(startingConvexHullEdge.index)];
            DETRIA_ASSERT(startingTriangle.isValid());

            _resultTriangles[size_t(startingTriangle.index)].data.locationData = typename TriangleData::KnownLocationData
            {
                // nullopt if the edge is not part of an outline or a hole
                startingEdgeData.getOutlineOrHoleIndex()
            };

            // For each polyline, store which other polyline contains them
            _parentPolylines.resize(_polylines.size());

            _autoDetectedPolylineTypes.resize(_polylines.size(), EdgeType::AutoDetect);

            CollectionWithAllocator<bool>& checkedTriangles = _classifyTriangles_CheckedTriangles;
            checkedTriangles.resize(_resultTriangles.size());
            checkedTriangles[size_t(startingTriangle.index)] = true;

            CollectionWithAllocator<TriangleIndex>& trianglesToCheck = _classifyTriangles_TrianglesToCheck;
            trianglesToCheck.push_back(startingTriangle);

            if (startingEdgeType == EdgeType::AutoDetect)
            {
                // Convex hull edge is auto-detect, so it must be an outline
                if (std::optional<Idx> startingEdgePolylineIndex = startingEdgeData.getOutlineOrHoleIndex())
                {
                    _autoDetectedPolylineTypes[size_t(*startingEdgePolylineIndex)] = EdgeType::Outline;
                }
                else
                {
                    DETRIA_ASSERT(false);
                }
            }

            while (!trianglesToCheck.empty())
            {
                TriangleIndex currentTriangle = trianglesToCheck.back();
                trianglesToCheck.pop_back();

                const auto& currentTriangleData = _resultTriangles[size_t(currentTriangle.index)].data;
                const typename TriangleData::KnownLocationData* currentTriangleLocationData =
                    std::get_if<typename TriangleData::KnownLocationData>(&currentTriangleData.locationData);

                DETRIA_ASSERT(currentTriangleLocationData != nullptr);

                bool currentTriangleIsInterior = getTriangleLocation(*currentTriangleLocationData) == TriangleLocation::Interior;

                std::array<THalfEdge, 3> edgesOfCurrentTriangle{ };
                edgesOfCurrentTriangle[0] = currentTriangleData.firstEdge;
                edgesOfCurrentTriangle[1] = _topology.getEdge(_topology.getOpposite(edgesOfCurrentTriangle[0])).prevEdge;
                edgesOfCurrentTriangle[2] = _topology.getEdge(_topology.getOpposite(edgesOfCurrentTriangle[1])).prevEdge;

                for (Idx i = 0; i < 3; ++i)
                {
                    THalfEdge edgeIndex = edgesOfCurrentTriangle[i];
                    const EdgeData& edgeData = _topology.getEdgeData(edgeIndex);

                    TriangleIndex neighborTriangle = trianglesByHalfEdgeIndex[size_t(_topology.getOpposite(edgeIndex).index)];
                    if (!neighborTriangle.isValid())
                    {
                        // The triangle is on the edge of the entire triangulation, so it does not have all 3 neighbors
                        continue;
                    }

                    if (checkedTriangles[size_t(neighborTriangle.index)])
                    {
                        // This triangle was already checked, don't check it again
                        continue;
                    }

                    checkedTriangles[size_t(neighborTriangle.index)] = true;

                    std::optional<Idx> neighborTriangleParentPolylineIndex{ };

                    if (std::optional<Idx> polylineIndex = edgeData.getOutlineOrHoleIndex())
                    {
                        // Check which type of edge we just crossed (if this branch is entered, then it's part of either an outline or a hole)
                        // There are a few possibilites:
                        // If the triangle is inside, and the edge is part of an outline,
                        // then the outline's index must be the same as the triangle's parent polyline index
                        // Otherwise it would mean that there is an outline inside another outline, which is not allowed
                        // Same for holes: if the triangle is outside, and the edge is part of a hole,
                        // then it must be the same hole as the current triangle's parent
                        // Outlines can have any number of holes in them, and holes can have any number of outlines too

                        /*
                        For example:
                        Let's say that we have two polylines: "a" and "b"

                          *---*    *---*
                         a|   |   b|   |
                          *---*    *---*

                        Let's also say that `startingTriangle` (declared near the start of this function) is not part of any of the polylines
                        So the classification starts from outside
                        "a" and "b" both must be outlines, otherwise we'd have a hole in an area that is already outside, which is invalid
                        To verify this, whenever we reach "a" or "b", check if the edge part of an outline or a hole
                        If it's an outline, then it's fine, because "outside triangle" -> "outline edge" -> "inside triangle" is valid
                        If it's a hole, then check if the hole's index is the same as the triangle's parent polyline index
                        Since the current triangle is not inside any polyline, its parent polyline index is nullopt, so the indices are not the same
                        So if either "a" or "b" is a hole, we can detect it, and fail the triangulation
                        */

                        EdgeType currentPolylineType = _polylines[size_t(*polylineIndex)].type;
                        if (currentPolylineType == EdgeType::AutoDetect)
                        {
                            // Current type is auto-detect, check if we already already know the actual type
                            EdgeType& autoDetectedType = _autoDetectedPolylineTypes[size_t(*polylineIndex)];
                            if (autoDetectedType == EdgeType::AutoDetect)
                            {
                                // Type is not known yet, so just set to the opposite
                                // So if we are outside, then classify as outline
                                // If we are inside, then classify as hole
                                autoDetectedType = currentTriangleIsInterior ? EdgeType::Hole : EdgeType::Outline;
                            }
                            // Else already classified

                            currentPolylineType = autoDetectedType;
                        }

                        bool currentEdgeIsHole = currentPolylineType == EdgeType::Hole;
                        if (currentTriangleIsInterior == currentEdgeIsHole)
                        {
                            // Simple case, we are outside and crossing an outline, or we are inside and crossing a hole (this is always valid)

                            // Also update the parent for the current polyline
                            _parentPolylines[size_t(*polylineIndex)] = currentTriangleLocationData->parentPolylineIndex;

                            // Just set index, which implicitly sets the opposite location
                            neighborTriangleParentPolylineIndex = polylineIndex;
                        }
                        else
                        {
                            // We are outside and crossing a hole, or we are inside and crossing an outline
                            if (currentTriangleLocationData->parentPolylineIndex == *polylineIndex) DETRIA_LIKELY
                            {
                                // Valid case, the neighbor triangle is the opposite location
                                // Set its polyline index to the current polyline's parent

                                neighborTriangleParentPolylineIndex = _parentPolylines[size_t(*polylineIndex)];
                            }
                            else
                            {
                                // Invalid case, either an outline is inside another outline, or a hole is inside another hole
                                return fail(TE_StackedPolylines{ currentEdgeIsHole });
                            }
                        }
                    }
                    else
                    {
                        // Simple case, we are not crossing outlines or holes, so just propagate the current location
                        neighborTriangleParentPolylineIndex = currentTriangleLocationData->parentPolylineIndex;
                    }

                    _resultTriangles[size_t(neighborTriangle.index)].data.locationData = typename TriangleData::KnownLocationData
                    {
                        neighborTriangleParentPolylineIndex
                    };

                    trianglesToCheck.push_back(neighborTriangle);
                }
            }

            return true;
        }
        
        inline TriangleLocation getTriangleLocation(const typename TriangleData::KnownLocationData& data) const
        {
            if (!data.parentPolylineIndex.has_value())
            {
                return TriangleLocation::ConvexHull;
            }
            else
            {
                DETRIA_DEBUG_ASSERT(size_t(*data.parentPolylineIndex) < _polylines.size());
                EdgeType polylineType = _polylines[size_t(*data.parentPolylineIndex)].type;
                if (polylineType == EdgeType::AutoDetect)
                {
                    polylineType = _autoDetectedPolylineTypes[size_t(*data.parentPolylineIndex)];
                    DETRIA_DEBUG_ASSERT(polylineType == EdgeType::Outline || polylineType == EdgeType::Hole);
                }

                return polylineType == EdgeType::Hole ? TriangleLocation::Hole : TriangleLocation::Interior;
            }
        }

        inline TriangleLocation getTriangleLocation(size_t triIndex) const
        {
            const typename TriangleData::KnownLocationData* data =
                std::get_if<typename TriangleData::KnownLocationData>(&_resultTriangles[triIndex].data.locationData);

            if (data != nullptr)
            {
                return getTriangleLocation(*data);
            }

            DETRIA_DEBUG_ASSERT(false);
            return TriangleLocation::ConvexHull;
        }

        template <bool Flip = false>
        inline Tri getTriangleOriginalIndices(size_t triIndex) const
        {
            Tri tri = _resultTriangles[triIndex].triangle;

            if constexpr (Flip)
            {
                std::swap(tri.y, tri.z);
            }

            return tri;
        }

        template <typename Callback, typename ProcessTriangleCallback>
        void forEachTriangleInternal(Callback&& callback, bool cwTriangles, ProcessTriangleCallback&& shouldProcessTriangle) const
        {
            for (size_t i = 0; i < _resultTriangles.size(); ++i)
            {
                if (shouldProcessTriangle(i))
                {
                    if (cwTriangles)
                    {
                        callback(getTriangleOriginalIndices<false>(i));
                    }
                    else
                    {
                        callback(getTriangleOriginalIndices<true>(i));
                    }
                }
            }
        }

        inline static math::Orientation orient2d(Vector2 a, Vector2 b, Vector2 c)
        {
            return math::orient2d<Config::UseRobustOrientationTests>(a, b, c);
        }

        inline static math::CircleLocation incircle(Vector2 a, Vector2 b, Vector2 c, Vector2 d)
        {
            return math::incircle<Config::UseRobustIncircleTests>(a, b, c, d);
        }

    private:
        Allocator _allocator;

        // Inputs
        std::optional<PointGetter> _pointGetter;
        size_t _numPoints;
        CollectionWithAllocator<PolylineData> _polylines;
        CollectionWithAllocator<Vec2<Idx>> _manuallyConstrainedEdges;
        CollectionWithAllocator<EdgeType> _autoDetectedPolylineTypes;

        Topology _topology;

        // Reused containers for multiple calculations
        CollectionWithAllocator<Idx> _initialTriangulation_SortedPoints;
        CollectionWithAllocator<TVertex> _constrainedEdgeVerticesCW;
        CollectionWithAllocator<TVertex> _constrainedEdgeVerticesCCW;
        CollectionWithAllocator<THalfEdge> _deletedConstrainedEdges;
        CollectionWithAllocator<TVertex> _constrainedEdgeReTriangulationStack;
        CollectionWithAllocator<TriangleIndex> _classifyTriangles_TrianglesByHalfEdgeIndex;
        CollectionWithAllocator<bool> _classifyTriangles_CheckedHalfEdges;
        CollectionWithAllocator<bool> _classifyTriangles_CheckedTriangles;
        CollectionWithAllocator<TriangleIndex> _classifyTriangles_TrianglesToCheck;

        struct TriangleWithData
        {
            Tri triangle;
            TriangleData data;
        };

        CollectionWithAllocator<TriangleWithData> _resultTriangles;

        // Results which are not related to the triangulation directly
        List _convexHullPoints;
        Idx _convexHullStartIndex = Idx(-1);
        CollectionWithAllocator<std::optional<Idx>> _parentPolylines;

        struct TopologyEdgeWithVertices
        {
            TVertex v0;
            TVertex v1;
            THalfEdge edge;
        };

        CollectionWithAllocator<TopologyEdgeWithVertices> _delaunayCheckStack;

        TriangulationErrorData _error = TE_NotStarted{ };
    };

#undef DETRIA_INIT_COLLECTION_WITH_ALLOCATOR
#undef DETRIA_CHECK
#undef DETRIA_ASSERT
#undef DETRIA_ASSERT_MSG
#undef DETRIA_DEBUG_ASSERT

#undef DETRIA_LIKELY
#undef DETRIA_UNLIKELY
}

#endif // DETRIA_HPP_INCLUDED

// License
// This library is available under 2 licenses, you can choose whichever you prefer.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Alternative A - Do What The Fuck You Want To Public License
//
// Copyright (c) Kimbatt (https://github.com/Kimbatt)
//
// This work is free. You can redistribute it and/or modify it under the terms of
// the Do What The Fuck You Want To Public License, Version 2, as published by Sam Hocevar.
//
//            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
//                    Version 2, December 2004
//
// Copyright (C) 2004 Sam Hocevar <sam@hocevar.net>
//
// Everyone is permitted to copy and distribute verbatim or modified
// copies of this license document, and changing it is allowed as long
// as the name is changed.
//
//            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
//   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
//
// 0. You just DO WHAT THE FUCK YOU WANT TO.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Alternative B - MIT License
//
// Copyright (c) Kimbatt (https://github.com/Kimbatt)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
// NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
// OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
