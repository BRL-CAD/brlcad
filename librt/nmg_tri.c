/*	N M G _ T R I . C --- Triangulate the faces of a polygonal NMG
 *
 */
#include <stdio.h>
#include <values.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"

/* macros for comparing 2D points in scanline order */
#define P_GT_V(_p, _v) \
	(((_p)->coord[Y] > (_v)->coord[Y]) || ((_p)->coord[Y] == (_v)->coord[Y] && (_p)->coord[X] < (_v)->coord[X]))
#define P_LT_V(_p, _v) \
	(((_p)->coord[Y] < (_v)->coord[Y]) || ((_p)->coord[Y] == (_v)->coord[Y] && (_p)->coord[X] > (_v)->coord[X]))
#define P_GE_V(_p, _v) \
	(((_p)->coord[Y] > (_v)->coord[Y]) || ((_p)->coord[Y] == (_v)->coord[Y] && (_p)->coord[X] <= (_v)->coord[X]))
#define P_LE_V(_p, _v) \
	(((_p)->coord[Y] < (_v)->coord[Y]) || ((_p)->coord[Y] == (_v)->coord[Y] && (_p)->coord[X] >= (_v)->coord[X]))

#define NMG_PT2D_MAGIC	0x2d2d2d2d
#define NMG_TRAP_MAGIC  0x1ab1ab
#define	NMG_CK_PT2D(_p)	NMG_CKMAG(_p, NMG_PT2D_MAGIC, "pt2d")
#define	NMG_CK_TRAP(_p)	{NMG_CKMAG(_p, NMG_TRAP_MAGIC, "trap");\
	if ( ! RT_LIST_PREV(rt_list, &(_p)->l) ) {\
		rt_log("%s %d bad prev pointer of trapezoid 0x%08x\n",\
			__FILE__, __LINE__, &(_p)->l);\
		rt_bomb("aborting");\
	} else if (! RT_LIST_NEXT(rt_list, &(_p)->l) ) {\
		rt_log("%s %d bad next pointer of trapezoid 0x%08x\n",\
			__FILE__, __LINE__, &(_p)->l);\
		rt_bomb("aborting");\
	}}

/* macros to retrieve the next/previous 2D point about loop */
#define PT2D_NEXT(tbl, pt) pt2d_pn(tbl, pt, 1)
#define PT2D_PREV(tbl, pt) pt2d_pn(tbl, pt, -1)

struct pt2d {
	struct rt_list	l;		/* scanline ordered list of points */
	fastf_t	coord[3];		/* point coordinates in 2-D space */
	struct vertexuse *vu_p;		/* associated vertexuse */
};


struct trap {
	struct rt_list	l;
	struct pt2d	*top;	   /* point at top of trapezoid */
	struct pt2d	*bot;	   /* point at bottom of trapezoid */
	struct edgeuse	*e_left;
	struct edgeuse	*e_right;
};

/* if there is an edge in this face which connects the points
 *	return 1
 * else
 *	return 0
 */

/* The "ray" here is the intersection line between two faces */
struct nmg_ray_state {
	struct vertexuse	**vu;		/* ptr to vu array */
	int			nvu;		/* len of vu[] */
	point_t			pt;		/* The ray */
	vect_t			dir;
	struct edge_g		*eg_p;		/* Edge geom of the ray */
	struct shell		*sA;
	struct shell		*sB;
	struct faceuse		*fu1;
	struct faceuse		*fu2;
	vect_t			left;		/* points left of ray, on face */
	int			state;		/* current (old) state */
	int			last_action;	/* last action taken */
	vect_t			ang_x_dir;	/* x axis for angle measure */
	vect_t			ang_y_dir;	/* y axis for angle measure */
	CONST struct rt_tol	*tol;
};


/* subroutine version to pass to the rt_tree functions */
PvsV(p, v)
struct trap *p, *v;
{
	NMG_CK_TRAP(p);
	NMG_CK_TRAP(v);

	if (p->top->coord[Y] > v->top->coord[Y]) return(1);
	else if (p->top->coord[Y] < v->top->coord[Y]) return(-1);
	else if (p->top->coord[X] < v->top->coord[X]) return(1);
	else if (p->top->coord[X] > v->top->coord[X]) return(-1);
	else	return(0);
}


static struct pt2d *find_pt2d();	
static FILE *plot_fd;

void
print_2d_eu(s, eu, tbl2d)
char *s;
struct edgeuse *eu;
struct rt_list *tbl2d;
{
	struct pt2d *pt, *pt_next;

	NMG_CK_EDGEUSE(eu);

	pt = find_pt2d(tbl2d, eu->vu_p);
	pt_next = find_pt2d(tbl2d, (RT_LIST_PNEXT_CIRC(edgeuse, eu))->vu_p);
	rt_log("%s: 0x%08x %g %g -> %g %g\n", s, eu,
		pt->coord[X], pt->coord[Y],
		pt_next->coord[X], pt_next->coord[Y]);	
}


void
print_trap(tp, tbl2d)
struct trap *tp;
struct rt_list *tbl2d;
{
	struct pt2d *pt, *pt_next;

	NMG_CK_TRAP(tp);

	rt_log("trap top pt2d:0x%08x %g %g vu:0x%08x\n",
			&tp->top, tp->top->coord[X], tp->top->coord[Y],
			tp->top->vu_p);

	if (tp->bot)
		rt_log("\tbot pt2d:0x%08x %g %g vu:0x%08x\n",
			&tp->bot, tp->bot->coord[X], tp->bot->coord[Y],
			tp->bot->vu_p);
	else {
		rt_log("\tbot (nil)\n");
	}
			
	if (tp->e_left)
		print_2d_eu("\te_left", tp->e_left, tbl2d);

	if (tp->e_right)
		print_2d_eu("\te_right", tp->e_right, tbl2d);
}
static void
print_tlist(tbl2d, tlist)
struct rt_list *tbl2d, *tlist;
{
	struct trap *tp;
	struct pt2d *pt, *pt_next;

	rt_log("Trapezoid list start ----------\n");
	for (RT_LIST_FOR(tp, trap, tlist)) {
		NMG_CK_TRAP(tp);
		print_trap(tp, tbl2d);
	}
	rt_log("Trapezoid list end ----------\n");
}

static int flatten_debug=0;
static int tri_debug = 1;


static struct pt2d *
find_pt2d(tbl2d, vu)
struct rt_list *tbl2d;
struct vertexuse *vu;
{
	struct pt2d *p;
	NMG_CK_VERTEXUSE(vu);

	for (RT_LIST_FOR(p, pt2d, tbl2d)) {
		if (p->vu_p == vu) {
			return p;
		}
	}
	return (struct pt2d *)NULL;
}

