#include <stdio.h>
#include <math.h>

#include "brlcad_config.h"
#include "machine.h"		/* machine specific definitions */
#include "vmath.h"		/* vector math macros */
#include "raytrace.h"		/* librt interface definitions */
#include "rtgeom.h"

#include "tie.h"
#include "struct.h"

#define DEBUG 1
#define dprintf if (DEBUG) fprintf


#define ACQUIRE_SEMAPHORE_TREE(_hash)	switch((_hash)&03)  { \
	case 0: \
		bu_semaphore_acquire( RT_SEM_TREE0 ); \
		break; \
	case 1: \
		bu_semaphore_acquire( RT_SEM_TREE1 ); \
		break; \
	case 2: \
		bu_semaphore_acquire( RT_SEM_TREE2 ); \
		break; \
	default: \
		bu_semaphore_acquire( RT_SEM_TREE3 ); \
		break; \
	}

#define RELEASE_SEMAPHORE_TREE(_hash)	switch((_hash)&03)  { \
	case 0: \
		bu_semaphore_release( RT_SEM_TREE0 ); \
		break; \
	case 1: \
		bu_semaphore_release( RT_SEM_TREE1 ); \
		break; \
	case 2: \
		bu_semaphore_release( RT_SEM_TREE2 ); \
		break; \
	default: \
		bu_semaphore_release( RT_SEM_TREE3 ); \
		break; \
	}


static int adrt_tri_prep(struct soltab *stp,
		  struct rt_db_internal *ip,
		  struct rt_i *rtip)
{return 0;}

static int adrt_tri_shot(struct soltab *stp,
		  struct xray *rp,
		  struct application *ap,
		  struct seg *seghead)
{return 0;}

static int adrt_tri_piece_shot(struct rt_piecestate *psp,
			struct rt_piecelist *plp,
			double dist_corr,
			struct xray *rp,
			struct application *ap,
			struct seg *seghead)
{return 0;}
static void adrt_tri_piece_hitsegs(struct rt_piecestate *psp,
			    struct seg *seghead,
			    struct application *ap)
{return;}
static void adrt_tri_print(const struct soltab *stp)
{return;}
static void adrt_tri_norm(struct hit *hitp,
		   struct soltab *stp,
		   struct xray *rp)
{return;}
static void adrt_tri_uv(struct application *ap,
		 struct soltab *stp,
		 struct hit *hitp,
		 struct uvcoord *uvp)
{return;}
static void adrt_tri_curve(struct curvature *cvp,
		    struct hit *hitp,
		    struct soltab *stp)
{return;}
static int adrt_tri_class() {return 0;}
static void adrt_tri_free(struct soltab *stp)
{return;}
static int adrt_tri_plot(struct bu_list *vhead,
		  struct rt_db_internal *ip,
		  const struct rt_tess_tol *ttol,
		  const struct bn_tol *tol) {return -1;}
static void adrt_tri_vshot(struct soltab *stp[],
		    struct xray *rp[],
		    struct seg segp[], int n,
		    struct application *ap) {return;}
static int adrt_tri_tess(struct nmgregion **r,
		  struct model *m,
		  struct rt_db_internal *ip,
		  const struct rt_tess_tol *ttol,
		  const struct bn_tol *tol) {return -1;}
static int adrt_tri_tnurb(struct nmgregion **r,
		   struct model *m,
		   struct rt_db_internal *ip,
		   const struct bn_tol *tol)  {return -1;}
static int adrt_tri_import5(struct rt_db_internal *ip,
		     const struct bu_external *ep,
		     const mat_t mat, const struct db_i *dbip,
		     struct resource *resp,
		     const int minot_type)  {return -1;}
static int adrt_tri_export5(struct bu_external *ep,
		     const struct rt_db_internal *ip,
		     double local2mm, const struct db_i *dbip,
		     struct resource *resp,
		     const int minor_type )  {return -1;}
static int adrt_tri_import(struct rt_db_internal *ip,
		    const struct bu_external *ep,
		    const mat_t mat, const struct db_i *dbip,
		    struct resource *resp )  {return -1;}
static int adrt_tri_export(struct bu_external *ep,
		    const struct rt_db_internal *ip,
		    double local2mm, const struct db_i *dbip,
		    struct resource *resp )  {return -1;}
static void adrt_tri_ifree(struct rt_db_internal *ip, struct resource *resp)  {return;}
static int adrt_tri_describe(struct bu_vls *str,
		      const struct rt_db_internal *ip,
		      int verbose, double mm2local, struct resource *resp,
		      struct db_i *db_i)  {return -1;}
static int adrt_tri_xform(struct rt_db_internal *op,
		   const mat_t mat, struct rt_db_internal *ip,
		   int free, struct db_i *dbip, struct resource *resp)  {return -1;}

