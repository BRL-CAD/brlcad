/*
 *			N M G _ P T _ F U . C
 *
 *  Routines for classifying a point with respect to a faceuse.
 *
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "./nmg_rt.h"


/* vertex/edge distance 
 * Each loop geometry element (edge/vertex) has one of these computed.
 * We keep track of them for the whole face so that this value is computed
 * only once per geometry element.  This way we get consistent answers
 */
struct ve_dist {
	struct rt_list	l;
	long		*magic_p;/* pointer to edge/vertex structure */
	double		dist;	/* distance from point to edge */
	point_t		pca;	/* point of closest approach */
	struct vertex	*v1;
	struct vertex	*v2;
	int		status;	/* return code from rt_dist_pt3_lseg3 */
};
#define NMG_VE_DIST_MAGIC	0x102938
#define NMG_CK_VED(_p)	NMG_CKMAG(_p, NMG_VE_DIST_MAGIC, "vertex/edge_dist")

/* The loop builds a list of these things so that it can figure out the point
 * classification based upon the classification of the pt vs each edge of
 * the loop.  This list is sorted in ascending "ved_p->dist" order.
 */
struct edge_info {
	struct rt_list		l;
	struct ve_dist 		*ved_p;	  /* ptr to ve_dist for this item */
	struct edgeuse		*eu_p;	  /* edgeuse pointer */
	int			class;	  /* pt classification WRT this item use */
};
#define NMG_EDGE_INFO_MAGIC 0xe100
#define NMG_CK_EI(_p)	NMG_CKMAG(_p, NMG_EDGE_INFO_MAGIC, "edge_info")

struct fpi {
	long			magic;
	CONST struct rt_tol	*tol;
	CONST struct faceuse	*fu_p;
	struct rt_list	ve_dh;		/* ve_dist list head */
	plane_t		norm;		/* surface normal for face(use) */
	pointp_t	pt;		/* pt in plane of face to classify */
	void		(*eu_func)();	/* call w/eu when pt on edgeuse */
	void		(*vu_func)();	/* call w/vu when pt on vertexuse */
	CONST char	*priv;		/* caller's private data */
	int		hits;		/* flag PERUSE/PERGEOM */
};
#define NMG_FPI_MAGIC 12345678 /* fpi\0 */
#define NMG_CK_FPI(_fpi) \
	NMG_CKMAG(_fpi, NMG_FPI_MAGIC, "fu_pt_info") ; \
	RT_CK_TOL( _fpi->tol ); \
	RT_CK_LIST_HEAD(&_fpi->ve_dh)

#define NMG_FPI_TOUCHED	27
#define NMG_FPI_MISSED  32768

#ifdef USE_PROTOTYPES
static int	nmg_class_pt_vu(struct fpi *fpi, struct vertexuse *vu);
static void	pl_pt_e(struct fpi *fpi, struct edge_info *ei);
static struct edge_info *nmg_class_pt_eu(struct fpi *fpi, struct edgeuse *eu, struct edge_info *edge_list);
static int	compute_loop_class(struct fpi *fpi,struct loopuse *lu,struct edge_info *edge_list);
static int	nmg_class_pt_lu(struct loopuse *lu, struct fpi *fpi);
int		nmg_class_pt_fu_except(point_t pt, CONST struct faceuse *fu, CONST struct loopuse *ignore_lu, void (*eu_func)(), void (*vu_func)(), CONST char *priv, CONST int call_on_hits, CONST struct rt_tol *tol);
#endif
/*
 *	N M G _ C L A S S _ P T _ V U 
 *
 *	Classify a point vs a vertex (touching/missed)
 */
static int
nmg_class_pt_vu(fpi, vu)
struct fpi *fpi;
struct vertexuse *vu;
{
	vect_t delta;
	struct ve_dist	*ve_d, *ved;

	NMG_CK_VERTEXUSE(vu);

	/* see if we've classified this vertex WRT the point already */
	for (RT_LIST_FOR(ved, ve_dist, &fpi->ve_dh)){
		NMG_CK_VED(ved);
		if (ved->magic_p == &vu->v_p->magic) {
			ved->dist;
			goto found;
		}
	}

	/* if we get here, we didn't find the vertex in the list of
	 * previously classified geometry.  Create an entry in the
	 * face's list of processed geometry.
	 */
	VSUB2(delta, vu->v_p->vg_p->coord, fpi->pt);

