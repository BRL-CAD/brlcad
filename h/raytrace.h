/*
 *			R A Y T R A C E . H
 *
 * All the structures necessary for dealing with the RT ray tracer library.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 *
 *  $Header$
 */

/*
 *  Definitions necessary to operate in a parallel environment.
 */
extern int	res_pt;			/* lock on free partition structs */
extern int	res_seg;		/* lock on free seg structs */
extern int	res_malloc;		/* lock on memory allocation */
extern int	res_printf;		/* lock on printing */
#ifdef HEP
/* full means resource free, empty means resource busy */
#define	RES_ACQUIRE(ptr)	(void)Daread(ptr)	/* wait full set empty */
#define RES_RELEASE(ptr)	(void)Daset(ptr,3)	/* set full */
#else
#define RES_ACQUIRE(ptr)	;
#define RES_RELEASE(ptr)	;
#endif

/*
 * Handy memory allocator
 */

/* Acquire storage for a given struct, eg, GETSTRUCT(ptr,structname); */
#define GETSTRUCT(p,str) \
	p = (struct str *)vmalloc(sizeof(struct str), "getstruct str"); \
	if( p == (struct str *)0 ) \
		exit(17); \
	bzero( (char *)p, sizeof(struct str));


/*
 *			X R A Y
 *
 * All necessary information about a ray.
 * Not called just "ray" to prevent conflicts with other VLD stuff.
 */
struct xray {
	point_t		r_pt;		/* Point at which ray starts */
	vect_t		r_dir;		/* Direction of ray (UNIT Length) */
	fastf_t		r_min;		/* entry dist to bounding sphere */
	fastf_t		r_max;		/* exit dist from bounding sphere */
	struct xray	*r_forw;	/* !0 -> ray after deflection */
};
#define RAY_NULL	((struct xray *)0)


/*
 *			H I T
 *
 *  Information about where a ray hits the surface
 *
 * Important Note:  Surface Normals always point OUT of a solid.
 */
struct hit {
	point_t		hit_point;	/* Intersection point */
	vect_t		hit_normal;	/* Surface Normal at hit_point */
	fastf_t		hit_dist;	/* dist from r_pt to hit_point */
};
#define HIT_NULL	((struct hit *)0)



/*
 *			S E G
 *
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
	union tree	*seg_tp;	/* pointer to solid leaf node */
	struct seg	*seg_next;	/* non-zero if more segments */
};
#define SEG_NULL	((struct seg *)0)
extern struct seg *FreeSeg;		/* Head of freelist */
#define GET_SEG(p)    {	RES_ACQUIRE(&res_seg); \
			while( ((p)=FreeSeg) == SEG_NULL ) \
				get_seg(); \
			FreeSeg = (p)->seg_next; \
			p->seg_next = SEG_NULL; \
			RES_RELEASE(&res_seg); }
#define FREE_SEG(p)   {	RES_ACQUIRE(&res_seg); \
			(p)->seg_next = FreeSeg; FreeSeg = (p); \
			RES_RELEASE(&res_seg); }


/*
 *			S O L T A B
 *
 * Internal information used to keep track of solids in the model
 * Leaf name and Xform matrix are unique identifier.
 */
struct soltab {
	int		st_id;		/* Solid ident */
	vect_t		st_center;	/* Center of bounding Sphere */
	fastf_t		st_radsq;	/* Bounding sphere Radius, squared */
	int		*st_specific;	/* -> ID-specific (private) struct */
	struct soltab	*st_forw;	/* Linked list of solids */
	char		*st_name;	/* Leaf name of solid */
	vect_t		st_min;		/* min X, Y, Z of bounding RPP */
	vect_t		st_max;		/* max X, Y, Z of bounding RPP */
/*** bin is NOT PARALLEL ** */
	int		st_bin;		/* Temporary for boolean processing */
	int		st_uses;	/* # of refs by tr_stp leaves */
	mat_t		st_pathmat;	/* Xform matrix on path */
};
#define SOLTAB_NULL	((struct soltab *)0)

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
#define ID_REC		7	/* Right Elliptical Cylinder [TGC special] */

struct functab {
	int		(*ft_prep)();
	struct seg 	*((*ft_shot)());
	int		(*ft_print)();
	char		*ft_name;
};
extern struct functab functab[];

#define EPSILON		0.0001
#define NEAR_ZERO(f)	( ((f) > -EPSILON) && ((f) < EPSILON) )
#define INFINITY	100000000.0


