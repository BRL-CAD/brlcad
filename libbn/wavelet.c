/*			W A V E L E T . C
 *
 *  Wavelet decompose/reconstruct operations
 *
 *	bn_wlt_1d_double_decompose(tbuf, buf, dimen, depth, limit)
 *	bn_wlt_1d_float_decompose (tbuf, buf, dimen, depth, limit)
 *	bn_wlt_1d_char_decompose  (tbuf, buf, dimen, depth, limit)
 *	bn_wlt_1d_short_decompose (tbuf, buf, dimen, depth, limit)
 *	bn_wlt_1d_int_decompose   (tbuf, buf, dimen, depth, limit)
 *	bn_wlt_1d_long_decompose  (tbuf, buf, dimen, depth, limit)
 *
 *	bn_wlt_1d_double_reconstruct(tbuf, buf, dimen, depth, sub_sz, limit)
 *	bn_wlt_1d_float_reconstruct (tbuf, buf, dimen, depth, sub_sz, limit)
 *	bn_wlt_1d_char_reconstruct  (tbuf, buf, dimen, depth, sub_sz, limit)
 *	bn_wlt_1d_short_reconstruct (tbuf, buf, dimen, depth, sub_sz, limit)
 *	bn_wlt_1d_int_reconstruct   (tbuf, buf, dimen, depth, sub_sz, limit)
 *	bn_wlt_1d_long_reconstruct  (tbuf, buf, dimen, depth, sub_sz, limit)
 *
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
 *  bn_wlt_1d_*_decompose(tbuf, buf, dimen, depth, limit)
 *  Parameters:
 *	tbuf	a temporary data buffer 1/2 as large as "buf". See (1) below.
 *	buf	pointer to the data to be decomposed
 *	dimen	The number of samples in the data buffer 
 *	depth	The number of values per sample
 *	limit	The extent of the decomposition
 *
 *  Perform a Haar wavelet decomposition on the data in buffer "buf".  The
 *  decomposition is done "in place" on the data, hence the values in "buf"
 *  are not preserved, but rather replaced by their decomposition.
 *  The number of original samples in the buffer (parameter "dimen") and the
 *  decomposition limit ("limit") must both be a power of 2 (e.g. 512, 1024).
 *  The buffer is decomposed into "average" and "detail" halves until the
 *  size of the "average" portion reaches "limit".  Simultaneous 
 *  decomposition of multi-plane (e.g. pixel) data, can be performed by
 *  indicating the number of planes in the "depth" parameter.
 *  
 *  (1) The process requires a temporary buffer which is 1/2 the size of the
 *  longest span to be decomposed.  If the "tbuf" argument is non-null then
 *  it is a pointer to a temporary buffer.  If the pointer is NULL, then a
 *  local temporary buffer will be allocated (and freed).
 *
 *  Examples:
 *	double dbuf[512], cbuf[256];
 *	...
 *	bn_wlt_1d_double_decompose(cbuf, dbuf, 512, 1, 1);
 *
 *    performs complete decomposition on the data in array "dbuf".
 *
 *	double buf[3][512];	 /_* 512 samples, 3 values/sample (e.g. RGB?)*_/
 *	double tbuf[3][256];	 /_* the temporary buffer *_/
 *	...
 *	bn_wlt_1d_double_decompose(tbuf, buf, 512, 3, 1);
 *
 *    This will completely decompose the data in buf.  The first sample will
 *    be the average of all the samples.  Alternatively:
 *
 *	bn_wlt_1d_double_decompose(tbuf, buf, 512, 3, 64);
 *
 *    decomposes buf into a 64-sample "average image" and 3 "detail" sets.
 *
 *
 *
 *  bn_wlt_1d_*_reconstruct(tbuf, buf, dimen, depth, sub_sz, limit)
 *
 *
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif
#include <stdio.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"

#define CK_POW_2(dimen) { register unsigned long j; register int ok;\
	for (ok=0, j=0 ; j < sizeof(unsigned long) * 8 ; j++) { \
		if ( (1<<j) == dimen) { ok = 1;  break; } \
	} \
	if ( ! ok ) { \
		bu_log("%s:%d Dimension %d should be power of 2 (%d)\n", \
			__FILE__, __LINE__, dimen, j); \
		bu_bomb("CK_POW_2"); \
	} \
}



#ifdef __STDC__
#define decompose_1d(DATATYPE) bn_wlt_1d_ ## DATATYPE ## _decompose
#else
#define decompose_1d(DATATYPE) bn_wlt_1d_/**/DATATYPE/**/_decompose
#endif



