/*
 *			N M G _ M I S C . C
 *
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
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
#include "nmg.h"

void nmg_pr_m(m)
struct model *m;
{
	struct nmgregion *r;
	rt_log("MODEL %8x\n", m);
	if (!m || m->magic != NMG_MODEL_MAGIC) {
		rt_log("bad model magic\n");
		return;
	}
	rt_log("%8x ma_p\n", m->ma_p);

	rt_log("%8x r_p\n", m->r_p);
	r = m->r_p;
	do {
		nmg_pr_r(r, (char *)NULL);
		r = r->next;
	} while (r != m->r_p);

	return;
}
static char padstr[32];
#define MKPAD(_h) { \
	if (!_h) { _h = padstr; bzero(h, sizeof(padstr)); } \
	else { if (strlen(_h) < sizeof(padstr)-4) (void)strcat(_h, "   "); } }

#define Return	{ h[strlen(h)-3] = '\0'; return; }

void nmg_pr_r(r, h)
struct nmgregion *r;
char *h;
{
	struct shell *s;

	rt_log("REGION %8x\n", r);

	MKPAD(h);

	if (!r || r->magic != NMG_REGION_MAGIC) {
		rt_log("bad region magic\n");
		Return;
	}

	rt_log("%8x m_p\n", r->m_p);
	rt_log("%8x next\n", r->next);
	rt_log("%8x last\n", r->last);
	rt_log("%8x ra_p\n", r->ra_p);
	rt_log("%8x s_p\n", r->s_p);

	if (r->s_p) {
		s = r->s_p;
		do {
			nmg_pr_s(s, h);
			s = s->next;
		} while (s != r->s_p);
	}
	Return;
}
void nmg_pr_sa(sa, h)
struct shell_a *sa;
char *h;
{
	MKPAD(h);
	NMG_CK_SHELL_A(sa);
	
	rt_log("%sSHELL_A %8x\n", h, sa);
	rt_log("%s%f %f %f Min\n", h, sa->min_pt[X], sa->min_pt[Y],
		sa->min_pt[Z]);
	rt_log("%s%f %f %f Max\n", h, sa->max_pt[X], sa->max_pt[Y],
		sa->max_pt[Z]);

	Return;
}
void nmg_pr_lg(lg, h)
struct loop_g *lg;
char *h;
{
	MKPAD(h);
	NMG_CK_LOOP_G(lg);
	
	rt_log("%sLOOP_G %8x\n", h, lg);
	rt_log("%s%f %f %f Min\n", h, lg->min_pt[X], lg->min_pt[Y],
		lg->min_pt[Z]);
	rt_log("%s%f %f %f Max\n", h, lg->max_pt[X], lg->max_pt[Y],
		lg->max_pt[Z]);

	Return;
}
void nmg_pr_fg(fg, h)
struct face_g *fg;
char *h;
{
	MKPAD(h);
	NMG_CK_FACE_G(fg);
	
	rt_log("%sFACE_G %8x\n", h, fg);
	rt_log("%s%f %f %f Min\n", h, fg->min_pt[X], fg->min_pt[Y],
		fg->min_pt[Z]);
	rt_log("%s%f %f %f Max\n", h, fg->max_pt[X], fg->max_pt[Y],
		fg->max_pt[Z]);

	rt_log("%s%fX + %fY + %fZ = %f\n", h, fg->N[0], fg->N[1],
		fg->N[2], fg->N[3]);

	Return;
}

void nmg_pr_s(s, h)
struct shell *s;
char *h;
{

	MKPAD(h);
	NMG_CK_SHELL(s);
	
	rt_log("%sSHELL %8x\n", h, s);
	if (!s || s->magic != NMG_SHELL_MAGIC) {
		rt_log ("bad magic!\n");
		Return;
	}
	rt_log("%s%8x r_p\n", h, s->r_p);
	rt_log("%s%8x next\n", h, s->next );
	rt_log("%s%8x last\n", h, s->last );
	rt_log("%s%8x sa_p\n", h, s->sa_p );
	if (s->sa_p)
		nmg_pr_sa(s->sa_p, h);
	
	rt_log("%s%8x fu_p\n", h, s->fu_p);
	rt_log("%s%8x lu_p\n", h, s->lu_p);
	rt_log("%s%8x eu_p\n", h, s->eu_p);
	rt_log("%s%8x vu_p\n", h, s->vu_p);

	if (s->fu_p) {
		struct faceuse *fu = s->fu_p;
		do {
			nmg_pr_fu(fu, h);
			fu = fu->next;
		} while (fu != s->fu_p);
	}
	if (s->lu_p) {
		struct loopuse *lu = s->lu_p;
		do {
			nmg_pr_lu(lu, h);
			lu = lu->next;
		} while (lu != s->lu_p);
	}
	if (s->eu_p) {
		struct edgeuse *eu = s->eu_p;
		do {
			nmg_pr_eu(eu, h);
			eu = eu->next;
		} while (eu != s->eu_p);
	}
	if (s->vu_p)
		nmg_pr_vu(s->vu_p, h);

	Return;
}
void nmg_pr_f(f, h)
struct face *f;
char *h;
{
	MKPAD(h);
	NMG_CK_FACE(f);

	rt_log("%sFACE %8x\n", h, f);
	rt_log("%s%8x fu_p\n", h, f->fu_p);
	rt_log("%s%8x fg_p\n", h, f->fg_p);
	if (f->fg_p)
		nmg_pr_fg(f->fg_p, h);

	Return;
}
void nmg_pr_fu(fu, h)
struct faceuse *fu;
char *h;
{
	struct loopuse *lu;
	MKPAD(h);
	NMG_CK_FACEUSE(fu);

	rt_log("%sFACEUSE %8x\n", h, fu);

	if (!fu || fu->magic != NMG_FACEUSE_MAGIC) {
		rt_log("bad faceuse magic\n");
		Return;
	}
	
	rt_log("%s%8x s_p\n", h, fu->s_p);
	rt_log("%s%8x next\n", h, fu->next);
	rt_log("%s%8x last\n", h, fu->last);
	rt_log("%s%8x fumate_p\n", h, fu->fumate_p);
	rt_log("%s%8x orientation\n", h, fu->orientation);
	rt_log("%s%8x fua_p\n", h, fu->fua_p);
	rt_log("%s%8x lu_p\n", h, fu->lu_p);

	rt_log("%s%8x f_p\n", h, fu->f_p);
	nmg_pr_f(fu->f_p, h);

	lu = fu->lu_p;
	do {
		nmg_pr_lu(lu, h);
		lu = lu->next;
	} while (lu != fu->lu_p);
	
	Return;
}
void nmg_pr_l(l, h)
struct loop *l;
char *h;
{
	MKPAD(h);
	NMG_CK_LOOP(l);

	rt_log("%sLOOP %8x\n", h, l);
	if (!l || l->magic != NMG_LOOP_MAGIC) {
		rt_log("bad loop magic\n");
		Return;
	}
	rt_log("%s%8x lu_p\n", h, l->lu_p);
	rt_log("%s%8x lg_p\n", h, l->lg_p);
	if (l->lg_p)
		nmg_pr_lg(l->lg_p, h);

	Return;
}
void nmg_pr_lu(lu, h)
struct loopuse *lu;
char *h;
{
	struct edgeuse *eu;
	
	MKPAD(h);
	NMG_CK_LOOPUSE(lu);

	rt_log("%sLOOPUSE %8x\n", h, lu);
	if (!lu || lu->magic != NMG_LOOPUSE_MAGIC) {
		rt_log("bad loopuse magic\n");
		Return;
	}

	switch (*lu->up.magic_p) {
	case NMG_SHELL_MAGIC	: rt_log("%s%8x up.s_p\n", h, lu->up.s_p);
					break;
	case NMG_FACEUSE_MAGIC	: rt_log("%s%8x up.fu_p\n", h, lu->up.fu_p);
					break;
	default			: rt_log("Bad loopuse parent magic\n");
					Return;
	}

	rt_log("%s%8x next\n", h, lu->next);
	rt_log("%s%8x last\n", h, lu->last);
	rt_log("%s%8x lumate_p\n", h, lu->lumate_p);
	rt_log("%s%8x lua_p\n", h, lu->lua_p);
	rt_log("%s%8x l_p\n", h, lu->l_p);
	if (lu->l_p)
		nmg_pr_l(lu->l_p, h);


	if (*lu->down.magic_p == NMG_VERTEXUSE_MAGIC) {
		rt_log("%s%8x down.vu_p\n", h, lu->down.vu_p);
		nmg_pr_vu(lu->down.vu_p, h);
	}
	else if (*lu->down.magic_p == NMG_EDGEUSE_MAGIC) {
		rt_log("%s%8x down.eu_p\n", h, lu->down.eu_p);
		eu = lu->down.eu_p;
		do {
			nmg_pr_eu(eu, h);
			eu = eu->next;
		} while (eu != lu->down.eu_p);
	}
	else
		rt_log("bad loopuse child magic\n");

	Return;
}
void nmg_pr_e(e, h)
struct edge *e;
char *h;
{
	MKPAD(h);
	NMG_CK_EDGE(e);

	rt_log("%sEDGE %8x\n", h, e);
	if (!e || e->magic != NMG_EDGE_MAGIC) {
		rt_log("bad edge magic\n");
		Return;
	}
	rt_log("%s%8x eu_p\n", h, e->eu_p);
	rt_log("%s%8x eg_p\n", h, e->eg_p);
	
	Return;
}

void nmg_pr_eu(eu, h)
struct edgeuse *eu;
char *h;
{
	MKPAD(h);
	NMG_CK_EDGEUSE(eu);

	rt_log("%sEDGEUSE %8x\n", h, eu);
	if (!eu || eu->magic != NMG_EDGEUSE_MAGIC) {
		rt_log("bad edgeuse magic\n");
		Return;
	}

	switch (*eu->up.magic_p) {
	case NMG_SHELL_MAGIC	: rt_log("%s%8x up.s_p\n", h, eu->up.s_p);
				break;
	case NMG_LOOPUSE_MAGIC	: rt_log("%s%8x up.lu_p\n", h, eu->up.lu_p);
				break;
	default			: rt_log("bad edgeuse parent magic\n");
				Return;
	}
	rt_log("%s%8x next\n", h, eu->next);
	rt_log("%s%8x last\n", h, eu->last);
	rt_log("%s%8x eumate_p\n", h, eu->eumate_p);
	rt_log("%s%8x radial_p\n", h, eu->radial_p);
	rt_log("%s%8x eua_p\n", h, eu->eua_p);
	rt_log("%s%8x orientation\n", h, eu->orientation);
	rt_log("%s%8x e_p\n", h, eu->e_p);
	rt_log("%s%8x vu_p\n", h, eu->vu_p);
	nmg_pr_e(eu->e_p, h);
	nmg_pr_vu(eu->vu_p, h);

	Return;
}
void nmg_pr_vg(vg, h)
struct vertex_g *vg;
char *h;
{
	MKPAD(h);
	NMG_CK_VERTEX_G(vg);

	rt_log("%sVERTEX_G %8x\n", h, vg);
	if (!vg || vg->magic != NMG_VERTEX_G_MAGIC) {
		rt_log("bad vertex_g magic\n");
		Return;
	}
	rt_log("%s%f coord[X]\n", h, vg->coord[X]);
	rt_log("%s%f coord[Y]\n", h, vg->coord[Y]);
	rt_log("%s%f coord[Z]\n", h, vg->coord[Z]);
	rt_log("%s%8d orientation\n", h, vg->orientation);

	Return;
}
void nmg_pr_v(v, h)
struct vertex *v;
char *h;
{
	MKPAD(h);
	NMG_CK_VERTEX(v);

	rt_log("%sVERTEX %8x\n", h, v);
	if (!v || v->magic != NMG_VERTEX_MAGIC) {
		rt_log("bad vertex magic\n");
		Return;
	}
	rt_log("%s%8x vu_p\n", h, v->vu_p);
	rt_log("%s%8x vg_p\n", h, v->vg_p);
	if (v->vg_p)
		nmg_pr_vg(v->vg_p, h);

	Return;
}

void nmg_pr_vu(vu, h)
struct vertexuse *vu;
char *h;
{
	MKPAD(h);
	NMG_CK_VERTEXUSE(vu);

	rt_log("%sVERTEXUSE %8x\n", h, vu);
	if (!vu || vu->magic != NMG_VERTEXUSE_MAGIC) {
		rt_log("bad vertexuse magic\n");
		Return;
	}

	switch (*vu->up.magic_p) {
	case NMG_SHELL_MAGIC	: rt_log("%s%8x up.s_p\n", h, vu->up.s_p); break;
	case NMG_LOOPUSE_MAGIC	: rt_log("%s%8x up.lu_p\n", h, vu->up.lu_p); break;
	case NMG_EDGEUSE_MAGIC	: rt_log("%s%8x up.eu_p\n", h, vu->up.eu_p); break;
	default			: rt_log("bad vertexuse parent magic\n"); 
				Return;
	}
	rt_log("%s%8x next\n", h, vu->next);
	rt_log("%s%8x last\n", h, vu->last);
	rt_log("%s%8x vua_p\n", h, vu->vua_p);
	rt_log("%s%8x v_p\n", h, vu->v_p);
	nmg_pr_v(vu->v_p, h);
	
	Return;
}



static nmg_pl_lu(fp, lu, b, R, G, B)
FILE *fp;
struct loopuse *lu;
struct nmg_ptbl *b;
unsigned char R, G, B;
{
	struct edgeuse *eu;	
	struct vertex_g *vg;

	NMG_CK_LOOPUSE(lu);
	if (nmg_tbl(b, TBL_LOC, &lu->l_p->magic) >= 0) return;
	(void)nmg_tbl(b, TBL_INS, &lu->l_p->magic);

	if (*lu->down.magic_p == NMG_VERTEXUSE_MAGIC) {
		vg = lu->down.vu_p->v_p->vg_p;
		NMG_CK_VERTEX_G(vg);
		pd_3point(fp, vg->coord[X], vg->coord[Y], vg->coord[Z]);

	} else if (*lu->down.magic_p == NMG_EDGEUSE_MAGIC) {
		eu = lu->down.eu_p;

		pl_color(fp, R, G, B);
#if 1
		do {
			NMG_CK_EDGEUSE(eu);
			NMG_CK_EDGE(eu->e_p);
			NMG_CK_VERTEXUSE(eu->vu_p);
			NMG_CK_VERTEX(eu->vu_p->v_p);
			vg = eu->vu_p->v_p->vg_p;
			NMG_CK_VERTEX_G(vg);

			if (nmg_tbl(b, TBL_LOC, &eu->e_p->magic) < 0) {
				struct vertex_g *vg2;
				pl_color(fp, R, G, B);
				vg2 = eu->eumate_p->vu_p->v_p->vg_p;
				pd_3line(fp,
				vg->coord[X], vg->coord[Y], vg->coord[Z],
				vg2->coord[X], vg2->coord[Y], vg2->coord[Z]);

				pl_color(fp, 255, 255, 255);
				pd_3point(fp, vg->coord[X], vg->coord[Y],
					vg->coord[Z]);
				pd_3point(fp, vg2->coord[X], vg2->coord[Y],
					vg2->coord[Z]);
			}
			eu = eu->next;
		} while (eu != lu->down.eu_p);
#else
		{int cont = 0;
		do {
			NMG_CK_EDGEUSE(eu);
			NMG_CK_EDGE(eu->e_p);
			NMG_CK_VERTEXUSE(eu->vu_p);
			NMG_CK_VERTEX(eu->vu_p->v_p);
			vg = eu->vu_p->v_p->vg_p;
			NMG_CK_VERTEX_G(vg);

			if (nmg_tbl(b, TBL_LOC, &eu->e_p->magic) < 0) {
				if (!cont) {
					vg = eu->vu_p->v_p->vg_p;
					NMG_CK_VERTEX_G(vg);
					pd_3move(fp, vg->coord[X],
						vg->coord[Y], vg->coord[Z]);
				}
				vg = eu->eumate_p->vu_p->v_p->vg_p;
				NMG_CK_VERTEX_G(vg);
				pd_3cont(fp, vg->coord[X], vg->coord[Y],
					vg->coord[Z]);

				cont = 1;
				(void)nmg_tbl(b, TBL_INS, &eu->e_p->magic);
			} else {
				cont = 0;
			}
			eu = eu->next;
		} while (eu != lu->down.eu_p);
		}
#endif


	}
}

static nmg_pl_fu(fp, fu, b, R, G, B)
FILE *fp;
struct faceuse *fu;
struct nmg_ptbl *b;
unsigned char R, G, B;
{
	struct loopuse *lu;

	NMG_CK_FACEUSE(fu);
	if (nmg_tbl(b, TBL_LOC, &fu->f_p->magic) >= 0) return;
	(void)nmg_tbl(b, TBL_INS, &fu->f_p->magic);

	lu = fu->lu_p;
	do {
		nmg_pl_lu(fp, lu, b, R, G, B);
		lu = lu->next;
	} while (lu != fu->lu_p);
}

void nmg_pl_s(fp, s)
FILE *fp;
struct shell *s;
{
	struct vertex_g *vg1, *vg2;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct nmg_ptbl b;	

	NMG_CK_SHELL(s);

	/* get space for list of items processed */
	(void)nmg_tbl(&b, TBL_INIT, (long *)0);	

	if (s->fu_p) {
		NMG_CK_FACEUSE(s->fu_p);
		fu = s->fu_p;
		do {
#if 0
			static unsigned color = 0xff;
#define Rcolor(_i)	(((_i&0x01)<<7)+32)
#define Gcolor(_i)	(((_i&0x02)<<6)+32)
#define Bcolor(_i)	(((_i&0x04)<<5)+32)
			nmg_pl_fu(fp, fu, &b, Rcolor(color), Gcolor(color), Bcolor(color));
			--color;
#else
			nmg_pl_fu(fp, fu, &b, 100, 100, 170);
#endif
			if (fu->next != s->fu_p &&
			    fu->next->f_p == fu->f_p)
			    	fu = fu->next->next;
			else
				fu = fu->next;
		} while (fu != s->fu_p);
	}

	if (s->lu_p) {
		NMG_CK_LOOPUSE(s->lu_p);
		lu = s->fu_p->lu_p;
		do {
			nmg_pl_lu(fp, lu, &b, 255, 0, 0);

			if (lu->next != s->lu_p &&
			    lu->next->l_p == lu->l_p)
			    	lu = lu->next->next;
			else
				lu = lu->next;
		} while (lu != s->fu_p->lu_p);
	}

	if (s->eu_p) {
		NMG_CK_EDGEUSE(s->eu_p);
		pl_color(fp, 0, 255, 0);
		eu = s->eu_p;
		do {
			NMG_CK_EDGEUSE(eu);
			NMG_CK_EDGE(eu->e_p);

			if (nmg_tbl(&b, TBL_LOC, &eu->e_p->magic) < 0) {

				NMG_CK_VERTEXUSE(eu->vu_p);
				NMG_CK_VERTEX(eu->vu_p->v_p);
				vg1 = eu->vu_p->v_p->vg_p;
				NMG_CK_VERTEX_G(vg1);

				NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
				NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
				vg2 = eu->eumate_p->vu_p->v_p->vg_p;
				NMG_CK_VERTEX_G(vg2);

				pd_3line(fp,
				vg1->coord[X], vg1->coord[Y], vg1->coord[Z],
				vg2->coord[X], vg2->coord[Y], vg2->coord[Z]);

				(void)nmg_tbl(&b, TBL_INS, &eu->e_p->magic);
			}

			eu = eu->next;
		} while (eu != s->eu_p);
	}
	if (s->vu_p) {
		pl_color(fp, 255, 255, 0);
		NMG_CK_VERTEXUSE(s->vu_p);
		NMG_CK_VERTEX(s->vu_p->v_p);
		NMG_CK_VERTEX_G(s->vu_p->v_p->vg_p);
		vg1 = s->vu_p->v_p->vg_p;
		pd_3point(fp, vg1->coord[X], vg1->coord[Y], vg1->coord[Z]);
	}

	if (!s->fu_p && !s->lu_p && !s->eu_p && !s->vu_p) {
		rt_log("At %d in %s shell has no children\n",
			__LINE__, __FILE__);
		rt_bomb("exiting\n");
	}

	(void)nmg_tbl(&b, TBL_FREE, (long *)NULL);
}


void nmg_pl_r(fp, r)
FILE *fp;
struct nmgregion *r;
{
	struct shell *s;

	s = r->s_p;
	do {
		nmg_pl_s(fp, s);
		s = s->next;
	} while (s != r->s_p);
}

void nmg_pl_m(fp, m)
FILE *fp;
struct model *m;
{
	struct nmgregion *r;

	r = m->r_p;
	do {
		nmg_pl_r(fp, r);
		r = r->next;
	} while (r != m->r_p);
}
