/*
 *			R A Y . H
 *
 * $Revision$
 */

/*
 * All necessary information about a ray.
 */
struct ray {
	point_t		r_pt;		/* Point at which ray starts */
	vect_t		r_dir;		/* Direction of ray (UNIT Length) */
	float		r_min;		/* entry dist to bounding sphere */
	float		r_max;		/* exit dist from bounding sphere */
	struct ray	*r_forw;	/* !0 -> ray after deflection */
};

/*
 *  Information about where a ray hits the surface
 *
 * Important Note:  Surface Normals always point OUT of a solid.
 */
struct hit {
	point_t		hit_point;	/* Intersection point */
	vect_t		hit_normal;	/* Surface Normal at hit_point */
	float		hit_dist;	/* dist from r_pt to hit_point */
};

/*
 * Intersection segment.
 *
 * Includes information about both endpoints of intersection.
 * Contains forward link to additional intersection segments
 * if the intersection spans multiple segments (eg, shooting
 * a ray through a torus).
 */
struct seg {
	int		seg_flag;
#define SEG_IN		0x4		/* IN point, normal, computed */
#define SEG_OUT		0x8		/* OUT point, normal, computed */
	struct hit	seg_in;		/* IN information */
	struct hit	seg_out;	/* OUT information */
	struct soltab	*seg_stp;	/* pointer back to soltab */
	struct seg	*seg_next;	/* non-zero if more segments */
};

/*
 * Internal information used to keep track of solids in the model
 */
struct soltab {
	int	st_id;		/* Solid ident */
	vect_t	st_center;	/* Center of bounding Sphere */
	float	st_radsq;	/* Bounding sphere Radius, squared */
	int	*st_specific;	/* Ptr to type-specific (private) struct */
	struct soltab *st_forw;	/* Linked list of solids */
	char	*st_name;	/* Name of solid */
};

/*
 *  Values for Solid ID.
 */
#define ID_NULL		0	/* Unused */
#define ID_TOR		1	/* Toroid */
#define ID_TGC		2	/* Generalized Truncated General Cone */
#define ID_ELL		3	/* Ellipsoid */
#define ID_ARB8		4	/* Generalized ARB.  V + 7 vectors */
#define ID_ARS		5	/* ARS */
#define ID_HALF		6	/* Half-space */

struct functab {
	int	(*ft_prep)();
	int	(*ft_shot)();
	int	(*ft_print)();
};

#define EPSILON		0.0001
#define NEAR_ZERO(f)	( ((f) < 0) ? ((f) > -EPSILON) : ((f) < EPSILON) )
