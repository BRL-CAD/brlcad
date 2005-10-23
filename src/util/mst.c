/*                           M S T . C
 * BRL-CAD
 *
 * Copyright (C) 1996-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file mst.c
 *
 *	Construct a minimum spanning tree using Prim's Algorithm
 *
 *	After reading in a graph, the program builds and maintains
 *	a priority queue of "bridges" between the gradually expanding
 *	MST and each of the vertices not yet in the MST.  In this
 *	context, vertices are classified as "civilized" (i.e. in the
 *	MST) and "uncivilized" (not yet in the MST).
 *
 *  Author -
 *	Paul J. Tanenbaum
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "machine.h"
#include "bu.h"
#include "redblack.h"

/************************************************************************
 *									*
 *			The data structures				*
 *									*
 ************************************************************************/
struct vertex
{
    long		v_magic;
    long		v_index;
    char		*v_label;
    int			v_civilized;
    struct bu_list	v_neighbors;
    struct bridge	*v_bridge;
};
#define	VERTEX_NULL	((struct vertex *) 0)
#define	VERTEX_MAGIC	0x6d737476

struct bridge
{
    long		b_magic;
    unsigned long	b_index;	/* automatically generated (unique) */
    double		b_weight;
    struct vertex	*b_vert_civ;
    struct vertex	*b_vert_unciv;
};
#define	BRIDGE_NULL	((struct bridge *) 0)
#define	BRIDGE_MAGIC	0x6d737462

static bu_rb_tree		*prioq;		/* Priority queue of bridges */
#define	PRIOQ_INDEX	0
#define	PRIOQ_WEIGHT	1

struct neighbor
{
    struct bu_list	l;
    struct vertex	*n_vertex;
    double		n_weight;
};
#define	n_magic		l.magic
#define	NEIGHBOR_NULL	((struct neighbor *) 0)
#define	NEIGHBOR_MAGIC	0x6d73746e

static int		debug = 1;

/************************************************************************
 *									*
 *	  Helper routines for manipulating the data structures		*
 *									*
 ************************************************************************/

/*
 *			     M K _ B R I D G E ( )
 *
 */
struct bridge *mk_bridge (struct vertex *vcp, struct vertex *vup, double weight)
{
    static unsigned long	next_index = 0;
    struct bridge		*bp;

    /*
     *	XXX Hope that unsigned long is sufficient to ensure
     *	uniqueness of bridge indices.
     */

    BU_CKMAG(vup, VERTEX_MAGIC, "vertex");

    bp = (struct bridge *) bu_malloc(sizeof(struct bridge), "bridge");

    bp -> b_magic = BRIDGE_MAGIC;
    bp -> b_index = ++next_index;
    bp -> b_weight = weight;
    bp -> b_vert_civ = vcp;
    bp -> b_vert_unciv = vup;

    return (bp);
}
#define	mk_init_bridge(vp)	(mk_bridge(VERTEX_NULL, (vp), 0.0))
#define	is_finite_bridge(bp)	((bp) -> b_vert_civ != VERTEX_NULL)
#define	is_infinite_bridge(bp)	((bp) -> b_vert_civ == VERTEX_NULL)

/*
 *			   F R E E _ B R I D G E ( )
 *
 *	N.B. - It is up to the calling routine to free
 *		the b_vert_civ and b_vert_unciv members of bp.
 */
void free_bridge (struct bridge *bp)
{
    BU_CKMAG(bp, BRIDGE_MAGIC, "bridge");
    bu_free((genptr_t) bp, "bridge");
}

/*
 *			  P R I N T _ B R I D G E ( )
 *
 */
void print_bridge (struct bridge *bp)
{
    BU_CKMAG(bp, BRIDGE_MAGIC, "bridge");

    bu_log(" bridge <x%x> %d... <x%x> and <x%x>, weight = %g\n",
	bp, bp -> b_index,
	bp -> b_vert_civ, bp -> b_vert_unciv, bp -> b_weight);
}

/*
 *			   P R I N T _ P R I O Q ( )
 */