static void
plfu( fu, tbl2d )
struct faceuse *fu;
struct rt_list *tbl2d;
{
	static int fileno=0;
	FILE *fd;
	char name[25];
	char buf[80];
	long *b;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct pt2d *p;
	
	
	sprintf(name, "tri%d.pl", fileno++);
	if ((fd=fopen(name, "w")) == (FILE *)NULL) {
		perror(name);
		abort();
	}

	rt_log("\tplotting %s\n", name);
	b = (long *)rt_calloc( fu->s_p->r_p->m_p->maxindex,
		sizeof(long), "bit vec"),

	pl_erase(fd);
	pd_3space(fd,
		fu->f_p->fg_p->min_pt[0]-1.0,
		fu->f_p->fg_p->min_pt[1]-1.0,
		fu->f_p->fg_p->min_pt[2]-1.0,
		fu->f_p->fg_p->max_pt[0]+1.0,
		fu->f_p->fg_p->max_pt[1]+1.0,
		fu->f_p->fg_p->max_pt[2]+1.0);
	
	nmg_pl_fu(fd, fu, b, 255, 255, 255);

	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC){
			if (p=find_pt2d(tbl2d,RT_LIST_FIRST(vertexuse, &lu->down_hd))) {
				pd_move(fd, p->coord[0], p->coord[1]);
				sprintf(buf, "%g, %g",
					p->coord[0], p->coord[1]);
				pl_label(fd, buf);
			} else
				pl_label(fd, "X, Y");

		} else if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC){
			for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
				if (p=find_pt2d(tbl2d,eu->vu_p)) {
					pd_move(fd, p->coord[0], p->coord[1]);
					sprintf(buf, "%g, %g",
						p->coord[0], p->coord[1]);
					pl_label(fd, buf);
				} else
					pl_label(fd, "X, Y");
			}
		} else {
			rt_bomb("bogus loopuse child\n");
		}
	}


	rt_free((char *)b, "plot table");
	fclose(fd);
}



/*	P T 2 D _ P N
 *
 *	Return Prev/Next 2D pt about loop from given 2D pt.
 *	if vertex is child of loopuse, return parameter 2D pt.
 */
static struct pt2d *
pt2d_pn(tbl, pt, dir)
struct rt_list *tbl;
struct pt2d *pt;
int dir;
{
	struct edgeuse *eu, *eu_other;
	struct pt2d *new_pt;

	NMG_CK_PT2D( pt );
	NMG_CK_VERTEXUSE( (pt)->vu_p );

	if ( *(pt)->vu_p->up.magic_p == NMG_EDGEUSE_MAGIC) {
		eu = (pt)->vu_p->up.eu_p;
		NMG_CK_EDGEUSE( eu );
		if (dir < 0)
			eu_other = RT_LIST_PPREV_CIRC(edgeuse, eu);
		else
			eu_other = RT_LIST_PNEXT_CIRC(edgeuse, eu);

		new_pt = find_pt2d(tbl, eu_other->vu_p);
		if (new_pt == (struct pt2d *)NULL) {
			if (dir < 0)
				rt_log("can't find prev of %g %g\n",
					pt->coord[X],
					pt->coord[Y]);
			else
				rt_log("can't find next of %g %g\n",
					pt->coord[X],
					pt->coord[Y]);
			rt_bomb("goodbye\n");
		}
		NMG_CK_PT2D( new_pt );
		return new_pt;
	}

	if ( *(pt)->vu_p->up.magic_p != NMG_LOOPUSE_MAGIC) {
		rt_log("%s %d Bad vertexuse parent\n", __FILE__, __LINE__);
		rt_bomb("goodbye\n");
	}

	return pt;
}



/*	M A P _ V U _ T O _ 2 D
 *
 *	Add a vertex to the 2D table if it isn't already there.
 */
static void
map_vu_to_2d(vu, tbl2d, mat, fu)
struct vertexuse *vu;
struct rt_list *tbl2d;
mat_t mat;
struct faceuse *fu;
{
	point_t	pt;
	struct vertex_g *vg;
	struct vertexuse *vu_p;
	struct vertex *vp;
	struct edgeuse *eu;
	struct pt2d *p, *np;

	NMG_CK_VERTEXUSE(vu);
	NMG_CK_FACEUSE(fu);

	/* if this vertexuse has already been transformed, we're done */
	if (find_pt2d(tbl2d, vu)) return;


	np = (struct pt2d *)rt_calloc(1, sizeof(struct pt2d), "pt2d struct");
	np->coord[2] = 0.0;
	np->vu_p = vu;
	RT_LIST_MAGIC_SET(&np->l, NMG_PT2D_MAGIC);

	/* if one of the other vertexuses has been mapped, use that data */
	for (RT_LIST_FOR(vu_p, vertexuse, &vu->v_p->vu_hd)) {
		if (p = find_pt2d(tbl2d, vu_p)) {
			VMOVE(np->coord, p->coord);
			return;
		}
	}

	/* transform the 3-D vertex into a 2-D vertex */
	vg = vu->v_p->vg_p;
	MAT4X3PNT(np->coord, mat, vg->coord);


	if (flatten_debug) rt_log(
		"Transforming 0x%x old (%g, %g, %g) new (%g, %g, %g)\n",
		vu, V3ARGS(vg->coord), V3ARGS(np->coord) );

	/* find location in scanline ordered list for vertex */
	for ( RT_LIST_FOR(p, pt2d, tbl2d) ) {
		if (P_GT_V(p, np)) continue;
		break;
	}
	RT_LIST_INSERT(&p->l, &np->l);

	if (flatten_debug)
		rt_log("transforming other vertexuses...\n");

	/* for all other uses of this vertex in this face, store the
	 * transformed 2D vertex
	 */
	vp = vu->v_p;

