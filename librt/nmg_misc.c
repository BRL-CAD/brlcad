/*
 *			N M G _ M I S C . C
 *
 *	As the ame implies, these are miscelaneous routines that work with
 *	the NMG structures.
 *
 *	Packages included:
 *		_pr_ routines call rt_log to print the contents of NMG structs
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
#include "machine.h"
#include "vmath.h"
#include "externs.h"
#include "nmg.h"
#include "raytrace.h"


/*	N M G _ T B L
 *	maintain a table of pointers (to magic numbers/structs)
 */
int
nmg_tbl(b, func, p)
struct nmg_ptbl *b;
int func;
long *p;
{
	if (func == TBL_INIT) {
		b->buffer = (long **)rt_calloc(b->blen=64,
						sizeof(p), "pointer table");
		b->end = 0;
		return(0);
	} else if (func == TBL_RST) {
		b->end = 0;
		return(0);
	} else if (func == TBL_INS) {
		register int i;
		union {
			struct loopuse *lu;
			struct edgeuse *eu;
			struct vertexuse *vu;
			long *l;
		} pp;

		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl Inserting %8x\n", p);

		pp.l = p;

		if (b->blen == 0) (void)nmg_tbl(b, TBL_INIT, p);
		if (b->end >= b->blen)
			b->buffer = (long **)rt_realloc( (char *)b->buffer,
			    sizeof(p)*(b->blen *= 4),
			    "pointer table" );

		b->buffer[i=b->end++] = p;
		return(i);
	} else if (func == TBL_LOC) {
		/* we do this a great deal, so make it go as fast as possible.
		 * this is the biggest argument I can make for changing to an
		 * ordered list.  Someday....
		 */
		register int	k;
		register long	**pp = b->buffer;

#		include "noalias.h"
		for( k = b->end-1; k >= 0; k-- )
			if (pp[k] == p) return(k);

		return(-1);
	} else if (func == TBL_INS_UNIQUE) {
		/* we do this a great deal, so make it go as fast as possible.
		 * this is the biggest argument I can make for changing to an
		 * ordered list.  Someday....
		 */
		register int	k;
		register long	**pp = b->buffer;

#		include "noalias.h"
		for( k = b->end-1; k >= 0; k-- )
			if (pp[k] == p) return(k);

		if (b->blen <= 0 || b->end >= b->blen)  {
			/* Table needs to grow */
			return( nmg_tbl( b, TBL_INS, p ) );
		}

		if (rt_g.NMG_debug & DEBUG_INS)
			rt_log("nmg_tbl Inserting %8x\n", p);

		b->buffer[k=b->end++] = p;
		return(-1);		/* To signal that it was added */
	} else if (func == TBL_RM) {
		/* we go backwards down the list looking for occurrences
		 * of p to delete.  We do it backwards to reduce the amount
		 * of data moved when there is more than one occurrence of p
		 * in the list.  A pittance savings, unless you're doing a
		 * lot of it.
		 */
		register int end = b->end, j, k, l;
		register long **pp = b->buffer;

		for (l = b->end-1 ; l >= 0 ; --l)
			if (pp[l] == p){
				/* delete occurrence(s) of p */

				j=l+1;
				while (pp[l-1] == p) --l;

				end -= j - l;
#				include "noalias.h"
				for(k=l ; j < end ;)
					b->buffer[k++] = b->buffer[j++];
			}

		b->end = end;
		return(0);
	} else if (func == TBL_CAT) {
		union {
			long *l;
			struct nmg_ptbl *t;
		} d;

		d.l = p;

		if ((b->blen - b->end) < d.t->end) {
			
			b->buffer = (long **)rt_realloc( (char *)b->buffer,
				sizeof(p)*(b->blen += d.t->blen),
				"pointer table (CAT)");
		}
		bcopy(d.t->buffer, &b->buffer[b->end], d.t->end*sizeof(p));
		return(0);
	} else if (func == TBL_FREE) {
		bzero((char *)b->buffer, b->blen * sizeof(p));
		rt_free((char *)b->buffer, "pointer table");
		bzero(b, sizeof(struct nmg_ptbl));
		return (0);
	} else {
		rt_log("Unknown table function %d\n", func);
		rt_bomb("nmg_tbl");
	}
	return(-1);/* this is here to keep lint happy */
}




