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

#ifndef RAYTRACE_H
#define RAYTRACE_H seen

#define INFINITY	(1.0e20)

/*
 * Handy memory allocator
 */

/* Acquire storage for a given struct, eg, GETSTRUCT(ptr,structname); */
#define GETSTRUCT(p,str) \
	if( (p = (struct str *)rt_malloc(sizeof(struct str), \
	    "getstruct str")) == (struct str *)0 ) \
		exit(17); \
	bzero( (char *)p, sizeof(struct str));


/*
 *			X R A Y
 *
 * All necessary information about a ray.
 * Not called just "ray" to prevent conflicts with VLD stuff.
 */
struct xray {
	point_t		r_pt;		/* Point at which ray starts */
	vect_t		r_dir;		/* Direction of ray (UNIT Length) */
	fastf_t		r_min;		/* entry dist to bounding sphere */
	fastf_t		r_max;		/* exit dist from bounding sphere */
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
	fastf_t		hit_dist;	/* dist from r_pt to hit_point */
	point_t		hit_point;	/* Intersection point */
	vect_t		hit_normal;	/* Surface Normal at hit_point */
	vect_t		hit_vpriv;	/* private vector for xxx_*() */
	char		*hit_private;	/* private handle for xxx_shot() */
};
#define HIT_NULL	((struct hit *)0)


/*
 *			C U R V A T U R E
 *
 *  Information about curvature of the surface at a hit point.
 *  The principal direction pdir has unit length and principal curvature c1.
 *  Principal curvature c1 is <= c2.  A POSITIVE curvature in a
 *  particular direction indicates that the surface bends AWAY
 *  from the (outward pointing) normal vector.  (Thanks to GIFT).
 *  c1 and c2 are inverse radii.
 */
struct curvature {
	vect_t		crv_pdir;	/* Principle direction */
	fastf_t		crv_c1;		/* curvature in principle dir */
	fastf_t		crv_c2;		/* curvature in other direction */
};
#define CURVE_NULL	((struct curvature *)0)

/*
 *  Use this macro after having computed the normal, to
 *  compute the curvature at a hit point.
 */
#define RT_CURVE( curvp, hitp, stp )  { \
	register int id = (stp)->st_id; \
	if( id < 0 || id >= rt_nfunctab )  { \
		rt_log("stp=x%x, id=%d.\n", stp, id); \
		rt_bomb("RT_CURVE:  bad st_id"); \
	} \
	rt_functab[id].ft_curve( curvp, hitp, stp ); }

/*
 *			U V C O O R D
 *
 *  Mostly for texture mapping, information about parametric space.
 */
struct uvcoord {
	fastf_t		uv_u;		/* Range 0..1 */
	fastf_t		uv_v;		/* Range 0..1 */
	fastf_t		uv_du;		/* delta in u */
	fastf_t		uv_dv;		/* delta in v */
};


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
	struct hit	seg_in;		/* IN information */
	struct hit	seg_out;	/* OUT information */
	struct soltab	*seg_stp;	/* pointer back to soltab */
	struct seg	*seg_next;	/* non-zero if more segments */
};
#define SEG_NULL	((struct seg *)0)

#define GET_SEG(p,res)    { \
			while( ((p)=res->re_seg) == SEG_NULL ) \
				rt_get_seg(res); \
			res->re_seg = (p)->seg_next; \
			p->seg_next = SEG_NULL; \
			res->re_segget++; }

#define FREE_SEG(p,res)  { \
			(p)->seg_next = res->re_seg; \
			res->re_seg = (p); \
			res->re_segfree++; }


/*
 *			S O L T A B
 *
 * Internal information used to keep track of solids in the model
 * Leaf name and Xform matrix are unique identifier.
 */