extern int rt_nul_tclget(Tcl_Interp *interp, const struct rt_db_internal *intern, const char *attr);
extern int rt_nul_tcladjust(Tcl_Interp *interp, struct rt_db_internal *intern, int argc, char **argv, struct resource *resp);
extern int rt_nul_tclform(const struct rt_functab *ftp, Tcl_Interp *interp);
extern void rt_nul_make(const struct rt_functab *ftp, struct rt_db_internal *intern, double diameter);
extern const struct bu_structparse rt_nul_parse[];



static struct rt_functab adrt_functab = 
    {
	RT_FUNCTAB_MAGIC, "ID_TRI", "TRI", 0,
	adrt_tri_prep, 
	adrt_tri_shot, 
	adrt_tri_print, 
	adrt_tri_norm, 
	adrt_tri_piece_shot, 
	adrt_tri_piece_hitsegs, 
	adrt_tri_uv, 
	adrt_tri_curve, 
	adrt_tri_class, 
	adrt_tri_free, 
	adrt_tri_plot, 
	adrt_tri_vshot, 
	adrt_tri_tess, 
	adrt_tri_tnurb, 
	adrt_tri_import5, 
	adrt_tri_export5, 
	adrt_tri_import, 
	adrt_tri_export, 
	adrt_tri_ifree, 
	adrt_tri_describe, 
	adrt_tri_xform,
	rt_nul_parse,
	0, 0,
	rt_nul_tclget,
	rt_nul_tcladjust,
	rt_nul_tclform,
	rt_nul_make
    };


struct triwrap {
    long magic;
    struct directory *dp;
    int tri_num;
    TIE_ID id;
};
#define TRIWRAP_MAGIC 1234321
#define CK_TRIWRAP(twp) { \
	if (!twp) { \
		fprintf(stderr, "%s:%d null triwrap ptr", __FILE__, __LINE__);\
		abort();\
	} else if (twp->magic != TRIWRAP_MAGIC) { \
		fprintf(stderr, "bad triwrap_magic: 0x%08x s/b 0x%08x\n", \
		twp->magic, TRIWRAP_MAGIC); \
		abort();\
	} \
}


static int reg_start_func (struct db_tree_state * tsp,
		    struct db_full_path * pathp,
		    const struct rt_comb_internal * combp,
		    genptr_t client_data )
{
    fprintf(stderr, "reg_start\n");
    return 1; /* don't skip */
}

static union tree * reg_end_func (
			   struct db_tree_state * tsp,
			   struct db_full_path * pathp,
			   union tree * curtree,
			   genptr_t client_data )
{
    fprintf(stderr, "reg_end\n");
    return curtree;
}

static union tree * leaf_func (
			struct db_tree_state * tsp,
			struct db_full_path * pathp,
			struct rt_db_internal * ip,
			genptr_t client_data )
{
    register struct soltab	*stp;
    union tree			*curtree;
    struct directory		*dp;
    register matp_t		mat;
    int				face;
    struct rt_i			*rtip;
    struct rt_bot_internal	*bot;
    TIE_3			v0, v1, v2;
    struct triwrap 		*tw;
    int 			hash;
    vectp_t			vp;

    fprintf(stderr, "leaf_func\n");

    RT_CK_DBTS(tsp);
    RT_CK_DBI(tsp->ts_dbip);
    RT_CK_FULL_PATH(pathp);
    RT_CK_DB_INTERNAL(ip);
    rtip = tsp->ts_rtip;
    RT_CK_RTI(rtip);
    RT_CK_RESOURCE(tsp->ts_resp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);


    if (ip->idb_minor_type != ID_BOT) {
	fprintf(stderr, "Minor type = %d (Non-BOT) regions must contain a single bot\n", ip->idb_minor_type);
	return( TREE_NULL );		/* BAD */
    }
    bot = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot);
    rtip->nsolids++;


    tw = bu_malloc(sizeof(struct triwrap)*bot->num_faces, "triwrap");
    fprintf(stderr, "%d faces\n", bot->num_faces);


    for (vp = &bot->vertices[bot->num_vertices-1] ; vp >= bot->vertices ; vp -= 3)
	VMINMAX(rtip->mdl_min, rtip->mdl_max, vp);



    for (face = 0 ; face < bot->num_faces ; face++) {
	int vert_idx;

	vert_idx = bot->faces[face*3];
	VMOVE(v0.v, &bot->vertices[vert_idx*3]);
	vert_idx = bot->faces[face*3+1];
	VMOVE(v1.v, &bot->vertices[vert_idx*3]);
	vert_idx = bot->faces[face*3+2];
	VMOVE(v2.v, &bot->vertices[vert_idx*3]);

	tw[face].magic = TRIWRAP_MAGIC;
	tw[face].dp = dp;
	tw[face].tri_num = face;

	TIE_AddTriangle(v0, v1, v2, (void *)&tw[face]/* user data payload */);
    }


    hash = db_dirhash( dp->d_namep );
    RT_GET_TREE( curtree, tsp->ts_resp );
    curtree->magic = RT_TREE_MAGIC;
    curtree->tr_op = OP_SOLID;
    /* regionp will be filled in later by rt_tree_region_assign() */
    curtree->tr_a.tu_regionp = (struct region *)0;

    BU_GETSTRUCT( stp, soltab);
    stp->l.magic = RT_SOLTAB_MAGIC;
    stp->l2.magic = RT_SOLTAB2_MAGIC;
    stp->st_rtip = rtip;
    stp->st_dp = dp;
    dp->d_uses++;	/* XXX gratuitous junk */
    stp->st_uses = 1;
    stp->st_matp = (matp_t)0;
    bu_ptbl_init( &stp->st_regions, 0, "regions using solid" ); /* XXX gratuitous junk */

    /* fake out, */
    stp->st_meth = &adrt_functab;
    curtree->tr_a.tu_stp = stp;

    ACQUIRE_SEMAPHORE_TREE(hash);
    BU_LIST_INSERT( &(rtip->rti_solidheads[hash]), &(stp->l) );
    BU_LIST_INSERT( &dp->d_use_hd, &(stp->l2) );
    RELEASE_SEMAPHORE_TREE(hash);

    bu_log("tree:0x%08x  stp:0x%08x\n", curtree, stp);


    return( curtree );
}