/*
 *			T R E E
 *
 *  Binary trees representing the Boolean operations which
 *  combine one or more solids into a region.
 */
#define MKOP(x)		((x)<<1)
#define OP_BINARY	0x01			/* Binary/Unary (leaf) flag */
#define BINOP(x)	((x)->tr_op & OP_BINARY)

#define OP_SOLID	MKOP(1)			/* Leaf:  tr_stp -> solid */
#define OP_UNION	MKOP(2)|OP_BINARY	/* Binary: L union R */
#define OP_INTERSECT	MKOP(3)|OP_BINARY	/* Binary: L intersect R */
#define OP_SUBTRACT	MKOP(4)|OP_BINARY	/* Binary: L subtract R */

#define TFLAG_NOBOUND	1			/* skip bounding RPP check */

union tree {
	int	tr_op;		/* Operation */
	struct tree_binary {
		int		tb_op;		/* | OP_BINARY */
		int		tb_flags;
		vect_t		tb_min;		/* subtree min pt of RPP */
		vect_t		tb_max;		/* subtree max pt of RPP */
		union tree	*tb_left;
		union tree	*tb_right;
	} tr_b;
	struct tree_unary {
		int		tu_op;		/* leaf, OP_SOLID */
		int		tu_flags;
		vect_t		tu_min;		/* subtree min pt of RPP */
		vect_t		tu_max;		/* subtree max pt of RPP */
		struct soltab	*tu_stp;
		struct region	*tu_regionp;	/* ptr to containing region */
		char		*tu_name;	/* full path name of leaf */
		char		*tu_materp;	/* (struct mater *) */
	} tr_a;
};
#define tr_left		tr_b.tb_left
#define tr_right	tr_b.tb_right
#define tr_flags	tr_b.tb_flags
#define tr_min		tr_b.tb_min
#define tr_max		tr_b.tb_max
#define tr_stp		tr_a.tu_stp
#define tr_regionp	tr_a.tu_regionp
#define tr_name		tr_a.tu_name
#define tr_materp	tr_a.tu_materp
#define TREE_NULL	((union tree *)0)



/*
 *			R E G I O N
 *
 *  The region structure.
 */
struct region  {
	char		*reg_name;	/* Identifying string */
	union tree	*reg_treetop;	/* Pointer to boolean tree */
	short		reg_regionid;	/* Region ID code;  index to ? */
	short		reg_aircode;	/* ?? */
	short		reg_material;	/* Material */
	short		reg_los;	/* equivalent LOS estimate ?? */
	struct region	*reg_forw;	/* linked list of all regions */
	struct region	*reg_active;	/* linked list of hit regions */
};
#define REGION_NULL	((struct region *)0)



/*
 *  			P A R T I T I O N
 *
 *  Partitions of a ray
 */
#define NBINS	200			/* # bins: # solids hit in ray */

struct partition {
	unsigned char	pt_solhit[NBINS];	/* marks for solids hit */
	struct seg	*pt_inseg;		/* IN seg ptr (gives stp) */
	struct hit	*pt_inhit;		/* IN hit pointer */
	struct seg	*pt_outseg;		/* OUT seg pointer */
	struct hit	*pt_outhit;		/* OUT hit ptr */
	struct region	*pt_regionp;		/* ptr to containing region */
	struct partition *pt_forw;		/* forwards link */
	struct partition *pt_back;		/* backwards link */
	char		pt_inflip;		/* flip inhit->hit_normal */
	char		pt_outflip;		/* flip outhit->hit_normal */
};
#define pt_indist	pt_inhit->hit_dist
#define pt_outdist	pt_outhit->hit_dist

#define PT_NULL	((struct partition *)0)
extern struct partition *FreePart;		 /* Head of freelist */

/* Initialize all the bins to FALSE, clear out structure */
#define GET_PT_INIT(p)	\
	{ GET_PT(p);bzero( ((char *) (p)), sizeof(struct partition) ); }

#define GET_PT(p)   { RES_ACQUIRE(&res_pt); \
			while( ((p)=FreePart) == PT_NULL ) \
				get_pt(); \
			FreePart = (p)->pt_forw; \
			RES_RELEASE(&res_pt); }
#define FREE_PT(p) { RES_ACQUIRE(&res_pt); \
			(p)->pt_forw = FreePart; FreePart = (p); \
			RES_RELEASE(&res_pt); }

