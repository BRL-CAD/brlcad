/*
 *			M S T . C
 *
 *  Author -
 *	Paul J. Tanenbaum
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
 *	This software is Copyright (C) 1996 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"
#include <stdio.h>
#include <machine.h>
#include "bu.h"
#include "redblack.h"

/************************************************************************
 *									*
 *		The data structures					*
 *									*
 ************************************************************************/
struct vertex
{
    long		v_magic;
    long		v_index;
    int			v_civilized;
    struct bu_list	v_neighbors;
    struct bridge	*v_bridge;
};
#define	VERTEX_NULL	((struct vertex *) 0)
#define	VERTEX_MAGIC	0x6d737476

struct bridge
{
    long		b_magic;
    double		b_weight;
    struct vertex	*b_vert_civ;
    struct vertex	*b_vert_unciv;
};
#define	BRIDGE_NULL	((struct bridge *) 0)
#define	BRIDGE_MAGIC	0x6d737462

static rb_tree		*prioq;		/* Priority queue of bridges */

struct neighbor
{
    struct bu_list	l;
    struct vertex	*n_vertex;
    double		n_weight;
};
#define	n_magic		l.magic
#define	NEIGHBOR_NULL	((struct neighbor *) 0)
#define	NEIGHBOR_MAGIC	0x6d73746e

/************************************************************************
 *									*
 *	  Helper routines for manipulating the data structures		*
 *									*
 ************************************************************************/

/*
 *			  M K _ B R I D G E ( )
 *
 */
struct bridge *mk_bridge (vcp, vup, weight)

struct vertex	*vcp;
struct vertex	*vup;
double		weight;

{
    struct bridge	*bp;

    bp = (struct bridge *) bu_malloc(sizeof(struct bridge), "bridge");

    bp -> b_magic = BRIDGE_MAGIC;
    bp -> b_weight = weight;
    bp -> b_vert_civ = vcp;
    bp -> b_vert_unciv = vup;

    return (bp);
}
#define	mk_init_bridge(vp)	(mk_bridge(VERTEX_NULL, (vp), 0.0))
#define	is_finite_bridge(bp)	((bp) -> b_vert_civ != VERTEX_NULL)
#define	is_infinite_bridge(bp)	((bp) -> b_vert_civ == VERTEX_NULL)

/*
 *		   F R E E _ B R I D G E ( )
 *
 *	N.B. - It is up to the calling routine to free
 *		the b_vert_civ and b_vert_unciv members of bp.
 */
void free_bridge (bp)

struct bridge	*bp;

{
    BU_CKMAG(bp, BRIDGE_MAGIC, "bridge");
    bu_free((genptr_t) bp, "bridge");
}

/*
 *			M K _ V E R T E X ( )
 *
 */
struct vertex *mk_vertex (index)

long	index;

{
    struct vertex	*vp;

    vp = (struct vertex *) bu_malloc(sizeof(struct vertex), "vertex");

    vp -> v_magic = VERTEX_MAGIC;
    vp -> v_index = index;
    vp -> v_civilized = 0;
    BU_LIST_INIT(&(vp -> v_neighbors));
    vp -> v_bridge = mk_init_bridge(vp);

    return (vp);
}

/*
 *		     P R I N T _ V E R T E X ( )
 *
 */
void print_vertex (v, depth)

void	*v;
int	depth;

{
    struct vertex	*vp = (struct vertex *) v;
    struct neighbor	*np;

    BU_CKMAG(vp, VERTEX_MAGIC, "vertex");

    bu_log(" vertex %d <x%x> %s...\n",
	vp -> v_index, vp,
	vp -> v_civilized ? "civilized" : "uncivilized");
    for (BU_LIST_FOR(np, neighbor, &(vp -> v_neighbors)))
    {
	BU_CKMAG(np, NEIGHBOR_MAGIC, "neighbor");
	BU_CKMAG(np -> n_vertex, VERTEX_MAGIC, "vertex");

	bu_log("  is a neighbor <x%x> of vertex <x%x> %d at cost %g\n",
	    np, np -> n_vertex, np -> n_vertex -> v_index, np -> n_weight);
    }
}

/*
 *		   F R E E _ V E R T E X ( )
 *
 *	N.B. - This routine frees the v_bridge member of vp.
 */
void free_vertex (vp)

struct vertex	*vp;

{
    BU_CKMAG(vp, VERTEX_MAGIC, "vertex");
    free_bridge(vp -> v_bridge);
    bu_free((genptr_t) vp, "vertex");
}

/*
 *			  M K _ N E I G H B O R ( )
 *
 */
struct neighbor *mk_neighbor (vp, weight)

struct vertex	*vp;
double		weight;

