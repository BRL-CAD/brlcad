/*                          N I R T . C
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
 */
/** @file nirt/nirt.c
 *
 * This program is Natalie's Interactive Ray-Tracer
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "raytrace.h"

/* private */
#include "./nirt.h"
#include "./usrfmt.h"
#include "brlcad_version.h"


/* bleh */
int need_prep = 1;

const com_table ComTab[] = {
    { "attr", cm_attr, "select attributes", "<-f(flush) | -p(print) | attribute_name>" },
    { "ae", az_el, "set/query azimuth and elevation", "azimuth elevation" },
    { "dir", dir_vect, "set/query direction vector", "x-component y-component z-component" },
    { "hv", grid_coor, "set/query gridplane coordinates", "horz vert [dist]" },
    { "xyz", target_coor, "set/query target coordinates", "X Y Z" },
    { "s", shoot, "shoot a ray at the target", NULL },
    { "backout", backout, "back out of model", NULL },
    { "useair", use_air, "set/query use of air", "<0|1|2|...>" },
    { "units", nirt_units, "set/query local units", "<mm|cm|m|in|ft>" },
    { "overlap_claims", do_overlap_claims, "set/query overlap rebuilding/retention", "<0|1|2|3>" },
    { "fmt", format_output, "set/query output formats", "{rhpfmog} format item item ..." },
    { "dest", direct_output, "set/query output destination", "file/pipe" },
    { "statefile", state_file, "set/query name of state file", "file" },
    { "dump", dump_state, "write current state of NIRT to the state file", NULL },
    { "load", load_state, "read new state for NIRT from the state file", NULL },
    { "print", print_item, "query an output item", "item" },
    { "bot_minpieces", bot_minpieces, "Get/Set value for rt_bot_minpieces (0 means do not use pieces, default is 32)", "min_pieces" },
    { "bot_mintie", bot_mintie, "Get/Set value for rt_bot_mintie (0 means do not use pieces, default is 4294967295)", "min_tie" },
    { "libdebug", cm_libdebug, "set/query librt debug flags", "hex_flag_value" },
    { "debug", cm_debug, "set/query nirt debug flags", "hex_flag_value" },
    { "!", sh_esc, "escape to the shell", NULL },
    { "q", quit, "quit", NULL },
    { "?", show_menu, "display this help menu", NULL },
    { (char *)NULL, NULL, (char *)NULL, (char *)NULL }
};


struct script_rec
{
    struct bu_list l;
    int sr_type;	/* Direct or indirect */
    struct bu_vls sr_script;	/* Literal or file name */
};
#define SCRIPT_REC_NULL ((struct script_rec *) 0)
#define SCRIPT_REC_MAGIC 0x73637270
#define sr_magic l.magic


int do_backout = 0;			/* Backout before shooting? */
int overlap_claims = OVLP_RESOLVE;	/* Rebuild/retain overlaps? */
char *ocname[4];
int silent_flag = SILENT_UNSET;	/* Refrain from babbling? */
int nirt_debug = 0;			/* Control of diagnostics */

/* Parallel structures needed for operation w/ and w/o air */
struct rt_i *rti_tab[2];
struct resource res_tab;

struct application ap;

attr_table a_tab;

/* the name of the BRL-CAD geometry file */
char *db_name;


void printusage(void)
{
    bu_log("Usage: 'nirt [options] model.g objects...'\n");
    bu_log("Options:\n");
    bu_log(" -b         back out of geometry before first shot\n");
    bu_log(" -B n       set rt_bot_minpieces=n\n");
    bu_log(" -T n       set rt_bot_mintie=n\n");
    bu_log(" -e script  run script before interacting\n");
    bu_log(" -f sfile   run script sfile before interacting\n");
    bu_log(" -L         list output formatting options\n");
    bu_log(" -M         read matrix, cmds on stdin\n");
    bu_log(" -O action  handle overlap claims via action\n");
    bu_log(" -s         run in silent (non-verbose) mode\n ");
    bu_log(" -u n       set use_air=n (default 0)\n");
    bu_log(" -v         run in verbose mode\n");
    bu_log(" -x v       set librt(3) diagnostic flag=v\n");
    bu_log(" -X v       set nirt diagnostic flag=v\n");
}

/**
 * List formats installed in global nirt data directory
 */