struct soltab {
	int		st_id;		/* Solid ident */
	vect_t		st_center;	/* Centroid of solid */
	fastf_t		st_aradius;	/* Radius of APPROXIMATING sphere */
	fastf_t		st_bradius;	/* Radius of BOUNDING sphere */
	int		*st_specific;	/* -> ID-specific (private) struct */
	struct soltab	*st_forw;	/* Forward linked list of solids */
	struct soltab	*st_back;	/* Backward links */
	char		*st_name;	/* Leaf name of solid */
	vect_t		st_min;		/* min X, Y, Z of bounding RPP */
	vect_t		st_max;		/* max X, Y, Z of bounding RPP */
	int		st_bit;		/* solids bit vector index (const) */
	int		st_maxreg;	/* highest bit set in st_regions */
	bitv_t		*st_regions;	/* bit vect of region #'s (const) */
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
#define ID_POLY		8	/* Polygonal facted object */
#define ID_BSPLINE	9	/* B-spline object */
#define ID_SPH		10	/* Sphere */

/*
 *			F U N C T A B
 *
 *  Object-oriented interface to geometry modules.
 *  Table is indexed by ID_xxx value of particular solid.
 */
struct rt_functab {
	char		*ft_name;
	int		ft_use_rpp;
	int		(*ft_prep)();
	struct seg 	*((*ft_shot)());
	void		(*ft_print)();
	void		(*ft_norm)();
	void		(*ft_uv)();
	void		(*ft_curve)();
	int		(*ft_classify)();
	void		(*ft_free)();
	void		(*ft_plot)();
};
extern struct rt_functab rt_functab[];
extern int rt_nfunctab;


/*
 *			T R E E
 *
 *  Binary trees representing the Boolean operations between solids.
 */
#define MKOP(x)		((x))

#define OP_SOLID	MKOP(1)		/* Leaf:  tr_stp -> solid */
#define OP_UNION	MKOP(2)		/* Binary: L union R */
#define OP_INTERSECT	MKOP(3)		/* Binary: L intersect R */
#define OP_SUBTRACT	MKOP(4)		/* Binary: L subtract R */
#define OP_XOR		MKOP(5)		/* Binary: L xor R, not both*/
/* Internal */
#define OP_NOT		MKOP(6)		/* Unary:  not L */
#define OP_GUARD	MKOP(7)		/* Unary:  not L, or else! */
#define OP_XNOP		MKOP(8)		/* Unary:  L, mark region */

union tree {
	int	tr_op;		/* Operation */
	struct tree_node {
		int		tb_op;		/* non-leaf */
		struct region	*tb_regionp;	/* ptr to containing region */
		union tree	*tb_left;
		union tree	*tb_right;
	} tr_b;
	struct tree_leaf {
		int		tu_op;		/* leaf, OP_SOLID */
		struct region	*tu_regionp;	/* ptr to containing region */
		struct soltab	*tu_stp;
		char		*tu_name;	/* full path name of leaf */
	} tr_a;
};
/* Things which are in the same place in both structures */
#define tr_regionp	tr_a.tu_regionp

#define TREE_NULL	((union tree *)0)

/*
 *			M A T E R _ I N F O
 */
struct mater_info {
	float	ma_color[3];		/* explicit color:  0..1  */
	char	ma_override;		/* non-0 ==> ma_color is valid */
	char	ma_cinherit;		/* DB_INH_LOWER / DB_INH_HIGHER */
	char	ma_minherit;		/* DB_INH_LOWER / DB_INH_HIGHER */
	char	ma_matname[32];		/* Material name */
	char	ma_matparm[60];		/* String Material parms */
};

/*
 *			R E G I O N
 *
 *  The region structure.
 */
struct region  {
	char		*reg_name;	/* Identifying string */
	union tree	*reg_treetop;	/* Pointer to boolean tree */
	short		reg_bit;	/* constant index into Regions[] */
	short		reg_regionid;	/* Region ID code;  index to ? */
	short		reg_aircode;	/* ?? */
	short		reg_gmater;	/* GIFT Material code */
	short		reg_los;	/* equivalent LOS estimate ?? */
	struct region	*reg_forw;	/* linked list of all regions */
	struct mater_info reg_mater;	/* Real material information */
	char		*reg_mfuncs;	/* User appl. funcs for material */
	char		*reg_udata;	/* User appl. data for material */
	short		reg_transmit;	/* flag:  material transmits light */
};
#define REGION_NULL	((struct region *)0)

/*
 *  			P A R T I T I O N
 *
 *  Partitions of a ray.  Passed from rt_shootray() into user's
 *  a_hit() function.
 *
 *  Only the hit_dist field of pt_inhit and pt_outhit are valid
 *  when a_hit() is called;  to compute both hit_point and hit_normal,
 *  use RT_HIT_NORM() macro;  to compute just hit_point, use
 *  VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
 *
 *  NOTE:  rt_get_pt allows enough storage at the end of the partition
 *  for a bit vector of "rt_i.nsolids" bits in length.
 */
#define RT_HIT_NORM( hitp, stp, rayp )  { \
	register int id = (stp)->st_id; \
	if( id < 0 || id >= rt_nfunctab ) { \
		rt_log("stp=x%x, id=%d. hitp=x%x, rayp=x%x\n", stp, id, hitp, rayp); \
		rt_bomb("RT_HIT_NORM:  bad st_id");\
	} \
	rt_functab[id].ft_norm(hitp, stp, rayp); }

