#include <stdio.h>
#include "machine.h"
#include "bu.h"
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


void
bn_wlt_1d_double_decompose(
	double *tbuf,		/* temporary buffer */
	double *buf,		/* data buffer */
	unsigned long dimen,	/* # of samples in data buffer */
	unsigned long depth,	/* # of data values per sample */
 	unsigned long limit )	/* extent of decomposition */
{
	double *detail;
	double *avg;
	unsigned long img_size;
	unsigned long half_size;
	int do_free = 0;
	unsigned long x, x_tmp, d, i, j;

	CK_POW_2( dimen );

	if ( ! tbuf ) {
		tbuf = (double *)bu_malloc((dimen/2) * depth * sizeof( *buf ),
				"1d 'double' wavelet buffer");
		do_free = 1;
	}

	for (img_size = dimen ; img_size > limit ; img_size = half_size ){
		half_size = img_size/2;
		
		detail = tbuf;
		avg = buf;

		for ( x=0 ; x < img_size ; x += 2 ) {
			x_tmp = x*depth;

			for (d=0 ; d < depth ; d++, avg++, detail++) {
				i = x_tmp + d;
				j = i + depth;
				*detail = (buf[i] - buf[j]) / 2.0;
				*avg    = (buf[i] + buf[j]) / 2.0;
			}
		}

		memcpy(&buf[ half_size*depth ], tbuf,
			sizeof(*buf) * depth * half_size);

	}
	
	if (do_free)
		bu_free( tbuf, "1d 'double' wavelet buffer");
}

void
bn_wlt_1d_double_reconstruct(
	double *tbuf,
	double *buf,
	unsigned long dimen,
	unsigned long depth,
	unsigned long subimage_size,
	unsigned long limit )
{
	register double *detail;
	register double *avg;
	unsigned long img_size;
	unsigned long dbl_size;
	int do_free = 0;
	unsigned long x_tmp, d, x, i, j;

	CK_POW_2( subimage_size );
	CK_POW_2( dimen );
	CK_POW_2( limit );

	/* XXX check for:
	 * subimage_size < dimen && subimage_size < limit
	 * limit <= dimen
	 */

	if ( ! tbuf ) {
		tbuf = (double *)bu_malloc((dimen/2) * depth * sizeof( *buf ),
				"1d 'double' wavelet buffer");
		do_free = 1;
	}


	for (img_size=subimage_size ; img_size < limit ; img_size=dbl_size) {
		dbl_size = img_size * 2;

		memcpy(tbuf, buf, sizeof(*buf) * depth * img_size);

		detail = &buf[ img_size * depth ];
		avg = tbuf;

		for (x=0 ; x < dbl_size ; x += 2 ) {
			x_tmp = x * depth;
			for (d=0 ; d < depth ; d++, avg++, detail++ ) {
				i = x_tmp + d;
				j = i + depth;
				buf[i] = *avg + *detail;
				buf[j] = *avg - *detail;
			}
		}
	}

	if (do_free)
		bu_free( tbuf, "1d 'double' wavelet buffer");
}
