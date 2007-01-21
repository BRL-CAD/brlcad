/*                     E X A M P L E _ G E O M . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
 *
 */
/** @file example_geom.c
 *	@brief An example of how to traverse a BRL-CAD database heirarchy.
 *
 *	This program uses the BRL-CAD librt function db_walk_tree() to traverse a
 *	user-specified portion of the Directed acyclic graph of the database.  This
 *	function allows for fast and easy database parsers to be developed
 *
 *	@param -h print help
 *	@param database_file The name of the geometry file to be processed
 *	@param objects_within_database A list of object names to be processed (tree tops)
 */

#include <stdio.h>
#include <string.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "bu.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/debug.h"

/* declarations to support use of getopt() system call */

/** list of legal command line options for use with getopt()  */
char *options = "hd:";
extern char *optarg;
extern int optind, opterr, getopt();

/** string that indicates what this program was invoked as */
char *progname = "(noname)";

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
 *	@param s an pointer to a null-terminated character string
 *	@return never returns
 */
void usage(char *s)
{
    if (s) (void)fputs(s, stderr);

    (void) fprintf(stderr, "Usage: %s [ -%s ] [<] infile [> outfile]\n",
		   progname, options);
    exit(1);
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

    if (  ! (progname=strrchr(*av, '/'))  )
	progname = *av;
    else
	++progname;

    /* Turn off getopt's error messages */
    opterr = 0;

    /* get all the option flags from the command line */
    while ((c=getopt(ac,av,options)) != EOF)
	switch (c) {
	case 'd'	: debug = strtol(optarg, NULL, 16); break;
	case '?'	:
	case 'h'	:
	default		: usage("Bad or help flag specified\n"); break;
	}

    return(optind);
}


/**
 *	R E G I O N _ S T A R T
 *
 * @brief This routine is called when a region is first encountered in the
 * heirarchy when processing a tree
 *
 *	@param tsp tree state (for parsing the tree)
 *	@param pathp A listing of all the nodes traversed to get to this node in the database
 *	@param combp the combination record for this region
 *	@param client_data pointer that was passed as last argument to db_walk_tree()
 *
 */
int
region_start (struct db_tree_state * tsp,
	      struct db_full_path * pathp,
	      const struct rt_comb_internal * combp,
	      genptr_t client_data )
{
    int i;
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
 *	@param tsp
 *	@param pathp
 *	@param curtree
 *	@param client_data
 *
 *	@return TREE_NULL if data in curtree was "stolen", otherwise db_walk_tree will
 *	clean up the dta in the union tree * that is returned
 *
 * If it wants to retain the data in curtree it can by returning TREE_NULL.  Otherwise
 * db_walk_tree will clean up the data in the union tree * that is returned.
 *
 */
union tree *
region_end (struct db_tree_state * tsp,
	    struct db_full_path * pathp,
	    union tree * curtree,
	    genptr_t client_data )
{
    int i;
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
leaf_func (struct db_tree_state * tsp,
	   struct db_full_path * pathp,
	   struct rt_db_internal * ip,
	   genptr_t client_data )
{
    int i;
    union tree *tp;

    if (debug&DEBUG_NAMES) {
	char *name = db_path_to_string(pathp);
	bu_log("leaf_func    %s\n", name);
	bu_free(name, "region_end name");
    }


    /* here we do primitive type specific processing */
    switch (ip->idb_minor_type) {
    case ID_BOT:
	{
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

    /** @struct rt_db_internal
     * this structure is used to manage the payload of an
     * "internal" or "in memory" representation of geometry
     * (as opposed to the "on-disk" version, which can be different)
     */
    struct rt_db_internal intern;

    /* This is the data payload for a "Bag of Triangles" or "BOT" primitive.
     * see rtgeom.h for more information about primitive solid specific data
     * structures.
     */
    struct rt_bot_internal *bot;

    /* This structure contains information about an object in the on-disk
     * database file.  Content is things such as the name, what type of object, etc.
     * see "raytrace.h" for more information
     */
    struct directory *dp;

    struct db_tree_state init_state; /* state table for the heirarchy walker */
    char idbuf[132];		/* Database title */
    int arg_count;

    /** @struct user_data
     * This is an example structure.
     * It contains anything you want to have available in the region/leaf processing routines
     */
    struct user_data {
	int stuff;
    } user_data;


    arg_count = parse_args(ac, av);

    if ( (ac - arg_count) < 1) {
	fprintf(stderr, "usage: %s geom.g [file.dxf] [bot1 bot2 bot3...]\n", progname);
	exit(-1);
    }

    /*
     *  Build an index of what's in the databse.
     *  rt_dirbuild() returns an "instance" pointer which describes
     *  the database.  It also gives you back the
     *  title string in the header (ID) record.
     */
    rtip=rt_dirbuild(av[arg_count], idbuf, sizeof(idbuf));
    if ( rtip == RTI_NULL) {
	fprintf(stderr,"rtexample: rt_dirbuild failure\n");
	exit(2);
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
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