struct partition {
	struct seg	*pt_inseg;		/* IN seg ptr (gives stp) */
	struct hit	*pt_inhit;		/* IN hit pointer */
	struct seg	*pt_outseg;		/* OUT seg pointer */
	struct hit	*pt_outhit;		/* OUT hit ptr */
	struct region	*pt_regionp;		/* ptr to containing region */
	struct partition *pt_forw;		/* forwards link */
	struct partition *pt_back;		/* backwards link */
	char		pt_inflip;		/* flip inhit->hit_normal */
	char		pt_outflip;		/* flip outhit->hit_normal */
	bitv_t		pt_solhit[1];		/* VAR bit array:solids hit */
};
#define PT_NULL		((struct partition *)0)

#define COPY_PT(ip,out,in)	{ \
	bcopy((char *)in, (char *)out, ip->rti_pt_bytes); }

/* Initialize all the bits to FALSE, clear out structure */
#define GET_PT_INIT(ip,p,res)	{\
	GET_PT(ip,p,res); bzero( ((char *) (p)), (ip)->rti_pt_bytes ); }

#define GET_PT(ip,p,res)   { \
			while( ((p)=res->re_part) == PT_NULL ) \
				rt_get_pt(ip, res); \
			res->re_part = (p)->pt_forw; \
			res->re_partget++; }

#define FREE_PT(p,res)  { \
			(p)->pt_forw = res->re_part; \
			res->re_part = (p); \
			res->re_partfree++; }

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
 *  Bit vectors
 */
union bitv_elem {
	union bitv_elem	*be_next;
	bitv_t		be_v[2];
};
#define BITV_NULL	((union bitv_elem *)0)

#define GET_BITV(ip,p,res)  { \
			while( ((p)=res->re_bitv) == BITV_NULL ) \
				rt_get_bitv(ip,res); \
			res->re_bitv = (p)->be_next; \
			p->be_next = BITV_NULL; \
			res->re_bitvget++; }

#define FREE_BITV(p,res)  { \
			(p)->be_next = res->re_bitv; \
			res->re_bitv = (p); \
			res->re_bitvfree++; }

/*
 *  Bit-string manipulators for arbitrarily long bit strings
 *  stored as an array of bitv_t's.
 *  BITV_SHIFT and BITV_MASK are defined in machine.h
 */
#define BITS2BYTES(nbits) (((nbits)+BITV_MASK)/8)	/* conservative */
#define BITTEST(lp,bit)	\
	((lp[bit>>BITV_SHIFT] & (((bitv_t)1)<<(bit&BITV_MASK)))?1:0)
#define BITSET(lp,bit)	\
	(lp[bit>>BITV_SHIFT] |= (((bitv_t)1)<<(bit&BITV_MASK)))
#define BITCLR(lp,bit)	\
	(lp[bit>>BITV_SHIFT] &= ~(((bitv_t)1)<<(bit&BITV_MASK)))
#define BITZERO(lp,nbits) bzero((char *)lp, BITS2BYTES(nbits))

/*
 *			C U T
 *
 *  Structure for space subdivision.
 */
union cutter  {
#define CUT_CUTNODE	1
#define CUT_BOXNODE	2
	char	cut_type;
	union cutter *cut_forw;		/* Freelist forward link */
	struct cutnode  {
		char	cn_type;
		char	cn_axis;	/* 0,1,2 = cut along X,Y,Z */
		fastf_t	cn_point;	/* cut through axis==point */
		union cutter *cn_l;	/* val < point */
		union cutter *cn_r;	/* val >= point */
	} cn;
	struct boxnode  {
		char	bn_type;
		fastf_t	bn_min[3];
		fastf_t	bn_max[3];
		struct soltab **bn_list;
		short	bn_len;		/* # of solids in list */
		short	bn_maxlen;	/* # of ptrs allocated to list */
	} bn;
};
#define CUTTER_NULL	((union cutter *)0)