#define	print_prioq()						\
{								\
    bu_log("----- The priority queue -----\n");			\
    bu_rb_walk(prioq, PRIOQ_WEIGHT, print_bridge, INORDER);	\
    bu_log("------------------------------\n\n");		\
}

/*
 *			     M K _ V E R T E X ( )
 *
 */
struct vertex *mk_vertex (long int index, char *label)
{
    struct vertex	*vp;

    vp = (struct vertex *) bu_malloc(sizeof(struct vertex), "vertex");

    vp -> v_magic = VERTEX_MAGIC;
    vp -> v_index = index;
    vp -> v_label = label;
    vp -> v_civilized = 0;
    BU_LIST_INIT(&(vp -> v_neighbors));
    vp -> v_bridge = mk_init_bridge(vp);

    return (vp);
}

/*
 *			  P R I N T _ V E R T E X ( )
 *
 */
void print_vertex (void *v, int depth)
{
    struct vertex	*vp = (struct vertex *) v;
    struct neighbor	*np;

    BU_CKMAG(vp, VERTEX_MAGIC, "vertex");

    bu_log(" vertex <x%x> %d '%s' %s...\n",
	vp, vp -> v_index, vp -> v_label,
	vp -> v_civilized ? "civilized" : "uncivilized");
    for (BU_LIST_FOR(np, neighbor, &(vp -> v_neighbors)))
    {
	BU_CKMAG(np, NEIGHBOR_MAGIC, "neighbor");
	BU_CKMAG(np -> n_vertex, VERTEX_MAGIC, "vertex");

	bu_log("  is a neighbor <x%x> of vertex <x%x> %d '%s' at cost %g\n",
	    np, np -> n_vertex,
	    np -> n_vertex -> v_index, np -> n_vertex -> v_label,
	    np -> n_weight);
    }
}

/*
 *			   F R E E _ V E R T E X ( )
 *
 *	N.B. - This routine frees the v_bridge member of vp.
 */
void free_vertex (struct vertex *vp)
{
    BU_CKMAG(vp, VERTEX_MAGIC, "vertex");
    free_bridge(vp -> v_bridge);
    bu_free((genptr_t) vp, "vertex");
}

/*
 *			   M K _ N E I G H B O R ( )
 *
 */
struct neighbor *mk_neighbor (struct vertex *vp, double weight)
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
 *		C O M P A R E _ V E R T E X _ I N D I C E S ( )
 */
int compare_vertex_indices (void *v1, void *v2)
{
    struct vertex	*vert1 = (struct vertex *) v1;
    struct vertex	*vert2 = (struct vertex *) v2;

    BU_CKMAG(vert1, VERTEX_MAGIC, "vertex");
    BU_CKMAG(vert2, VERTEX_MAGIC, "vertex");

    return (vert1 -> v_index  -  vert2 -> v_index);
}

/*
 *		 C O M P A R E _ V E R T E X _ L A B E L S ( )
 */
int compare_vertex_labels (void *v1, void *v2)
{
    struct vertex	*vert1 = (struct vertex *) v1;
    struct vertex	*vert2 = (struct vertex *) v2;

    BU_CKMAG(vert1, VERTEX_MAGIC, "vertex");
    BU_CKMAG(vert2, VERTEX_MAGIC, "vertex");
    if (vert1 -> v_label == '\0')
    {
	bu_log("compare_vertex_labels: null label in vertex <x%x> %d\n",
	    vert1, vert1 -> v_index);
	exit (1);
    }
    if (vert2 -> v_label == '\0')
    {
	bu_log("compare_vertex_labels: null label in vertex <x%x> %d\n",
	    vert2, vert2 -> v_index);
	exit (1);
    }

    if (*(vert1 -> v_label) < *(vert2 -> v_label))
	return -1;
    else if (*(vert1 -> v_label) > *(vert2 -> v_label))
	return 1;
    else
	return (strcmp(vert1 -> v_label, vert2 -> v_label));
}

