/*
 *			F I X P T . H
 *
 *  Data structures and macros for dealing with integer-based
 *  fixed point math.
 *
 *  These macros use integer instructions for
 *  a special "fixed point" format.
 *  The fractional part is stored in 28-bit integer form,
 *  to prevent roundoff errors.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited
 *
 *  $Header$
 */

#ifndef FIXPT_H
#define FIXPT_H seen

#define	FIXPT_SCALE	((1<<28)-1)
struct fixpt  {
	int	i;
	int	f;		/* Ranges 0 to FIXPT_SCALE-1 */
};

#define FIXPT_FLOAT( fp, fl )	{ \
	register double d = fl; \
	fp.f = (d - (fp.i = (int)d)) * FIXPT_SCALE; \
	FIXPT_NORMALIZE(fp); }

#define FLOAT_FIXPT( fp )  ( fp.i + ((double)fp.f)/FIXPT_SCALE )

#define FIXPT_NORMALIZE(fp)	{ \
	if( fp.f < 0 )  { \
		do {  \
			fp.i--; \
			fp.f += FIXPT_SCALE; \
		} while( fp.f < 0 ); \
	} else if( fp.f >= FIXPT_SCALE )  { \
		do { \
			fp.i++; \
			fp.f -= FIXPT_SCALE; \
		} while( fp.f >= FIXPT_SCALE ); \
	} }

#define FIXPT_ROUND(fp)		{ \
	if( fp.f > FIXPT_SCALE/2 )  { \
		if( fp.i >= 0 ) fp.i++; \
		else fp.i--; \
	}  fp.f = 0; }

#define FIXPT_ADD2( o, a, b )	{\
	o.i = a.i + b.i; \
	o.f = a.f + b.f; \
	FIXPT_NORMALIZE(o); }

#define PR_FIX( str, fp )	bu_log("%s = %2d+x%8.8x\n", str, fp.i, fp.f )

#define PR_FIX2( str, fp )	bu_log("%s = (%2d+x%8.8x,%2d+x%8.8x)\n", \
				str, fp[0].i, fp[0].f, fp[1].i, fp[1].f )

#endif /* FIXPT_H */