	for (RT_LIST_FOR(vu_p, vertexuse, &vp->vu_hd)) {
		register struct faceuse *fu_of_vu;
		NMG_CK_VERTEXUSE(vu_p);

		if (vu_p == vu) continue;

		if (flatten_debug)
			rt_log("transform 0x%x... ", vu_p);

		fu_of_vu = nmg_find_fu_of_vu(vu_p);
		NMG_CK_FACEUSE(fu_of_vu);
		if (fu_of_vu != fu) {
			if (flatten_debug)
				rt_log("vertexuse not in faceuse (That's OK)\n");
			continue;
		}

		/* if vertexuse already transformed, skip it */
		if (find_pt2d(tbl2d, vu_p)) {
			if (flatten_debug) {
				rt_log("vertexuse already transformed\n", vu);
				nmg_pr_vu(vu, NULL);
			}
			continue;
		}

		/* add vertexuse to list */
		p = (struct pt2d *)rt_calloc(1, sizeof(struct pt2d), "pt2d");
		p->vu_p = vu_p;
		VMOVE(p->coord, np->coord);
		RT_LIST_MAGIC_SET(&p->l, NMG_PT2D_MAGIC);

		RT_LIST_APPEND(&np->l, &p->l);

		if (flatten_debug)
			(void)rt_log( "vertexuse transformed\n");
	}
	if (flatten_debug)
		(void)rt_log( "Done.\n");
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
struct rt_list *
nmg_flatten_face(fu, TformMat)
struct faceuse *fu;
mat_t		TformMat;
{
	CONST vect_t twoDspace = { 0.0, 0.0, 1.0 };
	struct rt_list *tbl2d;
	struct vertexuse *vu;
	struct loopuse *lu;
	struct edgeuse *eu;

	NMG_CK_FACEUSE(fu);

	tbl2d = (struct rt_list *)rt_calloc(1, sizeof(struct rt_list),
		"2D coordinate list");

	/* we use the 0 index entry in the table as the head of the sorted
	 * list of verticies.  This is safe since the 0 index is always for
	 * the model structure
	 */

	RT_LIST_INIT( tbl2d );

	/* construct the matrix that maps the 3D coordinates into 2D space */
	mat_fromto( TformMat, fu->f_p->fg_p->N, twoDspace );

	if (flatten_debug)
		mat_print( "TformMat", TformMat );


	/* convert each vertex in the face to its 2-D equivalent */
	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC) {
			vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
			map_vu_to_2d(vu, tbl2d, TformMat, fu);

		} else if (RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_EDGEUSE_MAGIC) {
			for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
				vu = eu->vu_p;
				map_vu_to_2d(vu, tbl2d, TformMat, fu);
			}
		} else rt_bomb("bad magic of loopuse child\n");
	}

	return(tbl2d);
}



#define POLY_SIDE 1
#define HOLE_START 2
#define POLY_START 3
#define HOLE_END 4
#define POLY_END 5
#define HOLE_POINT 6
#define POLY_POINT 7
/*
 *
 *	characterize the edges which meet at this vertex.
 *
 *	  1 	     2	       3	   4	    5	      6		7
 *
 *      /- -\	  -------		-\   /-     \---/  -------
 *     /-- --\	  ---O---	O	--\ /--      \-/   ---O---	O
 *    O--- ---O	  --/ \--      /-\	---O---       O	   -------
 *     \-- --/	  -/   \-     /---\	-------	
 *      \- -/	
 *
 */
int
vtype2d(v, tbl2d)
struct pt2d *v;
struct rt_list *tbl2d;
{
	struct pt2d *p, *n;	/* previous/this edge endpoints */
	struct edgeuse *eu;
	struct loopuse *lu;

	/* get the next/previous points relative to v */
	p = PT2D_PREV(tbl2d, v);
	n = PT2D_NEXT(tbl2d, v);

	if (p == n && n == v) {
		/* loopuse of vertexuse or loopuse of 1 edgeuse */
		if (lu->orientation == OT_SAME)
			return(POLY_POINT);
		else if (lu->orientation == OT_OPPOSITE)
			return(HOLE_POINT);
	}

	lu = nmg_find_lu_of_vu( v->vu_p );

	if (P_GT_V(n, v) && P_GT_V(p, v)) {
		if (lu->orientation == OT_OPPOSITE)
			return(HOLE_END);
		else if (lu->orientation == OT_SAME)
			return(POLY_END);
	}

	if (P_LT_V(n, v) && P_LT_V(p, v)) {
		if (lu->orientation == OT_OPPOSITE)
			return(HOLE_START);
		else if (lu->orientation == OT_SAME)
			return(POLY_START);
	}
	if ( (P_GT_V(n, v) && P_LT_V(p, v)) || (P_LT_V(n, v) && P_GT_V(p, v)) )
		return(POLY_SIDE);

	rt_log(
		"%s %d HELP! special case:\n(%g %g)->(%g %g)\n(%g %g)->(%g %g)\n",
		__FILE__, __LINE__,
		p->coord[X], p->coord[Y],
		v->coord[X], v->coord[Y],
		n->coord[X], n->coord[Y]);

	return(0);
}

/*	Polygon point start.
 *
 *	  O
 *	 /-\
 *	/---\
 *	v
 *
 *	start new trapezoid
 */
void
poly_start_vertex(pt, tbl2d, tlist)
struct pt2d *pt;
struct rt_list *tbl2d, *tlist;
{
	struct trap *new_trap;
	struct edgeuse *eu;

	if (tri_debug)
		rt_log( "%g %g is polygon start vertex\n",
				pt->coord[X], pt->coord[Y]);

	new_trap = (struct trap *)rt_calloc(sizeof(struct trap), 1, "new poly_start trap");
	new_trap->top = pt;
	new_trap->bot = (struct pt2d *)NULL;
	new_trap->e_left = pt->vu_p->up.eu_p;
	new_trap->e_right = RT_LIST_PLAST_CIRC(edgeuse, pt->vu_p->up.eu_p);
	RT_LIST_MAGIC_SET(&new_trap->l, NMG_TRAP_MAGIC);

	/* add new trapezoid */
	RT_LIST_APPEND(tlist, &new_trap->l);
	NMG_CK_TRAP(new_trap);
}

/*
 *		^
 *	  /-	-\
 *	 /--	--\
 *	O---	---O
 *	 \--	--/
 *	  \-	-/
 *	   v
 *
 *	finish trapezoid from vertex, start new trapezoid from vertex
 */
void
poly_side_vertex(pt, tbl2d, tlist)
struct pt2d *pt, *tbl2d;
struct rt_list *tlist;
{
	struct trap *new_trap, *tp;
	struct edgeuse *upper_edge, *lower_edge;
	struct pt2d *pnext, *plast;

	pnext = PT2D_NEXT(tbl2d, pt);
	plast = PT2D_PREV(tbl2d, pt);
	if (tri_debug) {
		rt_log( "%g %g is polygon side vertex\n",
			pt->coord[X], pt->coord[Y]);
		rt_log( "%g %g -> %g %g -> %g %g\n",
			plast->coord[X],
			plast->coord[Y],
			pt->coord[X], pt->coord[Y],
			pnext->coord[X],
			pnext->coord[Y]);
	}

	/* find upper edge */
	if (P_LT_V(plast, pt) && P_GT_V(pnext, pt)) {
		/* ascending edge */
		upper_edge = pt->vu_p->up.eu_p;
		lower_edge = plast->vu_p->up.eu_p;
	} else if (P_LT_V(pnext, pt) && P_GT_V(plast, pt)) {
		/* descending edge */
		upper_edge = plast->vu_p->up.eu_p;
		lower_edge = pt->vu_p->up.eu_p;
 	}

