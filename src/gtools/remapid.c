/*                       R E M A P I D . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file remapid.c
 *
 * Perform batch modifications of region IDs for BRL-CAD (or TANKILL)
 * geometry
 *
 * The program reads a .g (or TANKILL) file and a spec file indicating
 * which region IDs to change to which new values.  For a .g file, the
 * specified changes are made to that file; For a TANKILL file, a
 * modified model is written to stdout.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "bn.h"
#include "db.h"
#include "vmath.h"
#include "raytrace.h"


/*
 * General I/O for ASCII files: remapid_file support
 */
struct remapid_file {
    long file_magic;
    FILE *file_ptr;	/* the actual file */
    char *file_name;
    struct bu_vls file_buf;	/* contents of current line */
    char *file_bp;	/* pointer into current line */
    int file_needline;	/* time to grab another line? */
    int file_linenm;
    int file_comment;	/* the comment character */
    int file_buflen;	/* length of intact buffer */
};
typedef struct remapid_file REMAPID_FILE;
#define REMAPID_FILE_MAGIC 0x6275666c
#define BU_CK_FILE(_fp) BU_CKMAG(_fp, REMAPID_FILE_MAGIC, "remapid_file")

#define bu_stdin (&bu_iob[0])
extern REMAPID_FILE bu_iob[1];
#define REMAPID_FILE_NO_COMMENT -1
#define COMMA ','


/*
 * XXX - The following initialization of bu_stdin is essentially
 * an inline version of remapid_fopen() and bu_vls_init().  As
 * such, it depends heavily on the definitions of struct
 * remapid_file and struct bu_vls in ../h/bu.h
 */
char dmy_eos = '\0';
REMAPID_FILE bu_iob[1] = {
    {
	REMAPID_FILE_MAGIC,
	NULL,
	"stdin",
	{
	    BU_VLS_MAGIC, (char *) 0, 0, 0, 0
	},
	&dmy_eos, 1, 0, '#', -1
    }
};


REMAPID_FILE *
remapid_fopen(char *fname, char *type)
{
    REMAPID_FILE *bfp;
    FILE *fp;

    if ((fp = fopen(fname, type)) == NULL)
	return NULL;

    bfp = (REMAPID_FILE *) bu_malloc(sizeof(REMAPID_FILE), "remapid_file struct");

    bfp->file_magic = REMAPID_FILE_MAGIC;
    bfp->file_ptr = fp;
    bfp->file_name = fname;
    bu_vls_init(&(bfp->file_buf));
    bfp->file_bp = bu_vls_addr(&(bfp->file_buf));
    bfp->file_needline = 1;
    bfp->file_linenm = 0;
    bfp->file_comment = '#';
    bfp->file_buflen = -1;

    return bfp;
}


/*
 * Close the file and free the associated memory
 */
int
remapid_fclose(REMAPID_FILE *bfp)
{
    int close_status;

    BU_CK_FILE(bfp);

    close_status = fclose(bfp->file_ptr);

    if (bfp != bu_stdin) {
	bfp->file_magic = 0;
	bfp->file_ptr = NULL;
	bfp->file_name = (char *) 0;
	bfp->file_bp = (char *) 0;
	bu_vls_free(&(bfp->file_buf));
	bu_free((genptr_t) bfp, "remapid_file struct");
    }
    return close_status;
}