/* N M G _ P U R G E _ U N W A N T E D _ I N T E R S E C T I O N _ P O I N T S
 *
 *	Make sure that the list of intersection points doesn't contain
 *	any vertexuses from loops whose bounding boxes don;t overlap the
 *	bounding box of a loop in the given faceuse.
 *
 *	This is really a special purpose routine to help the intersection
 *	operations of the boolean process.  The only reason it's here instead
 *	of nmg_inter.c is that it knows too much about the format and contents
 *	of an nmg_ptbl structure.
 */
void
nmg_purge_unwanted_intersection_points(vert_list, fu)
struct nmg_ptbl *vert_list;
struct faceuse *fu;
{
	int i, j;
	struct vertexuse *vu;
	struct loopuse *lu;
	struct loopuse *fu2lu;
	struct loop_g	*lg;
	struct loop_g	*fu2lg;
	int overlap;

	if (rt_g.NMG_debug & DEBUG_POLYSECT)
		rt_log("nmg_purge_unwanted_intersection_points(0x%08x, 0x%08x)\n", vert_list, fu);

	for (i=0 ; i < vert_list->end ; i++) {
		vu = (struct vertexuse *)vert_list->buffer[i];
		NMG_CK_VERTEXUSE(vu);
		lu = nmg_lu_of_vu( vu );
		NMG_CK_LOOPUSE(lu);
		lg = lu->l_p->lg_p;
		NMG_CK_LOOP_G(lg);

		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			rt_log("vu[%d]: 0x%08x (%g %g %g) lu: 0x%08x %s\n",
				i, vu, V3ARGS(vu->v_p->vg_p->coord),
				lu, nmg_orientation(lu->orientation) );
			rt_log("\tlu BBox: (%g %g %g) (%g %g %g)\n",
				V3ARGS(lg->min_pt), V3ARGS(lg->max_pt) );
		}
		if (lu->up.fu_p->f_p == fu->f_p)
			rt_log("I'm checking against my own face?\n");

		/* If the bounding box of a loop doesn't intersect the
		 * bounding box of a loop in the other face, it shouldn't
		 * be on the list of intersecting loops.
		 */
		overlap = 0;
		for (RT_LIST_FOR(fu2lu, loopuse, &fu->lu_hd )){
			NMG_CK_LOOPUSE(fu2lu);
			NMG_CK_LOOP(fu2lu->l_p);

			/* If this loop is just some drek deposited into the
			 * other loop as part of an intersection operation,
			 * it doesn't really count -- skip it.
			 */
			if (fu2lu->orientation == OT_BOOLPLACE)  continue;

			/* Everything should be OT_SAME or OT_BOOLPLACE, but...*/
			if (fu2lu->orientation != OT_SAME)
				rt_log("nmg_purge_unwanted_intersection_points encountered %s loop in fu2\n", nmg_orientation(fu2lu->orientation));

			fu2lg = fu2lu->l_p->lg_p;
			NMG_CK_LOOP_G(fu2lg);
			if (V3RPP_OVERLAP(fu2lg->min_pt, fu2lg->max_pt,
			    lg->min_pt, lg->max_pt)) {
				overlap = 1;
				break;
			}
		}
		if (rt_g.NMG_debug & DEBUG_POLYSECT) {
			rt_log("%s\tfu2lu BBox: (%g %g %g)  (%g %g %g) %s\n",
				overlap ? "keep" : "KILL",
				V3ARGS(fu2lg->min_pt), V3ARGS(fu2lg->max_pt),
				nmg_orientation(fu2lu->orientation) );
		}
		if (!overlap) {
			/* why is this vertexuse in the list? */
			if (rt_g.NMG_debug & DEBUG_POLYSECT) {
				rt_log("nmg_purge_unwanted_intersection_points This little bugger slipped in somehow.  Deleting it.\n");
				nmg_pr_vu_briefly(vu, (char *)NULL);
			}

			/* delete the entry from the vertex list */
			for (j=i ; j < vert_list->end ; j++)
				vert_list->buffer[j] = vert_list->buffer[j+1];

			--(vert_list->end);
			vert_list->buffer[vert_list->end] = (long *)NULL;
			--i;
		}
	}
}


