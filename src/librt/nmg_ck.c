/*                        N M G _ C K . C
 * BRL-CAD
 *
 * Copyright (C) 1993-2005 United States Government as represented by
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

/** \addtogroup nmg */

/*@{*/
/** @file nmg_ck.c
 *  Validators and consistency checkers for NMG data structures.
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
 *	John R. Anderson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/*@}*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"


#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"


/************************************************************************
 *									*
 *			Validator Routines				*
 *									*
 ************************************************************************/

/**
 *			N M G _ V V G
 *
 *  Verify vertex geometry
 */
void
nmg_vvg(const struct vertex_g *vg)
{
	NMG_CK_VERTEX_G(vg);
}

/**
 *			N M G _ V V E R T E X
 *
 *  Verify a vertex
 */
void
nmg_vvertex(const struct vertex *v, const struct vertexuse *vup)
{
	struct vertexuse *vu;
	int vup_is_in_list = 0;

	NMG_CK_VERTEX(v);

	for( BU_LIST_FOR( vu, vertexuse, &v->vu_hd ) )  {
		NMG_CK_VERTEXUSE(vu);
		if (vu->v_p != v)
			rt_bomb("nmg_vvertex() a vertexuse in my list doesn't share my vertex\n");
		if (vu == vup)
			vup_is_in_list = 1;
	}
	if (v->vg_p) nmg_vvg(v->vg_p);
	if ( ! vup_is_in_list )
		rt_bomb("nmg_vvertex() vup not found in list of vertexuses\n");
}

/* Verify vertex attributes */
void
nmg_vvua(const long int *vua)
{
	NMG_CK_VERTEXUSE_A_EITHER(vua);
}

/**
 *			N M G _ V V U
 *
 *  Verify vertexuse
 */
void
nmg_vvu(const struct vertexuse *vu, const long int *up_magic_p)
{
	long	magic;

	NMG_CK_VERTEXUSE(vu);
	if (vu->up.magic_p != up_magic_p)  {
		bu_log("nmg_vvu() up is %s, s/b %s\n",
			bu_identify_magic( *vu->up.magic_p ),
			bu_identify_magic( *up_magic_p ) );
		rt_bomb("nmg_vvu() vertexuse denies parent\n");
	}

	if (!vu->l.forw)
		rt_bomb("nmg_vvu() vertexuse has null forw pointer\n");

	magic = BU_LIST_FIRST_MAGIC( &vu->l );
	if( magic != NMG_VERTEXUSE_MAGIC && magic != BU_LIST_HEAD_MAGIC )
		rt_bomb("nmg_vvu() vertexuse forw is bad vertexuse\n");

	if (BU_LIST_PNEXT_PLAST(vertexuse,vu) != vu )
		rt_bomb("nmg_vvu() vertexuse not back of next vertexuse\n");

	nmg_vvertex(vu->v_p, vu);

	if (vu->a.magic_p) nmg_vvua(vu->a.magic_p);
}

/* Verify edge geometry */
void
nmg_veg(const long int *eg)
{
	struct bu_list	*eu2;

	NMG_CK_EDGE_G_EITHER(eg);
	switch( *eg )  {
	case NMG_EDGE_G_LSEG_MAGIC:
		bu_ck_list_magic( &((struct edge_g_lseg *)eg)->eu_hd2,
			"nmg_veg() edge_g_lseg eu_hd2 list",
			NMG_EDGEUSE2_MAGIC );
		break;
	case NMG_EDGE_G_CNURB_MAGIC:
		bu_ck_list_magic( &((struct edge_g_cnurb *)eg)->eu_hd2,
			"nmg_veg() edge_g_cnurb eu_hd2 list",
			NMG_EDGEUSE2_MAGIC );
		break;
	}

	/* Ensure that all edgeuses on the edge_g_* list point to me */
	for( BU_LIST_FOR( eu2, bu_list, &((struct edge_g_lseg *)eg)->eu_hd2 ) )  {
		struct edgeuse	*eu;

		if( eu2 == NULL)
		{
			 rt_bomb("nmg_veg null eu2\n");
		}
		eu = BU_LIST_MAIN_PTR( edgeuse, eu2, l2 );
		NMG_CK_EDGEUSE(eu);
		if( eu->g.magic_p == eg )  continue;
		bu_log("eg=x%x, eu=x%x, eu->g=x%x\n", eg, eu, eu->g.magic_p);
		bu_log("nmg_veg() edgeuse is on wrong eu_hd2 list for eu->g\n");
	}
}

/**
 *			N M G _ V E D G E
 *
 *  Verify edge
 */
void
nmg_vedge(const struct edge *e, const struct edgeuse *eup)
{
	const struct edgeuse *eu;
	int is_use = 0;		/* flag: eup is in edge's use list */

	NMG_CK_EDGE(e);
	NMG_CK_EDGEUSE(eup);
	NMG_CK_VERTEXUSE(eup->vu_p);
	NMG_CK_VERTEX(eup->vu_p->v_p);
	NMG_CK_EDGEUSE(eup->eumate_p);
	NMG_CK_VERTEXUSE(eup->eumate_p->vu_p);
	NMG_CK_VERTEX(eup->eumate_p->vu_p->v_p);

	if (!e->eu_p) rt_bomb("nmg_vedge() edge has null edgeuse pointer\n");

	NMG_CK_EDGEUSE( e->eu_p );

	eu = eup;
	do {
		NMG_CK_EDGEUSE(eu);
		NMG_CK_EDGEUSE(eu->eumate_p);
		if (eu == eup || eu->eumate_p == eup)
			is_use = 1;

		NMG_CK_VERTEXUSE(eu->vu_p);
		NMG_CK_VERTEX(eu->vu_p->v_p);
		if (eu->vu_p->v_p == eup->vu_p->v_p) {
			if (eu->eumate_p->vu_p->v_p != eup->eumate_p->vu_p->v_p)
			{
				bu_log("nmg_vedge() edgeuse mate does not have correct vertex\n");
				bu_log( "(eu=x%x, eu->vu_p->v_p=x%x, eu->eumate_p->vu_p->v_p=x%x)\n",
					eu, eu->vu_p->v_p, eu->eumate_p->vu_p->v_p);
				bu_log( "(eup=x%x, eup->vu_p->v_p=x%x, eup->eumate_p->vu_p->v_p=x%x)\n",
					eup, eup->vu_p->v_p, eup->eumate_p->vu_p->v_p);
				rt_bomb("nmg_vedge() edgeuse mate does not have correct vertex\n");
			}
		} else if (eu->vu_p->v_p == eup->eumate_p->vu_p->v_p) {
			if (eu->eumate_p->vu_p->v_p != eup->vu_p->v_p)
			{
				bu_log("nmg_vedge() edgeuse does not have correct vertex\n" );
				bu_log( "(eu=x%x, eu->vu_p->v_p=x%x, eu->eumate_p->vu_p->v_p=x%x)\n",
					eu, eu->vu_p->v_p, eu->eumate_p->vu_p->v_p);
				bu_log( "(eup=x%x, eup->vu_p->v_p=x%x, eup->eumate_p->vu_p->v_p=x%x)\n",
					eup, eup->vu_p->v_p, eup->eumate_p->vu_p->v_p);
				rt_bomb("nmg_vedge() edgeuse does not have correct vertex\n");
			}
		} else
		{
			bu_log("nmg_vedge() edgeuse does not share vertex endpoint\n" );
			bu_log( "(eu=x%x, eu->vu_p->v_p=x%x, eu->eumate_p->vu_p->v_p=x%x)\n",
				eu, eu->vu_p->v_p, eu->eumate_p->vu_p->v_p);
			bu_log( "(eup=x%x, eup->vu_p->v_p=x%x, eup->eumate_p->vu_p->v_p=x%x)\n",
				eup, eup->vu_p->v_p, eup->eumate_p->vu_p->v_p);
			rt_bomb("nmg_vedge() edgeuse does not share vertex endpoint\n");
		}

		eu = eu->eumate_p->radial_p;
	} while (eu != eup);

	if (!is_use)
		rt_bomb("nmg_vedge() Cannot get from edge to parent edgeuse\n");
}

