/*
 *			N M G _ C K . C
 *
 *  Validators and consistency checkers for NMG data structures
 *
 *  Authors -
 *	Lee A. Butler
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "nmg.h"

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

	NMG_CK_VERTEX(v);

	for( RT_LIST_FOR( vu, vertexuse, &v->vu_hd ) )  {
		NMG_CK_VERTEXUSE(vu);
		if (vu->v_p != v)
			rt_bomb("nmg_vvertex() a vertexuse in my list doesn't share my vertex\n");
	}
	if (v->vg_p) nmg_vvg(v->vg_p);
}

/* Verify vertex attributes */
void
nmg_vvua(vua)
struct vertexuse_a *vua;
{
	NMG_CK_VERTEXUSE_A(vua);
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
	NMG_CK_VERTEXUSE(vu);
	if (vu->up.magic_p != up_magic_p)  {
		rt_log("nmg_vvu() up is %s, s/b %s\n",
			rt_identify_magic( *vu->up.magic_p ),
			rt_identify_magic( *up_magic_p ) );
		rt_bomb("nmg_vvu() vertexuse denies parent\n");
	}

	if (!vu->l.forw)
		rt_bomb("nmg_vvu() vertexuse has null forw pointer\n");

	if( RT_LIST_FIRST_MAGIC( &vu->l ) != NMG_VERTEXUSE_MAGIC)
		rt_bomb("vertexuse forw is bad vertexuse\n");

	if (RT_LIST_PNEXT_PLAST(vertexuse,vu) != vu )
		rt_bomb("vertexuse not back of next vertexuse\n");

	nmg_vvertex(vu->v_p, vu);

	if (vu->vua_p) nmg_vvua(vu->vua_p);
}

/* Verify edgeuse attributes */
void
nmg_veua(eua)
struct edgeuse_a *eua;
{
	NMG_CK_EDGEUSE_A(eua);
}

/* Verify edge geometry */
void
nmg_veg(eg)
struct edge_g *eg;
{
	NMG_CK_EDGE_G(eg);
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

	if (!e->eu_p) rt_bomb("edge has null edgeuse pointer\n");

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
				rt_bomb("edgeuse mate does not have correct vertex\n");
		} else if (eu->vu_p->v_p == eup->eumate_p->vu_p->v_p) {
			if (eu->eumate_p->vu_p->v_p != eup->vu_p->v_p)
				rt_bomb("edgeuse does not have correct vertex\n");
		} else
			rt_bomb("edgeuse does not share vertex endpoint\n");

		eu = eu->eumate_p->radial_p;
	} while (eu != eup);

	if (!is_use)
		rt_bomb("Cannot get from edge to parent edgeuse\n");

	if (e->eg_p) nmg_veg(e->eg_p);
}

/*
 *			N M G _ V E U
 *
 *  Verify edgeuse
 */
