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

#define MAKE_TRIANGLES	0

#define ABS( _x )	((_x)<0.0?(-(_x)):(_x))

#define IN_SEG( _x, _a, _b)	(((_x) > _a + tol.dist && (_x) < _b - tol.dist) || \
				 ((_x) < _a - tol.dist && (_x) > _b + tol.dist))

struct refine_rpp
{
	struct bu_list h;
	point_t min, max;
	fastf_t tolerance;
};

static struct bu_list add_rpp_head;
static struct bu_list subtract_rpp_head;

struct refine_data
{
	struct loopuse *lu1, *lu2;
	struct edgeuse *eu1, *eu2;
	struct faceuse *fu1, *fu2;
	pointp_t mid_pt;
	struct bu_ptbl *new_edges;
};

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
static char *usage="Usage:\n\
	%s [-d debug_level] [-n] [-v] [-i initial_ray_dir] [-g cell_size] [-d debug_level] -o brlcad_output_file database.g object1 object2...\n";
static char *_inside="inside";
static char *_outside="outside";
static char dir_ch[3]={ 'X', 'Y', 'Z' };

static struct local_part *xy_parts=(struct local_part *)NULL;
static struct local_part *xz_parts=(struct local_part *)NULL;
static struct local_part *yz_parts=(struct local_part *)NULL;

static int	initial_ray_dir=-1;
static int	do_extra_rays=1;
static long	face_count=0;
static fastf_t	cell_size=50.0;
static fastf_t	cell_size_sq=2500.0;
static fastf_t	edge_tol=0.0;
static FILE	*fd_sgp=NULL, *fd_out=NULL, *fd_plot=NULL;
static char	*output_file=(char *)NULL;
static char	*plotfile;
static long	rpp_count=0;
static short	vert_ids[8]={1, 2, 4, 8, 16, 32, 64, 128};
static int	x_del[4]={0, 1, 1, 0};
static int	z_del[4]={0, 0, 1, 1};
static int	debug=0;
static char	*token_seps=" \t,;\n";
static int	cur_dir=0;
static int	cell_count[3];
static fastf_t	decimation_tol=0.0;
static fastf_t	min_angle=0.0;

