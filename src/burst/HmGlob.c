/*
	Author:	Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066

	$Header$ (BRL)
*/
/*LINTLIBRARY*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif
#include <stdio.h>
FILE	*HmTtyFp = NULL;   /* read keyboard, not stdin */
int	HmLftMenu = 1;	   /* default top-level menu position */
int	HmTopMenu = 1;
int	HmMaxVis = 10;	   /* default maximum menu items displayed */
int	HmLastMaxVis = 10; /* track changes in above parameter */
int	HmTtyFd; 	   /* read keyboard, not stdin */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