/**
 *			N M G _ V E U
 *
 *  Verify edgeuse list.
 */
void
nmg_veu(const struct bu_list *hp, const long int *up_magic_p)
{
	struct edgeuse	*eu;
	struct edgeuse	*eunext;
	struct edgeuse	*eulast;
	long		up_magic;

	bu_ck_list_magic( hp, "nmg_veu() edegeuse list head", NMG_EDGEUSE_MAGIC );

	up_magic = *up_magic_p;
	switch( up_magic )  {
	case NMG_SHELL_MAGIC:
	case NMG_LOOPUSE_MAGIC:
		break;
	default:
		rt_bomb("nmg_veu() bad up_magic_p\n");
	}
	for( BU_LIST_FOR( eu, edgeuse, hp ) )  {
		NMG_CK_EDGEUSE(eu);

		if (eu->up.magic_p != up_magic_p)
			rt_bomb("nmg_veu() edgeuse denies parentage\n");

		if (!eu->l.forw)
			rt_bomb("nmg_veu() edgeuse has Null \"forw\" pointer\n");
		eunext = BU_LIST_PNEXT_CIRC( edgeuse, eu );
		eulast = BU_LIST_PPREV_CIRC(edgeuse, &eu->l);
		if (eunext->l.magic != NMG_EDGEUSE_MAGIC)
			rt_bomb("nmg_veu() edgeuse forw is bad edgeuse\n");
		if (eulast->l.magic != NMG_EDGEUSE_MAGIC)
			rt_bomb("nmg_veu() edgeuse back is bad edgeuse\n");
		NMG_CK_EDGEUSE(eunext);
		NMG_CK_EDGEUSE(eulast);

		/* Check that forw->back is us */
		if (BU_LIST_PPREV_CIRC(edgeuse,eunext) != eu )  {
		    if (eunext->l.back)
			rt_bomb("nmg_veu() next edgeuse has back that points elsewhere\n");
		    rt_bomb("nmg_veu() next edgeuse has NULL back\n");
		}

		/*
		 *  For edgeuses in loops, ensure that vertices are shared.
		 *  This does not apply to wire edgeuses in the shell.
		 */
		if ( up_magic == NMG_LOOPUSE_MAGIC &&
		     eu->vu_p->v_p != eulast->eumate_p->vu_p->v_p) {
		     	bu_log("eu=x%x, e=x%x\n", eu, eu->e_p );
		     	bu_log("eulast=x%x, e=x%x\n", eulast, eulast->e_p);
		     	bu_log("	    eu: (%g, %g, %g) <--> (%g, %g, %g)\n",
		     		V3ARGS(eu->vu_p->v_p->vg_p->coord),
		     		V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord) );
		     	bu_log("	eulast: (%g, %g, %g) <--> (%g, %g, %g)\n",
		     		V3ARGS(eulast->vu_p->v_p->vg_p->coord),
		     		V3ARGS(eulast->eumate_p->vu_p->v_p->vg_p->coord) );
			bu_log("unshared vertex (mine) v=x%x: (%g, %g, %g)\n",
				eu->vu_p->v_p,
				V3ARGS(eu->vu_p->v_p->vg_p->coord) );
			bu_log("\t\t (last->eumate_p) v=x%x: (%g, %g, %g)\n",
				eulast->eumate_p->vu_p->v_p,
				V3ARGS(eulast->eumate_p->vu_p->v_p->vg_p->coord) );
		     	nmg_pr_lu_briefly(eu->up.lu_p, (char *)NULL);
		     	nmg_pr_lu_briefly(eu->up.lu_p->lumate_p, (char *)NULL);
			rt_bomb("nmg_veu() discontinuous edgeloop mine/last\n");
		}
		if ( up_magic == NMG_LOOPUSE_MAGIC &&
		     eunext->vu_p->v_p != eu->eumate_p->vu_p->v_p) {
		     	bu_log("eu=x%x, e=x%x\n", eu, eu->e_p );
		     	bu_log("eunext=x%x, e=x%x\n", eunext, eunext->e_p);
		     	bu_log("	    eu: (%g, %g, %g) <--> (%g, %g, %g)\n",
		     		V3ARGS(eu->vu_p->v_p->vg_p->coord),
		     		V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord) );
		     	bu_log("	eunext: (%g, %g, %g) <--> (%g, %g, %g)\n",
		     		V3ARGS(eunext->vu_p->v_p->vg_p->coord),
		     		V3ARGS(eunext->eumate_p->vu_p->v_p->vg_p->coord) );
			bu_log("unshared vertex (mate) v=x%x: (%g, %g, %g)\n",
				eu->eumate_p->vu_p->v_p,
				V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord) );
			bu_log("\t\t (next) v=x%x: (%g, %g, %g)\n",
				eunext->vu_p->v_p,
				V3ARGS(eunext->vu_p->v_p->vg_p->coord) );
		     	nmg_pr_lu_briefly(eu->up.lu_p, (char *)NULL);
		     	nmg_pr_lu_briefly(eu->up.lu_p->lumate_p, (char *)NULL);
			rt_bomb("nmg_veu() discontinuous edgeloop next/mate\n");
		}

		/* Check mate and radial */
		if (eu->eumate_p->l.magic != NMG_EDGEUSE_MAGIC)
			rt_bomb("nmg_veu() edgeuse mate is bad edgeuse\n");
		if (eu->eumate_p->eumate_p != eu)
			rt_bomb("nmg_veu() edgeuse mate spurns edgeuse\n");

		if (eu->radial_p->l.magic != NMG_EDGEUSE_MAGIC)
			rt_bomb("nmg_veu() edgeuse radial is bad edgeuse\n");
		if (eu->radial_p->radial_p != eu)
			rt_bomb("nmg_veu() edgeuse radial denies knowing edgeuse\n");

		nmg_vedge(eu->e_p, eu);

		if( eu->vu_p->v_p != eu->eumate_p->vu_p->v_p )
		{
			if( !eu->l2.forw )
				rt_bomb("nmg_veu() l2.forw is NULL\n");
			if( !eu->l2.back )
				rt_bomb("nmg_veu() l2.back is NULL\n");

			if( eu->g.magic_p != eu->eumate_p->g.magic_p )
				rt_bomb("nmg_veu() edgeuse and mate don't share geometry\n");
			if(eu->g.magic_p) nmg_veg(eu->g.magic_p);
		}
		
		switch (eu->orientation) {
		case OT_NONE	: break;
		case OT_SAME	: break;
		case OT_OPPOSITE: break;
		case OT_UNSPEC	: break;
		default		: rt_bomb("nmg_veu() unknown loopuse orintation\n");
					break;
		}

		nmg_vvu(eu->vu_p, &eu->l.magic);
	}
}

/**
 *			N M G _ V L G
 *
 *  Verify loop geometry
 */
void
nmg_vlg(const struct loop_g *lg)
{
	int i;
	
	NMG_CK_LOOP_G(lg);

	for (i=0 ; i < ELEMENTS_PER_PT ; ++i)
		if (lg->min_pt[i] > lg->max_pt[i])
			rt_bomb("nmg_vlg() loop geom min_pt greater than max_pt\n");
}

/**
 *			N M G _ V L O O P
 *
 *  Verify loop
 */
void
nmg_vloop(const struct loop *l, const struct loopuse *lup)
{

	NMG_CK_LOOP(l);
	NMG_CK_LOOPUSE(lup);

	if (!l->lu_p) rt_bomb("nmg_vloop() null loopuse pointer\n");

#if 0
	{
	struct loopuse *lu;
	for (lu=lup ; lu && lu != l->lu_p && lu->next != lup ; lu = lu->next);
	
	if (l->lu_p != lu)
		for (lu=lup->lumate_p ; lu && lu != l->lu_p && lu->next != lup->lumate_p ; lu = lu->next);

	if (l->lu_p != lu) rt_bomb("nmg_vloop() can't get to parent loopuse from loop\n");
	}
#endif

	if (l->lg_p) nmg_vlg(l->lg_p);
}

