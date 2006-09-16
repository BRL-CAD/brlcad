/*                     N M G _ V I S I T . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup nmg */
/*@{*/
/** @file nmg_visit.c
 *  A generalized, object-oriented subroutine family to
 *  visit all the data structures "below" a given structure.
 *
 *  The caller provides a pointer to the structure to start at,
 *  a table of "handlers" for each kind of strucuture,
 *  and a generic pointer for private state which will be sent along
 *  to the user's handlers.
 *  For non-leaf structures, there are two handlers, one called
 *  before any recursion starts, and the other called when
 *  recursion is finished.  Either or both may be omitted.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"


#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"


/**
 *			N M G _ V I S I T _ V E R T E X
 */
void
nmg_visit_vertex(struct vertex *v, const struct nmg_visit_handlers *htab, genptr_t state)


        			       		/* Handler's private state */
{
	NMG_CK_VERTEX(v);

	if(htab->vis_vertex) htab->vis_vertex( (long *)v, state, 0 );

	if(htab->vis_vertex_g && v->vg_p)
		htab->vis_vertex_g( (long *)v->vg_p, state, 0 );
}

/**
 *			N M G _ V I S I T _ V E R T E X U S E
 */
void
nmg_visit_vertexuse(struct vertexuse *vu, const struct nmg_visit_handlers *htab, genptr_t state)


        			       		/* Handler's private state */
{
	NMG_CK_VERTEXUSE(vu);

	if(htab->bef_vertexuse) htab->bef_vertexuse( (long *)vu, state, 0 );

	nmg_visit_vertex( vu->v_p, htab, state );

	if(htab->vis_vertexuse_a && vu->a.magic_p)
		htab->vis_vertexuse_a( vu->a.magic_p, state, 0 );

	if(htab->aft_vertexuse) htab->aft_vertexuse( (long *)vu, state, 1 );
}

/**
 *			N M G _ V I S I T _ E D G E
 */
void
nmg_visit_edge(struct edge *e, const struct nmg_visit_handlers *htab, genptr_t state)


        			       		/* Handler's private state */
{
	NMG_CK_EDGE( e );

	if(htab->vis_edge) htab->vis_edge( (long *)e, state, 0 );
}

/**
 *			N M G _ V I S I T _ E D G E U S E
 */
void
nmg_visit_edgeuse(struct edgeuse *eu, const struct nmg_visit_handlers *htab, genptr_t state)


        			       		/* Handler's private state */
{
	NMG_CK_EDGEUSE(eu);

	if(htab->bef_edgeuse) htab->bef_edgeuse( (long *)eu, state, 0 );

	nmg_visit_vertexuse( eu->vu_p, htab, state );
	nmg_visit_edge( eu->e_p, htab, state );

	if(htab->vis_edge_g && eu->g.magic_p)
		htab->vis_edge_g( eu->g.magic_p, state, 0 );

	if(htab->aft_edgeuse) htab->aft_edgeuse( (long *)eu, state, 1 );
}

/**
 *			N M G _ V I S I T _ L O O P
 */
void
nmg_visit_loop(struct loop *l, const struct nmg_visit_handlers *htab, genptr_t state)


        			       		/* Handler's private state */
{
	NMG_CK_LOOP(l);

	if(htab->vis_loop) htab->vis_loop( (long *)l, state, 0 );

	if(htab->vis_loop_g && l->lg_p)
		htab->vis_loop_g( (long *)l->lg_p, state, 0 );
}

/**
 *			N M G _ V I S I T _ L O O P U S E
 */
void
nmg_visit_loopuse(struct loopuse *lu, const struct nmg_visit_handlers *htab, genptr_t state)


        			       		/* Handler's private state */
{
	NMG_CK_LOOPUSE( lu );

	if(htab->bef_loopuse) htab->bef_loopuse( (long *)lu, state, 0 );

	if( BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC )  {
		struct vertexuse	*vu;
		vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
		nmg_visit_vertexuse( vu, htab, state );
	} else {
		struct edgeuse		*eu;
		for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			nmg_visit_edgeuse( eu, htab, state );
		}
	}
	nmg_visit_loop( lu->l_p, htab, state );

	if(htab->aft_loopuse) htab->aft_loopuse( (long *)lu, state, 1 );
}

/**
 *			N M G _ V I S I T _ F A C E
 */
void
nmg_visit_face(struct face *f, const struct nmg_visit_handlers *htab, genptr_t state)


        			       		/* Handler's private state */
{

	if(htab->vis_face) htab->vis_face( (long *)f, state, 0 );

	if(htab->vis_face_g && f->g.plane_p)
		htab->vis_face_g( (long *)f->g.plane_p, state, 0 );
}