/*
 *		C O M P A R E _ B R I D G E _ W E I G H T S ( )
 */
int compare_bridge_weights (void *v1, void *v2)
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
    return ((delta <  0.0) ? -1 :
	    (delta == 0.0) ?  0 :
			      1);
}

/*
 *		C O M P A R E _ B R I D G E _ I N D I C E S ( )
 */
int compare_bridge_indices (void *v1, void *v2)
{
    struct bridge	*b1 = (struct bridge *) v1;
    struct bridge	*b2 = (struct bridge *) v2;

    BU_CKMAG(b1, BRIDGE_MAGIC, "bridge");
    BU_CKMAG(b2, BRIDGE_MAGIC, "bridge");

    if (b1 -> b_index < b2 -> b_index)
	return -1;
    else if (b1 -> b_index == b2 -> b_index)
	return 0;
    else
	return 1;
}

/************************************************************************
 *									*
 *	    A similar comparison function				*
 *									*
 ************************************************************************/

/*
 *		       C O M P A R E _ W E I G H T S ( )
 */
#define	compare_weights(w1, w2)					\
(								\
    ((w1) <= 0.0) ?						\
    (								\
	((w2) <= 0.0) ?						\
	    0 :							\
	    1							\
    ) :								\
    (								\
	((w2) <= 0.0) ?						\
	    -1 :						\
	    ((w1) < (w2)) ?					\
		-1 :						\
		((w1) == (w2)) ?				\
		    0 :						\
		    1						\
    )								\
)

/************************************************************************
 *									*
 *	  Routines for manipulating the dictionary and priority queue	*
 *									*
 ************************************************************************/

/*
 *			 L O O K U P _ V E R T E X ( )
 */
struct vertex *lookup_vertex(bu_rb_tree *dict, long int index, char *label)
{
    int			rc;	/* Return code from bu_rb_insert() */
    struct vertex	*qvp;	/* The query */
    struct vertex	*vp;	/* Value to return */

    /*
     *	Prepare the dictionary query
     */
    qvp = mk_vertex(index, label);

    /*
     *	Perform the query by attempting an insertion...
     *	If the query succeeds (i.e., the insertion fails!),
     *	then we have our vertex.
     *	Otherwise, we must create a new vertex.
     */
    switch (rc = bu_rb_insert(dict, (void *) qvp))
    {
	case -1:
	    vp = (struct vertex *) bu_rb_curr1(dict);
	    free_vertex(qvp);
	    break;
	case 0:
	    vp = qvp;
	    break;
	default:
	    bu_log("bu_rb_insert() returns %d:  This should not happen\n", rc);
	    exit (1);
    }

    return (vp);
}

/*
 *			  A D D _ T O _ P R I O Q ( )
 *
 */
void add_to_prioq (void *v, int depth)
{
    struct vertex	*vp = (struct vertex *) v;

    BU_CKMAG(vp, VERTEX_MAGIC, "vertex");
    BU_CKMAG(vp -> v_bridge, BRIDGE_MAGIC, "bridge");

    bu_rb_insert(prioq, (void *) (vp -> v_bridge));
}

/*
 *			D E L _ F R O M _ P R I O Q ( )
 *
 */
void del_from_prioq (struct vertex *vp)
{
    BU_CKMAG(vp, VERTEX_MAGIC, "vertex");
    BU_CKMAG(vp -> v_bridge, BRIDGE_MAGIC, "bridge");

    if (debug)
	bu_log("del_from_prioq(<x%x>... bridge <x%x> %d)\n",
	    vp, vp -> v_bridge, vp -> v_bridge -> b_index);
    if (bu_rb_search(prioq, PRIOQ_INDEX, (void *) (vp -> v_bridge)) == NULL)
    {
	bu_log("del_from_prioq: Cannot find bridge <x%x>.", vp -> v_bridge);
	bu_log("  This should not happen\n");
	exit (1);
    }
    bu_rb_delete(prioq, PRIOQ_INDEX);
}

/*
 *			   E X T R A C T _ M I N ( )
 *
 */
