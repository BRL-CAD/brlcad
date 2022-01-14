/*                       W A V E L E T . H
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

/*----------------------------------------------------------------------*/
/** @addtogroup bn_wavelet
 *
 * @brief
 *  This is a standard wavelet library that takes a given data buffer of some data
 *  type and then performs a wavelet transform on that data.
 *
 * The transform
 *  operations available are to either decompose or reconstruct a signal into its
 *  corresponding wavelet form based on the haar wavelet.
 *
 *  Wavelet decompose/reconstruct operations
 *
 *	- bn_wlt_haar_1d_double_decompose(tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_1d_float_decompose (tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_1d_char_decompose  (tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_1d_short_decompose (tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_1d_int_decompose   (tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_1d_long_decompose  (tbuffer, buffer, dimen, channels, limit)
 *
 *	- bn_wlt_haar_1d_double_reconstruct(tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_1d_float_reconstruct (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_1d_char_reconstruct  (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_1d_short_reconstruct (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_1d_int_reconstruct   (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_1d_long_reconstruct  (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *
 *	- bn_wlt_haar_2d_double_decompose(tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_2d_float_decompose (tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_2d_char_decompose  (tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_2d_short_decompose (tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_2d_int_decompose   (tbuffer, buffer, dimen, channels, limit)
 *	- bn_wlt_haar_2d_long_decompose  (tbuffer, buffer, dimen, channels, limit)
 *
 *	- bn_wlt_haar_2d_double_reconstruct(tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_float_reconstruct (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_char_reconstruct  (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_short_reconstruct (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_int_reconstruct   (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_long_reconstruct  (tbuffer, buffer, dimen, channels, sub_sz, limit)
 *
 *	- bn_wlt_haar_2d_double_decompose2(tbuffer, buffer, width, height, channels, limit)
 *	- bn_wlt_haar_2d_float_decompose2 (tbuffer, buffer, width, height, channels, limit)
 *	- bn_wlt_haar_2d_char_decompose2  (tbuffer, buffer, width, height, channels, limit)
 *	- bn_wlt_haar_2d_short_decompose2 (tbuffer, buffer, width, height, channels, limit)
 *	- bn_wlt_haar_2d_int_decompose2   (tbuffer, buffer, width, height, channels, limit)
 *	- bn_wlt_haar_2d_long_decompose2  (tbuffer, buffer, width, height, channels, limit)
 *
 *	- bn_wlt_haar_2d_double_reconstruct2(tbuffer, buffer, width, height, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_float_reconstruct2 (tbuffer, buffer, width, height, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_char_reconstruct2  (tbuffer, buffer, width, height, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_short_reconstruct2 (tbuffer, buffer, width, height, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_int_reconstruct2   (tbuffer, buffer, width, height, channels, sub_sz, limit)
 *	- bn_wlt_haar_2d_long_reconstruct2  (tbuffer, buffer, width, height, channels, sub_sz, limit)
 *
 *
 *  For greatest accuracy, it is preferable to convert everything to "double"
 *  and decompose/reconstruct with that.  However, there are useful
 *  properties to performing the decomposition and/or reconstruction in
 *  various data types (most notably char).
 *
 *  Rather than define all of these routines explicitly, we define
 *  2 macros "decompose" and "reconstruct" which embody the structure of
 *  the function (which is common to all of them).  We then instantiate
 *  these macros once for each of the data types.  It's ugly, but it
 *  assures that a change to the structure of one operation type
 *  (decompose or reconstruct) occurs for all data types.
 *
 *
 *
 *
 *  bn_wlt_haar_1d_*_decompose(tbuffer, buffer, dimen, channels, limit)
 *  @par Parameters:
 *	- tbuffer     a temporary data buffer 1/2 as large as "buffer". See (1) below.
 *	- buffer      pointer to the data to be decomposed
 *	- dimen    the number of samples in the data buffer
 *	- channels the number of values per sample
 *	- limit    the extent of the decomposition
 *
 *  Perform a Haar wavelet decomposition on the data in buffer "buffer".  The
 *  decomposition is done "in place" on the data, hence the values in "buffer"
 *  are not preserved, but rather replaced by their decomposition.
 *  The number of original samples in the buffer (parameter "dimen") and the
 *  decomposition limit ("limit") must both be a power of 2 (e.g. 512, 1024).
 *  The buffer is decomposed into "average" and "detail" halves until the
 *  size of the "average" portion reaches "limit".  Simultaneous
 *  decomposition of multi-plane (e.g. pixel) data, can be performed by
 *  indicating the number of planes in the "channels" parameter.
 *
 *  (1) The process requires a temporary buffer which is 1/2 the size of the
 *  longest span to be decomposed.  If the "tbuffer" argument is non-null then
 *  it is a pointer to a temporary buffer.  If the pointer is NULL, then a
 *  local temporary buffer will be allocated (and freed).
 *
 *  @par Examples:
 @code
 double dbuffer[512], cbuffer[256];
 ...
 bn_wlt_haar_1d_double_decompose(cbuffer, dbuffer, 512, 1, 1);
 @endcode
 *
 *    performs complete decomposition on the data in array "dbuffer".
 *
 @code
 double buffer[3][512];	 /_* 512 samples, 3 values/sample (e.g. RGB?)*_/
 double tbuffer[3][256];	 /_* the temporary buffer *_/
 ...
 bn_wlt_haar_1d_double_decompose(tbuffer, buffer, 512, 3, 1);
 @endcode
 *
 *    This will completely decompose the data in buffer.  The first sample will
 *    be the average of all the samples.  Alternatively:
 *
 * bn_wlt_haar_1d_double_decompose(tbuffer, buffer, 512, 3, 64);
 *
 *    decomposes buffer into a 64-sample "average image" and 3 "detail" sets.
 *
 *  bn_wlt_haar_1d_*_reconstruct(tbuffer, buffer, dimen, channels, sub_sz, limit)
 *
 */
