/*
 *			N M G _ C K . C
 *
 *  Validators and consistency checkers for NMG data structures
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
 *	John R. Anderson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"
#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

/* XXX Move to raytrace.h */
RT_EXTERN(void		nmg_ck_list_magic, (CONST struct rt_list *hd,
			CONST char *str, CONST long magic) );

/************************************************************************
 *									*
 *			Validator Routines				*
 *									*
 ************************************************************************/

/*
 *			N M G _ V V G
 *
 *  Verify vertex geometry
 */
void
nmg_vvg(vg)
struct vertex_g *vg;
{
	NMG_CK_VERTEX_G(vg);
}

/*
 *			N M G _ V V E R T E X
 *
 *  Verify a vertex
 */
void
nmg_vvertex(v, vup)
struct vertex *v;
struct vertexuse *vup;
{
	struct vertexuse *vu;
	int vup_is_in_list = 0;

	NMG_CK_VERTEX(v);

	for( RT_LIST_FOR( vu, vertexuse, &v->vu_hd ) )  {
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
nmg_vvua(vua)
long *vua;
{
	NMG_CK_VERTEXUSE_A_EITHER(vua);
}

/*
 *			N M G _ V V U
 *
 *  Verify vertexuse
 */
void
nmg_vvu(vu, up_magic_p)
struct vertexuse *vu;
long		*up_magic_p;
{
	long	magic;

	NMG_CK_VERTEXUSE(vu);
	if (vu->up.magic_p != up_magic_p)  {
		rt_log("nmg_vvu() up is %s, s/b %s\n",
			rt_identify_magic( *vu->up.magic_p ),
			rt_identify_magic( *up_magic_p ) );
		rt_bomb("nmg_vvu() vertexuse denies parent\n");
	}

	if (!vu->l.forw)
		rt_bomb("nmg_vvu() vertexuse has null forw pointer\n");

	magic = RT_LIST_FIRST_MAGIC( &vu->l );
	if( magic != NMG_VERTEXUSE_MAGIC && magic != RT_LIST_HEAD_MAGIC )
		rt_bomb("nmg_vvu() vertexuse forw is bad vertexuse\n");

	if (RT_LIST_PNEXT_PLAST(vertexuse,vu) != vu )
		rt_bomb("nmg_vvu() vertexuse not back of next vertexuse\n");

	nmg_vvertex(vu->v_p, vu);

	if (vu->a.magic_p) nmg_vvua(vu->a.magic_p);
}

/* Verify edge geometry */
void
nmg_veg(eg)
long *eg;
{
	struct rt_list	*eu2;

	NMG_CK_EDGE_G_EITHER(eg);
	switch( *eg )  {
	case NMG_EDGE_G_LSEG_MAGIC:
		nmg_ck_list_magic( &((struct edge_g_lseg *)eg)->eu_hd2,
			"nmg_veg() edge_g_lseg eu_hd2 list",
			NMG_EDGEUSE2_MAGIC );
		break;
	case NMG_EDGE_G_CNURB_MAGIC:
		nmg_ck_list_magic( &((struct edge_g_cnurb *)eg)->eu_hd2,
			"nmg_veg() edge_g_cnurb eu_hd2 list",
			NMG_EDGEUSE2_MAGIC );
		break;
	}

	/* Ensure that all edgeuses on the edge_g_* list point to me */
	for( RT_LIST_FOR( eu2, rt_list, &((struct edge_g_lseg *)eg)->eu_hd2 ) )  {
		struct edgeuse	*eu;
		eu = RT_LIST_MAIN_PTR( edgeuse, eu2, l2 );
		NMG_CK_EDGEUSE(eu);
		if( eu->g.magic_p == eg )  continue;
		rt_log("eg=x%x, eu=x%x, eu->g=x%x\n", eg, eu, eu->g.magic_p);
		rt_log("nmg_veg() edgeuse is on wrong eu_hd2 list for eu->g\n");
	}
}

/*
 *			N M G _ V E D G E
 *
 *  Verify edge
 */
void
nmg_vedge(e, eup)
struct edge *e;
struct edgeuse *eup;
{
	struct edgeuse *eu;
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
				rt_bomb("nmg_vedge() edgeuse mate does not have correct vertex\n");
		} else if (eu->vu_p->v_p == eup->eumate_p->vu_p->v_p) {
			if (eu->eumate_p->vu_p->v_p != eup->vu_p->v_p)
				rt_bomb("nmg_vedge() edgeuse does not have correct vertex\n");
		} else
			rt_bomb("nmg_vedge() edgeuse does not share vertex endpoint\n");

		eu = eu->eumate_p->radial_p;
	} while (eu != eup);

	if (!is_use)
		rt_bomb("nmg_vedge() Cannot get from edge to parent edgeuse\n");
}

/*
 *			N M G _ V E U
 *
 *  Verify edgeuse list.
 */