	ved = (struct ve_dist *) rt_malloc(sizeof(struct ve_dist), "ve_dist structure");
	ved->magic_p = &vu->v_p->magic;
	ved->dist = MAGNITUDE(delta);
	if (ved->dist < fpi->tol->dist ) {
		ved->status = NMG_FPI_TOUCHED;
		if (fpi->hits == NMG_FPI_PERGEOM)
			fpi->vu_func(vu, fpi->pt, fpi->priv);
	}
	else ved->status = NMG_FPI_MISSED;
	VMOVE(ved->pca, vu->v_p->vg_p->coord);
	ved->v1 = ved->v2 = vu->v_p;

	RT_LIST_MAGIC_SET(&ved->l, NMG_VE_DIST_MAGIC);
	RT_LIST_APPEND(&fpi->ve_dh, &ved->l);
found:

	if (fpi->vu_func  &&
	    ved->status == NMG_FPI_TOUCHED &&
	    fpi->hits == NMG_FPI_PERUSE)
		fpi->vu_func(vu, fpi->pt, fpi->priv);

	return ved->status;
}

static void
pl_pt_e(fpi, ei)
struct fpi *fpi;
struct edge_info *ei;
{
	FILE *fd;
	char name[25];
	long *b;
	static int plot_file_number=0;

	NMG_CK_FPI(fpi);	
	NMG_CK_FACEUSE(fpi->fu_p);
	NMG_CK_EI(ei);
	
	sprintf(name, "pt_e%02d.pl", plot_file_number++);
	if ((fd=fopen(name, "w")) == (FILE *)NULL) {
		perror(name);
		abort();
	}

	rt_log("\toverlay %s\n", name);
	b = (long *)rt_calloc( fpi->fu_p->s_p->r_p->m_p->maxindex,
		sizeof(long), "bit vec"),

	pl_erase(fd);
	pd_3space(fd,
		fpi->fu_p->f_p->min_pt[0]-1.0,
		fpi->fu_p->f_p->min_pt[1]-1.0,
		fpi->fu_p->f_p->min_pt[2]-1.0,
		fpi->fu_p->f_p->max_pt[0]+1.0,
		fpi->fu_p->f_p->max_pt[1]+1.0,
		fpi->fu_p->f_p->max_pt[2]+1.0);
	
	nmg_pl_fu(fd, fpi->fu_p, b, 255, 255, 255);

	pl_color(fd, 255, 255, 50);
	pdv_3line(fd, ei->ved_p->pca, fpi->pt);

	rt_free((char *)b, "bit vec");
	fclose(fd);
}

/*	N M G _ C L A S S _ P T _ E U
 *
 * If there is no ve_dist structure for this edge, compute one and
 * add it to the list.
 *
 * Sort an edge_info structure into the loops list of edgeuse status
 */
static struct edge_info *
nmg_class_pt_eu(fpi, eu, edge_list)
struct fpi		*fpi;
struct edgeuse		*eu;
struct edge_info	*edge_list;
{
	struct ve_dist	*ved, *ed;
	struct edge_info	*ei_p;
	struct edge_info	*ei;
	vect_t		left;
	vect_t		pt_vec;
	vect_t		v_to_pt;
	int		found = 0;

	NMG_CK_FPI(fpi);	
	RT_CK_TOL(fpi->tol);

	if (rt_g.NMG_debug & DEBUG_PT_FU) {
		rt_log("pt (%g %g %g) vs_edge (%g %g %g) (%g %g %g)\n",
			V3ARGS(fpi->pt),
			V3ARGS(eu->vu_p->v_p->vg_p->coord),
			V3ARGS(eu->eumate_p->vu_p->v_p->vg_p->coord));
	}

	/* try to find this edge in the list of ve_dist's */
	for (RT_LIST_FOR(ed, ve_dist, &fpi->ve_dh)) {
		NMG_CK_VED(ed);
		if (ed->magic_p == &eu->e_p->magic) {

			/* found the data for this edge */

			if (rt_g.NMG_debug & DEBUG_PT_FU)
				rt_log ("pt previously classified WRT Edge\n");

			ved = ed;
			found = 1;
			goto found;
		}

		if (ed->dist <= fpi->tol->dist &&
		    (ed->magic_p == &eu->vu_p->v_p->magic ||
		     ed->magic_p == &eu->eumate_p->vu_p->v_p->magic) ) {
			/* The point is within tolerance of an endpoint
			 * of the edge.
			 */
			if (rt_g.NMG_debug & DEBUG_PT_FU) {
				register struct vertex *v_p = 
					(struct vertex *)ed->magic_p;
				rt_log ("vertex (%g %g %g) of edge previously touched\n",
					V3ARGS(v_p->vg_p->coord));
			}
			ved = ed;
			found = 1;
			goto found;
		}
	}