/** @{ */
/** @file wavelet.h */

#ifndef BN_WAVELET_H
#define BN_WAVELET_H

#include "common.h"
#include <stdio.h> /* For FILE */
#include "bn/defines.h"

__BEGIN_DECLS

#define CK_POW_2(dimen) { \
	register size_t j; \
	register size_t ok; \
	for (ok=0, j=0; j < sizeof(size_t) * 8; j++) { \
	    if ((size_t)(1<<j) == dimen) { ok = 1;  break; } \
	} \
	if (! ok) { \
	    bu_log("%s:%d value %ld should be power of 2 (2^%ld)\n", \
		   __FILE__, __LINE__, (long)dimen, (long)j); \
	    bu_bomb("CK_POW_2"); \
	} \
    }

BN_EXPORT extern void bn_wlt_haar_1d_double_decompose(double *tbuf,
						      double *buf,
						      size_t dimen,
						      size_t depth,
						      size_t limit);
BN_EXPORT extern void bn_wlt_haar_1d_double_reconstruct(double *tbuf,
							double *buf,
							size_t dimen,
							size_t depth,
							size_t subimage_size,
							size_t limit);

BN_EXPORT extern void bn_wlt_haar_1d_float_decompose(float *tbuf,
						     float *buf,
						     size_t dimen,
						     size_t depth,
						     size_t limit);
BN_EXPORT extern void bn_wlt_haar_1d_float_reconstruct(float *tbuf,
						       float *buf,
						       size_t dimen,
						       size_t depth,
						       size_t subimage_size,
						       size_t limit);

BN_EXPORT extern void bn_wlt_haar_1d_char_decompose(char *tbuf,
						    char *buf,
						    size_t dimen,
						    size_t depth,
						    size_t limit);
BN_EXPORT extern void bn_wlt_haar_1d_char_reconstruct(char *tbuf, char *buf,
						      size_t dimen,
						      size_t depth,
						      size_t subimage_size,
						      size_t limit);

BN_EXPORT extern void bn_wlt_haar_1d_short_decompose(short *tbuf, short *buf,
						     size_t dimen,
						     size_t depth,
						     size_t limit);
BN_EXPORT extern void bn_wlt_haar_1d_short_reconstruct(short *tbuf, short *buf,
						       size_t dimen,
						       size_t depth,
						       size_t subimage_size,
						       size_t limit);

BN_EXPORT extern void bn_wlt_haar_1d_int_decompose(int *tbuf, int *buf,
						   size_t dimen,
						   size_t depth,
						   size_t limit);
BN_EXPORT extern void bn_wlt_haar_1d_int_reconstruct(int *tbuf,
						     int *buf,
						     size_t dimen,
						     size_t depth,
						     size_t subimage_size,
						     size_t limit);

BN_EXPORT extern void bn_wlt_haar_1d_long_decompose(long *tbuf, long *buf,
						    size_t dimen,
						    size_t depth,
						    size_t limit);
BN_EXPORT extern void bn_wlt_haar_1d_long_reconstruct(long *tbuf, long *buf,
						      size_t dimen,
						      size_t depth,
						      size_t subimage_size,
						      size_t limit);


BN_EXPORT extern void bn_wlt_haar_2d_double_decompose(double *tbuf,
						      double *buf,
						      size_t dimen,
						      size_t depth,
						      size_t limit);
BN_EXPORT extern void bn_wlt_haar_2d_double_reconstruct(double *tbuf,
							double *buf,
							size_t dimen,
							size_t depth,
							size_t subimage_size,
							size_t limit);