#define make_wlt_1d_decompose(DATATYPE)  \
void \
decompose_1d(DATATYPE) \
( tbuf, buf, dimen, depth, limit ) \
DATATYPE *tbuf;		/* temporary buffer */ \
DATATYPE *buf;		/* data buffer */ \
unsigned long dimen;	/* # of samples in data buffer */ \
unsigned long depth;	/* # of data values per sample */ \
unsigned long limit;	/* extent of decomposition */ \
{ \
	register DATATYPE *detail; \
	register DATATYPE *avg; \
	unsigned long img_size; \
	unsigned long half_size; \
	int do_free = 0; \
	unsigned long x, x_tmp, d, i, j; \
\
	CK_POW_2( dimen ); \
\
	if ( ! tbuf ) { \
		tbuf = (DATATYPE *)bu_malloc( \
				(dimen/2) * depth * sizeof( *buf ), \
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
		detail = tbuf; \
		avg = buf; \
\
		for ( x=0 ; x < img_size ; x += 2 ) { \
			x_tmp = x*depth; \
\
			for (d=0 ; d < depth ; d++, avg++, detail++) { \
				i = x_tmp + d; \
				j = i + depth; \
				*detail = (buf[i] - buf[j]) / 2.0; \
				*avg    = (buf[i] + buf[j]) / 2.0; \
			} \
		} \
\
		/* "avg" now points to the first element AFTER the "average \
		 * image" section, and hence is the START of the "image  \
		 * detail" portion.  Convenient, since we now want to copy \
		 * the contents of "tbuf" (which holds the image detail) into \
		 * place. \
		 */ \
		memcpy(avg, tbuf, sizeof(*buf) * depth * half_size); \
	} \
	 \
	if (do_free) \
		bu_free( (genptr_t)tbuf, "1d wavelet buffer"); \
}


#if defined(__STDC__) 
#define reconstruct(DATATYPE ) bn_wlt_1d_ ## DATATYPE ## _reconstruct
#else
#define reconstruct(DATATYPE) bn_wlt_1d_/**/DATATYPE/**/_reconstruct
#endif

#define make_wlt_1d_reconstruct( DATATYPE ) \
void \
reconstruct(DATATYPE) \
( tbuf, buf, dimen, depth, subimage_size, limit )\
DATATYPE *tbuf; \
DATATYPE *buf; \
unsigned long dimen; \
unsigned long depth; \
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
	/* XXX check for: \
	 * subimage_size < dimen && subimage_size < limit \
	 * limit <= dimen \
	 */ \
\
	if ( ! tbuf ) { \
		tbuf = ( DATATYPE *)bu_malloc((dimen/2) * depth * sizeof( *buf ), \
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
		d = img_size * depth; \
		detail = &buf[ d ]; \
\
		/* copy the original or "average" data to temporary buffer */ \
		avg = tbuf; \
		memcpy(avg, buf, sizeof(*buf) * d ); \
\
\
		for (x=0 ; x < dbl_size ; x += 2 ) { \
			x_tmp = x * depth; \
			for (d=0 ; d < depth ; d++, avg++, detail++ ) { \
				i = x_tmp + d; \
				j = i + depth; \
				buf[i] = *avg + *detail; \
				buf[j] = *avg - *detail; \
			} \
		} \
	} \
\
	if (do_free) \
		bu_free( (genptr_t)tbuf, \
			"1d wavelet reconstruct tmp buffer"); \
}

/* Believe it or not, this is where the actual code is generated */

make_wlt_1d_decompose(double)
make_wlt_1d_reconstruct(double)

make_wlt_1d_decompose(float)
make_wlt_1d_reconstruct(float)

make_wlt_1d_decompose(char)
make_wlt_1d_reconstruct(char)

make_wlt_1d_decompose(int)
make_wlt_1d_reconstruct(int)

make_wlt_1d_decompose(short)
make_wlt_1d_reconstruct(short)

make_wlt_1d_decompose(long)
make_wlt_1d_reconstruct(long)


#ifdef __STDC__
#define decompose_2d( DATATYPE ) bn_wlt_2d_ ## DATATYPE ## _decompose
#else
#define decompose_2d(DATATYPE) bn_wlt_2d_/* */DATATYPE/* */_decompose
#endif

#define make_wlt_2d_decompose(DATATYPE) \
void \
decompose_2d(DATATYPE) \
(tbuf, buf, dimen, depth, limit) \
DATATYPE *tbuf; \
DATATYPE *buf; \
unsigned long dimen; \
unsigned long depth; \
unsigned long limit; \
{ \
	register DATATYPE *detail; \
	register DATATYPE *avg; \
	register DATATYPE *ptr; \
	unsigned long img_size; \
	unsigned long half_size; \
	unsigned long x, y, x_tmp, y_tmp, d, i, j; \
	int do_free; \
\
	CK_POW_2( dimen ); \
\
	if ( ! tbuf ) { \
		tbuf = (DATATYPE *)bu_malloc( \
				(dimen/2) * depth * sizeof( *buf ), \
				"1d wavelet buffer"); \
		do_free = 1; \
	} else { \
		do_free = 0; \
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
			y_tmp = y * dimen * depth; \
\
			detail = tbuf; \
			avg = &buf[y_tmp]; \
\
			for (x=0 ; x < img_size ; x += 2 ) { \
				x_tmp = x*depth + y_tmp; \
\
				for (d=0 ; d < depth ; d++, avg++, detail++){ \
					i = x_tmp + d; \
					j = i + depth; \
					*detail = (buf[i] - buf[j]) / 2.0; \
					*avg    = (buf[i] + buf[j]) / 2.0; \
				} \
			} \
			/* "avg" now points to the first element AFTER the \
			 * "average image" section, and hence is the START \
			 * of the "image detail" portion.  Convenient, since \
			 * we now want to copy the contents of "tbuf" (which \
			 * holds the image detail) into place. \
			 */ \
			memcpy(avg, tbuf, sizeof(*buf) * depth * half_size); \
		} \
\
		/* Now do the vertical decomposition */ \
		for (x=0 ; x < img_size ; x ++ ) { \
			x_tmp = x*depth; \
\
			detail = tbuf; \
			avg = &buf[x_tmp]; \
\
			for (y=0 ; y < img_size ; y += 2) { \
				y_tmp =y*dimen*depth + x_tmp; \
\
				for (d=0 ; d < depth ; d++, avg++, detail++) { \
					i = y_tmp + d; \
					j = i + dimen*depth; \
					*detail = (buf[i] - buf[j]) / 2.0; \
					*avg    = (buf[i] + buf[j]) / 2.0; \
				} \
				avg += (dimen-1)*depth; \
			} \
\
			/* "avg" now points to the element ABOVE the \
			 * last "average image" pixel or the first "detail" \
			 * location in the user buffer. \
			 * \
			 * There is no memcpy for the columns, so we have to \
			 * copy the data back to the user buffer ourselves. \
			 */ \
			detail = tbuf; \
			for (y=half_size ; y < img_size ; y++) { \
				for (d=0; d < depth ; d++) { \
					*avg++ = *detail++; \
				} \
				avg += (dimen-1)*depth; \
			} \
		} \
	} \
}

#define make_wlt_2d_reconstruct(DATATYPE) /* DATATYPE */

make_wlt_2d_decompose(double)
make_wlt_2d_decompose(float)
make_wlt_2d_decompose(char)
make_wlt_2d_decompose(int)
make_wlt_2d_decompose(short)
make_wlt_2d_decompose(long)

