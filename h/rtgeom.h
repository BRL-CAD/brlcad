/*
 *			R T G E O M . H
 *
 *  Details of the internal forms used by the LIBRT geometry routines
 *  for the different solids.
 *
 *  These structures are what the struct rt_db_internal
 *  generic pointer idb_ptr points at,
 *  based on idb_type indicating a solid id ID_xxx, such as ID_TGC.
 *
 *  Depends on having machine.h, vmath.h, and rtlist.h included first.
 *  RT_xxx_CK_MAGIC() can only be used if raytrace.h is included too.
 *
 *  XXX Should structure names start with rt_ ?  Probably yes.
 *
 *  Author -
 *	Michael John Muuss
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

/*
 *	ID_TOR
 */
struct rt_tor_internal {
	long	magic;
	point_t	v;
	vect_t	h;		/* r_h length */
	vect_t	a;		/* r_a length */
	vect_t	b;		/* r_b length */
	fastf_t	r_h;		/* radius in H direction */
	fastf_t	r_a;		/* radius in A direction */
	fastf_t	r_b;		/* radius in B direction (typ == r_a) */
};
#define RT_TOR_INTERNAL_MAGIC	0x9bffed887
#define RT_TOR_CK_MAGIC(_p)	RT_CKMAG(_p,RT_TOR_INTERNAL_MAGIC,"rt_tor_internal")

/*
 *	ID_TGC and ID_REC
 */
struct rt_tgc_internal {
	long	magic;
	vect_t	v;
	vect_t	h;
	vect_t	a;
	vect_t	b;
	vect_t	c;
	vect_t	d;
};
#define RT_TGC_INTERNAL_MAGIC	0xaabbdd87
#define RT_TGC_CK_MAGIC(_p)	RT_CKMAG(_p,RT_TGC_INTERNAL_MAGIC,"rt_tgc_internal")

/*
 *	ID_ELL, and ID_SPH
 */
struct rt_ell_internal  {
	long	magic;
	point_t	v;
	vect_t	a;
	vect_t	b;
	vect_t	c;
};
#define RT_ELL_INTERNAL_MAGIC	0x93bb23ff
#define RT_ELL_CK_MAGIC(_p)	RT_CKMAG(_p,RT_ELL_INTERNAL_MAGIC,"rt_ell_internal")

/*
 *	ID_ARB8
 *
 *  The internal (in memory) form of an ARB8 -- 8 points in space.
 *  The first 4 form the "bottom" face, the second 4 form the "top" face.
 */
struct rt_arb_internal {
	long	magic;
	point_t	pt[8];
};
#define RT_ARB_INTERNAL_MAGIC	0x9befd010
#define RT_ARB_CK_MAGIC(_p)	RT_CKMAG(_p,RT_ARB_INTERNAL_MAGIC,"rt_arb_internal")

/*
 *	ID_ARS
 */
struct rt_ars_internal {
	int	magic;
	int	ncurves;
	int	pts_per_curve;
	fastf_t	**curves;
};
#define RT_ARS_INTERNAL_MAGIC	0x77ddbbe3
#define RT_ARS_CK_MAGIC(_p)	RT_CKMAG(_p,RT_ARS_INTERNAL_MAGIC,"rt_ars_internal")

/*
 *	ID_HALF
 */
struct rt_half_internal  {
	long	magic;
	plane_t	eqn;
};
#define RT_HALF_INTERNAL_MAGIC	0xaabbdd87
#define RT_HALF_CK_MAGIC(_p)	RT_CKMAG(_p,RT_HALF_INTERNAL_MAGIC,"rt_half_internal")

/*
 *	ID_POLY
 */
struct rt_pg_internal {
	long	magic;
	int	npoly;
	struct rt_pg_face_internal {
		int	npts;		/* number of points for this polygon */
		fastf_t	*verts;		/* has 3*npts elements */
		fastf_t	*norms;		/* has 3*npts elements */
	} *poly;			/* has npoly elements */
	/* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
	int	max_npts;		/* maximum value of npts in poly[] */
};
#define RT_PG_INTERNAL_MAGIC	0x9bffed887
#define RT_PG_CK_MAGIC(_p)	RT_CKMAG(_p,RT_PG_INTERNAL_MAGIC,"rt_pg_internal")

/* ID_BSPLINE */

/* ID_NMG */

/*
 *	ID_EBM
 */
#define RT_EBM_NAME_LEN 256
struct rt_ebm_internal  {
	long		magic;
	char		file[RT_EBM_NAME_LEN];
	int		xdim;		/* X dimension (w cells) */
	int		ydim;		/* Y dimension (n cells) */
	double		tallness;	/* Z dimension (mm) */
	/* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
	unsigned char	*map;		/* actual bitmap, with padding */
};
#define RT_EBM_INTERNAL_MAGIC	0xf901b231
#define RT_EBM_CK_MAGIC(_p)	RT_CKMAG(_p,RT_EBM_INTERNAL_MAGIC,"rt_ebm_internal")

/*
 *	ID_VOL
 */
#define RT_VOL_NAME_LEN 128
struct rt_vol_internal  {
	long		magic;
	char		file[RT_VOL_NAME_LEN];
	int		xdim;		/* X dimension */
	int		ydim;		/* Y dimension */
	int		zdim;		/* Z dimension */
	int		lo;		/* Low threshold */
	int		hi;		/* High threshold */
	vect_t		cellsize;	/* ideal coords: size of each cell */
	/* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
	unsigned char	*map;
};
#define RT_VOL_INTERNAL_MAGIC	0x987ba1d0
#define RT_VOL_CK_MAGIC(_p)	RT_CKMAG(_p,RT_VOL_INTERNAL_MAGIC,"rt_vol_internal")

/*
 *	ID_ARBN
 */
struct rt_arbn_internal  {
	long	magic;
	int	neqn;
	plane_t	*eqn;
};
#define RT_ARBN_INTERNAL_MAGIC	0x18236461
#define RT_ARBN_CK_MAGIC(_p)	RT_CKMAG(_p,RT_ARBN_INTERNAL_MAGIC,"rt_arbn_internal")

/*
 *	ID_PIPE
 */
struct rt_pipe_internal {
	int		pipe_magic;
	int		pipe_count;
	struct rt_list	pipe_segs_head;
};
#define RT_PIPE_INTERNAL_MAGIC	0x77ddbbe3
#define RT_PIPE_CK_MAGIC(_p)	RT_CKMAG(_p,RT_PIPE_INTERNAL_MAGIC,"rt_pipe_internal")

/*
 *	ID_PARTICLE
 */
struct rt_part_internal {
	long	part_magic;
	point_t	part_V;
	vect_t	part_H;
	fastf_t	part_vrad;
	fastf_t	part_hrad;
	int	part_type;		/* sphere, cylinder, cone */
};
#define RT_PART_INTERNAL_MAGIC	0xaaccee87
#define RT_PART_CK_MAGIC(_p)	RT_CKMAG(_p,RT_PART_INTERNAL_MAGIC,"rt_part_internal")

#define RT_PARTICLE_TYPE_SPHERE		1
#define RT_PARTICLE_TYPE_CYLINDER	2
#define RT_PARTICLE_TYPE_CONE		3