void listformats(void)
{
    int files, i;
    char **filearray;
    struct bu_vls nirtfilespath, nirtpathtofile, vlsfileline;
    char suffix[5]=".nrt";
    FILE *cfPtr;
    int fnddesc;

    bu_vls_init(&vlsfileline);
    bu_vls_init(&nirtfilespath);
    bu_vls_init(&nirtpathtofile);
    bu_vls_printf(&nirtfilespath, "%s", bu_brlcad_data("nirt", 0));

    files = bu_count_path(bu_vls_addr(&nirtfilespath), suffix);

    filearray = (char **)bu_malloc(files*sizeof(char *), "filelist");

    bu_list_path(bu_vls_addr(&nirtfilespath), suffix, filearray);

    for (i = 0; i < files; i++) {
	bu_vls_trunc(&nirtpathtofile, 0);
	bu_vls_trunc(&vlsfileline, 0);
	bu_vls_printf(&nirtpathtofile, "%s/%s", bu_vls_addr(&nirtfilespath), filearray[i]);
	cfPtr = fopen(bu_vls_addr(&nirtpathtofile), "rb");
	fnddesc = 0;
	while (bu_vls_gets(&vlsfileline, cfPtr) && fnddesc == 0) {
	    if (strncmp(bu_vls_addr(&vlsfileline), "# Description: ", 15) == 0) {
		fnddesc = 1;
		bu_log("%s\n", bu_vls_addr(&vlsfileline)+15);
	    }
	    bu_vls_trunc(&vlsfileline, 0);
	}
	fclose(cfPtr);
    }

    bu_free(filearray, "filelist");
    bu_vls_free(&vlsfileline);
    bu_vls_free(&nirtfilespath);
    bu_vls_free(&nirtpathtofile);
}

void
attrib_print(void)
{
    int i;

    for (i=0; i < a_tab.attrib_use; i++) {
	bu_log("\"%s\"\n", a_tab.attrib[i]);
    }
}


/**
 * flush the list of desired attributes
 */
void
attrib_flush(void)
{
    int i;

    a_tab.attrib_use = 0;
    for (i=0; i < a_tab.attrib_use; i++)
	bu_free(a_tab.attrib[i], "strdup");
}


void
attrib_add(char *a, int *prep)
{
    char *p;

    if (!a) {
	bu_log("attrib_add null arg\n");
	return; /* null char ptr */
    }

    p = strtok(a, "\t ");
    while (p) {

	/* make sure we have space */
	if (!a_tab.attrib || a_tab.attrib_use >= (a_tab.attrib_cnt-1)) {
	    a_tab.attrib_cnt += 16;
	    a_tab.attrib = bu_realloc(a_tab.attrib,
				      a_tab.attrib_cnt * sizeof(char *),
				      "attrib_tab");
	}

	/* add the attribute name(s) */
	a_tab.attrib[a_tab.attrib_use] = bu_strdup(p);
	/* bu_log("attrib[%d]=\"%s\"\n", attrib_use, attrib[attrib_use]); */
	a_tab.attrib[++a_tab.attrib_use] = (char *)NULL;

	p = strtok((char *)NULL, "\t ");
	*prep = 1;
    }
}


/**
 * string is a literal or a file name
 */
static void
enqueue_script(struct bu_list *qp, int type, char *string)
{
    struct script_rec *srp;
    FILE *cfPtr;
    struct bu_vls str;
    bu_vls_init(&str);

    BU_CK_LIST_HEAD(qp);

    srp = (struct script_rec *)
	bu_malloc(sizeof(struct script_rec), "script record");
    srp->sr_magic = SCRIPT_REC_MAGIC;
    srp->sr_type = type;
    bu_vls_init(&(srp->sr_script));

    /*Check if supplied file name is local or in brlcad's nirt data dir*/
    if (type == READING_FILE) {
	bu_vls_trunc(&str, 0);
	bu_vls_printf(&str, "%s", string);
	cfPtr = fopen(bu_vls_addr(&str), "rb");
	if (cfPtr == NULL) {
	    bu_vls_trunc(&str, 0);
	    bu_vls_printf(&str, "%s/%s.nrt", bu_brlcad_data("nirt", 0), string);
	    cfPtr = fopen(bu_vls_addr(&str), "rb");
	    if (cfPtr != NULL) {
		fclose(cfPtr);
	    } else {
		bu_vls_trunc(&str, 0);
		bu_vls_printf(&str, "%s", string);
	    }
	} else {
	    fclose(cfPtr);
	}
	bu_vls_printf(&(srp->sr_script), "%s", bu_vls_addr(&str));
    } else {
	bu_vls_strcat(&(srp->sr_script), string);
    }
    BU_LIST_INSERT(qp, &(srp->l));
    bu_vls_free(&str);
}


