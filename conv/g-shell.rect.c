/*		G - S H E L L . R E C T
 *
 *	This routine creates an single NMG shell from an object by
 *	raytracing and using the hit points as vertices in the shell.
 *	Raytracing is doe in the Y-direction primarily. The -r option
 *	requests raytracing in the X and Z directions to refine the
 *	shape of the shell.
 */

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "../librt/debug.h"

struct nmg_shot_data
{
	struct shell *s;
	struct hitmiss **hitmiss;
	char *manifolds;
};

struct end_pt
{
	struct vertex *v;
	point_t pt;
};

struct local_part
{
	struct bu_list l;
	char is_void;
	fastf_t in_coord, out_coord;
	struct end_pt *in, *out;
};

static struct cube_str
{
	point_t start_pt;
	struct end_pt *corner[8];
} cube;

static struct bu_ptbl verts;
static struct xray *xy_rays;
static struct xray *xz_rays;
static struct xray *yz_rays;
static struct rt_i *rtip;
static struct bn_tol tol;
static char *usage="Usage:\n%s [-r] [-c] [-v] [-g cell_size] [-o brlcad_output_file] [-d debug_level] database.g object1 object2...\n";
static char *_inside="inside";
static char *_outside="outside";

static struct local_part *xy_parts=(struct local_part *)NULL;
static struct local_part *xz_parts=(struct local_part *)NULL;
static struct local_part *yz_parts=(struct local_part *)NULL;

static int	verbose=0;
static int	do_extra_rays=0;
static fastf_t	cell_size=50.0;
static int	x_cells, y_cells, z_cells;
static int	ray_dir;
static FILE	*fd_sgp=NULL, *fd_out=NULL, *fd_plot=NULL;
static char	*output_file=(char *)NULL;
static char	*sgp_file=(char *)NULL;
static char	*plotfile;
static long	rpp_count=0;
static short	vert_ids[8]={1, 2, 4, 8, 16, 32, 64, 128};
static int	x_del[4]={0, 1, 1, 0};
static int	z_del[4]={0, 0, 1, 1};
static int	debug=0;
static int	close_shell=1;

#define	XY_CELL( _i, _j )	((_i)*y_cells + (_j))
#define	XZ_CELL( _i, _j )	((_i)*z_cells + (_j))
#define	YZ_CELL( _i, _j )	((_i)*z_cells + (_j))
#define ADJ1(_i)		(_i+1>3?0:_i+1)
#define ADJ2(_i)		(_i-1<0?3:_i-1)

#define	GET_Y_INDEX( _y )	((int)(((_y) - rtip->mdl_min[Y])/cell_size))
#define GET_Y_VALUE( _index )	(rtip->mdl_min[Y] + (fastf_t)(_index)*cell_size)

#define NO		'n'
#define	YES		'y'
#define UNKNOWN		'\0'

#define GET_PART	(struct local_part *)bu_calloc( 1, sizeof( struct local_part ), "get_part" )
#define	GET_END		(struct end_pt *)bu_calloc( 1, sizeof( struct end_pt ), "get_end" )