int
remapid_fgetc(REMAPID_FILE *bfp)
{
    char *cp = (char *)NULL;
    int comment_char;	/* The comment character */
    int strip_comments;	/* Should I strip comments? */

    BU_CK_FILE(bfp);

    strip_comments = isprint(comment_char = bfp->file_comment);

    /*
     * If buffer is empty, note that it's time for a new line of input
     */
    if ((*(bfp->file_bp) == '\0') && ! (bfp->file_needline)) {
	bfp->file_needline = 1;
	return '\n';
    }

    /*
     * If it's time to grab a line of input from the file, do so.
     */
    while (bfp->file_needline) {
	bu_vls_trunc(&(bfp->file_buf), 0);
	if (bu_vls_gets(&(bfp->file_buf), bfp->file_ptr) == -1)
	    return EOF;
	bfp->file_bp = bu_vls_addr(&(bfp->file_buf));
	++(bfp->file_linenm);
	if (bu_vls_strlen(&(bfp->file_buf)) == 0)
	    continue;

	if (strip_comments) {
	    bfp->file_buflen = -1;
	    for (cp = bfp->file_bp; *cp != '\0'; ++cp)
		if (*cp == comment_char) {
		    bfp->file_buflen = (bfp->file_buf).vls_len;
		    bu_vls_trunc(&(bfp->file_buf), cp - bfp->file_bp);
		    break;
		}
	}
	if (cp == bfp->file_bp)
	    return '\n';
	bfp->file_needline = 0;
    }

    return *(bfp->file_bp)++;
}


/*
 * Print out a syntax error message about a REMAPID_FILE
 */
void
remapid_file_err(REMAPID_FILE *bfp, char *text1, char *text2, ssize_t cursor_pos)
{
    char *cp;
    int buflen;
    int i;
    int stripped_length;

    BU_CK_FILE(bfp);

    /*
     * Show any trailing comments
     */
    if ((buflen = bfp->file_buflen) > -1) {
	stripped_length = (bfp->file_buf).vls_len;
	*(bu_vls_addr(&(bfp->file_buf)) + stripped_length) =
	    bfp->file_comment;
	(bfp->file_buf).vls_len = buflen;
    } else
	stripped_length = -1;

    /*
     * Print out the first line of the error message
     */
    if (text1 && (*text1 != '\0'))
	bu_log("%s: ", text1);
    bu_log("Error: file %s, line %d: %s\n",
	   bfp->file_name, bfp->file_linenm, text2);
    bu_log("%s\n", bu_vls_addr(&(bfp->file_buf)));

    /*
     * Print out position-indicating arrow, if requested
     */
    if ((cursor_pos >= 0)
	&& ((size_t)cursor_pos < bu_vls_strlen(&(bfp->file_buf))))
    {
	cp = bu_vls_addr(&(bfp->file_buf));
	for (i = 0; i < cursor_pos; ++i)
	    if (*cp++ == '\t')
		bu_log("\t");
	    else
		bu_log("-");
	bu_log("^\n");
    }

    /*
     * Hide the comments again
     */
    if (stripped_length > -1)
	bu_vls_trunc(&(bfp->file_buf), stripped_length);
}


/*
 * ******************** Hack
 */


struct bu_rb_tree *assignment;	/* Remapping assignment */
struct db_i *dbip;		/* Instance of BRL-CAD database */


/************************************************************************
 *									*
 *			The Algorithm					*
 *									*
 *									*
 *  The remapping assignment is read from a specification file		*
 *  containing commands, the grammar for which looks something like:	*
 *									*
 *	 command --> id_list ':' id					*
 *	 id_list --> id_block | id_block ', ' id_list			*
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
 *	struct bu_rb_tree		| |				*
 *				        +-+				*
 *				       /   \				*
 *				   +-+/     \+-+			*
 *				   | |       | |			*
 *				   +-+       +-+			*
 *				   / \       / \			*
 *				 +-+ +-+   +-+ +-+			*
 *	curr_id structures	 | | | |   | | | |			*
 *	  			 +-+ +-+   +-+ +-+			*
 *				  |					*
 *				  | ...					*
 *				  |					*
 *	 			+---+   +---+   +---+   +---+		*
 *	remap_reg structures	|   |-->|   |-->|   |-->|   |--+	*
 *				+---+   +---+   +---+   +---+  |	*
 *				  ^                            |	*
 *				  |                            |	*
 *				  +----------------------------+	*
 *									*
 ************************************************************************/

struct curr_id
{
    struct bu_list l;
    int ci_id;		/* The region ID */
    struct bu_list ci_regions;	/* Regions now holding this ID */
    int ci_newid;	/* Replacement ID for the regions */
};
#define ci_magic l.magic
#define CURR_ID_NULL ((struct curr_id *) 0)
#define CURR_ID_MAGIC 0x63726964