	NMG_CK_EDGEUSE(upper_edge);
	NMG_CK_EDGEUSE(lower_edge);

	/* find the uncompleted trapezoid in the tree
	 * which contains the upper edge.  This is the trapezoid we will
	 * complete, and where we will add a new trapezoid
	 */
	for (RT_LIST_FOR(tp, trap, tlist)) {
		NMG_CK_TRAP(tp);
		NMG_CK_EDGEUSE(tp->e_left);
		NMG_CK_EDGEUSE(tp->e_right);
		if ((tp->e_left == upper_edge || tp->e_right == upper_edge) &&
		    tp->bot == (struct pt2d *)NULL) {
			break;
		    }
	}

	if (RT_LIST_MAGIC_WRONG(&tp->l, NMG_TRAP_MAGIC))
		rt_bomb ("didn't find trapezoid parent\n");

	/* complete trapezoid */
	tp->bot = pt;

	/* create new trapezoid with other (not upper) edge */
	new_trap = (struct trap *)rt_calloc(sizeof(struct trap), 1, "new side trap");
	RT_LIST_MAGIC_SET(&new_trap->l, NMG_TRAP_MAGIC);
	new_trap->top = pt;
	new_trap->bot = (struct pt2d *)NULL;
	if (tp->e_left == upper_edge) {
		new_trap->e_left = lower_edge;
		new_trap->e_right = tp->e_right;
	} else if (tp->e_right == upper_edge) {
		new_trap->e_right = lower_edge;
		new_trap->e_left = tp->e_left;
	} else	/* how did I get here? */
		rt_bomb("Why me?  Always me!\n");

	RT_LIST_INSERT(tlist, &new_trap->l);
	NMG_CK_TRAP(new_trap);
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
void
poly_end_vertex(pt, tbl2d, tlist)
struct pt2d *pt;
struct rt_list *tbl2d, *tlist;
{
	struct trap *tp;
	struct edgeuse *e_left, *e_right;
	struct pt2d *pprev;

	if (tri_debug)
		rt_log( "%g %g is polygon end vertex\n",
			pt->coord[X], pt->coord[Y]);

	/* get the two edges which end at this point */
	pprev = PT2D_PREV(tbl2d, pt);
	if (pprev == pt)
		rt_bomb("pprev == pt!\n");

	e_left = pprev->vu_p->up.eu_p;
	e_right = pt->vu_p->up.eu_p;

	/* find the trapezoid in tree which has
	 * both edges ending at this point.
	 */
	for (RT_LIST_FOR(tp, trap, tlist)) {
		NMG_CK_TRAP(tp);
		if (tp->e_left == e_left && tp->e_right == e_right && !tp->bot) {
			goto trap_found;
		} else if (tp->e_right == e_left && tp->e_left == e_right &&
		    !tp->bot) {
			/* straighten things out for notational convenience*/
			e_right = tp->e_right;
			e_left = tp->e_left;
			goto trap_found;
		}
	}

	rt_bomb("Didn't find trapezoid to close!\n");

	/* Complete the trapezoid. */
trap_found:
	tp->bot = pt;
}








/*	Hole Start in polygon
 *
 *	-------
 *	---O---
 *	--/ \--
 *	-/   \-
 *	      v
 *
 *	Finish existing trapezoid, start 2 new ones
 */
void
hole_start_vertex(pt, tbl2d, tlist)
struct pt2d *pt;
struct rt_list *tlist, *tbl2d;
{
	struct trap *tp, *new_trap;
	vect_t pv, ev, n;
	struct pt2d *e_pt, *next_pt;

	if (tri_debug)
		rt_log( "%g %g is hole start vertex\n", 
			pt->coord[X], pt->coord[Y]);

	/* we need to find the un-completed trapezoid which encloses this
	 * point.
	 */
	for (RT_LIST_FOR(tp, trap, tlist)) {
		NMG_CK_TRAP(tp);
		/* obviously, if the trapezoid has been completed, it's not
		 * the one we want.
		 */
		if (tp->bot) {
			if (tri_debug)
				rt_log("Trapezoid completed... Skipping\n");
			continue;
		}

		/* if point is right of left edge and left of right edge
		 * we've found the trapezoid we need to work with.
		 */

		/* form a vector from the start point of each edge to pt.
		 * if crossing this vector with the vector of the edge
		 * produces a vector with a positive Z component then the pt
		 * is "inside" the trapezoid as far as this edge is concerned
		 */
		e_pt = find_pt2d(tbl2d, tp->e_left->vu_p);
		next_pt = find_pt2d(tbl2d,
			(RT_LIST_PNEXT_CIRC(edgeuse, tp->e_left))->vu_p);
		VSUB2(pv, pt->coord, e_pt->coord);
		VSUB2(ev, next_pt->coord, e_pt->coord);
		VCROSS(n, ev, pv);
		if (n[2] <= 0.0) {
			rt_log("Continue #1\n");
			continue;
		}

		e_pt = find_pt2d(tbl2d, tp->e_right->vu_p);
		next_pt = find_pt2d(tbl2d,
			(RT_LIST_PNEXT_CIRC(edgeuse, tp->e_right))->vu_p);
		VSUB2(pv, pt->coord, e_pt->coord);
		VSUB2(ev, next_pt->coord, e_pt->coord);
		VCROSS(n, ev, pv);
		if (n[2] <= 0.0) {
			rt_log("Continue #2\n");
			continue;
		}

		goto gotit;

	}

	rt_bomb("didn't find trapezoid for hole-start point\n");
gotit:
	/* complete existing trapezoid */
	tp->bot = pt;
	/* create new left and right trapezoids */

	new_trap = (struct trap *)rt_calloc(sizeof(struct trap), 2, "New hole start trapezoids");
	new_trap->top = pt;
	new_trap->bot = (struct pt2d *)NULL;
	new_trap->e_left = tp->e_left;
	new_trap->e_right = RT_LIST_PLAST_CIRC(edgeuse, pt->vu_p->up.eu_p);
	RT_LIST_MAGIC_SET(&new_trap->l, NMG_TRAP_MAGIC);
	RT_LIST_APPEND(&tp->l, &new_trap->l);

	new_trap++;
	new_trap->top = pt;
	new_trap->bot = (struct pt2d *)NULL;
	new_trap->e_left = pt->vu_p->up.eu_p;
	new_trap->e_right = tp->e_right;
	RT_LIST_MAGIC_SET(&new_trap->l, NMG_TRAP_MAGIC);
	RT_LIST_APPEND(&tp->l, &new_trap->l);
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
void
hole_end_vertex(pt, tbl2d, tlist)
struct pt2d *pt; 
struct rt_list *tlist, *tbl2d;
{
	struct edgeuse *eunext, *euprev;
	struct trap *tp, *tpnext, *tpprev;

