/*                       W A V E L E T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup wavelet */
/** @{ */
/** @file wavelet.c
 *
 * @brief
 *  This is a standard wavelet library that takes a given data buffer of some data
 *  type and then performs a wavelet transform on that data.  
 *
 * The transform
 *  operations available are to either decompose or reconstruct a signal into it's
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
 *  the function (which is common to all of them).  We then instatiate
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
 *	bn_wlt_haar_1d_double_decompose(tbuffer, buffer, 512, 3, 64);
 *
 *    decomposes buffer into a 64-sample "average image" and 3 "detail" sets.
 *
 *  bn_wlt_haar_1d_*_reconstruct(tbuffer, buffer, dimen, channels, sub_sz, limit)
 *
 *
 *  @author
 *	Lee A. Butler
 *
 *  @par Modifications
 *      Christopher Sean Morrison
 *
 *  @par Source -
 *	The U. S. Army Research Laboratory
 *@n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"


#ifdef __STDC__
#define decompose_1d(DATATYPE) bn_wlt_haar_1d_ ## DATATYPE ## _decompose
#else
#define decompose_1d(DATATYPE) bn_wlt_haar_1d_/**/DATATYPE/**/_decompose
#endif



#define make_wlt_haar_1d_decompose(DATATYPE)  \
void \
decompose_1d(DATATYPE) \
( tbuffer, buffer, dimen, channels, limit ) \
DATATYPE *tbuffer;		/* temporary buffer */ \
DATATYPE *buffer;		/* data buffer */ \
unsigned long dimen;	/* # of samples in data buffer */ \
unsigned long channels;	/* # of data values per sample */ \
unsigned long limit;	/* extent of decomposition */ \
{ \
	register DATATYPE *detail; \
	register DATATYPE *avg; \
	unsigned long img_size; \
	unsigned long half_size; \
	int do_free = 0; \
	unsigned long x, x_tmp, d, i, j; \
	register fastf_t onehalf = (fastf_t)0.5; \
\
	CK_POW_2( dimen ); \
\
	if ( ! tbuffer ) { \
		tbuffer = (DATATYPE *)bu_malloc( \
				(dimen/2) * channels * sizeof( *buffer ), \
				"1d wavelet buffer"); \
		do_free = 1; \
	} \
\
	/* each iteration of this loop decomposes the data into 2 halves: \
	 * the "average image" and the "image detail" \
	 */ \
	for (img_size = dimen ; img_size > limit ; img_size = half_size ){ \
\
		half_size = img_size/2; \
		 \
		detail = tbuffer; \
		avg = buffer; \
\
		for ( x=0 ; x < img_size ; x += 2 ) { \
			x_tmp = x*channels; \
\
			for (d=0 ; d < channels ; d++, avg++, detail++) { \
				i = x_tmp + d; \
				j = i + channels; \
				*detail = (buffer[i] - buffer[j]) * onehalf; \
				*avg    = (buffer[i] + buffer[j]) * onehalf; \
			} \
		} \
\
		/* "avg" now points to the first element AFTER the "average \
		 * image" section, and hence is the START of the "image  \
		 * detail" portion.  Convenient, since we now want to copy \
		 * the contents of "tbuffer" (which holds the image detail) into \
		 * place. \
		 */ \
		memcpy(avg, tbuffer, sizeof(*buffer) * channels * half_size); \
	} \
	 \
	if (do_free) \
		bu_free( (genptr_t)tbuffer, "1d wavelet buffer"); \
}


#if defined(__STDC__)
#define reconstruct(DATATYPE ) bn_wlt_haar_1d_ ## DATATYPE ## _reconstruct
#else
#define reconstruct(DATATYPE) bn_wlt_haar_1d_/**/DATATYPE/**/_reconstruct
#endif