void
nmg_veu(hp, up_magic_p)
struct rt_list	*hp;
long	*up_magic_p;
{
	struct edgeuse	*eu;
	struct edgeuse	*eunext;
	
	for( RT_LIST_FOR( eu, edgeuse, hp ) )  {
		NMG_CK_EDGEUSE(eu);

		if (eu->up.magic_p != up_magic_p)
			rt_bomb("edgeuse denies parentage\n");

		if (!eu->l.forw)
			rt_bomb("edgeuse has Null \"forw\" pointer\n");
		eunext = RT_LIST_PNEXT_CIRC( edgeuse, eu );
		if (eunext->l.magic != NMG_EDGEUSE_MAGIC)
			rt_bomb("edgeuse forw is bad edgeuse\n");
		if (RT_LIST_PLAST_CIRC(edgeuse,eunext) != eu )  {
		    if (eunext->l.back)
			rt_bomb("next edgeuse has back that points elsewhere\n");
		    else
			rt_bomb("next edgeuse has NULL back\n");
		}
		if (eu->eumate_p->l.magic != NMG_EDGEUSE_MAGIC)
			rt_bomb("edgeuse mate is bad edgeuse\n");
		else if (eu->eumate_p->eumate_p != eu)
			rt_bomb("edgeuse mate spurns edgeuse\n");

		if (eu->radial_p->l.magic != NMG_EDGEUSE_MAGIC)
			rt_bomb("edgeuse radial is bad edgeuse\n");
		else if (eu->radial_p->radial_p != eu)
			rt_bomb("edgeuse radial denies knowing edgeuse\n");

		nmg_vedge(eu->e_p, eu);
		
		if (eu->eua_p) nmg_veua(eu->eua_p);

		switch (eu->orientation) {
		case OT_NONE	: break;
		case OT_SAME	: break;
		case OT_OPPOSITE: break;
		case OT_UNSPEC	: break;
		default		: rt_bomb("unknown loopuse orintation\n");
					break;
		}

		nmg_vvu(eu->vu_p, eu);
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
			rt_bomb("loop geom min_pt greater than max_pt\n");
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
	struct loopuse *lu;

	NMG_CK_LOOP(l);
	NMG_CK_LOOPUSE(lup);

	if (!l->lu_p) rt_bomb("loop has null loopuse pointer\n");

#if 0
	for (lu=lup ; lu && lu != l->lu_p && lu->next != lup ; lu = lu->next);
	
	if (l->lu_p != lu)
		for (lu=lup->lumate_p ; lu && lu != l->lu_p && lu->next != lup->lumate_p ; lu = lu->next);

	if (l->lu_p != lu) rt_bomb("can't get to parent loopuse from loop\n");
#endif

	if (l->lg_p) nmg_vlg(l->lg_p);
}

/* Verify loop attributes */
void
nmg_vlua(lua)
struct loopuse_a *lua;
{
	NMG_CK_LOOPUSE_A(lua);
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
			rt_bomb("loopuse denies parentage\n");
		}

		if (!lu->l.forw)
			rt_bomb("loopuse has null forw pointer\n");
		else if (RT_LIST_PNEXT_PLAST(loopuse,lu) != lu )
			rt_bomb("forw loopuse has back pointing somewhere else\n");

		if (!lu->lumate_p)
			rt_bomb("loopuse has null mate pointer\n");

		if (lu->lumate_p->l.magic != NMG_LOOPUSE_MAGIC)
			rt_bomb("loopuse mate is bad loopuse\n");

		if (lu->lumate_p->lumate_p != lu)
			rt_bomb("lumate spurns loopuse\n");

		switch (lu->orientation) {
		case OT_NONE	: break;
		case OT_SAME	: break;
		case OT_OPPOSITE	: break;
		case OT_UNSPEC	: break;
		default		: rt_bomb("unknown loopuse orintation\n");
					break;
		}
		if (lu->lumate_p->orientation != lu->orientation)
			rt_bomb("loopuse and mate have different orientation\n");

		if (!lu->l_p)
			rt_bomb("loopuse has Null loop pointer\n");
		else {
			nmg_vloop(lu->l_p, lu);
		}

		if (lu->lua_p) nmg_vlua(lu->lua_p);

		if( RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC)
			nmg_veu( &lu->down_hd, lu);
		else if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC)
			nmg_vvu(RT_LIST_FIRST(vertexuse,&lu->down_hd), lu);
		else
			rt_bomb("nmg_vlu bad magic\n");
	}
}

/*
 *			N M G _ V F G
 *
 *  Verify face geometry
 */
void
nmg_vfg(fg)
struct face_g *fg;
{
	int i;

	NMG_CK_FACE_G(fg);

	for (i=0 ; i < ELEMENTS_PER_PT ; ++i)
		if (fg->min_pt[i] > fg->max_pt[i])
			rt_bomb("face geom min_pt greater than max_pt\n");

	if (fg->N[X]==0.0 && fg->N[Y]==0.0 && fg->N[Z]==0.0 &&
	    fg->N[H]!=0.0) {
		rt_log("bad NMG plane equation %fX + %fY + %fZ = %f\n",
			fg->N[X], fg->N[Y], fg->N[Z], fg->N[H]);
		rt_bomb("Bad NMG geometry\n");
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
	struct faceuse *fu;

	NMG_CK_FACE(f);
	NMG_CK_FACEUSE(fup);

	/* make sure we can get back to the parent faceuse from the face */
	if (!f->fu_p) rt_bomb("face has null faceuse pointer\n");

#if 0
	for (fu = fup; fu && fu != f->fu_p && fu->forw != fup; fu = fu->forw);

	if (f->fu_p != fu) rt_bomb("can't get to parent faceuse from face\n");
#endif
	
	if (f->fg_p) nmg_vfg(f->fg_p);
}

/* Verify faceuse attributes */
void
nmg_vfua(fua)
struct faceuse_a *fua;
{
	NMG_CK_FACEUSE_A(fua);
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