	if (tri_debug)
		rt_log( "%g %g is hole end vertex\n",
			pt->coord[X], pt->coord[Y]);

	/* find the trapezoids that will end at this vertex */
	eunext = pt->vu_p->up.eu_p;
	euprev = RT_LIST_PLAST_CIRC(edgeuse, eunext);
	tpnext = tpprev = (struct trap *)NULL;

	print_2d_eu("eunext", eunext, tbl2d);
	print_2d_eu("euprev", euprev, tbl2d);
	
	for (RT_LIST_FOR(tp, trap, tlist)) {
		NMG_CK_TRAP(tp);
		/* obviously, if the trapezoid has been completed, it's not
		 * the one we want.
		 */
		NMG_CK_TRAP(tp);
		print_trap(tp, tbl2d);
		if (tp->bot) {
			rt_log("Completed... Skipping\n");
			continue;
		}

		if (tp->e_left == eunext || tp->e_right == eunext) {
			rt_log("Found tpnext\n");
			tpnext = tp;
		}
		if (tp->e_right == euprev || tp->e_left == euprev) {
			rt_log("Found tpprev\n");
			tpprev = tp;
		}
		if (tpnext && tpprev)
			goto gotem;
	}
	
	rt_bomb("couldn't find both trapezoids of hole closing vertex\n");
gotem:
	NMG_CK_TRAP(tpnext);
	NMG_CK_TRAP(tpprev);

	/* finish off the two trapezoids */
	tpnext->bot = pt;
	tpprev->bot = pt;

	/* start one new trapezoid */

	tp = (struct trap *)rt_calloc(1, sizeof(struct pt2d), "pt2d struct");
	tp->top = pt;
	tp->bot = (struct pt2d *)NULL;
	if (tpnext->e_left == eunext) {
		tp->e_right = tpnext->e_right;
		tp->e_left = tpprev->e_left;
	} else if (tpnext->e_right == eunext) {
		tp->e_left = tpnext->e_left;
		tp->e_right = tpprev->e_right;
	} else
		rt_bomb("Which is my left and which is my right?\n");

	RT_LIST_MAGIC_SET(&tp->l, NMG_TRAP_MAGIC);
	RT_LIST_APPEND(&tpprev->l, &tp->l);
}


/*
 *	N M G _ T R A P _ F A C E 
 *
 *	Produce the trapezoidal decomposition of a face from the set of
 *	2D points.
 */
static void
nmg_trap_face(tbl2d, tlist)
struct rt_list *tbl2d, *tlist;
{
	struct pt2d *pt;

	for (RT_LIST_FOR(pt, pt2d, tbl2d)) {
		NMG_CK_PT2D(pt);
		switch(vtype2d(pt, tbl2d)) {
		case POLY_SIDE:
			poly_side_vertex(pt, tbl2d, tlist);
			break;
		case HOLE_START:
			hole_start_vertex(pt, tbl2d, tlist);
			break;
		case POLY_START:
			poly_start_vertex(pt, tbl2d, tlist);
			break;
		case HOLE_END:
			hole_end_vertex(pt, tbl2d, tlist);
			break;
		case POLY_END:
			poly_end_vertex(pt, tbl2d, tlist);
			break;
		default:
			rt_log( "%g %g is UNKNOWN type vertex\n",
				pt->coord[X], pt->coord[Y]);
			break;
		}
	}

}


static void
map_new_vertexuse(tbl2d, vu_p)
struct rt_list *tbl2d;
struct vertexuse *vu_p;
{
	struct vertexuse *vu;
	struct pt2d *p, *new_pt2d;

	NMG_CK_VERTEXUSE(vu_p);

	/* if it's already mapped we're outta here! */
	if (p = find_pt2d(tbl2d, vu_p)) {
		rt_log("%s %d map_new_vertexuse() vertexuse already mapped!\n",
			__FILE__, __LINE__);
		return;
	}
	/* allocate memory for new 2D point */
	new_pt2d = (struct pt2d *)
		rt_calloc(1, sizeof(struct pt2d), "pt2d struct");

	/* find another use of the same vertex that is already mapped */
	for ( RT_LIST_FOR(vu, vertexuse, &vu_p->v_p->vu_hd) ) {
		NMG_CK_VERTEXUSE(vu);
		if (! (p=find_pt2d(tbl2d, vu)) )
			continue;

		/* map parameter vertexuse */
		new_pt2d->vu_p = vu_p;
		VMOVE(new_pt2d->coord, p->coord);
		RT_LIST_MAGIC_SET(&new_pt2d->l, NMG_PT2D_MAGIC);
		RT_LIST_APPEND(&p->l, &new_pt2d->l);
		return;
	}

	rt_bomb("Couldn't find mapped vertexuse of vertex!\n");
}


void
collect_and_sort_vu(tbl2d, p1, p2, tol)
struct rt_list *tbl2d;
struct pt2d **p1, **p2;
struct rt_tol *tol;
{
	struct nmg_ray_state rs;
	struct nmg_ptbl b;
	struct pt2d *pt1, *pt2;
	point_t pt;
	vect_t dir;
	struct faceuse *fu1, *fu2;
	struct loopuse *lu;
	struct vertexuse *vu, *vu1, *vu2;
	int mid, end, k;

	/* build the nmg_ptbl that nmg_face_rs_init() needs */

	NMG_CK_PT2D(*p1);
	NMG_CK_PT2D(*p2);
	pt1 = *p1;
	pt2 = *p2;
	NMG_CK_VERTEXUSE(pt1->vu_p);
	NMG_CK_VERTEXUSE(pt2->vu_p);

	rt_log("collect_and_sort_vu(tbl2d,\n\t0x%08x (%g %g),\n\t0x%08x (%g %g))\n",
		pt1->vu_p, pt1->coord[0], pt1->coord[1],
		pt2->vu_p, pt2->coord[0], pt2->coord[1]);

	nmg_tbl(&b, TBL_INIT, (long *)NULL); 
	
	vu1 = pt1->vu_p;
	vu2 = pt2->vu_p;

	/* put the vertexuses for vu1->v_p from this loop on the list */
	if ((fu1 = nmg_find_fu_of_vu(vu1)) == (struct faceuse *)NULL) {
		rt_log("no faceuse parent of vertexuse %s %d\n", __FILE__, __LINE__);
		rt_bomb("help!\n");
	}
	for (RT_LIST_FOR(vu, vertexuse, &vu1->v_p->vu_hd)) {
		NMG_CK_VERTEXUSE(vu);
		if (nmg_find_fu_of_vu(vu) == fu1)
			mid = nmg_tbl(&b, TBL_INS, (long *)vu);
	}

