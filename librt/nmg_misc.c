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
 *		_ck_ routines to check consistency
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
#include "nmg.h"
#include "raytrace.h"

/* #define DEBUG_PLEU */



/*	Print the orientation in a nice, english form
 */
void nmg_pr_orient(orientation, h)
int	orientation;
char	*h;
{
	switch (orientation) {
	case OT_SAME : rt_log("%s%8s orientation\n", h, "SAME"); break;
	case OT_OPPOSITE : rt_log("%s%8s orientation\n", h, "OPPOSITE"); break;
	case OT_NONE : rt_log("%s%8s orientation\n", h, "NONE"); break;
	case OT_UNSPEC : rt_log("%s%8s orientation\n", h, "UNSPEC"); break;
	case OT_BOOLPLACE : rt_log("%s%8s orientation\n", h, "BOOLPLACE"); break;
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

	for( NMG_LIST( r, nmgregion, &m->r_hd ) )  {
		nmg_pr_r(r, (char *)NULL);
	}
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

	if (!r || r->l.magic != NMG_REGION_MAGIC) {
		rt_log("bad region magic\n");
		Return;
	}

	rt_log("%8x m_p\n", r->m_p);
	rt_log("%8x l.forw\n", r->l.forw);
	rt_log("%8x l.back\n", r->l.back);
	rt_log("%8x ra_p\n", r->ra_p);

	for( NMG_LIST( s, shell, &r->s_hd ) )  {
		nmg_pr_s(s, h);
	}
	Return;
}

void nmg_pr_sa(sa, h)
struct shell_a *sa;
char *h;
{
	MKPAD(h);

	rt_log("%sSHELL_A %8x\n", h, sa);
	if (!sa || sa->magic != NMG_SHELL_A_MAGIC) {
		rt_log("bad shell_a magic\n");
		Return;
	}
	
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

	rt_log("%sFACE_G %8x\n", h, fg);
	NMG_CK_FACE_G(fg);
	
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
	struct faceuse	*fu;
	struct loopuse	*lu;
	struct edgeuse	*eu;

	MKPAD(h);
	
	rt_log("%sSHELL %8x\n", h, s);
	if (!s || s->l.magic != NMG_SHELL_MAGIC) {
		rt_log("bad shell magic\n");
		Return;
	}

	rt_log("%s%8x r_p\n", h, s->r_p);
	rt_log("%s%8x l.forw\n", h, s->l.forw );
	rt_log("%s%8x l.back\n", h, s->l.back );
	rt_log("%s%8x sa_p\n", h, s->sa_p );
	if (s->sa_p)
		nmg_pr_sa(s->sa_p, h);
	
	for( NMG_LIST( fu, faceuse, &s->fu_hd ) )  {
		nmg_pr_fu(fu, h);
	}

	for( NMG_LIST( lu, loopuse, &s->lu_hd ) )  {
		nmg_pr_lu(lu, h);
	}

	for( NMG_LIST( eu, edgeuse, &s->eu_hd ) )  {
		nmg_pr_eu(eu, h);
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

	if (!fu || fu->l.magic != NMG_FACEUSE_MAGIC) {
		rt_log("bad faceuse magic\n");
		Return;
	}
	
	rt_log("%s%8x s_p\n", h, fu->s_p);
	rt_log("%s%8x l.forw\n", h, fu->l.forw);
	rt_log("%s%8x l.back\n", h, fu->l.back);
	rt_log("%s%8x fumate_p\n", h, fu->fumate_p);
	nmg_pr_orient(fu->orientation, h);
	rt_log("%s%8x fua_p\n", h, fu->fua_p);

	rt_log("%s%8x f_p\n", h, fu->f_p);

	for( NMG_LIST( lu, loopuse, &fu->lu_hd ) )  {
		nmg_pr_lu(lu, h);
	}
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
	struct edgeuse	*eu;
	struct vertexuse *vu;
	long		magic1;
	
	MKPAD(h);
	NMG_CK_LOOPUSE(lu);

	rt_log("%sLOOPUSE %8x\n", h, lu);
	if (!lu || lu->l.magic != NMG_LOOPUSE_MAGIC) {
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

	rt_log("%s%8x l.forw\n", h, lu->l.forw);
	rt_log("%s%8x l.back\n", h, lu->l.back);
	rt_log("%s%8x lumate_p\n", h, lu->lumate_p);
	rt_log("%s%8x lua_p\n", h, lu->lua_p);
	nmg_pr_orient(lu->orientation, h);
	rt_log("%s%8x l_p\n", h, lu->l_p);
	if (lu->l_p)
		nmg_pr_l(lu->l_p, h);


	rt_log("%s%8x down_hd.magic\n", h, lu->down_hd.magic);
	rt_log("%s%8x down_hd.forw\n", h, lu->down_hd.forw);
	rt_log("%s%8x down_hd.back\n", h, lu->down_hd.back);

	magic1 = NMG_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC) {
		vu = NMG_LIST_PNEXT( vertexuse, &lu->down_hd );
		rt_log("%s%8x down_hd->forw (vu)\n", h, vu);
		nmg_pr_vu(vu, h);
	}
	else if (magic1 == NMG_EDGEUSE_MAGIC) {
		for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
			nmg_pr_eu(eu, h);
		}
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
	if (!eu || eu->l.magic != NMG_EDGEUSE_MAGIC) {
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
	rt_log("%s%8x l.forw\n", h, eu->l.forw);
	rt_log("%s%8x l.back\n", h, eu->l.back);
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

	if (!vg || vg->magic != NMG_VERTEX_G_MAGIC) {
		rt_log("%sVERTEX_G %8x\n", h, vg);
		rt_log("bad vertex_g magic\n");
		Return;
	}
	rt_log("%sVERTEX_G %8x %f %f %f = XYZ coord\n",
		h, vg, V3ARGS(vg->coord) );
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
	/* vu_hd ? */
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
	if (!vu || vu->l.magic != NMG_VERTEXUSE_MAGIC) {
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
	rt_log("%s%8x l.forw\n", h, vu->l.forw);
	rt_log("%s%8x l.back\n", h, vu->l.back);
	rt_log("%s%8x vua_p\n", h, vu->vua_p);
	rt_log("%s%8x v_p\n", h, vu->v_p);
	nmg_pr_v(vu->v_p, h);
	
	Return;
}

/*	C H E C K _ R A D I A L
 *	check to see if all radial uses of an edge (within a shell) are
 *	properly oriented with respect to each other.
 *
 *	Return
 *	0	OK
 *	1	bad edgeuse mate
 *	2	unclosed space
 */
static int check_radial(eu)
struct edgeuse *eu;
{
	char curr_orient;
	struct edgeuse *eur, *eu1, *eurstart;
	struct shell *s;
	pointp_t p, q;

	NMG_CK_EDGEUSE(eu);
	
	s = eu->up.lu_p->up.fu_p->s_p;
	NMG_CK_SHELL(s);

	curr_orient = eu->up.lu_p->up.fu_p->orientation;
	eur = eu->radial_p;

	/* skip the wire edges */
	while (*eur->up.magic_p == NMG_SHELL_MAGIC) {
		eur = eur->eumate_p->radial_p;
	}

	eurstart = eur;

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
				rt_log("bad edgeuse mate\n");
				return(1);
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
			if (rt_g.NMG_debug) {
				nmg_pr_fu(eu1->up.lu_p->up.fu_p, 0);
				rt_log("Radial loop:\n");
				nmg_pr_fu(eur->up.lu_p->up.fu_p, 0);
			}
			rt_log("unclosed space\n");
			return(2);
		}

		eu1 = eur->eumate_p;
		curr_orient = eu1->up.lu_p->up.fu_p->orientation;
		eur = eu1->radial_p;
		while (*eur->up.magic_p == NMG_SHELL_MAGIC) {
			eur = eur->eumate_p->radial_p;
		}

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
 *	!0	status code from check_radial()
 */
int nmg_ck_closed_surf(s)
struct shell *s;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	int		status;
	long		magic1;

	NMG_CK_SHELL(s);
	for( NMG_LIST( fu, faceuse, &s->fu_hd ) )  {
		NMG_CK_FACEUSE(fu);
		for( NMG_LIST( lu, loopuse, &fu->lu_hd ) )  {
			NMG_CK_LOOPUSE(lu);
			magic1 = NMG_LIST_FIRST_MAGIC( &lu->down_hd );
			if (magic1 == NMG_EDGEUSE_MAGIC) {
				for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
					if (status=check_radial(eu))
						return(status);
				}
			} else if (magic1 == NMG_VERTEXUSE_MAGIC) {
				register struct vertexuse	*vu;
				vu = NMG_LIST_FIRST( vertexuse, &lu->down_hd );
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
 *	!0	status code from check_radial()
 */
int
nmg_ck_closed_region(r)
struct nmgregion	*r;
{
	struct shell	*s;
	int		ret;

	NMG_CK_REGION(r);
	for( NMG_LIST( s, shell, &r->s_hd ) )  {
		ret = nmg_ck_closed_surf( s );
		if( ret != 0 )  return(ret);
	}
	return(0);
}


/*	P O L Y T O N M G
 *
 *	Read a polygon file and convert it to an NMG shell
 *
 *	A polygon file consists of the following:
 *	The first line consists of two integer numbers: the number of points
 *	(vertices) in the file, followed by the number of polygons in the file.
 *	This line is followed by lines for each of the
 *	verticies.  Each vertex is listed on its own line, as the 3tuple
 *	"X Y Z".  After the list of verticies comes the list of polygons.  
 *	each polygon is represented by a line containing 1) the number of
 *	verticies in the polygon, followed by 2) the indicies of the verticies
 *	that make up the polygon.
 *
 *	Implicit return:
 *		r->s_p	A new shell containing all the faces from the
 *			polygon file
 */
struct shell *polytonmg(fd, r)
FILE *fd;
struct nmgregion *r;
{
	int i, j, num_pts, num_facets, pts_this_face, facet;
	int vl_len;
	struct vertex **v;		/* list of all vertices */
	struct vertex **vl;	/* list of vertices for this polygon*/
	point_t p;
	struct shell *s;
	struct faceuse *fu;
	struct loopuse	*lu;
	struct edgeuse *eu;
	plane_t plane;

	s = nmg_msv(r);
	nmg_kvu(s->vu_p);

	/* get number of points & number of facets in file */
	if (fscanf(fd, "%d %d", &num_pts, &num_facets) != 2)
		rt_bomb("polytonmg() Error in first line of poly file\n");
	else
		if (rt_g.NMG_debug & DEBUG_POLYTO)
			rt_log("points: %d  facets: %d\n",
				num_pts, num_facets);


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
			rt_bomb("polytonmg() Error reading point");
		else
			if (rt_g.NMG_debug & DEBUG_POLYTO)
				rt_log("read vertex #%d (%g %g %g)\n",
					i, p[0], p[1], p[2]);

		nmg_vertex_gv(v[i], p);
	}

	vl = (struct vertex **)rt_calloc(vl_len=8, sizeof (struct vertex *),
		"vertex parameter list");

	for (facet = 0 ; facet < num_facets ; ++facet) {
		if (fscanf(fd, "%d", &pts_this_face) != 1)
			rt_bomb("polytonmg() error getting pt count for this face");

		if (rt_g.NMG_debug & DEBUG_POLYTO)
			rt_log("facet %d pts in face %d\n",
				facet, pts_this_face);

		if (pts_this_face > vl_len) {
			while (vl_len < pts_this_face) vl_len *= 2;
			vl = (struct vertex **)rt_realloc( (char *)vl,
				vl_len*sizeof(struct vertex *),
				"vertex parameter list (realloc)");
		}

		for (i=0 ; i < pts_this_face ; ++i) {
			if (fscanf(fd, "%d", &j) != 1)
				rt_bomb("polytonmg() error getting point index for v in f");
			vl[i] = v[j-1];
		}

		fu = nmg_cface(s, vl, pts_this_face);
		lu = NMG_LIST_FIRST( loopuse, &fu->lu_hd );
		/* XXX should check for vertex-loop */
		eu = NMG_LIST_FIRST( edgeuse, &lu->down_hd );
		NMG_CK_EDGEUSE(eu);
		if (rt_mk_plane_3pts(plane, eu->vu_p->v_p->vg_p->coord,
		    NMG_LIST_PNEXT(edgeuse,eu)->vu_p->v_p->vg_p->coord,
		    NMG_LIST_PLAST(edgeuse,eu)->vu_p->v_p->vg_p->coord ) )  {
			rt_log("At %d in %s\n", __LINE__, __FILE__);
			rt_bomb("polytonmg() cannot make plane equation\n");
		}
		else nmg_face_g(fu, plane);
	}

	for (i=0 ; i < num_pts ; ++i) {
		if( NMG_LIST_IS_EMPTY( &v[i]->vu_hd ) )  continue;
		FREE_VERTEX(v[i]);
	}
	rt_free( (char *)v, "vertex array");
	return(s);
}


/************************************************************************
 *									*
 *			Checking Routines				*
 *									*
 ************************************************************************/


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

		eu_next = NMG_LIST_PNEXT_CIRC(edgeuse, eu);
		if (eu_next->vu_p->v_p != eu->eumate_p->vu_p->v_p)
			rt_bomb("nmg_ck_eu: next and mate don't share vertex\n");

		eu_last = NMG_LIST_PLAST_CIRC(edgeuse, eu);
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
	magic1 = NMG_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC) {
		vu = NMG_LIST_FIRST( vertexuse, &lu->down_hd );
		NMG_CK_VERTEXUSE(vu);
		nmg_ck_vu(&lu->l.magic, vu, errstr);
	} else if (magic1 == NMG_EDGEUSE_MAGIC) {
		l = strlen(errstr);
		for( NMG_LIST( eu, edgeuse, &lu->down_hd ) )  {
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

	if( NMG_LIST_PNEXT_PLAST( faceuse, fu ) )
		rt_bomb( strcat(errstr, "Faceuse not lastward of next faceuse\n") );

	if( NMG_LIST_PLAST_PNEXT( faceuse, fu ) )
		rt_bomb( strcat(errstr, "Faceuse not nextward from last faceuse\n") );

	NMG_CK_FACEUSE(fu->fumate_p);
	if (fu->fumate_p->fumate_p != fu) rt_bomb(
		strcat(errstr, "Faceuse not fumate of fumate\n") );

	if (fu->fumate_p->s_p != s) rt_bomb(
		strcat(errstr, "faceuse mates not in same shell\n") );

	nmg_ck_f(fu, fu->f_p, errstr);

	l = strlen(errstr);
	for( NMG_LIST( lu, loopuse, &fu->lu_hd ) )  {
		NMG_CK_LOOPUSE(lu);
		(void)sprintf(&errstr[l] , "%sloopuse #%d (%8x)\n", 
			errstr, loop_number++, lu);
		nmg_ck_lu(&fu->l.magic, lu, errstr);
	}
	rt_free(errstr, "nmg_ck_fu error str");
}
