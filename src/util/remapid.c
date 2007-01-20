/*                       R E M A P I D . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2007 United States Government as represented by
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
/** @file remapid.c
 *
 *	Perform batch modifications of region IDs for BRL-CAD
 *	(or TANKILL) geometry
 *
 *	The program reads a .g (or TANKILL) file and a spec file
 *	indicating which region IDs to change to which new values.
 *	For a .g file, the specified changes are made to that file;
 *	For a TANKILL file, a modified model is written to stdout.
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

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "db.h"
#include "raytrace.h"
#include "redblack.h"


/*
 * ******************** Hack
 */

BU_EXTERN(struct bu_file	*bu_fopen, (char *fname, char *type) );
BU_EXTERN(int			bu_fclose, (struct bu_file *bfp) );
BU_EXTERN(int			bu_fgetc, (struct bu_file *bfp) );
BU_EXTERN(void			bu_printfile, (struct bu_file *bfp) );

/*
 *	General I/O for ASCII files: bu_file support
 */
struct bu_file  {
	long		file_magic;
	FILE		*file_ptr;	/* the actual file */
	char		*file_name;
	struct bu_vls	file_buf;	/* contents of current line */
	char		*file_bp;	/* pointer into current line */
	int		file_needline;	/* time to grab another line? */
	int		file_linenm;
	int		file_comment;	/* the comment character */
	int		file_buflen;	/* length of intact buffer */
};
typedef struct bu_file		BU_FILE;
#define BU_FILE_MAGIC		0x6275666c
#define BU_CK_FILE(_fp)		BU_CKMAG(_fp, BU_FILE_MAGIC, "bu_file")

#define bu_stdin		(&bu_iob[0])
extern BU_FILE			bu_iob[1];
#define BU_FILE_NO_COMMENT	-1


/*
 *			F I L E . C
 *
 *  General I/O for ASCII files
 *
 *  Author -
 *	Paul Tanenbaum
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
static const char RCSrtstring[] = "@(#)$Header$ (BRL)";

#include "common.h"

#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if defined(HAVE_STDARG_H)
# include <stdarg.h>
#endif

#include "machine.h"
#include "bu.h"

/*
 *		XXX	Warning!	XXX
 *
 *	The following initialization of bu_stdin is essentially
 *	an inline version of bu_fopen() and bu_vls_init().  As such,
 *	it depends heavily on the definitions of struct bu_file and
 *	struct bu_vls in ../h/bu.h
 *
 *		XXX			XXX
 */
char	dmy_eos = '\0';
BU_FILE	bu_iob[1] = {
    {
	BU_FILE_MAGIC,
#if 0
	stdin,		/* this won't work on Linux, others */
#else
    	NULL,
#endif
	"stdin",
	{
	    BU_VLS_MAGIC, (char *) 0, 0, 0, 0
	},
	&dmy_eos, 1, 0, '#', -1
    }
};

/*
 *			B U _ F O P E N
 *
 */
BU_FILE *bu_fopen (register char *fname, register char *type)
{
    BU_FILE	*bfp;
    FILE	*fp;

    if ((fp = fopen(fname, type)) == NULL)
	return (NULL);

    bfp = (BU_FILE *) bu_malloc(sizeof(BU_FILE), "bu_file struct");

    bfp -> file_magic = BU_FILE_MAGIC;
    bfp -> file_ptr = fp;
    bfp -> file_name = fname;
    bu_vls_init(&(bfp -> file_buf));
    bfp -> file_bp = bu_vls_addr(&(bfp -> file_buf));
    bfp -> file_needline = 1;
    bfp -> file_linenm = 0;
    bfp -> file_comment = '#';
    bfp -> file_buflen = -1;

    return (bfp);
}

/*
 *			B U _ F C L O S E
 *
 *	Close the file and free the associated memory
 */
int bu_fclose (register BU_FILE *bfp)
{
    int	close_status;

    BU_CK_FILE(bfp);

    close_status = fclose(bfp -> file_ptr);

    if (bfp != bu_stdin)
    {
	bfp -> file_magic = 0;
	bfp -> file_ptr = NULL;
	bfp -> file_name = (char *) 0;
	bfp -> file_bp = (char *) 0;
	bu_vls_free(&(bfp -> file_buf));
	bu_free((genptr_t) bfp, "bu_file struct");
    }
    return (close_status);
}

/*
 *			B U _ F G E T C
 *
 */