void
nmg_veu(hp, up_magic_p)
struct rt_list	*hp;
long	*up_magic_p;
{
	struct edgeuse	*eu;
	struct edgeuse	*eunext;
	struct edgeuse	*eulast;
	long		up_magic;

	nmg_ck_list_magic( hp, "nmg_veu() edegeuse list head", NMG_EDGEUSE_MAGIC );

	up_magic = *up_magic_p;
	switch( up_magic )  {
	case NMG_SHELL_MAGIC:
	case NMG_LOOPUSE_MAGIC:
		break;
	default:
		rt_bomb("nmg_veu() bad up_magic_p\n");
	}
	for( RT_LIST_FOR( eu, edgeuse, hp ) )  {
		NMG_CK_EDGEUSE(eu);

		if (eu->up.magic_p != up_magic_p)
			rt_bomb("nmg_veu() edgeuse denies parentage\n");

		if (!eu->l.forw)
			rt_bomb("nmg_veu() edgeuse has Null \"forw\" pointer\n");
		eunext = RT_LIST_PNEXT_CIRC( edgeuse, eu );
		eulast = RT_LIST_PPREV_CIRC(edgeuse, &eu->l);
		if (eunext->l.magic != NMG_EDGEUSE_MAGIC)
			rt_bomb("nmg_veu() edgeuse forw is bad edgeuse\n");
		if (eulast->l.magic != NMG_EDGEUSE_MAGIC)
			rt_bomb("nmg_veu() edgeuse back is bad edgeuse\n");
		NMG_CK_EDGEUSE(eunext);
		NMG_CK_EDGEUSE(eulast);

		/* Check that forw->back is us */
		if (RT_LIST_PPREV_CIRC(edgeuse,eunext) != eu )  {
		    if (eunext->l.back)
			rt_bomb("nmg_veu() next edgeuse has back that points elsewhere\n");
		    else
			rt_bomb("nmg_veu() next edgeuse has NULL back\n");
		}

		/*
		 *  For edgeuses in loops, ensure that vertices are shared.
		 *  This does not apply to wire edgeuses in the shell.
		 */
		if ( up_magic == NMG_LOOPUSE_MAGIC &&
		     eu->vu_p->v_p != eulast->eumate_p->vu_p->v_p) {
		     	rt_log("eu=x%x, e=x%x\n", eu, eu->e_p );
		     	rt_log("eulast=x%x, e=x%x\n", eulast, eulast->e_p);
		     	rt_log("	    eu: (%g, %g, %g) <--> (%g, %g, %g)\n",
		     		V3ARGS(eu->vu_p->v_p->vg_p->coord),
		     		V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord) );
		     	rt_log("	eulast: (%g, %g, %g) <--> (%g, %g, %g)\n",
		     		V3ARGS(eulast->vu_p->v_p->vg_p->coord),
		     		V3ARGS(eulast->eumate_p->vu_p->v_p->vg_p->coord) );
			rt_log("unshared vertex (mine) v=x%x: (%g, %g, %g)\n",
				eu->vu_p->v_p,
				V3ARGS(eu->vu_p->v_p->vg_p->coord) );
			rt_log("\t\t (last->eumate_p) v=x%x: (%g, %g, %g)\n",
				eulast->eumate_p->vu_p->v_p,
				V3ARGS(eulast->eumate_p->vu_p->v_p->vg_p->coord) );
		     	nmg_pr_lu_briefly(eu->up.lu_p, (char *)NULL);
		     	nmg_pr_lu_briefly(eu->up.lu_p->lumate_p, (char *)NULL);
			rt_bomb("nmg_veu() discontinuous edgeloop mine/last\n");
		}
		if ( up_magic == NMG_LOOPUSE_MAGIC &&
		     eunext->vu_p->v_p != eu->eumate_p->vu_p->v_p) {
		     	rt_log("eu=x%x, e=x%x\n", eu, eu->e_p );
		     	rt_log("eunext=x%x, e=x%x\n", eunext, eunext->e_p);
		     	rt_log("	    eu: (%g, %g, %g) <--> (%g, %g, %g)\n",
		     		V3ARGS(eu->vu_p->v_p->vg_p->coord),
		     		V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord) );
		     	rt_log("	eunext: (%g, %g, %g) <--> (%g, %g, %g)\n",
		     		V3ARGS(eunext->vu_p->v_p->vg_p->coord),
		     		V3ARGS(eunext->eumate_p->vu_p->v_p->vg_p->coord) );
			rt_log("unshared vertex (mate) v=x%x: (%g, %g, %g)\n",
				eu->eumate_p->vu_p->v_p,
				V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord) );
			rt_log("\t\t (next) v=x%x: (%g, %g, %g)\n",
				eunext->vu_p->v_p,
				V3ARGS(eunext->vu_p->v_p->vg_p->coord) );
		     	nmg_pr_lu_briefly(eu->up.lu_p, (char *)NULL);
		     	nmg_pr_lu_briefly(eu->up.lu_p->lumate_p, (char *)NULL);
			rt_bomb("nmg_veu() discontinuous edgeloop next/mate\n");
		}

		/* Check mate and radial */
		if (eu->eumate_p->l.magic != NMG_EDGEUSE_MAGIC)
			rt_bomb("nmg_veu() edgeuse mate is bad edgeuse\n");
		else if (eu->eumate_p->eumate_p != eu)
			rt_bomb("nmg_veu() edgeuse mate spurns edgeuse\n");

		if (eu->radial_p->l.magic != NMG_EDGEUSE_MAGIC)
			rt_bomb("nmg_veu() edgeuse radial is bad edgeuse\n");
		else if (eu->radial_p->radial_p != eu)
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

/*
 *			N M G _ V L G
 *
 *  Verify loop geometry
 */
void
nmg_vlg(lg)
struct loop_g *lg;
{
	int i;
	
	NMG_CK_LOOP_G(lg);

	for (i=0 ; i < ELEMENTS_PER_PT ; ++i)
		if (lg->min_pt[i] > lg->max_pt[i])
			rt_bomb("nmg_vlg() loop geom min_pt greater than max_pt\n");
}

/*
 *			N M G _ V L O O P
 *
 *  Verify loop
 */
void
nmg_vloop(l, lup)
struct loop *l;
struct loopuse *lup;
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

/*
 *			N M G _ V L U
 *
 *  Verify loopuse
 */
void
nmg_vlu(hp, up)
struct rt_list	*hp;
long		*up;
{
	struct loopuse *lu;

	for( RT_LIST_FOR( lu, loopuse, hp ) )  {
		NMG_CK_LOOPUSE(lu);

		if (lu->up.magic_p != up)  {
			rt_log("nmg_vlu() up is x%x, s/b x%x\n",
				lu->up.magic_p, up );
			rt_bomb("nmg_vlu() loopuse denies parentage\n");
		}

		if (!lu->l.forw)
			rt_bomb("nmg_vlu() loopuse has null forw pointer\n");
		else if (RT_LIST_PNEXT_PLAST(loopuse,lu) != lu )
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
			rt_log("lu=x%x, orientation=%d\n", lu, lu->orientation);
			rt_bomb("nmg_vlu() unknown loopuse orintation\n");
			break;
		}
		if (lu->lumate_p->orientation != lu->orientation)
			rt_bomb("nmg_vlu() loopuse and mate have different orientation\n");

		if (!lu->l_p)
			rt_bomb("nmg_vlu() loopuse has Null loop pointer\n");
		else {
			nmg_vloop(lu->l_p, lu);
		}

		if( RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC)
			nmg_veu( &lu->down_hd, &lu->l.magic);
		else if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC)
			nmg_vvu(RT_LIST_FIRST(vertexuse,&lu->down_hd), &lu->l.magic);
		else
			rt_bomb("nmg_vlu() bad down_hd magic\n");
	}
}