#define	MAKE_FACE( _ep0, _ep1, _ep2, _s )	{\
	struct faceuse *_fu; \
	struct vertex **_v[3]; \
	if( debug > 3 ) \
	{ \
		bu_log( "\t\tMaking face:\n" ); \
		bu_log( "\t\t\tx%x (%g %g %g)\n", _ep0, V3ARGS( _ep0->pt ) ); \
		bu_log( "\t\t\tx%x (%g %g %g)\n", _ep1, V3ARGS( _ep1->pt ) ); \
		bu_log( "\t\t\tx%x (%g %g %g)\n", _ep2, V3ARGS( _ep2->pt ) ); \
	} \
	_v[0] = &_ep0->v; \
	_v[1] = &_ep1->v; \
	_v[2] = &_ep2->v; \
	_fu = nmg_cmface( _s, _v, 3 ); \
	if( !(*_v[0])->vg_p ) \
		nmg_vertex_gv( *_v[0], _ep0->pt ); \
	if( !(*_v[1])->vg_p ) \
		nmg_vertex_gv( *_v[1], _ep1->pt ); \
	if( !(*_v[2])->vg_p ) \
		nmg_vertex_gv( *_v[2], _ep2->pt ); \
	if( nmg_calc_face_g( _fu, &tol ) ) \
	{ \
		if( debug > 3 ) \
			bu_log( "Killing degenerate face\n" ); \
		(void)nmg_kfu( _fu ); \
		if( (*_v[0])->vg_p->magic != NMG_VERTEX_G_MAGIC ) \
			(*_v[0])->vg_p = (struct vertex_g *)NULL; \
		if( (*_v[1])->vg_p->magic != NMG_VERTEX_G_MAGIC ) \
			(*_v[1])->vg_p = (struct vertex_g *)NULL; \
		if( (*_v[2])->vg_p->magic != NMG_VERTEX_G_MAGIC ) \
			(*_v[2])->vg_p = (struct vertex_g *)NULL; \
	} \
}
#define	MAKE_FACE_R( _ep0, _ep1, _ep2, _ep3, _s )	{\
	struct faceuse *_fu; \
	struct vertex **_v[4]; \
	if( debug > 3 ) \
	{ \
		bu_log( "\t\tMaking face:\n" ); \
		bu_log( "\t\t\tx%x (%g %g %g)\n", _ep0, V3ARGS( _ep0->pt ) ); \
		bu_log( "\t\t\tx%x (%g %g %g)\n", _ep1, V3ARGS( _ep1->pt ) ); \
		bu_log( "\t\t\tx%x (%g %g %g)\n", _ep2, V3ARGS( _ep2->pt ) ); \
		bu_log( "\t\t\tx%x (%g %g %g)\n", _ep3, V3ARGS( _ep3->pt ) ); \
	} \
	_v[0] = &_ep0->v; \
	_v[1] = &_ep1->v; \
	_v[2] = &_ep2->v; \
	_v[3] = &_ep3->v; \
	_fu = nmg_cmface( _s, _v, 4 ); \
	if( !(*_v[0])->vg_p ) \
		nmg_vertex_gv( *_v[0], _ep0->pt ); \
	if( !(*_v[1])->vg_p ) \
		nmg_vertex_gv( *_v[1], _ep1->pt ); \
	if( !(*_v[2])->vg_p ) \
		nmg_vertex_gv( *_v[2], _ep2->pt ); \
	if( !(*_v[3])->vg_p ) \
		nmg_vertex_gv( *_v[3], _ep3->pt ); \
	if( nmg_calc_face_g( _fu, &tol ) ) \
	{ \
		if( debug > 3 ) \
			bu_log( "Killing degenerate face\n" ); \
		(void)nmg_kfu( _fu ); \
		if( (*_v[0])->vg_p->magic != NMG_VERTEX_G_MAGIC ) \
			(*_v[0])->vg_p = (struct vertex_g *)NULL; \
		if( (*_v[1])->vg_p->magic != NMG_VERTEX_G_MAGIC ) \
			(*_v[1])->vg_p = (struct vertex_g *)NULL; \
		if( (*_v[2])->vg_p->magic != NMG_VERTEX_G_MAGIC ) \
			(*_v[2])->vg_p = (struct vertex_g *)NULL; \
		if( (*_v[3])->vg_p->magic != NMG_VERTEX_G_MAGIC ) \
			(*_v[3])->vg_p = (struct vertex_g *)NULL; \
	} \
}

/* flags for vertex status */
#define	ON_SURFACE	1
#define	OUTSIDE		2
#define	INSIDE		3

/* routine to replace default overlap handler.
 * overlaps are irrelevant to this application
 */
static int
a_overlap( ap, pp, reg1, reg2, pheadp )
register struct application     *ap;
register struct partition       *pp;
struct region                   *reg1;
struct region                   *reg2;
struct partition                *pheadp;
{
	return( 1 );
}


static int
miss( ap )
register struct application *ap;
{
	return(0);
}


static void
pr_part( ptr )
struct local_part *ptr;
{
	bu_log( "local_part: x%x\n", ptr );
	if( !ptr )
		return;
	if( ptr->is_void == YES )
		bu_log( "\tVOID: in_coord=%g, out_coord=%g\n", ptr->in_coord, ptr->out_coord );
	else if( ptr->is_void == NO )
		bu_log( "\tSOLID: in_coord=%g, out_coord=%g\n", ptr->in_coord, ptr->out_coord );
	else if( ptr->is_void == UNKNOWN )
		bu_log( "\tUNKNOWN: in_coord=%g, out_coord=%g\n", ptr->in_coord, ptr->out_coord );
	else
		bu_log( "\tERROR: in_coord=%g, out_coord=%g\n", ptr->in_coord, ptr->out_coord );
	if( ptr->in )
		bu_log( "\tin = x%x (%g %g %g), v=x%x\n", ptr->in, V3ARGS( ptr->in->pt ), ptr->in->v );
	else
		bu_log( "\tin = NULL\n" );
	if( ptr->out )
		bu_log( "\tout = x%x (%g %g %g), v=x%x\n", ptr->out, V3ARGS( ptr->out->pt ), ptr->out->v );
	else
		bu_log( "\tout = NULL\n" );
}

static void
Make_simple_faces( s, status, lpart )
struct shell *s;
int status;
struct local_part *lpart[4];
{
	struct vertex **verts[3];
	struct faceuse *fu;
	int i;

