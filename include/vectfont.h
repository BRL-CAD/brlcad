/*
 *			V E C T F O N T . H
 *
 *  Vector font definitions, for TIG-PACK fonts.
 *  Used by LIBPLOT3 and LIBRT for simple vector fonts.
 *
 *  Author -
 *	Michael John Muuss
 *	Douglas A. Gwyn
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 *
 *  $Header$
 */

/*
 *	Motion encoding macros
 *
 * All characters reference absolute points within a 10 x 10 square
 */
#define	brt(x,y)	(11*x+y)
#define drk(x,y)	-(11*x+y)
#define	LAST		-128		/* 0200 Marks end of stroke list */
#define	NEGY		-127		/* 0201 Denotes negative y stroke */
#define bneg(x,y)	NEGY, brt(x,y)
#define dneg(x,y)	NEGY, drk(x,y)

#if defined(CRAY1) || defined(CRAY2) || defined(mips)
#define TINY	int
#else
#define TINY	char		/* must be signed */
#endif

extern TINY	*tp_cindex[256];	/* index to stroke tokens */
extern TINY	tp_ctable[];		/* table of strokes */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