/**
 * text is for the title line
 */
static void
show_scripts(struct bu_list *sl, char *text)
{
    int i;
    struct script_rec *srp;

    BU_CK_LIST_HEAD(sl);

    i = 0;
    bu_log("- - - - - - - The command-line scripts - - - - - - -\n%s\n", text);
    for (BU_LIST_FOR(srp, script_rec, sl)) {
	BU_CKMAG(srp, SCRIPT_REC_MAGIC, "script record");

	bu_log("%d. script %s '%s'\n",
	       ++i,
	       (srp->sr_type == READING_STRING) ? "string" :
	       (srp->sr_type == READING_FILE) ? "file" : "???",
	       bu_vls_addr(&(srp->sr_script)));
    }
    bu_log("- - - - - - - - - - - - - - - - - - - - - - - - - -\n");
}


static void
free_script(struct script_rec *srp)
{
    BU_CKMAG(srp, SCRIPT_REC_MAGIC, "script record");

    bu_vls_free(&(srp->sr_script));
    bu_free((genptr_t) srp, "script record");
}


static void
run_scripts(struct bu_list *sl, struct rt_i *rtip)
{
    struct script_rec *srp;
    char *cp;
    FILE *fPtr;

    if (nirt_debug & DEBUG_SCRIPTS)
	show_scripts(sl, "before running them");

    while (BU_LIST_WHILE(srp, script_rec, sl)) {
	BU_LIST_DEQUEUE(&(srp->l));
	BU_CKMAG(srp, SCRIPT_REC_MAGIC, "script record");
	cp = bu_vls_addr(&(srp->sr_script));

	if (nirt_debug & DEBUG_SCRIPTS) {
	    bu_log("  Attempting to run %s '%s'\n",
		   (srp->sr_type == READING_STRING) ? "literal" :
		   (srp->sr_type == READING_FILE) ? "file" : "???",
		   cp);
	}

	switch (srp->sr_type) {
	    case READING_STRING:
		interact(READING_STRING, cp, rtip);
		break;
	    case READING_FILE:
		if ((fPtr = fopen(cp, "rb")) == NULL) {
		    bu_log("Cannot open script file '%s'\n", cp);
		} else {
		    interact(READING_FILE, fPtr, rtip);
		    fclose(fPtr);
		}
		break;
	    default:
		bu_exit (1, "%s:%d: script of type %d.  This shouldn't happen\n", __FILE__, __LINE__, srp->sr_type);
	}
	free_script(srp);
    }

    if (nirt_debug & DEBUG_SCRIPTS)
	show_scripts(sl, "after running them");
}


