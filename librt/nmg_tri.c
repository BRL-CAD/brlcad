/*	N M G _ T R I . C --- Triangulate the faces of a polygonal NMG
 *
 */
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

#define NMG_PT2D_MAGIC	0x2d2d2d2d
#define NMG_TRAP_MAGIC	0x4d4d4d4d
#define	NMG_CK_PT2D(_p)	NMG_CKMAG(_p, NMG_PT2D_MAGIC, "pt2d")
struct pt2d {
	struct rt_list	l;		/* scanline ordered list of points */
	fastf_t	coord[3];
	struct vertexuse *vu_p;
	long next, prev;		/* Index #'s of next/prev vu's in loop */
	struct trap *trap1, *trap2;
};

struct trapezoid {
	struct rt_list l;
	struct pt2d *top, *bot;
	struct edgeuse *left, *right;	/* edges really */
};

struct trap {
	long	magic;
	struct pt2d	*top_v, *bot_v;	/* top/bottom vertex of trapezoid */
	struct edgeuse	*eu_l, *eu_r;	/* left/right edgeuses of trapezoid */
	struct trap	*lc, *rc;	/* left/right child of trapezoid */
	struct trap	*np;		/* new trapezoid tree outside trap. */
};

/* macros for comparing 2D points in scanline order */

#define PgtV(_p, _v) \
	(((_p)->coord[Y] > (_v)->coord[Y]) || ((_p)->coord[Y] == (_v)->coord[Y] && (_p)->coord[X] < (_v)->coord[X]))
#define PltV(_p, _v) \
	(((_p)->coord[Y] < (_v)->coord[Y]) || ((_p)->coord[Y] == (_v)->coord[Y] && (_p)->coord[X] > (_v)->coord[X]))
#define PgeV(_p, _v) \
	(((_p)->coord[Y] > (_v)->coord[Y]) || ((_p)->coord[Y] == (_v)->coord[Y] && (_p)->coord[X] <= (_v)->coord[X]))
#define PleV(_p, _v) \
	(((_p)->coord[Y] < (_v)->coord[Y]) || ((_p)->coord[Y] == (_v)->coord[Y] && (_p)->coord[X] >= (_v)->coord[X]))

void nmg_plot_flat_face();

static FILE *plot_fd;
static int flatten_debug=1;
static int tri_debug = 1;
	
nmg_pr_trap(s, tp, tbl2d)
char *s;
struct trap *tp;
struct pt2d tbl2d[];
{
	struct edgeuse *eu;

	if (s)
		fprintf(stderr, "%s\n", s);

	if (!tp)
		fprintf(stderr, "Null trapezoid pointer\n");

	(void)fprintf(stderr, "0x%0x\n", tp->magic);

	if (tp->top_v)
		(void)fprintf(stderr, "\ttop_v %g %g\n", tp->top_v->coord[0], tp->top_v->coord[1]);
	else
		(void)fprintf(stderr, "\tnull top_v pointer\n");


	if (tp->bot_v)
		(void)fprintf(stderr, "\tbot_v %g %g\n", tp->bot_v->coord[0], tp->bot_v->coord[1]);
	else
		(void)fprintf(stderr, "\tnull bot_v pointer\n");


	if (tp->eu_l) {
		eu = RT_LIST_PNEXT_CIRC(edgeuse, &tp->eu_l->l);
		fprintf(stderr, "\teu_l %g %g -> %g %g\n",
			tbl2d[tp->eu_l->vu_p->index].coord[0],
			tbl2d[tp->eu_l->vu_p->index].coord[1],

			tbl2d[eu->vu_p->index].coord[0],
			tbl2d[eu->vu_p->index].coord[1]);
	} else
		(void)fprintf(stderr, "\tNull eu_l edgeuse\n");