BN_EXPORT extern void bn_wlt_haar_2d_float_decompose(float *tbuf,
						     float *buf,
						     size_t dimen,
						     size_t depth,
						     size_t limit);
BN_EXPORT extern void bn_wlt_haar_2d_float_reconstruct(float *tbuf,
						       float *buf,
						       size_t dimen,
						       size_t depth,
						       size_t subimage_size,
						       size_t limit);

BN_EXPORT extern void bn_wlt_haar_2d_char_decompose(char *tbuf,
						    char *buf,
						    size_t dimen,
						    size_t depth,
						    size_t limit);
BN_EXPORT extern void bn_wlt_haar_2d_char_reconstruct(char *tbuf,
						      char *buf,
						      size_t dimen,
						      size_t depth,
						      size_t subimage_size,
						      size_t limit);

BN_EXPORT extern void bn_wlt_haar_2d_short_decompose(short *tbuf,
						     short *buf,
						     size_t dimen,
						     size_t depth,
						     size_t limit);
BN_EXPORT extern void bn_wlt_haar_2d_short_reconstruct(short *tbuf,
						       short *buf,
						       size_t dimen,
						       size_t depth,
						       size_t subimage_size,
						       size_t limit);

BN_EXPORT extern void bn_wlt_haar_2d_int_decompose(int *tbuf,
						   int *buf,
						   size_t dimen,
						   size_t depth,
						   size_t limit);
BN_EXPORT extern void bn_wlt_haar_2d_int_reconstruct(int *tbuf,
						     int *buf,
						     size_t dimen,
						     size_t depth,
						     size_t subimage_size,
						     size_t limit);

BN_EXPORT extern void bn_wlt_haar_2d_long_decompose(long *tbuf,
						    long *buf,
						    size_t dimen,
						    size_t depth,
						    size_t limit);
BN_EXPORT extern void bn_wlt_haar_2d_long_reconstruct(long *tbuf,
						      long *buf,
						      size_t dimen,
						      size_t depth,
						      size_t subimage_size,
						      size_t limit);


BN_EXPORT extern void bn_wlt_haar_2d_double_decompose2(double *tbuf,
						       double *buf,
						       size_t dimen,
						       size_t width,
						       size_t height,
						       size_t limit);
BN_EXPORT extern void bn_wlt_haar_2d_double_reconstruct2(double *tbuf,
							 double *buf,
							 size_t dimen,
							 size_t width,
							 size_t height,
							 size_t subimage_size,
							 size_t limit);

BN_EXPORT extern void bn_wlt_haar_2d_float_decompose2(float *tbuf,
						      float *buf,
						      size_t dimen,
						      size_t width,
						      size_t height,
						      size_t limit);
BN_EXPORT extern void bn_wlt_haar_2d_float_reconstruct2(float *tbuf,
							float *buf,
							size_t dimen,
							size_t width,
							size_t height,
							size_t subimage_size,
							size_t limit);

BN_EXPORT extern void bn_wlt_haar_2d_char_decompose2(char *tbuf,
						     char *buf,
						     size_t dimen,
						     size_t width,
						     size_t height,
						     size_t limit);
BN_EXPORT extern void bn_wlt_haar_2d_char_reconstruct2(char *tbuf,
						       char *buf,
						       size_t dimen,
						       size_t width,
						       size_t height,
						       size_t subimage_size,
						       size_t limit);

BN_EXPORT extern void bn_wlt_haar_2d_short_decompose2(short *tbuf,
						      short *buf,
						      size_t dimen,
						      size_t width,
						      size_t height,
						      size_t limit);
BN_EXPORT extern void bn_wlt_haar_2d_short_reconstruct2(short *tbuf,
							short *buf,
							size_t dimen,
							size_t width,
							size_t height,
							size_t subimage_size,
							size_t limit);

BN_EXPORT extern void bn_wlt_haar_2d_int_decompose2(int *tbuf,
						    int *buf,
						    size_t dimen,
						    size_t width,
						    size_t height,
						    size_t limit);
BN_EXPORT extern void bn_wlt_haar_2d_int_reconstruct2(int *tbuf,
						      int *buf,
						      size_t dimen,
						      size_t width,
						      size_t height,
						      size_t subimage_size,
						      size_t limit);

BN_EXPORT extern void bn_wlt_haar_2d_long_decompose2(long *tbuf,
						     long *buf,
						     size_t dimen,
						     size_t width,
						     size_t height,
						     size_t limit);
BN_EXPORT extern void bn_wlt_haar_2d_long_reconstruct2(long *tbuf,
						       long *buf,
						       size_t dimen,
						       size_t width,
						       size_t height,
						       size_t subimage_size,
						       size_t limit);

__END_DECLS

#endif  /* BN_WAVELET_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