#define	XY_CELL( _i, _j )	((_i)*cell_count[Y] + (_j))
#define	XZ_CELL( _i, _j )	((_i)*cell_count[Z] + (_j))
#define	YZ_CELL( _i, _j )	((_i)*cell_count[Z] + (_j))
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
	fastf_t ave_y;
	fastf_t diff[4];
	int max_diff;
	int i;

	switch( status )
	{
		case 3:		/* bottom faces */
#if MAKE_TRIANGLES
				MAKE_FACE( lpart[1]->in, lpart[1]->out, lpart[0]->out, s )
				MAKE_FACE( lpart[1]->in, lpart[0]->out, lpart[0]->in, s )
#else
				MAKE_FACE_R( lpart[1]->in, lpart[1]->out, lpart[0]->out, lpart[0]->in, s )
#endif
			break;
		case 6:		/* right side faces */
#if MAKE_TRIANGLES
				MAKE_FACE( lpart[2]->in, lpart[2]->out, lpart[1]->out, s )
				MAKE_FACE( lpart[2]->in, lpart[1]->out, lpart[1]->in, s )
#else
				MAKE_FACE_R( lpart[2]->in, lpart[2]->out, lpart[1]->out, lpart[1]->in, s )
#endif
			break;
		case 7:		/* partial front and back faces and a diagonal face */
				MAKE_FACE( lpart[0]->in, lpart[1]->in, lpart[2]->in, s )
				MAKE_FACE( lpart[2]->out, lpart[1]->out, lpart[0]->out, s )
#if MAKE_TRIANGLES
				MAKE_FACE( lpart[2]->in, lpart[2]->out, lpart[0]->out, s )
				MAKE_FACE( lpart[2]->in, lpart[0]->out, lpart[0]->in, s )
#else
				MAKE_FACE_R( lpart[2]->in, lpart[2]->out, lpart[0]->out, lpart[0]->in, s )
#endif
			break;
		case 9:		/* left side faces */
#if MAKE_TRIANGLES
				MAKE_FACE( lpart[0]->in, lpart[0]->out, lpart[3]->out, s )
				MAKE_FACE( lpart[0]->in, lpart[3]->out, lpart[3]->in, s )
#else
				MAKE_FACE_R( lpart[0]->in, lpart[0]->out, lpart[3]->out, lpart[3]->in, s )
#endif
			break;
		case 11:	/* partial front and back faces and a diagonal face */
				MAKE_FACE( lpart[0]->in, lpart[1]->in, lpart[3]->in, s )
				MAKE_FACE( lpart[3]->out, lpart[1]->out, lpart[0]->out, s )
#if MAKE_TRIANGLES
				MAKE_FACE( lpart[1]->in, lpart[1]->out, lpart[3]->out, s )
				MAKE_FACE( lpart[1]->in, lpart[3]->out, lpart[3]->in, s )
#else
				MAKE_FACE_R( lpart[1]->in, lpart[1]->out, lpart[3]->out, lpart[3]->in, s )
#endif
			break;
		case 12:	/* top faces */
#if MAKE_TRIANGLES
				MAKE_FACE( lpart[3]->in, lpart[3]->out, lpart[2]->out, s )
				MAKE_FACE( lpart[3]->in, lpart[2]->out, lpart[2]->in, s )
#else
				MAKE_FACE_R( lpart[3]->in, lpart[3]->out, lpart[2]->out, lpart[2]->in, s )
#endif
			break;
		case 13:	/* partial front and back faces and a diagonal face */
				MAKE_FACE( lpart[0]->in, lpart[2]->in, lpart[3]->in, s )
				MAKE_FACE( lpart[3]->out, lpart[2]->out, lpart[0]->out, s )
#if MAKE_TRIANGLES
				MAKE_FACE( lpart[0]->in, lpart[0]->out, lpart[2]->out, s )
				MAKE_FACE( lpart[0]->in, lpart[2]->out, lpart[2]->in, s )
#else
				MAKE_FACE_R( lpart[0]->in, lpart[0]->out, lpart[2]->out, lpart[2]->in, s )
#endif
			break;
		case 14:	/* partial front and back faces and a diagonal face */
				MAKE_FACE( lpart[1]->in, lpart[2]->in, lpart[3]->in, s )
				MAKE_FACE( lpart[3]->out, lpart[2]->out, lpart[1]->out, s )
#if MAKE_TRIANGLES
				MAKE_FACE( lpart[3]->in, lpart[3]->out, lpart[1]->out, s )
				MAKE_FACE( lpart[3]->in, lpart[1]->out, lpart[1]->in, s )
#else
				MAKE_FACE_R( lpart[3]->in, lpart[3]->out, lpart[1]->out, lpart[1]->in, s )
#endif
			break;
		case 15:	/* front and back faces */
				ave_y = 0;
				for( i=0 ; i<4 ; i++ )
					ave_y += lpart[i]->in->pt[Y];
				ave_y /= 4.0;
				for( i=0 ; i<4 ; i++ )
				{
					diff[i] = lpart[i]->in->pt[Y] - ave_y;
					diff[i] = ABS( diff[i] );
				}
				max_diff = 0;
				for( i=1 ; i<4 ; i++ )
				{
					if( diff[i] > diff[max_diff] )
						max_diff = i;
				}
				if( max_diff == 1 || max_diff == 3 )
				{
					MAKE_FACE( lpart[0]->in, lpart[1]->in, lpart[2]->in, s )
					MAKE_FACE( lpart[0]->in, lpart[2]->in, lpart[3]->in, s )
				}
				else
				{
					MAKE_FACE( lpart[0]->in, lpart[1]->in, lpart[3]->in, s )
					MAKE_FACE( lpart[3]->in, lpart[1]->in, lpart[2]->in, s )
				}

				ave_y = 0;
				for( i=0 ; i<4 ; i++ )
					ave_y += lpart[i]->out->pt[Y];
				ave_y /= 4.0;
				for( i=0 ; i<4 ; i++ )
				{
					diff[i] = lpart[i]->out->pt[Y] - ave_y;
					diff[i] = ABS( diff[i] );
				}
				max_diff = 0;
				for( i=1 ; i<4 ; i++ )
				{
					if( diff[i] > diff[max_diff] )
						max_diff = i;
				}
				if( max_diff == 1 || max_diff == 3 )
				{
					MAKE_FACE( lpart[2]->out, lpart[1]->out, lpart[0]->out, s )
					MAKE_FACE( lpart[2]->out, lpart[0]->out, lpart[3]->out, s )
				}
				else
				{
					MAKE_FACE( lpart[3]->out, lpart[1]->out, lpart[0]->out, s )
					MAKE_FACE( lpart[2]->out, lpart[1]->out, lpart[3]->out, s )
				}
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

		if( (ap->a_user == X || NEAR_ZERO( ap->a_ray.r_pt[X] - vg->coord[X], tol.dist )) &&
		    (ap->a_user == Y || NEAR_ZERO( ap->a_ray.r_pt[Y] - vg->coord[Y], tol.dist )) &&
		    (ap->a_user == Z || NEAR_ZERO( ap->a_ray.r_pt[Z] - vg->coord[Z], tol.dist )) )
		{
			dist = vg->coord[ap->a_user] - ap->a_ray.r_pt[ap->a_user];
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
	}

	if( hit1_v || hit2_v )
	{
		vect_t diff1, diff2;
		fastf_t len1_sq, len2_sq;

		extremes=Get_extremes( s, &ap2, sd->hitmiss, sd->manifolds, hit1, hit2 );

		if( debug )
		{
			bu_log( "shrink_hit:\n\thit1_v=(%g %g %g), hit2_v=(%g %g %g)\n",
				V3ARGS( hit1_v->vg_p->coord ),
				V3ARGS( hit2_v->vg_p->coord ) );
			bu_log( "\tNMG extremes are (%g %g %g)<->(%g %g %g)\n",
				V3ARGS( hit1 ),
				V3ARGS( hit2 ) );
			bu_log( "\tmodel extremes are (%g %g %g)<->(%g %g %g)\n",
				V3ARGS( mhit1 ),
				V3ARGS( mhit2 ) );
		}

		VSUB2( diff1, hit1_v->vg_p->coord, hit1 );
		VSUB2( diff2, hit2_v->vg_p->coord, hit2 );

		len1_sq = MAGSQ( diff1 );
		len2_sq = MAGSQ( diff2 );

		if( !NEAR_ZERO( len1_sq, tol.dist_sq ) )
			hit1_v = (struct vertex *)NULL;

		if( !NEAR_ZERO( len2_sq, tol.dist_sq ) )
			hit2_v = (struct vertex *)NULL;
	}

	if( hit1_v && hit1_v == hit2_v )
	{
		/* only one vertex found, which is it?? */
		fastf_t dist1, dist2;

		if( debug )
			bu_log( "hit1_v == hit2_v (still) (%g %g %g)\n", V3ARGS( hit1_v->vg_p->coord ) );

		dist1 = hit1_v->vg_p->coord[ap->a_user] - mhit1[ap->a_user];
		if( dist1 < 0.0 )
			dist1 = -dist1;
		dist2 = hit2_v->vg_p->coord[ap->a_user] - mhit2[ap->a_user];
		if( dist2 < 0.0 )
			dist2 = -dist2;

		if( debug )
		{
			bu_log( "\tmhit1=(%g %g %g) mhit2=(%g %g %g)\n", V3ARGS( mhit1 ), V3ARGS( mhit2 ) );
			bu_log( "\tdist1=%g dist2=%g\n", dist1, dist2 );
		}

		if( dist2 >= dist1 )
		{
			if( debug )
				bu_log( "\t\teliminating hit2_v\n" );
			hit2_v = (struct vertex *)NULL;
		}
		else
		{
			if( debug )
				bu_log( "\t\teliminating hit1_v\n" );
			hit1_v = (struct vertex *)NULL;
		}
	}

	/* Don't allow moving the vertex normalward more than a cell width.
	 * If the point should have been there, the original rays should have caught it.
	 */
	if( hit1_v )
	{
		vect_t v1;
		struct vertexuse *vu;
		fastf_t dist_sq;

		VSUB2( v1, mhit1, hit1_v->vg_p->coord );
		dist_sq = MAGSQ( v1 );

		if( dist_sq > cell_size_sq )
		{

			for( BU_LIST_FOR( vu, vertexuse, &hit1_v->vu_hd ) )
			{
				struct faceuse *fu;
				vect_t norm;

				fu = nmg_find_fu_of_vu( vu );
				if( fu->orientation != OT_SAME )
					continue;

				NMG_GET_FU_NORMAL( norm, fu );

				if( VDOT( norm, v1 ) > 0.0 )
				{
					hit1_v = (struct vertex *)NULL;
					break;
				}
			}
		}
	}
	if( hit2_v )
	{
		vect_t v2;
		struct vertexuse *vu;
		fastf_t dist_sq;

		VSUB2( v2, mhit2, hit2_v->vg_p->coord );
		dist_sq = MAGSQ( v2 );

		if( dist_sq > cell_size_sq )
		{

			for( BU_LIST_FOR( vu, vertexuse, &hit2_v->vu_hd ) )
			{
				struct faceuse *fu;
				vect_t norm;

				fu = nmg_find_fu_of_vu( vu );
				if( fu->orientation != OT_SAME )
					continue;

				NMG_GET_FU_NORMAL( norm, fu );

				if( VDOT( norm, v2 ) > 0.0 )
				{
					hit2_v = (struct vertex *)NULL;
					break;
				}
			}
		}
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
			struct edgeuse *eu;
			struct edge_g_lseg *eg;
			pointp_t pt;

			if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
				continue;

			eu = vu->up.eu_p;
			eg = eu->g.lseg_p;

			VMOVE( eg->e_pt, mhit1 );
			pt = eu->eumate_p->vu_p->v_p->vg_p->coord;
			VSUB2(eg->e_dir, eg->e_pt, pt);

			fu = nmg_find_fu_of_eu( eu );
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
		struct vertexuse *vu;

		if( debug )
			bu_log( "Moving last hit vg x%x from (%g %g %g) to (%g %g %g)\n", hit2_v->vg_p,
				V3ARGS( hit2_v->vg_p->coord ), V3ARGS( mhit2 ) );
		VMOVE( hit2_v->vg_p->coord, mhit2 )
		for( BU_LIST_FOR( vu, vertexuse, &hit2_v->vu_hd ) )
		{
			struct faceuse *fu;
			struct face *f;
			struct edgeuse *eu;
			struct edge_g_lseg *eg;
			pointp_t pt;

			if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
				continue;

			eu = vu->up.eu_p;
			eg = eu->g.lseg_p;

			VMOVE( eg->e_pt, mhit2 );
			pt = eu->eumate_p->vu_p->v_p->vg_p->coord;
			VSUB2(eg->e_dir, eg->e_pt, pt);

			fu = nmg_find_fu_of_eu( eu );
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
Split_side_faces( s, tab )
struct shell *s;
struct bu_ptbl *tab;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct bu_ptbl faces;
	vect_t ray_dir;
	int face_no;

	NMG_CK_SHELL( s );

	bu_ptbl_init( &faces, 128, "faces" );

	VSETALL( ray_dir, 0.0 );
	ray_dir[cur_dir] = 1.0;

	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		int count=0;

		if( fu->orientation != OT_SAME )
			continue;

		lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );

		for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			count++;

		if( count == 3 )
			continue;

		if( count != 4 )
		{
			bu_log( "Found a loop with %d edge!!!! (should be 3 or 4)\n", count );
			nmg_pr_fu_briefly( fu, "" );
			continue;
		}

		bu_ptbl_ins( &faces, (long *)fu );
	}

	for( face_no=0 ; face_no<BU_PTBL_END( &faces ) ; face_no++ )
	{
		struct edgeuse *eu1=(struct edgeuse *)NULL, *eu2=(struct edgeuse *)NULL;
		fastf_t min_coord1=MAX_FASTF, min_coord2=MAX_FASTF;
		struct vertex_g *vg1a, *vg1b, *vg2a, *vg2b;
		int cell_no;

		fu = (struct faceuse *)BU_PTBL_GET( &faces, face_no );
		lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );

		for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
		{
			vect_t edge_dir;
			struct vertex_g *vga, *vgb;
			fastf_t dot;

			vga = eu->vu_p->v_p->vg_p;
			vgb = eu->eumate_p->vu_p->v_p->vg_p;

			VSUB2( edge_dir, vgb->coord, vga->coord );

			if( (cur_dir == X && (edge_dir[Y] != 0.0 || edge_dir[Z] != 0.0) ) ||
			    (cur_dir == Y && (edge_dir[X] != 0.0 || edge_dir[Z] != 0.0) ) ||
			    (cur_dir == Z && (edge_dir[Y] != 0.0 || edge_dir[X] != 0.0) ) )
				continue;

			if( edge_dir[cur_dir] > 0.0 )
			{
				if( vga->coord[cur_dir] < min_coord1 )
				{
					min_coord1 = vga->coord[cur_dir];
					eu1 = eu;
				}
			}
			else
			{
				if( vgb->coord[cur_dir] < min_coord2 )
				{
					min_coord2 = vgb->coord[cur_dir];
					eu2 = eu->eumate_p;
				}
			}
		}

		if( !eu1 || !eu2 )
		{
			bu_log( "Could not find edges to split in loop:\n" );
			nmg_pr_fu_briefly( fu, "" );
			continue;
		}

		/* split the parallel edges and cut the loop */
		for( cell_no=0 ; cell_no<cell_count[cur_dir] ; cell_no++ )
		{
			struct loopuse *new_lu;
			struct vertexuse *vu_cut;
			fastf_t cut_value;
			struct vertexuse *vu1_cut, *vu2_cut;

			switch( cur_dir )
			{
				case X:
					cut_value = xy_rays[XY_CELL(cell_no,0)].r_pt[cur_dir];
					break;
				case Y:
					cut_value = xy_rays[XY_CELL(0,cell_no)].r_pt[cur_dir];
					break;
				case Z:
					cut_value = xz_rays[XZ_CELL(0,cell_no)].r_pt[cur_dir];
					break;
			}

			vg1a = eu1->vu_p->v_p->vg_p;
			vg1b = eu1->eumate_p->vu_p->v_p->vg_p;
			vg2a = eu2->vu_p->v_p->vg_p;
			vg2b = eu2->eumate_p->vu_p->v_p->vg_p;

			while( vg1b->coord[cur_dir] < cut_value )
			{
				struct edgeuse *eu_tmp;
				struct vertex_g *vg_tmp;

				/* go to next eu */
				eu_tmp = BU_LIST_PNEXT_CIRC( edgeuse, &eu1->l );
				vg_tmp = eu_tmp->eumate_p->vu_p->v_p->vg_p;

				if( (cur_dir == X && (vg_tmp->coord[Y] == vg1b->coord[Y] && vg_tmp->coord[Z] == vg1b->coord[Z]) ) ||
				    (cur_dir == Y && (vg_tmp->coord[X] == vg1b->coord[X] && vg_tmp->coord[Z] == vg1b->coord[Z]) ) ||
				    (cur_dir == Z && (vg_tmp->coord[X] == vg1b->coord[X] && vg_tmp->coord[Y] == vg1b->coord[Y]) ) )
				{
					eu1 = eu_tmp;
					vg1a = vg1b;
					vg1b = vg_tmp;
				}
				else
					break;
			}

			while( vg2b->coord[cur_dir] < cut_value )
			{
				struct edgeuse *eu_tmp;
				struct vertex_g *vg_tmp;

				/* go to next eu */
				eu_tmp = BU_LIST_PNEXT_CIRC( edgeuse, &eu2->l );
				vg_tmp = eu_tmp->eumate_p->vu_p->v_p->vg_p;

				if( (cur_dir == X && (vg_tmp->coord[Y] == vg2b->coord[Y] && vg_tmp->coord[Z] == vg2b->coord[Z]) ) ||
				    (cur_dir == Y && (vg_tmp->coord[X] == vg2b->coord[X] && vg_tmp->coord[Z] == vg2b->coord[Z]) ) ||
				    (cur_dir == Z && (vg_tmp->coord[X] == vg2b->coord[X] && vg_tmp->coord[Y] == vg2b->coord[Y]) ) )
				{
					eu2 = eu_tmp;
					vg2a = vg2b;
					vg2b = vg_tmp;
				}
				else
					break;
			}

			vu1_cut = (struct vertexuse *)NULL;
			vu2_cut = (struct vertexuse *)NULL;

			if( IN_SEG( cut_value, vg1a->coord[cur_dir], vg1b->coord[cur_dir] ) )
			{
				struct edgeuse *new_eu;
				point_t cut_pt;

				/* split eu1 at cut_value */
				eu1 = nmg_esplit( (struct vertex *)NULL, eu1, 0 );
				VMOVE( cut_pt, vg1a->coord );
				cut_pt[cur_dir] = cut_value;
				nmg_vertex_gv( eu1->vu_p->v_p, cut_pt );
				bu_ptbl_ins( tab, (long *)eu1->vu_p->v_p );
				vg1a = eu1->vu_p->v_p->vg_p;
				vu1_cut = eu1->vu_p;
			}
			else if( vg1a->coord[cur_dir] == cut_value )
				vu1_cut = eu1->vu_p;
			else if( vg1b->coord[cur_dir] == cut_value )
				vu1_cut = BU_LIST_PNEXT_CIRC( edgeuse, &eu1->l )->vu_p;

			if( IN_SEG( cut_value, vg2a->coord[cur_dir], vg2b->coord[cur_dir] ) )
			{
				struct edgeuse *new_eu;
				point_t cut_pt;

				/* split eu2 at cut_value */
				eu2 = nmg_esplit( (struct vertex *)NULL, eu2, 0 );
				VMOVE( cut_pt, vg2a->coord );
				cut_pt[cur_dir] = cut_value;
				nmg_vertex_gv( eu2->vu_p->v_p, cut_pt );
				bu_ptbl_ins( tab, (long *)eu2->vu_p->v_p );
				vg2a = eu2->vu_p->v_p->vg_p;
				vu_cut = (struct vertexuse *)NULL;
				for( BU_LIST_FOR( vu_cut, vertexuse, &eu2->vu_p->v_p->vu_hd ) )
				{
					if( nmg_find_lu_of_vu( vu_cut ) == lu )
					{
						vu2_cut = vu_cut;
						break;
					}
				}
			}
			else if( vg2a->coord[cur_dir] == cut_value )
			{
				vu_cut = (struct vertexuse *)NULL;
				for( BU_LIST_FOR( vu_cut, vertexuse, &eu2->vu_p->v_p->vu_hd ) )
				{
					if( nmg_find_lu_of_vu( vu_cut ) == lu )
					{
						vu2_cut = vu_cut;
						break;
					}
				}
			}
			else if( vg2b->coord[cur_dir] == cut_value )
			{
				vu_cut = (struct vertexuse *)NULL;
				for( BU_LIST_FOR( vu_cut, vertexuse, &eu2->eumate_p->vu_p->v_p->vu_hd ) )
				{
					if( nmg_find_lu_of_vu( vu_cut ) == lu )
					{
						vu2_cut = vu_cut;
						break;
					}
				}
			}

			if( vu1_cut && vu2_cut )
			{
				/* need to set new eu1 and eu2 before cut, because they may up in new_lu */
				if( vu2_cut->v_p == eu2->eumate_p->vu_p->v_p )
					eu2 = BU_LIST_PNEXT_CIRC( edgeuse, &eu2->l );
				if( vu1_cut->v_p == eu1->eumate_p->vu_p->v_p )
					eu1 = BU_LIST_PNEXT_CIRC( edgeuse, &eu1->l );
				/* cut loop */
				new_lu = nmg_cut_loop( vu1_cut, vu2_cut );
				lu->orientation = OT_SAME;
				lu->lumate_p->orientation = OT_SAME;
				new_lu->orientation = OT_SAME;
				new_lu->lumate_p->orientation = OT_SAME;
			}
		}
	}

	bu_ptbl_free( &faces );
}