struct bridge *extract_min (void)
{
    struct bridge	*bp;

    bp = (struct bridge *) bu_rb_min(prioq, PRIOQ_WEIGHT);
    if (bp != BRIDGE_NULL)
    {
	BU_CKMAG(bp, BRIDGE_MAGIC, "bridge");
	bu_rb_delete(prioq, PRIOQ_WEIGHT);
    }
    return (bp);
}

/************************************************************************
 *									*
 *	  More or less vanilla-flavored stuff for MST			*
 *									*
 ************************************************************************/

/*
 *			      G E T _ E D G E ( )
 */
int get_edge (FILE *fp, long int *index, char **label, double *w, int numeric)


    	       		/* Indices of edge endpoints */
    	        	/* Labels of edge endpoints */
      	   		/* Weight */
   	        	/* Use indices instead of labels? */

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
	if (numeric)
	{
	    if (sscanf(bp, "%ld%ld%lg", &index[0], &index[1], w) != 3)
	    {
		bu_log("Illegal input on line %d: '%s'\n", line_nm, bp);
		return (-1);
	    }
	    else
	    {
		label[0] = label[1] = NULL;
		break;
	    }
	}
	else
	{
	    char	*bep;

	    for (bep = bp; (*++bep != ' ') && (*bep != '\t'); ++bep)
		if (*bep == '\0')
		{
		    bu_log("Illegal input on line %d: '%s'\n",
			line_nm, bu_vls_addr(&buf));
		    return (-1);
		}
	    *bep = '\0';
	    label[0] = bu_strdup(bp);

	    for (bp = bep + 1; (*bp == ' ') || (*bp == '\t'); ++bp)
		if (*bep == '\0')
		{
		    bu_log("Illegal input on line %d: '%s'\n",
			line_nm, bu_vls_addr(&buf));
		    return (-1);
		}
	    for (bep = bp; (*++bep != ' ') && (*bep != '\t'); ++bep)
		if (*bep == '\0')
		{
		    bu_log("Illegal input on line %d: '%s'\n",
			line_nm, bu_vls_addr(&buf));
		    return (-1);
		}
	    *bep = '\0';
	    label[1] = bu_strdup(bp);

	    if (sscanf(bep + 1, "%lg", w) != 1)
	    {
		bu_log("Illegal input on line %d: '%s'\n",
		    line_nm, bu_vls_addr(&buf));
		return (-1);
	    }
	    else
	    {
		index[0] = index[1] = -1;
		break;
	    }
	}
    }
    return (1);
}

/*
 *			   P R I N T _ U S A G E ( )
 */
void print_usage (void)
{
#define OPT_STRING	"n?"

    bu_log("Usage: 'mst [-n]'\n");
}

/*
 *                                M A I N ( )
 */