#define make_wlt_haar_1d_reconstruct( DATATYPE ) \
void \
reconstruct(DATATYPE) \
( tbuffer, buffer, dimen, channels, subimage_size, limit )\
DATATYPE *tbuffer; \
DATATYPE *buffer; \
unsigned long dimen; \
unsigned long channels; \
unsigned long subimage_size; \
unsigned long limit; \
{ \
	register DATATYPE *detail; \
	register DATATYPE *avg; \
	unsigned long img_size; \
	unsigned long dbl_size; \
	int do_free = 0; \
	unsigned long x_tmp, d, x, i, j; \
\
	CK_POW_2( subimage_size ); \
	CK_POW_2( dimen ); \
	CK_POW_2( limit ); \
\
        if ( ! (subimage_size < dimen) ) { \
		bu_log("%s:%d Dimension %d should be greater than subimage size (%d)\n", \
			__FILE__, __LINE__, dimen, subimage_size); \
		bu_bomb("reconstruct"); \
	} \
\
        if ( ! (subimage_size < limit) ) { \
		bu_log("%s:%d Channels limit %d should be greater than subimage size (%d)\n", \
			__FILE__, __LINE__, limit, subimage_size); \
		bu_bomb("reconstruct"); \
	} \
\
        if ( ! (limit <= dimen) ) { \
		bu_log("%s:%d Dimension %d should be greater than or equal to the channels limit (%d)\n", \
			__FILE__, __LINE__, dimen, limit); \
		bu_bomb("reconstruct"); \
	} \
\
\
	if ( ! tbuffer ) { \
		tbuffer = ( DATATYPE *)bu_malloc((dimen/2) * channels * sizeof( *buffer ), \
				"1d wavelet reconstruct tmp buffer"); \
		do_free = 1; \
	} \
\
	/* Each iteration of this loop reconstructs an image twice as \
	 * large as the original using a "detail image". \
	 */ \
	for (img_size=subimage_size ; img_size < limit ; img_size=dbl_size) { \
		dbl_size = img_size * 2; \
\
		d = img_size * channels; \
		detail = &buffer[ d ]; \
\
		/* copy the original or "average" data to temporary buffer */ \
		avg = tbuffer; \
		memcpy(avg, buffer, sizeof(*buffer) * d ); \
\
\
		for (x=0 ; x < dbl_size ; x += 2 ) { \
			x_tmp = x * channels; \
			for (d=0 ; d < channels ; d++, avg++, detail++ ) { \
				i = x_tmp + d; \
				j = i + channels; \
				buffer[i] = *avg + *detail; \
				buffer[j] = *avg - *detail; \
			} \
		} \
	} \
\
	if (do_free) \
		bu_free( (genptr_t)tbuffer, \
			"1d wavelet reconstruct tmp buffer"); \
}

/* Believe it or not, this is where the actual code is generated */

make_wlt_haar_1d_decompose(double)
make_wlt_haar_1d_reconstruct(double)

make_wlt_haar_1d_decompose(float)
make_wlt_haar_1d_reconstruct(float)

make_wlt_haar_1d_decompose(char)
make_wlt_haar_1d_reconstruct(char)

make_wlt_haar_1d_decompose(int)
make_wlt_haar_1d_reconstruct(int)

make_wlt_haar_1d_decompose(short)
make_wlt_haar_1d_reconstruct(short)

make_wlt_haar_1d_decompose(long)
make_wlt_haar_1d_reconstruct(long)


#ifdef __STDC__
#define decompose_2d( DATATYPE ) bn_wlt_haar_2d_ ## DATATYPE ## _decompose
#else
#define decompose_2d(DATATYPE) bn_wlt_haar_2d_/* */DATATYPE/* */_decompose
#endif

