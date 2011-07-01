/*                     W A L K _ E X A M P L E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
 *
 */
/** @file conv/walk_example.c
 *
 * @brief Example of how to traverse a BRL-CAD database heirarchy
 *
 * This program uses the BRL-CAD librt function db_walk_tree() to
 * traverse a user-specified portion of the Directed Acyclic Graph
 * (DAG) of the database.  This function allows for fast and easy
 * database parsers to be developed
 *
 * @param -h print help
 * @param database_file The name of the geometry file to be processed
 * @param objects_within_database A list of object names to be processed (tree tops)
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "bu.h"
#include "raytrace.h"
#include "wdb.h"


/** list of legal command line options for use with bu_getopt()  */
char *options = "hd:";

/** flag for printing names of objects as encountered  */
#define DEBUG_NAMES 1
/** print debugging statistics flag  */
#define DEBUG_STATS 2
/** holds bit values for various debug settings  */
long debug = 0;
/** when non-zero, program prints information for the user about progress  */
int verbose = 0;

/**
 *	U S A G E
 *	@brief tell user how to invoke this program, then exit
 *      @param name the name of the running program (argv[0])
 *	@param str a pointer to a null-terminated character string
 *	@return never returns
 */
void usage(const char *name, const char *str)
{
    if (str) {
	(void)fputs(str, stderr);
    }
    bu_exit(1, "Usage: %s [ -%s ] input.g object(s) ...\n", name, options);
}


/** @if no
 *	P A R S E _ A R G S
 * @endif
 *	@brief Parse command line flags.
 *
 *	This routine handles parsing of all command line options.
 *
 *	@param ac count of arguments
 *	@param av array of pointers to null-terminated strings
 *	@return index into av of first argument past options (new ac value)
 */
int parse_args(int ac, char *av[])
{
    int  c;
    char *strrchr();
    char *tmp_basename = NULL;

    /* Turn off bu_getopt's error messages */
    bu_opterr = 0;

    /* get all the option flags from the command line */
    while ((c=bu_getopt(ac, av, options)) != -1) {
	switch (c) {
	    case 'd':
		debug = strtol(bu_optarg, NULL, 16);
		break;
	    case '?':
	    case 'h':
	    default:
                tmp_basename = bu_basename(av[0]);
		usage(tmp_basename, "Bad or help flag specified\n");
		bu_free(tmp_basename, "tmp_basename free");
		break;
	}
    }

    return bu_optind;
}


/**
 *	R E G I O N _ S T A R T
 *
 * @brief This routine is called when a region is first encountered in the
 * heirarchy when processing a tree
 *
 *	@param pathp A listing of all the nodes traversed to get to this node in the database
 */
int
region_start(struct db_tree_state *UNUSED(tsp),
	     const struct db_full_path *pathp,
	     const struct rt_comb_internal *UNUSED(combp),
	     genptr_t UNUSED(client_data))
{
    if (debug&DEBUG_NAMES) {
	char *name = db_path_to_string(pathp);
	bu_log("region_start %s\n", name);
	bu_free(name, "reg_start name");
    }
    return 0;
}


/**
 *	R E G I O N _ E N D
 *
 *
 * @brief This is called when all sub-elements of a region have been processed by leaf_func.
 *
 *	@param pathp
 *	@param curtree
 *
 *	@return TREE_NULL if data in curtree was "stolen", otherwise db_walk_tree will
 *	clean up the dta in the union tree * that is returned
 *
 * If it wants to retain the data in curtree it can by returning TREE_NULL.  Otherwise
 * db_walk_tree will clean up the data in the union tree * that is returned.
 *
 */
union tree *
region_end(struct db_tree_state *UNUSED(tsp),
	   const struct db_full_path * pathp,
	   union tree *curtree,
	   genptr_t UNUSED(client_data))
{
    if (debug&DEBUG_NAMES) {
	char *name = db_path_to_string(pathp);
	bu_log("region_end   %s\n", name);
	bu_free(name, "region_end name");
    }

    return curtree;
}


/**
 *	L E A F _ F U N C
 *
 *	@brief Function to process a leaf node.
 *
 *     	This is actually invoked from db_recurse() from db_walk_subtree().
 *
 *	@return (union tree *) representing the leaf, or
 *	TREE_NULL if leaf does not exist or has an error.
 */
union tree *
leaf_func (struct db_tree_state *UNUSED(tsp),
	   const struct db_full_path *pathp,
	   struct rt_db_internal *internp,
	   genptr_t UNUSED(client_data))
{
    /* the rt_db_internal structure is used to manage the payload of
     * "internal" or "in memory" representation of geometry as opposed
     * to different the "on-disk" serialized "external" version.
     */
    struct rt_db_internal *ip = internp; /* only set for commenting
					    purposes */

    if (debug&DEBUG_NAMES) {
	char *name = db_path_to_string(pathp);
	bu_log("leaf_func    %s\n", name);
	bu_free(name, "region_end name");
    }

    /* here we do primitive type specific processing */
    switch (ip->idb_minor_type) {
	case ID_BOT:
	    {
		/* This is the data payload for a "Bag of Triangles" or
		 * "BOT" primitive.  see rtgeom.h for more information
		 * about primitive solid specific data structures.
		 */
		struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
		RT_BOT_CK_MAGIC(bot); /* check for data corruption */

		/* code to process bot goes here */

		break;
	    }
	case ID_ARB8:
	    {
		struct rt_arb_internal *arb = (struct rt_arb_internal *)ip->idb_ptr;
		RT_ARB_CK_MAGIC(arb);

		/* code to process arb goes here */

		break;
	    }
	    /*
	     * Note:  A complete program would process each possible type of object here,
	     * not just a couple of primitive types
	     */

    }

    return (union tree *)NULL;
}


/**
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(int ac, char *av[])
{
    /** @struct rt_i
     * This structure contains some global state information for librt
     */
    struct rt_i *rtip;

    struct db_tree_state init_state; /* state table for the heirarchy walker */
    char idbuf[1024] = {0};		/* Database title */
    int arg_count;
    char *tmp_basename;

    /** @struct user_data
     * This is an example structure.
     * It contains anything you want to have available in the region/leaf processing routines
     */
    struct user_data {
	int stuff;
    } user_data;


    arg_count = parse_args(ac, av);

    if ((ac - arg_count) < 1) {
	tmp_basename = bu_basename(av[0]);
	usage(tmp_basename, "bad arugment count");
	bu_free(tmp_basename, "tmp_basename free");
    }

    /*
     *  Build an index of what's in the databse.
     *  rt_dirbuild() returns an "instance" pointer which describes
     *  the database.  It also gives you back the
     *  title string in the header (ID) record.
     */
    rtip=rt_dirbuild(av[arg_count], idbuf, sizeof(idbuf));
    if (rtip == RTI_NULL) {
	bu_exit(2, "%s: rt_dirbuild failure\n", av[0]);
    }

    arg_count++;

    init_state = rt_initial_tree_state;
    db_walk_tree(rtip->rti_dbip, /* database instance */
		 ac-arg_count,		/* number of trees to get from the database */
		 (const char **)&av[arg_count],
		 1, /* number of cpus to use */
		 &init_state,
		 region_start,
		 region_end,
		 leaf_func,
		 (genptr_t)&user_data);

    /* at this point you can do things with the geometry you have obtained */

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