/**
 *			N M G _ V L U
 *
 *  Verify loopuse
 */
void
nmg_vlu(const struct bu_list *hp, const long int *up)
{
	struct loopuse *lu;

	for( BU_LIST_FOR( lu, loopuse, hp ) )  {
		NMG_CK_LOOPUSE(lu);

		if (lu->up.magic_p != up)  {
			bu_log("nmg_vlu() up is x%x, s/b x%x\n",
				lu->up.magic_p, up );
			rt_bomb("nmg_vlu() loopuse denies parentage\n");
		}

		if (!lu->l.forw)
			rt_bomb("nmg_vlu() loopuse has null forw pointer\n");
		if (BU_LIST_PNEXT_PLAST(loopuse,lu) != lu )
			rt_bomb("nmg_vlu() forw loopuse has back pointing somewhere else\n");

		if (!lu->lumate_p)
			rt_bomb("nmg_vlu() loopuse has null mate pointer\n");

		if (lu->lumate_p->l.magic != NMG_LOOPUSE_MAGIC)
			rt_bomb("nmg_vlu() loopuse mate is bad loopuse\n");

		if (lu->lumate_p->lumate_p != lu)
			rt_bomb("nmg_vlu() lumate spurns loopuse\n");

		switch (lu->orientation) {
		case OT_NONE	: break;
		case OT_SAME	: break;
		case OT_OPPOSITE	: break;
		case OT_UNSPEC	: break;
		case OT_BOOLPLACE:	break;
		default:
			bu_log("lu=x%x, orientation=%d\n", lu, lu->orientation);
			rt_bomb("nmg_vlu() unknown loopuse orintation\n");
			break;
		}
		if (lu->lumate_p->orientation != lu->orientation)
			rt_bomb("nmg_vlu() loopuse and mate have different orientation\n");

		if (!lu->l_p)
			rt_bomb("nmg_vlu() loopuse has Null loop pointer\n");
		nmg_vloop(lu->l_p, lu);

		if( BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC)
			nmg_veu( &lu->down_hd, &lu->l.magic);
		else if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC)
			nmg_vvu(BU_LIST_FIRST(vertexuse,&lu->down_hd), &lu->l.magic);
		else
			rt_bomb("nmg_vlu() bad down_hd magic\n");
	}
}

/**
 *			N M G _ V F G
 *
 *  Verify face geometry
 */
void
nmg_vfg(const struct face_g_plane *fg)
{
	NMG_CK_FACE_G_EITHER(fg);

	if( fg->magic == NMG_FACE_G_PLANE_MAGIC )
	{
		if (fg->N[X]==0.0 && fg->N[Y]==0.0 && fg->N[Z]==0.0 &&
		    fg->N[H]!=0.0) {
			bu_log("bad NMG plane equation %fX + %fY + %fZ = %f\n",
				fg->N[X], fg->N[Y], fg->N[Z], fg->N[H]);
			rt_bomb("nmg_vfg() Bad NMG geometry\n");
		    }
	}
	if( fg->magic == NMG_FACE_G_SNURB_MAGIC )
	{
		/* XXX Should the face's NURB be checked somehow?? */
	}
}

/**
 *			N M G _ V F A C E
 *
 *  Verify face
 */
void
nmg_vface(const struct face *f, const struct faceuse *fup)
{
	int		i;

	NMG_CK_FACE(f);
	NMG_CK_FACEUSE(fup);

	/* make sure we can get back to the parent faceuse from the face */
	if (!f->fu_p) rt_bomb("nmg_vface() null faceuse pointer\n");

#if 0
	for (fu = fup; fu && fu != f->fu_p && fu->forw != fup; fu = fu->forw);

	if (f->fu_p != fu) rt_bomb("nmg_vface() can't get to parent faceuse from face\n");
#endif

	for (i=0 ; i < ELEMENTS_PER_PT ; ++i)
		if (f->min_pt[i] >= f->max_pt[i]) {
			bu_log("nmg_vface() face min_pt[%d]:%g greater than max_pt[%d]:%g\n",
				i, f->min_pt[i], i, f->max_pt[i]);
			bu_log("min_pt(%g %g %g)  ", V3ARGS(f->min_pt));
			bu_log("max_pt(%g %g %g)\n", V3ARGS(f->max_pt));
			rt_bomb("Invalid NMG\n");
		}
	if (f->g.plane_p) nmg_vfg(f->g.plane_p);
}

/**
 *			N M G _ V F U
 *
 *	Validate a list of faceuses
 */
void
nmg_vfu(const struct bu_list *hp, const struct shell *s)
{
	struct faceuse *fu;

	NMG_CK_SHELL(s);

	for( BU_LIST_FOR( fu, faceuse, hp ) )  {
		NMG_CK_FACEUSE(fu);
		if (fu->s_p != s) {
			bu_log("faceuse claims shell parent (%8x) instead of (%8x)\n",
				fu->s_p, s);
			rt_bomb("nmg_vfu()\n");
		}

		if (!fu->l.forw) {
			rt_bomb("nmg_vfu() faceuse forw is NULL\n");
		} else if (fu->l.forw->back != (struct bu_list *)fu) {
			rt_bomb("nmg_vfu() faceuse->forw->back != faceuse\n");
		}

		if (!fu->fumate_p)
			rt_bomb("nmg_vfu() null faceuse fumate_p pointer\n");

		if (fu->fumate_p->l.magic != NMG_FACEUSE_MAGIC)
			rt_bomb("nmg_vfu() faceuse mate is bad faceuse ptr\n");

		if (fu->fumate_p->fumate_p != fu)
			rt_bomb("nmg_vfu() faceuse mate spurns faceuse!\n");

		switch (fu->orientation) {
		case OT_NONE	: break;
		case OT_SAME	: if (fu->fumate_p->orientation != OT_OPPOSITE)
					rt_bomb("nmg_vfu() faceuse of \"SAME\" orientation has mate that is not \"OPPOSITE\" orientation\n");
				break;
		case OT_OPPOSITE:  if (fu->fumate_p->orientation != OT_SAME)
					rt_bomb("nmg_vfu() faceuse of \"OPPOSITE\" orientation has mate that is not \"SAME\" orientation\n");
				break;
		case OT_UNSPEC	: break;
		default		: rt_bomb("nmg_vfu() unknown faceuse orintation\n"); break;
		}

		NMG_CK_FACE(fu->f_p);
		nmg_vface(fu->f_p, fu);
		
		nmg_vlu( &fu->lu_hd, &fu->l.magic);
	}
}


/**
 *			N M G _ V S H E L L
 *
 *	validate a list of shells and all elements under them
 */
void
nmg_vshell(const struct bu_list *hp, const struct nmgregion *r)
{
	struct shell *s;
	pointp_t lpt, hpt;

	NMG_CK_REGION(r);

	for( BU_LIST_FOR( s, shell, hp ) )  {
		NMG_CK_SHELL(s);
		if (s->r_p != r) {
			bu_log("shell's r_p (%8x) doesn't point to parent (%8x)\n",
				s->r_p, r);
			rt_bomb("nmg_vshell()\n");
		}

		if (!s->l.forw) {
			rt_bomb("nmg_vshell(): Shell's forw ptr is null\n");
		} else if (s->l.forw->back != (struct bu_list *)s) {
			bu_log("forw shell's back(%8x) is not me (%8x)\n",
				s->l.forw->back, s);
			rt_bomb("nmg_vshell()\n");
		}

		if (s->sa_p) {
			NMG_CK_SHELL_A(s->sa_p);
			/* we make sure that all values of min_pt
			 * are less than or equal to the values of max_pt
			 */
			lpt = s->sa_p->min_pt;
			hpt = s->sa_p->max_pt;
			if (lpt[0] > hpt[0] || lpt[1] > hpt[1] ||
			    lpt[2] > hpt[2]) {
				bu_log("Bnmg_vshell() ad min_pt/max_pt for shell(%8x)'s extent\n");
				bu_log("Min_pt %g %g %g\n", lpt[0], lpt[1],
					lpt[2]);
				bu_log("Max_pt %g %g %g\n", hpt[0], hpt[1],
					hpt[2]);
			}
		}

		/* now we check out the "children"
		 */

		if (s->vu_p) {
			if( BU_LIST_NON_EMPTY( &s->fu_hd ) ||
			    BU_LIST_NON_EMPTY( &s->lu_hd ) ||
			    BU_LIST_NON_EMPTY( &s->eu_hd ) )  {
				bu_log("shell (%8x) with vertexuse (%8x) has other children\n",
					s, s->vu_p);
				rt_bomb("nmg_vshell()\n");
			}
		}

		nmg_vfu( &s->fu_hd, s);
		nmg_vlu( &s->lu_hd, &s->l.magic);
		nmg_veu( &s->eu_hd, &s->l.magic);
	}
}