#define make_wlt_haar_2d_decompose(DATATYPE) \
void \
decompose_2d(DATATYPE) \
(tbuffer, buffer, dimen, channels, limit) \
DATATYPE *tbuffer; \
DATATYPE *buffer; \
unsigned long dimen; \
unsigned long channels; \
unsigned long limit; \
{ \
	register DATATYPE *detail; \
	register DATATYPE *avg; \
	unsigned long img_size; \
	unsigned long half_size; \
	unsigned long x, y, x_tmp, y_tmp, d, i, j; \
	register fastf_t onehalf = (fastf_t)0.5; \
\
	CK_POW_2( dimen ); \
\
	if ( ! tbuffer ) { \
		tbuffer = (DATATYPE *)bu_malloc( \
				(dimen/2) * channels * sizeof( *buffer ), \
				"1d wavelet buffer"); \
	} \
\
	/* each iteration of this loop decomposes the data into 4 quarters: \
	 * the "average image", the horizontal detail, the vertical detail \
	 * and the horizontal-vertical detail \
	 */ \
	for (img_size = dimen ; img_size > limit ; img_size = half_size ) { \
		half_size = img_size/2; \
\
		/* do a horizontal detail decomposition first */ \
		for (y=0 ; y < img_size ; y++ ) { \
			y_tmp = y * dimen * channels; \
\
			detail = tbuffer; \
			avg = &buffer[y_tmp]; \
\
			for (x=0 ; x < img_size ; x += 2 ) { \
				x_tmp = x*channels + y_tmp; \
\
				for (d=0 ; d < channels ; d++, avg++, detail++){ \
					i = x_tmp + d; \
					j = i + channels; \
					*detail = (buffer[i] - buffer[j]) * onehalf; \
					*avg    = (buffer[i] + buffer[j]) * onehalf; \
				} \
			} \
			/* "avg" now points to the first element AFTER the \
			 * "average image" section, and hence is the START \
			 * of the "image detail" portion.  Convenient, since \
			 * we now want to copy the contents of "tbuffer" (which \
			 * holds the image detail) into place. \
			 */ \
			memcpy(avg, tbuffer, sizeof(*buffer) * channels * half_size); \
		} \
\
		/* Now do the vertical decomposition */ \
		for (x=0 ; x < img_size ; x ++ ) { \
			x_tmp = x*channels; \
\
			detail = tbuffer; \
			avg = &buffer[x_tmp]; \
\
			for (y=0 ; y < img_size ; y += 2) { \
				y_tmp =y*dimen*channels + x_tmp; \
\
				for (d=0 ; d < channels ; d++, avg++, detail++) { \
					i = y_tmp + d; \
					j = i + dimen*channels; \
					*detail = (buffer[i] - buffer[j]) * onehalf; \
					*avg    = (buffer[i] + buffer[j]) * onehalf; \
				} \
				avg += (dimen-1)*channels; \
			} \
\
			/* "avg" now points to the element ABOVE the \
			 * last "average image" pixel or the first "detail" \
			 * location in the user buffer. \
			 * \
			 * There is no memcpy for the columns, so we have to \
			 * copy the data back to the user buffer ourselves. \
			 */ \
			detail = tbuffer; \
			for (y=half_size ; y < img_size ; y++) { \
				for (d=0; d < channels ; d++) { \
					*avg++ = *detail++; \
				} \
				avg += (dimen-1)*channels; \
			} \
		} \
	} \
}


#ifdef __STDC__
#define reconstruct_2d( DATATYPE ) bn_wlt_haar_2d_ ## DATATYPE ## _reconstruct
#else
#define reconstruct_2d(DATATYPE) bn_wlt_haar_2d_/* */DATATYPE/* */_reconstruct
#endif

