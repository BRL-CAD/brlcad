/*
 *			N M G _ M I S C . C
 *
 *	As the name implies, these are miscelaneous routines that work with
 *	the NMG structures.  Ordinarily, applications will not need these
 *	routines once they have been debugged.  As a result, these routines
 *	are in a separate file so that they may be omitted from linked
 *	executables that do not need them.  Someday, all machines may have
 *	shared library support.  Then this will be irrelevant.
 *
 *	Packages included:
 *		_pr_ routines call rt_log to print the contents of NMG structs
 *		_pl_ routines use the 3d plot library to create 3d plots of
 *			NMG structs
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
#include "externs.h"
#include "machine.h"
#include "vmath.h"
#include "fb.h"
#include "nmg.h"

/* #define DEBUG_PLEU */

/*	Print the orientation in a nice, english form
 */
void nmg_pr_orient(o, h)
char o, *h;
{
	switch (o) {
	case OT_SAME : rt_log("%s%8s orientation\n", h, "SAME"); break;
	case OT_OPPOSITE : rt_log("%s%8s orientation\n", h, "OPPOSITE"); break;
	case OT_NONE : rt_log("%s%8s orientation\n", h, "NONE"); break;
	case OT_UNSPEC : rt_log("%s%8s orientation\n", h, "UNSPEC"); break;
	default : rt_log("%s%8s orientation\n", h, "BOGUS!!!"); break;
	}
}


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
	nmg_pr_orient(fu->orientation, h);
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
	nmg_pr_orient(lu->orientation, h);
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
	nmg_pr_orient(eu->orientation, h);
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


static void nmg_pl_v(fp, v, b, R, G, B)
FILE *fp;
struct vertex *v;
struct nmg_ptbl *b;
unsigned char R, G, B;
{
	pointp_t p;
	static char label[128];

	if (nmg_tbl(b, TBL_LOC, &v->magic) >= 0) return;

	(void)nmg_tbl(b, TBL_INS, &v->magic);

	NMG_CK_VERTEX(v);
	NMG_CK_VERTEX_G(v->vg_p);
	p = v->vg_p->coord;
	pl_color(fp, 255, 255, 255);

	(void)sprintf(label, "%g %g %g", p[0], p[1], p[2]);
	pd_3move(fp, p[0], p[1], p[2]);
	pl_label(fp, label);
	pd_3point(fp, p[0], p[1], p[2]);


	pl_color(fp, R, G, B);
}
/*	E U _ C O O R D S
 *
 *	compute a pair of coordinates for representing an edgeuse
 */
static eu_coords(eu, base, tip)
struct edgeuse *eu;
point_t base, tip;
{
	fastf_t mag, dist1, rt_dist_line_point();
	struct edgeuse *peu;
	struct loopuse *lu;
	vect_t v1,		/* vector of edgeuse */
		v2,		/* vector of last edgeuse/start of edgeuse plot */
		N;		/* normal vector for this edgeuse's face */
	pointp_t p1, p0;


	lu = eu->up.lu_p;

	NMG_CK_FACE(lu->up.fu_p->f_p);
	NMG_CK_FACE_G(lu->up.fu_p->f_p->fg_p);

#ifdef DEBUG_PLEU
	HPRINT("Normal", lu->up.fu_p->f_p->fg_p->N);
	nmg_pr_orient(lu->up.fu_p->orientation, "");
#endif
	VMOVE(N, lu->up.fu_p->f_p->fg_p->N);
	if (lu->up.fu_p->orientation == OT_OPPOSITE) {
		VREVERSE(N,N);
	}
#ifdef DEBUG_PLEU
	VPRINT("Adjusted Normal", N);
#endif

	p0 = eu->vu_p->v_p->vg_p->coord;
	p1 = eu->eumate_p->vu_p->v_p->vg_p->coord;

	/* v1 is the vector of the edgeuse
	 * mag is the magnitude of the edge vector
	 */
	VSUB2(v1, p1, p0); 
	mag = MAGNITUDE(v1);
	VUNITIZE(v1);

	/* find a point not on the edge */
	peu = eu->last;
	p1 = peu->vu_p->v_p->vg_p->coord;
	dist1 = rt_dist_line_point(p0, v1, p1);
	while (dist1 == 0.0 && peu != eu) {
		peu = peu->last;
		p1 = peu->vu_p->v_p->vg_p->coord;
		dist1 = rt_dist_line_point(p0, v1, p1);
	}

	/* make a vector from the "last" edge */
	VSUB2(v2, p1, p0); VUNITIZE(v2);

	/* combine the two vectors to get a vector
	 * pointing to the location where the edgeuse
	 * should start
	 */
	VADD2(v2, v2, v1); VUNITIZE(v2);
	
	/* compute the start of the edgeuse */
	VJOIN2(base, p0, 0.5,v2, 0.1,N);

	mag *= 0.6;
	VJOIN1(tip, base, mag, v1);
}