/**
 *			N M G _ V I S I T _ F A C E U S E
 */
void
nmg_visit_faceuse(struct faceuse *fu, const struct nmg_visit_handlers *htab, genptr_t state)


        			       		/* Handler's private state */
{
	struct loopuse	*lu;

	NMG_CK_FACEUSE(fu);

	if(htab->bef_faceuse) htab->bef_faceuse( (long *)fu, state, 0 );

	for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		nmg_visit_loopuse( lu, htab, state );
	}

	nmg_visit_face( fu->f_p, htab, state );

	if(htab->aft_faceuse) htab->aft_faceuse( (long *)fu, state, 1 );
}

/**
 *			N M G _ V I S I T _ S H E L L
 */
void
nmg_visit_shell(struct shell *s, const struct nmg_visit_handlers *htab, genptr_t state)


        			       		/* Handler's private state */
{
	struct faceuse	*fu;
	struct loopuse	*lu;
	struct edgeuse	*eu;

	NMG_CK_SHELL(s);

	if(htab->bef_shell) htab->bef_shell( (long *)s, state, 0 );

	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		nmg_visit_faceuse( fu, htab, state );
	}
	for( BU_LIST_FOR( lu, loopuse, &s->lu_hd ) )  {
		nmg_visit_loopuse( lu, htab, state );
	}
	for( BU_LIST_FOR( eu, edgeuse, &s->eu_hd ) )  {
		nmg_visit_edgeuse( eu, htab, state );
	}
	if( s->vu_p )  nmg_visit_vertexuse( s->vu_p, htab, state );
	if(htab->vis_shell_a && s->sa_p)
		htab->vis_shell_a( (long *)s->sa_p, state, 0 );

	if(htab->aft_shell) htab->aft_shell( (long *)s, state, 1 );
}

/**
 *			N M G _ V I S I T _ R E G I O N
 */
void
nmg_visit_region(struct nmgregion *r, const struct nmg_visit_handlers *htab, genptr_t state)


        			       		/* Handler's private state */
{
	struct shell		*s;

	NMG_CK_REGION(r);

	if(htab->bef_region) htab->bef_region( (long *)r, state, 0 );

	for( BU_LIST_FOR( s, shell, &r->s_hd ) )  {
		nmg_visit_shell( s, htab, state );
	}
	if(htab->vis_region_a && r->ra_p)
		htab->vis_region_a( (long *)r->ra_p, state, 0 );

	if(htab->aft_region) htab->aft_region( (long *)r, state, 1 );
}
/**
 *			N M G _ V I S I T _ M O D E L
 */
void
nmg_visit_model(struct model *model, const struct nmg_visit_handlers *htab, genptr_t state)


        			       		/* Handler's private state */
{
	struct nmgregion *r;

	NMG_CK_MODEL(model);

	if(htab->bef_model) htab->bef_model( (long *)model, state, 0 );

	for( BU_LIST_FOR( r, nmgregion, &model->r_hd ) )  {
		nmg_visit_region( r, htab, state );
	}

	if(htab->aft_model) htab->aft_model( (long *)model, state, 1 );
}

/**
 *			N M G _ V I S I T
 */
void
nmg_visit(const long int *magicp, const struct nmg_visit_handlers *htab, genptr_t state)
        			       		/* Handler's private state */
{
	switch( *magicp )  {
	default:
		bu_log("nmg_visit() Can't visit %s directly\n", bu_identify_magic( *magicp ));
		rt_bomb("nmg_visit()\n");
		/* NOTREACHED */
	case NMG_MODEL_MAGIC:
		nmg_visit_model( (struct model *)magicp, htab, state );
		break;
	case NMG_REGION_MAGIC:
		nmg_visit_region( (struct nmgregion *)magicp, htab, state );
		break;
	case NMG_SHELL_MAGIC:
		nmg_visit_shell( (struct shell *)magicp, htab, state );
		break;
	case NMG_FACEUSE_MAGIC:
		nmg_visit_faceuse( (struct faceuse *)magicp, htab, state );
		break;
	case NMG_LOOPUSE_MAGIC:
		nmg_visit_loopuse( (struct loopuse *)magicp, htab, state );
		break;
	case NMG_EDGEUSE_MAGIC:
		nmg_visit_edgeuse( (struct edgeuse *)magicp, htab, state );
		break;
	case NMG_VERTEXUSE_MAGIC:
		nmg_visit_vertexuse( (struct vertexuse *)magicp, htab, state );
		break;
	}
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