{
    struct neighbor	*np;

    np = (struct neighbor *) bu_malloc(sizeof(struct neighbor), "neighbor");

    np -> n_magic = NEIGHBOR_MAGIC;
    np -> n_vertex = vp;
    np -> n_weight = weight;

    return (np);
}

/************************************************************************
 *									*
 *	    The comparison callbacks for libredblack(3)			*
 *									*
 ************************************************************************/

/*
 *		C O M P A R E _ V E R T I C E S ( )
 */
int compare_vertices (v1, v2)

void	*v1;
void	*v2;

{
    struct vertex	*vert1 = (struct vertex *) v1;
    struct vertex	*vert2 = (struct vertex *) v2;

    BU_CKMAG(vert1, VERTEX_MAGIC, "vertex");
    BU_CKMAG(vert2, VERTEX_MAGIC, "vertex");

    return (vert1 -> v_index  -  vert2 -> v_index);
}

/*
 *	    C O M P A R E _ B R I D G E S ( )
 */
int compare_bridges (v1, v2)

void	*v1;
void	*v2;

{
    double		delta;
    struct bridge	*b1 = (struct bridge *) v1;
    struct bridge	*b2 = (struct bridge *) v2;

    BU_CKMAG(b1, BRIDGE_MAGIC, "bridge");
    BU_CKMAG(b2, BRIDGE_MAGIC, "bridge");

    if (is_infinite_bridge(b1))
	if (is_infinite_bridge(b2))
	    return 0;
	else
	    return 1;
    else if (is_infinite_bridge(b2))
	return -1;

    delta = b1 -> b_weight  -  b2 -> b_weight;
    return ((delta <  0) ? -1 :
	    (delta == 0) ?  0 :
			    1);
}

/************************************************************************
 *									*
 *	  Routines for manipulating the dictionary and priority queue	*
 *									*
 ************************************************************************/

/*
 *			L O O K U P _ V E R T E X ( )
 */
struct vertex *lookup_vertex(dict, index)

rb_tree	*dict;
long	index;

{
    int			rc;	/* Return code from rb_insert() */
    struct vertex	*qvp;	/* The query */
    struct vertex	*vp;	/* Value to return */

    /*
     *	Prepare the dictionary query
     */
    qvp = mk_vertex(index);

    /*
     *	Perform the query by attempting an insertion...
     *	If the query succeeds (i.e., the insertion fails!),
     *	then we have our vertex.
     *	Otherwise, we must create a new vertex.
     */
    switch (rc = rb_insert(dict, (void *) qvp))
    {
	case -1:
	    vp = (struct vertex *) rb_curr1(dict);
	    free_vertex(qvp);
	    break;
	case 0:
	    vp = qvp;
	    break;
	default:
	    bu_log("rb_insert() returns %d:  This shouldn't happen\n", rc);
	    exit (1);
    }

    return (vp);
}

/*
 *		     A D D _ T O _ P R I O Q ( )
 *
 */
void add_to_prioq (v, depth)

void	*v;
int	depth;

{
    struct vertex	*vp = (struct vertex *) v;

    BU_CKMAG(vp, VERTEX_MAGIC, "vertex");
    BU_CKMAG(vp -> v_bridge, BRIDGE_MAGIC, "bridge");

    rb_insert(prioq, (void *) (vp -> v_bridge));
}

/*
 *		     D E L _ F R O M _ P R I O Q ( )
 *
 */
void del_from_prioq (vp)

struct vertex	*vp;

{

    BU_CKMAG(vp, VERTEX_MAGIC, "vertex");
    BU_CKMAG(vp -> v_bridge, BRIDGE_MAGIC, "bridge");

    if (rb_search1(prioq, (void *) (vp -> v_bridge)) == NULL)
    {
	bu_log("del_from_prioq: Cannot find bridge <x%x>.", vp -> v_bridge);
	bu_log("  This should not happen\n");
	exit (1);
    }
    rb_delete1(prioq);
}

/*
 *		     E X T R A C T _ M I N ( )
 *
 */
struct bridge *extract_min ()
{
    struct bridge	*bp;

    bp = (struct bridge *) rb_min(prioq, 0);
    if (bp != BRIDGE_NULL)
    {
	BU_CKMAG(bp, BRIDGE_MAGIC, "bridge");
    }
    rb_delete1(prioq);
    return (bp);
}

/************************************************************************
 *									*
 *	  More or less vanilla-flavored stuff for MST			*
 *									*
 ************************************************************************/

/*
 *			G E T _ E D G E ( )
 */
int get_edge (fp, index, w)

FILE	*fp;
long	*index;		/* Indices of edge endpoints */
double	*w;		/* Weight */