struct remap_reg
{
    struct bu_list l;
    char *rr_name;
    struct directory *rr_dp;
    struct rt_db_internal *rr_ip;
};
#define rr_magic l.magic
#define REMAP_REG_NULL ((struct remap_reg *) 0)
#define REMAP_REG_MAGIC 0x726d7267

static int debug = 0;

/************************************************************************
 *									*
 *	  Helper routines for manipulating the data structures		*
 *									*
 ************************************************************************/

/*
 * M K _ C U R R _ I D
 *
 */
struct curr_id *
mk_curr_id(int region_id)
{
    struct curr_id *cip;

    cip = (struct curr_id *) bu_malloc(sizeof(struct curr_id), "curr_id");

    cip->ci_magic = CURR_ID_MAGIC;
    cip->ci_id = region_id;
    BU_LIST_INIT(&(cip->ci_regions));
    cip->ci_newid = region_id;

    return cip;
}


/*
 * P R I N T _ C U R R _ I D
 *
 */
void
print_curr_id(void *v, int UNUSED(depth))
{
    struct curr_id *cip = (struct curr_id *) v;
    struct remap_reg *rp;

    BU_CKMAG(cip, CURR_ID_MAGIC, "curr_id");

    bu_log(" curr_id <x%x> %d %d...\n",
	   cip, cip->ci_id, cip->ci_newid);
    for (BU_LIST_FOR(rp, remap_reg, &(cip->ci_regions))) {
	BU_CKMAG(rp, REMAP_REG_MAGIC, "remap_reg");

	bu_log("  %s\n", rp->rr_name);
    }
}


/*
 * P R I N T _ N O N E M P T Y _ C U R R _ I D
 *
 */
void
print_nonempty_curr_id(void *v, int UNUSED(depth))
{
    struct curr_id *cip = (struct curr_id *) v;
    struct remap_reg *rp;

    BU_CKMAG(cip, CURR_ID_MAGIC, "curr_id");

    if (BU_LIST_NON_EMPTY(&(cip->ci_regions))) {
	bu_log(" curr_id <x%x> %d %d...\n",
	       cip, cip->ci_id, cip->ci_newid);
	for (BU_LIST_FOR(rp, remap_reg, &(cip->ci_regions))) {
	    BU_CKMAG(rp, REMAP_REG_MAGIC, "remap_reg");

	    bu_log("  %s\n", rp->rr_name);
	}
    }
}


/*
 * F R E E _ C U R R _ I D
 *
 */
void
free_curr_id(struct curr_id *cip)
{
    BU_CKMAG(cip, CURR_ID_MAGIC, "curr_id");
    bu_free((genptr_t) cip, "curr_id");
}


/*
 * L O O K U P _ C U R R _ I D
 *
 * Scrounge for a particular region in the red-black tree.
 * If it's not found there, add it to the tree.  In either
 * event, return a pointer to it.
 */
struct curr_id *
lookup_curr_id(int region_id)
{
    int rc;	/* Return code from bu_rb_insert() */
    struct curr_id *qcip = NULL;/* The query */
    struct curr_id *cip = NULL;	/* Value to return */

    /*
     * Prepare the query
     */
    qcip = mk_curr_id(region_id);

    /*
     * Perform the query by attempting an insertion...
     * If the query succeeds (i.e., the insertion fails!),
     * then we have our curr_id.
     * Otherwise, we must create a new curr_id.
     */
    switch (rc = bu_rb_insert(assignment, (void *) qcip)) {
	case -1:
	    cip = (struct curr_id *) bu_rb_curr1(assignment);
	    free_curr_id(qcip);
	    break;
	case 0:
	    cip = qcip;
	    break;
	default:
	    bu_exit (1, "bu_rb_insert() returns %d:  This should not happen\n", rc);
    }

    return cip;
}


/*
 * M K _ R E M A P _ R E G
 *
 */
