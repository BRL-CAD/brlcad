/*
 *			M S R . H
 *
 * Define data structures and constants for the "MSR" random number package.
 *
 * Also define a set of macros to access the random number tables
 * and to limit the area/volume that a set of random numbers inhabit.
 *
 * Note:  Application must include "machine.h" and "vmath.h" so that
 * #include of raytrace.h will work.  Necessary because of use of
 * rt_malloc() by the library routines.
 *
 * Note: MSRMAXTBL must be an even number.
 *
 *  Author -
 *	Christopher T. Johnson
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 *
 *  $Header$
 */

#if !defined(MACHINE_H)
# include "machine.h"
#endif
#if !defined(VMATH_H)
# include "vmath.h"
#endif
#if !defined(RAYTRACE_H)
# include "raytrace.h"	/* needed for RT_CKMAG definition */
#endif


#define	MSRMAXTBL	4096	/* Size of random number tables. */
#define MSR_UNIF_MAGIC	12481632
#define MSR_GAUSS_MAGIC 512256128

#define MSR_CK_UNIF(_p) RT_CKMAG(_p, MSR_UNIF_MAGIC, "msr_unif")
#define MSR_CK_GAUSS(_p) RT_CKMAG(_p, MSR_GAUSS_MAGIC, "msr_gauss")

struct msr_unif {
	long	magic;
	long	msr_seed;
	int	msr_double_ptr;
	double	*msr_doubles;
	int	msr_long_ptr;
	long	*msr_longs;
};
/*
 * NOTE!!! The order of msr_gauss and msr_unif MUST match in the
 * first three entries as msr_gauss is passed as a msr_unif in
 * msr_gauss_fill.
 */
struct msr_gauss {
	long	magic;
	long	msr_gauss_seed;
	int	msr_gauss_dbl_ptr;
	double	*msr_gauss_doubles;
	int	msr_gauss_ptr;
	double	*msr_gausses;
};
double msr_unif_double_fill();
double msr_gauss_fill();
struct msr_unif *msr_unif_init();
struct msr_gauss *msr_gauss_init();

#define	MSR_UNIF_LONG(_p)	\
	 (((_p)->msr_long_ptr ) ? \
		(_p)->msr_longs[--(_p)->msr_long_ptr] : \
		msr_unif_long_fill(_p))
#define MSR_UNIF_DOUBLE(_p)	\
	(((_p)->msr_double_ptr) ? \
		(_p)->msr_doubles[--(_p)->msr_double_ptr] : \
		msr_unif_double_fill(_p))

#define MSR_UNIF_CIRCLE(_p,_x,_y,_r) { \
	do { \
		(_x) = 2.0*MSR_UNIF_DOUBLE((_p)); \
		(_y) = 2.0*MSR_UNIF_DOUBLE((_p)); \
		(_r) = (_x)*(_x)+(_y)*(_y); \
	} while ((_r) >= 1.0);  }

#define	MSR_UNIF_SPHERE(_p,_x,_y,_z,_r) { \
	do { \
		(_x) = 2.0*MSR_UNIF_DOUBLE(_p); \
		(_y) = 2.0*MSR_UNIF_DOUBLE(_p); \
		(_z) = 2.0*MSR_UNIF_DOUBLE(_p); \
		(_r) = (_x)*(_x)+(_y)*(_y)+(_z)*(_z);\
	} while ((_r) >= 1.0) }

#define	MSR_GAUSS_DOUBLE(_p)	\
	(((_p)->msr_gauss_ptr) ? \
		(_p)->msr_gausses[--(_p)->msr_gauss_ptr] : \
		msr_gauss_fill(_p))
