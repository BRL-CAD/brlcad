/*
 *			R E M A P I D . C
 *
 *	Perform batch modifications of region IDs for BRL-CAD geometry
 *
 *	The program reads a .g file and a spec file indicating which
 *	region IDs to change to which new values.  It makes the
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
#include <ctype.h>
#include "machine.h"
#include "externs.h"			/* for getopt() */
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "db.h"
#include "raytrace.h"
#include "redblack.h"

rb_tree		*assignment;	/* Remapping assignment */
struct db_i	*dbip;		/* Instance of BRL-CAD database */

/************************************************************************
 *									*
 *			The Algorithm					*
 *									*
 *									*
 *  The remapping assignment is read from a specification file		*
 *  containing commands, the grammar for which looks something like:	*
 *									*
 *	 command --> id_list ':' id					*
 *	 id_list --> id_block | id_block ',' id_list			*
 *	id_block --> id | id '-' id					*
 *	   id    --> [0-9]+						*
 *									*
 *  The semantics of a command is:  For every region in the database	*
 *  whose region ID appears in the id_list before the ':', change its	*
 *  region ID to the value appearing after the ':'.			*
 *									*
 *  Consider all the (current) region IDs in the id_list of any		*
 *  command.  For each one, the corresponding curr_id structure is	*
 *  looked up in the red-black tree (and created, if necessary).	*
 *  As they're found, the curr_id's are stored in a list, so that	*
 *  when their new ID is found at the end of the command, it can be	*
 *  recorded in each of them.  All this processing of the specification	*
 *  file is performed by the function read_spec().			*
 *									*
 *  After the specification file has been processed, we read through	*
 *  the entire database, find every region and its (current) region ID,	*
 *  and add the region to the list of regions currently sharing that	*
 *  ID.  This is done by the function db_init().			*
 *									*
 *  At that point, it's a simple matter for main() to perform an	*
 *  inorder traversal of the red-black tree, writing to the database	*
 *  all the new region IDs.						*
 *									*
 *   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *  *
 *									*
 *			The Data Structures				*
 *									*
 *  assignment points to the red-black tree of curr_id structures,	*
 *  each of which records a region ID and all the regions in the	*
 *  database that currently sport that ID, in a remap_reg structure	*
 *  per region).							*
 *									*
 *									*
 *				        +-+				*
 *	       rb_tree		        | |				*
 *				        +-+				*
 *				       /   \				*
 *				   +-+/     \+-+			*
 *				   | |       | |			*
 *				   +-+       +-+			*
 *				   / \       / \			*
 *				 +-+ +-+   +-+ +-+			*
 *	  curr_id structures	 | | | |   | | | |			*
 *	  			 +-+ +-+   +-+ +-+			*
 *				  |					*
 *				  | ...					*
 *				  |					*
 *	 			+---+   +---+   +---+   +---+		*
 *	 remap_reg structures	|   |-->|   |-->|   |-->|   |--+	*
 *				+---+   +---+   +---+   +---+  |	*
 *				  ^                            |	*
 *				  |                            |	*
 *				  +----------------------------+	*
 *									*
 ************************************************************************/

struct curr_id
{
    struct bu_list	l;		
    int			ci_id;		/* The region ID */
    struct bu_list	ci_regions;	/* Regions now holding this ID */
    int			ci_newid;	/* Replacement ID for the regions */
};
#define	ci_magic	l.magic
#define	CURR_ID_NULL	((struct curr_id *) 0)
#define	CURR_ID_MAGIC	0x63726964

struct remap_reg
{
    struct bu_list		l;
    char			*rr_name;
    struct directory		*rr_dp;
    struct rt_db_internal	*rr_ip;
};
#define	rr_magic	l.magic
#define	REMAP_REG_NULL	((struct remap_reg *) 0)
#define	REMAP_REG_MAGIC	0x726d7267