	for( RT_LIST_FOR( fu, faceuse, hp ) )  {
		NMG_CK_FACEUSE(fu);
		if (fu->s_p != s) {
			rt_log("faceuse claims shell parent (%8x) instead of (%8x)\n",
				fu->s_p, s);
			rt_bomb("nmg_vfu\n");
		}

		if (!fu->l.forw) {
			rt_bomb("faceuse forw is NULL\n");
		} else if (fu->l.forw->back != (struct rt_list *)fu) {
			rt_bomb("faceuse->forw->back != faceuse\n");
		}

		if (!fu->fumate_p)
			rt_bomb("null faceuse fumate_p pointer\n");

		if (fu->fumate_p->l.magic != NMG_FACEUSE_MAGIC)
			rt_bomb("faceuse mate is bad faceuse ptr\n");

		if (fu->fumate_p->fumate_p != fu)
			rt_bomb("faceuse mate spurns faceuse!\n");

		switch (fu->orientation) {
		case OT_NONE	: break;
		case OT_SAME	: if (fu->fumate_p->orientation != OT_OPPOSITE)
					rt_bomb("faceuse of \"SAME\" orientation has mate that is not \"OPPOSITE\" orientation");
				break;
		case OT_OPPOSITE:  if (fu->fumate_p->orientation != OT_SAME)
					rt_bomb("faceuse of \"OPPOSITE\" orientation has mate that is not \"SAME\" orientation");
				break;
		case OT_UNSPEC	: break;
		default		: rt_bomb("unknown faceuse orintation\n"); break;
		}

		if (fu->fua_p) nmg_vfua(fu->fua_p);
		
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

	for( RT_LIST_FOR( s, shell, hp ) )  {
		NMG_CK_SHELL(s);
		if (s->r_p != r) {
			rt_log("shell's r_p (%8x) doesn't point to parent (%8x)\n",
				s->r_p, r);
			rt_bomb("nmg_vshell");
		}

		if (!s->l.forw) {
			rt_bomb("nmg_vshell: Shell's forw ptr is null\n");
		} else if (s->l.forw->back != (struct rt_list *)s) {
			rt_log("forw shell's back(%8x) is not me (%8x)\n",
				s->l.forw->back, s);
			rt_bomb("nmg_vshell\n");
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
				rt_log("Bad min_pt/max_pt for shell(%8x)'s extent\n");
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
				rt_bomb("");
			}
		}

		nmg_vfu( &s->fu_hd, s);
		nmg_vlu( &s->lu_hd, s);
		nmg_veu( &s->eu_hd, s);
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
			rt_bomb("nmg_vregion\n");
		}
		if (r->ra_p) {
			NMG_CK_REGION_A(r->ra_p);
		}

		nmg_vshell( &r->s_hd, r);

		if( RT_LIST_PNEXT_PLAST(nmgregion, r) != r )  {
			rt_bomb("forw nmgregion's back is not me\n");
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
	if (m->ma_p) {
		NMG_CK_MODEL_A(m->ma_p);
	}
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
		strcat(errstr, "Edge denies edgeuse parentage\n"));

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
		strcat(errstr, "Vertexuse denies parentage\n"));

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
		strcat(errstr, "Edgeuse child denies parentage\n"));

	if (*eu->eumate_p->up.magic_p != *eu->up.magic_p) rt_bomb(
		strcat(errstr, "eumate has differnt kind of parent\n"));
	if (*eu->up.magic_p == NMG_SHELL_MAGIC) {
		if (eu->eumate_p->up.s_p != eu->up.s_p) rt_bomb(
			strcat(errstr, "eumate in different shell\n"));

		eur = eu->radial_p;
		while (eur && eur != eu && eur != eu->eumate_p)
			eur = eur->eumate_p->radial_p;

		if (!eur) rt_bomb(strcat(errstr,
			"Radial trip from eu ended in null pointer\n"));


	} else if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		if (eu->eumate_p->up.lu_p != eu->up.lu_p->lumate_p) rt_bomb(
			strcat(errstr, "eumate not in same loop\n"));

		eur = eu->radial_p;
		while (eur && eur != eu->eumate_p && eur != eu)
			eur = eur->eumate_p->radial_p;

		if (!eur) rt_bomb(
			strcat(errstr, "radial path leads to null ptr\n"));
		else if (eur == eu) rt_bomb(
			strcat(errstr, "Never saw eumate\n"));

		eu_next = RT_LIST_PNEXT_CIRC(edgeuse, eu);
		if (eu_next->vu_p->v_p != eu->eumate_p->vu_p->v_p)
			rt_bomb("nmg_ck_eu: next and mate don't share vertex\n");

		eu_last = RT_LIST_PLAST_CIRC(edgeuse, eu);
		if (eu_last->eumate_p->vu_p->v_p != eu->vu_p->v_p)
			rt_bomb("nmg_ck_eu: edge and last-mate don't share vertex\n");

	} else {
		rt_bomb(strcat(errstr, "Bad edgeuse parent\n"));
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
		strcat(errstr, "Cannot get from loop to loopuse\n"));

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
		strcat(errstr, "loopuse child denies parentage\n") );

	/* check the parent of lu and lumate WRT each other */
	NMG_CK_LOOPUSE(lu->lumate_p);
	if (*lu->lumate_p->up.magic_p != *lu->up.magic_p) rt_bomb(
		strcat(errstr,"loopuse mate has different kind of parent\n"));

	if (*lu->up.magic_p == NMG_SHELL_MAGIC) {
		if (lu->lumate_p->up.s_p != lu->up.s_p) rt_bomb(
			strcat(errstr, "Lumate not in same shell\n") );
	} else if (*lu->up.magic_p == NMG_FACEUSE_MAGIC) {
		if (lu->lumate_p->up.fu_p != lu->up.fu_p->fumate_p) rt_bomb(
			strcat(errstr, "lumate part of different face\n"));
	} else {
		rt_bomb(strcat(errstr, "Bad loopuse parent type\n"));
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
		rt_bomb(strcat(errstr, "Bad loopuse down pointer\n") );
	}
	rt_free(errstr, "nmg_ck_lu error str");
}

