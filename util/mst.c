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
    struct bu_list	v_halfedges;
};
#define	VERTEX_NULL	((struct vertex *) 0)
#define	VERTEX_MAGIC	0x6d737476

struct edge
{
    long		e_magic;
    struct halfedge	*e_half[2];	/* The edge's ends */
    double		e_weight;	/*  "    "    weight */
};
#define	EDGE_NULL	((struct edge *) 0)
#define	EDGE_MAGIC	0x6d737465

struct halfedge
{
    struct bu_list	l;
    struct edge		*h_edge;	/* The parent edge */
    struct halfedge	*h_mate;	/* The other half of this edge */
    struct vertex	*h_vertex;	/* The vertex */
    struct vertex	*h_neighbor;	/* The adjacent vertex */
};
#define h_magic		l.magic
#define	HALFEDGE_NULL	((struct halfedge *) 0)
#define	HALFEDGE_MAGIC	0x6d737468

struct prioq_entry
{
    long		q_magic;
    struct halfedge	*q_hedge;	/* The (far half of the) edge */
    double		q_weight;	/*  "    "    weight */
};
#define	PRIOQENTRY_NULL	((struct prioq_entry *) 0)
#define	PRIOQENTRY_MAGIC	0x6d737471

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
 *	    C O M P A R E _ P R I O Q _ E N T R I E S ( )
 */
int compare_prioq_entries (v1, v2)

void	*v1;
void	*v2;

{
    double	delta;
    struct prioq_entry	*q1 = (struct prioq_entry *) v1;
    struct prioq_entry	*q2 = (struct prioq_entry *) v2;

    BU_CKMAG(q1, PRIOQENTRY_MAGIC, "priority-queue entry");
    BU_CKMAG(q2, PRIOQENTRY_MAGIC, "priority-queue entry");

    delta = q1 -> q_weight  -  q2 -> q_weight;
    return ((delta <  0) ? -1 :
	    (delta == 0) ?  0 :
			    1);
}

/************************************************************************
 *									*
 *	  Helper routines for manipulating the data structures		*
 *									*
 ************************************************************************/

/*
 *			M K _ V E R T E X ( )
 *
 */
struct vertex *mk_vertex ()
{
    struct vertex	*vp;

    vp = (struct vertex *) bu_malloc(sizeof(struct vertex), "vertex");

    vp -> v_magic = VERTEX_MAGIC;
    BU_LIST_INIT(&(vp -> v_halfedges));

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
    struct vertex	*vp = (struct vertex *) v;	/* The vertex */
    struct halfedge	*hp;			/* One of its edge ends */
    struct halfedge	*mp;			/* Its mate */
    struct vertex	*np;			/* The neighbor */
    struct edge		*ep;			/* The edge */

    BU_CKMAG(vp, VERTEX_MAGIC, "vertex");

    bu_log(" vertex %d...\n", vp -> v_index);
    for (BU_LIST_FOR(hp, halfedge, &(vp -> v_halfedges)))
    {
	BU_CKMAG(hp, HALFEDGE_MAGIC, "halfedge");

	ep = hp -> h_edge;
	BU_CKMAG(ep, EDGE_MAGIC, "edge");

	mp = hp -> h_mate;
	BU_CKMAG(mp, HALFEDGE_MAGIC, "halfedge");

	np = hp -> h_neighbor;
	BU_CKMAG(np, VERTEX_MAGIC, "vertex");

	bu_log("  shares edge <x%x> (<x%x>, <x%x>) with %d at cost %g\n",
	    ep, hp, mp, np -> v_index, ep -> e_weight);
    }
}

/*
 *		   F R E E _ V E R T E X ( )
 *
 */
void free_vertex (vp)

struct vertex	*vp;

{
    BU_CKMAG(vp, VERTEX_MAGIC, "vertex");
    bu_free((genptr_t) vp, "vertex");
}

/*
 *			  M K _ E D G E ( )
 *
 */
struct edge *mk_edge ()
{
    struct edge	*ep;