static int		debug = 0;

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
 *		P R I N T _ N O N E M P T Y _ C U R R _ I D ( )
 *
 */
void print_nonempty_curr_id (v, depth)

void	*v;
int	depth;

{
    struct curr_id	*cip = (struct curr_id *) v;
    struct remap_reg	*rp;

    BU_CKMAG(cip, CURR_ID_MAGIC, "curr_id");

    if (BU_LIST_NON_EMPTY(&(cip -> ci_regions)))
    {
	bu_log(" curr_id <x%x> %d %d...\n",
	    cip, cip -> ci_id, cip -> ci_newid);
	for (BU_LIST_FOR(rp, remap_reg, &(cip -> ci_regions)))
	{
	    BU_CKMAG(rp, REMAP_REG_MAGIC, "remap_reg");

	    bu_log("  %s\n", rp -> rr_name);
	}
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
 *		L O O K U P _ C U R R _ I D ( )
 *
 *	Scrounge for a particular region in the red-black tree.
 *	If it's not found there, add it to the tree.  In either
 *	event, return a pointer to it.
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

    rp -> rr_name = (char *) bu_malloc(strlen(region_name) + 1, "region name");
    strcpy(rp -> rr_name, region_name);

    rp -> rr_dp = DIR_NULL;
    rp -> rr_ip = (struct rt_db_internal *) 0;

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
    bu_free((genptr_t) rp -> rr_ip, "rt_db_internal");
    bu_free((genptr_t) rp, "remap_reg");
}

/************************************************************************
 *									*
 *	  	The comparison callback for libredblack(3)		*
 *									*
 ************************************************************************/

/*
 *		C O M P A R E _ C U R R _ I D S ( )
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
 *	  Routines for reading the specification file			*
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
 *			  R E A D _ B L O C K ( )
 */
int read_block (sfp, ch, n1, n2)

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
	    bu_file_err(sfp, "remapid:read_block()",
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
	    switch (read_block(sfp, &ch, &num1, &num2))
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
			exit (-1);
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
		    exit (-1);
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

/************************************************************************
 *									*
 *	  Routines for procesing the geometry databases			*
 *									*
 ************************************************************************/

void record_region (region_name, region_id, dp, ip)

char			*region_name;
int			region_id;
struct directory	*dp;
struct rt_db_internal	*ip;

{
    struct curr_id	*cip;
    struct remap_reg	*rp;

    cip = lookup_curr_id(region_id);
    rp = mk_remap_reg(region_name);
    rp -> rr_dp = dp;
    rp -> rr_ip = ip;
    BU_LIST_INSERT(&(cip -> ci_regions), &(rp -> l));
}

void db_init(db_name)

char	*db_name;

{
    int				i;
    struct directory		*dp;
    struct rt_comb_internal	*comb;
    struct rt_db_internal	*ip;

    if ((dbip = db_open(db_name, "r+w")) == DBI_NULL)
    {
	bu_log("Cannot open database file '%s'\n", db_name);
	exit (1);
    }
    db_scan(dbip, (int (*)()) db_diradd, 1);

    for (i = 0; i < RT_DBNHASH; ++i)
	for (dp = dbip -> dbi_Head[i]; dp != DIR_NULL; dp = dp -> d_forw)
	{
	    if (!(dp -> d_flags & DIR_REGION))
		continue;
	    ip = (struct rt_db_internal *)
		bu_malloc(sizeof(struct rt_db_internal), "rt_db_internal");
	    if (rt_db_get_internal(ip, dp, dbip, (mat_t *) NULL) < 0)
	    {
		bu_log("remapid: rt_db_get_internal(%s) failed.  ",
		    dp -> d_namep);
		bu_log("This shouldn't happen\n");
		exit (1);
	    }
	    comb = (struct rt_comb_internal *) (ip -> idb_ptr);
	    RT_CK_COMB(comb);
	    record_region(dp -> d_namep, comb -> region_id, dp, ip);
	}
}

/*
 *		W R I T E _ A S S I G N M E N T ( )
 *
 */
void write_assignment (v, depth)

void	*v;
int	depth;

{
    int				region_id;
    struct curr_id		*cip = (struct curr_id *) v;
    struct remap_reg		*rp;
    struct rt_comb_internal	*comb;

    BU_CKMAG(cip, CURR_ID_MAGIC, "curr_id");

    if (BU_LIST_NON_EMPTY(&(cip -> ci_regions)))
    {
	region_id = cip -> ci_newid;
	for (BU_LIST_FOR(rp, remap_reg, &(cip -> ci_regions)))
	{
	    BU_CKMAG(rp, REMAP_REG_MAGIC, "remap_reg");
	    RT_CK_DB_INTERNAL(rp -> rr_ip);

	    comb = (struct rt_comb_internal *) rp -> rr_ip -> idb_ptr;
	    RT_CK_COMB(comb);
	    comb -> region_id = region_id;
	    if (rt_db_put_internal(rp -> rr_dp, dbip, rp -> rr_ip) < 0)
	    {
		bu_log("remapid: rt_db_put_internal(%s) failed.  ",
		    rp -> rr_dp -> d_namep);
		bu_log("This shouldn't happen\n");
		exit (1);
	    }
	}
    }
}

static void
tankill_reassign( db_name )
char *db_name;
{
	FILE *fd_in;
	int vertex_count, id, surr_code;
	struct curr_id *id_map, *cip;

	/* open TANKILL model */
	if( (fd_in=fopen( db_name, "r" )) == NULL )
	{
		bu_log( "Cannot open TANKILL database (%s)\n", db_name );
		perror( "remapid" );
		bu_bomb( "Cannot open TANKILL database\n" );
	}

	/* make a 'curr_id' structure to feed to rb_search */
	cip = mk_curr_id( 0 );

	/* filter TANKILL model, changing ids as we go */
	while( fscanf( fd_in, "%d %d %d", &vertex_count, &id, &surr_code ) != EOF )
	{
		int coord_no=0;
		int in_space=1;
		int ch;

		cip->ci_id = id;
		id_map = (struct curr_id *)rb_search( assignment, 0, (void *)cip );
		if( !id_map )
			printf( "%d %d %d", vertex_count, id, surr_code );
		else
			printf( "%d %d %d", vertex_count, id_map->ci_newid, surr_code );

		/* just copy the rest of the component */
		while( coord_no < 3*vertex_count || !in_space )
		{
			ch = fgetc( fd_in );
			if( ch == EOF && coord_no < 3*vertex_count )
			{
				bu_log( "Unexpected EOF while processing ident %d\n", id );
				bu_bomb( "Unexpected EOF\n" );
			}

			if( isspace( ch ) )
				in_space = 1;
			else if( in_space )
			{
				in_space = 0;
				coord_no++;
			}
			putchar( ch );
		}
	}
}

/************************************************************************
 *									*
 *	  	And finally... the main program				*
 *									*
 ************************************************************************/

/*
 *			   P R I N T _ U S A G E ( )
 */
void print_usage (void)
{
#define OPT_STRING	"gt?"

    bu_log("Usage: 'remapid [-{g|t}] [file.g|file.tankill] [spec_file]'\n\
	Note that the '-t' option sends the modified TANKILL model to stdout\n");
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
	    case 'g':
		tankill = 0;
		break;
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
    assignment = rb_create1("Remapping assignment", compare_curr_ids);
    rb_uniq_on1(assignment);

    /*
     *	Read in the specification for the reassignment
     */
    read_spec (sfp, sf_name);

    if( tankill )
	tankill_reassign( db_name );
    else
    {
	db_init(db_name);

	if (debug)
		rb_walk1(assignment, print_nonempty_curr_id, INORDER);
	else
		rb_walk1(assignment, write_assignment, INORDER);
    }
}