/*				N M G _ I N _ O R _ R E F
 *
 *	if the given vertexuse "vu" is in the table given by "b" or if "vu"
 *	references a vertex which is refernced by a vertexuse in the table,
 *	then we return 1.  Otherwise, we return 0.
 */
int 
nmg_in_or_ref(vu, b)
struct vertexuse *vu;
struct nmg_ptbl *b;
{
	union {
		struct vertexuse **vu;
		long **magic_p;
	} p;
	int i;

	p.magic_p = b->buffer;
	for (i=0 ; i < b->end ; ++i) {
		if (p.vu[i] && *p.magic_p[i] == NMG_VERTEXUSE_MAGIC &&
		    (p.vu[i] == vu || p.vu[i]->v_p == vu->v_p))
			return(1);
	}
	return(0);
}

/*
 *			N M G _ O R I E N T A T I O N
 *
 *  Convert orientation code to string.
 */
char *
nmg_orientation(orientation)
int	orientation;
{
	switch (orientation) {
	case OT_SAME:
		return "OT_SAME";
	case OT_OPPOSITE:
		return "OT_OPPOSITE";
	case OT_NONE:
		return "OT_NONE";
	case OT_UNSPEC:
		return "OT_UNSPEC";
	case OT_BOOLPLACE:
		return "OT_BOOLPLACE";
	}
	return "OT_IS_BOGUS!!";
}

/*	Print the orientation in a nice, english form
 */
void 
nmg_pr_orient(orientation, h)
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


void 
nmg_pr_m(m)
struct model *m;
{
	struct nmgregion *r;

	rt_log("MODEL %8x\n", m);
	if (!m || m->magic != NMG_MODEL_MAGIC) {
		rt_log("bad model magic\n");
		return;
	}
	rt_log("%8x ma_p\n", m->ma_p);

	for( RT_LIST_FOR( r, nmgregion, &m->r_hd ) )  {
		nmg_pr_r(r, (char *)NULL);
	}
}

static char padstr[32];
#define MKPAD(_h) { \
	if (!_h) { _h = padstr; bzero(h, sizeof(padstr)); } \
	else { if (strlen(_h) < sizeof(padstr)-4) (void)strcat(_h, "   "); } }

#define Return	{ h[strlen(h)-3] = '\0'; return; }

void 
nmg_pr_r(r, h)
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

	for( RT_LIST_FOR( s, shell, &r->s_hd ) )  {
		nmg_pr_s(s, h);
	}
	Return;
}

void 
nmg_pr_sa(sa, h)
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

void 
nmg_pr_lg(lg, h)
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

void 
nmg_pr_fg(fg, h)
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

void 
nmg_pr_s(s, h)
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
	
	for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		nmg_pr_fu(fu, h);
	}

	for( RT_LIST_FOR( lu, loopuse, &s->lu_hd ) )  {
		nmg_pr_lu(lu, h);
	}

	for( RT_LIST_FOR( eu, edgeuse, &s->eu_hd ) )  {
		nmg_pr_eu(eu, h);
	}
	if (s->vu_p)
		nmg_pr_vu(s->vu_p, h);

	Return;
}

void 
nmg_pr_f(f, h)
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

void 
nmg_pr_fu(fu, h)
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
	if (fu->f_p)
		nmg_pr_f(fu->f_p, h);

	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		nmg_pr_lu(lu, h);
	}
	Return;
}

void 
nmg_pr_fu_briefly(fu, h)
struct faceuse *fu;
char *h;
{
	struct loopuse *lu;
	MKPAD(h);
	NMG_CK_FACEUSE(fu);

	rt_log("%sFACEUSE %8x (%s)\n",
		h, fu, nmg_orientation(fu->orientation));

	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		nmg_pr_lu_briefly(lu, h);
	}
	Return;
}