/**
 *			N M G _ V R E G I O N
 *
 *	validate a list of nmgregions and all elements under them
 */
void
nmg_vregion(const struct bu_list *hp, const struct model *m)
{
	struct nmgregion *r;

	for( BU_LIST_FOR( r, nmgregion, hp ) )  {
		NMG_CK_REGION(r);
		if (r->m_p != m) {
			bu_log("nmgregion pointer m_p %8x should be %8x\n",
				r->m_p, m);
			rt_bomb("nmg_vregion()\n");
		}
		if (r->ra_p) {
			NMG_CK_REGION_A(r->ra_p);
		}

		nmg_vshell( &r->s_hd, r);

		if( BU_LIST_PNEXT_PLAST(nmgregion, r) != r )  {
			rt_bomb("nmg_vregion() forw nmgregion's back is not me\n");
		}
	}
}

/**
 *			N M G _ V M O D E L
 *
 *	validate an NMG model and all elements in it.
 */
void
nmg_vmodel(const struct model *m)
{
	NMG_CK_MODEL(m);
	nmg_vregion( &m->r_hd, m);
}


/************************************************************************
 *									*
 *			Checking Routines				*
 *									*
 ************************************************************************/

/**
 *			N M G _ C K _ E
 */
void
nmg_ck_e(const struct edgeuse *eu, const struct edge *e, const char *str)
{
	char *errstr;
	struct edgeuse *eparent;
	errstr = bu_calloc(strlen(str)+128, 1, "nmg_ck_e error str");
	(void)sprintf(errstr, "%sedge %8lx\n", str, (unsigned long)e);
	
	NMG_CK_EDGE(e);
	NMG_CK_EDGEUSE(eu);

	eparent = e->eu_p;

	NMG_CK_EDGEUSE(eparent);
	NMG_CK_EDGEUSE(eparent->eumate_p);
	do {
		if (eparent == eu || eparent->eumate_p == eu) break;

		eparent = eparent->radial_p->eumate_p;
	} while (eparent != e->eu_p);

	if (eparent != eu && eparent->eumate_p != eu) rt_bomb(
		strcat(errstr, "nmg_ck_e() Edge denies edgeuse parentage\n"));

	bu_free(errstr, "nmg_ck_e error str");
}

/**
 *			N M G _ C K _ V U
 */
void
nmg_ck_vu(const long int *parent, const struct vertexuse *vu, const char *str)
{
	char *errstr;

	errstr = bu_calloc(strlen(str)+128, 1, "nmg_ck_vu error str");
	(void)sprintf(errstr, "%svertexuse %8lx\n", str, (unsigned long)vu);
	
	if (vu->up.magic_p != parent) rt_bomb(
		strcat(errstr, "nmg_ck_vu() Vertexuse denies parentage\n"));

	bu_free(errstr, "nmg_ck_vu error str");
}

/**
 *			N M G _ C K _ E U
 */
void
nmg_ck_eu(const long int *parent, const struct edgeuse *eu, const char *str)
{
	char *errstr;
	struct edgeuse *eur, *eu_next, *eu_last;	

	errstr = bu_calloc(strlen(str)+128, 1, "nmg_ck_eu error str");
	(void)sprintf(errstr, "%sedgeuse %8lx\n", str, (unsigned long)eu);

	NMG_CK_EDGEUSE(eu);

	if (eu->up.magic_p != parent) rt_bomb(
		strcat(errstr, "nmg_ck_eu() Edgeuse child denies parentage\n"));

	if (*eu->eumate_p->up.magic_p != *eu->up.magic_p) rt_bomb(
		strcat(errstr, "nmg_ck_eu() eumate has differnt kind of parent\n"));
	if (*eu->up.magic_p == NMG_SHELL_MAGIC) {
		if (eu->eumate_p->up.s_p != eu->up.s_p) rt_bomb(
			strcat(errstr, "nmg_ck_eu() eumate in different shell\n"));

		eur = eu->radial_p;
		while (eur && eur != eu && eur != eu->eumate_p)
			eur = eur->eumate_p->radial_p;

		if (!eur) rt_bomb(strcat(errstr,
			"nmg_ck_eu() Radial trip from eu ended in null pointer\n"));


	} else if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		if (eu->eumate_p->up.lu_p != eu->up.lu_p->lumate_p) rt_bomb(
			strcat(errstr, "nmg_ck_eu() eumate not in same loop\n"));

		eur = eu->radial_p;
		while (eur && eur != eu->eumate_p && eur != eu)
			eur = eur->eumate_p->radial_p;

		if (!eur) rt_bomb(
			strcat(errstr, "nmg_ck_eu() radial path leads to null ptr\n"));
		if (eur == eu) rt_bomb(
			strcat(errstr, "nmg_ck_eu() Never saw eumate\n"));

		eu_next = BU_LIST_PNEXT_CIRC(edgeuse, eu);
		if (eu_next->vu_p->v_p != eu->eumate_p->vu_p->v_p)
			rt_bomb("nmg_ck_eu: next and mate don't share vertex\n");

		eu_last = BU_LIST_PPREV_CIRC(edgeuse, eu);
		if (eu_last->eumate_p->vu_p->v_p != eu->vu_p->v_p)
			rt_bomb("nmg_ck_eu: edge and last-mate don't share vertex\n");

	} else {
		rt_bomb(strcat(errstr, "nmg_ck_eu() Bad edgeuse parent\n"));
	}

	NMG_CK_EDGE(eu->e_p);
	nmg_ck_e(eu, eu->e_p, errstr);

	NMG_CK_VERTEXUSE(eu->vu_p);
	nmg_ck_vu(&eu->l.magic, eu->vu_p, errstr);

	bu_free(errstr, "nmg_ck_eu error str");
}

/**
 *			N M G _ C K _ L G
 */
void
nmg_ck_lg(const struct loop *l, const struct loop_g *lg, const char *str)
{
	char *errstr;
	errstr = bu_calloc(strlen(str)+128, 1, "nmg_ck_lg error str");
	(void)sprintf(errstr, "%sloop_g %8lx\n", str, (unsigned long)lg);

	NMG_CK_LOOP_G(lg);
	NMG_CK_LOOP(l);

	bu_free(errstr, "nmg_ck_lg error str");
}

/**
 *			N M G _ C K _ L
 */
void
nmg_ck_l(const struct loopuse *lu, const struct loop *l, const char *str)
{
	char *errstr;
	errstr = bu_calloc(strlen(str)+128, 1, "nmg_ck_l error str");
	(void)sprintf(errstr, "%sloop %8lx\n", str, (unsigned long)l);

	NMG_CK_LOOP(l);
	NMG_CK_LOOPUSE(lu);

	if (l->lu_p != lu && l->lu_p->lumate_p != lu) rt_bomb(
		strcat(errstr, "nmg_ck_l() Cannot get from loop to loopuse\n"));

	if (l->lg_p) nmg_ck_lg(l, l->lg_p, errstr);

	bu_free(errstr, "");
}