	if (tp->eu_r) {
		eu = RT_LIST_PNEXT_CIRC(edgeuse, &tp->eu_r->l);
		fprintf(stderr, "\teu_r %g %g -> %g %g\n",
			tbl2d[tp->eu_r->vu_p->index].coord[0],
			tbl2d[tp->eu_r->vu_p->index].coord[1],

			tbl2d[eu->vu_p->index].coord[0],
			tbl2d[eu->vu_p->index].coord[1]);
	} else
		(void)fprintf(stderr, "\tNull eu_r edgeuse\n");

}

/*	N M G _ F I N D _ F U _ O F _ V U
 *
 *	return a pointer to the parent faceuse of the vertexuse
 *	or a null pointer if vu is not a child of a faceuse.
 */
struct faceuse *
nmg_find_fu_of_vu(vu)
struct vertexuse *vu;
{
	struct loopuse *lu_p;
	NMG_CK_VERTEXUSE(vu);

	switch (*vu->up.magic_p) {
	case NMG_SHELL_MAGIC:
		return ((struct faceuse *)NULL);
		break;
	case NMG_LOOPUSE_MAGIC:
		lu_p = vu->up.lu_p;
gotaloop:
		switch (*lu_p->up.magic_p) {
		case NMG_FACEUSE_MAGIC:
			return (vu->up.lu_p->up.fu_p);
		case NMG_SHELL_MAGIC:
			return ((struct faceuse *)NULL);
			break;
		default:
			fprintf(stderr,
				"Error at %s %d:\nInvalid loopuse parent magic (0x%x %d)\n",
				__FILE__, __LINE__,
				*vu->up.lu_p->up.magic_p,
				*vu->up.lu_p->up.magic_p);
			abort();
			break;
		}
		break;
	case NMG_EDGEUSE_MAGIC:
		switch (*vu->up.eu_p->up.magic_p) {
		case NMG_LOOPUSE_MAGIC:
			lu_p = vu->up.eu_p->up.lu_p;
			goto gotaloop;
			break;
		case NMG_SHELL_MAGIC:
			return ((struct faceuse *)NULL);
			break;
		default:
			fprintf(stderr,
				"Error at %s %d:\nInvalid loopuse parent magic 0x%x\n",
				__FILE__, __LINE__,
				*vu->up.lu_p->up.magic_p);
			abort();
			break;
		}
		break;
	default:
		fprintf(stderr,
			"Error at %s %d:\nInvalid vertexuse parent magic 0x%x\n",
			__FILE__, __LINE__,
			*vu->up.magic_p);
		abort();
		break;
	}
}


/*	N M G _ V U _ I S _ I N _ F U
 *
 *	Returns 1 if vu is child of fu, 0 otherwise
 */
nmg_vu_is_in_fu(vu_p, fu)
struct vertexuse *vu_p;
struct faceuse *fu;
{
	return (nmg_find_fu_of_vu(vu_p) == fu);
}



/*	M A P _ V U _ T O _ 2 D
 *
 *	Add a vertex to the 2D table if it isn't already there.
 */
static void
map_vu_to_2d(vu, tbl2d, mat, fu, hv)
struct vertexuse *vu;
struct pt2d *tbl2d;
mat_t mat;
struct faceuse *fu;
{
	point_t	pt;
	struct vertex_g *vg;
	struct vertexuse *vu_p;
	struct edgeuse *eu;
	struct pt2d *p;

	/* if this vertexuse has already been transformed, we're done */
	if (RT_LIST_MAGIC_OK(&tbl2d[vu->index].l, NMG_PT2D_MAGIC) )
		return;

	/* transform the 3-D vertex into a 2-D vertex */
	vg = vu->v_p->vg_p;
	MAT4X3PNT(tbl2d[vu->index].coord, mat, vg->coord);
	if (flatten_debug)
		(void)fprintf(stderr, "old (%g, %g, %g) new (%g, %g, %g)\n",
			V3ARGS(vg->coord), V3ARGS(tbl2d[vu->index].coord) );

	tbl2d[vu->index].coord[2] = 0.0;
	tbl2d[vu->index].vu_p = vu;