/*
 *			D I R E C T O R Y
 */
struct directory  {
	char		*d_namep;		/* pointer to name string */
	long		d_addr;			/* disk address in obj file */
	struct directory *d_forw;		/* link to next dir entry */
	struct animate	*d_animate;		/* link to animation */
};
#define DIR_NULL	((struct directory *)0)

/* Args to rt_dir_lookup() */
#define LOOKUP_NOISY	1
#define LOOKUP_QUIET	0

/*
 *			A N I M A T E
 *
 *  Each one of these structures specifies an arc in the tree that
 *  is to be operated on for animation purposes.  More than one
 *  animation operation may be applied at any given arc.  The directory
 *  structure points to a linked list of animate structures
 *  (built by rt_anim_add()), and the operations are processed in the
 *  order given.
 */
struct anim_mat {
	short		anm_op;			/* ANM_RSTACK, ANM_RARC... */
	mat_t		anm_mat;		/* Matrix */
};
#define ANM_RSTACK	1			/* Replace stacked matrix */
#define ANM_RARC	2			/* Replace arc matrix */
#define ANM_LMUL	3			/* Left (root side) mul */
#define ANM_RMUL	4			/* Right (leaf side) mul */
#define ANM_RBOTH	5			/* Replace stack, arc=Idn */

struct animate {
	struct animate	*an_forw;		/* forward link */
	struct directory **an_path;		/* pointer to path array */
	short		an_pathlen;		/* 0 = root */
	short		an_type;		/* AN_MATRIX, AN_COLOR... */
	union animate_specific {
		struct anim_mat anu_m;
	}		an_u;
};
#define AN_MATRIX	1			/* Matrix animation */
#define AN_PROPERTY	2			/* Material property anim */
#define AN_COLOR	3			/* Material color anim */
#define AN_SOLID	4			/* Solid parameter anim */

#define ANIM_NULL	((struct animate *)0)

/*
 *			R E S O U R C E
 *
 *  One of these structures is allocated per processor.
 *  To prevent excessive competition for free structures,
 *  memory is now allocated on a per-processor basis.
 *  The application structure a_resource element specifies
 *  the resource structure to be used;  if uniprocessing,
 *  a null a_resource pointer results in using the internal global
 *  structure, making initial application development simpler.
 *
 *  Note that if multiple models are being used, the partition and bitv
 *  structures (which are variable length) will require there to be
 *  ncpus * nmodels resource structures, the selection of which will
 *  be the responsibility of the application.
 */
struct resource {
	struct seg 	*re_seg;	/* Head of segment freelist */
	long		re_seglen;
	long		re_segget;
	long		re_segfree;
	struct partition *re_part;	/* Head of freelist */
	long		re_partlen;
	long		re_partget;
	long		re_partfree;
	union bitv_elem *re_bitv;	/* head of freelist */
	long		re_bitvlen;
	long		re_bitvget;
	long		re_bitvfree;
	int		re_cpu;		/* processor number, for ID */
};
#define RESOURCE_NULL	((struct resource *)0)


/*
 *			A P P L I C A T I O N
 *
 * Note:  When calling rt_shootray(), these fields are mandatory:
 *	a_ray.r_pt	Starting point of ray to be fired
 *	a_ray.r_dir	UNIT VECTOR with direction to fire in (dir cosines)
 *	a_hit		Routine to call when something is hit
 *	a_miss		Routine to call when ray misses everything
 *
 * Also note that rt_shootray() returns the (int) return of a_hit()/a_miss().
 */
struct application  {
	/* THESE ELEMENTS ARE MANDATORY */
	struct xray	a_ray;		/* Actual ray to be shot */
	int		(*a_hit)();	/* called when shot hits model */
	int		(*a_miss)();	/* called when shot misses */
	int		(*a_overlap)();	/* called when overlaps occur */
	int		a_level;	/* recursion level (for printing) */
	int		a_onehit;	/* flag to stop on first hit */
	struct rt_i	*a_rt_i;	/* this librt instance */
	struct resource	*a_resource;	/* dynamic memory resources */
	/* THE FOLLOWING ROUTINES ARE MAINLINE & APPLICATION SPECIFIC */
	int		a_x;		/* Screen X of ray, if applicable */
	int		a_y;		/* Screen Y of ray, if applicable */
	char		*a_purpose;	/* Debug string:  purpose of ray */
	int		a_user;		/* application-specific value */
	char		*a_uptr;	/* application-specific pointer */
	fastf_t		a_rbeam;	/* initial beam radius (mm) */
	fastf_t		a_diverge;	/* slope of beam divergance/mm */
	fastf_t		a_color[9];	/* application-specific color */
	vect_t		a_uvec;		/* application-specific vector */
	vect_t		a_vvec;		/* application-specific vector */
	fastf_t		a_refrac_index;	/* current index of refraction */
	fastf_t		a_cumlen;	/* cumulative length of ray */
};
#define RT_AFN_NULL	((int (*)())0)