	/* we didn't find a ve_dist structure for this edge, so we'll
	 * have to do the calculations.
	 */
	ved = (struct ve_dist *)rt_malloc(sizeof(struct ve_dist), "ve_dist structure");
	ved->magic_p = &eu->e_p->magic;
	ved->status = rt_dist_pt3_lseg3(&ved->dist, ved->pca,
					eu->vu_p->v_p->vg_p->coord,
					eu->eumate_p->vu_p->v_p->vg_p->coord,
					fpi->pt,
					fpi->tol);
	ved->v1 = eu->vu_p->v_p;
	ved->v2 = eu->eumate_p->vu_p->v_p;
	RT_LIST_MAGIC_SET(&ved->l, NMG_VE_DIST_MAGIC);
	RT_LIST_APPEND(&fpi->ve_dh, &ved->l);

found:

	/* Add a struct for this edgeuse to the loop's list of dist-sorted
	 * edgeuses.
	 */
	ei = (struct edge_info *)rt_malloc(sizeof(struct edge_info), "struct edge_info");
	ei->ved_p = ved;
	ei->eu_p = eu;
	RT_LIST_MAGIC_SET(&ei->l, NMG_EDGE_INFO_MAGIC);

	/* compute the status (ei->status) of the point WRT this edge */

	switch (ved->status) {
	case 0: /* pt is on the edge(use) */
		ei->class = NMG_CLASS_AonBshared;
		if (fpi->eu_func &&
		    (fpi->hits == NMG_FPI_PERUSE ||
		     (fpi->hits == NMG_FPI_PERGEOM && !found) ) ) {
			fpi->eu_func(eu, fpi->pt, fpi->priv);
		}
		break;
	case 1:	/* within tolerance of endpoint at ved->v1 */
		ei->class = NMG_CLASS_AonBshared;
		/* add an entry for the vertex in the edge list so that
		 * other uses of this vertex will claim the point is within
		 * tolerance without re-computing
		 */
		ed = (struct ve_dist *)rt_malloc(sizeof(struct ve_dist), "ve_dist structure");
		ed->magic_p = &ved->v1->magic;
		ed->status = ved->status;
		ed->v1 = ed->v2 = ved->v1;

		RT_LIST_APPEND(&fpi->ve_dh, &ed->l);

		if (fpi->vu_func &&
		    (fpi->hits == NMG_FPI_PERUSE ||
		     (fpi->hits == NMG_FPI_PERGEOM && !found) ) ) {
			fpi->vu_func(eu->vu_p, fpi->pt, fpi->priv);
		}

		break;
	case 2:	/* within tolerance of endpoint at ved->v2 */
		ei->class = NMG_CLASS_AonBshared;
		/* add an entry for the vertex in the edge list so that
		 * other uses of this vertex will claim the point is within
		 * tolerance without re-computing
		 */
		ed = (struct ve_dist *)rt_malloc(sizeof(struct ve_dist), "ve_dist structure");
		ed->magic_p = &ved->v2->magic;
		ed->status = ved->status;
		ed->v1 = ed->v2 = ved->v2;

		RT_LIST_APPEND(&fpi->ve_dh, &ed->l);
		if (fpi->vu_func &&
		    (fpi->hits == NMG_FPI_PERUSE ||
		     (fpi->hits == NMG_FPI_PERGEOM && !found) ) ) {
			fpi->vu_func(eu->vu_p, fpi->pt, fpi->priv);
		}
		break;

	case 3: /* PCA of pt on line is "before" ved->v1 of segment */
		/* fallthrough */
	case 4: /* PCA of pt on line is "before" ved->v2 of segment */
		ei->class = NMG_CLASS_AoutB;
		break;

	case 5: /* PCA is along length of edge, but point is NOT on edge. */
		if (nmg_find_eu_leftvec( left, eu ))
			rt_bomb("can't find left vector\n");
		/* take dot product of v->pt vector with left to determine
		 * if pt is inside/left of edge
		 */
		VSUB2(v_to_pt, fpi->pt, ved->pca);
		if (VDOT(v_to_pt, left) >= 0.0)
			ei->class = NMG_CLASS_AinB;
		else
			ei->class = NMG_CLASS_AoutB;
		break;
	default:
		rt_log("%s:%d status = %d\n", __FILE__, __LINE__, ved->status);
		rt_bomb("Why did this happen?");
		break;
	}


