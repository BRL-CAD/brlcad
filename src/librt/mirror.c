/*                        M I R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mirror.c
 *
 * Routine(s) to mirror objects.
 *
 * Authors -
 *   Christopher Sean Morrison
 *   Michael John Muuss
 *
 * Source -
 *   BRL-CAD Open Source
 */

#include "common.h"

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"
#include "bn.h"
#include "bu.h"
#include "vmath.h"
#include "nurb.h"


/**
 * mirror object about some axis
 *
 * returns a directory pointer to the new mirrored object
 **/
struct directory *
rt_mirror(struct db_i *dbip, const char *from, const char *to, int axis, struct resource *resp)
{
    register struct directory *dp;
    struct rt_db_internal internal;

    register int i, j;
    int	id;
    mat_t mirmat;
    mat_t temp;

    if (!resp) {
	resp=&rt_uniresource;
    }

    /* look up the object being mirrored */
    if ((dp = db_lookup(dbip, from, LOOKUP_NOISY)) == DIR_NULL) {
	return DIR_NULL;
    }

    /* make sure object mirroring to does not already exist */
    if (db_lookup(dbip, to, LOOKUP_QUIET) != DIR_NULL) {
	bu_log("%s already exists\n", to);
	return DIR_NULL;
    }

    /* validate axis */
    switch (axis) {
	case X:
	case Y:
	case Z:
	    break;
	default:
	    bu_log("Unknown axis specified [%d]\n", axis);
	    return DIR_NULL;
    }

    /* get object being mirrored */
    id = rt_db_get_internal(&internal, dp, dbip, NULL, resp);
    if( id < 0 )  {
	bu_log("ERROR: Unable to load solid [%s]\n", from);
	return DIR_NULL;
    }
    RT_CK_DB_INTERNAL( &internal );

    /* Build mirror transform matrix, for those who need it. */
    MAT_IDN( mirmat );
    mirmat[axis*5] = -1.0;

    switch (id) {
	case ID_TOR:
	    {
		struct rt_tor_internal *tor;

		tor = (struct rt_tor_internal *)internal.idb_ptr;
		RT_TOR_CK_MAGIC( tor );

		tor->v[axis] *= -1.0;
		tor->h[axis] *= -1.0;

		break;
	    }
	case ID_TGC:
	case ID_REC:
	    {
		struct rt_tgc_internal *tgc;

		tgc = (struct rt_tgc_internal *)internal.idb_ptr;
		RT_TGC_CK_MAGIC( tgc );

		tgc->v[axis] *= -1.0;
		tgc->h[axis] *= -1.0;
		tgc->a[axis] *= -1.0;
		tgc->b[axis] *= -1.0;
		tgc->c[axis] *= -1.0;
		tgc->d[axis] *= -1.0;

		break;
	    }
	case ID_ELL:
	case ID_SPH:
	    {
		struct rt_ell_internal *ell;

		ell = (struct rt_ell_internal *)internal.idb_ptr;
		RT_ELL_CK_MAGIC( ell );

		ell->v[axis] *= -1.0;
		ell->a[axis] *= -1.0;
		ell->b[axis] *= -1.0;
		ell->c[axis] *= -1.0;

		break;
	    }
	case ID_ARB8:
	    {
		struct rt_arb_internal *arb;

		arb = (struct rt_arb_internal *)internal.idb_ptr;
		RT_ARB_CK_MAGIC( arb );

		for( i=0 ; i<8 ; i++ )
		    arb->pt[i][axis] *= -1.0;
		break;
	    }
	case ID_HALF:
	    {
		struct rt_half_internal *haf;

		haf = (struct rt_half_internal *)internal.idb_ptr;
		RT_HALF_CK_MAGIC( haf );

		haf->eqn[axis] *= -1.0;

		break;
	    }
	case ID_GRIP:
	    {
		struct rt_grip_internal *grp;

		grp = (struct rt_grip_internal *)internal.idb_ptr;
		RT_GRIP_CK_MAGIC( grp );

		grp->center[axis] *= -1.0;
		grp->normal[axis] *= -1.0;

		break;
	    }
	case ID_POLY:
	    {
		struct rt_pg_internal *pg;
		fastf_t *verts;
		fastf_t *norms;

		pg = (struct rt_pg_internal *)internal.idb_ptr;
		RT_PG_CK_MAGIC( pg );

		verts = (fastf_t *)bu_calloc( pg->max_npts*3, sizeof( fastf_t ), "rt_mirror: verts" );
		norms = (fastf_t *)bu_calloc( pg->max_npts*3, sizeof( fastf_t ), "rt_mirror: norms" );

		for( i=0 ; i<pg->npoly ; i++ ) {
		    int last;

		    last = (pg->poly[i].npts - 1)*3;
		    /* mirror coords and temporarily store in reverse order */
		    for( j=0 ; j<pg->poly[i].npts*3 ; j += 3 ) {
			pg->poly[i].verts[j+axis] *= -1.0;
			VMOVE( &verts[last-j], &pg->poly[i].verts[j] );
			pg->poly[i].norms[j+axis] *= -1.0;
			VMOVE( &norms[last-j], &pg->poly[i].norms[j] );
		    }

		    /* write back mirrored and reversed face loop */
		    for( j=0 ; j<pg->poly[i].npts*3 ; j += 3 ) {
			VMOVE( &pg->poly[i].norms[j], &norms[j] );
			VMOVE( &pg->poly[i].verts[j], &verts[j] );
		    }
		}

		bu_free( (char *)verts, "rt_mirror: verts" );
		bu_free( (char *)norms, "rt_mirror: norms" );

		break;
	    }
	case ID_BSPLINE:
	    {
		struct rt_nurb_internal *nurb;

		nurb = (struct rt_nurb_internal *)internal.idb_ptr;
		RT_NURB_CK_MAGIC( nurb );

		for( i=0 ; i<nurb->nsrf ; i++ ) {
		    fastf_t *ptr;
		    int tmp;
		    int orig_size[2];
		    int ncoords;
		    int m;
		    int l;

		    /* swap knot vetcors between u and v */
		    ptr = nurb->srfs[i]->u.knots;
		    tmp = nurb->srfs[i]->u.k_size;

		    nurb->srfs[i]->u.knots = nurb->srfs[i]->v.knots;
		    nurb->srfs[i]->u.k_size = nurb->srfs[i]->v.k_size;
		    nurb->srfs[i]->v.knots = ptr;
		    nurb->srfs[i]->v.k_size = tmp;

		    /* swap order */
		    tmp = nurb->srfs[i]->order[0];
		    nurb->srfs[i]->order[0] = nurb->srfs[i]->order[1];
		    nurb->srfs[i]->order[1] = tmp;

		    /* swap mesh size */
		    orig_size[0] = nurb->srfs[i]->s_size[0];
		    orig_size[1] = nurb->srfs[i]->s_size[1];

		    nurb->srfs[i]->s_size[0] = orig_size[1];
		    nurb->srfs[i]->s_size[1] = orig_size[0];

		    /* allocat memory for a new control mesh */
		    ncoords = RT_NURB_EXTRACT_COORDS( nurb->srfs[i]->pt_type );
		    ptr = (fastf_t *)bu_calloc( orig_size[0]*orig_size[1]*ncoords, sizeof( fastf_t ), "rt_mirror: ctl mesh ptr" );

		    /* mirror each control point */
		    for( j=0 ; j<orig_size[0]*orig_size[1] ; j++ ) {
			nurb->srfs[i]->ctl_points[j*ncoords+axis] *= -1.0;
		    }

		    /* copy mirrored control points into new mesh
		     * while swaping u and v */
		    m = 0;
		    for( j=0 ; j<orig_size[0] ; j++ ) {
			for( l=0 ; l<orig_size[1] ; l++ ) {
			    VMOVEN( &ptr[(l*orig_size[0]+j)*ncoords], &nurb->srfs[i]->ctl_points[m*ncoords], ncoords );
			    m++;
			}
		    }

		    /* free old mesh */
		    bu_free( (char *)nurb->srfs[i]->ctl_points , "rt_mirror: ctl points" );

		    /* put new mesh in place */
		    nurb->srfs[i]->ctl_points = ptr;
		}

		break;
	    }
	case ID_ARBN:
	    {
		struct rt_arbn_internal *arbn;

		arbn = (struct rt_arbn_internal *)internal.idb_ptr;
		RT_ARBN_CK_MAGIC( arbn );

		for( i=0 ; i<arbn->neqn ; i++ ) {
		    arbn->eqn[i][axis] *= -1.0;
		}

		break;
	    }
	case ID_PIPE:
	    {
		struct rt_pipe_internal *pipe;
		struct wdb_pipept *ps;

		pipe = (struct rt_pipe_internal *)internal.idb_ptr;
		RT_PIPE_CK_MAGIC( pipe );

		for( BU_LIST_FOR( ps, wdb_pipept, &pipe->pipe_segs_head ) ) {
		    ps->pp_coord[axis] *= -1.0;
		}

		break;
	    }
	case ID_PARTICLE:
	    {
		struct rt_part_internal *part;

		part = (struct rt_part_internal *)internal.idb_ptr;
		RT_PART_CK_MAGIC( part );

		part->part_V[axis] *= -1.0;
		part->part_H[axis] *= -1.0;

		break;
	    }
	case ID_RPC:
	    {
		struct rt_rpc_internal *rpc;

		rpc = (struct rt_rpc_internal *)internal.idb_ptr;
		RT_RPC_CK_MAGIC( rpc );

		rpc->rpc_V[axis] *= -1.0;
		rpc->rpc_H[axis] *= -1.0;
		rpc->rpc_B[axis] *= -1.0;

		break;
	    }
	case ID_RHC:
	    {
		struct rt_rhc_internal *rhc;

		rhc = (struct rt_rhc_internal *)internal.idb_ptr;
		RT_RHC_CK_MAGIC( rhc );

		rhc->rhc_V[axis] *= -1.0;
		rhc->rhc_H[axis] *= -1.0;
		rhc->rhc_B[axis] *= -1.0;

		break;
	    }
	case ID_EPA:
	    {
		struct rt_epa_internal *epa;

		epa = (struct rt_epa_internal *)internal.idb_ptr;
		RT_EPA_CK_MAGIC( epa );

		epa->epa_V[axis] *= -1.0;
		epa->epa_H[axis] *= -1.0;
		epa->epa_Au[axis] *= -1.0;

		break;
	    }
	case ID_ETO:
	    {
		struct rt_eto_internal *eto;

		eto = (struct rt_eto_internal *)internal.idb_ptr;
		RT_ETO_CK_MAGIC( eto );

		eto->eto_V[axis] *= -1.0;
		eto->eto_N[axis] *= -1.0;
		eto->eto_C[axis] *= -1.0;

		break;
	    }
	case ID_NMG:
	    {
		struct model *m;
		struct nmgregion *r;
		struct shell *s;
		struct bu_ptbl table;
		struct vertex *v;

		m = (struct model *)internal.idb_ptr;
		NMG_CK_MODEL( m );

		/* move every vertex */
		nmg_vertex_tabulate( &table, &m->magic );
		for( i=0 ; i<BU_PTBL_END( &table ) ; i++ ) {
		    v = (struct vertex *)BU_PTBL_GET( &table, i );
		    NMG_CK_VERTEX( v );

		    v->vg_p->coord[axis] *= -1.0;
		}

		bu_ptbl_reset( &table );

		nmg_face_tabulate( &table, &m->magic );
		for( i=0 ; i<BU_PTBL_END( &table ) ; i++ ) {
		    struct face *f;

		    f = (struct face *)BU_PTBL_GET( &table, i );
		    NMG_CK_FACE( f );

		    if( !f->g.magic_p )
			continue;

		    if( *f->g.magic_p != NMG_FACE_G_PLANE_MAGIC ) {
			bu_log("Sorry, can only mirror NMG solids with planar faces\n");
			bu_log("Error [%s] has \n", from);
			bu_ptbl_free( &table );
			rt_db_free_internal( &internal, resp );
			return DIR_NULL;
		    }
		}

		for( BU_LIST_FOR( r, nmgregion, &m->r_hd ) ) {
		    for( BU_LIST_FOR( s, shell, &r->s_hd ) ) {
			nmg_invert_shell(s);
		    }
		}


		for( i=0 ; i<BU_PTBL_END( &table ) ; i++ ) {
		    struct face *f;
		    struct faceuse *fu;

		    f = (struct face *)BU_PTBL_GET( &table, i );
		    NMG_CK_FACE( f );

		    fu = f->fu_p;
		    if( fu->orientation != OT_SAME ) {
			fu = fu->fumate_p;
		    }
		    if( fu->orientation != OT_SAME ) {
			bu_log("Error with orientation of the NMG faces for [%s]\n", from);
			bu_ptbl_free( &table );
			rt_db_free_internal( &internal, resp );
			return DIR_NULL;
		    }

		    if( nmg_calc_face_g( fu ) ) {
			bu_log("Error calculating the NMG faces for [%s]\n", from);
			bu_ptbl_free( &table );
			rt_db_free_internal( &internal, resp );
			return DIR_NULL;
		    }
		}

		bu_ptbl_free( &table );
		nmg_rebound( m, &(dbip->dbi_wdbp->wdb_tol) );

		break;
	    }
	case ID_ARS:
	    {
		struct rt_ars_internal *ars;
		fastf_t *tmp_curve;

		ars = (struct rt_ars_internal *)internal.idb_ptr;
		RT_ARS_CK_MAGIC( ars );

		/* mirror each vertex */
		for( i=0 ; i<ars->ncurves ; i++ ) {
		    for( j=0 ; j<ars->pts_per_curve ; j++ ) {
			ars->curves[i][j*3+axis] *= -1.0;
		    }
		}

		/* now reverse order of vertices in each curve */
		tmp_curve = (fastf_t *)bu_calloc( 3*ars->pts_per_curve, sizeof( fastf_t ), "rt_mirror: tmp_curve" );
		for( i=0 ; i<ars->ncurves ; i++ ) {
		    /* reverse vertex order */
		    for( j=0 ; j<ars->pts_per_curve ; j++ ) {
			VMOVE( &tmp_curve[(ars->pts_per_curve-j-1)*3], &ars->curves[i][j*3] );
		    }

		    /* now copy back */
		    bcopy( tmp_curve, ars->curves[i], ars->pts_per_curve*3*sizeof( fastf_t ) );
		}

		bu_free( (char *)tmp_curve, "rt_mirror: tmp_curve" );

		break;
	    }
	case ID_EBM:
	    {
		struct rt_ebm_internal *ebm;

		ebm = (struct rt_ebm_internal *)internal.idb_ptr;
		RT_EBM_CK_MAGIC( ebm );

		bn_mat_mul( temp, mirmat, ebm->mat );
		MAT_COPY( ebm->mat, temp );

		break;
	    }
	case ID_DSP:
	    {
		struct rt_dsp_internal *dsp;

		dsp = (struct rt_dsp_internal *)internal.idb_ptr;
		RT_DSP_CK_MAGIC( dsp );

		bn_mat_mul( temp, mirmat, dsp->dsp_mtos);
		MAT_COPY( dsp->dsp_mtos, temp);

		break;
	    }
	case ID_VOL:
	    {
		struct rt_vol_internal *vol;

		vol = (struct rt_vol_internal *)internal.idb_ptr;
		RT_VOL_CK_MAGIC( vol );

		bn_mat_mul( temp, mirmat, vol->mat );
		MAT_COPY( vol->mat, temp );

		break;
	    }
	case ID_SUPERELL:
	    {
		struct rt_superell_internal *superell;

		superell = (struct rt_superell_internal *)internal.idb_ptr;
		RT_SUPERELL_CK_MAGIC( superell );

		superell->v[axis] *= -1.0;
		superell->a[axis] *= -1.0;
		superell->b[axis] *= -1.0;
		superell->c[axis] *= -1.0;
		superell->n = 1.0;
		superell->e = 1.0;

		break;
	    }
	case ID_COMBINATION:
	    {
		struct rt_comb_internal	*comb;

		comb = (struct rt_comb_internal *)internal.idb_ptr;
		RT_CK_COMB(comb);

		if( comb->tree ) {
		    db_tree_mul_dbleaf( comb->tree, mirmat );
		}
		break;
	    }
	default:
	    {
		rt_db_free_internal( &internal, resp );
		bu_log("Unknown or unsupported object type (id==%d)\n", id);
		bu_log("ERROR: cannot mirror object [%s]\n", from);
		return DIR_NULL;
	    }
    }

    /* save the mirrored object to disk */
    if( (dp = db_diradd( dbip, to, -1L, 0, dp->d_flags, (genptr_t)&internal.idb_type)) == DIR_NULL )  {
	bu_log("ERROR: Unable to add [%s] to the database directory", to);
	return DIR_NULL;
    }
    if( rt_db_put_internal( dp, dbip, &internal, resp ) < 0 )  {
	bu_log("ERROR: Unable to store [%s] to the database", to);
	return DIR_NULL;
    }

    return dp;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