/*
 *			R T _ G
 *
 *  Definitions for librt.a which are global to the library
 *  regardless of how many different models are being worked on
 */
struct rt_g {
	int		debug;		/* non-zero for debug, see debug.h */
	union cutter	*rtg_CutFree;	/* cut Freelist */
	/*  Definitions necessary to interlock in a parallel environment */
	int		rtg_parallel;	/* !0 = trying to use multi CPUs */
	int		res_syscall;	/* lock on system calls */
	int		res_worker;	/* lock on work to do */
	int		res_stats;	/* lock on statistics */
	int		res_results;	/* lock on result buffer */
	int		res_model;	/* lock on model growth (splines) */
	struct vlist	*rtg_vlFree;	/* vlist freelist */
};
extern struct rt_g rt_g;

/*
 *			R T _ I
 *
 *  Definitions for librt which are specific to the
 *  particular model being processed, one copy for each model.
 *  Initially, a pointer to this is returned from rt_dirbuild().
 */
struct rt_i {
	struct region	**Regions;	/* ptrs to regions [reg_bit] */
	struct soltab	*HeadSolid;	/* ptr to list of solids in model */
	struct region	*HeadRegion;	/* ptr of list of regions in model */
	char		*file;		/* name of file */
	FILE		*fp;		/* file handle of database */
	vect_t		mdl_min;	/* min corner of model bounding RPP */
	vect_t		mdl_max;	/* max corner of model bounding RPP */
	long		nregions;	/* total # of regions participating */
	long		nsolids;	/* total # of solids participating */
	long		nshots;		/* # of calls to ft_shot() */
	long		nmiss_model;	/* rays missed model RPP */
	long		nmiss_tree;	/* rays missed sub-tree RPP */
	long		nmiss_solid;	/* rays missed solid RPP */
	long		nmiss;		/* solid ft_shot() returned a miss */
	long		nhits;		/* solid ft_shot() returned a hit */
	int		needprep;	/* needs rt_prep */
	int		useair;		/* "air" regions are used */
	int		rti_nrays;	/* # calls to rt_shootray() */
	union cutter	rti_CutHead;	/* Head of cut tree */
	union cutter	rti_inf_box;	/* List of infinite solids */
	struct directory *rti_DirHead;	/* directory for this DB */
	union record	*rti_db;	/* in-core database, when needed */
	struct animate	*rti_anroot;	/* heads list of anim at root lvl */
	int		rti_pt_bytes;	/* length of partition struct */
	int		rti_bv_bytes;	/* length of BITV array */
	long		rti_magic;	/* magic # for integrity check */
	vect_t		rti_pmin;	/* for plotting, min RPP */
	vect_t		rti_pmax;	/* for plotting, max RPP */
	fastf_t		rti_pconv;	/* scale from rti_pmin */
	int		rti_nlights;	/* number of light sources */
};
#define RTI_NULL	((struct rt_i *)0)
#define RTI_MAGIC	0x01016580	/* magic # for integrity check */

/*
 *			V L I S T
 *
 *  Definitions for handling lists of vectors (really verticies, or points)
 *  in 3-space.
 *  Intented for common handling of wireframe display information.
 *  XXX For the moment, allocated with individual malloc() calls.
 */
struct vlist {
	point_t		vl_pnt;		/* coordinates in space */
	int		vl_draw;	/* 1=draw, 0=move */
	struct vlist	*vl_forw;	/* next structure in list */
};
#define VL_NULL		((struct vlist *)0)

struct vlhead {
	struct vlist	*vh_first;
	struct vlist	*vh_last;
};