	switch( status )
	{
		case 3:		/* bottom faces */
				MAKE_FACE_R( lpart[1]->in, lpart[1]->out, lpart[0]->out, lpart[0]->in, s )
			break;
		case 6:		/* right side faces */
				MAKE_FACE_R( lpart[2]->in, lpart[2]->out, lpart[1]->out, lpart[1]->in, s )
			break;
		case 7:		/* partial front and back faces and a diagonal face */
				MAKE_FACE( lpart[0]->in, lpart[1]->in, lpart[2]->in, s )
				MAKE_FACE( lpart[2]->out, lpart[1]->out, lpart[0]->out, s )
				MAKE_FACE_R( lpart[2]->in, lpart[2]->out, lpart[0]->out, lpart[0]->in, s )
			break;
		case 9:		/* left side faces */
				MAKE_FACE_R( lpart[0]->in, lpart[0]->out, lpart[3]->out, lpart[3]->in, s )
			break;
		case 11:	/* partial front and back faces and a diagonal face */
				MAKE_FACE( lpart[0]->in, lpart[1]->in, lpart[3]->in, s )
				MAKE_FACE( lpart[3]->out, lpart[1]->out, lpart[0]->out, s )
				MAKE_FACE_R( lpart[1]->in, lpart[1]->out, lpart[3]->out, lpart[3]->in, s )
			break;
		case 12:	/* top faces */
				MAKE_FACE_R( lpart[3]->in, lpart[3]->out, lpart[2]->out, lpart[2]->in, s )
			break;
		case 13:	/* partial front and back faces and a diagonal face */
				MAKE_FACE( lpart[0]->in, lpart[2]->in, lpart[3]->in, s )
				MAKE_FACE( lpart[3]->out, lpart[0]->out, lpart[2]->out, s )
				MAKE_FACE_R( lpart[0]->in, lpart[0]->out, lpart[2]->out, lpart[2]->in, s )
			break;
		case 14:	/* partial front and back faces and a diagonal face */
				MAKE_FACE( lpart[1]->in, lpart[2]->in, lpart[3]->in, s )
				MAKE_FACE( lpart[3]->out, lpart[2]->out, lpart[1]->out, s )
				MAKE_FACE_R( lpart[3]->in, lpart[3]->out, lpart[1]->out, lpart[1]->in, s )
			break;
		case 15:	/* front and back faces */
				MAKE_FACE( lpart[0]->in, lpart[1]->in, lpart[2]->in, s )
				MAKE_FACE( lpart[0]->in, lpart[2]->in, lpart[3]->in, s )
				MAKE_FACE( lpart[2]->out, lpart[1]->out, lpart[0]->out, s )
				MAKE_FACE( lpart[2]->out, lpart[0]->out, lpart[3]->out, s )
			break;
	}
}

static int
Get_extremes( s, ap, hitmiss, manifolds, hit1, hit2 )
struct application *ap;
struct shell *s;
struct hitmiss **hitmiss;
char *manifolds;
point_t hit1, hit2;
{
	struct model *m;
	struct ray_data rd;
	struct seg seghead;
	struct xray *rp;
	int ret;

	NMG_CK_SHELL( s );
	m = nmg_find_model( &s->l.magic );

	bzero( &rd, sizeof( struct ray_data ) );

	rp = &ap->a_ray;
	rd.tol = &tol;
	rd.rd_m = m;
	rd.rp = rp;
	rd.ap = ap;
	rd.manifolds = manifolds;
	rd.hitmiss = hitmiss;
	bzero( hitmiss, m->maxindex*sizeof( struct hitmiss *) );
	BU_LIST_INIT(&rd.rd_hit);
	BU_LIST_INIT(&rd.rd_miss);
	BU_LIST_INIT( &seghead.l );
	rd.seghead = &seghead;

	/* Compute the inverse of the direction cosines */
	if( !NEAR_ZERO( rp->r_dir[X], SQRT_SMALL_FASTF ) )  {
		rd.rd_invdir[X]=1.0/rp->r_dir[X];
	} else {
		rd.rd_invdir[X] = INFINITY;
		rp->r_dir[X] = 0.0;
	}
	if( !NEAR_ZERO( rp->r_dir[Y], SQRT_SMALL_FASTF ) )  {
		rd.rd_invdir[Y]=1.0/rp->r_dir[Y];
	} else {
		rd.rd_invdir[Y] = INFINITY;
		rp->r_dir[Y] = 0.0;
	}
	if( !NEAR_ZERO( rp->r_dir[Z], SQRT_SMALL_FASTF ) )  {
		rd.rd_invdir[Z]=1.0/rp->r_dir[Z];
	} else {
		rd.rd_invdir[Z] = INFINITY;
		rp->r_dir[Z] = 0.0;
	}
	rd.magic = NMG_RAY_DATA_MAGIC;

	nmg_isect_ray_model(&rd);

	if( BU_LIST_IS_EMPTY( &rd.rd_hit ) )
		ret = 0;
	else
	{
		struct hitmiss *a_hit;

		a_hit = BU_LIST_FIRST( hitmiss, &rd.rd_hit );
		VMOVE( hit1, a_hit->hit.hit_point );
		a_hit = BU_LIST_LAST( hitmiss, &rd.rd_hit );
		VMOVE( hit2, a_hit->hit.hit_point );

		NMG_FREE_HITLIST( &rd.rd_hit, ap );

		ret = 1;
	}

	NMG_FREE_HITLIST( &rd.rd_miss, ap );

	return( ret );
}