int
main(int argc, char *argv[])
{
    struct rt_i *rtip = NULL;

    char db_title[TITLE_LEN+1];/* title from MGED file */
    const char *tmp_str;
    extern char local_u_name[65];
    extern double base2local;
    extern double local2base;
    FILE *fPtr;
    int Ch;		/* Option name */
    int mat_flag = 0;	/* Read matrix from stdin? */
    int use_of_air = 0;
    char ocastring[1024] = {0};
    struct bu_list script_list;	/* For -e and -f options */
    struct script_rec *srp;
    extern outval ValTab[];

    /* from if.c, callback functions for overlap, hit, and miss shots */
    int if_overlap(struct application *, struct partition *, struct region *, struct region *, struct partition *);
    int if_hit(struct application *, struct partition *, struct seg *);
    int if_miss(struct application *);

    BU_LIST_INIT(&script_list);

    ocname[OVLP_RESOLVE] = "resolve";
    ocname[OVLP_REBUILD_FASTGEN] = "rebuild_fastgen";
    ocname[OVLP_REBUILD_ALL] = "rebuild_all";
    ocname[OVLP_RETAIN] = "retain";
    *ocastring = '\0';

    bu_optind = 1;		/* restart */

    /* Handle command-line options */
    while ((Ch = bu_getopt(argc, argv, OPT_STRING)) != -1) {
	switch (Ch) {
	    case 'A':
		attrib_add(bu_optarg, &need_prep);
		break;
	    case 'B':
		rt_bot_minpieces = atoi(bu_optarg);
		break;
	    case 'T':
		rt_bot_mintie = atoi(bu_optarg);
		break;
	    case 'b':
		do_backout = 1;
		break;
	    case 'E':
		if (nirt_debug & DEBUG_SCRIPTS)
		    show_scripts(&script_list, "before erasure");
		while (BU_LIST_WHILE(srp, script_rec, &script_list)) {
		    BU_LIST_DEQUEUE(&(srp->l));
		    free_script(srp);
		}
		if (nirt_debug & DEBUG_SCRIPTS)
		    show_scripts(&script_list, "after erasure");
		break;
	    case 'e':
		enqueue_script(&script_list, READING_STRING, bu_optarg);
		if (nirt_debug & DEBUG_SCRIPTS)
		    show_scripts(&script_list, "after enqueueing a literal");
		break;
	    case 'f':
		enqueue_script(&script_list, READING_FILE, bu_optarg);
		if (nirt_debug & DEBUG_SCRIPTS)
		    show_scripts(&script_list, "after enqueueing a file name");
		break;
	    case 'L':
		listformats();
		bu_exit(EXIT_SUCCESS, NULL);
	    case 'M':
		mat_flag = 1;
		break;
	    case 'O':
		sscanf(bu_optarg, "%1024s", ocastring);
		break;
	    case 's':
		silent_flag = SILENT_YES;	/* Positively yes */
		break;
	    case 'v':
		silent_flag = SILENT_NO;	/* Positively no */
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&rt_g.debug);
		break;
	    case 'X':
		sscanf(bu_optarg, "%x", (unsigned int *)&nirt_debug);
		break;
	    case 'u':
		if (sscanf(bu_optarg, "%d", &use_of_air) != 1) {
		    (void) fprintf(stderr,
				   "Illegal use-air specification: '%s'\n", bu_optarg);
		    return 1;
		}
		break;
	    case '?':
	    default:
		printusage();
		bu_exit (Ch != '?', NULL);
	}
    } /* end while getopt */

    if (argc - bu_optind < 2) {
	printusage();
	return 1;
    }

    if (isatty(0)) {
	if (silent_flag != SILENT_YES)
	    silent_flag = SILENT_NO;
    } else {
	/* stdin is not a TTY */
	if (silent_flag != SILENT_NO)
	    silent_flag = SILENT_YES;
    }
    if (silent_flag != SILENT_YES)
	(void) fputs(brlcad_ident("Natalie's Interactive Ray Tracer"), stdout);

    if (use_of_air && (use_of_air != 1)) {
	fprintf(stderr,
		"Warning: useair=%d specified, will set to 1\n", use_of_air);
	use_of_air = 1;
    }

    switch (*ocastring) {
	case '\0':
	    overlap_claims = OVLP_RESOLVE;
	    break;
	case '0':
	case '1':
	case '2':
	case '3':
	    if (ocastring[1] == '\0') {
		sscanf(ocastring, "%d", &overlap_claims);
	    } else {
		fprintf(stderr,
			"Illegal overlap_claims specification: '%s'\n", ocastring);
		return 1;
	    }
	    break;
	case 'r':
	    if (BU_STR_EQUAL(ocastring, "resolve"))
		overlap_claims = OVLP_RESOLVE;
	    else if (BU_STR_EQUAL(ocastring, "rebuild_fastgen"))
		overlap_claims = OVLP_REBUILD_FASTGEN;
	    else if (BU_STR_EQUAL(ocastring, "rebuild_all"))
		overlap_claims = OVLP_REBUILD_ALL;
	    else if (BU_STR_EQUAL(ocastring, "retain"))
		overlap_claims = OVLP_RETAIN;
	    else {
		fprintf(stderr,
			"Illegal overlap_claims specification: '%s'\n", ocastring);
		return 1;
	    }
	    break;
	default:
	    fprintf(stderr,
		    "Illegal overlap_claims specification: '%s'\n", ocastring);
	    return 1;
    }

    db_name = argv[bu_optind];

    /* build directory for target object */
    if (silent_flag != SILENT_YES) {
	printf("Database file:  '%s'\n", db_name);
	printf("Building the directory...");
    }
    if ((rtip = rt_dirbuild(db_name, db_title, TITLE_LEN)) == RTI_NULL) {
	fflush(stdout);
	fprintf(stderr, "Could not load file %s\n", db_name);
	return 1;
    }

    rti_tab[use_of_air] = rtip;
    rti_tab[1 - use_of_air] = RTI_NULL;
    rtip->useair = use_of_air;
    rtip->rti_save_overlaps = (overlap_claims > 0);

    ++bu_optind;
    do_rt_gettrees(rtip, argv + bu_optind, argc - bu_optind, &need_prep);

    /* Initialize the table of resource structures */
    rt_init_resource(&res_tab, 0, rtip);

    /* initialization of the application structure */
    RT_APPLICATION_INIT(&ap);
    ap.a_hit = if_hit;        /* branch to if_hit routine */
    ap.a_miss = if_miss;      /* branch to if_miss routine */
    ap.a_overlap = if_overlap;/* branch to if_overlap routine */
    ap.a_logoverlap = rt_silent_logoverlap;
    ap.a_onehit = 0;          /* continue through shotline after hit */
    ap.a_resource = &res_tab;
    ap.a_purpose = "NIRT ray";
    ap.a_rt_i = rtip;         /* rt_i pointer */
    ap.a_zero1 = 0;           /* sanity check, sayth raytrace.h */
    ap.a_zero2 = 0;           /* sanity check, sayth raytrace.h */
    ap.a_uptr = (genptr_t)a_tab.attrib;

    /* initialize variables */
    azimuth() = 0.0;
    elevation() = 0.0;
    direct(X) = -1.0;
    direct(Y) = 0.0;
    direct(Z) = 0.0;
    grid(HORZ) = 0.0;
    grid(VERT) = 0.0;
    grid(DIST) = 0.0;
    grid2targ();
    set_diameter(rtip);

    /* initialize the output specification */
    default_ospec();

    /* initialize NIRT's local units */
    base2local = rtip->rti_dbip->dbi_base2local;
    local2base = rtip->rti_dbip->dbi_local2base;
    tmp_str = bu_units_string(local2base);
    if (tmp_str) {
	bu_strlcpy(local_u_name, bu_units_string(local2base), sizeof(local_u_name));
    } else {
	strcpy(local_u_name, "Unknown units");
    }

    if (silent_flag != SILENT_YES) {
	printf("Database title: '%s'\n", db_title);
	printf("Database units: '%s'\n", local_u_name);
	printf("model_min = (%g, %g, %g)    model_max = (%g, %g, %g)\n",
	       rtip->mdl_min[X] * base2local,
	       rtip->mdl_min[Y] * base2local,
	       rtip->mdl_min[Z] * base2local,
	       rtip->mdl_max[X] * base2local,
	       rtip->mdl_max[Y] * base2local,
	       rtip->mdl_max[Z] * base2local);
    }

    /* Run the run-time configuration file, if it exists */
    if ((fPtr = fopenrc()) != NULL) {
	interact(READING_FILE, fPtr, rtip);
	fclose(fPtr);
    }

    /* Run all scripts specified on the command line */
    run_scripts(&script_list, rtip);

    /* Perform the user interface */
    if (mat_flag) {
	read_mat(rtip);
	return 0;
    } else {
	interact(READING_FILE, stdin, rtip);
    }

    return 0;
}