void 
nmg_pr_l(l, h)
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
void 
nmg_pr_lu(lu, h)
struct loopuse *lu;
char *h;
{
	struct edgeuse	*eu;
	struct vertexuse *vu;
	long		magic1;
	
	MKPAD(h);
	NMG_CK_LOOPUSE(lu);

	rt_log("%sLOOPUSE %8x\n", h, lu);

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

	magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC) {
		vu = RT_LIST_PNEXT( vertexuse, &lu->down_hd );
		rt_log("%s%8x down_hd->forw (vu)\n", h, vu);
		nmg_pr_vu(vu, h);
	}
	else if (magic1 == NMG_EDGEUSE_MAGIC) {
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			nmg_pr_eu(eu, h);
		}
	}
	else
		rt_log("bad loopuse child magic\n");

	Return;
}

void 
nmg_pr_lu_briefly(lu, h)
struct loopuse *lu;
char *h;
{
	struct edgeuse	*eu;
	struct vertexuse *vu;
	long		magic1;
	
	MKPAD(h);
	NMG_CK_LOOPUSE(lu);

	rt_log("%sLOOPUSE %8x, lumate_p=x%x (%s) \n",
		h, lu, lu->lumate_p, nmg_orientation(lu->orientation) );

	magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
	if (magic1 == NMG_VERTEXUSE_MAGIC) {
		vu = RT_LIST_PNEXT( vertexuse, &lu->down_hd );
		rt_log("%s%8x down_hd->forw (vu)\n", h, vu);
		nmg_pr_vu_briefly(vu, h);
	}
	else if (magic1 == NMG_EDGEUSE_MAGIC) {
		for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
			nmg_pr_eu_briefly(eu, h);
		}
	}
	else
		rt_log("bad loopuse child magic\n");

	Return;
}

void
nmg_pr_eg(eg, h)
struct edge_g *eg;
char *h;
{
	MKPAD(h);
	NMG_CK_EDGE_G(eg);
	
	rt_log("%sEDGE_G %8x pt:(%f %f %f)\n",
		h, eg, V3ARGS(eg->e_pt));
	rt_log("%s       use %d  dir:(%f %f %f)\n",
		h, eg->usage, V3ARGS(eg->e_dir));

	Return;
}

void 
nmg_pr_e(e, h)
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

	if (e->eg_p)
		nmg_pr_eg(e->eg_p, h);

	Return;
}


void 
nmg_pr_eu(eu, h)
struct edgeuse *eu;
char *h;
{
	MKPAD(h);
	NMG_CK_EDGEUSE(eu);

	rt_log("%sEDGEUSE %8x\n", h, eu);

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

void 
nmg_pr_eu_briefly(eu, h)
struct edgeuse *eu;
char *h;
{
	MKPAD(h);
	NMG_CK_EDGEUSE(eu);

	rt_log("%sEDGEUSE %8x\n", h, eu);
	nmg_pr_vu_briefly(eu->vu_p, h);

	Return;
}

void 
nmg_pr_vg(vg, h)
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

void 
nmg_pr_v(v, h)
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
	rt_log("%s   vu_hd %8x\n", h, &v->vu_hd);
	rt_log("%s%8x vu_hd.forw\n", h, v->vu_hd.forw);
	rt_log("%s%8x vu_hd.back\n", h, v->vu_hd.back);


	rt_log("%s%8x vg_p\n", h, v->vg_p);
	if (v->vg_p)
		nmg_pr_vg(v->vg_p, h);

	Return;
}

void 
nmg_pr_vu(vu, h)
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

void 
nmg_pr_vu_briefly(vu, h)
struct vertexuse *vu;
char *h;
{
	struct vertex_g	*vg;

	MKPAD(h);
	NMG_CK_VERTEXUSE(vu);
	NMG_CK_VERTEX(vu->v_p);

	if( vg = vu->v_p->vg_p )  {
		NMG_CK_VERTEX_G(vg);
		rt_log("%sVERTEXUSE %8x, v=x%x, %f %f %f\n", h, vu, vu->v_p,
			V3ARGS(vg->coord) );
	} else {
		rt_log("%sVERTEXUSE %8x, v=x%x\n", h, vu, vu->v_p);
	}

	Return;
}