static void eu_radial(fp, eu, tip)
FILE *fp;
struct edgeuse *eu;
point_t tip;
{
	point_t b2, t2, p;
	vect_t v;

	NMG_CK_EDGEUSE(eu->radial_p);
	NMG_CK_VERTEXUSE(eu->radial_p->vu_p);
	NMG_CK_VERTEX(eu->radial_p->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->radial_p->vu_p->v_p->vg_p);

	eu_coords(eu->radial_p, b2, t2);

	VSUB2SCALE(v, t2, b2, 0.8);
	VADD2(p, b2, v);
	pd_3line(fp, tip[0], tip[1], tip[2], p[0], p[1], p[2]);
}
static void eu_next(fp, eu, tip)
FILE *fp;
struct edgeuse *eu;
point_t tip;
{
	point_t b2, t2;

	NMG_CK_EDGEUSE(eu->next);
	NMG_CK_VERTEXUSE(eu->next->vu_p);
	NMG_CK_VERTEX(eu->next->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->next->vu_p->v_p->vg_p);

	eu_coords(eu->next, b2, t2);
	pl_color(fp, 0, 100, 0);
	pd_3line(fp, tip[0], tip[1], tip[2], b2[0], b2[1], b2[2]);
}


static nmg_pl_e(fp, e, b, R, G, B)
FILE *fp;
struct edge *e;
struct nmg_ptbl *b;
unsigned char R, G, B;
{
	pointp_t p0, p1;

	if (nmg_tbl(b, TBL_LOC, &e->magic) >= 0) return;

	(void)nmg_tbl(b, TBL_INS, &e->magic);
	
	NMG_CK_EDGEUSE(e->eu_p);
	NMG_CK_VERTEXUSE(e->eu_p->vu_p);
	NMG_CK_VERTEX(e->eu_p->vu_p->v_p);
	NMG_CK_VERTEX_G(e->eu_p->vu_p->v_p->vg_p);
	p0 = e->eu_p->vu_p->v_p->vg_p->coord;

	NMG_CK_VERTEXUSE(e->eu_p->eumate_p->vu_p);
	NMG_CK_VERTEX(e->eu_p->eumate_p->vu_p->v_p);
	NMG_CK_VERTEX_G(e->eu_p->eumate_p->vu_p->v_p->vg_p);
	p1 = e->eu_p->eumate_p->vu_p->v_p->vg_p->coord;

	pl_color(fp, R, G, B);
	pd_3line(fp, p0[0], p0[1], p0[2], p1[0], p1[1], p1[2]);
	nmg_pl_v(fp, e->eu_p->vu_p->v_p, b, R, G, B);
	nmg_pl_v(fp, e->eu_p->eumate_p->vu_p->v_p, b, R, G, B);
}


