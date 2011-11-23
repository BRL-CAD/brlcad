/*                           F F T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file fft.h
 *
 */

#include "common.h"

#include <math.h>

#ifndef M_PI
#  define M_PI 3.14159265358979323846264338328
#endif
#ifndef M_SQRT1_2
#  define M_SQRT1_2 0.70710678118654752440084436210
#endif
#ifndef M_SQRT2
#  define M_SQRT2 1.41421356237309504880168872421
#endif

#ifndef FFT_EXPORT
#  if defined(FFT_DLL_EXPORTS) && defined(FFT_DLL_IMPORTS)
#    error "Only FFT_DLL_EXPORTS or FFT_DLL_IMPORTS can be defined, not both."
#  elif defined(FFT_DLL_EXPORTS)
#    define FFT_EXPORT __declspec(dllexport)
#  elif defined(FFT_DLL_IMPORTS)
#    define FFT_EXPORT __declspec(dllimport)
#  else
#    define FFT_EXPORT
#  endif
#endif

/* The COMPLEX type used throughout */
typedef struct {
    double re;	/* Real Part */
    double im;	/* Imaginary Part */
} COMPLEX;

FFT_EXPORT extern void splitdit(int N, int M);
FFT_EXPORT extern void ditsplit(int n /* length */, int m /* n = 2^m */);
FFT_EXPORT extern void rfft(double *X, int N);
FFT_EXPORT extern void irfft(double *X, int n);
FFT_EXPORT extern void cfft(COMPLEX *dat, int num);
FFT_EXPORT extern void icfft(COMPLEX *dat, int num);
FFT_EXPORT extern void cdiv(COMPLEX *result, COMPLEX *val1, COMPLEX *val2);

/* These should come from a generated header, but until
 * CMake is live we'll just add the ones used by our current code */
FFT_EXPORT extern void rfft256(register double X[]);
FFT_EXPORT extern void irfft256(register double X[]);

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