/*
 *			N M G _ C H E C K _ R A D I A L
 *
 *	check to see if all radial uses of an edge (within a shell) are
 *	properly oriented with respect to each other.
 *
 *	Return
 *	0	OK
 *	1	bad edgeuse mate
 *	2	unclosed space
 */
int
nmg_check_radial(eu)
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
 *	!0	status code from nmg_check_radial()
 */
int
nmg_ck_closed_surf(s)
struct shell *s;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	int		status;
	long		magic1;

	NMG_CK_SHELL(s);
	for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		NMG_CK_FACEUSE(fu);
		for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
			NMG_CK_LOOPUSE(lu);
			magic1 = RT_LIST_FIRST_MAGIC( &lu->down_hd );
			if (magic1 == NMG_EDGEUSE_MAGIC) {
				for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )  {
					if (status=nmg_check_radial(eu))
						return(status);
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
nmg_ck_closed_region(r)
struct nmgregion	*r;
{
	struct shell	*s;
	int		ret;

	NMG_CK_REGION(r);
	for( RT_LIST_FOR( s, shell, &r->s_hd ) )  {
		ret = nmg_ck_closed_surf( s );
		if( ret != 0 )  return(ret);
	}
	return(0);
}


/*
 *			N M G _ P O L Y T O N M G
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
struct shell *
nmg_polytonmg(fp, r, tol)
FILE *fp;
struct nmgregion	*r;
CONST struct rt_tol	*tol;
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
	struct model	*m;

	s = nmg_msv(r);
	m = s->r_p->m_p;
	nmg_kvu(s->vu_p);

	/* get number of points & number of facets in file */
	if (fscanf(fp, "%d %d", &num_pts, &num_facets) != 2)
		rt_bomb("polytonmg() Error in first line of poly file\n");
	else
		if (rt_g.NMG_debug & DEBUG_POLYTO)
			rt_log("points: %d  facets: %d\n",
				num_pts, num_facets);


	v = (struct vertex **) rt_calloc(num_pts, sizeof (struct vertex *),
			"vertices");

	/* build the vertices */ 
	for (i = 0 ; i < num_pts ; ++i) {
		GET_VERTEX(v[i], m);
		v[i]->magic = NMG_VERTEX_MAGIC;
	}

	/* read in the coordinates of the vertices */
	for (i=0 ; i < num_pts ; ++i) {
		if (fscanf(fp, "%lg %lg %lg", &p[0], &p[1], &p[2]) != 3)
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
		if (fscanf(fp, "%d", &pts_this_face) != 1)
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
			if (fscanf(fp, "%d", &j) != 1)
				rt_bomb("polytonmg() error getting point index for v in f");
			vl[i] = v[j-1];
		}

		fu = nmg_cface(s, vl, pts_this_face);
		lu = RT_LIST_FIRST( loopuse, &fu->lu_hd );
		/* XXX should check for vertex-loop */
		eu = RT_LIST_FIRST( edgeuse, &lu->down_hd );
		NMG_CK_EDGEUSE(eu);
		if (rt_mk_plane_3pts(plane, eu->vu_p->v_p->vg_p->coord,
		    RT_LIST_PNEXT(edgeuse,eu)->vu_p->v_p->vg_p->coord,
		    RT_LIST_PLAST(edgeuse,eu)->vu_p->v_p->vg_p->coord,
		    tol ) )  {
			rt_log("At %d in %s\n", __LINE__, __FILE__);
			rt_bomb("polytonmg() cannot make plane equation\n");
		}
		else nmg_face_g(fu, plane);
	}

	for (i=0 ; i < num_pts ; ++i) {
		if( RT_LIST_IS_EMPTY( &v[i]->vu_hd ) )  continue;
		FREE_VERTEX(v[i]);
	}
	rt_free( (char *)v, "vertex array");
	return(s);
}

/*				N M G _ L U _ O F _ V U 
 *
 *	Given a vertexuse, return the loopuse somewhere above
 */