int
main (int argc, char **argv)
{
    char		*label[2];	/* Labels of edge endpoints */
    int			ch;		/* Command-line character */
    int			i;
    int			numeric = 0;	/* Use vertex indices (vs. labels)? */
    long		index[2];	/* Indices of edge endpoints */
    double		weight;		/* Edge weight */
    bu_rb_tree		*dictionary;	/* Dictionary of vertices */
    struct bridge	*bp;		/* The current bridge */
    struct vertex	*up;		/* An uncivilized neighbor of vup */
    struct vertex	*vcp;		/* The civilized vertex of bp */
    struct vertex	*vup;		/* The uncvilized vertex of bp */
    struct vertex	*vertex[2];	/* The current edge */
    struct neighbor	*neighbor[2];	/* Their neighbors */
    struct neighbor	*np;		/* A neighbor of vup */
    int			(*po[2])();	/* Priority queue order functions */

    while ((ch = bu_getopt(argc, argv, OPT_STRING)) != EOF)
	switch (ch)
	{
	    case 'n':
		numeric = 1;
		break;
	    case '?':
	    default:
		print_usage();
		exit (ch != '?');
		return(0);
	}

    /*
     *	Initialize the dictionary
     */
    dictionary = bu_rb_create1("Dictionary of vertices",
		    numeric ? compare_vertex_indices
			    : compare_vertex_labels);
    bu_rb_uniq_on1(dictionary);

    /*
     *	Read in the graph
     */
    while (get_edge(stdin, index, label, &weight, numeric))
    {
	for (i = 0; i < 2; ++i)		/* For each end of the edge... */
	{
	    vertex[i] = lookup_vertex(dictionary, index[i], label[i]);
	    neighbor[i] = mk_neighbor(VERTEX_NULL, weight);
	    BU_LIST_INSERT(&(vertex[i] -> v_neighbors), &(neighbor[i] -> l));
	}
	neighbor[0] -> n_vertex = vertex[1];
	neighbor[1] -> n_vertex = vertex[0];
    }

    /*
     *	Initialize the priority queue
     */
    po[PRIOQ_INDEX] = compare_bridge_indices;
    po[PRIOQ_WEIGHT] = compare_bridge_weights;
    prioq = bu_rb_create("Priority queue of bridges", 2, po);
    bu_rb_walk1(dictionary, add_to_prioq, INORDER);

    if (debug)
    {
	print_prioq();
	bu_rb_walk1(dictionary, print_vertex, INORDER);
    }

    /*
     *	Grow a minimum spanning tree, using Prim's algorithm...
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
    weight = 0.0;
    while ((bp = extract_min()) != BRIDGE_NULL)
    {
	if (debug)
	{
	    bu_log("extracted min-weight bridge <x%x>, leaving...\n", bp);
	    print_prioq();
	}

	BU_CKMAG(bp, BRIDGE_MAGIC, "bridge");
	vcp = bp -> b_vert_civ;
	vup = bp -> b_vert_unciv;
	BU_CKMAG(vup, VERTEX_MAGIC, "vertex");

	if (is_finite_bridge(bp))
	{
	    BU_CKMAG(vcp, VERTEX_MAGIC, "vertex");
	    if (numeric)
		(void) printf("%ld %ld %g\n",
		    vcp -> v_index, vup -> v_index, bp -> b_weight);
	    else
		(void) printf("%s %s %g\n",
		    vcp -> v_label, vup -> v_label, bp -> b_weight);
	    weight += bp -> b_weight;
	}
	free_bridge(bp);
	vup -> v_civilized = 1;

	if (debug)
	{
	    bu_log("Looking for uncivilized neighbors of...\n");
	    print_vertex((void *) vup, 0);
	}
	while (BU_LIST_WHILE(np, neighbor, &(vup -> v_neighbors)))
	{
	    BU_CKMAG(np, NEIGHBOR_MAGIC, "neighbor");
	    up = np -> n_vertex;
	    BU_CKMAG(up, VERTEX_MAGIC, "vertex");

	    if (up -> v_civilized == 0)
	    {
		BU_CKMAG(up -> v_bridge, BRIDGE_MAGIC, "bridge");
		if (compare_weights(np -> n_weight,
				    up -> v_bridge -> b_weight) < 0)
		{
		    del_from_prioq(up);
		    if (debug)
		    {
			bu_log("After the deletion of bridge <x%x>...\n",
			    up -> v_bridge);
			print_prioq();
		    }
		    up -> v_bridge -> b_vert_civ = vup;
		    up -> v_bridge -> b_weight = np -> n_weight;
		    add_to_prioq((void *) up, 0);
		    if (debug)
		    {
			bu_log("Reduced bridge <x%x> weight to %g\n",
			    up -> v_bridge,
			    up -> v_bridge -> b_weight);
			print_prioq();
		    }
		}
		else if (debug)
		    bu_log("bridge <x%x>'s weight of %g stands up\n",
			    up -> v_bridge, up -> v_bridge -> b_weight);
	    }
	    else if (debug)
		bu_log("Skipping civilized neighbor <x%x>\n", up);
	    BU_LIST_DEQUEUE(&(np -> l));
	}
    }
    bu_log("MST weight: %g\n", weight);
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
