/*                           F F T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/** @addtogroup libfft
 *
 * @brief
 * The Fast-Fourier Transform library is a signal processing
 * library for performing FFTs or inverse FFTs efficiently.
 */
#ifndef FFT_H
#define FFT_H

#include "common.h"

#include <math.h>

__BEGIN_DECLS

/* NOTE: intentionally not using vmath.h to avoid dependency */
#ifndef M_PI
#  define M_PI 3.14159265358979323846264338328
#endif
#ifndef M_SQRT1_2
#  define M_SQRT1_2 0.70710678118654752440084436210
#endif

#ifndef FFT_EXPORT
#  if defined(FFT_DLL_EXPORTS) && defined(FFT_DLL_IMPORTS)
#    error "Only FFT_DLL_EXPORTS or FFT_DLL_IMPORTS can be defined, not both."
#  elif defined(FFT_DLL_EXPORTS)
#    define FFT_EXPORT COMPILER_DLLEXPORT
#  elif defined(FFT_DLL_IMPORTS)
#    define FFT_EXPORT COMPILER_DLLIMPORT
#  else
#    define FFT_EXPORT
#  endif
#endif

/** @addtogroup libfft */
/** @{ */
/** @file fft.h */

/* The COMPLEX type used throughout */
typedef struct {
    double re;	/* Real Part */
    double im;	/* Imaginary Part */
} COMPLEX;

/**
 * Generates Real Split Radix Decimation in Time source files
 */
FFT_EXPORT extern void splitdit(int N, int M);

/**
 * Generates Real Split Radix Decimation in Freq Inverse FFT source files
 *
 * @param n [in] length
 * @param m [in] n = 2^m
 */
FFT_EXPORT extern void ditsplit(int n, int m);

/**
 * @brief
 * Real valued, split-radix, decimation in time FFT.
 *
 * Data comes out as: [ Re(0), Re(1), ..., Re(N/2), Im(N/2-1), ..., Im(1) ]
 */
FFT_EXPORT extern void rfft(double *X, int N);

/**
 * @brief
 * Split Radix, Decimation in Frequency, Inverse Real-valued FFT.
 *
 * Input order: [ Re(0), Re(1), ..., Re(N/2), Im(N/2-1), ..., Im(1) ]
 *
 * @param X [in] ?
 * @param n [in] length
 *
 */
FFT_EXPORT extern void irfft(double *X, int n);

/**
 * @brief
 * Forward Complex Fourier Transform
 *
 *  "Fast" Version - Function calls to complex math routines removed.
 *  Uses pre-computed sine/cosine tables.
 *
 * The FFT is:
 *
 *              N-1
 *      Xf(k) = Sum x(n)(cos(2PI(nk/N)) - isin(2PI(nk/N)))
 *              n=0
 */
FFT_EXPORT extern void cfft(COMPLEX *dat, int num);

/**
 * Inverse Complex Fourier Transform
 */
FFT_EXPORT extern void icfft(COMPLEX *dat, int num);

/**
 * Complex divide (why is this public API when none of the other
 * complex math routines are?)
 */
FFT_EXPORT extern void cdiv(COMPLEX *result, COMPLEX *val1, COMPLEX *val2);

/* These should come from a generated header, but until
 * CMake is live we'll just add the ones used by our current code */
FFT_EXPORT extern void rfft256(register double X[]);
FFT_EXPORT extern void irfft256(register double X[]);

__END_DECLS

/** @} */
#endif /* FFT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