/**
 *			N M G _ C K _ L U
 */
void
nmg_ck_lu(const long int *parent, const struct loopuse *lu, const char *str)
{
	struct edgeuse *eu;
	struct vertexuse *vu;
	char *errstr;
	int l;
	int edgeuse_num=0;
	long	magic1;

	errstr = bu_calloc(strlen(str)+128, 1, "nmg_ck_lu error str");
	(void)sprintf(errstr, "%sloopuse %8lx\n", str, (unsigned long)lu);
	
	NMG_CK_LOOPUSE(lu);

	if (lu->up.magic_p != parent) rt_bomb(
		strcat(errstr, "nmg_ck_lu() loopuse child denies parentage\n") );

	/* check the parent of lu and lumate WRT each other */
	NMG_CK_LOOPUSE(lu->lumate_p);
	if (*lu->lumate_p->up.magic_p != *lu->up.magic_p) rt_bomb(
		strcat(errstr,"nmg_ck_lu() loopuse mate has different kind of parent\n"));

	if (*lu->up.magic_p == NMG_SHELL_MAGIC) {
		if (lu->lumate_p->up.s_p != lu->up.s_p) rt_bomb(
			strcat(errstr, "nmg_ck_lu() Lumate not in same shell\n") );
	} else if (*lu->up.magic_p == NMG_FACEUSE_MAGIC) {
		if (lu->lumate_p->up.fu_p != lu->up.fu_p->fumate_p) rt_bomb(
			strcat(errstr, "nmg_ck_lu() lumate part of different face\n"));
	} else {
		rt_bomb(strcat(errstr, "nmg_ck_lu() Bad loopuse parent type\n"));
	}

	NMG_CK_LOOP(lu->l_p);
	nmg_ck_l(lu, lu->l_p, errstr);

	/* check the children of the loopuse */
	magic1 = BU_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC) {
		vu = BU_LIST_FIRST( vertexuse, &lu->down_hd );
		NMG_CK_VERTEXUSE(vu);
		nmg_ck_vu(&lu->l.magic, vu, errstr);
	} else if (magic1 == NMG_EDGEUSE_MAGIC) {
		l = strlen(errstr);
		for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			NMG_CK_EDGEUSE(eu);
			(void)sprintf(&errstr[l], "%sedgeuse #%d (%8lx)\n",
				errstr, edgeuse_num++, (unsigned long)eu);
			nmg_ck_eu(&lu->l.magic, eu, errstr);
		}
	} else {
		rt_bomb(strcat(errstr, "nmg_ck_lu() Bad loopuse down pointer\n") );
	}
	bu_free(errstr, "nmg_ck_lu error str");
}

/**
 *			N M G _ C K _ F G
 */
void
nmg_ck_fg(const struct face *f, const struct face_g_plane *fg, const char *str)
{
	char *errstr;
	errstr = bu_calloc(strlen(str)+128, 1, "nmg_ck_fg error str");
	(void)sprintf(errstr, "%sFace_g %8lx\n", str, (unsigned long)f);

	NMG_CK_FACE_G_PLANE(fg);
	if (fg->N[X]==0.0 && fg->N[Y]==0.0 && fg->N[Z]==0.0 && fg->N[H]!=0.0){
		(void)sprintf(&errstr[strlen(errstr)],
			"nmg_ck_fg() bad NMG plane equation %fX + %fY + %fZ = %f\n",
			fg->N[X], fg->N[Y], fg->N[Z], fg->N[H]);
	        rt_bomb(errstr);
	}

	bu_free(errstr, "nmg_ck_fg error str");
}

/** 
 *			N M G _ C K _ F
 */
void
nmg_ck_f(const struct faceuse *fu, const struct face *f, const char *str)
{
	char *errstr;
	errstr = bu_calloc(strlen(str)+128, 1, "nmg_ck_f error str");
	(void)sprintf(errstr, "%sFace %8lx\n", str, (unsigned long)f);

	NMG_CK_FACE(f);
	NMG_CK_FACEUSE(fu);
	NMG_CK_FACE_G_PLANE(f->g.plane_p);
	if (f->fu_p != fu && f->fu_p->fumate_p != fu) rt_bomb(
		strcat(errstr,"nmg_ck_f() Cannot get from face to \"parent faceuse\"\n"));

	if (f->g.plane_p) nmg_ck_fg(f, f->g.plane_p, errstr);

	bu_free(errstr, "nmg_ck_f error str");
}

/**
 *			N M G _ C K _ F U
 */
void
nmg_ck_fu(const struct shell *s, const struct faceuse *fu, const char *str)
{
	char *errstr;
	int l;
	int loop_number = 0;
	struct loopuse *lu;

	NMG_CK_FACEUSE(fu);
	NMG_CK_SHELL(s);

	errstr = bu_calloc(strlen(str)+128, 1, "nmg_ck_fu error str");
	(void)sprintf(errstr, "%sFaceuse %8lx\n", str, (unsigned long)fu);

	if (fu->s_p != s) rt_bomb(
		strcat(errstr, "nmg_ck_fu() faceuse child denies shell parentage\n") );

	if( BU_LIST_PNEXT_PLAST( faceuse, fu ) )
		rt_bomb( strcat(errstr, "nmg_ck_fu() Faceuse not lastward of next faceuse\n") );

	if( BU_LIST_PLAST_PNEXT( faceuse, fu ) )
		rt_bomb( strcat(errstr, "nmg_ck_fu() Faceuse not nextward from last faceuse\n") );

	NMG_CK_FACEUSE(fu->fumate_p);
	if (fu->fumate_p->fumate_p != fu) rt_bomb(
		strcat(errstr, "nmg_ck_fu() Faceuse not fumate of fumate\n") );

	if (fu->fumate_p->s_p != s) rt_bomb(
		strcat(errstr, "nmg_ck_fu() faceuse mates not in same shell\n") );

	nmg_ck_f(fu, fu->f_p, errstr);

	l = strlen(errstr);
	for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		(void)sprintf(&errstr[l] , "%sloopuse #%d (%8lx)\n", 
			errstr, loop_number++, (unsigned long)lu);
		nmg_ck_lu(&fu->l.magic, lu, errstr);
	}
	bu_free(errstr, "nmg_ck_fu error str");
}

/**		N M G _ C K _ E G _ V E R T S
 *
 * Check if vertices from edgeuses using this edge geometry
 * actually lie on the edge geomatry.
 *
 * "eg" must be LSEG
 * returns number of vertices not on edge line
 */

int
nmg_ck_eg_verts(const struct edge_g_lseg *eg, const struct bn_tol *tol)
{
	struct bu_list *eu2;
	vect_t e_dir;
	int count=0;

	NMG_CK_EDGE_G_LSEG( eg );
	BN_CK_TOL( tol );

	VMOVE( e_dir , eg->e_dir );
	VUNITIZE( e_dir );

	for( BU_LIST_FOR( eu2 , bu_list , &eg->eu_hd2 ) )
	{
		struct edgeuse *eu;
		struct vertex *v1,*v2;
		struct vertex_g *vg1,*vg2;
		vect_t pt_to_vert;
		vect_t eg_to_vert;

		eu = BU_LIST_MAIN_PTR( edgeuse, eu2, l2 );

		NMG_CK_EDGEUSE( eu );

		v1 = eu->vu_p->v_p;
		NMG_CK_VERTEX( v1 );
		vg1 = v1->vg_p;
		NMG_CK_VERTEX_G( vg1 );

		v2 = eu->eumate_p->vu_p->v_p;
		NMG_CK_VERTEX( v2 );
		vg2 = v2->vg_p;
		NMG_CK_VERTEX_G( vg2 );

		VSUB2( pt_to_vert , vg1->coord , eg->e_pt );
		VJOIN1( eg_to_vert , pt_to_vert , -VDOT( e_dir , pt_to_vert ) , e_dir );
		if( MAGSQ( eg_to_vert ) > tol->dist_sq )
		{
			count++;
			bu_log( "vertex ( %g %g %g ) on eu to ( %g %g %g )\n", V3ARGS( vg1->coord ),
					V3ARGS( vg2->coord ) );
			bu_log( "\tnot on edge geometry: pt=( %g %g %g ), dir=( %g %g %g )\n",
					V3ARGS( eg->e_pt ), V3ARGS( eg->e_dir ) );
		}
	}

	return( count );
}