void nmg_pl_eu(fp, eu, b, R, G, B)
FILE *fp;
struct edgeuse *eu;
struct nmg_ptbl *b;
unsigned char R, G, B;
{
	point_t base, tip;

	

	NMG_CK_EDGEUSE(eu);
	NMG_CK_EDGE(eu->e_p);
	NMG_CK_VERTEXUSE(eu->vu_p);
	NMG_CK_VERTEX(eu->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);

	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);

	if (nmg_tbl(b, TBL_LOC, eu) >= 0) return;
	(void)nmg_tbl(b, TBL_INS, &eu->magic);

	nmg_pl_e(fp, eu->e_p, b, R, G, B);

	if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
	    *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC) {

	    	eu_coords(eu, base, tip);
	    	if (eu->up.lu_p->up.fu_p->orientation == OT_SAME)
	    		R += 50;
		else if (eu->up.lu_p->up.fu_p->orientation == OT_OPPOSITE)
			R -= 50;
	    	else
	    		R = G = B = (unsigned char)255;

		pl_color(fp, R, G, B);

		pd_3line(fp, base[0], base[1], base[2],
			tip[0], tip[1], tip[2]);


	    	eu_radial(fp, eu, tip);
	    	eu_next(fp, eu, tip);

	    }
}

void nmg_pl_lu(fp, lu, b, R, G, B)
FILE *fp;
struct loopuse *lu;
struct nmg_ptbl *b;
unsigned char R, G, B;
{
	struct edgeuse *eu;	

	NMG_CK_LOOPUSE(lu);
	if (nmg_tbl(b, TBL_LOC, &lu->magic) >= 0) return;

	(void)nmg_tbl(b, TBL_INS, &lu->magic);

	if (*lu->down.magic_p == NMG_VERTEXUSE_MAGIC) {
		nmg_pl_v(fp, lu->down.vu_p->v_p, b, R, G, B);
	} else if (*lu->down.magic_p == NMG_EDGEUSE_MAGIC) {
		eu = lu->down.eu_p;

		do {
			nmg_pl_eu(fp, eu, b, R, G, B);
			eu = eu->next;
		} while (eu != lu->down.eu_p);

	}
}

