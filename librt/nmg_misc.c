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
 *	John R. Anderson
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

NMG_EXTERN( struct faceuse *nmg_find_fu_of_eu , (struct edgeuse *eu) );

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
		b->magic = NMG_PTBL_MAGIC;
		b->buffer = (long **)rt_calloc(b->blen=64,
						sizeof(p), "pointer table");
		b->end = 0;
		return(0);
	} else if (func == TBL_RST) {
		NMG_CK_PTBL(b);
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

		NMG_CK_PTBL(b);
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

		NMG_CK_PTBL(b);
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

		NMG_CK_PTBL(b);
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

		NMG_CK_PTBL(b);
		for (l = b->end-1 ; l >= 0 ; --l)
			if (pp[l] == p){
				/* delete occurrence(s) of p */

				j=l+1;
				while (pp[l-1] == p) --l;

				end -= j - l;
#				include "noalias.h"
				for(k=l ; j <= end ;)
					b->buffer[k++] = b->buffer[j++];
			}

		b->end = end;
		return(0);
	} else if (func == TBL_CAT) {
		union {
			long *l;
			struct nmg_ptbl *t;
		} d;

		NMG_CK_PTBL(b);
		d.l = p;

		if ((b->blen - b->end) < d.t->end) {
			
			b->buffer = (long **)rt_realloc( (char *)b->buffer,
				sizeof(p)*(b->blen += d.t->blen),
				"pointer table (CAT)");
		}
		bcopy(d.t->buffer, &b->buffer[b->end], d.t->end*sizeof(p));
		return(0);
	} else if (func == TBL_FREE) {
		NMG_CK_PTBL(b);
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
nmg_purge_unwanted_intersection_points(vert_list, fu, tol)
struct nmg_ptbl		*vert_list;
CONST struct faceuse	*fu;
CONST struct rt_tol	*tol;
{
	int			i;
	int			j;
	struct vertexuse	*vu;
	struct loopuse		*lu;
	CONST struct loop_g	*lg;
	CONST struct loopuse	*fu2lu;
	CONST struct loop_g	*fu2lg = (CONST struct loop_g *)NULL;
	int			overlap = 0;

	NMG_CK_FACEUSE(fu);
	RT_CK_TOL(tol);

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

			switch(fu2lu->orientation)  {
			case OT_BOOLPLACE:
				/*  If this loop is destined for removal
				 *  by sanitize(), skip it.
				 */
				continue;
			case OT_UNSPEC:
				/* If this is a loop of a lone vertex,
				 * it was deposited into the
				 * other loop as part of an intersection
				 * operation, and is quite important.
				 */
				if( RT_LIST_FIRST_MAGIC(&fu2lu->down_hd) != NMG_VERTEXUSE_MAGIC )
					rt_log("nmg_purge_unwanted_intersection_points() non self-loop OT_UNSPEC vertexuse in fu2?\n");
				break;
			case OT_SAME:
			case OT_OPPOSITE:
				break;
			default:
				rt_log("nmg_purge_unwanted_intersection_points encountered %s loop in fu2\n",
					nmg_orientation(fu2lu->orientation));
				/* Process it anyway */
				break;
			}

			fu2lg = fu2lu->l_p->lg_p;
			NMG_CK_LOOP_G(fu2lg);
			if (V3RPP_OVERLAP_TOL(fu2lg->min_pt, fu2lg->max_pt,
			    lg->min_pt, lg->max_pt, tol)) {
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
				rt_log("nmg_purge_unwanted_intersection_points This little bugger slipped in somehow.  Deleting it from the list.\n");
				nmg_pr_vu_briefly(vu, (char *)NULL);
			}
			if( RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC &&
			    lu->orientation == OT_UNSPEC )  {
				/* Drop this loop of a single vertex in sanitize() */
				if (rt_g.NMG_debug & DEBUG_POLYSECT)
					rt_log("nmg_purge_unwanted_intersection_points() remarking as OT_BOOLPLACE\n");
				lu->orientation =
				  lu->lumate_p->orientation = OT_BOOLPLACE;
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

/*
 *			N M G _ P R _ O R I E N T
 *
 *	Print the orientation in a nice, english form
 */
void 
nmg_pr_orient(orientation, h)
int		orientation;
CONST char	*h;
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

/*
 *			N M G _ P R _ M
 */
void 
nmg_pr_m(m)
CONST struct model *m;
{
	CONST struct nmgregion *r;

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

/*
 *			M K P A D
 *
 *  NOTE:  All the nmg_pr_*() routines take an "h" (header string) pointer.
 *  This can be an arbitrary caller-provided string, as long as it is kept
 *  short.  The string will be copied over into nmg_pr_padstr[], and
 *  "h" will be changed to point there, so spaces can be added to the end.
 */
static char nmg_pr_padstr[128];
#define MKPAD(_h) { \
	if (!_h) { _h = nmg_pr_padstr; nmg_pr_padstr[0] = '\0'; } \
	else if( (_h) < nmg_pr_padstr || (_h) >= nmg_pr_padstr+sizeof(nmg_pr_padstr) )  { \
		(void)strncpy(nmg_pr_padstr, (_h), sizeof(nmg_pr_padstr)/2); \
		_h = nmg_pr_padstr; \
	} else { if (strlen(_h) < sizeof(nmg_pr_padstr)-4) (void)strcat(_h, "   "); } }

#define Return	{ h[strlen(h)-3] = '\0'; return; }

/*
 *			N M G _ P R _ R
 */
void 
nmg_pr_r(r, h)
CONST struct nmgregion *r;
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

/*
 *			N M G _ P R _ S A
 */
void 
nmg_pr_sa(sa, h)
CONST struct shell_a *sa;
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

/*
 *			N M G _ P R _ L G
 */
void 
nmg_pr_lg(lg, h)
CONST struct loop_g *lg;
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

/*
 *			N M G _ P R _ F G
 */
void 
nmg_pr_fg(fg, h)
CONST struct face_g *fg;
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

/*
 *			N M G _ P R _ S
 */
void 
nmg_pr_s(s, h)
CONST struct shell *s;
char *h;
{
	CONST struct faceuse	*fu;
	CONST struct loopuse	*lu;
	CONST struct edgeuse	*eu;

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

/*
 *			N M G _ P R _ S _ B R I E F L Y
 */
void 
nmg_pr_s_briefly(s, h)
CONST struct shell *s;
char *h;
{
	CONST struct faceuse	*fu;
	CONST struct loopuse	*lu;
	CONST struct edgeuse	*eu;

	MKPAD(h);
	
	rt_log("%sSHELL %8x\n", h, s);
	if (!s || s->l.magic != NMG_SHELL_MAGIC) {
		rt_log("bad shell magic\n");
		Return;
	}

	for( RT_LIST_FOR( fu, faceuse, &s->fu_hd ) )  {
		nmg_pr_fu_briefly(fu, h);
	}

	for( RT_LIST_FOR( lu, loopuse, &s->lu_hd ) )  {
		nmg_pr_lu_briefly(lu, h);
	}

	for( RT_LIST_FOR( eu, edgeuse, &s->eu_hd ) )  {
		nmg_pr_eu_briefly(eu, h);
	}
	if (s->vu_p)
		nmg_pr_vu_briefly(s->vu_p, h);

	Return;
}

/*
 *			N M G _ P R _ F
 */
void 
nmg_pr_f(f, h)
CONST struct face *f;
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

/*
 *			N M G _ P R _ F U
 */
void 
nmg_pr_fu(fu, h)
CONST struct faceuse *fu;
char *h;
{
	CONST struct loopuse *lu;

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

/*
 *			N M G _ P R _ F U _ B R I E F L Y
 */
void 
nmg_pr_fu_briefly(fu, h)
CONST struct faceuse *fu;
char *h;
{
	CONST struct loopuse *lu;

	MKPAD(h);
	NMG_CK_FACEUSE(fu);

	rt_log("%sFACEUSE %8x (%s)\n",
		h, fu, nmg_orientation(fu->orientation));

	for( RT_LIST_FOR( lu, loopuse, &fu->lu_hd ) )  {
		nmg_pr_lu_briefly(lu, h);
	}
	Return;
}

/*
 *			N M G _ P R _ L
 */
void 
nmg_pr_l(l, h)
CONST struct loop *l;
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

/*
 *			N M G _ P R _ L U
 */
void 
nmg_pr_lu(lu, h)
CONST struct loopuse *lu;
char *h;
{
	CONST struct edgeuse	*eu;
	CONST struct vertexuse *vu;
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

/*
 *			N M G _ P R _ L U _ B R I E F L Y
 */
void 
nmg_pr_lu_briefly(lu, h)
CONST struct loopuse *lu;
char *h;
{
	CONST struct edgeuse	*eu;
	CONST struct vertexuse *vu;
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

/*
 *			N M G _ P R _ E G
 */
void
nmg_pr_eg(eg, h)
CONST struct edge_g *eg;
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

/*
 *			N M G _ P R _ E
 */
void 
nmg_pr_e(e, h)
CONST struct edge *e;
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

/*
 *			N M G _ P R _ E U
 */
void 
nmg_pr_eu(eu, h)
CONST struct edgeuse *eu;
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

/*
 *			N M G _ P R _ E U _ B R I E F L Y
 */
void 
nmg_pr_eu_briefly(eu, h)
CONST struct edgeuse *eu;
char *h;
{
	MKPAD(h);
	NMG_CK_EDGEUSE(eu);

	rt_log("%sEDGEUSE %8x\n", h, eu);
	nmg_pr_vu_briefly(eu->vu_p, h);

	Return;
}

/*
 *			N M G _ P R _ V G
 */
void 
nmg_pr_vg(vg, h)
CONST struct vertex_g *vg;
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

/*
 *			N M G _ P R _ V
 */
void 
nmg_pr_v(v, h)
CONST struct vertex *v;
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

/*
 *			N M G _ P R _ V U
 */
void 
nmg_pr_vu(vu, h)
CONST struct vertexuse *vu;
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

/*
 *			N M G _ P R _ V U _ B R I E F L Y
 */
void 
nmg_pr_vu_briefly(vu, h)
CONST struct vertexuse *vu;
char *h;
{
	CONST struct vertex_g	*vg;

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
 *			N M G _ E U P R I N T
 */
void 
nmg_euprint(str, eu)
CONST char		*str;
CONST struct edgeuse	*eu;
{
	CONST fastf_t	*eup;
	CONST fastf_t	*matep;
	
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
CONST struct model *m;
unsigned long *total_wires;
unsigned long *total_faces;
unsigned long *total_points;
{
	short *tbl;

	CONST struct nmgregion *r;
	CONST struct shell *s;
	CONST struct faceuse *fu;
	CONST struct loopuse *lu;
	CONST struct edgeuse *eu;

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

/*
 *	O R D E R _ T B L
 *
 *	private support routine for nmg_close_shell
 *	creates an array of indices into a table of edgeuses, ordered
 *	to create a loop.
 *
 *	Arguments:
 *	tbl is the table (provided by caller)
 *	index is the array of indices created by order_tbl
 *	tbl_size is the size of the table (provided by caller)
 *	loop_size is the number of edgeuses in the loop (calculated by order_tbl)
 */
static void
order_tbl( struct nmg_ptbl *tbl , int **index , int tbl_size , int *loop_size )
{
	int i,j;
	int found;
	struct edgeuse *eu,*eu1;

	/* create an index into the table, ordered to create a loop */
	if( *index == NULL )
		(*index) = (int *)rt_calloc( tbl_size , sizeof( int ) , "Table index" );

	for( i=0 ; i<tbl_size ; i++ )
		(*index)[i] = (-1);

	/* start the loop at index = 0 */
	(*index)[0] = 0;
	*loop_size = 1;
	eu = (struct edgeuse *)NMG_TBL_GET( tbl , 0 );
	found = 1;
	i = 0;
	while( found )
	{
		found = 0;

		/* Look for edgeuse that starts where "eu" ends */
		for( j=1 ; j<tbl_size ; j++ )
		{
			eu1 = (struct edgeuse *)NMG_TBL_GET( tbl , j );
			if( eu1->vu_p->v_p == eu->eumate_p->vu_p->v_p )
			{
				/* Found it */
				found = 1;
				(*index)[++i] = j;
				(*loop_size)++;
				eu = eu1;
				break;
			}
		}
	}
}

/*
 *	N M G _ C L O S E _ S H E L L
 *
 *	Examines the passed shell and, if there are holes, closes them
 *	note that note much care is taken as to how the holes are closed
 *	so the results are not entirely predictable.
 *	A list of free edges is created (edges bounding only one face).
 *	New faces are constructed by taking two consecutive edges
 *	and making a face. The newly created edge is added to the list
 *	of free edges and the two used ones are removed.
 *
 */
void
nmg_close_shell( struct shell *s , struct rt_tol *tol )
{
	struct nmg_ptbl eu_tbl;		/* table of free edgeuses from shell */
	struct nmg_ptbl vert_tbl;	/* table of vertices for use in nmg_cface */
	int *index;			/* array of indices into eu_tbl, ordered to form a loop */
	int loop_size;			/* number of edgeueses in loop */
	struct faceuse **fu_list;	/* array of pointers to faceuses, for use in nmg_gluefaces */
	int fu_counter=0;		/* number of faceuses in above array */
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu,*eu1,*eu2,*eu3,*eu_new;
	int i,j;
	int found;

	index = NULL;

	NMG_CK_SHELL( s );

	/* construct a table of free edges */
	(void)nmg_tbl( &eu_tbl , TBL_INIT , NULL );

	/* loop through all the faces in the shell */
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		/* only look at OT_SAME faces */
		if( fu->orientation == OT_SAME )
		{
			/* count 'em */
			fu_counter++;
			/* loop through each loopuse in the face */
			for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );
				/* ignore loops that are just a vertex */
				if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) ==
					NMG_VERTEXUSE_MAGIC )
						continue;

				/* loop through all the edgeuses in the loop */
				for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					NMG_CK_EDGEUSE( eu );
					/* if this edgeuse is a free edge, add its mate to the list */
					if( eu->radial_p == eu->eumate_p )
						(void)nmg_tbl( &eu_tbl , TBL_INS , (long *) eu->eumate_p );
				}
			}
		}
	}

	/* if there is nothing in our list of free edges, the shell is already closed */
	if( NMG_TBL_END( &eu_tbl ) == 0 )
	{
		nmg_tbl( &eu_tbl , TBL_FREE , NULL );
		return;
	}

	/* put all the existing faces in a list (needed later for "nmg_gluefaces") */
	fu_list = (struct faceuse **)rt_calloc( fu_counter + NMG_TBL_END( &eu_tbl ) - 2 , sizeof( struct faceuse *) , "face use list " );
	fu_counter = 0;
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_SAME )
			fu_list[fu_counter++] = fu;
	}

	/* create a table of vertices */
	(void)nmg_tbl( &vert_tbl , TBL_INIT , NULL );

	while( NMG_TBL_END( &eu_tbl ) )
	{
		vect_t normal,v1,v2,tmp_norm;
		int give_up_on_face=0;

		/* Create an index into the table that orders the edgeuses into a loop */
		order_tbl( &eu_tbl , &index , NMG_TBL_END( &eu_tbl ) , &loop_size );

		/* Create new faces to close the shell */
		while( loop_size > 3 )
		{
			vect_t inside;			/* vector pointing to left of edge (inside face) */
			struct edgeuse **eu_used;	/* array of edgueses used, for deletion */
			vect_t v1,v2;			/* edge vectors */
			int edges_used;			/* number of edges used in making a face */
			int found_face=0;		/* flag - indicates that a face with the correct normal will be created */
			int start_index,end_index;	/* start and stop index for loop */
			int coplanar;			/* flag - indicates entire loop is coplanar */
			plane_t pl1,pl2;		/* planes for checking coplanarity of loop */
			point_t pt[3];			/* points for calculating planes */

			/* Look for an easy way out, maybe this loop is planar */
			/* first, calculate a plane from the first three non-collinear vertices */
			start_index = 0;
			end_index = start_index + 3;
			
			for( i=start_index ; i<end_index ; i++ )
			{
				eu = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
				VMOVE( pt[i-start_index] , eu->vu_p->v_p->vg_p->coord );
			}
			while( rt_mk_plane_3pts( pl1 , pt[0] , pt[1] , pt[2] , tol ) && end_index<loop_size )
			{
				start_index++;
				end_index++;
				for( i=start_index ; i<end_index ; i++ )
				{
					eu = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
					VMOVE( pt[i-start_index] , eu->vu_p->v_p->vg_p->coord );
				}
			}
			if( end_index == loop_size )
			{
				/* Could not even make a plane, this is some serious screw-up */
				rt_bomb( "nmg_close_shell: cannot make any planes from loop\n" );
			}

			/* now we have one plane, let's check the others */
			coplanar = 1;
			while( end_index < loop_size && coplanar )
			{
				end_index +=3;
				if( end_index > loop_size )
					end_index = loop_size;
				start_index = end_index - 3;

				for( i=start_index ; i<end_index ; i++ )
				{
					eu = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
					VMOVE( pt[i-start_index] , eu->vu_p->v_p->vg_p->coord );
				}

				/* if these three points make a plane, is it coplanar with
				 * our first one??? */
				if( !rt_mk_plane_3pts( pl2 , pt[0] , pt[1] , pt[2] , tol ) )
				{
					if( rt_coplanar( pl1 , pl2 , tol ) < 1 )
						coplanar = 0;
				}
			}

			if( coplanar )	/* excellent! - just make one big face */
			{
				/* put vertices in table */
				nmg_tbl( &vert_tbl , TBL_RST , NULL );
				for( i=0 ; i<loop_size ; i++ )
				{
					eu = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
					nmg_tbl( &vert_tbl , TBL_INS , (long *)eu->vu_p->v_p );
				}

				/* make face */
				fu = nmg_cface( s , (struct vertex **)NMG_TBL_BASEADDR(&vert_tbl) , loop_size );

				/* already have face geometry, so don't need to call nmg_fu_planeeqn */
				nmg_face_g( fu , pl1 );

				/* add this face to list for glueing */
				fu_list[fu_counter++] = fu;

				/* now eliminate loop from table */
				eu_used = (struct edgeuse **)rt_calloc( loop_size , sizeof( struct edguse *) , "edges used list" );
				for( i=0 ; i<loop_size ; i++ )
					eu_used[i] = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );

				for( i=0 ; i<loop_size ; i++ )
					nmg_tbl( &eu_tbl , TBL_RM , (long *)eu_used[i] );

				rt_free( (char *)eu_used , "edge used list" );

				/* set some flags to get us back to start of loop */
				found_face = 1;
				give_up_on_face = 1;
			}

			/* OK, so we have to do this one little-by-little */
			start_index = 0;
			while( !found_face )
			{
				/* refresh the vertex list */
				(void)nmg_tbl( &vert_tbl , TBL_RST , NULL );

				end_index = start_index + 1;
				if( end_index == loop_size )
					end_index = 0;

				/* Get two edgeuses from the loop */
				eu1 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[start_index] );
				nmg_tbl( &vert_tbl , TBL_INS , (long *)eu1->vu_p->v_p );

				VSUB2( v1 , eu1->eumate_p->vu_p->v_p->vg_p->coord , eu1->vu_p->v_p->vg_p->coord );
				VCROSS( inside , normal , v1 );

				eu2 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[end_index] );
				nmg_tbl( &vert_tbl , TBL_INS , (long *)eu2->vu_p->v_p );

				edges_used = 2;	
				/* if the edges are collinear, we can't make a face */
				while( rt_3pts_collinear(
					eu1->vu_p->v_p->vg_p->coord,
					eu2->vu_p->v_p->vg_p->coord,
					eu2->eumate_p->vu_p->v_p->vg_p->coord,
					tol ) && edges_used < loop_size )
				{
					/* So, add another edge */
					end_index++;
					if( end_index == loop_size )
						end_index = 0;
					eu1 = eu2;
					eu2 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[end_index]);
					nmg_tbl( &vert_tbl , TBL_INS , (long *)eu2->vu_p->v_p );
					edges_used++;
				}

				found_face = 1;
				VSUB2( v2 , eu2->eumate_p->vu_p->v_p->vg_p->coord , eu2->vu_p->v_p->vg_p->coord );
				fu = nmg_find_fu_of_eu( eu1 );
				VCROSS( inside , fu->f_p->fg_p->N , v1 );
				if( VDOT( inside , v2 ) > 0.0 )
				{
					/* this face normal would be in the wrong direction */
					found_face = 0;

					/* move along the loop by one edge and try again */
					start_index++;
					if( start_index > loop_size-2 )
					{
						/* can't make a face from this loop, so delete it */
						eu_used = (struct edgeuse **)rt_calloc( loop_size , sizeof( struct edguse *) , "edges used list" );
						for( i=0 ; i<loop_size ; i++ )
							eu_used[i] = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
						for( i=0 ; i<loop_size ; i++ )
							nmg_tbl( &eu_tbl , TBL_RM , (long *)eu_used[i] );

						rt_free( (char *)eu_used , "edge used list" );

						give_up_on_face = 1;
						break;
					}
				}
			}

			if( give_up_on_face )
				break;			

			/* add last vertex to table */
			nmg_tbl( &vert_tbl , TBL_INS , (long *)eu2->eumate_p->vu_p->v_p );

			/* save list of used edges to be removed later */
			eu_used = (struct edgeuse **)rt_calloc( edges_used , sizeof( struct edguse *) , "edges used list" );
			for( i=0 ; i<edges_used ; i++ )
				eu_used[i] = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );

			/* make a face */
			fu = nmg_cface( s , (struct vertex **)NMG_TBL_BASEADDR(&vert_tbl) , edges_used+1 );
			if( nmg_fu_planeeqn( fu , tol ) )
				rt_log( "Failed planeeq\n" );

			/* add new face to the list of faces */
			fu_list[fu_counter++] = fu;

			/* find the newly created edgeuse */
			found = 0;
			for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );
				if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) ==
					NMG_VERTEXUSE_MAGIC )
						continue;
				for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					NMG_CK_EDGEUSE( eu );
					if( eu->vu_p->v_p == (struct vertex *)NMG_TBL_GET( &vert_tbl , 0 )
					&& eu->eumate_p->vu_p->v_p == (struct vertex *)NMG_TBL_GET( &vert_tbl , edges_used) )
					{
						eu_new = eu;
						found = 1;
						break;
					}
					else if( eu->vu_p->v_p == (struct vertex *)NMG_TBL_GET( &vert_tbl , edges_used)
					&& eu->eumate_p->vu_p->v_p == (struct vertex *)NMG_TBL_GET( &vert_tbl , 0 ) )
					{
						eu_new = eu->eumate_p;
						found = 1;
						break;
					}

				}
				if( found )
					break;
			}

			/* out with the old, in with the new */
			for( i=0 ; i<edges_used ; i++ )
				nmg_tbl( &eu_tbl , TBL_RM , (long *)eu_used[i] );
			nmg_tbl( &eu_tbl , TBL_INS , (long *)eu_new );

			rt_free( (char *)eu_used , "edge used list" );

			/* re-order loop */
			order_tbl( &eu_tbl , &index , NMG_TBL_END( &eu_tbl ) , &loop_size );
		}

		if( give_up_on_face )
			continue;

		if( loop_size != 3 )
		{
			rt_log( "Error, loop size should be 3\n" );
			nmg_tbl( &eu_tbl , TBL_FREE , NULL );
			nmg_tbl( &vert_tbl , TBL_FREE , NULL );
			rt_free( (char *)index , "index" );
			rt_free( (char *)fu_list , "faceuse list " );
			return;
		}

		/* if the last 3 vertices are collinear, then don't make the last face */
		nmg_tbl( &vert_tbl , TBL_RST , NULL );
		for( i=0 ; i<3 ; i++ )
		{
			eu = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[i] );
			(void)nmg_tbl( &vert_tbl , TBL_INS , (long *)eu->vu_p->v_p);
		}

		if( !rt_3pts_collinear(
			((struct vertex *)NMG_TBL_GET( &vert_tbl , 0 ))->vg_p->coord,
			((struct vertex *)NMG_TBL_GET( &vert_tbl , 1 ))->vg_p->coord,
			((struct vertex *)NMG_TBL_GET( &vert_tbl , 2 ))->vg_p->coord,
			tol ) )
		{
		
			/* Create last face from remaining 3 edges */
			fu = nmg_cface( s , (struct vertex **)NMG_TBL_BASEADDR(&vert_tbl) , 3 );
			if( nmg_fu_planeeqn( fu , tol ) )
				rt_log( "Failed planeeq\n" );

			/* and add it to the list */
			fu_list[fu_counter++] = fu;

		}

		/* remove the last three edges from the table */
		eu1 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[0] );
		eu2 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[1] );
		eu3 = (struct edgeuse *)NMG_TBL_GET( &eu_tbl , index[2] );
		nmg_tbl( &eu_tbl , TBL_RM , (long *)eu1 );
		nmg_tbl( &eu_tbl , TBL_RM , (long *)eu2 );
		nmg_tbl( &eu_tbl , TBL_RM , (long *)eu3 );
	}

	/* finally, glue it all together */
	nmg_gluefaces( fu_list , fu_counter );

	/* Free up all the memory */
	rt_free( (char *)index , "index" );
	rt_free( (char *)fu_list , "faceuse list " );
	nmg_tbl( &eu_tbl , TBL_FREE , NULL );
	nmg_tbl( &vert_tbl , TBL_FREE , NULL );

	/* we may have constructed some coplanar faces */
	nmg_shell_coplanar_face_merge( s , tol , 1 );
	if( nmg_simplify_shell( s ) )
	{
		rt_log( "nmg_close_shell(): Simplified shell is empty" );
		return;
	}
}