struct remap_reg *
mk_remap_reg(char *region_name)
{
    struct remap_reg *rp;

    rp = (struct remap_reg *) bu_malloc(sizeof(struct remap_reg), "remap_reg");

    rp->rr_magic = REMAP_REG_MAGIC;

    rp->rr_name = (char *) bu_malloc(strlen(region_name)+1, "region name");
    bu_strlcpy(rp->rr_name, region_name, strlen(region_name)+1);

    rp->rr_dp = RT_DIR_NULL;
    rp->rr_ip = (struct rt_db_internal *) 0;

    return rp;
}


/*
 * F R E E _ R E M A P _ R E G
 *
 */
void
free_remap_reg(struct remap_reg *rp)
{
    BU_CKMAG(rp, REMAP_REG_MAGIC, "remap_reg");
    bu_free((genptr_t) rp->rr_name, "region name");
    bu_free((genptr_t) rp->rr_ip, "rt_db_internal");
    bu_free((genptr_t) rp, "remap_reg");
}


/************************************************************************
 *									*
 *	  	The comparison callback for libredblack(3)		*
 *									*
 ************************************************************************/

/*
 * C O M P A R E _ C U R R _ I D S
 */
int
compare_curr_ids(void *v1, void *v2)
{
    struct curr_id *id1 = (struct curr_id *) v1;
    struct curr_id *id2 = (struct curr_id *) v2;

    BU_CKMAG(id1, CURR_ID_MAGIC, "curr_id");
    BU_CKMAG(id2, CURR_ID_MAGIC, "curr_id");

    return id1->ci_id  -  id2->ci_id;
}


/************************************************************************
 *									*
 *	  Routines for reading the specification file			*
 *									*
 ************************************************************************/

/*
 * R E A D _ I N T
 *
 * ch is the result
 */
int
read_int(REMAPID_FILE *sfp, int *ch, int *n)
{
    int got_digit = 0;	/* Did we actually succeed in reading a number? */
    int result;

    BU_CK_FILE(sfp);

    while (isspace(*ch))
	*ch = remapid_fgetc(sfp);

    for (result = 0; isdigit(*ch); *ch = remapid_fgetc(sfp)) {
	got_digit = 1;
	result *= 10;
	result += *ch - '0';
    }

    if (got_digit) {
	*n = result;
	return 1;
    } else if (*ch == EOF)
	remapid_file_err(sfp, "remapid",
			 "Encountered EOF while expecting an integer", -1);
    else
	remapid_file_err(sfp, "remapid:read_int()", "Encountered nondigit",
			 (int)((sfp->file_bp) - bu_vls_addr(&(sfp->file_buf)) - 1));
    return -1;
}


/*
 * R E A D _ B L O C K
 */
int
read_block(REMAPID_FILE *sfp, int *ch, int *n1, int *n2)
{
    BU_CK_FILE(sfp);

    if (read_int(sfp, ch, n1) != 1)
	return -1;

    while (isspace(*ch))
	*ch = remapid_fgetc(sfp);
    switch (*ch) {
	case COMMA:
	case ':':
	    return 1;
	case '-':
	    *ch = remapid_fgetc(sfp);
	    if (read_int(sfp, ch, n2) != 1)
		return -1;
	    else
		return 2;
	default:
	    remapid_file_err(sfp, "remapid:read_block()", "Syntax error",
			     (int)((sfp->file_bp) - bu_vls_addr(&(sfp->file_buf)) - 1));
	    return -1;
    }
}


/*
 * R E A D _ S P E C
 */