#define make_wlt_haar_2d_reconstruct(DATATYPE) \
void \
reconstruct_2d(DATATYPE) \
(tbuf, buf, width, channels, avg_size, limit) \
DATATYPE *tbuf; \
DATATYPE *buf; \
unsigned long width; \
unsigned long channels; \
unsigned long avg_size; \
unsigned long limit; \
{ \
	register DATATYPE *detail; \
	register DATATYPE *avg; \
	unsigned long img_size; \
	unsigned long dbl_size; \
	unsigned long x_tmp, d, x, i, j; \
	unsigned long y, row_len, row_start; \
 \
	CK_POW_2( avg_size ); \
	CK_POW_2( width ); \
	CK_POW_2( limit ); \
 \
	/* XXX check for: \
	 * subimage_size < dimen && subimage_size < limit \
	 * limit <= dimen \
	 */ \
 \
 \
	if ( ! tbuf ) { \
		tbuf = ( DATATYPE *)bu_malloc((width/2) * channels * sizeof( *buf ), \
				"1d wavelet reconstruct tmp buffer"); \
	} \
 \
	row_len = width * channels; \
 \
	/* Each iteration of this loop reconstructs an image twice as \
	 * large as the original using a "detail image". \
	 */ \
 \
	for (img_size = avg_size ; img_size < limit ; img_size = dbl_size) { \
		dbl_size = img_size * 2; \
		 \
		 \
		/* first is a vertical reconstruction */ \
		for (x=0 ; x < dbl_size ; x++ ) { \
			/* reconstruct column x */ \
 \
			/* copy column of "average" data to tbuf */ \
			x_tmp = x*channels; \
			for (y=0 ; y < img_size ; y++) { \
				i = x_tmp + y*row_len; \
				j = y * channels; \
				for (d=0 ; d < channels ; d++) { \
					tbuf[j++] = buf[i++]; \
				} \
			} \
			avg = tbuf; \
			detail = &buf[x_tmp + img_size*row_len]; \
 \
			/* reconstruct column */ \
			for (y=0 ; y < dbl_size ; y += 2) { \
 \
				i = x_tmp + y*row_len; \
				j = i + row_len; \
 \
				for (d=0 ; d < channels ; d++,avg++,detail++){ \
					buf[i++] = *avg + *detail; \
					buf[j++] = *avg - *detail; \
				} \
				detail += row_len - channels; \
			} \
		} \
 \
		/* now a horizontal reconstruction */ \
		for (y=0 ; y < dbl_size ; y++ ) { \
			/* reconstruct row y */ \
 \
			/* copy "average" row to tbuf and set pointer to \
			 * begining of "detail" \
			 */ \
			d = img_size * channels; \
			row_start = y*row_len; \
 \
 \
			avg = &buf[ row_start ]; \
			detail = &buf[ row_start + d]; \
 \
			memcpy(tbuf, avg, sizeof(*buf) * d ); \
			avg = tbuf; \
 \
			/* reconstruct row */ \
			for (x=0 ; x < dbl_size ; x += 2 ) { \
				x_tmp = x * channels; \
				i = row_start + x * channels; \
				j = i + channels; \
 \
				for (d=0 ; d < channels ; d++,avg++,detail++){ \
					buf[i++] = *avg + *detail; \
					buf[j++] = *avg - *detail; \
				} \
			} \
		} \
	} \
}


make_wlt_haar_2d_decompose(double)
make_wlt_haar_2d_decompose(float)
make_wlt_haar_2d_decompose(char)
make_wlt_haar_2d_decompose(int)
make_wlt_haar_2d_decompose(short)
make_wlt_haar_2d_decompose(long)

make_wlt_haar_2d_reconstruct(double)
make_wlt_haar_2d_reconstruct(float)
make_wlt_haar_2d_reconstruct(char)
make_wlt_haar_2d_reconstruct(int)
make_wlt_haar_2d_reconstruct(short)
make_wlt_haar_2d_reconstruct(long)



#ifdef __STDC__
#define decompose_2d_2( DATATYPE ) bn_wlt_haar_2d_ ## DATATYPE ## _decompose2
#else
#define decompose_2d_2(DATATYPE) bn_wlt_haar_2d_/* */DATATYPE/* */_decompose2
#endif