int ADrt_gettree(struct rt_i *rtip, const char *name)
{
    struct db_tree_state ts;
    char *argv[2];
    const char **av;

    argv[0] = (char *)name;
    argv[1] = (char *)NULL;
    av = (const char **)argv;

    ts = rt_initial_tree_state;
    ts.ts_rtip = rtip;

    fprintf(stderr, "Hello world\n");
    db_walk_tree(rtip->rti_dbip, 1, av, 1 /* ncpu */, &ts,
		 &reg_start_func, &reg_end_func, &leaf_func, (void *)0);
}

void ADrt_prep_parallel(struct rt_i *rtip, const int ncpu)
{
    TIE_Prep();
}

void *user_hit(TIE_Ray *ray,
	       TIE_ID *id,
	       TIE_Triangle *tri,
	       void *ptr)
{
    struct triwrap *tw = ((struct triwrap *)(tri->Ptr));

    if (!tw) { fprintf(stderr, "%s:%d Null triwrap ptr\n", __FILE__, __LINE__); abort();}
    if (tw->magic != TRIWRAP_MAGIC) { fprintf(stderr, "bad triwrap magic %d", tw->magic); }

    tw->id = *id; /* struct copy */
    
    printf("  TRIANGLE HIT!: [%.3f, %.3f, %.3f]\n", id -> Pos.v[0], id -> Pos.v[1], id -> Pos.v[2]);
    fprintf(stderr, "User_hit adding tri ptr 0x%08lx 0x%08lx\n", tri, tri->Ptr);

    (void)bu_ptbl_ins((struct bu_ptbl *)ptr, (long *)tri);

    return (void *)0; /* always continue shooting the ray */
}