static int
shrink_hit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	point_t hit1, hit2;
	point_t mhit1, mhit2;
	struct partition *pp;
	struct vertex *hit1_v=(struct vertex *)NULL;
	struct vertex *hit2_v=(struct vertex *)NULL;
	fastf_t extreme_dist1;
	fastf_t extreme_dist2;
	struct application ap2;
	struct shell *s;
	struct shell_a *sa;
	struct nmgregion *r;
	struct nmgregion_a *ra;
	struct model *m;
	struct nmg_shot_data *sd;
	int i;
	int extremes;

	sd = (struct nmg_shot_data *)ap->a_uptr;
	s = sd->s;
	NMG_CK_SHELL( s );
	sa = s->sa_p;
	r = s->r_p;
	ra = r->ra_p;
	m = r->m_p;
	NMG_CK_MODEL( m );
	bzero( &ap2, sizeof( struct application ) );
	ap2.a_resource = ap->a_resource;
	ap2.a_ray = ap->a_ray;

	pp = PartHeadp->pt_forw;

	VJOIN1( mhit1, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir )

	pp = PartHeadp->pt_back;

	VJOIN1( mhit2, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir )

	extreme_dist1 = MAX_FASTF;
	extreme_dist2 = -MAX_FASTF;

	/* find hit vertex */
	for( i=0 ; i<BU_PTBL_END( &verts ) ; i++ )
	{
		struct vertex *v;
		struct vertex_g *vg;
		fastf_t dist;

		v = (struct vertex *)BU_PTBL_GET( &verts, i );
		if( !v )
			continue;
		vg = v->vg_p;

		switch( ap->a_user )
		{
			case X:
				if( NEAR_ZERO( ap->a_ray.r_pt[Y] - vg->coord[Y], tol.dist ) &&
				    NEAR_ZERO( ap->a_ray.r_pt[Z] - vg->coord[Z], tol.dist ) )
				{
					dist = ap->a_ray.r_pt[X] - vg->coord[X];
					if( dist < extreme_dist1 )
					{
						extreme_dist1 = dist;
						hit1_v = v;
					}
					if( dist > extreme_dist2 )
					{
						extreme_dist2 = dist;
						hit2_v = v;
					}
				}
				break;
			case Z:
				if( NEAR_ZERO( ap->a_ray.r_pt[Y] - vg->coord[Y], tol.dist ) &&
				    NEAR_ZERO( ap->a_ray.r_pt[X] - vg->coord[X], tol.dist ) )
				{
					dist = ap->a_ray.r_pt[Z] - vg->coord[Z];
					if( dist < extreme_dist1 )
					{
						extreme_dist1 = dist;
						hit1_v = v;
					}
					if( dist > extreme_dist2 )
					{
						extreme_dist2 = dist;
						hit2_v = v;
					}
				}
				break;
		}
	}

	if( hit1_v && hit1_v == hit2_v )
	{
		fastf_t dist1, dist2;

		dist1 = hit1_v->vg_p->coord[ap->a_user] - mhit1[ap->a_user];
		if( dist1 < 0.0 )
			dist1 = -dist1;
		dist2 = hit2_v->vg_p->coord[ap->a_user] - mhit2[ap->a_user];
		if( dist2 < 0.0 )
			dist2 = -dist2;

		if( dist2 >= dist1 )
			hit2_v = (struct vertex *)NULL;
		else
			hit1_v = (struct vertex *)NULL;
	}

	if( hit1_v )
	{
		/* check if hit1_v is an extreme vertex (raytrace the NMG) */
		if( (extremes=Get_extremes( s, &ap2, sd->hitmiss, sd->manifolds, hit1, hit2 ) ))
		{
			switch( ap->a_user )
			{
				case X:
					if( hit1[X] > hit1_v->vg_p->coord[X] + tol.dist )
						hit1_v = (struct vertex *)NULL;
					break;
				case Z:
					if( hit1[Z] > hit1_v->vg_p->coord[Z] + tol.dist )
					{
						if( debug )
						{
							bu_log( "Eliminating hit1_v (%g %g %g) hit1=(%g %g %g)\n",
								V3ARGS( hit1_v->vg_p->coord ),
								V3ARGS( hit1 ) );
						}
						hit1_v = (struct vertex *)NULL;
					}
					break;
			}
		}
		else if( debug )
		{
			bu_log( "No hits for pt=(%g %g %g) dir=(%g %g %g), not adjusting vertex at (%g %g %g)\n",
				V3ARGS( ap->a_ray.r_pt ), V3ARGS( ap->a_ray.r_dir ), V3ARGS( hit1_v->vg_p->coord ) );
			hit1_v = (struct vertex *)NULL;
		}
	}
	else if( debug )
	{
		bu_log( "Cannot find NMG vertex for first hit at (%g %g %g), in dir (%g %g %g)\n",
			V3ARGS( mhit1 ), V3ARGS( ap->a_ray.r_dir ) );
	}
	if( hit1_v )
	{
		struct vertexuse *vu;

		if( debug )
			bu_log( "Moving first hit vg x%x from (%g %g %g) to (%g %g %g)\n", hit1_v->vg_p,
				V3ARGS( hit1_v->vg_p->coord ), V3ARGS( mhit1 ) );
		VMOVE( hit1_v->vg_p->coord, mhit1 )
		for( BU_LIST_FOR( vu, vertexuse, &hit1_v->vu_hd ) )
		{
			struct faceuse *fu;
			struct face *f;

			fu = nmg_find_fu_of_vu( vu );
			if( !fu )
				continue;
			if( fu->orientation != OT_SAME )
				continue;

			nmg_calc_face_g( fu );
			f = fu->f_p;
			nmg_face_bb( f, &tol );

			VMINMAX( sa->min_pt, sa->max_pt, f->min_pt )
			VMINMAX( sa->min_pt, sa->max_pt, f->max_pt )
		}

		VMINMAX( ra->min_pt, ra->max_pt, sa->min_pt )
		VMINMAX( ra->min_pt, ra->max_pt, sa->max_pt )

		bu_ptbl_zero( &verts, (long *)hit1_v );
	}

	if( hit2_v )
	{
		/* check if hit2_v is an extreme vertex (raytrace the NMG) */
		if( extremes )
		{
			switch( ap->a_user )
			{
				case X:
					if( hit2[X] < hit2_v->vg_p->coord[X] - tol.dist )
						hit2_v = (struct vertex *)NULL;
					break;
				case Z:
					if( hit2[Z] < hit2_v->vg_p->coord[Z] - tol.dist )
					{
						if( debug )
						{
							bu_log( "Eliminating hit2_v (%g %g %g) hit2=(%g %g %g)\n",
								V3ARGS( hit2_v->vg_p->coord ),
								V3ARGS( hit2 ) );
						}
						hit2_v = (struct vertex *)NULL;
					}
					break;
			}
		}
		else
			hit2_v = (struct vertex *)NULL;
	}
	else if( debug )
	{
		bu_log( "Cannot find NMG vertex for last hit at (%g %g %g), in dir (%g %g %g)\n",
			V3ARGS( mhit2 ), V3ARGS( ap->a_ray.r_dir ) );
	}
	if( hit2_v )
	{
		struct vertexuse *vu;

		if( debug )
			bu_log( "Moving last hit vg x%x from (%g %g %g) to (%g %g %g)\n", hit2_v->vg_p,
				V3ARGS( hit2_v->vg_p->coord ), V3ARGS( mhit2 ) );
		VMOVE( hit2_v->vg_p->coord, mhit2 )
		for( BU_LIST_FOR( vu, vertexuse, &hit2_v->vu_hd ) )
		{
			struct faceuse *fu;
			struct face *f;

			fu = nmg_find_fu_of_vu( vu );
			if( !fu )
				continue;
			if( fu->orientation != OT_SAME )
				continue;

			nmg_calc_face_g( fu );
			f = fu->f_p;
			nmg_face_bb( f, &tol );

			VMINMAX( sa->min_pt, sa->max_pt, f->min_pt )
			VMINMAX( sa->min_pt, sa->max_pt, f->max_pt )
		}

		VMINMAX( ra->min_pt, ra->max_pt, sa->min_pt )
		VMINMAX( ra->min_pt, ra->max_pt, sa->max_pt )

		bu_ptbl_zero( &verts, (long *)hit2_v );
	}

	return( 1 );
}