/**	N M G _ C K _ G E O M E T R Y
 *
 * Check that vertices actually lie on geometry for
 * faces and edges
 *
 * returns number of vertices that do not lie on geometry
 */
int
nmg_ck_geometry(const struct model *m, const struct bn_tol *tol)
{
	struct bu_ptbl g_tbl;
	int i;
	int count=0;

	NMG_CK_MODEL( m );
	BN_CK_TOL( tol );

	bu_ptbl_init( &g_tbl , 64, " &g_tbl ");

	nmg_edge_g_tabulate( &g_tbl , &m->magic );

	for( i=0 ; i<BU_PTBL_END( &g_tbl ) ; i++ )
	{
		long *ep;
		struct edge_g_lseg *eg;

		ep = BU_PTBL_GET( &g_tbl , i );
		switch( *ep )
		{
			case NMG_EDGE_G_LSEG_MAGIC:
				eg = (struct edge_g_lseg *)ep;
				NMG_CK_EDGE_G_LSEG( eg );
				count += nmg_ck_eg_verts( eg , tol );
				break;
			case NMG_EDGE_G_CNURB_MAGIC:
				/* XXX any checking for vertices on CNURB geometry?? */
				break;
		}
	}

	bu_ptbl_reset( &g_tbl );

	nmg_face_tabulate( &g_tbl , &m->magic );

	for( i=0 ; i<BU_PTBL_END( &g_tbl ) ; i++ )
	{
		struct face *f;

		f = (struct face *)BU_PTBL_GET( &g_tbl , i );
		NMG_CK_FACE( f );

		count += nmg_ck_fg_verts( f->fu_p , f , tol );
	}

	bu_ptbl_free( &g_tbl );

	return( count );
}

/**
 *			N M G _ C K _ F A C E _ W O R T H L E S S _ E D G E S
 *
 *  Search for null ("worthless") edges in a face.
 *  Such edges are legitimate to have, but can be troublesome
 *  for the boolean routines.
 *
 *  Often used to see if breaking an edge at a given
 *  vertex results in a null edge being created.
 */
int
nmg_ck_face_worthless_edges(const struct faceuse *fu)
{
	const struct loopuse	*lu;

	for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		struct edgeuse	*eu;

		NMG_CK_LOOPUSE(lu);
		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
			continue;
		for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			struct edgeuse		*neu;
			neu = BU_LIST_PNEXT_CIRC( edgeuse, eu );
			if( eu == neu )
				rt_bomb("nmg_ck_face_worthless_edges() lu has only one edge?\n");
			if( eu->vu_p == neu->vu_p )
				rt_bomb("nmg_ck_face_worthless_edges() edge runs between two copies of vu??\n");
			if( eu->vu_p->v_p == neu->vu_p->v_p )  {
#if 0
				nmg_pr_eu( eu, NULL );
				nmg_pr_eu( neu, NULL );
#endif
				bu_log("eu=x%x, neu=x%x, v=x%x\n", eu, neu, eu->vu_p->v_p);
				bu_log("eu=x%x, neu=x%x, v=x%x\n", eu->eumate_p, neu->eumate_p, eu->eumate_p->vu_p->v_p);
				rt_bomb("nmg_ck_face_worthless_edges() edge runs from&to same vertex\n");
				return 1;
			}
		}
	}
	return 0;

}



/**
 *			N M G _ C K _ L U E U
 *
 *	check all the edgeuses of a loopuse to make sure these children
 *	know who thier parent really is.
 */
void nmg_ck_lueu(const struct loopuse *cklu, const char *s)
{
	struct edgeuse *eu;

	if (BU_LIST_FIRST_MAGIC(&cklu->down_hd) == NMG_VERTEXUSE_MAGIC)
		rt_bomb("NMG nmg_ck_lueu.  I got a vertex loop!\n");

	eu = BU_LIST_FIRST(edgeuse, &cklu->down_hd);
	if (eu->l.back != &cklu->down_hd) {
		rt_bomb("nmg_ck_lueu first element in list doesn't point back to head\n");
	}

	for (BU_LIST_FOR(eu, edgeuse, &cklu->down_hd)) {
		NMG_CK_EDGEUSE(eu);
		if (eu->up.lu_p != cklu) {
			bu_log("nmg_cl_lueu() edgeuse of %s (going next) has lost proper parent\n", s);
			rt_bomb("nmg_ck_lueu\n");
		}
		if ((struct edgeuse *)eu->l.forw->back != eu) {
			bu_log("nmg_cl_lueu() %s next edge (%8x) doesn't point back to me (%8x)!\n", s, eu->l.forw, eu);
			nmg_pr_lu(cklu, NULL);
		}
		if ((struct edgeuse *)eu->l.back->forw != eu) {
			bu_log("nmg_cl_lueu() %s last edge (%8x) doesn't point forward to me (%8x)!\n", s, eu->l.forw, eu);
			nmg_pr_lu(cklu, NULL);
		}
	}

	cklu = cklu->lumate_p;

	eu = BU_LIST_FIRST(edgeuse, &cklu->down_hd);
	if (eu->l.back != &cklu->down_hd) {
		rt_bomb("nmg_ck_lueu first element in lumate list doesn't point back to head\n");
	}

	for (BU_LIST_FOR(eu, edgeuse, &cklu->down_hd)) {
		NMG_CK_EDGEUSE(eu);
		if (eu->up.lu_p != cklu) {
			bu_log("nmg_cl_lueu() edgeuse of %s (lumate going next) has lost proper parent\n", s);
			rt_bomb("nmg_ck_lueu\n");
		}
		if ((struct edgeuse *)eu->l.forw->back != eu) {
			bu_log("nmg_cl_lueu() %s next edge (%8x) doesn't point back to me (%8x)!\n", s, eu->l.forw, eu);
			nmg_pr_lu(cklu, NULL);
		}
		if ((struct edgeuse *)eu->l.back->forw != eu) {
			bu_log("nmg_cl_lueu() %s (lumate) back edge (%8x) doesn't point forward to me (%8x)!\n", s, eu->l.forw, eu);
			nmg_pr_lu(cklu, NULL);
		}
	}
}

/**
 *			N M G _ C H E C K _ R A D I A L
 *
 *	check to see if all radial uses of an edge (within a shell) are
 *	properly oriented with respect to each other.
 *	NOTE that ONLY edgeuses belonging to the shell of eu are checked.
 *
 *	Can't check faceuse orientation parity for
 *	things from more than one shell;  parity is conserved
 *	only within faces from a single shell.
 *
 *  XXX Added code to skip dangling faces (needs to be checked a little more) - JRA
 *
 *  XXX I think that if dangling faces are to be processed correctly,
 *  XXX the caller should pass in a table of dangling faces.  -Mike
 *  XXX I've #if'ed that check out, for now.
 *
 *	Return
 *	0	OK
 *	1	bad edgeuse mate
 *	2	unclosed space
 */