int bu_fgetc (register BU_FILE *bfp)
{
    char	*cp = (char *)NULL;
    int		comment_char;	/* The comment character */
    int		strip_comments;	/* Should I strip comments? */

    BU_CK_FILE(bfp);

    strip_comments = isprint(comment_char = bfp -> file_comment);

    /*
     *	If buffer is empty, note that it's time for a new line of input
     */
    if ((*(bfp -> file_bp) == '\0') && ! (bfp -> file_needline))
    {
	bfp -> file_needline = 1;
	return ('\n');
    }

    /*
     *    If it's time to grab a line of input from the file, do so.
     */
    while (bfp -> file_needline)
    {
	bu_vls_trunc(&(bfp -> file_buf), 0);
	if (bu_vls_gets(&(bfp -> file_buf), bfp -> file_ptr) == -1)
	    return (EOF);
	bfp -> file_bp = bu_vls_addr(&(bfp -> file_buf));
	++(bfp -> file_linenm);
	if (bu_vls_strlen(&(bfp -> file_buf)) == 0)
	    continue;

	if (strip_comments)
	{
	    bfp -> file_buflen = -1;
	    for (cp = bfp -> file_bp; *cp != '\0'; ++cp)
		if (*cp == comment_char)
		{
		    bfp -> file_buflen = (bfp -> file_buf).vls_len;
		    bu_vls_trunc(&(bfp -> file_buf), cp - bfp -> file_bp);
		    break;
		}
	}
	if (cp == bfp -> file_bp)
	    return ('\n');
	bfp -> file_needline = 0;
    }

    return (*(bfp -> file_bp)++);
}

/*
 *			B U _ P R I N T F I L E
 *
 *	Diagnostic routine to print out the contents of a struct bu_file
 */
void bu_printfile (register BU_FILE *bfp)
{
    BU_CK_FILE(bfp);

    bu_log("File     '%s'...\n", bfp -> file_name);
    bu_log("  ptr      %x\n", bfp -> file_ptr);
    bu_log("  buf      '%s'\n", bu_vls_addr(&(bfp -> file_buf)));
    bu_log("  bp       %d", bfp -> file_bp - bu_vls_addr(&(bfp -> file_buf)));
    bu_log(": '%c' (%03o)\n", *(bfp -> file_bp), *(bfp -> file_bp));
    bu_log("  needline %d\n", bfp -> file_needline);
    bu_log("  linenm   %d\n", bfp -> file_linenm);
    bu_log("  comment  '%c' (%d)\n",
	bfp -> file_comment, bfp -> file_comment);
    bu_log("  buflen   %d\n", bfp -> file_buflen);
}

/*
 *			B U _ F I L E _ E R R
 *
 *	Print out a syntax error message about a BU_FILE
 */
void bu_file_err (register BU_FILE *bfp, register char *text1, register char *text2, register int cursor_pos)
{
    char		*cp;
    int			buflen;
    int			i;
    int			stripped_length;

    BU_CK_FILE(bfp);

    /*
     *	Show any trailing comments
     */
    if ((buflen = bfp -> file_buflen) > -1)
    {
	stripped_length = (bfp -> file_buf).vls_len;
	*(bu_vls_addr(&(bfp -> file_buf)) + stripped_length) =
	    bfp -> file_comment;
	(bfp -> file_buf).vls_len = buflen;
    }
    else
	stripped_length = -1;

    /*
     *	Print out the first line of the error message
     */
    if (text1 && (*text1 != '\0'))
	bu_log("%s: ", text1);
    bu_log("Error: file %s, line %d: %s\n",
	bfp -> file_name, bfp -> file_linenm, text2);
    bu_log("%s\n", bu_vls_addr(&(bfp -> file_buf)));

    /*
     *	Print out position-indicating arrow, if requested
     */
    if ((cursor_pos >= 0)
     && (cursor_pos < bu_vls_strlen(&(bfp -> file_buf))))
    {
	cp = bu_vls_addr(&(bfp -> file_buf));
	for (i = 0; i < cursor_pos; ++i)
	    if (*cp++ == '\t')
		bu_log("\t");
	    else
		bu_log("-");
	bu_log("^\n");
    }

    /*
     *	Hide the comments again
     */
    if (stripped_length > -1)
	bu_vls_trunc(&(bfp -> file_buf), stripped_length);
}

/*
 * ******************** Hack
 */




bu_rb_tree		*assignment;	/* Remapping assignment */
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
 *	       bu_rb_tree		        | |				*
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
struct curr_id *mk_curr_id (int region_id)
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
void print_curr_id (void *v, int depth)
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
void print_nonempty_curr_id (void *v, int depth)
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
void free_curr_id (struct curr_id *cip)
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
struct curr_id *lookup_curr_id(int region_id)
{
    int			rc;	/* Return code from bu_rb_insert() */
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
    switch (rc = bu_rb_insert(assignment, (void *) qcip))
    {
	case -1:
	    cip = (struct curr_id *) bu_rb_curr1(assignment);
	    free_curr_id(qcip);
	    break;
	case 0:
	    cip = qcip;
	    break;
	default:
	    bu_log("bu_rb_insert() returns %d:  This should not happen\n", rc);
	    exit (1);
    }

    return (cip);
}

/*
 *		M K _ R E M A P _ R E G ( )
 *
 */