static void
shrink_wrap( s )
struct shell *s;
{
	struct faceuse *fu;
	struct application ap;
	struct nmg_shot_data sd;
	struct bu_ptbl extra_verts;
	struct model *m;
	int vert_no;
	int i,j;
	int dirs;

	NMG_CK_SHELL( s );

	m = nmg_find_model( &s->l.magic);

	bu_ptbl_init( &extra_verts, 64, "extra verts" );

	bu_ptbl_init( &verts, 128, "verts" );
	Split_side_faces( s, &verts );

	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		if( fu->orientation == OT_SAME )
			nmg_triangulate_fu( fu, &tol );
	}

	nmg_split_loops_into_faces( &s->l.magic, &tol );

	sd.s = s;
	sd.manifolds = nmg_manifolds( m );
	sd.hitmiss = (struct hitmiss **)bu_calloc( 2 * m->maxindex, sizeof( struct hitmiss *), "nmg geom hit list");

	bzero( &ap, sizeof( struct application ) );
	ap.a_uptr = (genptr_t)&sd;
	ap.a_rt_i = rtip;
	ap.a_miss = miss;
	ap.a_overlap = a_overlap;
	ap.a_onehit = 0;
	ap.a_hit = shrink_hit;

	for( dirs=0 ; dirs<2 ; dirs++ )
	{
		cur_dir++;
		if( cur_dir == 3 )
			cur_dir = 0;
		bu_log( "Shooting refining rays in %c-direction...\n", dir_ch[cur_dir] );
		ap.a_user = cur_dir;
		switch( cur_dir )
		{
			case X:
				for( i=0 ; i<cell_count[Y] ; i++ )
				{
					for( j=0 ; j<cell_count[Z] ; j++ )
					{
						VMOVE( ap.a_ray.r_pt, yz_rays[YZ_CELL(i,j)].r_pt )
						VMOVE( ap.a_ray.r_dir, yz_rays[YZ_CELL(i,j)].r_dir )
						(void)rt_shootray( &ap );
					}
				}
				break;
			case Y:
				for( i=0 ; i<cell_count[X] ; i++ )
				{
					for( j=0 ; j<cell_count[Z] ; j++ )
					{
						VMOVE( ap.a_ray.r_pt, xz_rays[XZ_CELL(i,j)].r_pt )
						VMOVE( ap.a_ray.r_dir, xz_rays[XZ_CELL(i,j)].r_dir )
						(void)rt_shootray( &ap );
					}
				}
				break;
			case Z:
				for( i=0 ; i<cell_count[X] ; i++ )
				{
					for( j=0 ; j<cell_count[Y] ; j++ )
					{
						VMOVE( ap.a_ray.r_pt, xy_rays[XY_CELL(i,j)].r_pt )
						VMOVE( ap.a_ray.r_dir, xy_rays[XY_CELL(i,j)].r_dir )
						(void)rt_shootray( &ap );
					}
				}
				break;
		}
	}

	for( vert_no=0 ; vert_no<BU_PTBL_END( &extra_verts ) ; vert_no++ )
	{
		struct vertex *v;
		struct faceuse *fu;
		struct vertexuse *vu;
		fastf_t count=0;
		vect_t dir;
		vect_t abs_dir;
		vect_t min_dist, max_dist, mins;
		int dir_index;

		v = (struct vertex *)BU_PTBL_GET( &extra_verts, vert_no );
		VSETALL( dir, 0.0 );
		for( BU_LIST_FOR( vu, vertexuse, &v->vu_hd ) )
		{
			vect_t norm;

			fu = nmg_find_fu_of_vu( vu );
			if( fu->orientation != OT_SAME )
				continue;

			NMG_GET_FU_NORMAL( norm, fu );

			VADD2( dir, dir, norm );
		}

		VUNITIZE( dir );

		abs_dir[X] = ABS( dir[X] );
		abs_dir[Y] = ABS( dir[Y] );
		abs_dir[Z] = ABS( dir[Z] );

		dir_index = X;
		if( abs_dir[Y] > abs_dir[dir_index] )
			dir_index = Y;
		if( abs_dir[Z] > abs_dir[dir_index] )
			dir_index = Z;

		ap.a_user = dir_index;
		VMOVE( ap.a_ray.r_pt, v->vg_p->coord );
		ap.a_ray.r_pt[dir_index] -=  5 * cell_size;
		VSETALL( ap.a_ray.r_dir, 0.0 );
		ap.a_ray.r_dir[dir_index] = 1.0;
		(void)rt_shootray( &ap );
	}
	bu_free( sd.manifolds, "manifolds" );
	bu_free( (char *)sd.hitmiss, "hitmiss" );
}