	/* put the vertexuses for vu2->v_p from this loop on the list */
	if ((fu2 = nmg_find_fu_of_vu(vu2)) == (struct faceuse *)NULL) {
		rt_log("no faceuse parent of vertexuse %s %d\n", __FILE__, __LINE__);
		rt_bomb("help!\n");
	} else if (fu2 != fu1) {
		rt_log("faceuses are not the same %s %d\n", __FILE__, __LINE__);
		rt_bomb("The vertexuse is familiar, but I can't place the face\n");
	}
	for (RT_LIST_FOR(vu, vertexuse, &vu2->v_p->vu_hd)) {
		NMG_CK_VERTEXUSE(vu);
		if (nmg_find_fu_of_vu(vu) == fu2)
			end = nmg_tbl(&b, TBL_INS, (long *)vu);
	}


	VMOVE(pt,  vu1->v_p->vg_p->coord);
	VSUB2(dir, vu2->v_p->vg_p->coord, pt);
	nmg_face_rs_init(&rs, &b, fu1, fu1->fumate_p, pt, dir);
	rs.tol = tol;

	if (mid == 0 && end == 1) {
		rt_log("\tNo sorting necessary\n");
		return;
	}


	rt_log("\tun-sorted vertexuses:\n");

	for (k=0 ; k <= end ; k++) {
		vu = (struct vertexuse *)b.buffer[k];
		NMG_CK_VERTEXUSE( vu );
		pt1 = find_pt2d(tbl2d, vu);
		pt2 = PT2D_NEXT(tbl2d, pt1);

		rt_log("\t0x%08x %g %g -> 0x%08x %g %g\n",
			pt1->vu_p, pt1->coord[0], pt1->coord[1], 
			pt2->vu_p, pt2->coord[0], pt2->coord[1]);
	}

	if (mid > 0)
		nmg_face_coincident_vu_sort(&rs, 0, mid+1);
	if (end-mid > 1)
		nmg_face_coincident_vu_sort(&rs, mid+1, end+1);

	rt_log("\tsorted vertexuses:\n");

	for (k=0 ; k <= end ; k++) {
		vu = (struct vertexuse *)b.buffer[k];
		NMG_CK_VERTEXUSE( vu );
		pt1 = find_pt2d(tbl2d, vu);
		pt2 = PT2D_NEXT(tbl2d, pt1);

		if (k == mid) *p1 = pt1;
		else if (k== mid+1) *p2 = pt1;

		rt_log("\t0x%08x %g %g -> 0x%08x %g %g\n",
			pt1->vu_p, pt1->coord[0], pt1->coord[1], 
			pt2->vu_p, pt2->coord[0], pt2->coord[1]);
	}

	rt_log("\tpicking 0x%08x %g %g -> 0x%08x %g %g\n",
		(*p1)->vu_p, (*p1)->coord[0], (*p1)->coord[1], 
		(*p2)->vu_p, (*p2)->coord[0], (*p2)->coord[1]);
}





/*
 *
 *  Cut a loop which has a 2D mapping.  Since this entails the creation
 *  of new vertexuses, it is necessary to add a 2D mapping for the new
 *  vertexuses.
 *
 *
 *
 */
static struct pt2d *
cut_mapped_loop(tbl2d, p1, p2, color, tol)
struct rt_list *tbl2d;
struct pt2d *p1, *p2;
int color[3];
struct rt_tol	*tol;
{
	struct loopuse *new_lu;
	struct edgeuse *eu;
	struct vertexuse *vu;
	struct vertex *v;
	struct pt2d *new_pt2d, *p;

	NMG_CK_PT2D(p1);
	NMG_CK_PT2D(p2);
	NMG_CK_VERTEXUSE(p1->vu_p);
	NMG_CK_VERTEXUSE(p2->vu_p);

	if (tri_debug)
		rt_log("\tcutting loop @ %g %g -> %g %g\n",
			p1->coord[X], p1->coord[Y],
			p2->coord[X], p2->coord[Y]);

	if (p1->vu_p->up.eu_p->up.lu_p != p2->vu_p->up.eu_p->up.lu_p) {
		rt_log("parent loops are not the same %s %d\n", __FILE__, __LINE__);
		rt_bomb("goodnight\n");
	}
	collect_and_sort_vu(tbl2d, &p1, &p2);
	if (p1->vu_p->up.eu_p->up.lu_p != p2->vu_p->up.eu_p->up.lu_p) {
		rt_log("parent loops are not the same %s %d\n", __FILE__, __LINE__);
		rt_bomb("goodnight\n");
	}

	if (plot_fd) {
		pl_color(plot_fd, color[0], color[1], color[2]);
		pdv_3line(plot_fd, p1->coord, p2->coord);
	}

	new_lu = nmg_cut_loop(p1->vu_p, p2->vu_p);
	NMG_CK_LOOPUSE(new_lu);

	/* get the edgeuse of the new vertexuse we just created */
	eu = RT_LIST_PREV(edgeuse, &new_lu->down_hd);
	NMG_CK_EDGEUSE(eu);

	/* map it to the 2D plane */
	map_new_vertexuse(tbl2d, eu->vu_p);

	/* now map the vertexuse on the radially-adjacent edgeuse */
	NMG_CK_EDGEUSE(eu->radial_p);
	map_new_vertexuse(tbl2d, eu->radial_p->vu_p);

	eu = RT_LIST_PREV( edgeuse, &(p1->vu_p->up.eu_p->l));
	return find_pt2d(tbl2d, eu->vu_p);
}

static void
join_mapped_loops(tbl2d, p1, p2, color, tol)
struct rt_list *tbl2d;
struct pt2d *p1, *p2;
int color[3];
struct rt_tol	*tol;
{
	struct vertexuse *vu1, *vu2;
	struct vertexuse *vu;
	struct edgeuse *eu;
	struct loopuse *lu;

	vu1 = p1->vu_p;
	vu2 = p2->vu_p;

	NMG_CK_VERTEXUSE(vu1);
	NMG_CK_VERTEXUSE(vu2);

	if (p1->vu_p->up.eu_p->up.lu_p == p2->vu_p->up.eu_p->up.lu_p) {
		rt_log("parent loops are the same %s %d\n", __FILE__, __LINE__);
		rt_bomb("goodnight\n");
	}

	collect_and_sort_vu(tbl2d, &p1, &p2);

	if (p1->vu_p->up.eu_p->up.lu_p == p2->vu_p->up.eu_p->up.lu_p) {
		rt_log("parent loops are the same %s %d\n", __FILE__, __LINE__);
		(void)cut_mapped_loop(tbl2d, p1, p2, color, tol);
		return;
	}


	/* XXX nmg_join_2loops() requires that the two loops not be BOTH
	 *	OT_OPPOSITE.  We should check for this here.
	 *
	 * XXX what if vu2 is a vertex loop?
	 */