	if (rt_g.NMG_debug & DEBUG_PT_FU) {
		rt_log("pt @ dist %g from edge classed %s vs edge\n",
			ei->ved_p->dist, nmg_class_name(ei->class));
		pl_pt_e(fpi, ei);
	}

	/* now that it's complete, add ei to the edge list */
	for (RT_LIST_FOR(ei_p, edge_info, &edge_list->l)) {
		/* if the distance to this edge is smaller, or
		 * if the distance is the same & the edge is the same
		 *	Insert edge_info struct here in list
		 */
		if (ved->dist < ei_p->ved_p->dist ||
		   (ved->dist == ei_p->ved_p->dist &&
		    ei_p->ved_p->magic_p == ved->magic_p) )
			break;
	}

	RT_LIST_INSERT(&ei_p->l, &ei->l);
	return ei;
}

/*
 * Make a list of all edgeuses which are at the same distance as the
 * first element on the list.  Toss out opposing pairs of edgeuses of the
 * same edge.
 *
 */
static void
make_near_list(edge_list, near)
struct edge_info *edge_list;
struct rt_list *near;
{
	struct edge_info *ei;
	struct edge_info *ei_p;
	double dist;

	RT_CK_LIST_HEAD(&edge_list->l);
	RT_CK_LIST_HEAD(near);

	ei = RT_LIST_FIRST(edge_info, &edge_list->l);
	NMG_CK_EI(ei);
	NMG_CK_VED(ei->ved_p);
	dist = ei->ved_p->dist;
	
	/* create "near" list with all ei's at this dist */
	for (RT_LIST_FOR(ei, edge_info, &edge_list->l)) {
		NMG_CK_EI(ei);
		NMG_CK_VED(ei->ved_p);
		if (ei->ved_p->dist == dist) {
			ei_p = RT_LIST_PLAST(edge_info, &ei->l);
			RT_LIST_DEQUEUE(&ei->l);
			RT_LIST_APPEND(near, &ei->l);
			ei = ei_p;
		}
	}

	if (rt_g.NMG_debug & DEBUG_PT_FU ) {
		rt_log("dist %g near list\n", dist);
		for (RT_LIST_FOR(ei, edge_info, near)) {
			rt_log("\t(%g %g %g) -> (%g %g %g)\n",
				V3ARGS(ei->eu_p->vu_p->v_p->vg_p->coord),
				V3ARGS(ei->eu_p->eumate_p->vu_p->v_p->vg_p->coord));
rt_log("\tdist:%g class:%s status:%d pca(%g %g %g)\n\t\tv1(%g %g %g) v2(%g %g %g)\n",
			ei->ved_p->dist, nmg_class_name(ei->class),
			ei->ved_p->status,
			V3ARGS(ei->ved_p->pca),
			V3ARGS(ei->ved_p->v1->vg_p->coord),
			V3ARGS(ei->ved_p->v2->vg_p->coord));
		}
	}


	/* toss opposing pairs of uses of the same edge from the list */
	for (RT_LIST_FOR(ei, edge_info, near)) {
		NMG_CK_EI(ei);
		for (RT_LIST_FOR(ei_p, edge_info, near)) {
			NMG_CK_EI(ei_p);
			NMG_CK_VED(ei_p->ved_p);

			/* if we've found an opposing use of the same
			 *    edge toss the pair of them
			 */
			if (ei_p->ved_p->magic_p == ei->ved_p->magic_p &&
			    ei_p->eu_p->vu_p != ei->eu_p->vu_p &&
			    ei_p->eu_p->vu_p == ei->eu_p->eumate_p->vu_p ) {
				if (rt_g.NMG_debug & DEBUG_PT_FU ) {
					rt_log("tossing edgeuse pair:\n");
					rt_log("(%g %g %g) -> (%g %g %g)\n",
						V3ARGS(ei->eu_p->vu_p->v_p->vg_p->coord),
						V3ARGS(ei->eu_p->eumate_p->vu_p->v_p->vg_p->coord));
					rt_log("(%g %g %g) -> (%g %g %g)\n",
						V3ARGS(ei_p->eu_p->vu_p->v_p->vg_p->coord),
						V3ARGS(ei_p->eu_p->eumate_p->vu_p->v_p->vg_p->coord));
				}

				RT_LIST_DEQUEUE(&ei_p->l);
				rt_free((char *)ei_p, "edge info struct");
				ei_p = RT_LIST_PLAST(edge_info, &ei->l);
				RT_LIST_DEQUEUE(&ei->l);
				rt_free((char *)ei, "edge info struct");
				ei = ei_p;
			    	break;
			}
		}
	}
}