#define make_wlt_haar_2d_decompose2(DATATYPE) \
void \
decompose_2d_2(DATATYPE) \
(tbuffer, buffer, width, height, channels, limit) \
DATATYPE *tbuffer; \
DATATYPE *buffer; \
unsigned long width; \
unsigned long height; \
unsigned long channels; \
unsigned long limit; \
{ \
	register DATATYPE *detail; \
	register DATATYPE *avg; \
	unsigned long img_wsize; \
	unsigned long img_hsize; \
	unsigned long half_wsize; \
	unsigned long half_hsize; \
	unsigned long x, y, x_tmp, y_tmp, d, i, j; \
	register fastf_t onehalf = (fastf_t)0.5; \
\
	CK_POW_2( width ); \
	CK_POW_2( height ); \
\
        /* create a temp buffer the half the size of the larger dimension \
         */ \
	if ( ! tbuffer ) { \
		tbuffer = (DATATYPE *)bu_malloc( \
				(((width>height)?width:height)/2) * channels * sizeof( *buffer ), \
				"1d wavelet buffer"); \
	} \
\
	/* each iteration of this loop decomposes the data into 4 quarters: \
	 * the "average image", the horizontal detail, the vertical detail \
	 * and the horizontal-vertical detail \
	 */ \
	for (img_wsize = width, img_hsize = height ; (img_wsize > limit) && (img_hsize > limit) ; img_wsize = half_wsize, img_hsize = half_hsize ) { \
		half_wsize = img_wsize/2; \
		half_hsize = img_hsize/2; \
\
		/* do a horizontal detail decomposition first */ \
		for (y=0 ; y < img_hsize ; y++ ) { \
			y_tmp = y * width * channels; \
\
			detail = tbuffer; \
			avg = &buffer[y_tmp]; \
\
			for (x=0 ; x < img_wsize ; x += 2 ) { \
				x_tmp = x*channels + y_tmp; \
\
				for (d=0 ; d < channels ; d++, avg++, detail++){ \
					i = x_tmp + d; \
					j = i + channels; \
					*detail = (buffer[i] - buffer[j]) * onehalf; \
					*avg    = (buffer[i] + buffer[j]) * onehalf; \
				} \
			} \
			/* "avg" now points to the first element AFTER the \
			 * "average image" section, and hence is the START \
			 * of the "image detail" portion.  Convenient, since \
			 * we now want to copy the contents of "tbuffer" (which \
			 * holds the image detail) into place. \
			 */ \
			memcpy(avg, tbuffer, sizeof(*buffer) * channels * half_wsize); \
		} \
\
		/* Now do the vertical decomposition */ \
		for (x=0 ; x < img_wsize ; x ++ ) { \
			x_tmp = x*channels; \
\
			detail = tbuffer; \
			avg = &buffer[x_tmp]; \
\
			for (y=0 ; y < img_hsize ; y += 2) { \
				y_tmp =y*width*channels + x_tmp; \
\
				for (d=0 ; d < channels ; d++, avg++, detail++) { \
					i = y_tmp + d; \
					j = i + width*channels; \
					*detail = (buffer[i] - buffer[j]) * onehalf; \
					*avg    = (buffer[i] + buffer[j]) * onehalf; \
				} \
				avg += (width-1)*channels; \
			} \
\
			/* "avg" now points to the element ABOVE the \
			 * last "average image" pixel or the first "detail" \
			 * location in the user buffer. \
			 * \
			 * There is no memcpy for the columns, so we have to \
			 * copy the data back to the user buffer ourselves. \
			 */ \
			detail = tbuffer; \
			for (y=half_hsize ; y < img_hsize ; y++) { \
				for (d=0; d < channels ; d++) { \
					*avg++ = *detail++; \
				} \
				avg += (width-1)*channels; \
			} \
		} \
	} \
}

make_wlt_haar_2d_decompose2(double)
make_wlt_haar_2d_decompose2(float)
make_wlt_haar_2d_decompose2(char)
make_wlt_haar_2d_decompose2(int)
make_wlt_haar_2d_decompose2(short)
make_wlt_haar_2d_decompose2(long)



/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
