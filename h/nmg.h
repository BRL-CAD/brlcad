/*
 *			N M G . H
 *
 *	PRELIMINARY VERSION
 *
 *  Definition of data structures for "Non-Manifold Geometry Modelling."
 *  Developed from "Non-Manifold Geometric Boundary Modeling" by 
 *  Kevin Weiler, 5/7/87 (SIGGraph 1989 Course #20 Notes)
 *
 *  Author -
 *	Lee A. Butler
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

#ifndef FACET_H
#define FACET_H seen

#define OT_UNSPEC   '\0'    /* orientation unspecified */
#define OT_SAME     '\1'    /* orientation same */
#define OT_OPPOSITE '\2'    /* orientation opposite */

/*
 *  ptr types
 */
#define PTR_SHELL   5
#define PTR_FACEUSE 4
#define PTR_LOOPUSE 3
#define PTR_EDGEUSE 2
#define PTR_VERTEXUSE	1

/*
 *  Magic Numbers.
 */
#define NMG_MODEL_MAGIC 	12121212
#define NMG_MODEL_A_MAGIC	0x68652062
#define NMG_REGION_MAGIC	23232323
#define NMG_REGION_A_MAGIC	0x696e6720
#define NMG_SHELL_MAGIC 	34343434
#define NMG_SHELL_A_MAGIC	0x65207761
#define NMG_FACE_MAGIC		45454545
#define NMG_FACE_A_MAGIC	0x726b6e65
#define NMG_FACEUSE_MAGIC	56565656
#define NMG_FACEUSE_A_MAGIC	0x20476f64
#define NMG_LOOP_MAGIC		67676767
#define NMG_LOOP_A_MAGIC	0x6420224c
#define NMG_LOOPUSE_MAGIC	78787878
#define NMG_LOOPUSE_A_MAGIC	0x68657265
#define NMG_EDGE_MAGIC		89898989
#define NMG_EDGE_G_MAGIC	0x6c696768
#define NMG_EDGEUSE_MAGIC	90909090
#define NMG_EDGEUSE_A_MAGIC	0x20416e64
#define NMG_VERTEX_MAGIC	123123
#define NMG_VERTEX_G_MAGIC	727737707
#define NMG_VERTEXUSE_MAGIC	12341234
#define NMG_VERTEXUSE_A_MAGIC	0x69676874

#define NMG_CKMAG(_ptr, _magic, _str)	\
	if( !(_ptr) )  { \
		rt_log("ERROR: NMG null %s ptr, file %s, line %d\n", \
			_str, __FILE__, __LINE__ ); \
		rt_bomb("NULL NMG pointer"); \
	} else if( (_ptr)->magic != (_magic) )  { \
		rt_log("ERROR: NMG bad %s ptr x%x, s/b x%x, was %s(x%x), file %s, line %d\n", \
			_str, _ptr, _magic, \
			nmg_identify_magic( (_ptr)->magic ), (_ptr)->magic, \
			__FILE__, __LINE__ ); \
		rt_bomb("Bad NMG pointer"); \
	}