/*	C O M P U T E _ L O O P _ C L A S S
 *
 * Given a list of edge_info structures for the edges of a loop,
 *	determine what the classification for the loop should be.
 */

static int
compute_loop_class(fpi, lu, edge_list)
struct fpi *fpi;
struct loopuse *lu;
struct edge_info *edge_list;
{
	struct edge_info *ei;
	struct edge_info *ei_p;
	struct edge_info *ei_vdot_max;
	double		vdot_max;
	double		vdot_val;
	vect_t		v_pt;
	vect_t		left;
	double		dist;
	struct rt_list	near;
	int 		lu_class = NMG_CLASS_Unknown;

	if (rt_g.NMG_debug & DEBUG_PT_FU ) {
		rt_log("compute_loop_class()\n");
		for (RT_LIST_FOR(ei, edge_info, &edge_list->l)) {
rt_log("dist:%g class:%s status:%d pca(%g %g %g)\n\tv1(%g %g %g) v2(%g %g %g)\n",
				ei->ved_p->dist, nmg_class_name(ei->class),
				ei->ved_p->status,
				V3ARGS(ei->ved_p->pca),
				V3ARGS(ei->ved_p->v1->vg_p->coord),
				V3ARGS(ei->ved_p->v2->vg_p->coord));
		}
	}

	RT_CK_LIST_HEAD(&edge_list->l);
	RT_LIST_INIT(&near);

	/* get a list of "closest/useful" edges to use in classifying
	 * the pt WRT the loop
	 */
	while (RT_LIST_IS_EMPTY(&near) && RT_LIST_NON_EMPTY(&edge_list->l)) {
		make_near_list(edge_list, &near);
	}
	
	if (RT_LIST_IS_EMPTY(&near)) {
		/* This was a "crack" or "snake" loop */
		
		if (lu->orientation == OT_SAME) {
			lu_class = NMG_CLASS_AoutB;
		} else if (lu->orientation == OT_OPPOSITE) {
			lu_class = NMG_CLASS_AinB;
		} else
			rt_bomb("bad lu orientation\n");

		if (rt_g.NMG_debug & DEBUG_PT_FU ) {
			rt_log("list was empty, so class is %s\n", 
				nmg_class_name(lu_class));
		}
		goto departure;
	}


	vdot_max = 0.0;
	ei_vdot_max = (struct edge_info *)NULL;

	for (RT_LIST_FOR(ei, edge_info, &near)) {
		NMG_CK_EI(ei);
		NMG_CK_VED(ei->ved_p);
		switch (ei->ved_p->status) {
		case 0: /* pt is on edge */
		case 1: /* pt is on ei->ved_p->v1 */
		case 2:	/* pt is on ei->ved_p->v2 */
			lu_class = NMG_CLASS_AonBshared;
			goto departure;
			break;
		case 3: /* pt pca is v1 */
			if (ei_vdot_max == (struct edge_info *)NULL) {
				VSUB2(v_pt, fpi->pt, ei->ved_p->v1->vg_p->coord);
				ei_vdot_max = ei;
			}
			/* fallthrough */
		case 4: /* pt pca is v2 */
			if (ei_vdot_max == (struct edge_info *)NULL) {
				VSUB2(v_pt, fpi->pt, ei->ved_p->v2->vg_p->coord);
				ei_vdot_max = ei;
			}

			nmg_find_eu_leftvec( left, ei->eu_p );

			if (fabs(vdot_val=VDOT(v_pt, left)) > fabs(vdot_max)){
				vdot_max = vdot_val;
				ei_vdot_max = ei;
				if (vdot_max > 0.0)
					lu_class = NMG_CLASS_AinB;
				else /*  vdot_max < 0.0 */
					lu_class = NMG_CLASS_AoutB;
			}

			break;
		case 5:
			/* if there is an edge where the PCA is not 
			 * at a vertex, we can just classify the point with
			 * this edge.
			 */
			lu_class = ei->class;
			if (rt_g.NMG_debug & DEBUG_PT_FU ) {
				rt_log("found status 5 edge, loop class is %s\n", 
					nmg_class_name(lu_class));
			}
			goto departure;
			break;
		default:
			rt_log("%s:%d status = %d\n",
				__FILE__, __LINE__, ei->ved_p->status);
			rt_bomb("Why did this happen?");
			break;
		}
	}

departure:

	/* the caller will get whatever is left of the edge_list, but
	 * we need to free up the "near" list
	 */
	while (RT_LIST_WHILE(ei, edge_info, &near)) {
		RT_LIST_DEQUEUE( &ei->l );
		rt_free( (char *)ei, "edge_info struct");
	}
	
	if (rt_g.NMG_debug & DEBUG_PT_FU ) {
		rt_log("compute_loop_class() returns %s\n", 
			nmg_class_name(lu_class));
	}

	return lu_class;
}
/*
 *
 * For each edgeuse, compute an edge_info structure.
 *
 * if min_dist == 0 return ON
 *
 * For dist min -> max
 *	if even # of uses of edge in this loop
 *		"ignore" all uses of this edge since we can't answer the
 *		    "spike" problem:	*---------*
 *					| .	  |
 *					*---*	  |  .
 *					|	  *----*
 *					|	  |
 *					*---------*
 *	else (odd # of uses of edge in this loop)
 *		"ignore" consecutive uses of the same edge to avoid the
 *		    "accordian pleat" problem:	*-------*
 *						|  .	|
 *						*----*	|
 *						*----*	|
 *						*----*	|
 *						*----*--*
 *		classify pt WRT remaining edgeuse
 *
 * The "C-clamp" problem:
 *	*---------------* 
 *	|		|
 *	|  *----*	|
 *	|  |	|   .	|
 *	|  |	*-------*
 *	|  |	|	|
 *	|  *----*	|
 *	|		|
 *	*---------------* 
 *
 */
static int
nmg_class_pt_lu(lu, fpi)
struct loopuse	*lu;
struct fpi	*fpi;
{
	int 	lu_class = NMG_CLASS_Unknown;

	NMG_CK_FPI(fpi);
	NMG_CK_LOOPUSE(lu);
	NMG_CK_LOOP(lu->l_p);
	NMG_CK_LOOP_G(lu->l_p->lg_p);



	if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
		register struct edgeuse	*eu;
		struct edge_info edge_list;
		struct edge_info *ei;

		RT_LIST_INIT(&edge_list.l);

		for (RT_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			ei = nmg_class_pt_eu(fpi, eu, &edge_list);
			NMG_CK_EI(ei);
			NMG_CK_VED(ei->ved_p);
			if (ei->ved_p->dist < fpi->tol->dist) {
				lu_class = NMG_CLASS_AinB;
				break;
			}
		}
		/* */
		if (lu_class == NMG_CLASS_Unknown)
			lu_class = compute_loop_class(fpi, lu, &edge_list);
		else if (rt_g.NMG_debug & DEBUG_PT_FU )
			rt_log("loop class already known (pt must touch edge)\n");

		/* free up the edge_list elements */
		while ( RT_LIST_WHILE(ei, edge_info, &edge_list.l) ) {
			RT_LIST_DEQUEUE( &ei->l );
			rt_free( (char *)ei, "edge info struct");
		}
	} else if (RT_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
		register struct vertexuse *vu;
		int v_class;

		v_class = nmg_class_pt_vu(fpi, vu);

		switch (lu->orientation) {
		case OT_SAME:
			if (v_class == NMG_FPI_TOUCHED)
				lu_class = NMG_CLASS_AinB;
			else
				lu_class = NMG_CLASS_AoutB;
			break;
		case OT_OPPOSITE:
				lu_class = NMG_CLASS_AinB;
			break;
		default:
			rt_bomb("Loop orientation error\n");
			break;
		}
	} else {
		rt_log("%s:%d bad child of loopuse\n", __FILE__, __LINE__);
		rt_bomb("crash and burn\n");
	}


	if (rt_g.NMG_debug & DEBUG_PT_FU )
		rt_log("pt classed %s vs loop\n", nmg_class_name(lu_class));

	return lu_class;
}