static int
refine_hit( ap, PartHeadp )
register struct application *ap;
struct partition *PartHeadp;
{
	struct partition *pp;
	struct refine_data *ref_data;
	struct vertexuse *vu1, *vu2;
	struct edgeuse *new_eu;
	struct vertexuse *new_vu;
	struct vertexuse *new_vu1, *new_vu2;
	struct loopuse *new_lu1, *new_lu2;
	struct vertexuse *vu;
	struct edgeuse *eu;
	point_t hit_pt;
	vect_t diff;
	fastf_t dist;
	fastf_t use_tolerance=edge_tol;
	struct refine_rpp *rpp;

	if( debug )
		bu_log( "hit\n" );

	ref_data = (struct refine_data *)ap->a_uptr;
	pp = PartHeadp->pt_forw;
	VJOIN1( hit_pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir );

	for( BU_LIST_FOR( rpp, refine_rpp, &add_rpp_head ) )
	{
		if( V3PT_IN_RPP( hit_pt, rpp->min, rpp->max ) )
		{
			if( rpp->tolerance < use_tolerance )
				use_tolerance = rpp->tolerance;
		}
	}

	VSUB2( diff, hit_pt, ref_data->mid_pt );
	dist = MAGNITUDE( diff );
	if( dist <= use_tolerance || dist > 1.4142 * cell_size )
		return( 0 );

