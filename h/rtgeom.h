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
struct tor_internal {
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
#define RT_TOR_CK_MAGIC(_p)	RT_CKMAG(_p,RT_TOR_INTERNAL_MAGIC,"tor_internal")

/*
 *	ID_TGC and ID_REC
 */
struct tgc_internal {
	long	magic;
	vect_t	v;
	vect_t	h;
	vect_t	a;
	vect_t	b;
	vect_t	c;
	vect_t	d;
};
#define RT_TGC_INTERNAL_MAGIC	0xaabbdd87
#define RT_TGC_CK_MAGIC(_p)	RT_CKMAG(_p,RT_TGC_INTERNAL_MAGIC,"tgc_internal")

/* ID_ELL */

/* ID_ARB8 */

/*
 *	ID_ARS
 */
struct ars_internal {
	int	magic;
	int	ncurves;
	int	pts_per_curve;
	fastf_t	**curves;
};
#define RT_ARS_INTERNAL_MAGIC	0x77ddbbe3
#define RT_ARS_CK_MAGIC(_p)	RT_CKMAG(_p,RT_ARS_INTERNAL_MAGIC,"ars_internal")

/*
 *	ID_HALF
 */
struct half_internal  {
	long	magic;
	plane_t	eqn;
};
#define RT_HALF_INTERNAL_MAGIC	0xaabbdd87
#define RT_HALF_CK_MAGIC(_p)	RT_CKMAG(_p,RT_HALF_INTERNAL_MAGIC,"half_internal")

/* ID_POLY */

/* ID_BSPLINE */

/* ID_SPH */

/* ID_NMG */

/* ID_EBM */

/* ID_VOL */

/*
 *	ID_ARBN
 */
struct arbn_internal  {
	long	magic;
	int	neqn;
	plane_t	*eqn;
};
#define RT_ARBN_INTERNAL_MAGIC	0x18236461
#define RT_ARBN_CK_MAGIC(_p)	RT_CKMAG(_p,RT_ARBN_INTERNAL_MAGIC,"arbn_internal")

/*
 *	ID_PIPE
 */
struct pipe_internal {
	int		pipe_magic;
	int		pipe_count;
	struct rt_list	pipe_segs_head;
};
#define RT_PIPE_INTERNAL_MAGIC	0x77ddbbe3
#define RT_PIPE_CK_MAGIC(_p)	RT_CKMAG(_p,RT_PIPE_INTERNAL_MAGIC,"pipe_internal")

/*
 *	ID_PARTICLE
 */
struct part_internal {
	long	part_magic;
	point_t	part_V;
	vect_t	part_H;
	fastf_t	part_vrad;
	fastf_t	part_hrad;
	int	part_type;		/* sphere, cylinder, cone */
};
#define RT_PART_INTERNAL_MAGIC	0xaaccee87
#define RT_PART_CK_MAGIC(_p)	RT_CKMAG(_p,RT_PART_INTERNAL_MAGIC,"part_internal")

#define RT_PARTICLE_TYPE_SPHERE		1
#define RT_PARTICLE_TYPE_CYLINDER	2
#define RT_PARTICLE_TYPE_CONE		3