	NMG_CK_EDGEUSE(vu2->up.eu_p);

	/* need to save this so we can use it later to get
	 * the new "next" edge/vertexuse
	 */
	eu = RT_LIST_PPREV_CIRC(edgeuse, vu2->up.eu_p);

	vu = nmg_join_2loops(vu1, vu2);
	if (plot_fd) {
		pl_color(plot_fd, color[0], color[1], color[2]);
		pdv_3line(plot_fd, p1->coord,  p2->coord);
	}


	NMG_CK_VERTEXUSE(vu);

	if (vu == vu2)
		return;

	/* since we've just made some new vertexuses
	 * we need to map them to the 2D plane.  
	 *
	 * XXX This should be made more direct and efficient.  For now we
	 * just go looking for vertexuses without a mapping.
	 */
	NMG_CK_EDGEUSE(vu->up.eu_p);
	NMG_CK_LOOPUSE(vu->up.eu_p->up.lu_p);
	lu = vu->up.eu_p->up.lu_p;
	for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		if (! find_pt2d(tbl2d, eu->vu_p))
			map_new_vertexuse(tbl2d, eu->vu_p);
	}
}


static void
cut_diagonals(tbl2d, tlist, tol)
struct rt_list *tbl2d, *tlist;
struct rt_tol	*tol;
{
	struct trap *tp;
	struct pt2d *top_next, *bot_next;
	struct pt2d *top, *bot;

	int cut_color[3] = {255, 80, 80};
	int join_color[3] = {80, 80, 255};

	extern struct loopuse *nmg_find_lu_of_vu();
	struct loopuse *toplu, *botlu;

	/* Convert trap list to unimonotone polygons */
	for (RT_LIST_FOR(tp, trap, tlist)) {
		/* if top and bottom points are not on same edge of 
		 * trapezoid, we cut across the trapezoid with a new edge.
		 */

		top = tp->top;
		bot = tp->bot;


		top_next = PT2D_NEXT(tbl2d, top);
		bot_next = PT2D_NEXT(tbl2d, bot);

		if (top_next == tp->bot || bot_next == tp->top) {
			rt_log("skipping %g %g/%g %g because pts on same edge\n",
					tp->top->coord[X],
					tp->top->coord[Y],
					tp->bot->coord[X],
					tp->bot->coord[Y]);
			continue;
		}

		rt_log("trying to cut ...\n");
		print_trap(tp, tbl2d);

		/* top/bottom points are not on same side of trapezoid. */

		toplu = nmg_find_lu_of_vu(tp->top->vu_p);
		botlu = nmg_find_lu_of_vu(tp->bot->vu_p);
		NMG_CK_VERTEXUSE(tp->top->vu_p);
		NMG_CK_VERTEXUSE(tp->bot->vu_p);
		NMG_CK_LOOPUSE(toplu);
		NMG_CK_LOOPUSE(botlu);

		if (toplu == botlu){
			struct loopuse *new_lu;
			register struct pt2d *bot_next;

			/* points are in same loop.  Cut the loop */

			(void)cut_mapped_loop(tbl2d, tp->top, tp->bot, cut_color, tol);

			/* if the bottom vertexuse is on a rising edge and
			 * is a top vertex of another trapezoid then
			 * 	replace the occurrance of the old bottom
			 *	vertexuse with the new one in trapezoid "top"
			 *	locations.
			 */
			bot_next = PT2D_NEXT(tbl2d, tp->bot );

			if ( P_LT_V( tp->bot, bot_next ) ) {
				register struct pt2d *new_pt;
				struct trap *trp;

				/* find the new vertexuse of this vertex */
				new_pt = PT2D_PREV(tbl2d, tp->top);

				/* replace all "top" uses of tp->bot with
				 * new_pt
				 */
				for (RT_LIST_FOR(trp, trap, tlist)) {
					if (trp->top == tp->bot) {
						trp->top = new_pt;
					}
				}

				/* clean up old trapezoid so that top/bot
				 * are in same loop
				 */
				tp->top = PT2D_PREV( tbl2d, tp->bot );
			}

		} else {
			/* points are in different loops, join the
			 * loops together.
			 */
			/* XXX This is un-tested */

			if (toplu->orientation == OT_OPPOSITE &&
				botlu->orientation == OT_OPPOSITE)
					rt_bomb("trying to join 2 interior loops in triangulator?\n");

			if (tri_debug)
				rt_log("joining 2 loops @ %g %g -> %g %g\n",
					tp->top->coord[X],
					tp->top->coord[Y],
					tp->bot->coord[X],
					tp->bot->coord[Y]);

			join_mapped_loops(tbl2d,tp->top, tp->bot, join_color, tol);

		}

		plfu( toplu->up.fu_p, tbl2d );
	}

}

static int
is_convex(a, b, c)
struct pt2d *a, *b, *c;
{
	vect_t ab, bc, pv, N;
	double angle;
	
	VSET(N, 0.0, 0.0, 1.0);

	VSUB2(ab, b->coord, a->coord);
	VCROSS(pv, N, ab);

	VSUB2(bc, c->coord, b->coord);

	angle = rt_angle_measure( bc, ab, pv );

	if (tri_debug)
		rt_log("\tangle == %g\n", angle);

	/* XXX this should be a toleranced comparison */
	return (angle > SQRT_SMALL_FASTF && angle <= M_PI);
}

/*	C U T _ U N I M O N O T O N E
 *
 *	Given a unimonotone loopuse, triangulate it into multiple loopuses
 */
static void
cut_unimonotone( tbl2d, tlist, lu )
struct rt_list *tbl2d, *tlist;
struct loopuse *lu;
{
	struct pt2d *min, *max, *new, *first, *prev, *next, *current, *save;
	struct edgeuse *eu;
	int verts=0;
	int cut_color[3] = { 90, 255, 90};
	int join_color[3] = { 190, 255, 190};

	if (tri_debug)
		rt_log("cutting unimonotone:\n");

	min = max = (struct pt2d *)NULL;

	/* find min/max points & count vertex points */
	for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		new = find_pt2d(tbl2d, eu->vu_p);
		if (!new) rt_bomb("why can't I find this?\n");

		if (tri_debug)
			rt_log("%g %g\n", new->coord[X], new->coord[Y]);

		verts++;

		if (!min || P_LT_V(new, min))
			min = new;
		if (!max || P_GT_V(new, max))
			max = new;
	}

	/* pick the pt which does NOT have the other as a "next" pt in loop */
	if (PT2D_NEXT(tbl2d, max) == min)
		first = min;
	else if (PT2D_NEXT(tbl2d, min) == max)
		first = max;
	else 
		rt_bomb("is this a loop of just 2 points?\n");
	
