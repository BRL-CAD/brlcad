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

rb_tree		*assignment;	/* Relabeling assignment */

/************************************************************************
 *									*
 *			The data structures				*
 *									*
 ************************************************************************/
struct curr_id
{
    struct bu_list	l;
    int			ci_id;
    struct bu_list	ci_regions;
    int			ci_newid;
};
#define	ci_magic	l.magic
#define	CURR_ID_NULL	((struct curr_id *) 0)
#define	CURR_ID_MAGIC	0x63726964

struct remap_reg
{
    struct bu_list	l;
    char		*rr_name;
};
#define	rr_magic	l.magic
#define	REMAP_REG_NULL	((struct remap_reg *) 0)
#define	REMAP_REG_MAGIC	0x726d7267

static int		debug = 1;

/************************************************************************
 *									*
 *	  Helper routines for manipulating the data structures		*
 *									*
 ************************************************************************/

/*
 *			     M K _ C U R R _ I D ( )
 *
 */
struct curr_id *mk_curr_id (region_id)

int	region_id;

{
    struct curr_id	*cip;

    cip = (struct curr_id *) bu_malloc(sizeof(struct curr_id), "curr_id");

    cip -> ci_magic = CURR_ID_MAGIC;
    cip -> ci_id = region_id;
    BU_LIST_INIT(&(cip -> ci_regions));
    cip -> ci_newid = region_id;

    return (cip);
}

/*
 *			  P R I N T _ C U R R _ I D ( )
 *
 */
void print_curr_id (v, depth)

void	*v;
int	depth;

{
    struct curr_id	*cip = (struct curr_id *) v;
    struct remap_reg	*rp;

    BU_CKMAG(cip, CURR_ID_MAGIC, "curr_id");

    bu_log(" curr_id <x%x> %d %d...\n",
	cip, cip -> ci_id, cip -> ci_newid);
    for (BU_LIST_FOR(rp, remap_reg, &(cip -> ci_regions)))
    {
	BU_CKMAG(rp, REMAP_REG_MAGIC, "remap_reg");

	bu_log("  %s\n", rp -> rr_name);
    }
}

/*
 *		F R E E _ C U R R _ I D ( )
 *
 */
void free_curr_id (cip)

struct curr_id	*cip;

{
    BU_CKMAG(cip, CURR_ID_MAGIC, "curr_id");
    bu_free((genptr_t) cip, "curr_id");
}

/*
 *		M K _ R E M A P _ R E G ( )
 *
 */
struct remap_reg *mk_remap_reg (region_name)

char	*region_name;

{
    struct remap_reg	*rp;

    rp = (struct remap_reg *) bu_malloc(sizeof(struct remap_reg), "remap_reg");

    rp -> rr_magic = REMAP_REG_MAGIC;

    rp -> rr_name = (char *) bu_malloc(strlen(region_name), "region name");
    strcpy(rp -> rr_name, region_name);

    return (rp);
}

/*
 *		F R E E _ R E M A P _ R E G ( )
 *
 */
void free_remap_reg (rp)

struct remap_reg	*rp;

{
    BU_CKMAG(rp, REMAP_REG_MAGIC, "remap_reg");
    bu_free((genptr_t) rp -> rr_name, "region name");
    bu_free((genptr_t) rp, "remap_reg");
}

/*
 *		C O M P A R E _ C U R R _ I D S ( )
 *
 *	    The comparison callback for libredblack(3)
 */
int compare_curr_ids (v1, v2)

void	*v1;
void	*v2;

{
    struct curr_id	*id1 = (struct curr_id *) v1;
    struct curr_id	*id2 = (struct curr_id *) v2;

    BU_CKMAG(id1, CURR_ID_MAGIC, "curr_id");
    BU_CKMAG(id2, CURR_ID_MAGIC, "curr_id");

    return (id1 -> ci_id  -  id2 -> ci_id);
}

/************************************************************************
 *									*
 *	  	Routines for manipulating the assignment		*
 *									*
 ************************************************************************/

/*
 *		L O O K U P _ C U R R _ I D ( )
 */
struct curr_id *lookup_curr_id(region_id)

int	region_id;