/*
 *			N M G _ C K _ F G
 */
void
nmg_ck_fg(f, fg, str)
struct face *f;
struct face_g *fg;
char *str;
{
	char *errstr;
	errstr = rt_calloc(strlen(str)+128, 1, "nmg_ck_fg error str");
	(void)sprintf(errstr, "%sFace_g %8x\n", str, f);

	NMG_CK_FACE_G(fg);
	if (fg->N[X]==0.0 && fg->N[Y]==0.0 && fg->N[Z]==0.0 && fg->N[H]!=0.0){
		(void)sprintf(&errstr[strlen(errstr)],
			"bad NMG plane equation %fX + %fY + %fZ = %f\n",
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
	NMG_CK_FACE_G(f->fg_p);
	if (f->fu_p != fu && f->fu_p->fumate_p != fu) rt_bomb(
		strcat(errstr,"Cannot get from face to \"parent faceuse\"\n"));

	if (f->fg_p) nmg_ck_fg(f, f->fg_p, errstr);

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
		strcat(errstr, "faceuse child denies shell parentage\n") );

	if( RT_LIST_PNEXT_PLAST( faceuse, fu ) )
		rt_bomb( strcat(errstr, "Faceuse not lastward of next faceuse\n") );

	if( RT_LIST_PLAST_PNEXT( faceuse, fu ) )
		rt_bomb( strcat(errstr, "Faceuse not nextward from last faceuse\n") );

	NMG_CK_FACEUSE(fu->fumate_p);
	if (fu->fumate_p->fumate_p != fu) rt_bomb(
		strcat(errstr, "Faceuse not fumate of fumate\n") );

	if (fu->fumate_p->s_p != s) rt_bomb(
		strcat(errstr, "faceuse mates not in same shell\n") );

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

/*
 *			N M G _ C K _ L I S T
 *
 *  Generic rt_list doubly-linked list checker.
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
		if( cur->forw->back != cur )  {
			rt_log("nmg_ck_list(%s) cur=x%x, cur->forw=x%x, cur->forw->back=x%x\n",
				str, cur, cur->forw, cur->forw->back );
			rt_bomb("nmg_ck_list() forw\n");
		}
		if( cur->back->forw != cur )  {
			rt_log("nmg_ck_list(%s) cur=x%x, cur->back=x%x, cur->back->forw=x%x\n",
				str, cur, cur->back, cur->back->forw );
			rt_bomb("nmg_ck_list() back\n");
		}
		cur = cur->forw;
	} while( cur != hd );

	if( head_count != 1 )  {
		rt_log("nmg_ck_list(%s) head_count = %d\n", head_count);
		rt_bomb("headless!\n");
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
			rt_bomb("nmg_ck_lueu");
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
			rt_bomb("nmg_ck_lueu");
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