#define GET_VL(p)	{ \
			if( ((p) = rt_g.rtg_vlFree) == VL_NULL )  { \
				(p) = (struct vlist *)rt_malloc(sizeof(struct vlist), "vlist"); \
			} else { \
				rt_g.rtg_vlFree = (p)->vl_forw; \
			} }

/* Free an entire chain of vlist structs */
#define FREE_VL(p)	{ register struct vlist *_vp = (p); \
			while( _vp->vl_forw != VL_NULL ) _vp=_vp->vl_forw; \
			_vp->vl_forw = rt_g.rtg_vlFree; \
			rt_g.rtg_vlFree = (p);  }

#define ADD_VL(hd,pnt,draw)  { \
			register struct vlist *_vp; \
			GET_VL(_vp); \
			VMOVE( _vp->vl_pnt, pnt ); \
			_vp->vl_draw = draw; \
			_vp->vl_forw = VL_NULL; \
			if( (hd)->vh_first == VL_NULL ) { \
				(hd)->vh_first = (hd)->vh_last = _vp; \
			} else { \
				(hd)->vh_last->vl_forw = _vp; \
				(hd)->vh_last = _vp; \
			} }

/*
 *  Replacements for definitions from ../h/vmath.h
 */
#undef VPRINT
#undef HPRINT
#define VPRINT(a,b)	rt_log("%s (%g, %g, %g)\n", a, (b)[0], (b)[1], (b)[2])
#define HPRINT(a,b)	rt_log("%s (%g, %g, %g, %g)\n", a, (b)[0], (b)[1], (b)[2], (b)[3])


/*****************************************************************
 *                                                               *
 *          Applications interface to the RT library             *
 *                                                               *
 *****************************************************************/

#ifdef __STDC__
extern void rt_bomb(char *str);		/* Fatal error */
extern void rt_log();			/* Log message */
					/* Read named MGED db, build toc */
extern struct rt_i *rt_dirbuild(char *filename, char *buf, int len);
					/* Prepare for raytracing */
extern void rt_prep(struct rt_i *rtip);
					/* Shoot a ray */
extern int rt_shootray(struct application *ap);
					/* Get expr tree for object */
extern int rt_gettree(struct rt_i *rtip, char *node);
					/* Print seg struct */
extern void rt_pr_seg(struct seg *segp);
					/* Print the partitions */
extern void rt_pr_partitions(struct rt_i *rtip,
	struct partition *phead, char *title);
					/* Find solid by leaf name */
extern struct soltab *rt_find_solid(struct rt_t *rtip, char *name);
					/* Start the timer */
extern void rt_prep_timer(void);
					/* Read timer, return time + str */
extern double rt_read_timer(char *str, int len);

#else

extern void rt_bomb();			/* Fatal error */
extern void rt_log();			/* Log message */

extern struct rt_i *rt_dirbuild();	/* Read named MGED db, build toc */
extern void rt_prep();			/* Prepare for raytracing */
extern int rt_shootray();		/* Shoot a ray */

extern int rt_gettree();		/* Get expr tree for object */
extern void rt_pr_seg();		/* Print seg struct */
extern void rt_pr_partitions();		/* Print the partitions */
extern struct soltab *rt_find_solid();	/* Find solid by leaf name */

extern void rt_prep_timer();		/* Start the timer */
extern double rt_read_timer();		/* Read timer, return time + str */
#endif

/* The matrix math routines */
extern void mat_zero(), mat_idn(), mat_copy(), mat_mul(), matXvec();
extern void mat_inv(), mat_trn(), mat_ae(), mat_angles();
extern void vtoh_move(), htov_move(), mat_print();
extern void eigen2x2(), mat_fromto(), mat_lookat();
extern double mat_atan2();

/*****************************************************************
 *                                                               *
 *  Internal routines in the RT library.  Not for Applications   *
 *                                                               *
 *****************************************************************/

#ifdef __STDC__
					/* visible malloc() */
extern char *rt_malloc(unsigned int cnt, char *str);
					/* visible free() */
extern void rt_free(char *ptr, char *str);
					/* Duplicate str w/malloc */
extern char *rt_strdup(char *cp);

					/* Look up name in toc */
extern struct directory *rt_dir_lookup(struct rt_i *rtip, char *str, int noisy);
					/* Add name to toc */
extern struct directory *rt_dir_add(struct rt_i *rtip, char *name, long laddr);
					/* Weave segs into partitions */