int ADrt_shootray( struct application *ap )
{
    TIE_Ray		ray;
    TIE_ID		id;
    TIE_Triangle	*tri, **tp, **tp_out;
    void		*ptr;
    struct seg		*seg_p = (struct seg *)NULL;
    struct bu_ptbl	segs, triHits;
    struct partition	*pp;
    struct partition	*PartHdp;
    struct resource	*res = ap -> a_resource;
    struct soltab *stp;
    struct triwrap *tw;
    struct triwrap *tw_out;

    GET_PT_INIT(rtip, PartHdp, res);
    PartHdp->pt_forw = PartHdp->pt_back = PartHdp;
    PartHdp->pt_magic = PT_HD_MAGIC;

    VMOVE(ray.Pos.v, ap->a_ray.r_pt);
    VMOVE(ray.Dir.v, ap->a_ray.r_dir);

    bu_ptbl_init(&triHits, 16, "triangle Hits");

    TIE_ShootRay(&ray, &id, &user_hit, &triHits); /* xxx 3? */

    BU_CK_PTBL(&triHits);
    bu_ptbl_init(&segs, 8, "triangle Hits");
    /* walk the list of triangles we got and build the segments */

    BU_GETSTRUCT(stp, soltab);
    BU_LIST_INIT(&stp->l);
    BU_LIST_MAGIC_SET(&stp->l, RT_SOLTAB_MAGIC);
    BU_LIST_INIT(&stp->l2);
    BU_LIST_MAGIC_SET(&stp->l2, RT_SOLTAB2_MAGIC);
    stp->st_meth = &adrt_functab;
    stp->st_rtip = ap->a_rt_i;
    stp->st_id = ID_NULL;



    for (tp  = (TIE_Triangle **)BU_PTBL_BASEADDR(&triHits) ;
	 tp <= (TIE_Triangle **)BU_PTBL_LASTADDR(&triHits) ;
	 tp++) {

	/* if this one has already been harvested for something else, move on. */
	if (! *tp) continue;

	dprintf(stderr, "triangle: 0x%08x\n", *tp);

	/* get the triwrap data for this hit */
	tw = (struct triwrap *)(*tp)->Ptr;
	CK_TRIWRAP(tw);

	/* start a seg/partition for this hit */


	/* go looking for an out-hit triangle */
	tp_out = tp;
	
	for (tp_out++;
	     tp_out <= (TIE_Triangle **)BU_PTBL_LASTADDR(&triHits);
	     tp_out++) {

	    if (! *tp_out) continue;

	    dprintf(stderr, "examine triangle: 0x%08x\n", *tp_out);

	    tw_out = (struct triwrap *)(*tp_out)->Ptr;
	    CK_TRIWRAP(tw_out);

	    if (tw_out->dp == tw->dp) {
		register int delta = tw_out->id.Dist - tw->id.Dist;
		if (delta > ap->a_rt_i->rti_tol.dist) {
		    /* found a hit on the same object */
		    goto found;
		} else {
		    double dot_in, dot_out;
		    fprintf(stderr, "next hit within tolerance a:%g b:%g tol:%g\n",
			    tw->id.Dist, tw_out->id.Dist, ap->a_rt_i->rti_tol.dist);

		    /* is this: an in/out across an edge?
		     * 		a penetration at an edge?
		     *		just plain bogus?
		     */
		    dot_in = VDOT(ap->a_ray.r_dir, tw->id.Norm.v);
		    dot_out = VDOT(ap->a_ray.r_dir, tw_out->id.Norm.v);

		    if ( (dot_in > 0 && dot_out < 0) ||
			 (dot_in < 0 && dot_out > 0) ) {
			/* this is an in-and-out at an edge, save or discard both */
			dprintf(stderr, "in & out again at edge\n");
		    } else if ( (dot_in >= 0 && dot_out >= 0) ||
				(dot_in <= 0 && dot_out <= 0) ) {
			/* this is a penetrating hit on an edge, discard one */
			*tp_out = (TIE_Triangle*)NULL;
			dprintf(stderr, "penetrating, discarding one\n");
		    } else {
			/* unknown what is going on here */
			*tp_out = (TIE_Triangle*)NULL;
			dprintf(stderr, "bogus, discarding one\n");
		    }
		}
	    }
	}

	/* couldn't find a matching hit for this triangle */
	fprintf(stderr, "could not find match for triangle hit, skipping\n");
	continue;
    found:
	dprintf(stderr, "found matching in/out hits\n");

	/* XXX look for other hits at the out-hit location & pick the best one */

	/* setup in segment */
	RT_GET_SEG(seg_p, ap->a_resource);
	seg_p->seg_in.hit_dist = tw->id.Dist;
	VMOVE(seg_p->seg_out.hit_normal, tw->id.Norm.v);
	seg_p->seg_in.hit_rayp = &ap->a_ray;
	seg_p->seg_in.hit_surfno = 0;
	seg_p->seg_stp = stp;

	/* setup out segment */
	RT_GET_SEG(seg_p, ap->a_resource);
	seg_p->seg_out.hit_dist = tw_out->id.Dist;
	VMOVE(seg_p->seg_out.hit_normal, tw_out->id.Norm.v);
	seg_p->seg_out.hit_rayp = &ap->a_ray;
	seg_p->seg_out.hit_surfno = 0;
	seg_p->seg_stp = stp;

	/* fill in the soltab for the segment */

	GET_PT_INIT( rtip, pp, res );
	bu_ptbl_ins_unique( &pp->pt_seglist, (long *)seg_p );
	pp->pt_inseg = seg_p;
	pp->pt_inhit = &seg_p->seg_in;
	pp->pt_outseg = seg_p;
	pp->pt_outhit = &seg_p->seg_out;
	pp->pt_regionp = NULL; /* SET IT TO NULL */
	APPEND_PT( pp, PartHdp );
    }

    /* Call the user hit callback routine */
    ap->a_hit(ap, PartHdp, NULL);

    bu_ptbl_reset(&triHits);
}