static void
Split_side_faces( s )
struct shell *s;
{
	int i;
	struct bu_ptbl edges;

	nmg_edge_tabulate( &edges, &s->l.magic );
	bu_ptbl_init( &verts, 128, "verts" );

	for( i=0 ; i<BU_PTBL_END( &edges ) ; i++ )
	{
		struct edge *e;
		struct vertex_g *vg1, *vg2;
		struct edgeuse *new_eu;
		struct vertex *v;
		fastf_t y_coord;
		point_t pt;
		int j;

		e = (struct edge *)BU_PTBL_GET( &edges, i );
		NMG_CK_EDGE( e );

		vg1 = e->eu_p->vu_p->v_p->vg_p;
		vg2 = e->eu_p->eumate_p->vu_p->v_p->vg_p;

		if( vg1->coord[X] != vg2->coord[X] )
			continue;
		if( vg1->coord[Z] != vg2->coord[Z] )
			continue;

		if( vg1->coord[Y] > vg2->coord[Y] )
		{
			struct vertex_g *tmp_vg;

			tmp_vg = vg1;
			vg1 = vg2;
			vg2 = tmp_vg;
			new_eu = e->eu_p->eumate_p;
		}
		else
			new_eu = e->eu_p;

		for( j=0 ; j<y_cells ; j++ )
		{
			VSET( pt, vg1->coord[X], xy_rays[XY_CELL(0,j)].r_pt[Y], vg1->coord[Z] )
			if( pt[Y] <= vg1->coord[Y] + tol.dist || pt[Y] >= vg2->coord[Y] - tol.dist )
				continue;

			new_eu = nmg_esplit( (struct vertex *)NULL, new_eu, 0 );
			nmg_vertex_gv( new_eu->vu_p->v_p, pt );
			bu_ptbl_ins( &verts, (long *)new_eu->vu_p->v_p );
		}
	}

	bu_ptbl_free( &edges );
}

static void
shrink_wrap( s )
struct shell *s;
{
	struct faceuse *fu;
	struct application ap;
	struct nmg_shot_data sd;
	struct model *m;
	int i,j;

	NMG_CK_SHELL( s );