	/* find location in scanline ordered list for vertex */
	for (RT_LIST_FOR(p, pt2d, &tbl2d[0].l)) {
		if (PgtV(p, &tbl2d[vu->index])) continue;
		RT_LIST_INSERT(&p->l, &tbl2d[vu->index].l);
		RT_LIST_MAGIC_SET(&tbl2d[vu->index].l, NMG_PT2D_MAGIC);
		break;
	}
	if (RT_LIST_MAGIC_WRONG(&tbl2d[vu->index].l, NMG_PT2D_MAGIC)) {
		RT_LIST_INSERT( &tbl2d[0].l, &tbl2d[vu->index].l);
		RT_LIST_MAGIC_SET(&tbl2d[vu->index].l, NMG_PT2D_MAGIC);
	}

	/* if vu part of edge in loop remember next/prev vertexuse in
	 * loop.
	 */
	if (*vu->up.magic_p == NMG_EDGEUSE_MAGIC &&
	     *vu->up.eu_p->up.magic_p == NMG_LOOPUSE_MAGIC) {
	     
		eu = RT_LIST_PNEXT_CIRC(edgeuse, &vu->up.eu_p->l);
		tbl2d[vu->index].next = eu->vu_p->index;
		eu = RT_LIST_PLAST_CIRC(edgeuse, &vu->up.eu_p->l);
		tbl2d[vu->index].prev = eu->vu_p->index;
	}

	/* for all other uses of this vertex in this face, store the
	 * transformed 2D vertex
	 */
	for (RT_LIST_FOR(vu_p, vertexuse, &vu->v_p->vu_hd)) {
		if (!nmg_vu_is_in_fu(vu_p, fu)) continue;

		RT_LIST_APPEND(&tbl2d[vu->index].l, &tbl2d[vu_p->index].l);
		RT_LIST_MAGIC_SET(&tbl2d[vu_p->index].l, NMG_PT2D_MAGIC);
		VMOVE(tbl2d[vu_p->index].coord, tbl2d[vu->index].coord);
		tbl2d[vu_p->index].vu_p = vu_p;

		/* if vu_p part of edge in loop remember next/prev
		 * vertexuse in loop.
		 */
		if (*vu_p->up.magic_p == NMG_EDGEUSE_MAGIC &&
		     *vu_p->up.eu_p->up.magic_p == NMG_LOOPUSE_MAGIC) {
	     
			eu = RT_LIST_PNEXT_CIRC(edgeuse, &vu_p->up.eu_p->l);
			tbl2d[vu_p->index].next = eu->vu_p->index;
			eu = RT_LIST_PLAST_CIRC(edgeuse, &vu_p->up.eu_p->l);
			tbl2d[vu_p->index].prev = eu->vu_p->index;
		}
	}
}

/*	N M G _ F L A T T E N _ F A C E
 *
 *	Create the 2D coordinate table for the vertexuses of a face.
 *
 *	---------	-----------------------------------
 *	|pt2d --+-----> |     struct pt2d.{magic,coord[3]} |
 *	---------	-----------------------------------
 *			|     struct pt2d.{magic,coord[3]} |
 *			-----------------------------------
 *			|     struct pt2d.{magic,coord[3]} |
 *			-----------------------------------
 *	
 *	When the caller is done, nmg_free_2d_map() should be called to dispose
 *	of the map
 */