struct remap_reg *mk_remap_reg (char *region_name)
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
void free_remap_reg (struct remap_reg *rp)
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
int compare_curr_ids (void *v1, void *v2)
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
int read_int (BU_FILE *sfp, int *ch, int *n)



   	   		/* The result */

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
	(int)(    (sfp -> file_bp) - bu_vls_addr(&(sfp -> file_buf)) - 1));
    return (-1);
}

/*
 *			  R E A D _ B L O C K ( )
 */
int read_block (BU_FILE *sfp, int *ch, int *n1, int *n2)
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
	(int)(	(sfp -> file_bp) - bu_vls_addr(&(sfp -> file_buf)) - 1) );
	    return (-1);
    }
}

/*
 *			  R E A D _ S P E C ( )
 */
int read_spec (BU_FILE *sfp, char *sf_name)
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
			(int)(    (sfp -> file_bp) - bu_vls_addr(&(sfp -> file_buf))
			    - 1) );
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
			(int)((sfp -> file_bp) - bu_vls_addr(&(sfp -> file_buf))
			- 1) );
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

void record_region (char *region_name, int region_id, struct directory *dp, struct rt_db_internal *ip)
{
    struct curr_id	*cip;
    struct remap_reg	*rp;

    cip = lookup_curr_id(region_id);
    rp = mk_remap_reg(region_name);
    rp -> rr_dp = dp;
    rp -> rr_ip = ip;
    BU_LIST_INSERT(&(cip -> ci_regions), &(rp -> l));
}

void db_init(char *db_name)
{
    struct directory		*dp;
    struct rt_comb_internal	*comb;
    struct rt_db_internal	*ip;

    if ((dbip = db_open(db_name, "r+w")) == DBI_NULL)
    {
	bu_log("Cannot open database file '%s'\n", db_name);
	exit (1);
    }
    db_dirbuild(dbip);

    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if (!(dp -> d_flags & DIR_REGION))
	    continue;
	ip = (struct rt_db_internal *) bu_malloc(sizeof(struct rt_db_internal), "rt_db_internal");
	if (rt_db_get_internal(ip, dp, dbip, (fastf_t *) NULL, &rt_uniresource) < 0) {
	    bu_log("remapid: rt_db_get_internal(%s) failed.  ",
		   dp -> d_namep);
	    bu_log("This shouldn't happen\n");
	    exit (1);
	}
	comb = (struct rt_comb_internal *) (ip -> idb_ptr);
	RT_CK_COMB(comb);
	record_region(dp -> d_namep, comb -> region_id, dp, ip);
    } FOR_ALL_DIRECTORY_END;
}

/*
 *		W R I T E _ A S S I G N M E N T ( )
 *
 */
void write_assignment (void *v, int depth)
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
	    if (rt_db_put_internal(rp -> rr_dp, dbip, rp -> rr_ip, &rt_uniresource) < 0)
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
tankill_reassign(char *db_name)
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

	/* make a 'curr_id' structure to feed to bu_rb_search */
	cip = mk_curr_id( 0 );

	/* filter TANKILL model, changing ids as we go */
	while( fscanf( fd_in, "%d %d %d", &vertex_count, &id, &surr_code ) != EOF )
	{
		int coord_no=0;
		int in_space=1;
		int ch;

		cip->ci_id = id;
		id_map = (struct curr_id *)bu_rb_search( assignment, 0, (void *)cip );
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

    bu_log("Usage: 'remapid [-{g|t}] {file.g|file.tankill} [spec_file]'\n\
	%sNote: The '-g' option modifies file.g in place\n\
	%sthe '-t' option writes a modified file.tankill to stdout\n",
	"  ", "        ");
}

/*
 *                                M A I N ( )
 */
int
main (int argc, char **argv)
{
    char		*db_name;	/* Name of database */
    char		*sf_name = NULL;	/* Name of spec file */
    BU_FILE		*sfp = NULL;	/* Spec file */
    int			ch;		/* Command-line character */
    int			tankill = 0;	/* TANKILL format (vs. BRL-CAD)? */

    extern int	bu_optind;			/* index from bu_getopt(3C) */

    bu_stdin->file_ptr = stdin;		/* LINUX-required init */

    while ((ch = bu_getopt(argc, argv, OPT_STRING)) != EOF)
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

    switch (argc - bu_optind)
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

	rt_init_resource( &rt_uniresource, 0, NULL );

    /*
     *	Open database and specification file, as necessary
     */
    db_name = argv[bu_optind++];
    if (sfp == NULL)
	sf_name = argv[bu_optind];

    /*
     *	Initialize the assignment
     */
    assignment = bu_rb_create1("Remapping assignment", compare_curr_ids);
    bu_rb_uniq_on1(assignment);

    /*
     *	Read in the specification for the reassignment
     */
    read_spec (sfp, sf_name);

    /*
     *	Make the specified reassignment
     */
    if( tankill )
	tankill_reassign( db_name );
    else
    {
	db_init(db_name);

	if (debug)
	    bu_rb_walk1(assignment, print_nonempty_curr_id, INORDER);
	else
	    bu_rb_walk1(assignment, write_assignment, INORDER);
    }
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