extern void rt_boolweave(struct seg *segp_in, struct partition *PartHeadp,
	struct application *ap);
					/* Eval booleans over partitions */
extern void rt_boolfinal(struct partition *InputHdp,
	struct partition *FinalHdp,
	fastf_t startdist, fastf_t enddist,
	bitv_t *regionbits, struct application *ap);
					/* Eval bool tree node */
extern int rt_booleval(union tree *treep, struct partition *partp,
	 struct region **trueregp);
					/* Approx Floating compare */
extern int rt_fdiff(double a, double b);
					/* Relative Difference */
extern double rt_reldiff(double a, double b);
					/* Print a region */
extern void rt_pr_region(struct region *rp);
					/* Print an expr tree */
extern void rt_pr_tree(union tree *tp, int lvl);
					/* Print a partition */
extern void rt_pr_pt(struct rt_i *rtip, struct partition *pp);
					/* Print a bit vector */
extern void rt_pr_bitv(char *str, bitv_t *bv, int len);
					/* Print a hit point */
extern void rt_pr_hit(char *str, struct hit *hitp);
					/* convert dbfloat->fastf_t */
/* XXX these next two should be dbfloat_t, but that means
 * XXX including db.h in absolutely everything.  No way.
 */
extern void rt_fastf_float(fastf_t *ff, float *fp, int n);
					/* convert dbfloat mat->fastf_t */
extern void rt_mat_dbmat(fastf_t *ff, float *dbp);
					/* storage obtainers */
extern void rt_get_seg(struct resource *res);
extern void rt_get_pt(struct rt_i *rtip, struct resource *res);
extern void rt_get_bitv(struct rt_i *rtip, struct resource *res);
					/* malloc rounder */
extern int rt_byte_roundup(int nbytes);
/* ran out of energy here */
extern void rt_bitv_or();		/* logical OR on bit vectors */
extern void rt_cut_it();		/* space partitioning */
extern void rt_pr_cut();		/* print cut node */
extern void rt_draw_box();		/* unix-plot an RPP */
extern void rt_region_color_map();	/* regionid-driven color override */
extern void rt_color_addrec();		/* process ID_MATERIAL record */
extern void rt_cut_extend();		/* extend a cut box */
extern int rt_rpp_region();		/* find RPP of one region */

#else

extern char *rt_malloc();		/* visible malloc() */
extern void rt_free();			/* visible free() */
extern char *rt_strdup();		/* Duplicate str w/malloc */

extern struct directory *rt_dir_lookup();/* Look up name in toc */
extern struct directory *rt_dir_add();	/* Add name to toc */
extern void rt_boolweave();		/* Weave segs into partitions */
extern void rt_boolfinal();		/* Eval booleans over partitions */
extern int rt_booleval();		/* Eval bool tree node */
extern int rt_fdiff();			/* Approx Floating compare */
extern double rt_reldiff();		/* Relative Difference */
extern void rt_pr_region();		/* Print a region */
extern void rt_pr_tree();		/* Print an expr tree */
extern void rt_pr_pt();			/* Print a partition */
extern void rt_pr_bitv();		/* Print a bit vector */
extern void rt_pr_hit();		/* Print a hit point */
extern void rt_fastf_float();		/* convert dbfloat->fastf_t */
extern void rt_mat_dbmat();		/* convert dbfloat mat->fastf_t */
extern void rt_get_seg();		/* storage obtainer */
extern void rt_get_pt();
extern void rt_get_bitv();
extern int rt_byte_roundup();		/* malloc rounder */
extern void rt_bitv_or();		/* logical OR on bit vectors */
extern void rt_cut_it();		/* space partitioning */
extern void rt_pr_cut();		/* print cut node */
extern void rt_draw_box();		/* unix-plot an RPP */
extern void rt_region_color_map();	/* regionid-driven color override */
extern void rt_color_addrec();		/* process ID_MATERIAL record */
extern void rt_cut_extend();		/* extend a cut box */
extern int rt_rpp_region();		/* find RPP of one region */
#endif

/* CxDiv, CxSqrt */
extern void rt_pr_roots();		/* print complex roots */

/*
 *  Constants provided and used by the RT library.
 */
extern double	rt_invpi, rt_inv2pi;
extern double	rt_inv255;

/*
 *  System library routines used by the RT library.
 */
extern char	*malloc();
/**extern void	free(); **/

#endif RAYTRACE_H