	if( fd_plot )
	{
		struct loopuse *lu;
		struct vertex_g *vg;

		fprintf( fd_plot, "-----\n" );
		fprintf( fd_plot, "vdraw params color 00ff00\n" );
		for( BU_LIST_FOR( lu, loopuse, &ref_data->fu1->lu_hd ) )
		{
			eu = BU_LIST_LAST( edgeuse, &lu->down_hd );
			vg = eu->vu_p->v_p->vg_p;
			fprintf( fd_plot, "vdraw write 0 %g %g %g\n", V3ARGS( vg->coord ) );
			
			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			{
				vg = eu->vu_p->v_p->vg_p;
				fprintf( fd_plot, "vdraw write 1 %g %g %g\n", V3ARGS( vg->coord ) );
			}
		}

		for( BU_LIST_FOR( lu, loopuse, &ref_data->fu2->lu_hd ) )
		{
			eu = BU_LIST_LAST( edgeuse, &lu->down_hd );
			vg = eu->vu_p->v_p->vg_p;
			fprintf( fd_plot, "vdraw write 0 %g %g %g\n", V3ARGS( vg->coord ) );
			
			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			{
				vg = eu->vu_p->v_p->vg_p;
				fprintf( fd_plot, "vdraw write 1 %g %g %g\n", V3ARGS( vg->coord ) );
			}
		}
	}

	eu = BU_LIST_PNEXT_CIRC( edgeuse, &ref_data->eu1->l );
	eu = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
	vu1 = eu->vu_p;
	eu = BU_LIST_PNEXT_CIRC( edgeuse, &ref_data->eu2->l );
	eu = BU_LIST_PNEXT_CIRC( edgeuse, &eu->l );
	vu2 = eu->vu_p;
	new_eu = nmg_esplit( (struct vertex *)NULL, ref_data->eu1, 0 );
	new_vu = new_eu->vu_p;

	/* find new vu in lu1 */
	new_vu1 = (struct vertexuse *)NULL;
	for( BU_LIST_FOR( vu, vertexuse, &new_vu->v_p->vu_hd ) )
	{
		if( nmg_find_lu_of_vu( vu ) == ref_data->lu1 )
		{
			new_vu1 = vu;
			break;
		}
	}
	if( !new_vu1 )
		bu_bomb( "Cannot find VU in lu1\n" );

	/* find new vu in lu2 */
	new_vu2 = (struct vertexuse *)NULL;
	for( BU_LIST_FOR( vu, vertexuse, &new_vu->v_p->vu_hd ) )
	{
		if( nmg_find_lu_of_vu( vu ) == ref_data->lu2 )
		{
			new_vu2 = vu;
			break;
		}
	}
	if( !new_vu2 )
		bu_bomb( "Cannot find VU in lu2\n" );
	new_lu1 = nmg_cut_loop( new_vu1, vu1 );
	new_lu2 = nmg_cut_loop( new_vu2, vu2 );
	ref_data->lu1->orientation = OT_SAME;
	ref_data->lu2->orientation = OT_SAME;
	new_lu1->orientation = OT_SAME;
	new_lu2->orientation = OT_SAME;
	ref_data->lu1->lumate_p->orientation = OT_SAME;
	ref_data->lu2->lumate_p->orientation = OT_SAME;
	new_lu1->lumate_p->orientation = OT_SAME;
	new_lu2->lumate_p->orientation = OT_SAME;

	if( debug )
		bu_log( "\tmoving to (%g %g %g)\n", V3ARGS( hit_pt ) );
	nmg_vertex_gv( new_vu->v_p, hit_pt );

	if( fd_plot )
	{
		struct loopuse *lu;
		struct vertex_g *vg;

		fprintf( fd_plot, "xxxxx\n" );
		fprintf( fd_plot, "vdraw params color 0000ff\n" );
		for( BU_LIST_FOR( lu, loopuse, &ref_data->fu1->lu_hd ) )
		{
			eu = BU_LIST_LAST( edgeuse, &lu->down_hd );
			vg = eu->vu_p->v_p->vg_p;
			fprintf( fd_plot, "vdraw write 0 %g %g %g\n", V3ARGS( vg->coord ) );
			
			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			{
				vg = eu->vu_p->v_p->vg_p;
				fprintf( fd_plot, "vdraw write 1 %g %g %g\n", V3ARGS( vg->coord ) );
			}
		}

		for( BU_LIST_FOR( lu, loopuse, &ref_data->fu2->lu_hd ) )
		{
			eu = BU_LIST_LAST( edgeuse, &lu->down_hd );
			vg = eu->vu_p->v_p->vg_p;
			fprintf( fd_plot, "vdraw write 0 %g %g %g\n", V3ARGS( vg->coord ) );
			
			for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
			{
				vg = eu->vu_p->v_p->vg_p;
				fprintf( fd_plot, "vdraw write 1 %g %g %g\n", V3ARGS( vg->coord ) );
			}
		}
	}

	(void) nmg_split_loops_into_faces( ref_data->fu1 , &tol );
	(void) nmg_split_loops_into_faces( ref_data->fu2 , &tol );

	for( BU_LIST_FOR( vu, vertexuse, &new_vu->v_p->vu_hd ) )
	{
		struct faceuse *fu;

		if( *vu->up.magic_p != NMG_EDGEUSE_MAGIC )
			continue;

		eu = vu->up.eu_p;

		fu = nmg_find_fu_of_vu ( vu );
		if( !fu )
			continue;

		if( fu->orientation != OT_SAME )
			continue;

		bu_ptbl_ins( ref_data->new_edges, (long *)eu->e_p );

		nmg_calc_face_g( fu, &tol );
	}

	return( 1 );
}

static int
refine_edges( s )
struct shell *s;
{
	struct bu_ptbl edges_1;
	struct bu_ptbl edges_2;
	struct bu_ptbl *cur, *next, *tmp;
	struct application ap;
	struct model *m;
	int breaks=0;
	int loop_count=0;
	int i;

	NMG_CK_SHELL( s );

	m = nmg_find_model( &s->l.magic );

	nmg_edge_tabulate( &edges_1, &s->l.magic );
	cur = &edges_1;
	bu_ptbl_init( &edges_2, 64, "edges_2" );
	next = &edges_2;

	bzero( &ap, sizeof( struct application ) );
	ap.a_rt_i = rtip;
	ap.a_miss = miss;
	ap.a_overlap = a_overlap;
	ap.a_onehit = 0;
	ap.a_hit = refine_hit;

	if( debug )
	{
		nmg_rebound( m, &tol );
		mk_nmg( fd_out, "break.0", m );
	}