/*
 *	N M G _ M E R G E _ S H E L L S
 *
 *	Move everything from source shell to destination
 *	shell, then destroy source shell
 *
 */
void
nmg_merge_shells( struct shell *dst , struct shell *src )
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct vertexuse *vu;

	NMG_CK_SHELL( src );
	NMG_CK_SHELL( dst );

	while( RT_LIST_NON_EMPTY( &src->fu_hd ) )
	{
		fu = RT_LIST_FIRST( faceuse , &src->fu_hd );
		NMG_CK_FACEUSE( fu );
		nmg_mv_fu_between_shells( dst , src , fu );
	}

	while( RT_LIST_NON_EMPTY( &src->lu_hd ) )
	{
		lu = RT_LIST_FIRST( loopuse , &src->lu_hd );
		NMG_CK_LOOPUSE( lu );
		nmg_mv_lu_between_shells( dst , src , lu );
	}

	while( RT_LIST_NON_EMPTY( &src->eu_hd ) )
	{
		eu = RT_LIST_FIRST( edgeuse , &src->eu_hd );
		NMG_CK_EDGEUSE( eu );
		nmg_mv_eu_between_shells( dst , src , eu );
	}

	if( src->vu_p )
	{
		NMG_CK_VERTEXUSE( src->vu_p );
		nmg_mv_vu_between_shells( dst , src , src->vu_p );
	}

	nmg_ks( src );
}