    ep = (struct edge *) bu_malloc(sizeof(struct edge), "edge");

    ep -> e_magic = EDGE_MAGIC;

    return (ep);
}

/*
 *			M K _ H A L F E D G E ( )
 *
 */
struct halfedge *mk_halfedge ()
{
    struct halfedge	*hp;

    hp = (struct halfedge *) bu_malloc(sizeof(struct halfedge), "halfedge");

    BU_LIST_INIT(&(hp -> l));
    hp -> h_magic = HALFEDGE_MAGIC;

    return (hp);
}

/*
 *		     P R I N T _ H A L F E D G E ( )
 *
 */
void print_halfedge (hp)

struct halfedge	*hp;

{
    BU_CKMAG(hp, HALFEDGE_MAGIC, "halfedge");

    bu_log(" halfedge <x%x>...\n {\n", hp);
    bu_log("     edge   =   <x%x>...\n", hp -> h_edge);
    bu_log("     mate   =   <x%x>...\n", hp -> h_mate);
    bu_log("     vertex  =  <x%x>...\n", hp -> h_vertex);
    bu_log("     neighbor = <x%x>...\n }\n", hp -> h_neighbor);
}

/*
 *		    M K _ P R I O Q _ E N T R Y ( )
 *
 */
struct prioq_entry *mk_prioq_entry ()
{
    struct prioq_entry	*qp;

    qp = (struct prioq_entry *) bu_malloc(sizeof(struct prioq_entry),
					    "priority-queue entry");

    qp -> q_magic = PRIOQENTRY_MAGIC;

    return (qp);
}

/*
 *			G E T _ E D G E ( )
 */
int get_edge (fp, vertex, w)

FILE	*fp;
long	*vertex;		/* Indices of edge endpoints */
double	*w;			/* Weight */

{
    char		*bp;
    static int		line_nm = 0;
    struct edge		*ep;
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
	if (sscanf(bp, "%ld%ld%lg", &vertex[0], &vertex[1], w) != 3)
	{
	    bu_log("Illegal input on line %d: '%s'\n", line_nm, bp);
	    return (-1);
	}
	else
	    break;
    }
    return (1);
}

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
    qvp = mk_vertex();
    qvp-> v_index = index;

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
 *		E N Q U E U E _ H A L F E D G E ( )
 */
int enqueue_halfedge (prioq, hp)

rb_tree		*prioq;
struct halfedge	*hp;

{
    int			rc;	/* Return code from rb_insert() */
    struct prioq_entry	*qp;	/* The queue entry */

    /*
     *	Prepare the queue query
     */
    qp = mk_prioq_entry();
    qp -> q_hedge = hp;
    BU_CKMAG(hp -> h_edge, EDGE_MAGIC, "edge");
    qp -> q_weight = hp -> h_edge -> e_weight;

    /*
     *	Perform the query by attempting an insertion...
     *	If the query succeeds (i.e., the insertion fails!),
     *	then we have our vertex.
     *	Otherwise, we must create a new vertex.
     */
    bu_log(" enqueueing halfedge <x%x>\n", hp);
    return (rb_insert(prioq, (void *) qp));
}

/*
 *		D E Q U E U E _ H A L F E D G E ( )
 */
struct halfedge *dequeue_halfedge (prioq)

rb_tree	*prioq;

{
    struct prioq_entry	*qp;

    qp = (struct prioq_entry *) rb_min(prioq, 0);
    BU_CKMAG(qp, PRIOQENTRY_MAGIC, "priority-queue entry");
    rb_delete1(prioq);
    bu_log(" dequeueing");
    print_halfedge(qp -> q_hedge);
    return (qp -> q_hedge);
}

main (argc, argv)

int	argc;
char	*argv[];