#define NMG_CK_MODEL(_p)	NMG_CKMAG(_p, NMG_MODEL_MAGIC, "model")
#define NMG_CK_MODEL_A(_p)	NMG_CKMAG(_p, NMG_MODEL_A_MAGIC, "model_a")
#define NMG_CK_REGION(_p)	NMG_CKMAG(_p, NMG_REGION_MAGIC, "region")
#define NMG_CK_REGION_A(_p)	NMG_CKMAG(_p, NMG_REGION_A_MAGIC, "region_a")
#define NMG_CK_SHELL(_p)	NMG_CKMAG(_p, NMG_SHELL_MAGIC, "shell")
#define NMG_CK_SHELL_A(_p)	NMG_CKMAG(_p, NMG_SHELL_A_MAGIC, "shell_a")
#define NMG_CK_FACE(_p)		NMG_CKMAG(_p, NMG_FACE_MAGIC, "face")
#define NMG_CK_FACE_A(_p)	NMG_CKMAG(_p, NMG_FACE_A_MAGIC, "face_a")
#define NMG_CK_FACEUSE(_p)	NMG_CKMAG(_p, NMG_FACEUSE_MAGIC, "faceuse")
#define NMG_CK_FACEUSE_A(_p)	NMG_CKMAG(_p, NMG_FACEUSE_A_MAGIC, "faceuse_a")
#define NMG_CK_LOOP(_p)		NMG_CKMAG(_p, NMG_LOOP_MAGIC, "loop")
#define NMG_CK_LOOP_A(_p)	NMG_CKMAG(_p, NMG_LOOP_A_MAGIC, "loop_a")
#define NMG_CK_LOOPUSE(_p)	NMG_CKMAG(_p, NMG_LOOPUSE_MAGIC, "loopuse")
#define NMG_CK_LOOPUSE_A(_p)	NMG_CKMAG(_p, NMG_LOOPUSE_A_MAGIC, "loopuse_a")
#define NMG_CK_EDGE(_p)		NMG_CKMAG(_p, NMG_EDGE_MAGIC, "edge")
#define NMG_CK_EDGE_G(_p)	NMG_CKMAG(_p, NMG_EDGE_G_MAGIC, "edge_g")
#define NMG_CK_EDGEUSE(_p)	NMG_CKMAG(_p, NMG_EDGEUSE_MAGIC, "edgeuse")
#define NMG_CK_EDGEUSE_A(_p)	NMG_CKMAG(_p, NMG_EDGEUSE_A_MAGIC, "edgeuse_a")
#define NMG_CK_VERTEX(_p)	NMG_CKMAG(_p, NMG_VERTEX_MAGIC, "vertex")
#define NMG_CK_VERTEX_G(_p)	NMG_CKMAG(_p, NMG_VERTEX_G_MAGIC, "vertex_g")
#define NMG_CK_VERTEXUSE(_p)	NMG_CKMAG(_p, NMG_VERTEXUSE_MAGIC, "vertexuse")
#define NMG_CK_VERTEXUSE_A(_p)	NMG_CKMAG(_p, NMG_VERTEXUSE_A_MAGIC, "vertexuse_a")


/*  M O D E L
 */
struct model {
    long	    magic;
    struct nmgregion   *r_p;		/* list of regions in model space */
    struct model_a  *ma_p;
};

struct model_a {
	long magic;
};

/*  R E G I O N
 */
struct nmgregion {
    long	    magic;
    struct model    *m_p;	/* owning model */
    struct nmgregion *next,
		    *last;	/* regions in model list of regions */
    struct shell    *s_p;	/* list of shells in region */
    struct nmgregion_a *ra_p;	/* attributes */
};

struct nmgregion_a {
	long	magic;
};

/*  S H E L L
 */
struct shell {
    long	    magic;
    struct nmgregion   *r_p;	    /* owning region */
    struct shell    *next,
		    *last;	    /* shells in region's list of shells */
    struct shell_a  *sa_p;	    /* attribs */
    char	    downptr_type;
    union {
	struct faceuse	    *fu_p;  /* list of face uses in shell */
	struct vertexuse    *vu_p;  /* shell is single vertex */
    } downptr;
};

struct shell_a {
	long	magic;
};

/*  F A C E
 */
struct face {
    long	    magic;
    struct faceuse  *fu_p;  /* list of uses of this face. use fu mate field */
    struct face_g   *fg_p;  /* geometry */
};

struct face_g {
    long    magic;
    double  N[3];	/* Surface Normal */
};

struct faceuse { /* Note: there will always be exactly two uses of a face */
    long	    magic;
    struct shell    *s_p;	    /* owning shell */
    struct faceuse  *next,
		    *last,	    /* fu's in shell's list of fu's */
		    *fumate_p;	    /* opposite side of face */
    struct loopuse  *lu_p;	    /* list of loops in face-use */
    char	    orientation;    /* compared to face geom definition */
    struct face     *f_p;	    /* face definition and attributes */
    struct faceuse_a *fua_p;	    /* attributess */
};

struct faceuse_a {
    long    magic;
    int     flip;	/* (Flip/don't Flip) face normal */
};

/*  L O O P
 */
struct loop {
    long	    magic;
    struct loopuse  *lu_p;  /* list of uses of this loop. -
					use eu_mate eulu fields */
    struct loop_g   *lg_p;  /* Geoometry */
};

struct loop_g {
    long    magic;
};

struct loopuse {
    long	    magic;
    struct faceuse  *fu_p;	    /* owning face-use */
    struct loopuse  *next,
		    *last,	    /* lu's in fu's list of lu's */
		    *lumate_p;	    /* loopuse on other side of face */
    struct loop     *l_p;	    /* loop definition and attributes */
    struct loopuse_a *lua_p;	    /* attributes */
    struct edgeuse  *eu_p;	    /* list of eu's in lu */
};

struct loopuse_a {
    long    magic;
};

/*  E D G E
 */
struct edge {
    long	    magic;
    struct edgeuse  *eu_p;  /* list of uses of this edge -
				    use eu radial/mate fields */
    struct edge_g   *eg_p;  /* geometry */
};