/* Insert "new" partition in front of "old" partition */
#define INSERT_PT(new,old)	{ \
	(new)->pt_back = (old)->pt_back; \
	(old)->pt_back = (new); \
	(new)->pt_forw = (old); \
	(new)->pt_back->pt_forw = (new);  }

/* Append "new" partition after "old" partition */
#define APPEND_PT(new,old)	{ \
	(new)->pt_forw = (old)->pt_forw; \
	(new)->pt_back = (old); \
	(old)->pt_forw = (new); \
	(new)->pt_forw->pt_back = (new);  }

/* Dequeue "cur" partition from doubly-linked list */
#define DEQUEUE_PT(cur)	{ \
	(cur)->pt_forw->pt_back = (cur)->pt_back; \
	(cur)->pt_back->pt_forw = (cur)->pt_forw;  }

/*
 *		A P P L I C A T I O N
 *
 * Note:  When calling shootray(), these fields are mandatory:
 *	a_ray.r_pt	Starting point of ray to be fired
 *	a_ray.r_dir	UNIT VECTOR with direction to fire in (dir cosines)
 *	a_hit		Routine to call when something is hit
 *	a_miss		Routine to call when ray misses everything
 *
 * Also note that shootray() returns the return of a_hit/a_miss().
 */
struct application  {
	/* THESE ELEMENTS ARE MANDATORY */
	struct xray a_ray;	/* Actual ray to be shot */
	int	(*a_hit)();	/* routine to call when shot hits model */
	int	(*a_miss)();	/* routine to call when shot misses */
	/* THE FOLLOWING ROUTINES ARE MAINLINE & APPLICATION SPECIFIC */
	int	a_x;		/* Screen X of ray, where applicable */
	int	a_y;		/* Screen Y of ray, where applicable */
	int	a_user;		/* application-specific value */
	int	(*a_init)();	/* routine to init application */
	int	(*a_eol)();	/* routine for end of scan-line */
	int	(*a_end)();	/* routine to end application */
};

/*
 *  Global variables used by the RT library.
 */
extern int ged_fd;		/* fd of object file */
extern int one_hit_flag;	/* non-zero to return first hit only */
extern int debug;		/* non-zero for debugging, see debug.h */
extern long nsolids;		/* total # of solids participating */
extern long nregions;		/* total # of regions participating */
extern long nshots;		/* # of calls to ft_shot() */
extern long nmiss_model;	/* rays missed model RPP */
extern long nmiss_tree;		/* rays missed sub-tree RPP */
extern long nmiss_solid;	/* rays missed solid RPP */
extern long nmiss;		/* solid ft_shot() returned a miss */
extern long nhits;		/* solid ft_shot() returned a hit */
extern struct soltab *HeadSolid;/* pointer to list of solids in model */
extern vect_t mdl_min;		/* min corner of model bounding RPP */
extern vect_t mdl_max;		/* max corner of model bounding RPP */

/*
 *  Global routines to interface with the RT library.
 */
extern int get_tree();			/* Get expr tree for object */
extern int shootray();			/* Shoot a ray */
extern void rtbomb();			/* Exit with error message */
extern void prep_timer();		/* Start the timer */
extern double pr_timer();		/* Stop timer, print, return time */
extern int dir_build();			/* Read named GED db, build toc */
extern void pr_seg();			/* Print seg struct */
extern void pr_partitions();		/* Print the partitions */

extern char *vmalloc();			/* visible malloc() */
extern void vfree();			/* visible free() */

/* The matrix math routines */
extern void mat_zero(), mat_idn(), mat_copy(), mat_mul(), matXvec();
extern void mat_inv(), mat_trn(), mat_ae(), mat_angles();
extern void vtoh_move(), htov_move(), mat_print();

/*
 *  Internal routines in RT library.
 *  Not for general use.
 */
extern struct directory *dir_lookup();	/* Look up name in toc */
extern struct directory *dir_add();	/* Add name to toc */
extern char *strdup();			/* Duplicate str w/malloc */
extern void bool_regions();		/* Eval booleans */
extern int bool_eval();			/* Eval bool tree node */
extern int fdiff();			/* Approx Floating compare */
extern double reldiff();		/* Relative Difference */
extern void pr_bins();			/* Print bins */
extern void pr_region();		/* Print a region */
extern void pr_tree();			/* Print an expr tree */
extern void fastf_float();		/* convert float->fastf_t */
extern void get_seg(), get_pt();	/* storage obtainers */

/*
 *  Library routines used by the RT library.
 */
extern long	lseek();
extern int	read(), write();
extern char	*malloc();
extern void	free();