{
    char		*bp;
    static int		line_nm = 0;
    struct bu_vls	buf;

    bu_vls_init_if_uninit(&buf);
    for ( ; ; )
    {
	++line_nm;
	bu_vls_trunc(&buf, 0);
	if (bu_vls_gets(&buf, fp) == -1)
	    return (0);
	bp = bu_vls_addr(&buf);
	while ((*bp == ' ') || (*bp == '\t'))
	    ++bp;
	if (*bp == '#')
	    continue;
	if (sscanf(bp, "%ld%ld%lg", &index[0], &index[1], w) != 3)
	{
	    bu_log("Illegal input on line %d: '%s'\n", line_nm, bp);
	    return (-1);
	}
	else
	    break;
    }
    return (1);
}

main (argc, argv)

int	argc;
char	*argv[];

{
    int			i;
    long		index[2];	/* Indices of edge endpoints */
    double		weight;		/* Edge weight */
    rb_tree		*dictionary;	/* Dictionary of vertices */
    struct bridge	*bp;		/* The current bridge */
    struct vertex	*up;		/* An uncivilized neighbor of vup */
    struct vertex	*vcp;		/* The civilized vertex of bp */
    struct vertex	*vup;		/* The uncvilized vertex of bp */
    struct vertex	*vertex[2];	/* The current edge */
    struct neighbor	*neighbor[2];	/* Their neighbors */
    struct neighbor	*np;		/* A neighbor of vup */

    /*
     *	Initialize the dictionary and the priority queue
     */
    dictionary = rb_create1("Dictionary of vertices", compare_vertices);
    rb_uniq_on1(dictionary);

    /*
     *	Read in the graph
     */
    while (get_edge(stdin, index, &weight))
    {
	for (i = 0; i < 2; ++i)		/* For each end of the edge... */
	{
	    vertex[i] = lookup_vertex(dictionary, index[i]);
	    neighbor[i] = mk_neighbor(VERTEX_NULL, weight);
	    BU_LIST_INSERT(&(vertex[i] -> v_neighbors), &(neighbor[i] -> l));
	}
	neighbor[0] -> n_vertex = vertex[1];
	neighbor[1] -> n_vertex = vertex[0];
    }

    /*
     *	Initialize the priority queue
     */
    prioq = rb_create1("Priority queue of bridges", compare_bridges);
    rb_walk1(dictionary, add_to_prioq, INORDER);
    
    /*
     *	Grow a minimum spanning tree,
     *	using Prim's algorithm
     */
    while ((bp = extract_min()) != BRIDGE_NULL)
    {
	BU_CKMAG(bp, BRIDGE_MAGIC, "bridge");
	vcp = bp -> b_vert_civ;
	vup = bp -> b_vert_unciv;
	BU_CKMAG(vup, VERTEX_MAGIC, "vertex");

	if (is_finite_bridge(bp))
	{
	    BU_CKMAG(vcp, VERTEX_MAGIC, "vertex");
	    bu_log(" ...edge %d %d of weight %d\n",
		vcp -> v_index, vup -> v_index, bp -> b_weight);
	}
	free_bridge(bp);
	vup -> v_civilized = 1;

	for (BU_LIST_FOR(np, neighbor, &(vup -> v_neighbors)))
	{
	    BU_CKMAG(np, NEIGHBOR_MAGIC, "neighbor");
	    up = np -> n_vertex;
	    BU_CKMAG(up, VERTEX_MAGIC, "vertex");

	    if (up -> v_civilized)
	    {
		BU_CKMAG(up -> v_bridge, BRIDGE_MAGIC, "bridge");
		if (np -> n_weight < up -> v_bridge -> b_weight)
		{
		    del_from_prioq(up);
		    up -> v_bridge -> b_vert_civ = vup;
		    up -> v_bridge -> b_weight = np -> n_weight;
		    add_to_prioq(up);
		}
	    }
	    BU_LIST_DEQUEUE(&(np -> l));
	}
    }
}

/*
 *	For each edge in the input stream
 *	    For each vertex of the edge
 *		Get its vertex structure
 *		    If vertex is not in the dictionary
 *			Create the vertex struc
 *			Record the vertex struc in the dictionary
 *		    Return the vertex struc
 *		Create a neighbor structure and install it
 *	    Record each vertex as the other's neighbor
 *
 *	For every vertex v
 *	    Enqueue an infinite-weight bridge to v
 *
 *	While there exists a min-weight bridge (to a vertex v) in the queue
 *	    Dequeue the bridge
 *	    If the weight is finite
 *	        Output the bridge
 *	    Mark v as civilized
 *	    For every uncivilized neighbor u of v
 *	        if uv is cheaper than u's current bridge
 *		    dequeue u's current bridge
 *		    enqueue bridge(uv)
 */