struct edge_g {
    long    magic;
};

struct edgeuse {
    long		magic;
    struct vertexuse	*vu_p;	    /* starting vu of eu in this orient */
    struct edgeuse	*eumate_p,  /* eu on other face or other end of wire*/
		  	*eu_radial_p,/* eu on radially adjacent fu */
    			*lueu_cw_p, /* clockwise/counter-clockwise..*/
		  	*lueu_ccw_p;/* eu's in lu's ordered eu list */
    struct edge 	*e_p;	    /* edge definition and attributes */
    struct edgeuse_a	*eua_p;	    /* parametric space geom */
    char	  	orientation;/* compared to geom */
    struct loopuse	*lu_p;	    /* owning loop */
};

struct edgeuse_a {
    long    magic;
};

/*  V E R T E X
 */
struct vertex {
    long		magic;
    struct vertexuse	*vu_p;	/* list of uses of this vertex - 
					    use vu_next fields */
    struct vertex_g	*vg_p;	/* geometry */
};

struct vertex_g {
    long    magic;
    double  coord[3];	    /* coordinates of vertex in space */
};

struct vertexuse {
    long		magic;
    struct vertexuse	*next,   /* list of all vu's of vertex */
			*last;
    struct vertex	*v_p;	    /* vertex definition and attributes */
    struct vertexuse_a	*vua_p;     /* Attributes */
    char		upptr_type; /* selects pointer type in union upptr */
    union {
	struct shell	*s_p;	/* no fu's or eu's on shell */
	struct edgeuse	*eu_p;	/* eu causing this vu */
    } upptr;
};

struct vertexuse_a {
    long	    magic;
};

/*
 *  Support Function Return codes
 *
 */
#define SUCCESS		0
#define ALLOC_FAIL	1	/* memory allocation failed */
#define PARAM_ERROR	2	/* function parameter error */
#define NOT_EMPTY	3	/* structure free might cause memory leak */

extern char *malloc();
#ifdef DEBUG
#define NMG_GETSTRUCT(ptr, str) \
 ( ((ptr)=(struct str *)nmg_malloc(sizeof(struct str))) == (struct str *)NULL )
#else
#define NMG_GETSTRUCT(ptr, str) \
 ( ((ptr)=(struct str *)malloc(sizeof(struct str))) == (struct str *)NULL )
#endif

#define GET_MODEL(p)	    NMG_GETSTRUCT(p, model)
#define GET_MODEL_A(p)	    NMG_GETSTRUCT(p, model_a)
#define GET_REGION(p)	    NMG_GETSTRUCT(p, nmgregion)
#define GET_REGION_A(p)     NMG_GETSTRUCT(p, nmgregion_a)
#define GET_SHELL(p)	    NMG_GETSTRUCT(p, shell)
#define GET_SHELL_A(p)	    NMG_GETSTRUCT(p, shell_a)
#define GET_FACE(p)	    NMG_GETSTRUCT(p, face)
#define GET_FACE_G(p)	    NMG_GETSTRUCT(p, face_g)
#define GET_FACEUSE(p)	    NMG_GETSTRUCT(p, faceuse)
#define GET_FACEUSE_A(p)    NMG_GETSTRUCT(p, faceuse_a)
#define GET_LOOP(p)	    NMG_GETSTRUCT(p, loop)
#define GET_LOOP_G(p)	    NMG_GETSTRUCT(p, loop_g)
#define GET_LOOPUSE(p)	    NMG_GETSTRUCT(p, loopuse)
#define GET_LOOPUSE_A(p)    NMG_GETSTRUCT(p, loopuse_a)
#define GET_EDGE(p)	    NMG_GETSTRUCT(p, edge)
#define GET_EDGE_G(p)	    NMG_GETSTRUCT(p, edge_g)
#define GET_EDGEUSE(p)	    NMG_GETSTRUCT(p, edgeuse)
#define GET_EDGEUSE_A(p)    NMG_GETSTRUCT(p, edgeuse_a)
#define GET_VERTEX(p)	    NMG_GETSTRUCT(p, vertex)
#define GET_VERTEX_G(p)     NMG_GETSTRUCT(p, vertex_g)
#define GET_VERTEXUSE(p)    NMG_GETSTRUCT(p, vertexuse)
#define GET_VERTEXUSE_A(p)  NMG_GETSTRUCT(p, vertexuse_a)

#ifdef DEBUG
#define FREESTRUCT(ptr, str) \
	{fprintf(stderr, "free(%8x) (%8x)->magic %d\n", ptr, ptr, ptr->magic); \
	bzero((char *)(ptr), sizeof(struct str)); free((char *)(ptr)); }