/*
 *			N M G _ V F G
 *
 *  Verify face geometry
 */
void
nmg_vfg(fg)
struct face_g_plane *fg;
{
	NMG_CK_FACE_G_EITHER(fg);

	if( fg->magic == NMG_FACE_G_PLANE_MAGIC )
	{
		if (fg->N[X]==0.0 && fg->N[Y]==0.0 && fg->N[Z]==0.0 &&
		    fg->N[H]!=0.0) {
			rt_log("bad NMG plane equation %fX + %fY + %fZ = %f\n",
				fg->N[X], fg->N[Y], fg->N[Z], fg->N[H]);
			rt_bomb("nmg_vfg() Bad NMG geometry\n");
		    }
	}
	if( fg->magic == NMG_FACE_G_SNURB_MAGIC )
	{
		/* XXX Should the face's NURB be checked somehow?? */
	}
}

/*
 *			N M G _ V F A C E
 *
 *  Verify face
 */
void
nmg_vface(f, fup)
struct face *f;
struct faceuse *fup;
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
			rt_log("nmg_vface() face min_pt[%d]:%g greater than max_pt[%d]:%g\n",
				i, f->min_pt[i], i, f->max_pt[i]);
			rt_log("min_pt(%g %g %g)  ", V3ARGS(f->min_pt));
			rt_log("max_pt(%g %g %g)\n", V3ARGS(f->max_pt));
			rt_bomb("Invalid NMG\n");
		}
	if (f->g.plane_p) nmg_vfg(f->g.plane_p);
}

/*
 *			N M G _ V F U
 *
 *	Validate a list of faceuses
 */
void
nmg_vfu(hp, s)
struct rt_list	*hp;
struct shell *s;
{
	struct faceuse *fu;

	NMG_CK_SHELL(s);