void nmg_pl_fu(fp, fu, b, R, G, B)
FILE *fp;
struct faceuse *fu;
struct nmg_ptbl *b;
unsigned char R, G, B;
{
	struct loopuse *lu;

	NMG_CK_FACEUSE(fu);
	if (nmg_tbl(b, TBL_LOC, &fu->magic) >= 0) return;
	(void)nmg_tbl(b, TBL_INS, &fu->magic);

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
			nmg_pl_fu(fp, fu, &b, 80, 100, 170);
#endif
			fu = fu->next;
		} while (fu != s->fu_p);
	}

	if (s->lu_p) {
		NMG_CK_LOOPUSE(s->lu_p);
		lu = s->fu_p->lu_p;
		do {
			nmg_pl_lu(fp, lu, &b, 255, 0, 0);

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

			nmg_pl_e(fp, eu->e_p, &b, (unsigned char)255, 
				(unsigned char)255, 
				(unsigned char)0);

			eu = eu->next;
		} while (eu != s->eu_p);
	}
	if (s->vu_p) {
		nmg_pl_v(fp, s->vu_p->v_p, &b, 180, 180, 180);
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


/*	C H E C K _ R A D I A L
 *	check to see if all radial uses of an edge (within a shell) are
 *	properly oriented with respect to each other.
 */
static void check_radial(eu)
struct edgeuse *eu;
{
	char curr_orient;
	struct edgeuse *eur, *eu1;
	struct shell *s;
	pointp_t p, q;

	NMG_CK_EDGEUSE(eu);
	
	s = eu->up.lu_p->up.fu_p->s_p;
	NMG_CK_SHELL(s);

	curr_orient = eu->up.lu_p->up.fu_p->orientation;
	eur = eu->radial_p;
	eu1 = eu;
	NMG_CK_EDGEUSE(eur);
	do {

		NMG_CK_LOOPUSE(eu1->up.lu_p);
		NMG_CK_FACEUSE(eu1->up.lu_p->up.fu_p);
		NMG_CK_LOOPUSE(eur->up.lu_p);
		NMG_CK_FACEUSE(eur->up.lu_p->up.fu_p);
		/* go find a radial edgeuse of the same shell
		 */
		while (eur->up.lu_p->up.fu_p->s_p != s) {
			NMG_CK_EDGEUSE(eur->eumate_p);
			if (eur->eumate_p->eumate_p != eur) {
				p = eur->vu_p->v_p->vg_p->coord;
				q = eur->eumate_p->vu_p->v_p->vg_p->coord;
				rt_log("edgeuse mate has different mate %g %g %g -> %g %g %g\n",
					p[0], p[1], p[2], q[0], q[1], q[2]);
				nmg_pr_lu(eu->up.lu_p, (char *)NULL);
				nmg_pr_lu(eu->eumate_p->up.lu_p, (char *)NULL);
				rt_bomb("bad edgeuse mate\n");
			}
			eur = eur->eumate_p->radial_p;
			NMG_CK_EDGEUSE(eur);
			NMG_CK_LOOPUSE(eur->up.lu_p);
			NMG_CK_FACEUSE(eur->up.lu_p->up.fu_p);
		}

		/* if that radial edgeuse doesn't have the
		 * correct orientation, print & bomb
		 */
		if (eur->up.lu_p->up.fu_p->orientation != curr_orient) {
			p = eu1->vu_p->v_p->vg_p->coord;
			q = eu1->eumate_p->vu_p->v_p->vg_p->coord;
			rt_log("Radial orientation problem at edge %g %g %g -> %g %g %g\n",
				p[0], p[1], p[2], q[0], q[1], q[2]);
			rt_log("Problem Edgeuses: %8x, %8x\n", eu1, eur);
			nmg_pr_fu(eu1->up.lu_p->up.fu_p, 0);
			rt_log("Radial loop:\n");
			nmg_pr_fu(eur->up.lu_p->up.fu_p, 0);
			rt_bomb("unclosed space\n");
		}

		eu1 = eur->eumate_p;
		curr_orient = eu1->up.lu_p->up.fu_p->orientation;
		eur = eu1->radial_p;
	} while (eur != eu->radial_p);

}

/* 	N M G _ C K _ C L O S E D _ S U R F
 *	Verify that we have a closed object in shell s1
 */
void nmg_ck_closed_surf(s)
struct shell *s;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;

	fu = s->fu_p;
	lu = fu->lu_p;
	
	do {
		if (*lu->down.magic_p == NMG_EDGEUSE_MAGIC) {
			eu = lu->down.eu_p;
			do {

				check_radial(eu);

				eu = eu->next;
			} while (eu != lu->down.eu_p);
		} else if (*lu->down.magic_p == NMG_VERTEXUSE_MAGIC) {
			NMG_CK_VERTEXUSE(lu->down.vu_p);
			NMG_CK_VERTEX(lu->down.vu_p->v_p);
		}
		lu = lu->next;
	} while (lu != fu->lu_p);
}

/*	P O L Y T O N M G
 *
 *	Read a polygon file and convert it to an NMG shell
 *
 *	A polygon file consists of the following:
 *	The first line consists of two integer numbers: the number of points
 *	(vertices) in the file
 *
 *
 *	Implicit return:
 *		r->s_p	A new shell containing all the faces from the
 *			polygon file
 *
 *		min	minimum point of the bounding RPP of the shell
 *		max	maximum point of the bounding RPP of the shell
 */