	breaks = 1;
	while( breaks && loop_count < 6 )
	{
		struct refine_data ref_data;

		breaks = 0;
		ref_data.new_edges = next;

		for( i=0 ; i<BU_PTBL_END( cur ) ; i++ )
		{
			vect_t v1, v2, v3, v4;
			fastf_t alpha, cosa, sina;
			vect_t ave_norm;
			struct edge *e;
			struct vertex_g *vga, *vgb;
			struct edgeuse *eu1, *eu2;
			struct loopuse *lu1, *lu2;
			struct faceuse *fu1, *fu2;
			point_t mid_pt;
			vect_t norm1, norm2;

			e = (struct edge *)BU_PTBL_GET( cur, i );
			NMG_CK_EDGE( e );

			eu1 = e->eu_p;
			if( *eu1->up.magic_p != NMG_LOOPUSE_MAGIC )
				continue;
			lu1 = eu1->up.lu_p;
			if( *lu1->up.magic_p != NMG_FACEUSE_MAGIC )
				continue;
			fu1 = lu1->up.fu_p;

			if( fu1->orientation != OT_SAME )
			{
				eu1 = eu1->eumate_p;
				if( *eu1->up.magic_p != NMG_LOOPUSE_MAGIC )
					continue;
				lu1 = eu1->up.lu_p;
				if( *lu1->up.magic_p != NMG_FACEUSE_MAGIC )
					continue;
				fu1 = lu1->up.fu_p;
			}

			if( fu1->orientation != OT_SAME )
				bu_bomb( "Cannot find OT_SAME side of face\n" );

			eu2 = eu1->radial_p;
			if( *eu2->up.magic_p != NMG_LOOPUSE_MAGIC )
				continue;
			lu2 = eu2->up.lu_p;
			if( *lu2->up.magic_p != NMG_FACEUSE_MAGIC )
				continue;
			fu2 = lu2->up.fu_p;

			if( fu2->orientation != OT_SAME )
			{
				eu2 = eu2->eumate_p;
				if( *eu2->up.magic_p != NMG_LOOPUSE_MAGIC )
					continue;
				lu2 = eu2->up.lu_p;
				if( *lu2->up.magic_p != NMG_FACEUSE_MAGIC )
					continue;
				fu2 = lu2->up.fu_p;
			}

			if( fu2->orientation != OT_SAME )
				bu_bomb( "Cannot find OT_SAME side of face\n" );

			NMG_GET_FU_NORMAL( norm1, fu1 );
			NMG_GET_FU_NORMAL( norm2, fu2 );

			vga = eu1->vu_p->v_p->vg_p;
			vgb = eu1->eumate_p->vu_p->v_p->vg_p;

			VSUB2( v3, vgb->coord, vga->coord )
			VUNITIZE( v3 )
			VMOVE( v2, norm1 )
			VCROSS( v1, v2, v3)
			VUNITIZE( v1 )
			VCROSS( v4, v3, norm2 )
			alpha = atan2( VDOT( v4, v2 ), VDOT( v4, v1 ) );
			if( alpha < 0.0 )
				alpha += bn_twopi;
			alpha = alpha / 2.0;
			cosa = cos( alpha );
			sina = sin( alpha );
			VBLEND2( ave_norm, cosa, v1, sina, v2 )

			VBLEND2( mid_pt, 0.5, vga->coord, 0.5, vgb->coord );
			VJOIN1( ap.a_ray.r_pt, mid_pt, (2.0*cell_size), ave_norm );
			VREVERSE( ap.a_ray.r_dir, ave_norm );
			ref_data.fu1 = fu1;
			ref_data.fu2 = fu2;
			ref_data.lu1 = lu1;
			ref_data.lu2 = lu2;
			ref_data.eu1 = eu1;
			ref_data.eu2 = eu2;
			ref_data.mid_pt = mid_pt;
			ap.a_uptr = (genptr_t)&ref_data;

			if( debug )
			{
				bu_log( "norm1=(%g %g %g), norm2=(%g %g %g), alpha=%g, ave_norm=(%g %g %g)\n",
					V3ARGS( norm1 ), V3ARGS( norm2 ), alpha, V3ARGS( ave_norm ) );
				bu_log( "\tmid_pt = (%g %g %g)\n", V3ARGS( mid_pt ) );
				bu_log( "\tray_pt=(%g %g %g), dir=(%g %g %g)\n", V3ARGS( ap.a_ray.r_pt ), V3ARGS( ap.a_ray.r_dir ) );
			}
			breaks += rt_shootray( &ap );
		}

		bu_log( "\tBroke %d edges\n", breaks );
		if( debug )
		{
			char name[16];

			sprintf( name, "break.%d", loop_count );
			nmg_rebound( m, &tol );
			mk_nmg( fd_out, name, m );
		}

		bu_ptbl_reset( cur );
		tmp = cur;
		cur = next;
		next = tmp;

		++loop_count;
	}

	bu_ptbl_free( cur );
	bu_ptbl_free( next );

	return( breaks );
}