	for( RT_LIST_FOR( fu, faceuse, hp ) )  {
		NMG_CK_FACEUSE(fu);
		if (fu->s_p != s) {
			rt_log("faceuse claims shell parent (%8x) instead of (%8x)\n",
				fu->s_p, s);
			rt_bomb("nmg_vfu()\n");
		}

		if (!fu->l.forw) {
			rt_bomb("nmg_vfu() faceuse forw is NULL\n");
		} else if (fu->l.forw->back != (struct rt_list *)fu) {
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


/*
 *			N M G _ V S H E L L
 *
 *	validate a list of shells and all elements under them
 */
void
nmg_vshell(hp, r)
struct rt_list	*hp;
struct nmgregion *r;
{
	struct shell *s;
	pointp_t lpt, hpt;

	NMG_CK_REGION(r);

	for( RT_LIST_FOR( s, shell, hp ) )  {
		NMG_CK_SHELL(s);
		if (s->r_p != r) {
			rt_log("shell's r_p (%8x) doesn't point to parent (%8x)\n",
				s->r_p, r);
			rt_bomb("nmg_vshell()\n");
		}

		if (!s->l.forw) {
			rt_bomb("nmg_vshell(): Shell's forw ptr is null\n");
		} else if (s->l.forw->back != (struct rt_list *)s) {
			rt_log("forw shell's back(%8x) is not me (%8x)\n",
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
				rt_log("Bnmg_vshell() ad min_pt/max_pt for shell(%8x)'s extent\n");
				rt_log("Min_pt %g %g %g\n", lpt[0], lpt[1],
					lpt[2]);
				rt_log("Max_pt %g %g %g\n", hpt[0], hpt[1],
					hpt[2]);
			}
		}

		/* now we check out the "children"
		 */

		if (s->vu_p) {
			if( RT_LIST_NON_EMPTY( &s->fu_hd ) ||
			    RT_LIST_NON_EMPTY( &s->lu_hd ) ||
			    RT_LIST_NON_EMPTY( &s->eu_hd ) )  {
				rt_log("shell (%8x) with vertexuse (%8x) has other children\n",
					s, s->vu_p);
				rt_bomb("nmg_vshell()\n");
			}
		}

		nmg_vfu( &s->fu_hd, s);
		nmg_vlu( &s->lu_hd, &s->l.magic);
		nmg_veu( &s->eu_hd, &s->l.magic);
	}
}



/*
 *			N M G _ V R E G I O N
 *
 *	validate a list of nmgregions and all elements under them
 */
void
nmg_vregion(hp, m)
struct rt_list	*hp;
struct model *m;
{
	struct nmgregion *r;

	for( RT_LIST_FOR( r, nmgregion, hp ) )  {
		NMG_CK_REGION(r);
		if (r->m_p != m) {
			rt_log("nmgregion pointer m_p %8x should be %8x\n",
				r->m_p, m);
			rt_bomb("nmg_vregion()\n");
		}
		if (r->ra_p) {
			NMG_CK_REGION_A(r->ra_p);
		}

		nmg_vshell( &r->s_hd, r);

		if( RT_LIST_PNEXT_PLAST(nmgregion, r) != r )  {
			rt_bomb("nmg_vregion() forw nmgregion's back is not me\n");
		}
	}
}

/*
 *			N M G _ V M O D E L
 *
 *	validate an NMG model and all elements in it.
 */
void
nmg_vmodel(m)
struct model *m;
{
	NMG_CK_MODEL(m);
	nmg_vregion( &m->r_hd, m);
}


/************************************************************************
 *									*
 *			Checking Routines				*
 *									*
 ************************************************************************/

/*
 *			N M G _ C K _ E
 */
void
nmg_ck_e(eu, e, str)
struct edgeuse *eu;
struct edge *e;
char *str;
{
	char *errstr;
	struct edgeuse *eparent;
	errstr = rt_calloc(strlen(str)+128, 1, "nmg_ck_e error str");
	(void)sprintf(errstr, "%sedge %8x\n", str, e);
	
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

	rt_free(errstr, "nmg_ck_e error str");
}

/*
 *			N M G _ C K _ V U
 */
void
nmg_ck_vu(parent, vu, str)
long *parent;
struct vertexuse *vu;
char *str;
{
	char *errstr;

	errstr = rt_calloc(strlen(str)+128, 1, "nmg_ck_vu error str");
	(void)sprintf(errstr, "%svertexuse %8x\n", str, vu);
	
	if (vu->up.magic_p != parent) rt_bomb(
		strcat(errstr, "nmg_ck_vu() Vertexuse denies parentage\n"));

	rt_free(errstr, "nmg_ck_vu error str");
}

/*
 *			N M G _ C K _ E U
 */
void
nmg_ck_eu(parent, eu, str)
long *parent;
struct edgeuse *eu;
char *str;
{
	char *errstr;
	struct edgeuse *eur, *eu_next, *eu_last;	

	errstr = rt_calloc(strlen(str)+128, 1, "nmg_ck_eu error str");
	(void)sprintf(errstr, "%sedgeuse %8x\n", str, eu);

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
		else if (eur == eu) rt_bomb(
			strcat(errstr, "nmg_ck_eu() Never saw eumate\n"));

		eu_next = RT_LIST_PNEXT_CIRC(edgeuse, eu);
		if (eu_next->vu_p->v_p != eu->eumate_p->vu_p->v_p)
			rt_bomb("nmg_ck_eu: next and mate don't share vertex\n");

		eu_last = RT_LIST_PLAST_CIRC(edgeuse, eu);
		if (eu_last->eumate_p->vu_p->v_p != eu->vu_p->v_p)
			rt_bomb("nmg_ck_eu: edge and last-mate don't share vertex\n");

	} else {
		rt_bomb(strcat(errstr, "nmg_ck_eu() Bad edgeuse parent\n"));
	}

	NMG_CK_EDGE(eu->e_p);
	nmg_ck_e(eu, eu->e_p, errstr);

	NMG_CK_VERTEXUSE(eu->vu_p);
	nmg_ck_vu(&eu->l.magic, eu->vu_p, errstr);

	rt_free(errstr, "nmg_ck_eu error str");
}

/*
 *			N M G _ C K _ L G
 */
void
nmg_ck_lg(l, lg, str)
struct loop *l;
struct loop_g *lg;
char *str;
{
	char *errstr;
	errstr = rt_calloc(strlen(str)+128, 1, "nmg_ck_lg error str");
	(void)sprintf(errstr, "%sloop_g %8x\n", str, lg);

	NMG_CK_LOOP_G(lg);
	NMG_CK_LOOP(l);

	rt_free(errstr, "nmg_ck_lg error str");
}

/*
 *			N M G _ C K _ L
 */
void
nmg_ck_l(lu, l, str)
struct loopuse *lu;
struct loop *l;
char *str;
{
	char *errstr;
	errstr = rt_calloc(strlen(str)+128, 1, "nmg_ck_l error str");
	(void)sprintf(errstr, "%sloop %8x\n", str, l);

	NMG_CK_LOOP(l);
	NMG_CK_LOOPUSE(lu);

	if (l->lu_p != lu && l->lu_p->lumate_p != lu) rt_bomb(
		strcat(errstr, "nmg_ck_l() Cannot get from loop to loopuse\n"));

	if (l->lg_p) nmg_ck_lg(l, l->lg_p, errstr);

	rt_free(errstr, "");
}

/*
 *			N M G _ C K _ L U
 */
void
nmg_ck_lu(parent, lu, str)
long *parent;
struct loopuse *lu;
char *str;
{
	struct edgeuse *eu;
	struct vertexuse *vu;
	char *errstr;
	int l;
	int edgeuse_num=0;
	long	magic1;

	errstr = rt_calloc(strlen(str)+128, 1, "nmg_ck_lu error str");
	(void)sprintf(errstr, "%sloopuse %8x\n", str, lu);
	
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
	magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC) {
		vu = RT_LIST_FIRST( vertexuse, &lu->down_hd );
		NMG_CK_VERTEXUSE(vu);
		nmg_ck_vu(&lu->l.magic, vu, errstr);
	} else if (magic1 == NMG_EDGEUSE_MAGIC) {
		l = strlen(errstr);
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			NMG_CK_EDGEUSE(eu);
			(void)sprintf(&errstr[l], "%sedgeuse #%d (%8x)\n",
				errstr, edgeuse_num++, eu);
			nmg_ck_eu(&lu->l.magic, eu, errstr);
		}
	} else {
		rt_bomb(strcat(errstr, "nmg_ck_lu() Bad loopuse down pointer\n") );
	}
	rt_free(errstr, "nmg_ck_lu error str");
}

/*
 *			N M G _ C K _ F G
 */
void
nmg_ck_fg(f, fg, str)
struct face *f;
struct face_g_plane *fg;
char *str;
{
	char *errstr;
	errstr = rt_calloc(strlen(str)+128, 1, "nmg_ck_fg error str");
	(void)sprintf(errstr, "%sFace_g %8x\n", str, f);

	NMG_CK_FACE_G_PLANE(fg);
	if (fg->N[X]==0.0 && fg->N[Y]==0.0 && fg->N[Z]==0.0 && fg->N[H]!=0.0){
		(void)sprintf(&errstr[strlen(errstr)],
			"nmg_ck_fg() bad NMG plane equation %fX + %fY + %fZ = %f\n",
			fg->N[X], fg->N[Y], fg->N[Z], fg->N[H]);
	        rt_bomb(errstr);
	}

	rt_free(errstr, "nmg_ck_fg error str");
}

/* 
 *			N M G _ C K _ F
 */
void
nmg_ck_f(fu, f, str)
struct faceuse *fu;
struct face *f;
char *str;
{
	char *errstr;
	errstr = rt_calloc(strlen(str)+128, 1, "nmg_ck_f error str");
	(void)sprintf(errstr, "%sFace %8x\n", str, f);

	NMG_CK_FACE(f);
	NMG_CK_FACEUSE(fu);
	NMG_CK_FACE_G_PLANE(f->g.plane_p);
	if (f->fu_p != fu && f->fu_p->fumate_p != fu) rt_bomb(
		strcat(errstr,"nmg_ck_f() Cannot get from face to \"parent faceuse\"\n"));

	if (f->g.plane_p) nmg_ck_fg(f, f->g.plane_p, errstr);

	rt_free(errstr, "nmg_ck_f error str");
}