int
read_spec(REMAPID_FILE *sfp, char *sf_name)
{
    int ch;
    int i;
    int num1, num2;
    int newid = 0;
    struct bu_list cids;
    struct curr_id *cip;

    if ((sfp == NULL) && ((sfp = remapid_fopen(sf_name, "r")) == NULL))
	bu_exit (1, "Cannot open specification file '%s'\n", sf_name);
    BU_CK_FILE(sfp);

    BU_LIST_INIT(&cids);

    for (;;) {
	/*
	 *  Read in guy(s) to be assigned a particular new regionid
	 */
	for (;;) {
	    while (isspace(ch = remapid_fgetc(sfp)))
		;
	    if (ch == EOF)
		return 1;
	    switch (read_block(sfp, &ch, &num1, &num2)) {
		case 1:
		    cip = lookup_curr_id(num1);
		    BU_LIST_INSERT(&cids, &(cip->l));
		    break;
		case 2:
		    if (num1 >= num2) {
			remapid_file_err(sfp, "remapid:read_spec()", "Range out of order",
					 (int)((sfp->file_bp) - bu_vls_addr(&(sfp->file_buf)) - 1));
			bu_exit(-1, NULL);
		    }
		    for (i = num1; i <= num2; ++i) {
			cip = lookup_curr_id(i);
			BU_LIST_INSERT(&cids, &(cip->l));
		    }
		    break;
		default:
		    return -1;
	    }
	    while (isspace(ch))
		ch = remapid_fgetc(sfp);

	    switch (ch) {
		case COMMA:
		    continue;
		case ':':
		    ch = remapid_fgetc(sfp);
		    if (read_int(sfp, &ch, &newid) != 1)
			return -1;
		    break;
		default:
		    remapid_file_err(sfp, "remapid:read_spec()", "Syntax error",
				     (int)((sfp->file_bp) - bu_vls_addr(&(sfp->file_buf)) - 1));
		    bu_exit(-1, NULL);
	    }
	    break;
	}

	/*
	 * Tell each of these current regionids
	 * about it's going to get the new regionid value
	 */
	while (BU_LIST_WHILE(cip, curr_id, &cids)) {
	    cip->ci_newid = newid;
	    BU_LIST_DEQUEUE(&(cip->l));
	}
    }
}


/************************************************************************
 *									*
 * Routines for procesing the geometry databases			*
 *									*
 ************************************************************************/

void
record_region(char *region_name, int region_id, struct directory *dp, struct rt_db_internal *ip)
{
    struct curr_id *cip;
    struct remap_reg *rp;

    cip = lookup_curr_id(region_id);
    rp = mk_remap_reg(region_name);
    rp->rr_dp = dp;
    rp->rr_ip = ip;
    BU_LIST_INSERT(&(cip->ci_regions), &(rp->l));
}


void
db_init(char *db_name)
{
    struct directory *dp;
    struct rt_comb_internal *comb;
    struct rt_db_internal *ip;

    if ((dbip = db_open(db_name, "r+w")) == DBI_NULL)
	bu_exit(1, "Cannot open database file '%s'\n", db_name);
    db_dirbuild(dbip);

    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if (!(dp->d_flags & RT_DIR_REGION))
	    continue;
	ip = (struct rt_db_internal *) bu_malloc(sizeof(struct rt_db_internal), "rt_db_internal");
	if (rt_db_get_internal(ip, dp, dbip, (fastf_t *) NULL, &rt_uniresource) < 0) {
	    bu_log("remapid: rt_db_get_internal(%s) failed.  ",
		   dp->d_namep);
	    bu_exit(1, "This shouldn't happen\n");
	}
	comb = (struct rt_comb_internal *) (ip->idb_ptr);
	RT_CK_COMB(comb);
	record_region(dp->d_namep, comb->region_id, dp, ip);
    } FOR_ALL_DIRECTORY_END;
}


/*
 * W R I T E _ A S S I G N M E N T
 *
 */
void
write_assignment(void *v, int UNUSED(depth))
{
    int region_id;
    struct curr_id *cip = (struct curr_id *) v;
    struct remap_reg *rp;
    struct rt_comb_internal *comb;

    BU_CKMAG(cip, CURR_ID_MAGIC, "curr_id");

    if (BU_LIST_NON_EMPTY(&(cip->ci_regions))) {
	region_id = cip->ci_newid;
	for (BU_LIST_FOR(rp, remap_reg, &(cip->ci_regions))) {
	    BU_CKMAG(rp, REMAP_REG_MAGIC, "remap_reg");
	    RT_CK_DB_INTERNAL(rp->rr_ip);

	    comb = (struct rt_comb_internal *) rp->rr_ip->idb_ptr;
	    RT_CK_COMB(comb);
	    comb->region_id = region_id;
	    if (rt_db_put_internal(rp->rr_dp, dbip, rp->rr_ip, &rt_uniresource) < 0) {
		bu_log("remapid: rt_db_put_internal(%s) failed.  ",
		       rp->rr_dp->d_namep);
		bu_exit(1, "This shouldn't happen\n");
	    }
	}
    }
}