static void
Make_shell()
{
	int i,j;
	int x_index, y_index, z_index;
	int cell_no[4];
	int status;
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;
	struct local_part *lpart[4];

	bu_log( "Building initial shell...\n" );

	m = nmg_mm();
	r = nmg_mrsv( m );
	s = BU_LIST_FIRST( shell, &r->s_hd );

	switch (cur_dir )
	{
		case X:
			for( y_index=1 ; y_index < cell_count[Y] ; y_index++ )
			{
				for( z_index=0 ; z_index < cell_count[Z] - 1 ; z_index++ )
				{
					cell_no[0] = YZ_CELL(y_index,z_index);
					cell_no[1] = YZ_CELL(y_index-1,z_index);
					cell_no[2] = YZ_CELL(y_index-1,z_index+1);
					cell_no[3] = YZ_CELL(y_index,z_index+1);

					for( i=0 ; i<4 ; i++ )
					{
						if( BU_LIST_IS_EMPTY( &yz_parts[cell_no[i]].l ) )
							lpart[i] = (struct local_part *)NULL;
						else
							lpart[i] = BU_LIST_FIRST( local_part, &yz_parts[cell_no[i]].l );
					}

					status = 0;
					for( i=0 ; i<4 ; i++ )
					{
						if( lpart[i] && lpart[i]->is_void == NO )
							status += vert_ids[i];
					}

					if( debug > 3 )
					{
						bu_log( "Making faces for y_index=%d, z_index=%d\n", y_index, z_index );
						for( i=0 ; i<4 ; i++ )
						{
							bu_log( "part #%d:\n", i );
							bu_log( "\tray start is (%g %g %g)\n", V3ARGS( yz_rays[cell_no[i]].r_pt ) );
							pr_part( lpart[i] );
						}
					}

					Make_simple_faces( s, status, lpart );
				}
			}
			break;
		case Y:
			for( x_index=0 ; x_index < cell_count[X] - 1 ; x_index++ )
			{
				for( z_index=0 ; z_index < cell_count[Z] - 1 ; z_index++ )
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
			break;
		case Z:
			for( x_index=1 ; x_index < cell_count[X] ; x_index++ )
			{
				for( y_index=0 ; y_index < cell_count[Y] - 1 ; y_index++ )
				{
					cell_no[0] = XY_CELL(x_index,y_index);
					cell_no[1] = XY_CELL(x_index-1,y_index);
					cell_no[2] = XY_CELL(x_index-1,y_index+1);
					cell_no[3] = XY_CELL(x_index,y_index+1);

					for( i=0 ; i<4 ; i++ )
					{
						if( BU_LIST_IS_EMPTY( &xy_parts[cell_no[i]].l ) )
							lpart[i] = (struct local_part *)NULL;
						else
							lpart[i] = BU_LIST_FIRST( local_part, &xy_parts[cell_no[i]].l );
					}

					status = 0;
					for( i=0 ; i<4 ; i++ )
					{
						if( lpart[i] && lpart[i]->is_void == NO )
							status += vert_ids[i];
					}

					if( debug > 3 )
					{
						bu_log( "Making faces for x_index=%d, y_index=%d\n", x_index, y_index );
						for( i=0 ; i<4 ; i++ )
						{
							bu_log( "part #%d:\n", i );
							bu_log( "\tray start is (%g %g %g)\n", V3ARGS( xy_rays[cell_no[i]].r_pt ) );
							pr_part( lpart[i] );
						}
					}

					Make_simple_faces( s, status, lpart );
				}
			}
			break;
	}

	nmg_rebound( m, &tol );

	if( do_extra_rays )
		shrink_wrap( s );

	if( edge_tol > 0.0 )
	{
		if( debug )
		{
			nmg_rebound( m, &tol );
			mk_nmg( fd_out, "break.0", m );
		}

		bu_log( "Shooting rays at edge mid points...\n" );
		refine_edges( s );
	}

	if( decimation_tol > 0.0 )
	{
		bu_log( "%d edges eliminated by decimation to tolerance of %gmm\n",
			nmg_edge_collapse( m, &tol, decimation_tol, min_angle ), decimation_tol );
	}

	if( do_extra_rays || edge_tol > 0.0 || decimation_tol > 0.0 )
		nmg_rebound( m, &tol );

	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		if( fu->orientation != OT_SAME )
			continue;
		face_count++;
	}

	bu_log( "Bounding box of output: (%g %g %g) <-> (%g %g %g)\n", V3ARGS( r->ra_p->min_pt ), V3ARGS( r->ra_p->max_pt ) );
	bu_log( "%d facets\n", face_count );

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
	fastf_t part_len;

	first_pp = PartHeadp->pt_forw;
	last_pp = PartHeadp->pt_back;

	part_len = last_pp->pt_outhit->hit_dist - first_pp->pt_inhit->hit_dist;
	if( NEAR_ZERO( part_len, tol.dist ) )
		return( 0 );

	lpart = GET_PART;
	lpart->in = GET_END;
	lpart->out = GET_END;
	lpart->is_void = NO;
	lpart->in->v = (struct vertex *)NULL;

	switch( cur_dir )
	{
		case X:
			VSET( lpart->in->pt, 
				yz_rays[ap->a_user].r_pt[X] + first_pp->pt_inhit->hit_dist,
				yz_rays[ap->a_user].r_pt[Y],
				yz_rays[ap->a_user].r_pt[Z] );
			lpart->out->v = (struct vertex *)NULL;
			VSET( lpart->out->pt, 
				yz_rays[ap->a_user].r_pt[X] + last_pp->pt_outhit->hit_dist,
				yz_rays[ap->a_user].r_pt[Y],
				yz_rays[ap->a_user].r_pt[Z] );
			BU_LIST_INSERT( &yz_parts[ap->a_user].l, &lpart->l )
			break;
		case Y:
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
			break;
		case Z:
			VSET( lpart->in->pt, 
				xy_rays[ap->a_user].r_pt[X],
				xy_rays[ap->a_user].r_pt[Y],
				xy_rays[ap->a_user].r_pt[Z] + first_pp->pt_inhit->hit_dist );
			lpart->out->v = (struct vertex *)NULL;
			VSET( lpart->out->pt, 
				xy_rays[ap->a_user].r_pt[X],
				xy_rays[ap->a_user].r_pt[Y],
				xy_rays[ap->a_user].r_pt[Z] + last_pp->pt_outhit->hit_dist );
			BU_LIST_INSERT( &xy_parts[ap->a_user].l, &lpart->l )
			break;
	}
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
	fastf_t bb_area[3];

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

	BU_LIST_INIT( &add_rpp_head );
	BU_LIST_INIT( &subtract_rpp_head );

	/* Get command line arguments. */
	while( (c=getopt( argc, argv, "i:a:s:nR:g:o:d:p:X:")) != EOF)
	{
		switch( c )
		{
			case 'i':	/* set initial ray direction */
					switch( *optarg )
					{
						case 'x':
						case 'X':
							initial_ray_dir = X;
							break;
						case 'y':
						case 'Y':
							initial_ray_dir = Y;
							break;
						case 'z':
						case 'Z':
							initial_ray_dir = Z;
							break;
						default:
							bu_log( "Illegal ray direction (%c), must be X, Y, or Z!!!\n", *optarg );
							exit( 1 );
					}
					break;
			case 'a':	/* add an rpp for refining */
				{
					char *ptr;
					struct refine_rpp *rpp;
					int bad_opt=0;

					ptr = optarg;

					rpp = ( struct refine_rpp *)bu_malloc( sizeof( struct refine_rpp ), "add refine rpp" );
					ptr = strtok( optarg, token_seps );
					if( !ptr )
					{
						bu_log( "Bad -a option '%s'\n", optarg );
						bu_free( (char *) rpp, "rpp" );
						break;
					}

					rpp->tolerance = atof( ptr );
					if( rpp->tolerance <= 0.0 )
					{
						bu_log( "Illegal tolerance in -a option (%s)\n", ptr );
						bu_free( (char *) rpp, "rpp" );
						break;
					}

					/* get minimum */
					for( i=0 ; i<3 ; i++ )
					{
						ptr = strtok( (char *)NULL, token_seps );
						if( !ptr )
						{
							bu_log( "Unexpected end of option args for -a!!!\n" );
							bad_opt = 1;
							break;
						}
						rpp->min[i] = atof( ptr );
					}
					if( bad_opt )
					{
						bu_free( (char *) rpp, "rpp" );
						break;
					}

					/* get maximum */
					for( i=0 ; i<3 ; i++ )
					{
						ptr = strtok( (char *)NULL, token_seps );
						if( !ptr )
						{
							bu_log( "Unexpected end of option args for -a!!!\n" );
							bad_opt = 1;
							break;
						}
						rpp->max[i] = atof( ptr );
					}
					if( bad_opt )
					{
						bu_free( (char *) rpp, "rpp" );
						break;
					}

					BU_LIST_APPEND( &add_rpp_head, &rpp->h );
				}
				break;
			case 'n':	/* don't do extra raytracing to refine shape */
				do_extra_rays = 0;
				break;
			case 'R':	/* do edge breaking */
				edge_tol = atof( optarg );
				break;
			case 'p':	/* plot edge breaking */
				plotfile = optarg;
				break;
			case 'd':	/* debug level */
				debug = atoi( optarg );
				break;
			case 'g':	/* cell size */
				cell_size = atof( optarg );
				cell_size_sq = cell_size * cell_size;
				break;
			case 'o':	/* BRL-CAD output file */
				output_file = optarg;
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
	else
		bu_bomb( "Output file must be specified!!!\n" );

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

	if( initial_ray_dir == -1 )
	{
		bb_area[X] = (rtip->mdl_max[Y] - rtip->mdl_min[Y]) *
			     (rtip->mdl_max[Z] - rtip->mdl_min[Z]);

		bb_area[Y] = (rtip->mdl_max[X] - rtip->mdl_min[X]) *
			     (rtip->mdl_max[Z] - rtip->mdl_min[Z]);

		bb_area[Z] = (rtip->mdl_max[Y] - rtip->mdl_min[Y]) *
			     (rtip->mdl_max[X] - rtip->mdl_min[X]);

		cur_dir = X;
		if( bb_area[Y] > bb_area[cur_dir] )
			cur_dir = Y;
		if( bb_area[Z] > bb_area[cur_dir] )
			cur_dir = Z;
	}
	else
		cur_dir = initial_ray_dir;

	cell_count[X] = (int)((rtip->mdl_max[X] - rtip->mdl_min[X])/cell_size) + 3;
	cell_count[Y] = (int)((rtip->mdl_max[Y] - rtip->mdl_min[Y])/cell_size) + 3;
	cell_count[Z] = (int)((rtip->mdl_max[Z] - rtip->mdl_min[Z])/cell_size) + 3;
	bu_log( "cell size is %gmm\n\t%d cells in X-direction\n\t%d cells in Y-direction\n\t%d cells in Z-direction\n",
		cell_size, cell_count[X], cell_count[Y], cell_count[Z] );

	x_start = rtip->mdl_min[X] - ((double)cell_count[X] * cell_size - (rtip->mdl_max[X] - rtip->mdl_min[X]))/2.0;
	y_start = rtip->mdl_min[Y] - ((double)cell_count[Y] * cell_size - (rtip->mdl_max[Y] - rtip->mdl_min[Y]))/2.0;
	z_start = rtip->mdl_min[Z] - ((double)cell_count[Z] * cell_size - (rtip->mdl_max[Z] - rtip->mdl_min[Z]))/2.0;

	xz_parts = (struct local_part *)bu_calloc( cell_count[X] * cell_count[Z], sizeof( struct local_part ), "xz_parts" );
	yz_parts = (struct local_part *)bu_calloc( cell_count[Y] * cell_count[Z], sizeof( struct local_part ), "yz_parts" );
	xy_parts = (struct local_part *)bu_calloc( cell_count[X] * cell_count[Y], sizeof( struct local_part ), "xy_parts" );

	xy_rays = (struct xray *)bu_calloc( cell_count[X] * cell_count[Y], sizeof( struct xray ), "xy_rays" );
	xz_rays = (struct xray *)bu_calloc( cell_count[X] * cell_count[Z], sizeof( struct xray ), "xz_rays" );
	yz_rays = (struct xray *)bu_calloc( cell_count[Y] * cell_count[Z], sizeof( struct xray ), "yz_rays" );

	for( i=0 ; i<cell_count[Y] ; i++ )
	{
		for( j=0 ; j<cell_count[Z] ; j++ )
		{
			VSET( yz_rays[YZ_CELL(i,j)].r_dir, 1.0, 0.0, 0.0 )
			VSET( yz_rays[YZ_CELL(i,j)].r_pt,
				rtip->mdl_min[X] - cell_size,
				y_start + (fastf_t)i * cell_size,
				z_start + (fastf_t)j * cell_size )
			yz_parts[YZ_CELL(i,j)].is_void = UNKNOWN;
			yz_parts[YZ_CELL(i,j)].in_coord = -MAX_FASTF;
			yz_parts[YZ_CELL(i,j)].out_coord = -MAX_FASTF;
			yz_parts[YZ_CELL(i,j)].in = (struct end_pt *)NULL;
			yz_parts[YZ_CELL(i,j)].out = (struct end_pt *)NULL;
			BU_LIST_INIT( &yz_parts[YZ_CELL(i,j)].l );
		}
	}

	for( i=0 ; i<cell_count[X] ; i++ )
	{
		for( j=0 ; j<cell_count[Y] ; j++ )
		{
			VSET( xy_rays[XY_CELL(i,j)].r_dir, 0.0, 0.0, 1.0 )
			VSET( xy_rays[XY_CELL(i,j)].r_pt,
				x_start + (fastf_t)i * cell_size,
				y_start + (fastf_t)j * cell_size,
				rtip->mdl_min[Z] - cell_size )
			xy_parts[XY_CELL(i,j)].is_void = UNKNOWN;
			xy_parts[XY_CELL(i,j)].in_coord = -MAX_FASTF;
			xy_parts[XY_CELL(i,j)].out_coord = -MAX_FASTF;
			xy_parts[XY_CELL(i,j)].in = (struct end_pt *)NULL;
			xy_parts[XY_CELL(i,j)].out = (struct end_pt *)NULL;
			BU_LIST_INIT( &xy_parts[XY_CELL(i,j)].l );
		}

		for( j=0 ; j<cell_count[Z] ; j++ )
		{
			VSET( xz_rays[XZ_CELL(i,j)].r_dir, 0.0, 1.0, 0.0 )
			VSET( xz_rays[XZ_CELL(i,j)].r_pt,
				x_start + (fastf_t)i * cell_size,
				rtip->mdl_min[Y] - cell_size,
				z_start + (fastf_t)j * cell_size )
			xz_parts[XZ_CELL(i,j)].is_void = UNKNOWN;
			xz_parts[XZ_CELL(i,j)].in_coord = -MAX_FASTF;
			xz_parts[XZ_CELL(i,j)].out_coord = -MAX_FASTF;
			xz_parts[XZ_CELL(i,j)].in = (struct end_pt *)NULL;
			xz_parts[XZ_CELL(i,j)].out = (struct end_pt *)NULL;
			BU_LIST_INIT( &xz_parts[XZ_CELL(i,j)].l );
		}
	}

	bu_log( "Bounding box of BRL-CAD model: (%g %g %g) <-> (%g %g %g)\n", V3ARGS( rtip->mdl_min ), V3ARGS( rtip->mdl_max ) );
	bu_log( "Shooting rays in %c-direction...\n", dir_ch[cur_dir] );
	switch( cur_dir )
	{
		case X:
			for( i=0 ; i<cell_count[Y] ; i++ )
			{
				for( j=0 ; j<cell_count[Z] ; j++ )
				{
					ap.a_x = i;
					ap.a_y = j;
					ap.a_user = YZ_CELL(i,j);
					VMOVE( ap.a_ray.r_pt, yz_rays[YZ_CELL(i,j)].r_pt )
					VMOVE( ap.a_ray.r_dir, yz_rays[YZ_CELL(i,j)].r_dir )
					(void)rt_shootray( &ap );
				}
			}
			break;
		case Y:
			for( i=0 ; i<cell_count[X] ; i++ )
			{
				for( j=0 ; j<cell_count[Z] ; j++ )
				{
					ap.a_x = i;
					ap.a_y = j;
					ap.a_user = XZ_CELL(i,j);
					VMOVE( ap.a_ray.r_pt, xz_rays[XZ_CELL(i,j)].r_pt )
					VMOVE( ap.a_ray.r_dir, xz_rays[XZ_CELL(i,j)].r_dir )
					(void)rt_shootray( &ap );
				}
			}
			break;
		case Z:
			for( i=0 ; i<cell_count[X] ; i++ )
			{
				for( j=0 ; j<cell_count[Y] ; j++ )
				{
					ap.a_x = i;
					ap.a_y = j;
					ap.a_user = XY_CELL(i,j);
					VMOVE( ap.a_ray.r_pt, xy_rays[XY_CELL(i,j)].r_pt )
					VMOVE( ap.a_ray.r_dir, xy_rays[XY_CELL(i,j)].r_dir )
					(void)rt_shootray( &ap );
				}
			}
			break;
	}

	Make_shell();
}