	Split_side_faces( s );

	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		if( fu->orientation == OT_SAME )
			nmg_triangulate_fu( fu, &tol );
	}

	nmg_split_loops_into_faces( &s->l.magic, &tol );

	m = nmg_find_model( &s->l.magic);
	nmg_rebound( m, &tol );
	mk_nmg( fd_out, "shell.1", m );

	sd.s = s;
	sd.manifolds = nmg_manifolds( m );
	sd.hitmiss = (struct hitmiss **)bu_calloc( m->maxindex, sizeof( struct hitmiss *), "nmg geom hit list");

	bzero( &ap, sizeof( struct application ) );
	ap.a_uptr = (genptr_t)&sd;
	ap.a_rt_i = rtip;
	ap.a_miss = miss;
	ap.a_overlap = a_overlap;
	ap.a_onehit = 0;
	ap.a_hit = shrink_hit;
	ap.a_user = X;

	/* shoot rays in the X-direction */
	for( i=0 ; i<y_cells ; i++ )
	{
		for( j=0 ; j<z_cells ; j++ )
		{
			VMOVE( ap.a_ray.r_pt, yz_rays[YZ_CELL(i,j)].r_pt )
			VMOVE( ap.a_ray.r_dir, yz_rays[YZ_CELL(i,j)].r_dir )
			(void)rt_shootray( &ap );
		}
	}

	/* shoot rays in the Z-direction */
	ap.a_user = Z;
	for( i=0 ; i<x_cells ; i++ )
	{
		for( j=0 ; j<y_cells ; j++ )
		{
			VMOVE( ap.a_ray.r_pt, xy_rays[XY_CELL(i,j)].r_pt )
			VMOVE( ap.a_ray.r_dir, xy_rays[XY_CELL(i,j)].r_dir )
			(void)rt_shootray( &ap );
		}
	}

	bu_free( sd.manifolds, "manifolds" );
	bu_free( (char *)sd.hitmiss, "hitmiss" );
}

static void
Make_shell()
{
	int i,j;
	int x_index, z_index;
	int cell_no[4];
	int status;
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	struct local_part *lpart[4];

	m = nmg_mm();
	r = nmg_mrsv( m );
	s = BU_LIST_FIRST( shell, &r->s_hd );

	for( x_index=0 ; x_index < x_cells - 1 ; x_index++ )
	{
		for( z_index=0 ; z_index < z_cells - 1 ; z_index++ )
		{
			cell_no[0] = XZ_CELL(x_index,z_index);
			cell_no[1] = XZ_CELL(x_index+1,z_index);
			cell_no[2] = XZ_CELL(x_index+1,z_index+1);
			cell_no[3] = XZ_CELL(x_index,z_index+1);

			for( i=0 ; i<4 ; i++ )
			{
				if( BU_LIST_IS_EMPTY( &xz_parts[cell_no[i]].l ) )
					lpart[i] = (struct local_part *)NULL;
				else
					lpart[i] = BU_LIST_FIRST( local_part, &xz_parts[cell_no[i]].l );
			}

			status = 0;
			for( i=0 ; i<4 ; i++ )
			{
				if( lpart[i] && lpart[i]->is_void == NO )
					status += vert_ids[i];
			}

			if( debug > 3 )
			{
				bu_log( "Making faces for x_index=%d, z_index=%d\n", x_index, z_index );
				for( i=0 ; i<4 ; i++ )
				{
					bu_log( "part #%d:\n", i );
					bu_log( "\tray start is (%g %g %g)\n", V3ARGS( xz_rays[cell_no[i]].r_pt ) );
					pr_part( lpart[i] );
				}
			}

			Make_simple_faces( s, status, lpart );
		}
	}

	nmg_rebound( m, &tol );

	if( do_extra_rays )
	{
		shrink_wrap( s );
		nmg_rebound( m, &tol );
	}

	bu_log( "Bounding box of output: (%g %g %g) <-> (%g %g %g)\n", V3ARGS( r->ra_p->min_pt ), V3ARGS( r->ra_p->max_pt ) );

	mk_nmg( fd_out, "shell", m );
}

static int
hit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	register struct partition *first_pp;
	register struct partition *last_pp;
	struct local_part *lpart;

	first_pp = PartHeadp->pt_forw;
	last_pp = PartHeadp->pt_back;

	lpart = GET_PART;
	lpart->in = GET_END;
	lpart->out = GET_END;
	lpart->is_void = NO;
	lpart->in->v = (struct vertex *)NULL;
	VSET( lpart->in->pt, 
		xz_rays[ap->a_user].r_pt[X],
		xz_rays[ap->a_user].r_pt[Y] + first_pp->pt_inhit->hit_dist,
		xz_rays[ap->a_user].r_pt[Z] );
	lpart->out->v = (struct vertex *)NULL;
	VSET( lpart->out->pt, 
		xz_rays[ap->a_user].r_pt[X],
		xz_rays[ap->a_user].r_pt[Y] + last_pp->pt_outhit->hit_dist,
		xz_rays[ap->a_user].r_pt[Z] );
	BU_LIST_INSERT( &xz_parts[ap->a_user].l, &lpart->l )
	return( 1 );
}