/*
 *	N M G _ D U P _ S H E L L
 *
 *	Duplicate a shell and return the new copy. New shell is
 *	in the same region.
 *
 *  The vertex geometry is copied from the source faces into topologically
 *  distinct (new) vertex and vertex_g structs.
 *  They will start out being geometricly coincident, but it is anticipated
 *  that the caller will modify the geometry, e.g. as in an extrude operation.
 */
struct shell *
nmg_dup_shell( struct shell *s )
{
	struct model *m;
	struct shell *new_s;
	struct faceuse *fu;
	struct loopuse *lu,*new_lu;
	struct edgeuse *eu;
	struct faceuse *new_fu;
	long ** trans_tbl;
	struct nmg_ptbl faces;

	NMG_CK_SHELL( s );

	m = nmg_find_model( (long *)s );

	/* create translation table double size to accomodate both copies */
	trans_tbl = (long **)rt_calloc(m->maxindex*2, sizeof(long *),
		"nmg_dup_shell trans_tbl" );

	nmg_tbl( &faces , TBL_INIT , NULL );

	new_s = nmg_msv( s->r_p );

	/* copy face uses */
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_SAME )
		{
			new_fu = (struct faceuse *)NULL;
			for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			{
				NMG_CK_LOOPUSE( lu );
				if( new_fu )
					new_lu = nmg_dup_loop( lu , &new_fu->l.magic , trans_tbl );
				else
				{
					new_lu = nmg_dup_loop( lu , &new_s->l.magic , trans_tbl );
					new_fu = nmg_mf( new_lu );
				}
			}
			if (fu->f_p->fg_p)
				nmg_face_g(new_fu, fu->f_p->fg_p->N);
			new_fu->orientation = fu->orientation;
			new_fu->fumate_p->orientation = fu->fumate_p->orientation;
			nmg_tbl( &faces , TBL_INS , (long *)new_fu );
		}
	}

	/* glue new faces */
	nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ) );
	nmg_tbl( &faces , TBL_FREE , NULL );

	/* copy wire loops */
	for( RT_LIST_FOR( lu , loopuse , &s->lu_hd ) )
	{
		NMG_CK_LOOPUSE( lu );
		new_lu = nmg_dup_loop( lu , &new_s->l.magic , trans_tbl );
	}

	/* copy wire edges */
	for( RT_LIST_FOR( eu , edgeuse , &s->eu_hd ) )
	{
		struct vertex *old_v1,*old_v2,*new_v1,*new_v2;
		struct edgeuse *new_eu;

		NMG_CK_EDGEUSE( eu );
		NMG_CK_VERTEXUSE( eu->vu_p );
		NMG_CK_VERTEX( eu->vu_p->v_p );
		NMG_CK_EDGEUSE( eu->eumate_p );
		NMG_CK_VERTEXUSE( eu->eumate_p->vu_p );
		NMG_CK_VERTEX( eu->eumate_p->vu_p->v_p );

		old_v1 = eu->vu_p->v_p;
		new_v1 = NMG_INDEX_GETP(vertex, trans_tbl, old_v1);
		old_v2 = eu->eumate_p->vu_p->v_p;
		new_v2 = NMG_INDEX_GETP(vertex, trans_tbl, old_v2);

		/* make the wire edge */
		new_eu = nmg_me( new_v1 , new_v2 , new_s );

		new_v1 = new_eu->vu_p->v_p;
		NMG_INDEX_ASSIGN( trans_tbl , old_v1 , (long *)new_v1 );

		new_v2 = new_eu->eumate_p->vu_p->v_p;
		NMG_INDEX_ASSIGN( trans_tbl , old_v2 , (long *)new_v2 );

	}

#if 0
	/* XXX for this to work nmg_mvu and nmg_mvvu must not be private
	 *     perhaps there is another way???? */
	/* copy vertex use
	 * This must be done last, since other routines may steal it */
	if( s->vu_p )
	{
		old_vu = s->vu_p;
		NMG_CK_VERTEXUSE( old_vu );
		old_v = old_vu->v_p;
		NMG_CK_VERTEX( old_v );
		new_v = NMG_INDEX_GETP(vertex, trans_tbl, old_v);
		if( new_v )
		{
			/* already copied vertex, just need a use */
			if( new_s->vu_p )
				(void )nmg_kvu( new_s->vu_p );
			new_s->vu_p = nmg_mvu( new_v , (long *)new_s , m );
		}
		else
			new_s->vu_p = nmg_mvvu( (long *)new_s , m );
	}
#endif
	

	/* free some memory */
	rt_free( (char *)trans_tbl , "nmg_dup_shell: trans_tbl" );

	return( new_s );
}