struct loopuse *
nmg_lu_of_vu(vu)
struct vertexuse *vu;
{
	NMG_CK_VERTEXUSE(vu);
	
	if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC &&
		*vu->up.eu_p->up.magic_p == NMG_LOOPUSE_MAGIC)
			return(vu->up.eu_p->up.lu_p);
	else if (*vu->up.magic_p != NMG_LOOPUSE_MAGIC)
		rt_bomb("NMG vertexuse has no loopuse ancestor\n");

	return(vu->up.lu_p);		
}

/*				N M G _ L U P S
 *
 *	return parent shell for loopuse
 */
struct shell *
nmg_lups(lu)
struct loopuse *lu;
{
	if (*lu->up.magic_p == NMG_SHELL_MAGIC) return(lu->up.s_p);
	else if (*lu->up.magic_p != NMG_FACEUSE_MAGIC) 
		rt_bomb("bad parent for loopuse\n");

	return(lu->up.fu_p->s_p);
}

/*				N M G _ E U P S 
 *
 *	return parent shell of edgeuse
 */
struct shell *
nmg_eups(eu)
struct edgeuse *eu;
{
	if (*eu->up.magic_p == NMG_SHELL_MAGIC) return(eu->up.s_p);
	else if (*eu->up.magic_p != NMG_LOOPUSE_MAGIC)
		rt_bomb("bad parent for edgeuse\n");

	return(nmg_lups(eu->up.lu_p));
}

/*				N M G _ F A C E R A D I A L
 *
 *	Looking radially around an edge, find another edge in the same
 *	face as the current edge. (this could be the mate to the current edge)
 */
struct edgeuse *
nmg_faceradial(eu)
struct edgeuse *eu;
{
	struct faceuse *fu;
	struct edgeuse *eur;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_LOOPUSE(eu->up.lu_p);
	NMG_CK_FACEUSE(eu->up.lu_p->up.fu_p);
	fu = eu->up.lu_p->up.fu_p;

	eur = eu->radial_p;

	while (*eur->up.magic_p != NMG_LOOPUSE_MAGIC ||
	    *eur->up.lu_p->up.magic_p != NMG_FACEUSE_MAGIC ||
	    eur->up.lu_p->up.fu_p->f_p != fu->f_p)
	    	eur = eur->eumate_p->radial_p;

	return(eur);
}


/*
 *	looking radially around an edge, find another edge which is a part
 *	of a face in the same shell
 */
struct edgeuse *
nmg_radial_face_edge_in_shell(eu)
struct edgeuse *eu;
{
	struct edgeuse *eur;
	struct faceuse *fu;

	NMG_CK_EDGEUSE(eu);
	NMG_CK_LOOPUSE(eu->up.lu_p);
	NMG_CK_FACEUSE(eu->up.lu_p->up.fu_p);

	fu = eu->up.lu_p->up.fu_p;
	eur = eu->radial_p;
	NMG_CK_EDGEUSE(eur);

	while (eur != eu->eumate_p) {
		if (*eur->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    *eur->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC &&
		    eur->up.lu_p->up.fu_p->s_p == fu->s_p)
			break; /* found another face in shell */
		else {
			eur = eur->eumate_p->radial_p;
			NMG_CK_EDGEUSE(eur);
		}
	}
	return(eur);
}


/*
 *			N M G _ E U P R I N T
 */
void 
nmg_euprint(str, eu)
char *str;
struct edgeuse *eu;
{
	pointp_t eup, matep;
	
	NMG_CK_EDGEUSE(eu);
	NMG_CK_VERTEXUSE(eu->vu_p);
	NMG_CK_VERTEX(eu->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->vu_p->v_p->vg_p);

	NMG_CK_EDGEUSE(eu->eumate_p);
	NMG_CK_VERTEXUSE(eu->eumate_p->vu_p);
	NMG_CK_VERTEX(eu->eumate_p->vu_p->v_p);
	NMG_CK_VERTEX_G(eu->eumate_p->vu_p->v_p->vg_p);