/*
 *			N M G _ C K _ F U
 */
void
nmg_ck_fu(s, fu, str)
struct shell *s;
struct faceuse *fu;
char *str;
{
	char *errstr;
	int l;
	int loop_number = 0;
	struct loopuse *lu;

	NMG_CK_FACEUSE(fu);
	NMG_CK_SHELL(s);

	errstr = rt_calloc(strlen(str)+128, 1, "nmg_ck_fu error str");
	(void)sprintf(errstr, "%sFaceuse %8x\n", str, fu);

	if (fu->s_p != s) rt_bomb(
		strcat(errstr, "nmg_ck_fu() faceuse child denies shell parentage\n") );

	if( RT_LIST_PNEXT_PLAST( faceuse, fu ) )
		rt_bomb( strcat(errstr, "nmg_ck_fu() Faceuse not lastward of next faceuse\n") );

	if( RT_LIST_PLAST_PNEXT( faceuse, fu ) )
		rt_bomb( strcat(errstr, "nmg_ck_fu() Faceuse not nextward from last faceuse\n") );

	NMG_CK_FACEUSE(fu->fumate_p);
	if (fu->fumate_p->fumate_p != fu) rt_bomb(
		strcat(errstr, "nmg_ck_fu() Faceuse not fumate of fumate\n") );

	if (fu->fumate_p->s_p != s) rt_bomb(
		strcat(errstr, "nmg_ck_fu() faceuse mates not in same shell\n") );

	nmg_ck_f(fu, fu->f_p, errstr);

	l = strlen(errstr);
	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		(void)sprintf(&errstr[l] , "%sloopuse #%d (%8x)\n", 
			errstr, loop_number++, lu);
		nmg_ck_lu(&fu->l.magic, lu, errstr);
	}
	rt_free(errstr, "nmg_ck_fu error str");
}

/*	N M G _ C K _ E G _ V E R T S
 *
 * Check if vertices from edgeuses using this edge geometry
 * actually lie on the edge geomatry.
 *
 * "eg" must be LSEG
 * returns number of vertices not on edge line
 */

int
nmg_ck_eg_verts( eg , tol )
CONST struct edge_g_lseg *eg;
CONST struct rt_tol *tol;
{
	struct rt_list *eu2;
	vect_t e_dir;
	int count=0;

	NMG_CK_EDGE_G_LSEG( eg );
	RT_CK_TOL( tol );

	VMOVE( e_dir , eg->e_dir );
	VUNITIZE( e_dir );

	for( RT_LIST_FOR( eu2 , rt_list , &eg->eu_hd2 ) )
	{
		struct edgeuse *eu;
		struct vertex *v1,*v2;
		struct vertex_g *vg1,*vg2;
		vect_t pt_to_vert;
		vect_t eg_to_vert;

		eu = RT_LIST_MAIN_PTR( edgeuse, eu2, l2 );

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
			rt_log( "vertex ( %g %g %g ) on eu to ( %g %g %g )\n", V3ARGS( vg1->coord ),
					V3ARGS( vg2->coord ) );
			rt_log( "\tnot on edge geometry: pt=( %g %g %g ), dir=( %g %g %g )\n",
					V3ARGS( eg->e_pt ), V3ARGS( eg->e_dir ) );
		}
	}

	return( count );
}

/*	N M G _ C K _ G E O M E T R Y
 *
 * Check that vertices actually lie on geometry for
 * faces and edges
 *
 * returns number of vertices that do not lie on geometry
 */