struct pt2d *
nmg_flatten_face(fu, TformMat)
struct faceuse *fu;
mat_t		TformMat;
{
	CONST vect_t twoDspace = { 0.0, 0.0, 1.0 };
	struct pt2d *tbl2d;
	struct vertexuse *vu;
	struct loopuse *lu;
	struct edgeuse *eu;

	NMG_CK_FACEUSE(fu);


	tbl2d = (struct pt2d *)rt_calloc(fu->s_p->r_p->m_p->maxindex,
		sizeof(struct pt2d), "2D coordinate table");

	/* we use the 0 index entry in the table as the head of the sorted
	 * list of verticies.  This is safe since the 0 index is always for
	 * the model structure
	 */

	RT_LIST_INIT( &tbl2d[0].l );

	/* construct the matrix that maps the 3D coordinates into 2D space */
	mat_fromto( TformMat, twoDspace, fu->f_p->fg_p->N );
	if (flatten_debug)
		mat_print( "TformMat", TformMat );


	/* convert each vertex in the face to its 2-D equivalent */
	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC) {
			vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
			map_vu_to_2d(vu, tbl2d, TformMat);

		} else if (RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_EDGEUSE_MAGIC) {
			for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
				vu = eu->vu_p;
				map_vu_to_2d(vu, tbl2d, TformMat);
			}
		} else rt_bomb("bad magic of loopuse child\n");
	}

	return(tbl2d);
}





/*
 *
 *	characterize the edges which meet at this vertex.
 *
 *	  1 	     2	       3	   4	    5
 *
 *      /- -\	  -------		-\   /-     \---/
 *     /-- --\	  ---O---	O	--\ /--      \-/
 *    O--- ---O	  --/ \--      /-\	---O---       O
 *     \-- --/	  -/   \-     /---\	-------
 *      \- -/	
 *
 */
int
vtype2d(v, tbl2d)
struct pt2d * v;
struct pt2d *tbl2d;
{
	struct pt2d *p, *n;	/* previous/this edge endpoints */
	struct edgeuse *eu;

	/* get the indicies of the 2D versions of the verticies
	 * on the other ends of the edgeuses which meet at this 
	 * 2D vertexuse.
	 *
	 * XXX parent of vertex might be loopuse
	 */
	if (*v->vu_p->up.magic_p != NMG_EDGEUSE_MAGIC ||
	    *v->vu_p->up.eu_p->up.magic_p != NMG_LOOPUSE_MAGIC ) {
		printf("oops\n");
		abort();
	}

	n = &tbl2d[v->next];
	p = &tbl2d[v->prev];

	NMG_CK_PT2D(n);
	NMG_CK_PT2D(p);

	if (PgtV(n, v) && PgtV(p, v)) {
		if (v->vu_p->up.eu_p->up.lu_p->orientation == OT_OPPOSITE)
			return(4);
		else if (v->vu_p->up.eu_p->up.lu_p->orientation == OT_SAME)
			return(5);
	}

	if (PltV(n, v) && PltV(p, v)) {
		if (v->vu_p->up.eu_p->up.lu_p->orientation == OT_OPPOSITE)
			return(2);
		else if (v->vu_p->up.eu_p->up.lu_p->orientation == OT_SAME)
			return(3);
	}
	if ( (PgtV(n, v) && PltV(p, v)) || (PltV(n, v) && PgtV(p, v)) )
		return(1);

	fprintf(stderr,
		"%s %d HELP! special case:\n(%g %g)->(%g %g)\n(%g %g)->(%g %g)\n",
		__FILE__, __LINE__,
		p->coord[X], p->coord[Y],
		v->coord[X], v->coord[Y],
		n->coord[X], n->coord[Y]);

	return(0);
}

/*
 *	if "prev_trap" is incomplete and has an edgeuse of "upper_eu"
 *	then complete "prev_trap" and create a new trapezoid as a child
 *	of "prev_trap"
 */
int
new_type1_trap(pt, prev_trap, upper_eu, lower_eu)
struct pt2d pt;
struct trap *prev_trap;
struct edgeuse *upper_eu, *lower_eu;
{
#if 0
	/* if trapezoid complete, it can't be the right one */
	if (prev_trap->bot_v)
		return (0);