	eup = eu->vu_p->v_p->vg_p->coord;
	matep = eu->eumate_p->vu_p->v_p->vg_p->coord;

	rt_log("%s (%g, %g, %g -> %g, %g, %g)\n", str, eup[0], eup[1], eup[2],
		matep[0], matep[1], matep[2]);
}

/*
 *			N M G _ R E B O U N D
 *
 *  Re-compute all the bounding boxes in the NMG model.
 *  Bounding boxes are presently found in these structures:
 *	loop_g
 *	face_g
 *	shell_a
 *	nmgregion_a
 *  The re-bounding must be performed in a bottom-up manner,
 *  computing the loops first, and working up to the nmgregions.
 */
void
nmg_rebound( m )
struct model	*m;
{
	struct nmgregion	*r;
	struct shell		*s;
	struct faceuse		*fu;
	struct face		*f;
	struct loopuse		*lu;
	struct loop		*l;
	register int		*flags;

	NMG_CK_MODEL(m);

	flags = (int *)rt_calloc( m->maxindex, sizeof(int), "rebound flags[]" );

	for( RT_LIST_FOR( r, nmgregion, &m->r_hd ) )  {
		NMG_CK_REGION(r);
		for( RT_LIST_FOR( s, shell, &r->s_hd ) )  {
			NMG_CK_SHELL(s);

			/* Loops in faces in shell */
			for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
				NMG_CK_FACEUSE(fu);
				/* Loops in face */
				for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
					NMG_CK_LOOPUSE(lu);
					l = lu->l_p;
					NMG_CK_LOOP(l);
					if( NMG_INDEX_FIRST_TIME(flags,l) )
						nmg_loop_g(l);
				}
			}
			/* Faces in shell */
			for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
				NMG_CK_FACEUSE(fu);
				f = fu->f_p;
				NMG_CK_FACE(f);

				/* Rebound the face */
				if( NMG_INDEX_FIRST_TIME(flags,f) )
					nmg_face_bb( f );
			}

			/* Wire loops in shell */
			for( RT_LIST_FOR( lu, loopuse, &s->lu_hd ) )  {
				NMG_CK_LOOPUSE(lu);
				l = lu->l_p;
				NMG_CK_LOOP(l);
				if( NMG_INDEX_FIRST_TIME(flags,l) )
					nmg_loop_g(l);
			}

			/*
			 *  Rebound the shell.
			 *  This routine handles wire edges and lone vertices.
			 */
			if( NMG_INDEX_FIRST_TIME(flags,s) )
				nmg_shell_a( s );
		}

		/* Rebound the nmgregion */
		nmg_region_a( r );
	}

	rt_free( (char *)flags, "rebound flags[]" );
}

/*
 *			N M G _ C O U N T _ S H E L L _ K I D S
 */
void
nmg_count_shell_kids(m, total_faces, total_wires, total_points)
struct model *m;
unsigned long *total_wires;
unsigned long *total_faces;
unsigned long *total_points;
{
	short *tbl;

	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;

	NMG_CK_MODEL(m);

	tbl = (short *)rt_calloc(m->maxindex+1, sizeof(char),
		"face/wire/point counted table");

	*total_faces = *total_wires = *total_points = 0;
	for (RT_LIST_FOR(r, nmgregion, &m->r_hd)) {
		for (RT_LIST_FOR(s, shell, &r->s_hd)) {
			if (s->vu_p) {
				total_points++;
				continue;
			}
			for (RT_LIST_FOR(fu, faceuse, &s->fu_hd)) {
				if (NMG_INDEX_TEST_AND_SET(tbl, fu->f_p))
					(*total_faces)++;
			}
			for (RT_LIST_FOR(lu, loopuse, &s->lu_hd)) {
				if (NMG_INDEX_TEST_AND_SET(tbl, lu->l_p))
					(*total_wires)++;
			}
			for (RT_LIST_FOR(eu, edgeuse, &s->eu_hd)) {
				if (NMG_INDEX_TEST_AND_SET(tbl, eu->e_p))
					(*total_wires)++;
			}
		}
	}

	rt_free((char *)tbl, "face/wire/point counted table");
}