int
nmg_ck_geometry( m , tol )
CONST struct model *m;
CONST struct rt_tol *tol;
{
	struct nmg_ptbl g_tbl;
	int i;
	int count=0;

	NMG_CK_MODEL( m );
	RT_CK_TOL( tol );

	nmg_tbl( &g_tbl , TBL_INIT , (long *)NULL );

	nmg_edge_g_tabulate( &g_tbl , &m->magic );

	for( i=0 ; i<NMG_TBL_END( &g_tbl ) ; i++ )
	{
		long *ep;
		struct edge_g_lseg *eg;

		ep = NMG_TBL_GET( &g_tbl , i );
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

	nmg_tbl( &g_tbl , TBL_RST , (long *)NULL );

	nmg_face_tabulate( &g_tbl , &m->magic );

	for( i=0 ; i<NMG_TBL_END( &g_tbl ) ; i++ )
	{
		struct face *f;

		f = (struct face *)NMG_TBL_GET( &g_tbl , i );
		NMG_CK_FACE( f );

		count += nmg_ck_fg_verts( f->fu_p , f , tol );
	}

	nmg_tbl( &g_tbl , TBL_FREE , (long *)NULL );

	return( count );
}

/*
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
nmg_ck_face_worthless_edges( fu )
CONST struct faceuse	*fu;
{
	CONST struct loopuse	*lu;

	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		struct edgeuse	*eu;

		NMG_CK_LOOPUSE(lu);
		if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
			continue;
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			struct edgeuse		*neu;
			neu = RT_LIST_PNEXT_CIRC( edgeuse, eu );
			if( eu == neu )
				rt_bomb("nmg_ck_face_worthless_edges() lu has only one edge?\n");
			if( eu->vu_p == neu->vu_p )
				rt_bomb("nmg_ck_face_worthless_edges() edge runs between two copies of vu??\n");
			if( eu->vu_p->v_p == neu->vu_p->v_p )  {
#if 0
				nmg_pr_eu( eu, NULL );
				nmg_pr_eu( neu, NULL );
#endif
				rt_log("eu=x%x, neu=x%x, v=x%x\n", eu, neu, eu->vu_p->v_p);
				rt_log("eu=x%x, neu=x%x, v=x%x\n", eu->eumate_p, neu->eumate_p, eu->eumate_p->vu_p->v_p);
				rt_bomb("nmg_ck_face_worthless_edges() edge runs from&to same vertex\n");
				return 1;
			}
		}
	}
	return 0;
}

/*
 *			N M G _ C K _ L I S T
 *
 *  Generic rt_list doubly-linked list checker.
 *
 *  XXX Probably should be called rt_ck_list().
 */
void
nmg_ck_list( hd, str )
struct rt_list		*hd;
CONST char		*str;
{
	register struct rt_list	*cur;
	int	head_count = 0;

	cur = hd;
	do  {
		if( cur->magic == RT_LIST_HEAD_MAGIC )  head_count++;
		if( !cur->forw )  {
			rt_log("nmg_ck_list(%s) cur=x%x, cur->forw=x%x, hd=x%x\n",
				str, cur, cur->forw, hd );
			rt_bomb("nmg_ck_list() forw\n");
		}
		if( cur->forw->back != cur )  {
			rt_log("nmg_ck_list(%s) cur=x%x, cur->forw=x%x, cur->forw->back=x%x, hd=x%x\n",
				str, cur, cur->forw, cur->forw->back, hd );
			rt_bomb("nmg_ck_list() forw->back\n");
		}
		if( !cur->back )  {
			rt_log("nmg_ck_list(%s) cur=x%x, cur->back=x%x, hd=x%x\n",
				str, cur, cur->back, hd );
			rt_bomb("nmg_ck_list() back\n");
		}
		if( cur->back->forw != cur )  {
			rt_log("nmg_ck_list(%s) cur=x%x, cur->back=x%x, cur->back->forw=x%x, hd=x%x\n",
				str, cur, cur->back, cur->back->forw, hd );
			rt_bomb("nmg_ck_list() back->forw\n");
		}
		cur = cur->forw;
	} while( cur != hd );

	if( head_count != 1 )  {
		rt_log("nmg_ck_list(%s) head_count = %d, hd=x%x\n", str, head_count, hd);
		rt_bomb("nmg_ck_list() headless!\n");
	}
}




/*
 *			N M G _ C K _ L I S T _ M A G I C
 *
 *  rt_list doubly-linked list checker which checks the magic number for
 *	all elements in the linked list
 *  XXX Probably should be called rt_ck_list_magic().
 */
void
nmg_ck_list_magic( hd, str, magic )
CONST struct rt_list	*hd;
CONST char		*str;
CONST long		magic;
{
	register CONST struct rt_list	*cur;
	int	head_count = 0;

	cur = hd;
	do  {
		if( cur->magic == RT_LIST_HEAD_MAGIC )  {
			head_count++;
		} else if( cur->magic != magic ) {
			rt_log("nmg_ck_list(%s) cur magic=(%s)x%x, cur->forw magic=(%s)x%x, hd magic=(%s)x%x\n",
				str, rt_identify_magic(cur->magic), cur->magic,
				rt_identify_magic(cur->forw->magic), cur->forw->magic,
				rt_identify_magic(hd->magic), hd->magic);
			rt_bomb("nmg_ck_list_magic() cur->magic\n");
		}

		if( !cur->forw )  {
			rt_log("nmg_ck_list_magic(%s) cur=x%x, cur->forw=x%x, hd=x%x\n",
				str, cur, cur->forw, hd );
			rt_bomb("nmg_ck_list_magic() forw\n");
		}
		if( cur->forw->back != cur )  {
			rt_log("nmg_ck_list_magic(%s) cur=x%x, cur->forw=x%x, cur->forw->back=x%x, hd=x%x\n",
				str, cur, cur->forw, cur->forw->back, hd );
			rt_log(" cur=%s, cur->forw=%s, cur->forw->back=%s\n",
				rt_identify_magic(cur->magic),
				rt_identify_magic(cur->forw->magic),
				rt_identify_magic(cur->forw->back->magic) );
			rt_bomb("nmg_ck_list_magic() forw->back\n");
		}
		if( !cur->back )  {
			rt_log("nmg_ck_list_magic(%s) cur=x%x, cur->back=x%x, hd=x%x\n",
				str, cur, cur->back, hd );
			rt_bomb("nmg_ck_list_magic() back\n");
		}
		if( cur->back->forw != cur )  {
			rt_log("nmg_ck_list_magic(%s) cur=x%x, cur->back=x%x, cur->back->forw=x%x, hd=x%x\n",
				str, cur, cur->back, cur->back->forw, hd );
			rt_bomb("nmg_ck_list_magic() back->forw\n");
		}
		cur = cur->forw;
	} while( cur != hd );

	if( head_count != 1 )  {
		rt_log("nmg_ck_list_magic(%s) head_count = %d, hd=x%x\n", str, head_count, hd);
		rt_bomb("nmg_ck_list_magic() headless!\n");
	}
}






/*
 *			N M G _ C K _ L U E U
 *
 *	check all the edgeuses of a loopuse to make sure these children
 *	know who thier parent really is.
 */
void nmg_ck_lueu(cklu, s)
struct loopuse *cklu;
char *s;
{
	struct edgeuse *eu;

	if (RT_LIST_FIRST_MAGIC(&cklu->down_hd) == NMG_VERTEXUSE_MAGIC)
		rt_bomb("NMG nmg_ck_lueu.  I got a vertex loop!\n");

	eu = RT_LIST_FIRST(edgeuse, &cklu->down_hd);
	if (eu->l.back != &cklu->down_hd) {
		rt_bomb("nmg_ck_lueu first element in list doesn't point back to head\n");
	}

	for (RT_LIST_FOR(eu, edgeuse, &cklu->down_hd)) {
		NMG_CK_EDGEUSE(eu);
		if (eu->up.lu_p != cklu) {
			rt_log("nmg_cl_lueu() edgeuse of %s (going next) has lost proper parent\n", s);
			rt_bomb("nmg_ck_lueu\n");
		}
		if ((struct edgeuse *)eu->l.forw->back != eu) {
			rt_log("nmg_cl_lueu() %s next edge (%8x) doesn't point back to me (%8x)!\n", s, eu->l.forw, eu);
			nmg_pr_lu(cklu, NULL);
		}
		if ((struct edgeuse *)eu->l.back->forw != eu) {
			rt_log("nmg_cl_lueu() %s last edge (%8x) doesn't point forward to me (%8x)!\n", s, eu->l.forw, eu);
			nmg_pr_lu(cklu, NULL);
		}
	}

	cklu = cklu->lumate_p;

	eu = RT_LIST_FIRST(edgeuse, &cklu->down_hd);
	if (eu->l.back != &cklu->down_hd) {
		rt_bomb("nmg_ck_lueu first element in lumate list doesn't point back to head\n");
	}

	for (RT_LIST_FOR(eu, edgeuse, &cklu->down_hd)) {
		NMG_CK_EDGEUSE(eu);
		if (eu->up.lu_p != cklu) {
			rt_log("nmg_cl_lueu() edgeuse of %s (lumate going next) has lost proper parent\n", s);
			rt_bomb("nmg_ck_lueu\n");
		}
		if ((struct edgeuse *)eu->l.forw->back != eu) {
			rt_log("nmg_cl_lueu() %s next edge (%8x) doesn't point back to me (%8x)!\n", s, eu->l.forw, eu);
			nmg_pr_lu(cklu, NULL);
		}
		if ((struct edgeuse *)eu->l.back->forw != eu) {
			rt_log("nmg_cl_lueu() %s (lumate) back edge (%8x) doesn't point forward to me (%8x)!\n", s, eu->l.forw, eu);
			nmg_pr_lu(cklu, NULL);
		}
	}
}

/*
 *			N M G _ C H E C K _ R A D I A L
 *
 *	check to see if all radial uses of an edge (within a shell) are
 *	properly oriented with respect to each other.
 *
 *  XXX Note that this routine will not work properly if there are
 *  XXX dangling faces present.  It will report an erroneous error.
 *
 *	Return
 *	0	OK
 *	1	bad edgeuse mate
 *	2	unclosed space
 */
int
nmg_check_radial(eu, tol)
CONST struct edgeuse	*eu;
CONST struct rt_tol	*tol;
{
	char curr_orient;
	CONST struct edgeuse	*eur;
	CONST struct edgeuse	*eu1;
	CONST struct edgeuse	*eurstart;
	CONST struct faceuse	*fu;
	CONST struct shell	*s;
	pointp_t p, q;

	NMG_CK_EDGEUSE(eu);
	RT_CK_TOL(tol);
	s = nmg_find_s_of_eu(eu);
	NMG_CK_SHELL(s);

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		rt_log("nmg_check_radial(eu=x%x, tol)\n", eu);
	}

	eu1 = eu;

	/* If this eu is a wire, advance to first non-wire. */
	while( (fu = nmg_find_fu_of_eu(eu)) == (struct faceuse *)NULL )  {
		eu = eu->radial_p->eumate_p;
		if( eu == eu1 )  return 0;	/* wires all around */
	}

	curr_orient = fu->orientation;
	eur = eu->radial_p;
	eurstart = eur;

	/* skip the wire edges in the radial direction from eu. */
	while( (fu = nmg_find_fu_of_eu(eur)) == (struct faceuse *)NULL )  {
		eur = eur->eumate_p->radial_p;
		if( eur == eurstart )  return 0;
	}

	NMG_CK_EDGEUSE(eur);
	do {
		/*
		 *  Search until another edgeuse in this shell is found.
		 *  Continue search if it is a wire edge.
		 */
		while( nmg_find_s_of_eu((struct edgeuse *)eur) != s  ||
		       (fu = nmg_find_fu_of_eu(eur)) == (struct faceuse *)NULL
		)  {
			/* Advance to next eur */
			NMG_CK_EDGEUSE(eur->eumate_p);
			if (eur->eumate_p->eumate_p != eur) {
				p = eur->vu_p->v_p->vg_p->coord;
				q = eur->eumate_p->vu_p->v_p->vg_p->coord;
				rt_log("edgeuse mate has different mate %g %g %g -> %g %g %g\n",
					p[0], p[1], p[2], q[0], q[1], q[2]);
				nmg_pr_lu(eu->up.lu_p, (char *)NULL);
				nmg_pr_lu(eu->eumate_p->up.lu_p, (char *)NULL);
				rt_bomb("nmg_check_radial: bad edgeuse mate\n");
				/*return(1);*/
			}
			eur = eur->eumate_p->radial_p;
			NMG_CK_EDGEUSE(eur);
			if( eur == eurstart )  return 0;

			/* Can't check faceuse orientation parity for
			 * things from another shell;  parity is conserved
			 * only within faces from a single shell.
			 */
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
		    	char buf[80];

			p = eu1->vu_p->v_p->vg_p->coord;
			q = eu1->eumate_p->vu_p->v_p->vg_p->coord;
			rt_log("nmg_check_radial(): Radial orientation problem at edge %g %g %g -> %g %g %g\n",
				p[0], p[1], p[2], q[0], q[1], q[2]);
			rt_log("Problem Edgeuses: eu1=%8x, eur=%8x\n", eu1, eur);

			/* Plot the edge in yellow, & the loops */
			rt_g.NMG_debug |= DEBUG_PLOTEM;
			nmg_face_lu_plot( eu1->up.lu_p, eu1->vu_p,
				eu1->eumate_p->vu_p );
			nmg_face_lu_plot( eur->up.lu_p, eur->vu_p,
				eur->eumate_p->vu_p );

		    	sprintf(buf, "Orientation problem %g %g %g -> %g %g %g\n",
				p[0], p[1], p[2], q[0], q[1], q[2]);

		    	nmg_stash_model_to_file("radial.g", 
		    		nmg_find_model(&(fu->l.magic)), buf);

			nmg_pr_fu_around_eu( eu1, tol );

			rt_log("nmg_check_radial: unclosed space\n");
			return(2);
		}

		eu1 = eur->eumate_p;
		NMG_CK_LOOPUSE(eu1->up.lu_p);
		NMG_CK_FACEUSE(eu1->up.lu_p->up.fu_p);
		curr_orient = eu1->up.lu_p->up.fu_p->orientation;
		eur = eu1->radial_p;
	} while (eur != eurstart);
	return(0);
}