int
nmg_check_radial(const struct edgeuse *eu, const struct bn_tol *tol)
{
	const struct shell	*s;

	NMG_CK_EDGEUSE(eu);
	BN_CK_TOL(tol);
	s = nmg_find_s_of_eu(eu);
	NMG_CK_SHELL(s);

#if 1
	/* Just use the new checker */
	return nmg_eu_radial_check( eu, s, tol );
#else
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_check_radial(eu=x%x, tol)\n", eu);
	}

	eu_orig = eu;
	eu1 = eu;

	/* If this eu is a wire, advance to first non-wire (skipping dangling faces). */
	while( (fu = nmg_find_fu_of_eu(eu)) == (struct faceuse *)NULL ||
		nmg_find_s_of_eu((struct edgeuse *)eu) != s
#if BO_DANGLE
		|| nmg_dangling_face( fu, (char *)NULL )
#endif
	)  {
		eu = eu->radial_p->eumate_p;
		if( eu == eu1 )  return 0;	/* wires all around */
	}

	curr_orient = fu->orientation;
	eur = eu->radial_p;
	eurstart = eur;
	eu1 = eu;				/* virtual radial to eur */

	NMG_CK_EDGEUSE(eur);
	do {
		/*
		 *  Search until another edgeuse in this shell is found.
		 *  Continue search if it is a wire edge or dangling face.
		 */
		while( nmg_find_s_of_eu((struct edgeuse *)eur) != s  ||
		       (fu = nmg_find_fu_of_eu(eur)) == (struct faceuse *)NULL
#if BO_DANGLE
			|| nmg_dangling_face( fu, (char *)NULL )
#endif
		)  {
			/* Advance to next eur */
			NMG_CK_EDGEUSE(eur->eumate_p);
			if (eur->eumate_p->eumate_p != eur) {
				rt_bomb("nmg_check_radial: bad edgeuse mate\n");
			}
			eur = eur->eumate_p->radial_p;
			NMG_CK_EDGEUSE(eur);
			if( eur == eurstart )  return 0;
		}

		/* if that radial edgeuse doesn't have the
		 * correct orientation, print & bomb
		 * If radial (eur) is my (virtual, this-shell) mate (eu1),
		 * then it's ok, a mis-match is to be expected.
		 */
		NMG_CK_LOOPUSE(eur->up.lu_p);
		fu = eur->up.lu_p->up.fu_p;
		NMG_CK_FACEUSE(fu);
		if (fu->orientation != curr_orient &&
		    eur != eu1->eumate_p ) {
		    	char file[128];
		    	char buf[128];
		    	static int num=0;

			p = eu1->vu_p->v_p->vg_p->coord;
			q = eu1->eumate_p->vu_p->v_p->vg_p->coord;
			bu_log("nmg_check_radial(): Radial orientation problem\n  edge: %g %g %g -> %g %g %g\n",
				p[0], p[1], p[2], q[0], q[1], q[2]);
			bu_log("  eu_orig=%8x, eur=%8x, s=x%x, eurstart=x%x, curr_orient=%s\n",
				eu_orig, eur, s, eurstart,
				nmg_orientation(curr_orient) );

			/* Plot the edge in yellow, & the loops */
			rt_g.NMG_debug |= DEBUG_PLOTEM;
			nmg_face_lu_plot( eu1->up.lu_p, eu1->vu_p,
				eu1->eumate_p->vu_p );
			nmg_face_lu_plot( eur->up.lu_p, eur->vu_p,
				eur->eumate_p->vu_p );

		    	sprintf(buf, "%g %g %g -> %g %g %g\n",
				p[0], p[1], p[2], q[0], q[1], q[2]);
		    	
		    	sprintf(file, "radial%d.g", num++);
		    	nmg_stash_model_to_file( file, 
		    		nmg_find_model(&(fu->l.magic)), buf);

			nmg_pr_fu_around_eu( eu_orig, tol );

			bu_log("nmg_check_radial: unclosed space\n");
			return(2);
		}

		eu1 = eur->eumate_p;
		NMG_CK_LOOPUSE(eu1->up.lu_p);
		NMG_CK_FACEUSE(eu1->up.lu_p->up.fu_p);
		curr_orient = eu1->up.lu_p->up.fu_p->orientation;
		eur = eu1->radial_p;
	} while (eur != eurstart);
	return(0);
#endif
}

/**
 *			N M G _ E U _ 2 S _ O R I E N T _ B A D
 *
 *  Given an edgeuse, check that the proper orientation "parity" of
 *  same/opposite/opposite/same is preserved, for all non-wire edgeuses
 *  within shell s1.
 *  If s2 is non-null, then ensure that the parity of all edgeuses in
 *  BOTH s1 and s2 are correct, and mutually compatible.
 *
 *  This routine does not care if a face is "dangling" or not.
 *
 *  If the edgeuse specified is a wire edgeuse, skip forward to a non-wire.
 *
 *  Returns -
 *	0	OK
 *	!0	Bad orientation parity.
 */
int
nmg_eu_2s_orient_bad(const struct edgeuse *eu, const struct shell *s1, const struct shell *s2, const struct bn_tol *tol)
{
	char			curr_orient;
	const struct edgeuse	*eu_orig;
	const struct edgeuse	*eur;
	const struct edgeuse	*eu1;
	const struct edgeuse	*eurstart;
	const struct faceuse	*fu;
	const struct shell	*s;
	int			ret = 0;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_SHELL(s1);
	if(s2) NMG_CK_SHELL(s2);	/* s2 may be NULL */
	BN_CK_TOL(tol);

	eu_orig = eu;			/* for printing */
	eu1 = eu;			/* remember, for loop termination */

	/*
	 *  If this eu is not in s1, or it is a wire,
	 *  advance to first non-wire.
	 */
	for(;;)  {
		fu = nmg_find_fu_of_eu(eu);
		if( !fu ) goto next_a;		/* it's a wire */
		s = fu->s_p;
		NMG_CK_SHELL(s);
		if( s != s1 )  goto next_a;
		break;
next_a:
		eu = eu->radial_p->eumate_p;
		if( eu == eu1 )  goto out;	/* wires all around */
	}

	curr_orient = fu->orientation;
	eur = eu->radial_p;
	eurstart = eur;
	eu1 = eu;				/* virtual radial to eur */

	NMG_CK_EDGEUSE(eur);
	do {
		/*
		 *  Search until another edgeuse in shell s1 or s2 is found.
		 *  Continue search if it is a wire edge or dangling face.
		 */
		for(;;)  {
			fu = nmg_find_fu_of_eu(eur);
			if( !fu ) goto next_eu;		/* it's a wire */
			NMG_CK_FACEUSE(fu);
			s = fu->s_p;
			NMG_CK_SHELL(s);
			if( s != s1 )  {
				if( !s2 )  goto next_eu;
				if( s != s2 )  goto next_eu;
			}
			break;
next_eu:
			/* Advance to next eur */
			NMG_CK_EDGEUSE(eur->eumate_p);
			if (eur->eumate_p->eumate_p != eur)
				rt_bomb("nmg_eu_2s_orient_bad: bad edgeuse mate\n");

			eur = eur->eumate_p->radial_p;
			NMG_CK_EDGEUSE(eur);
			if( eur == eurstart )  goto out;
		}

		/*
		 *  eur is mate's radial of last eu.
		 *  If the orientation does not match, this is an error.
		 *  If radial (eur) is my (virtual, this-shell) mate (eu1),
		 *  then it's OK, a mis-match is to be expected when there
		 *  is only one edgeuse&mate from this shell on this edge.
		 */
		if (fu->orientation != curr_orient &&
		    eur != eu1->eumate_p ) {
			nmg_pr_fu_around_eu( eu_orig, tol );
			bu_log("nmg_eu_2s_orient_bad(eu=x%x, s1=x%x, s2=x%x) bad radial parity eu1=x%x, eur=x%x, eurstart=x%x\n",
				eu_orig, s1, s2, eu1, eur, eurstart);
		    	ret = 1;
		    	goto out;
		}

		/* If eu belongs to a face, eumate had better, also! */
		eu1 = eur->eumate_p;
		NMG_CK_LOOPUSE(eu1->up.lu_p);
		fu = eu1->up.lu_p->up.fu_p;
		NMG_CK_FACEUSE(fu);
		curr_orient = fu->orientation;
		eur = eu1->radial_p;
	} while (eur != eurstart);
	/* All is well, the whole way 'round */
out:
	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		bu_log("nmg_eu_2s_orient_bad(eu=x%x, s1=x%x, s2=x%x) ret=%d\n",
			eu_orig, s1, s2, ret);
	}
	return ret;
}