	/* trapezoid is incomplete, check to see if it uses proper edgesue */
	if (prev_pt->eu_l == upper_eu) {
		/* complete trapezoid */
	    	prev_trap->bot_v = pt;

		/* start new trapezoid */
		new_trap = (struct trap *)
			rt_calloc(1, sizeof(struct trap),
			"new trapezoid");

		new_trap->top = pt;

	    	new_trap->eu_l = lower_eu;
	    	new_trap->eu_r = prev_trap->eu_r;
		new_trap->magic = NMG_TRAP_MAGIC;

		/* make new trapezoid child of old one */
		prev_trap->lc = new;

	    	return(1);
	} else if (prev_pt->eu_r == upper_eu) {
		/* complete trapezoid */
	    	prev_trap->bot_v = pt;

		/* start new trapezoid */
		new_trap = (struct trap *)
			rt_calloc(1, sizeof(struct trap),
			"new trapezoid");

		new_trap->top = pt;

	    	new_trap->eu_l = prev_trap->eu_l;
	    	new_trap->eu_r = lower_eu;
		new_trap->magic = NMG_TRAP_MAGIC;

		/* make new trapezoid child of old one */
		prev_trap->rc = new;

	    	return(1);
	}
	return(0);
#endif
}

/*
 *		^
 *	  /-	-\
 *	 /--	--\
 *	O---	---O
 *	 \--	--/
 *	  \-	-/
 *	  V
 *
 *	finish trapezoid from vertex, start new trapezoid from vertex
 */
void
type1vertex(pt, tbl2d, trap)
struct pt2d *pt, *tbl2d;
struct trap *trap;
{
	struct trap *tp, *new;
	struct edgeuse *upper_eu, *lower_eu;
	struct edgeuse *other_eu;
	struct pt2d *n, *p;
	struct pt2d *prev_pt;

	if (tri_debug)
		fprintf(stderr, "%g %g is type 1 vertex\n",
			pt->coord[X], pt->coord[Y]);

	n = &tbl2d[pt->next];
	p = &tbl2d[pt->prev];

	if (PgeV(n, pt) && PleV(p, pt)) {
	    /* rising */
	    if (tri_debug) {
		other_eu = RT_LIST_PNEXT_CIRC(edgeuse, &pt->vu_p->up.eu_p->l);
		fprintf(stderr, "(%g %g)\n\t<- (%g %g)\n\t\t<- ",
			tbl2d[other_eu->vu_p->index].coord[0],
			tbl2d[other_eu->vu_p->index].coord[1],
			pt->coord[0],
			pt->coord[1]);
		other_eu = RT_LIST_PLAST_CIRC(edgeuse, &pt->vu_p->up.eu_p->l);
		fprintf(stderr, "(%g %g)\n",
			tbl2d[other_eu->vu_p->index].coord[0],
			tbl2d[other_eu->vu_p->index].coord[1]);
	    }
	    upper_eu = pt->vu_p->up.eu_p;
	    lower_eu = RT_LIST_PLAST_CIRC(edgeuse, &pt->vu_p->up.eu_p->l);
	} else {
	    /* decending */
	    if (tri_debug) {
		other_eu = RT_LIST_PLAST_CIRC(edgeuse, &pt->vu_p->up.eu_p->l);
		fprintf(stderr, "(%g %g) ->\n\t(%g %g) ->\n\t\t",
			tbl2d[other_eu->vu_p->index].coord[0],
			tbl2d[other_eu->vu_p->index].coord[1],
			pt->coord[0],
			pt->coord[1]);
		other_eu = RT_LIST_PNEXT_CIRC(edgeuse, &pt->vu_p->up.eu_p->l);
		fprintf(stderr, "(%g %g)\n",
			tbl2d[other_eu->vu_p->index].coord[0],
			tbl2d[other_eu->vu_p->index].coord[1]);
	    }
	    upper_eu = RT_LIST_PLAST_CIRC(edgeuse, &pt->vu_p->up.eu_p->l);
	    lower_eu = pt->vu_p->up.eu_p;
	}
#if 0