{
    int			i;
    int			n;		/* Number of vertices in the graph */
    long		vertex[2];	/* Indices of edge endpoints */
    double		weight;		/* Edge weight */
    rb_tree		*dictionary;	/* Dictionary of vertices */
    rb_tree		*prioq;		/* Priority queue of edges */
    struct vertex	*vp;		/* The current vertex */
    struct vertex	*np;		/* The neighbor of vp */
    struct edge		*ep;
    struct halfedge	*hp;		/* The current half edge */
    struct halfedge	*mp;		/* The mate (other end) of hp */

    /*
     *	Initialize the dictionary and the priority queue
     */
    dictionary = rb_create1("Dictionary of vertices", compare_vertices);
    rb_uniq_on1(dictionary);
    prioq = rb_create1("Priority queue of edges", compare_prioq_entries);

    /*
     *	Read in the graph
     */
    while (get_edge(stdin, vertex, &weight))
    {
	ep = mk_edge();
	for (i = 0; i < 2; ++i)		/* For each end of the edge... */
	{
	    hp = mk_halfedge();
	    vp = lookup_vertex(dictionary, vertex[i]);
	    hp -> h_edge = ep;
	    hp -> h_vertex = vp;
	    ep -> e_half[i] = hp;
	    BU_LIST_INSERT(&(vp -> v_halfedges), &(hp -> l));
	}
	ep -> e_half[0] -> h_mate = ep -> e_half[1];
	ep -> e_half[1] -> h_mate = ep -> e_half[0];
	ep -> e_half[0] -> h_neighbor = ep -> e_half[1] -> h_vertex;
	ep -> e_half[1] -> h_neighbor = ep -> e_half[0] -> h_vertex;
	bu_log("Got edge <x%x>, comprised of...\n", ep);
	print_halfedge(ep -> e_half[0]);
	print_halfedge(ep -> e_half[1]);
	ep -> e_weight = weight;
    }
    n = dictionary -> rbt_nm_nodes;

    /*
     *	Show us what you got
     */
#if 1
    bu_log("Residual adjaceny lists\n");
    rb_walk1(dictionary, print_vertex, INORDER);
    bu_log("-----------------------\n");
#endif
    
    /*
     *	Grow a minimum spanning tree,
     *	using Prim's algorithm
     */
    for (i = 1; i < n; ++i)
    {
	bu_log("%d: vp is now <x%x> %d", i, vp, vp -> v_index);
	for (BU_LIST_FOR(hp, halfedge, &(vp -> v_halfedges)))
	{
	    BU_CKMAG(hp, HALFEDGE_MAGIC, "halfedge");
	    mp = hp -> h_mate;
	    BU_CKMAG(mp, HALFEDGE_MAGIC, "halfedge");

	    enqueue_halfedge(prioq, hp);
	    bu_log("discarding halfedge <x%x>, mate of <x%x>\n", mp, hp);
	    BU_LIST_DEQUEUE(&(mp -> l));
	}
	hp = dequeue_halfedge(prioq);
	BU_CKMAG(hp, HALFEDGE_MAGIC, "halfedge");

	np = hp -> h_neighbor;
	BU_CKMAG(np, VERTEX_MAGIC, "vertex");
	bu_log(" and np is <x%x> %d\n", np, np -> v_index);
	BU_CKMAG(hp -> h_edge, EDGE_MAGIC, "edge");

	printf("OK, we're adding edge <x%x> (%d,%d) of weight %g\n",
	    hp -> h_edge, vp -> v_index, np -> v_index,
	    hp -> h_edge -> e_weight);
	fflush(stdout);
	vp = np;
    }
}

/*
 *	For each edge on input
 *	    Create the edge struc
 *	    For each vertex of the edge
 *		Get its vertex structure
 *		    If vertex is not in the dictionary
 *			Create the vertex struc
 *			Record the vertex struc in the dictionary
 *		    Return the vertex struc
 *		Record the vertex in the edge's vertexlist
 *		Record the edge in the vertex's edgelist
 *	    Record the weight in the edge
 *
 *    Pick any vertex v
 *    While you haven't reached every vertex
 *	For each of v's edges
 *	    Add the edge to the priority queue
 *	    Delete edge from the other endpoint's edgelist
 *	Dequeue a min-weight edge
 *	Set v to the edge's not-yet-reached vertex
 */