void
do_rt_gettrees(struct rt_i *my_rtip, char **object_name, int nm_objects, int *prep)
{
    static char **prev_names = 0;
    static int prev_nm = 0;
    int i;

    if (object_name == NULL) {
	if ((object_name = prev_names) == 0)
	    bu_exit (1, "%s:%d: This shouldn't happen\n", __FILE__, __LINE__);
	nm_objects = prev_nm;
    }

    if (prev_names == 0) {
	prev_names = object_name;
	prev_nm = nm_objects;
    }

    if (silent_flag != SILENT_YES) {
	printf("\nGet trees...");
	fflush(stdout);
    }

    i = rt_gettrees_and_attrs(my_rtip, (const char **)a_tab.attrib, nm_objects, (const char **) object_name, 1);
    if (i) {
	fflush(stdout);
	bu_exit (1, "rt_gettrees() failed\n");
    }

    if (*prep) {
	if (silent_flag != SILENT_YES) {
	    printf("\nPrepping the geometry...");
	    fflush(stdout);
	}
	rt_prep(my_rtip);
	*prep = 0;
    }

    if (silent_flag != SILENT_YES) {
	printf("\n%s", (nm_objects == 1) ? "Object" : "Objects");
	for (i = 0; i < nm_objects; ++i)
	    printf(" '%s'", object_name[i]);
	printf(" processed\n");
    }
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