/*
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
nmg_ck_closed_surf(s, tol)
CONST struct shell	*s;
CONST struct rt_tol	*tol;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	int		status = 0;
	long		magic1;

	NMG_CK_SHELL(s);
	RT_CK_TOL(tol);
	for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		NMG_CK_FACEUSE(fu);
		for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
			NMG_CK_LOOPUSE(lu);
			magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
			if (magic1 == NMG_EDGEUSE_MAGIC) {
				/* Check status on all the edgeuses before quitting */
				for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
					if (nmg_check_radial(eu, tol))
						status = 1;
				}
				if( status )  {
					rt_log("nmg_ck_closed_surf(x%x), problem with loopuse x%x\n", s, lu);
					return 1;
				}
			} else if (magic1 == NMG_VERTEXUSE_MAGIC) {
				register struct vertexuse	*vu;
				vu = RT_LIST_FIRST( vertexuse, &lu->down_hd );
				NMG_CK_VERTEXUSE(vu);
				NMG_CK_VERTEX(vu->v_p);
			}
		}
	}
	return(0);
}

/*
 *			N M G _ C K _ C L O S E D _ R E G I O N
 *
 *  Check all the shells in a region for being closed.
 *
 *  Returns -
 *	 0	OK
 *	!0	status code from nmg_check_radial()
 */