struct shell *polytonmg(fd, r, min, max)
FILE *fd;
struct nmgregion *r;
point_t min, max;
{
	int i, j, num_pts, num_facets, pts_this_face, facet;
	int vl_len;
	struct vertex **v;		/* list of all vertices */
	struct vertex **vl;	/* list of vertices for this polygon*/
	point_t p;
	struct shell *s;

	s = nmg_msv(r);
	nmg_kvu(s->vu_p);

	/* get number of points & number of facets in file */
	if (fscanf(fd, "%d %d", &num_pts, &num_facets) != 2)
		rt_bomb("Error in first line of poly file\n");
	
	/* get the bounds of the model */
	if (fscanf(fd, "%lg %lg %lg %lg %lg %lg", &min[0], &max[0],
	    &min[1], &max[1], &min[2], &max[2]) != 6)
		rt_bomb("error reading bounding box\n");


	v = (struct vertex **) rt_calloc(num_pts, sizeof (struct vertex *),
			"vertices");

	/* build the vertices */ 
	for (i = 0 ; i < num_pts ; ++i) {
		GET_VERTEX(v[i]);
		v[i]->magic = NMG_VERTEX_MAGIC;
	}

	/* read in the coordinates of the vertices */
	for (i=0 ; i < num_pts ; ++i) {
		if (fscanf(fd, "%lg %lg %lg", &p[0], &p[1], &p[2]) != 3)
			rt_bomb("Error reading point");
		nmg_vertex_gv(v[i], p);
	}


	vl = (struct vertex **)rt_calloc(vl_len=8, sizeof (struct vertex *),
		"vertex parameter list");

	for (facet = 0 ; facet < num_facets ; ++facet) {
		if (fscanf(fd, "%d", &pts_this_face) != 1)
			rt_bomb("error getting pt count for this face");

		if (pts_this_face > vl_len) {
			while (vl_len < pts_this_face) vl_len *= 2;
			rt_realloc(vl, vl_len*sizeof(struct vertex *),
				"vertex parameter list (realloc)");
		}

		for (i=0 ; i < pts_this_face ; ++i) {
			if (fscanf(fd, "%d", &j) != 1)
				rt_bomb("error getting point index for v in f");
			vl[i] = v[j-1];
		}

		(void)nmg_cface(s, vl, (unsigned)pts_this_face);
	}

	for (i=0 ; i < num_pts ; ++i) {
		if (v[i]->vu_p == (struct vertexuse *)NULL)
			FREE_VERTEX(v[i]);
	}

	return(s);
}



/**********************************************************************/
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
nmg_ck_eu(parent, eu, str)
long *parent;
struct edgeuse *eu;
char *str;
{
	char *errstr;
	struct edgeuse *eur;	

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

	} else {
		rt_bomb(strcat(errstr, "Bad edgeuse parent\n"));
	}

	NMG_CK_EDGE(eu->e_p);
	nmg_ck_e(eu, eu->e_p, errstr);

	NMG_CK_VERTEXUSE(eu->vu_p);
	nmg_ck_vu(&eu->magic, eu->vu_p, errstr);

	rt_free(errstr, "nmg_ck_eu error str");
}

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