	/* */
	if (tri_debug)
		rt_log("%d verts in unimonotone, Min: %g %g  Max: %g %g first:%g %g 0x%08x\n", verts,
			min->coord[X], min->coord[Y],
			max->coord[X], max->coord[Y],
			first->coord[X], first->coord[Y], first);

	current = PT2D_NEXT(tbl2d, first);

	while (verts > 3) {

		prev = PT2D_PREV(tbl2d, current);
		next = PT2D_NEXT(tbl2d, current);

		if (tri_debug)
			rt_log("%g %g -> %g %g -> %g %g ...\n",
				prev->coord[X],
				prev->coord[Y],
				current->coord[X],
				current->coord[Y],
				next->coord[X],
				next->coord[Y]);

		if (is_convex(prev, current, next)) {
			struct pt2d *t;
			/* cut a triangular piece off of the loop to
			 * create a new loop.
			 */
			current = cut_mapped_loop(tbl2d, next, prev, cut_color);
			verts--;

			plfu( lu->up.fu_p, tbl2d );

			if (current->vu_p->v_p == first->vu_p->v_p) { 
				t = PT2D_NEXT(tbl2d, first);
				rt_log("\tfirst(0x%08x -> %g %g\n", first, t->coord[X], t->coord[Y]);
				t = PT2D_NEXT(tbl2d, current);
				rt_log("\tcurrent(0x%08x) -> %g %g\n", current, t->coord[X], t->coord[Y]);

				current = PT2D_NEXT(tbl2d, current);
				rt_log("\tcurrent(0x%08x) -> %g %g\n", current, t->coord[X], t->coord[Y]);
			}
		} else {
			if (tri_debug)
				rt_log("\tConcave, moving ahead\n");
			current = next;
		}
	}
}



void
nmg_plot_flat_face(fu, tbl2d)
struct faceuse *fu;
struct rt_list *tbl2d;
{
	struct loopuse *lu;
	struct edgeuse *eu;
	char buf[80];
	vect_t pt;
	struct pt2d *p, *pn;


	if (!plot_fd && (plot_fd = popen("pl-fb", "w")) == (FILE *)NULL) {
		rt_log( "cannot open pipe\n");
	}

	pl_erase(plot_fd);
	pd_3space(plot_fd,
		fu->f_p->fg_p->min_pt[0]-1.0,
		fu->f_p->fg_p->min_pt[1]-1.0,
		fu->f_p->fg_p->min_pt[2]-1.0,
		fu->f_p->fg_p->max_pt[0]+1.0,
		fu->f_p->fg_p->max_pt[1]+1.0,
		fu->f_p->fg_p->max_pt[2]+1.0);

	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC) {
			register struct vertexuse *vu;

			vu = RT_LIST_FIRST( vertexuse, &lu->down_hd );
			if (tri_debug)
				rt_log( "lone vert @ %g %g %g\n",
					vu->v_p->vg_p->coord[0],
					vu->v_p->vg_p->coord[1],
					vu->v_p->vg_p->coord[2]);
			pl_color(plot_fd, 200, 200, 100);

			if (! (p=find_pt2d(tbl2d, vu)) )
				rt_bomb("didn't find vertexuse in list!\n");

			pdv_3point(plot_fd, p->coord);
			sprintf(buf, "%g, %g", p->coord[0], p->coord[1]);
			pl_label(plot_fd, buf);

			continue;
		}
		
		for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd ) ) {
			register struct edgeuse *eu_pnext;

			eu_pnext = RT_LIST_PNEXT_CIRC(edgeuse, &eu->l);

#if 0
			if (tri_debug)
				rt_log( "eu vert @ %g %g %g\n",
					eu->vu_p->v_p->vg_p->coord[0],
					eu->vu_p->v_p->vg_p->coord[1],
					eu->vu_p->v_p->vg_p->coord[2]);
#endif
			if (! (p=find_pt2d(tbl2d, eu->vu_p)) )
				rt_bomb("didn't find vertexuse in list!\n");

			if (! (pn=find_pt2d(tbl2d, eu_pnext->vu_p)) )
				rt_bomb("didn't find vertexuse in list!\n");


			VSUB2(pt, pn->coord, p->coord);

			VSCALE(pt, pt, 0.80);
			VADD2(pt, p->coord, pt);

			pl_color(plot_fd, 200, 200, 200);
			pdv_3line(plot_fd, p->coord, pt);
			pd_3move(plot_fd, p->coord[0], p->coord[1], p->coord[2]);
			sprintf(buf, "%g, %g", p->coord[0], p->coord[1]);
			pl_label(plot_fd, buf);
		}
	}
}


void
nmg_triangulate_face(fu, tol)
struct faceuse *fu;
struct rt_tol	*tol;
{
	mat_t TformMat;
	struct rt_list *tbl2d;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct rt_list tlist;
	struct trap *tp;
	struct pt2d *pt;
	int vert_count;

	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		nmg_split_touchingloops(lu);
	}

	/* convert 3D face to face in the X-Y plane */
	tbl2d = nmg_flatten_face(fu, TformMat);


	if (tri_debug) {
		struct pt2d *pt;
		rt_log( "Face Flattened\n");
		rt_log( "Vertex list:\n");
		for (RT_LIST_FOR(pt, pt2d, tbl2d)) {
			rt_log("\tpt2d %g %g\n", pt->coord[0], pt->coord[1]);
		}

		plfu( fu, tbl2d );
		nmg_plot_flat_face(fu, tbl2d);
		rt_log( "Face plotted\n\tmaking trapezoids...\n");
	}


	RT_LIST_INIT(&tlist);
	nmg_trap_face(tbl2d, &tlist);

	print_tlist(tbl2d, &tlist);

	rt_log("Cutting diagonals ----------\n");
	cut_diagonals(tbl2d, &tlist,tol);
	rt_log("Diagonals are cut ----------\n");

	/* now we're left with a face that has some triangle loops and some
	 * uni-monotone loops.  Find the uni-monotone loops and triangulate.
	 */
	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {

		if (RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC) {
			rt_bomb("How did I miss this?\n");
		} else if (RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_EDGEUSE_MAGIC) {
			vert_count = 0;
			for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd )) {
				if (++vert_count > 3) {
					cut_unimonotone(tbl2d, &tlist, lu);
					break;
				}
			}
		}
	}


	plfu( fu, tbl2d );

	while (RT_LIST_WHILE(tp, trap, &tlist)) {
		RT_LIST_DEQUEUE(&tp->l);
		rt_free((char *)tp, "trapezoid free");
	}

	while (RT_LIST_WHILE(pt, pt2d, tbl2d)) {
		RT_LIST_DEQUEUE(&pt->l);
		rt_free((char *)pt, "pt2d free");
	}
	rt_free((char *)tbl2d, "discard tbl2d");

	return;
}

