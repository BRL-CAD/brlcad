/*
 *			R E M A P I D . C
 *
 *	Perform batch modifications of region_id's for BRL-CAD geometry
 *
 *	The program reads a .g file and a spec file indicating which
 *	region_id's to change to which new values.  It makes the
 *	specified changes in the .g file.
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
 *	This software is Copyright (C) 1997 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"
#include <stdio.h>
#include "machine.h"
#include "externs.h"			/* for getopt() */
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

static rb_tree		*prioq;		/* Priority queue of bridges */
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
struct bridge *mk_bridge (vcp, vup, weight)

struct vertex	*vcp;
struct vertex	*vup;
double		weight;

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
void free_bridge (bp)

struct bridge	*bp;

{
    BU_CKMAG(bp, BRIDGE_MAGIC, "bridge");
    bu_free((genptr_t) bp, "bridge");
}

/*
 *			  P R I N T _ B R I D G E ( )
 *
 */
void print_bridge (bp)

struct bridge	*bp;

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
    rb_walk(prioq, PRIOQ_WEIGHT, print_bridge, INORDER);	\
    bu_log("------------------------------\n\n");		\
}

/*
 *			     M K _ V E R T E X ( )
 *
 */
struct vertex *mk_vertex (index, label)

long	index;
char	*label;

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
void print_vertex (v, depth)

void	*v;
int	depth;

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
void free_vertex (vp)

struct vertex	*vp;

{
    BU_CKMAG(vp, VERTEX_MAGIC, "vertex");
    free_bridge(vp -> v_bridge);
    bu_free((genptr_t) vp, "vertex");
}

/*
 *			   M K _ N E I G H B O R ( )
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
 *		C O M P A R E _ V E R T E X _ I N D I C E S ( )
 */
int compare_vertex_indices (v1, v2)

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
 *		 C O M P A R E _ V E R T E X _ L A B E L S ( )
 */
int compare_vertex_labels (v1, v2)

void	*v1;
void	*v2;

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
int compare_bridge_weights (v1, v2)

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
    return ((delta <  0.0) ? -1 :
	    (delta == 0.0) ?  0 :
			      1);
}

/*
 *		C O M P A R E _ B R I D G E _ I N D I C E S ( )
 */
int compare_bridge_indices (v1, v2)

void	*v1;
void	*v2;

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
struct vertex *lookup_vertex(dict, index, label)

rb_tree	*dict;
long	index;
char	*label;

{
    int			rc;	/* Return code from rb_insert() */
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
	    bu_log("rb_insert() returns %d:  This should not happen\n", rc);
	    exit (1);
    }

    return (vp);
}

/*
 *			  A D D _ T O _ P R I O Q ( )
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
 *			D E L _ F R O M _ P R I O Q ( )
 *
 */
void del_from_prioq (vp)

struct vertex	*vp;

{
    BU_CKMAG(vp, VERTEX_MAGIC, "vertex");
    BU_CKMAG(vp -> v_bridge, BRIDGE_MAGIC, "bridge");

    if (debug)
	bu_log("del_from_prioq(<x%x>... bridge <x%x> %d)\n",
	    vp, vp -> v_bridge, vp -> v_bridge -> b_index);
    if (rb_search(prioq, PRIOQ_INDEX, (void *) (vp -> v_bridge)) == NULL)
    {
	bu_log("del_from_prioq: Cannot find bridge <x%x>.", vp -> v_bridge);
	bu_log("  This should not happen\n");
	exit (1);
    }
    rb_delete(prioq, PRIOQ_INDEX);
}

/*
 *			   E X T R A C T _ M I N ( )
 *
 */
struct bridge *extract_min ()
{
    struct bridge	*bp;

    bp = (struct bridge *) rb_min(prioq, PRIOQ_WEIGHT);
    if (bp != BRIDGE_NULL)
    {
	BU_CKMAG(bp, BRIDGE_MAGIC, "bridge");
	rb_delete(prioq, PRIOQ_WEIGHT);
    }
    return (bp);
}

/************************************************************************
 *									*
 *	  More or less vanilla-flavored stuff for MST			*
 *									*
 ************************************************************************/

/*
 *			  R E A D _ I N T ( )
 */
int read_int (sfp, ch, n)

BU_FILE	*sfp;
int	*ch;
int	*n;		/* The result */

{
    int	got_digit = 0;	/* Did we actually succeed in reading a number? */
    int	result;

    BU_CK_FILE(sfp);

    while (isspace(*ch))
	*ch = bu_fgetc(sfp);
    
    for (result = 0; isdigit(*ch); *ch = bu_fgetc(sfp))
    {
	got_digit = 1;
	result *= 10;
	result += *ch - '0';
    }

    if (got_digit)
    {
	*n = result;
	return (1);
    }
    else if (*ch == EOF)
	bu_file_err(sfp, "remapid",
	    "Encountered EOF while expecting an integer", -1);
    else
	bu_file_err(sfp, "remapid:read_int()",
	    "Encountered nondigit",
	    (sfp -> file_bp) - bu_vls_addr(&(sfp -> file_buf)) - 1);
    return (-1);
}