/**
 *		 	N M G _ C K _ C L O S E D _ S U R F
 *
 *  Verify that shell is closed.
 *  Do this by verifying that it is not possible to get from outside
 *  to inside the solid by crossing any face edge.
 *
 *  Returns -
 *	 0	OK
 *	!0	Problem.
 */
int
nmg_ck_closed_surf(const struct shell *s, const struct bn_tol *tol)
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	int		status = 0;
	long		magic1;

	NMG_CK_SHELL(s);
	BN_CK_TOL(tol);
	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		NMG_CK_FACEUSE(fu);
		for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
			NMG_CK_LOOPUSE(lu);
			magic1 = BU_LIST_FIRST_MAGIC( &lu->down_hd );
			if (magic1 == NMG_EDGEUSE_MAGIC) {
				/* Check status on all the edgeuses before quitting */
				for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
					if (nmg_check_radial(eu, tol))
						status = 1;
				}
				if( status )  {
					bu_log("nmg_ck_closed_surf(x%x), problem with loopuse x%x\n", s, lu);
					return 1;
				}
			} else if (magic1 == NMG_VERTEXUSE_MAGIC) {
				register struct vertexuse	*vu;
				vu = BU_LIST_FIRST( vertexuse, &lu->down_hd );
				NMG_CK_VERTEXUSE(vu);
				NMG_CK_VERTEX(vu->v_p);
			}
		}
	}
	return(0);
}

/**
 *			N M G _ C K _ C L O S E D _ R E G I O N
 *
 *  Check all the shells in a region for being closed.
 *
 *  Returns -
 *	 0	OK
 *	!0	status code from nmg_check_radial()
 */
int
nmg_ck_closed_region(const struct nmgregion *r, const struct bn_tol *tol)
{
	const struct shell	*s;
	int		ret;

	NMG_CK_REGION(r);
	BN_CK_TOL(tol);
	for( BU_LIST_FOR( s, shell, &r->s_hd ) )  {
		ret = nmg_ck_closed_surf( s, tol );
		if( ret != 0 )  return(ret);
	}
	return(0);
}

/**	N M G _ C K _ V _ I N _ 2 F U S
 *
 *	accepts a vertex pointer, two faceuses, and a tolerance.
 *	Checks if the vertex is in both faceuses (topologically
 *	and geometrically within tolerance of plane).
 *
 *	Calls rt_bomb if vertex is not in the faceuses topology or
 *	out of tolerance of either face.
 *
 */

void
nmg_ck_v_in_2fus(const struct vertex *vp, const struct faceuse *fu1, const struct faceuse *fu2, const struct bn_tol *tol)
{
	struct bu_vls str;
	struct faceuse *fu;
	struct vertexuse *vu;
	fastf_t dist1,dist2;
	int found1=0,found2=0;
	plane_t	n1, n2;

	NMG_CK_VERTEX( vp );
	NMG_CK_FACEUSE( fu1 );
	NMG_CK_FACEUSE( fu2 );
	BN_CK_TOL( tol );

	/* topology check */
	for( BU_LIST_FOR( vu , vertexuse , &vp->vu_hd ) )
	{
		fu = nmg_find_fu_of_vu( vu );
		if( fu == fu1 )
			found1 = 1;
		if( fu == fu2 )
			found2 = 1;
		if( found1 && found2 )
			break;
	}

	if( !found1 || !found2 )
	{
		bu_vls_init( &str );
		bu_vls_printf( &str , "nmg_ck_v_in_2fus: vertex x%x not used in" , vp );
		if( !found1 )
			bu_vls_printf( &str , " faceuse x%x" , fu1 );
		if( !found2 )
			bu_vls_printf( &str , " faceuse x%x" , fu2 );
		rt_bomb( bu_vls_addr( &str ) );
	}

	/* geometry check */
	NMG_GET_FU_PLANE(n1, fu1);
	NMG_GET_FU_PLANE(n2, fu2);
	dist1 = DIST_PT_PLANE( vp->vg_p->coord , n1 );
	dist2 = DIST_PT_PLANE( vp->vg_p->coord , n2 );

	if( !NEAR_ZERO( dist1 , tol->dist ) || !NEAR_ZERO( dist2 , tol->dist ) )
	{
		bu_vls_init( &str );
		bu_vls_printf( &str , "nmg_ck_v_in_2fus: vertex x%x ( %g %g %g ) not in plane of" ,
				vp , V3ARGS( vp->vg_p->coord ) );
		if( !NEAR_ZERO( dist1 , tol->dist ) )
			bu_vls_printf( &str , " faceuse x%x (off by %g)" , fu1 , dist1 );
		if( !NEAR_ZERO( dist2 , tol->dist ) )
			bu_vls_printf( &str , " faceuse x%x (off by %g)" , fu2 , dist2 );
		rt_bomb( bu_vls_addr( &str ) );
	}

}
/**	N M G _ C K _ V S _ I N _ R E G I O N
 *
 *	Visits every vertex in the region and checks if the
 *	vertex coordinates are within tolerance of every face
 *	it is supposed to be in (according to the topology).
 *
 */

struct v_ck_state {
        char            *visited;
        struct bu_ptbl *tabl;
	struct bn_tol	*tol;
};

static void
nmg_ck_v_in_fus(long int *vp, genptr_t state, int first)
{
        register struct v_ck_state *sp = (struct v_ck_state *)state;
        register struct vertex  *v = (struct vertex *)vp;

        NMG_CK_VERTEX(v);
        /* If this vertex has been processed before, do nothing more */
        if( NMG_INDEX_FIRST_TIME(sp->visited, v) )
	{
		struct vertexuse *vu;
		struct faceuse *fu;

		for( BU_LIST_FOR( vu , vertexuse , &v->vu_hd ) )
		{
			fastf_t dist;

			fu = nmg_find_fu_of_vu( vu );
			if( fu )
			{
				plane_t		n;

				NMG_CK_FACEUSE( fu );
				if( fu->orientation != OT_SAME )
					continue;
				if( !fu->f_p->g.magic_p )
					bu_log( "ERROR - nmg_ck_vs_in_region: fu (x%x) has no geometry\n", fu );
				else if( *fu->f_p->g.magic_p == NMG_FACE_G_PLANE_MAGIC )
				{
					NMG_GET_FU_PLANE( n, fu );
					dist = DIST_PT_PLANE( v->vg_p->coord , n );
					if( !NEAR_ZERO( dist , sp->tol->dist ) )
					{
						bu_log( "ERROR - nmg_ck_vs_in_region: vertex x%x ( %g %g %g ) is %g from faceuse x%x\n" , 
							v , V3ARGS( v->vg_p->coord ) , dist , fu );
					}
				}
				/* else if( *fu->f_p->g.magic_p == NMG_FACE_G_SNURB_MAGIC ) XXXX */
			}
		}
	}
}

void
nmg_ck_vs_in_region(const struct nmgregion *r, const struct bn_tol *tol)
{
	struct model			*m;
	struct v_ck_state		st;
	struct bu_ptbl			tab;
	static const struct nmg_visit_handlers handlers = {NULL, NULL, NULL, NULL, NULL, 
							   NULL, NULL, NULL, NULL, NULL, 
							   NULL, NULL, NULL, NULL, NULL, 
							   NULL, NULL, NULL, NULL, NULL, 
							   NULL, NULL, NULL, nmg_ck_v_in_fus, NULL};
        /* handlers.vis_vertex = nmg_ck_v_in_fus; */

        NMG_CK_REGION(r);
	BN_CK_TOL( tol );
        m = r->m_p;
        NMG_CK_MODEL(m);

        st.visited = (char *)bu_calloc(m->maxindex+1, sizeof(char), "visited[]");
        st.tabl = &tab;
	st.tol = (struct bn_tol *)tol;

        (void)bu_ptbl_init( &tab, 64, " &tab");

        nmg_visit( &r->l.magic, &handlers, (genptr_t)&st );

	bu_ptbl_free( &tab );

        bu_free( (char *)st.visited, "visited[]");
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