HIDDEN void
tankill_reassign(char *db_name)
{
    FILE *fd_in;
    int vertex_count, id, surr_code;
    struct curr_id *id_map, *cip;

    /* open TANKILL model */
    if ((fd_in=fopen(db_name, "r")) == NULL) {
	bu_log("Cannot open TANKILL database (%s)\n", db_name);
	perror("remapid");
	bu_exit(EXIT_FAILURE, "Cannot open TANKILL database\n");
    }

    /* make a 'curr_id' structure to feed to bu_rb_search */
    cip = mk_curr_id(0);

    /* filter TANKILL model, changing ids as we go */
    while (fscanf(fd_in, "%d %d %d", &vertex_count, &id, &surr_code) != EOF) {
	int coord_no=0;
	int in_space=1;
	int ch;

	cip->ci_id = id;
	id_map = (struct curr_id *)bu_rb_search(assignment, 0, (void *)cip);
	if (!id_map)
	    printf("%d %d %d", vertex_count, id, surr_code);
	else
	    printf("%d %d %d", vertex_count, id_map->ci_newid, surr_code);

	/* just copy the rest of the component */
	while (coord_no < 3*vertex_count || !in_space) {
	    ch = fgetc(fd_in);
	    if (ch == EOF && coord_no < 3*vertex_count) {
		bu_log("Unexpected EOF while processing ident %d\n", id);
		bu_exit(EXIT_FAILURE, "Unexpected EOF\n");
	    }

	    if (isspace(ch))
		in_space = 1;
	    else if (in_space) {
		in_space = 0;
		coord_no++;
	    }
	    putchar(ch);
	}
    }
}


/************************************************************************
 *									*
 *	  	And finally... the main program				*
 *									*
 ************************************************************************/

/*
 * P R I N T _ U S A G E
 */
void
print_usage(void)
{
    bu_exit(1, "Usage: 'remapid [-{g|t}] {file.g|file.tankill} [spec_file]'\n\
  Note: The '-g' option modifies file.g in place\n\
        the '-t' option writes a modified file.tankill to stdout\n");
}


/*
 * M A I N
 */
int
main(int argc, char **argv)
{
    char *db_name;	/* Name of database */
    char *sf_name = NULL;	/* Name of spec file */
    REMAPID_FILE *sfp = NULL;	/* Spec file */
    int ch;		/* Command-line character */
    int tankill = 0;	/* TANKILL format (vs. BRL-CAD)? */

    bu_stdin->file_ptr = stdin;		/* LINUX-required init */

    while ((ch = bu_getopt(argc, argv, "gt?")) != -1)
	switch (ch) {
	    case 'g':
		tankill = 0;
		break;
	    case 't':
		tankill = 1;
		break;
	    case '?':
	    default:
		print_usage();
	}

    switch (argc - bu_optind) {
	case 1:
	    sf_name = "stdin";
	    sfp = bu_stdin;
	    /* Break intentionally missing */
	case 2:
	    break;
	default:
	    print_usage();
    }

    rt_init_resource(&rt_uniresource, 0, NULL);

    /*
     * Open database and specification file, as necessary
     */
    db_name = argv[bu_optind++];
    if (sfp == NULL)
	sf_name = argv[bu_optind];

    /*
     * Initialize the assignment
     */
    assignment = bu_rb_create1("Remapping assignment", compare_curr_ids);
    bu_rb_uniq_on1(assignment);

    /*
     * Read in the specification for the reassignment
     */
    read_spec(sfp, sf_name);

    /*
     * Make the specified reassignment
     */
    if (tankill)
	tankill_reassign(db_name);
    else {
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
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