/*
 *			  R E A D _ R A N G E ( )
 */
int read_range (sfp, ch, n1, n2)

BU_FILE	*sfp;
int	*ch;
int	*n1, *n2;

{
    BU_CK_FILE(sfp);

    if (read_int(sfp, ch, n1) != 1)
	return (-1);

    while (isspace(*ch))
	*ch = bu_fgetc(sfp);
    switch (*ch)
    {
	case ',':
	case ':':
	    return (1);
	case '-':
	    *ch = bu_fgetc(sfp);
	    if (read_int(sfp, ch, n2) != 1)
		return (-1);
	    else
		return (2);
	default:
	    bu_file_err(sfp, "remapid:read_range()",
		"Syntax error",
		(sfp -> file_bp) - bu_vls_addr(&(sfp -> file_buf)) - 1);
	    return (-1);
    }
}

/*
 *			  R E A D _ S P E C ( )
 */
int read_spec (sfp, sf_name)

BU_FILE	*sfp;
char	*sf_name;

{
    int	ch;
    int	num1, num2;

    if ((sfp == NULL) && ((sfp = bu_fopen(sf_name, "r")) == NULL))
    {
	bu_log("Cannot open specification file '%s'\n", sf_name);
	exit (1);
    }
    BU_CK_FILE(sfp);

    for ( ; ; )
    for ( ; ; )
    {
	while (isspace(ch = bu_fgetc(sfp)))
	    ;
	if (ch == EOF)
	    return (1);
	switch (read_range(sfp, &ch, &num1, &num2))
	{
	    case 1:
		printf("OK, I got a single number %d\n", num1);
		break;
	    case 2:
		printf("OK, I got a pair of numbers %d and %d\n", num1, num2);
		break;
	    default:
		return (-1);
	}
	while (isspace(ch))
	    ch = bu_fgetc(sfp);

	switch (ch)
	{
	    case ',':
		continue;
	    case ':':
		ch = bu_fgetc(sfp);
		if (read_int(sfp, &ch, &num1) != 1)
		    return (-1);
		printf("OK, I got the last number %d\n\n", num1);
		break;
	    default:
		bu_file_err(sfp, "remapid:read_spec()",
		    "Syntax error",
		    (sfp -> file_bp) - bu_vls_addr(&(sfp -> file_buf)) - 1);
		break;
	}
	break;
    }
}

/*
 *			   P R I N T _ U S A G E ( )
 */
void print_usage (void)
{
#define OPT_STRING	"t?"

    bu_log("Usage: 'remapid [-t] file.g [spec_file]'\n");
}

/*
 *                                M A I N ( )
 */
main (argc, argv)

int	argc;
char	*argv[];

{
    int		numeric;
    char	*label;

    char		*db_name;	/* Name of database */
    char		*sf_name;	/* Name of spec file */
    BU_FILE		*sfp = NULL;	/* Spec file */
    int			ch;		/* Command-line character */
    int			i;
    int			tankill = 0;	/* Handle TANKILL format (vs. BRL-CAD)? */
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
    int			(*po[2])();	/* Priority queue order functions */

    extern int	optind;			/* index from getopt(3C) */
    extern char	*optarg;		/* argument from getopt(3C) */

    while ((ch = getopt(argc, argv, OPT_STRING)) != EOF)
	switch (ch)
	{
	    case 't':
		tankill = 1;
		break;
	    case '?':
	    default:
		print_usage();
		exit (ch != '?');
		return(0);
	}

    switch (argc - optind)
    {
	case 1:
	    sf_name = "stdin";
	    sfp = bu_stdin;
	    /* Break intentionally missing */
	case 2:
	    break;
	default:
	    print_usage();
	    exit (1);
    }

    /*
     *	Open database and specification file, as necessary
     */
    db_name = argv[optind++];
    if (sfp == NULL)
	sf_name = argv[optind];

#if 0
    /*
     *	Initialize the dictionary
     */
    dictionary = rb_create1("Dictionary of vertices",
		    numeric ? compare_vertex_indices
			    : compare_vertex_labels);
    rb_uniq_on1(dictionary);
#endif

    /*
     *	Read in the graph
     */
    read_spec (sfp, sf_name);
    exit (0);

    /*
     *	Initialize the priority queue
     */
    po[PRIOQ_INDEX] = compare_bridge_indices;
    po[PRIOQ_WEIGHT] = compare_bridge_weights;
    prioq = rb_create("Priority queue of bridges", 2, po);
    rb_walk1(dictionary, add_to_prioq, INORDER);

    if (debug)
    {
	print_prioq();
	rb_walk1(dictionary, print_vertex, INORDER);
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
		(void) printf("%d %d %g\n",
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
}