	/* search backwards in the sorted list of 2d vertex structures for
	 * the vertex which has an incomplete trapezoid with the upper 
	 * edge of this vertex.
	 */
	for (prev_pt = RT_LIST_PLAST(pt2d, pt) ;
	    RT_LIST_MAGIC(&prev_pt->l) != RT_LIST_HEAD_MAGIC ;
	    prev_pt = RT_LIST_PLAST(pt2d, prev) ) {

	    	prev_trap = prev_pt->trap2;

		if (prev_pt->trap2 &&
		    new_type1_trap(pt, prev_pt->trap2, upper_eu, lower_eu))
			return;

		if (prev_pt->trap1 &&
		    new_type1_trap(pt, prev_pt->trap1, upper_eu, lower_eu))
			return;
	}
	(void)fprintf(stderr,
		"%s %d didn't find trapezoid for type 1 vertex\n",
		__FILE__, __LINE__);
	abort();
#endif
}















pt_insize_trap(pt, tp)
struct pt2d *pt, *tp;
{
	vect_t lv, lp, rv, rp, result;
	/* form vector of left edge of trap */
	/* form vector from left edge start to pt */


	/* form vector of right edge of trap */
	/* form vector from right edge start to pt */

	VCROSS(result, lv, lp);
	/* if Z component is negative, point is inside */

	VCROSS(result, rv, rp);
	/* if Z component is negative, point is inside */
	

}
/*	Hole Start in polygon
 *
 *	-------
 *	---O---
 *	--/ \--
 *	-/   \-
 *	      V
 *
 *	Finish existing trapezoid, start 2 new ones
 */
struct trap
type2vertex(pt, tbl2d, trap)
struct pt2d *pt, *tbl2d;
struct trap *trap;
{
	struct trap *tp, *save;

	if (tri_debug)
		fprintf(stderr, "%g %g is type 2 vertex\n", 
			pt->coord[X], pt->coord[Y]);

#if 0

	save = (struct trap *)NULL;
	for (RT_LIST_FOR(tp, trap, &partial->l)) {
		if (PleV(tp, pt))
			save = tp;

		if (pt_inside_trap(pt, tp)) {
			/* finish this trapezoid & start 2 new ones */
			/* XXX */
			return;
		}
	}
#endif
	(void)fprintf(stderr,
		"%s %d Didn't find trapezoid for type 2 vertex\n",
		__FILE__, __LINE__);
	abort();
}

/*	Polygon point start.
 *
 *	  O
 *	 /-\
 *	/---\
 *	    V
 *
 *	start new trapezoid
 */
struct trap
type3vertex(pt, tbl2d, trap)
struct pt2d *pt, *tbl2d;
struct trap *trap;
{
	struct trap *tp;

	if (tri_debug)
		fprintf(stderr, "%g %g is type 3 vertex\n",
				pt->coord[X], pt->coord[Y]);

	/* make a single new trapezoid */
	tp = (struct trap *)
		rt_calloc(1, sizeof(struct trap), "new trapezoid");

	tp->top_v = pt;
	tp->eu_l = pt->vu_p->up.eu_p;
	tp->eu_r = RT_LIST_PLAST_CIRC(edgeuse, pt->vu_p->up.eu_p);
	tp->magic = NMG_TRAP_MAGIC;


	pt->trap1 = tp;
	pt->trap2 = (struct trap *)NULL;


	if (RT_LIST_LAST_MAGIC(&pt->l) != RT_LIST_HEAD_MAGIC) {
		struct pt2d *prev_pt;
		struct trap *parent_trap;	/* disney movie */

		/* attach this trapezoid to the "np" pointer of the
		 * right-most trapezoid of the previous vertex
		 * in the sorted list.
		 */

		prev_pt = RT_LIST_LAST(trap, &pt->l);

		if (prev_pt->trap2)
			parent_trap = prev_pt->trap2;
		else
			parent_trap = prev_pt->trap1;

		parent_trap->np = tp;
	}


	if (tri_debug)
		nmg_pr_trap("New trapezoid", tp, tbl2d);