#else
#define FREESTRUCT(ptr, str) \
	{ bzero((char *)(ptr), sizeof(struct str)); free((char *)(ptr)); }
#endif

#define FREE_MODEL(p)	    FREESTRUCT(p, model)
#define FREE_MODEL_A(p)	    FREESTRUCT(p, model_a)
#define FREE_REGION(p)	    FREESTRUCT(p, nmgregion)
#define FREE_REGION_A(p)    FREESTRUCT(p, nmgregion_a)
#define FREE_SHELL(p)	    FREESTRUCT(p, shell)
#define FREE_SHELL_A(p)	    FREESTRUCT(p, shell_a)
#define FREE_FACE(p)	    FREESTRUCT(p, face)
#define FREE_FACE_G(p)	    FREESTRUCT(p, face_g)
#define FREE_FACEUSE(p)	    FREESTRUCT(p, faceuse)
#define FREE_FACEUSE_A(p)   FREESTRUCT(p, faceuse_a)
#define FREE_LOOP(p)	    FREESTRUCT(p, loop)
#define FREE_LOOP_G(p)	    FREESTRUCT(p, loop_g)
#define FREE_LOOPUSE(p)	    FREESTRUCT(p, loopuse)
#define FREE_LOOPUSE_A(p)   FREESTRUCT(p, loopuse_a)
#define FREE_EDGE(p)	    FREESTRUCT(p, edge)
#define FREE_EDGE_G(p)	    FREESTRUCT(p, edge_g)
#define FREE_EDGEUSE(p)	    FREESTRUCT(p, edgeuse)
#define FREE_EDGEUSE_A(p)   FREESTRUCT(p, edgeuse_a)
#define FREE_VERTEX(p)	    FREESTRUCT(p, vertex)
#define FREE_VERTEX_G(p)    FREESTRUCT(p, vertex_g)
#define FREE_VERTEXUSE(p)   FREESTRUCT(p, vertexuse)
#define FREE_VERTEXUSE_A(p) FREESTRUCT(p, vertexuse_a)

struct walker_tbl {
	int     pre_post;       /* pre-process or post process nodes */
#define PREPROCESS -1
#define POSTPROCESS 1
	int     (*fmodel)();
	int     (*fregion)();
	int     (*fshell)();
        int     (*ffaceuse)();
        int     (*floopuse)();
        int     (*fedgeuse)();
        int     (*fvertexuse)();
};

/*
 *  Support Function Declarations
 *
 */
extern struct model * nmg_mkmodel();	/* Make Model & initial region */
extern int nmg_mkregion();	/* make new region, shell, vertex in model */
extern int nmg_mkshell();	/* Make Shell, Vertex */
extern int nmg_mkface1();	/* Promote shell vertex to face */
extern int nmg_mkfaceN();	/* Make face from/on existing vertex */
extern int nmg_newfacev();	/* Make new vertex on edge */
extern int nmg_insfacev();	/* insert existing vertex on edge */
extern int nmg_wmodel();	/* walk model tree */
extern int nmg_wregion();	/* walk region tree */
extern int nmg_wshell();	/* walk shell tree */
extern int nmg_wfaceuse();	/* walk faceuse tree */
extern int nmg_wloopuse();	/* walk loopuse tree */
extern int nmg_wedgeuse();	/* walk edgeuse tree */
extern int nmg_wvertexuse();	/* walk vertexuse */
extern int nmg_vertex_g();	/* associate geometry with vertex */
extern void nmg_movevu();	/* move vertexuse to new vertex */
extern void nmg_kvu();
extern void nmg_keu();
extern void nmg_klu();
extern void nmg_kfu();
extern void nmg_kshell();
extern void nmg_kregion();
extern void nmg_kmodel();
extern void nmg_jv();		/* join vertexes */
extern int nmg_dupface();
extern int nmg_splitface();
extern char	*nmg_identify_magic();	/* describe kind of magic */

#if defined(SYSV) && !defined(bzero)
#	define bzero(str,n)		memset( str, '\0', n )
#	define bcopy(from,to,count)	memcpy( to, from, count )
#endif

/* insert a node into a doubly linked list */
#define DLLINS(listp, nodep) {\
	if (listp) { /* link node into existing list */ \
		nodep->next = listp; nodep->last = listp->last; \
		listp->last->next = nodep; listp->last = nodep; \
	} else { /* make node the entire list */\
		nodep->next = nodep->last = nodep; \
	} \
	listp = nodep; }

#endif