/*
 *
 * Classify a point on a face's plane as being inside/outside the area
 * of the face.
 *
 * For each loopuse, compute IN/ON/OUT
 *
 * if any loop has pt classified as "ON" return "ON"
 *
 * ignore all OT_SAME loops w/pt classified as "OUT"
 * ignore all OT_OPPOSITE loops w/pt classified as "IN"
 * If (# OT_SAME loops == # OT_OPPOSITE loops)
 *	pt is "OUT"
 * else if (# OT_SAME loops - 1 == # OT_OPPOSITE loops)
 *	pt is "IN"
 * else
 *	Error! panic!
 *
 *
 *  Values for "call_on_hits"
 *	1	find all elements pt touches, call user routine for each geom.
 *	2	find all elements pt touches, call user routine for each use
 */
int
nmg_class_pt_fu_except(pt, fu, ignore_lu,
	eu_func, vu_func, priv, call_on_hits, tol)
point_t pt;
CONST struct faceuse *fu;
CONST struct loopuse    *ignore_lu;
void                    (*eu_func)();   /* func to call when pt on edgeuse */
void                    (*vu_func)();   /* func to call when pt on vertexuse*/
CONST char		*priv;          /* private data for [ev]u_func */
CONST int		call_on_hits;
CONST struct rt_tol     *tol;
{
	struct fpi	fpi;
	struct loopuse	*lu;
	int		ot_same_in = 0;
	int		ot_opposite_out = 0;
	int		lu_class;
	int		fu_class = NMG_CLASS_Unknown;
	double		dist;
	struct ve_dist	*ved_p;

	NMG_CK_FACEUSE(fu);
	NMG_CK_FACE(fu->f_p);
	NMG_CK_FACE_G_PLANE(fu->f_p->g.plane_p);
	if(ignore_lu) NMG_CK_LOOPUSE(ignore_lu);
	RT_CK_TOL(tol);

	/* Validate distance from point to plane */
	NMG_GET_FU_PLANE( fpi.norm, fu );
	if( (dist=fabs(DIST_PT_PLANE( pt, fpi.norm ))) > tol->dist )  {
		rt_log("nmg_class_pt_fu_except() ERROR, point (%g,%g,%g)\nnot on face %g %g %g %g,\ndist=%g\n",
			V3ARGS(pt), V4ARGS(fpi.norm), dist );
	}

	fpi.fu_p = fu;
	fpi.tol = tol;
	RT_LIST_INIT(&fpi.ve_dh);
	fpi.pt = pt;
	fpi.eu_func = eu_func;
	fpi.vu_func = vu_func;
	fpi.priv = priv;
	fpi.hits = call_on_hits;
	fpi.magic = NMG_FPI_MAGIC;

	for (RT_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if( ignore_lu && (ignore_lu==lu || ignore_lu==lu->lumate_p) )
			continue;

		lu_class = nmg_class_pt_lu(lu, &fpi);

		if (lu->orientation == OT_OPPOSITE &&
		    lu_class == NMG_CLASS_AoutB )
			ot_opposite_out++;
		else if (lu->orientation == OT_SAME &&
		    (lu_class == NMG_CLASS_AinB ||
		     lu_class == NMG_CLASS_AonBshared))
			ot_same_in++;
	}

	if (rt_g.NMG_debug & DEBUG_PT_FU )
		rt_log("loops ot_same_in:%d ot_opposite_out:%d\n",
			ot_same_in, ot_opposite_out);

	if (ot_same_in == ot_opposite_out) fu_class = NMG_CLASS_AoutB;
	else if (ot_same_in - 1 != ot_opposite_out) {
		/* Panic time!  How did I get a parity mis-match? */
		rt_log("nmg_class_pt_fu_except(%g %g %g) Parity error @ %s:%d\n",
			V3ARGS(pt), __FILE__, __LINE__);
		rt_bomb("");
	} else 
		fu_class = NMG_CLASS_AinB;

	while (RT_LIST_WHILE(ved_p, ve_dist, &fpi.ve_dh)) {
		RT_LIST_DEQUEUE( &ved_p->l );
		rt_free( (char *)ved_p, "ve_dist struct");
	}


	if (rt_g.NMG_debug & DEBUG_PT_FU )
		rt_log("nmg_class_pt_fu_except() returns %s\n",
			nmg_class_name(fu_class));

	return fu_class;
}