nmg_ck_lu(parent, lu, str)
long *parent;
struct loopuse *lu;
char *str;
{
	struct edgeuse *eu;
	char *errstr;
	int l;
	int edgeuse_num=0;


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
	if (*lu->down.magic_p == NMG_VERTEXUSE_MAGIC) {
		nmg_ck_vu(&lu->magic, lu->down.vu_p, errstr);
	} else if (*lu->down.magic_p == NMG_EDGEUSE_MAGIC) {
		NMG_CK_EDGEUSE(lu->down.eu_p);
		l = strlen(errstr);
		eu = lu->down.eu_p;
		do {
			(void)sprintf(&errstr[l], "%sedgeuse #%d (%8x)\n",
				errstr, edgeuse_num++, eu);
			nmg_ck_eu(&lu->magic, eu, errstr);
			eu = eu->next;
		} while (eu != lu->down.eu_p);
	} else {
		rt_bomb(strcat(errstr, "Bad loopuse down pointer\n") );
	}
	rt_free(errstr, "nmg_ck_lu error str");
}




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

	if (fu->next->last != fu) rt_bomb(
		strcat(errstr, "Faceuse not lastward of next faceuse\n") );

	if (fu->last->next != fu) rt_bomb(
		strcat(errstr, "Faceuse not nextward from last faceuse\n") );

	NMG_CK_FACEUSE(fu->fumate_p);
	if (fu->fumate_p->fumate_p != fu) rt_bomb(
		strcat(errstr, "Faceuse not fumate of fumate\n") );

	if (fu->fumate_p->s_p != s) rt_bomb(
		strcat(errstr, "faceuse mates not in same shell\n") );

	nmg_ck_f(fu, fu->f_p, errstr);

	NMG_CK_LOOPUSE(fu->lu_p);
	lu = fu->lu_p;
	l = strlen(errstr);
	do {
		(void)sprintf(&errstr[l] , "%sloopuse #%d (%8x)\n", 
			errstr, loop_number++, lu);
		nmg_ck_lu(&fu->magic, lu, errstr);
		lu = lu->next;
	} while (lu != fu->lu_p);

	rt_free(errstr, "nmg_ck_fu error str");
}

static void plot_edge(fd, b, eu)
FILE *fd;
struct nmg_ptbl *b;
struct edgeuse *eu;
{
	struct edgeuse *eur = eu;
	do {
		nmg_pl_eu(fd, eur, b, 180, 180, 180);
		eur = eur->radial_p->eumate_p;
	} while (eur != eu);
}

void pl_isect_eu(fd, b, eu)
FILE *fd;
struct nmg_ptbl *b;
struct edgeuse *eu;
{
	struct edgeuse *eur;

	eur = eu;
	NMG_CK_EDGEUSE(eu);
	NMG_CK_LOOPUSE(eu->up.lu_p);
	NMG_CK_FACEUSE(eu->up.lu_p->up.fu_p);
	NMG_CK_SHELL(eu->up.lu_p->up.fu_p->s_p);

	do {
		NMG_CK_EDGEUSE(eur);

		if (*eur->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    *eur->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
		    eur->up.lu_p->up.fu_p->s_p != eu->up.lu_p->up.fu_p->s_p) {
			plot_edge(fd, b, eu);
		    	break;
		    }

		eur = eur->radial_p->eumate_p;
	} while (eur != eu);
}

void nmg_pl_isect(filename, s)
char *filename;
struct shell *s;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct nmg_ptbl b;
	FILE *fd, *fopen();

	NMG_CK_SHELL(s);

	if ((fd=fopen(filename, "w")) == (FILE *)NULL) {
		(void)perror(filename);
		exit(-1);
	}

	(void)nmg_tbl(&b, TBL_INIT, (long *)NULL);

	rt_log("Plotting to \"%s\"\n", filename);
	fu = s->fu_p;
	do {
		NMG_CK_FACEUSE(fu);
		lu = fu->lu_p;
		do {
			NMG_CK_LOOPUSE(lu);
			if (*lu->down.magic_p == NMG_EDGEUSE_MAGIC) {
				eu = lu->down.eu_p;
				do {
					NMG_CK_EDGEUSE(eu);
					pl_isect_eu(fd, &b, eu);

					eu = eu->next;
				} while (eu != lu->down.eu_p);
			} else if (*lu->down.magic_p == NMG_VERTEXUSE_MAGIC) {
				;
			} else {
				rt_log("in %s at %d bad loopuse child\n",
					__FILE__, __LINE__);
				rt_bomb("BAD NMG struct\n");
			}
			lu = lu->next;
		} while (lu != fu->lu_p);

		fu = fu->next;
	} while (fu != s->fu_p);

	(void)nmg_tbl(&b, TBL_FREE, (long *)NULL);

	(void)fclose(fd);
}
