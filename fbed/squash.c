/*
	SCCS id:	@(#) squash.c	2.1
	Modified: 	12/9/86 at 15:54:49
	Retrieved: 	12/26/86 at 21:54:46
	SCCS archive:	/vld/moss/src/fbed/s.squash.c

	Author:		Paul R. Stay
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6640 or AV-298-6640
*/
/* 
 * squash.c - Filter super-sampled image for one scan line
 */
#if ! defined( lint )
static
char	sccsTag[] = "@(#) squash.c 2.1, modified 12/9/86 at 15:54:49, archive /vld/moss/src/fbed/s.squash.c";
#endif
#include <stdio.h>
/* Cone filtering weights. 
 * #define CNTR_WT 0.23971778
 * #define MID_WT  0.11985889
 * #define CRNR_WT 0.07021166
 */

/* Gaussian filtering weights. */
#define CNTR_WT 0.3011592441
#define MID_WT 0.1238102667
#define CRNR_WT 0.0508999223

/*	Squash takes three super-sampled "bit arrays", and returns an array
	of intensities at half the resolution.  N is the size of the bit
	arrays.  The "bit arrays" are actually int arrays whose values are
	assumed to be only 0 or 1.
 */
void
squash( buf0, buf1, buf2, ret_buf, n )
register int	*buf0, *buf1, *buf2;	
register float	*ret_buf;
register int	n;
	{
	register int     j;

	for( j = 1; j < n - 1; j++ )
		{
		ret_buf[j] =
			(
			buf2[j - 1] * CRNR_WT +
			buf2[j] * MID_WT +
			buf2[j + 1] * CRNR_WT +
			buf1[j - 1] * MID_WT +
			buf1[j] * CNTR_WT +
			buf1[j + 1] * MID_WT +
			buf0[j - 1] * CRNR_WT +
			buf0[j] * MID_WT +
			buf0[j + 1] * CRNR_WT
			);
		}
	return;
	}