	return (tp);

}

/*	Close hole
 *
 *	^
 *	-\   /-
 *	--\ /--
 *	---O---
 *	-------
 *
 *	complete right and left trapezoids
 *	start new trapezoid
 *
 */
struct trap
type4vertex(pt, tbl2d, trap)
struct pt2d *pt, *tbl2d;
struct trap *trap;
{
	if (tri_debug)
		fprintf(stderr, "%g %g is type 4 vertex\n",
			pt->coord[X], pt->coord[Y]);
}



/*	Polygon point end.
 *
 *	     ^
 *	\---/
 *	 \-/
 *	  O
 *
 *	complete trapezoid
 */
struct trap
type5vertex(pt, tbl2d, trap)
struct pt2d *pt, *tbl2d;
struct trap *trap;
{
	struct trap *tp;
	struct edgeuse *my_left, *my_right;

	if (tri_debug)
		fprintf(stderr, "%g %g is type 5 vertex\n",
			pt->coord[X], pt->coord[Y]);

	my_right = pt->vu_p->up.eu_p;
	my_left = RT_LIST_PLAST_CIRC(edgeuse,  pt->vu_p->up.eu_p);

	/* find the trapezoid which has these two edges in it */


	/* complete the trapezoid */



	(void)fprintf(stderr,
		"%s %d Didn't find trapezoid for type 5 vertex\n",
		__FILE__, __LINE__);
	abort();
}


/*
 *	N M G _ T R A P _ F A C E 
 *
 *	Produce the trapezoidal decomposition of a face from the set of
 *	2D points.
 */
static void
nmg_trap_face(tbl2d)
struct pt2d *tbl2d;
{
	struct pt2d *pt;
	struct trap *traptree
	


	for (RT_LIST_FOR(pt, pt2d, &tbl2d[0].l)) {
		NMG_CK_PT2D(pt);
		switch(vtype2d(pt, tbl2d)) {
		case 1:
			type1vertex(pt, tbl2d, traptree);
			break;
		case 2:
			type2vertex(pt, tbl2d, traptree);
			break;
		case 3:
			if (!traptree)
				traptree = type3vertex(pt, tbl2d, traptree);
			else
				(void)type3vertex(pt, tbl2d, traptree);
			break;
		case 4:
			type4vertex(pt, tbl2d, traptree);
			break;
		case 5:
			type5vertex(pt, tbl2d, traptree);
			break;
		default:
			fprintf(stderr, "%g %g is UNKNOWN type vertex\n",
				pt->coord[X], pt->coord[Y]);
			break;
		}
	}

}


void
nmg_triangulate_face(fu)
struct faceuse *fu;
{
	mat_t TformMat;
	struct pt2d *tbl2d;

	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		nmg_split_touchingloops(lu);
	}

	tbl2d = nmg_flatten_face(fu, TformMat);

	nmg_plot_flat_face(fu, tbl2d, "tri1.pl");

	nmg_trap_face(tbl2d);

	rt_free((char *)tbl2d, "discard tbl2d");
}

void
nmg_plot_flat_face(fu, tbl2d, filename)
struct faceuse *fu;
struct pt2d *tbl2d;
char *filename;
{
	struct loopuse *lu;
	struct edgeuse *eu;

	if ((plot_fd=fopen(filename, "w")) == (FILE *)NULL) {
		perror(filename);
		abort();
	}

	pd_3space(plot_fd, -300.0, -300.0, -1.0, 100.0, 100.0, 1.0);

	pl_color(plot_fd, 200, 200, 200);

	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC)
			continue;
		for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd ) ) {
			register struct edgeuse *eu_p;

			eu_p = RT_LIST_PNEXT_CIRC(edgeuse, &eu->l);

			pdv_3line(plot_fd,
				    tbl2d[eu->vu_p->index].coord,
				    tbl2d[eu_p->vu_p->index].coord);
		}
	}
}