int
nmg_ck_closed_region(r, tol)
CONST struct nmgregion	*r;
CONST struct rt_tol	*tol;
{
	CONST struct shell	*s;
	int		ret;

	NMG_CK_REGION(r);
	RT_CK_TOL(tol);
	for( RT_LIST_FOR( s, shell, &r->s_hd ) )  {
		ret = nmg_ck_closed_surf( s, tol );
		if( ret != 0 )  return(ret);
	}
	return(0);
}

/*	N M G _ C K _ V _ I N _ 2 F U S
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
nmg_ck_v_in_2fus( vp , fu1 , fu2 , tol )
CONST struct vertex *vp;
CONST struct faceuse *fu1;
CONST struct faceuse *fu2;
CONST struct rt_tol *tol;
{
	struct rt_vls str;
	struct faceuse *fu;
	struct vertexuse *vu;
	fastf_t dist1,dist2;
	int found1=0,found2=0;
	plane_t	n1, n2;

	NMG_CK_VERTEX( vp );
	NMG_CK_FACEUSE( fu1 );
	NMG_CK_FACEUSE( fu2 );
	RT_CK_TOL( tol );

	/* topology check */
	for( RT_LIST_FOR( vu , vertexuse , &vp->vu_hd ) )
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
		rt_vls_init( &str );
		rt_vls_printf( &str , "nmg_ck_v_in_2fus: vertex x%x not used in" , vp );
		if( !found1 )
			rt_vls_printf( &str , " faceuse x%x" , fu1 );
		if( !found2 )
			rt_vls_printf( &str , " faceuse x%x" , fu2 );
		rt_bomb( rt_vls_addr( &str ) );
	}

	/* geometry check */
	NMG_GET_FU_PLANE(n1, fu1);
	NMG_GET_FU_PLANE(n2, fu2);
	dist1 = DIST_PT_PLANE( vp->vg_p->coord , n1 );
	dist2 = DIST_PT_PLANE( vp->vg_p->coord , n2 );

	if( !NEAR_ZERO( dist1 , tol->dist ) || !NEAR_ZERO( dist2 , tol->dist ) )
	{
		rt_vls_init( &str );
		rt_vls_printf( &str , "nmg_ck_v_in_2fus: vertex x%x ( %g %g %g ) not in plane of" ,
				vp , V3ARGS( vp->vg_p->coord ) );
		if( !NEAR_ZERO( dist1 , tol->dist ) )
			rt_vls_printf( &str , " faceuse x%x (off by %g)" , fu1 , dist1 );
		if( !NEAR_ZERO( dist2 , tol->dist ) )
			rt_vls_printf( &str , " faceuse x%x (off by %g)" , fu2 , dist2 );
		rt_bomb( rt_vls_addr( &str ) );
	}

}
/*	N M G _ C K _ V S _ I N _ R E G I O N
 *
 *	Visits every vertex in the region and checks if the
 *	vertex coordinates are within tolerance of every face
 *	it is supposed to be in (according to the topology).
 *
 */

struct v_ck_state {
        char            *visited;
        struct nmg_ptbl *tabl;
	struct rt_tol	*tol;
};

static void
nmg_ck_v_in_fus( vp , state , first )
long			*vp;
genptr_t	        state;
int			first;
{
        register struct v_ck_state *sp = (struct v_ck_state *)state;
        register struct vertex  *v = (struct vertex *)vp;

        NMG_CK_VERTEX(v);
        /* If this vertex has been processed before, do nothing more */
        if( NMG_INDEX_FIRST_TIME(sp->visited, v) )
	{
		struct vertexuse *vu;
		struct faceuse *fu;

		for( RT_LIST_FOR( vu , vertexuse , &v->vu_hd ) )
		{
			fastf_t dist;

			fu = nmg_find_fu_of_vu( vu );
			if( fu )
			{
				plane_t		n;

				NMG_CK_FACEUSE( fu );
				if( fu->orientation != OT_SAME )
					continue;
				NMG_GET_FU_PLANE( n, fu );
				dist = DIST_PT_PLANE( v->vg_p->coord , n );
				if( !NEAR_ZERO( dist , sp->tol->dist ) )
				{
					rt_log( "ERROR - nmg_ck_vs_in_region: vertex x%x ( %g %g %g ) is %g from faceuse x%x\n" , 
						v , V3ARGS( v->vg_p->coord ) , dist , fu );
				}
			}
		}
	}
}

void
nmg_ck_vs_in_region( r , tol )
CONST struct nmgregion *r;
CONST struct rt_tol *tol;
{
	struct model			*m;
	struct v_ck_state		st;
	struct nmg_visit_handlers       handlers;
	struct nmg_ptbl			tab;

        NMG_CK_REGION(r);
	RT_CK_TOL( tol );
        m = r->m_p;
        NMG_CK_MODEL(m);

        st.visited = (char *)rt_calloc(m->maxindex+1, sizeof(char), "visited[]");
        st.tabl = &tab;
	st.tol = (struct rt_tol *)tol;

        (void)nmg_tbl( &tab, TBL_INIT, 0 );

        handlers = nmg_visit_handlers_null;             /* struct copy */
        handlers.vis_vertex = nmg_ck_v_in_fus;
        nmg_visit( &r->l.magic, &handlers, (genptr_t)&st );

	nmg_tbl( &tab , TBL_FREE , 0 );

        rt_free( (char *)st.visited, "visited[]");
}