{
    int			rc;	/* Return code from rb_insert() */
    struct curr_id	*qcip;	/* The query */
    struct curr_id	*cip;	/* Value to return */

    /*
     *	Prepare the query
     */
    qcip = mk_curr_id(region_id);

    /*
     *	Perform the query by attempting an insertion...
     *	If the query succeeds (i.e., the insertion fails!),
     *	then we have our curr_id.
     *	Otherwise, we must create a new curr_id.
     */
    switch (rc = rb_insert(assignment, (void *) qcip))
    {
	case -1:
	    cip = (struct curr_id *) rb_curr1(assignment);
	    free_curr_id(qcip);
	    break;
	case 0:
	    cip = qcip;
	    break;
	default:
	    bu_log("rb_insert() returns %d:  This should not happen\n", rc);
	    exit (1);
    }

    return (cip);
}

/************************************************************************
 *									*
 *	  More or less vanilla-flavored stuff				*
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
    int			ch;
    int			i;
    int			num1, num2;
    int			newid;
    struct bu_list	cids;
    struct curr_id	*cip;

    if ((sfp == NULL) && ((sfp = bu_fopen(sf_name, "r")) == NULL))
    {
	bu_log("Cannot open specification file '%s'\n", sf_name);
	exit (1);
    }
    BU_CK_FILE(sfp);

    BU_LIST_INIT(&cids);

    for ( ; ; )
    {
	/*
	 *  Read in guy(s) to be assigned a particular new regionid
	 */
	for ( ; ; )
	{
	    while (isspace(ch = bu_fgetc(sfp)))
		;
	    if (ch == EOF)
		return (1);
	    switch (read_range(sfp, &ch, &num1, &num2))
	    {
		case 1:
		    cip = lookup_curr_id(num1);
		    BU_LIST_INSERT(&cids, &(cip -> l));
		    break;
		case 2:
		    if (num1 >= num2)
		    {
			bu_file_err(sfp, "remapid:read_spec()",
			    "Range out of order",
			    (sfp -> file_bp) - bu_vls_addr(&(sfp -> file_buf))
			    - 1);
		    }
		    for (i = num1; i <= num2; ++i)
		    {
			cip = lookup_curr_id(i);
			BU_LIST_INSERT(&cids, &(cip -> l));
		    }
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
		    if (read_int(sfp, &ch, &newid) != 1)
			return (-1);
		    break;
		default:
		    bu_file_err(sfp, "remapid:read_spec()",
			"Syntax error",
			(sfp -> file_bp) - bu_vls_addr(&(sfp -> file_buf))
			- 1);
		    break;
	    }
	    break;
	}

	/*
	 *	Tell each of these current regionids
	 *	about it's going to get the new regionid value
	 */
	while (BU_LIST_WHILE(cip, curr_id, &cids))
	{
	    cip -> ci_newid = newid;
	    BU_LIST_DEQUEUE(&(cip -> l));
	}
    }
}

void record_region (region_name, region_id)

char	*region_name;
int	region_id;

{
    struct curr_id	*cip;
    struct remap_reg	*rp;

    cip = lookup_curr_id(region_id);
    rp = mk_remap_reg(region_name);
    BU_LIST_INSERT(&(cip -> ci_regions), &(rp -> l));
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
    char		*db_name;	/* Name of database */
    char		*sf_name;	/* Name of spec file */
    BU_FILE		*sfp = NULL;	/* Spec file */
    int			ch;		/* Command-line character */
    int			i;
    int			tankill = 0;	/* TANKILL format (vs. BRL-CAD)? */

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

    /*
     *	Initialize the assignment
     */
    assignment = rb_create1("Relabeling assignment", compare_curr_ids);
    rb_uniq_on1(assignment);

    /*
     *	Read in the specification for the reassignment
     */
    read_spec (sfp, sf_name);

    if (debug)
	rb_walk1(assignment, print_curr_id, INORDER);

    record_region("USA", 1776);
    record_region("Will", 7);
    record_region("Griff", 8);
    record_region("Jake", 11);
    record_region("Chad", 11);

    if (debug)
    {
	bu_log(" . . . . . . . . . . .. .\n");
	rb_walk1(assignment, print_curr_id, INORDER);
    }
}