main( argc, argv )
int argc;
char *argv[];
{
	char idbuf[132];
	struct application ap;
	int i,j;
	int c;
	fastf_t x_start, y_start, z_start;

	bu_debug = BU_DEBUG_COREDUMP;

#ifdef BSD
	setlinebuf( stderr );
#else
#	if defined( SYSV ) && !defined( sgi ) && !defined(CRAY2) && \
	 !defined(n16)
		(void) setvbuf( stderr, (char *) NULL, _IOLBF, BUFSIZ );
#	endif
#	if defined(sgi) && defined(mips)
		if( setlinebuf( stderr ) != 0 )
			perror("setlinebuf(stderr)");
#	endif
#endif

	/* These need to be improved */
	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-5;
	tol.para = 1 - tol.perp;


	/* Get command line arguments. */
	while( (c=getopt( argc, argv, "rcvg:o:d:p:X:")) != EOF)
	{
		switch( c )
		{
			case 'r':	/* do extra raytracing to refine shape */
				do_extra_rays = 1;
				break;
			case 'p':	/* plot final partitions */
				plotfile = optarg;
				break;
			case 'd':	/* debug level */
				debug = atoi( optarg );
				break;
			case 'v':	/* verbose */
				verbose = 1;
				break;
			case 'g':	/* cell size */
				cell_size = atof( optarg );
				break;
			case 'o':	/* BRL-CAD output file */
				output_file = optarg;
				break;
			case 'c':	/* don't close shell */
				close_shell = 0;
				break;
			case 'X':	/* nmg debug flags */
				sscanf( optarg, "%x", &rt_g.NMG_debug );
				bu_log( "%s: setting rt_g.NMG_debug to x%x\n", argv[0], rt_g.NMG_debug );
				break;
		}
	}

	if (optind+1 >= argc)
	{
		bu_log( usage, argv[0] );
		exit( 1 );
	}

	if( output_file && sgp_file )
	{
		bu_log( usage, argv[0] );
		exit( 1 );
	}

	if( output_file )
	{
		if( (fd_out=fopen( output_file, "w")) == NULL )
		{
			bu_log( "Cannot open output file (%s)\n", output_file );
			perror( argv[0] );
			exit( 1 );
		}
		mk_id( fd_out, "test g-sgp" );
	}

	if( sgp_file )
	{
		if( (fd_sgp=fopen( sgp_file, "w")) == NULL )
		{
			bu_log( "Cannot open output file (%s)\n", sgp_file );
			perror( argv[0] );
			exit( 1 );
		}
		fprintf( fd_sgp, "object\n" );
	}

	if( plotfile )
	{
		if( (fd_plot=fopen( plotfile, "w")) == NULL )
		{
			bu_log( "Cannot open plot file (%s)\n", plotfile );
			perror( argv[0] );
			exit( 1 );
		}
	}

	/* Open brl-cad database */
	if ((rtip=rt_dirbuild(argv[optind], idbuf, sizeof(idbuf))) == RTI_NULL )
	{
		bu_log( "rt_durbuild FAILED on %s\n", argv[optind] );
		exit(1);
	}

	rtip->rti_space_partition = RT_PART_NUBSPT;

	bzero( &ap, sizeof( struct application ) );
	ap.a_rt_i = rtip;
	ap.a_hit = hit;
	ap.a_miss = miss;
	ap.a_overlap = a_overlap;
	ap.a_onehit = 0;

	while( ++optind < argc )
	{
		if( rt_gettree( rtip, argv[optind] ) < 0 )
			bu_log( "rt_gettree failed on %s\n", argv[optind] );
	}

	rt_prep( rtip );

	if( fd_plot )
		pdv_3space( fd_plot, rtip->mdl_min, rtip->mdl_max );

	x_cells = (int)((rtip->mdl_max[X] - rtip->mdl_min[X])/cell_size) + 3;
	y_cells = (int)((rtip->mdl_max[Y] - rtip->mdl_min[Y])/cell_size) + 3;
	z_cells = (int)((rtip->mdl_max[Z] - rtip->mdl_min[Z])/cell_size) + 3;
	bu_log( "cell size is %gmm\n\t%d cells in X-direction\n\t%d cells in Y-direction\n\t%d cells in Z-direction\n",
		cell_size, x_cells, y_cells, z_cells );

	x_start = rtip->mdl_min[X] - ((double)x_cells * cell_size - (rtip->mdl_max[X] - rtip->mdl_min[X]))/2.0;
	y_start = rtip->mdl_min[Y] - ((double)y_cells * cell_size - (rtip->mdl_max[Y] - rtip->mdl_min[Y]))/2.0;
	z_start = rtip->mdl_min[Z] - ((double)z_cells * cell_size - (rtip->mdl_max[Z] - rtip->mdl_min[Z]))/2.0;

	xz_parts = (struct local_part *)bu_calloc( x_cells * z_cells, sizeof( struct local_part ), "xz_parts" );

	xy_rays = (struct xray *)bu_calloc( x_cells * y_cells, sizeof( struct xray ), "xy_rays" );
	xz_rays = (struct xray *)bu_calloc( x_cells * z_cells, sizeof( struct xray ), "xz_rays" );
	yz_rays = (struct xray *)bu_calloc( y_cells * z_cells, sizeof( struct xray ), "yz_rays" );

	for( i=0 ; i<x_cells ; i++ )
	{
		for( j=0 ; j<z_cells ; j++ )
		{
			xz_parts[XZ_CELL(i,j)].is_void = UNKNOWN;
			xz_parts[XZ_CELL(i,j)].in_coord = -MAX_FASTF;
			xz_parts[XZ_CELL(i,j)].out_coord = -MAX_FASTF;
			xz_parts[XZ_CELL(i,j)].in = (struct end_pt *)NULL;
			xz_parts[XZ_CELL(i,j)].out = (struct end_pt *)NULL;
			BU_LIST_INIT( &xz_parts[XZ_CELL(i,j)].l );
		}
	}


	for( i=0 ; i<y_cells ; i++ )
	{
		for( j=0 ; j<z_cells ; j++ )
		{
			VSET( yz_rays[YZ_CELL(i,j)].r_dir, -1.0, 0.0, 0.0 )
			VSET( yz_rays[YZ_CELL(i,j)].r_pt,
				rtip->mdl_max[X] + cell_size,
				y_start + (fastf_t)i * cell_size,
				z_start + (fastf_t)j * cell_size )
		}
	}

	for( i=0 ; i<x_cells ; i++ )
	{
		for( j=0 ; j<y_cells ; j++ )
		{
			VSET( xy_rays[XY_CELL(i,j)].r_dir, 0.0, 0.0, -1.0 )
			VSET( xy_rays[XY_CELL(i,j)].r_pt,
				x_start + (fastf_t)i * cell_size,
				y_start + (fastf_t)j * cell_size,
				rtip->mdl_max[Z] + cell_size )
		}

		for( j=0 ; j<z_cells ; j++ )
		{
			VSET( xz_rays[XZ_CELL(i,j)].r_dir, 0.0, 1.0, 0.0 )
			VSET( xz_rays[XZ_CELL(i,j)].r_pt,
				x_start + (fastf_t)i * cell_size,
				rtip->mdl_min[Y] - cell_size,
				z_start + (fastf_t)j * cell_size )
		}
	}

	bu_log( "Bounding box of BRL-CAD model: (%g %g %g) <-> (%g %g %g)\n", V3ARGS( rtip->mdl_min ), V3ARGS( rtip->mdl_max ) );
	/* shoot rays in Y-direction */
	for( i=0 ; i<x_cells ; i++ )
	{
		ray_dir = Y;
		for( j=0 ; j<z_cells ; j++ )
		{
			ap.a_x = i;
			ap.a_y = j;
			ap.a_user = XZ_CELL(i,j);
			VMOVE( ap.a_ray.r_pt, xz_rays[XZ_CELL(i,j)].r_pt )
			VMOVE( ap.a_ray.r_dir, xz_rays[XZ_CELL(i,j)].r_dir )
			(void)rt_shootray( &ap );
		}
	}

	Make_shell();
	if( fd_plot )
	{
		vect_t tmp_dir;
		fastf_t dir_len;

		VSET( tmp_dir, 0, 0, 1 );
		dir_len = (rtip->mdl_max[Z] - rtip->mdl_min[Z])/((double)(z_cells * 2));
		VSCALE( tmp_dir, tmp_dir, dir_len );
		/* plot partitions */
		for( i=0 ; i<x_cells ; i++ )
		{
			for( j=0 ; j<z_cells ; j++ )
			{
				struct local_part *lpart;
				int index;
				point_t tmp_pt;

				index = XZ_CELL(i,j);
				if( BU_LIST_IS_EMPTY( &xz_parts[index].l ) )
					continue;

				lpart = BU_LIST_FIRST( local_part,  &xz_parts[index].l );
				while( BU_LIST_NOT_HEAD( &lpart->l, &xz_parts[index].l ) )
				{
					switch( lpart->is_void )
					{
						case YES:
							/* make voids red */
							pl_color( fd_plot, 255, 0, 0 );
							pdv_3line( fd_plot, lpart->in->pt, lpart->out->pt );
							break;
						case NO:
							/* make solid area green */
							pl_color( fd_plot, 0, 255, 0 );
							pdv_3line( fd_plot, lpart->in->pt, lpart->out->pt );
							break;
						case UNKNOWN:
							/* make unknown blue */
							pl_color( fd_plot, 0, 0, 255 );
							pdv_3line( fd_plot, lpart->in->pt, lpart->out->pt );
							break;
						default:
							/* make errors purple */
							pl_color( fd_plot, 255, 0, 255 );
							pdv_3line( fd_plot, lpart->in->pt, lpart->out->pt );
							break;
					}
					/* put a little white line at start and end of each partition */
					pl_color( fd_plot, 255, 255, 255 );
					VADD2( tmp_pt, lpart->in->pt, tmp_dir );
					pdv_3line( fd_plot, lpart->in->pt, tmp_pt );
					VADD2( tmp_pt, lpart->out->pt, tmp_dir );
					pdv_3line( fd_plot, lpart->out->pt, tmp_pt );

					lpart = BU_LIST_NEXT( local_part, &lpart->l );
				}
			}
		}
	}
}
