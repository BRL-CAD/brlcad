/*                         S T L - G . C
 * BRL-CAD
 *
 * Copyright (c) 2002-2025 United States Government as represented by
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
/** @file stl-g.c
 *
 * Convert Stereolithography format files to BRL-CAD .g binary format
 *
 * Note that binary STL format use a little-endian byte ordering where
 * bytes at lower addresses have lower significance.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "bnetwork.h"
#include "bio.h"

#include "bu/app.h"
#include "bu/cv.h"
#include "bu/getopt.h"
#include "bu/path.h"
#include "bu/units.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"

static struct bg_vert_tree *tree;
static struct wmember all_head;
static char *input_file;	/* name of the input file */
static char *brlcad_file;	/* name of output file */
static char *forced_name=NULL;	/* name specified on command line */
static int solid_count=0;	/* count of solids converted */
static struct bn_tol tol;	/* Tolerance structure */
static int id_no=1000;		/* Ident numbers */
static int const_id=-1;		/* Constant ident number (assigned to all regions if non-negative) */
static int mat_code=1;		/* default material code */
static int debug=0;		/* Debug flag */
static int binary=0;		/* flag indicating input is binary format */
static FILE *fd_in;		/* input file */
static struct rt_wdb *fd_out;	/* Resulting BRL-CAD file */
static float conv_factor=1.0;	/* conversion factor from model units to mm */
static int *bot_faces=NULL;	 /* array of ints (indices into tree->the_array array) three per face */
static int bot_fsize=0;		/* current size of the bot_faces array */
static int bot_fcurr=0;		/* current bot face */

/* Size of blocks of faces to malloc */
#define BOT_FBLOCK 128

#define MAX_LINE_SIZE 512


static void
usage(const char *argv0)
{
    bu_log("Usage: %s [-db] [-t tolerance] [-N forced_name] [-i initial_ident] [-I constant_ident] [-m material_code] [-c units_str] [-x rt_debug_flag] input.stl output.g\n", argv0);
    bu_log("	where input.stl is a STereoLithography file\n");
    bu_log("	and output.g is the name of a BRL-CAD database file to receive the conversion.\n");
    bu_log("	The -d option prints additional debugging information.\n");
    bu_log("	The -b option specifies that the input file is in the binary STL format (default is ASCII). \n");
    bu_log("	The -t option specifies the minimum distance between two distinct vertices (mm).\n");
    bu_log("	The -N option specifies a name to use for the object.\n");
    bu_log("	The -i option sets the initial region ident number (default is 1000).\n");
    bu_log("	The -I option sets the ident number that will be assigned to all regions (conflicts with -i).\n");
    bu_log("	The -m option sets the integer material code for all the parts (default is 1).\n");
    bu_log("	The -c option specifies the units used in the STL file (units_str may be \"in\", \"ft\", ... default is \"mm\"\n");
    bu_log("	The -x option specifies an RT debug flags (see raytrace.h).\n");
}


void
Add_face(int face[3])
{
    if (!bot_faces) {
	bot_faces = (int *)bu_malloc(3 * BOT_FBLOCK * sizeof(int), "bot_faces");
	bot_fsize = BOT_FBLOCK;
	bot_fcurr = 0;
    } else if (bot_fcurr >= bot_fsize) {
	bot_fsize += BOT_FBLOCK;
	bot_faces = (int *)bu_realloc((void *)bot_faces, 3 * bot_fsize * sizeof(int), "bot_faces increase");
    }

    VMOVE(&bot_faces[3*bot_fcurr], face);
    bot_fcurr++;
}

static int
_db_uniq_test(struct bu_vls *n, void *data)
{
    struct db_i *dbip = (struct db_i *)data;
    if (db_lookup(dbip, bu_vls_cstr(n), LOOKUP_QUIET) == RT_DIR_NULL) return 1;
    return 0;
}

void
mk_unique_brlcad_name(struct bu_vls *name)
{
    struct db_i *dbip = fd_out->dbip;

    struct bu_vls tname = BU_VLS_INIT_ZERO;
    const char *c = bu_vls_cstr(name);
    int skip = 0;
    while (*c != '\0') {
	if (skip && *c != '>') {
	    c++;
	    continue;
	}
	if (*c == '<') {
	    skip = 1;
	    c++;
	    continue;
	}
	if (*c == '>') {
	    skip = 0;
	    c++;
	    continue;
	}
	bu_vls_putc(&tname, *c);
	c++;
    }

    const char *keep_chars = "+-=._";
    const char *dedup_chars = "._ ";
    const char *collapse_chars = "._ ";
    bu_vls_simplify(&tname, keep_chars, dedup_chars, collapse_chars);

    if (db_lookup(dbip, bu_vls_cstr(&tname), LOOKUP_QUIET) != RT_DIR_NULL)
	bu_vls_incr(&tname, NULL, NULL, &_db_uniq_test, (void *)dbip);

    bu_vls_sprintf(name, "%s", bu_vls_cstr(&tname));
    bu_vls_free(&tname);
}

static void
Convert_part_ascii(char line[MAX_LINE_SIZE])
{
    char line1[MAX_LINE_SIZE];
    struct bu_vls solid_name = BU_VLS_INIT_ZERO;
    struct bu_vls region_name = BU_VLS_INIT_ZERO;

    int start;
    int i;
    int face_count=0;
    int degenerate_count=0;
    float colr[3]={0.5, 0.5, 0.5};
    unsigned char color[3]={ 128, 128, 128 };
    struct wmember head;
    vect_t normal={0, 0, 0};
    int solid_in_region=0;

    bot_fcurr = 0;
    BU_LIST_INIT(&head.l);


    bu_vls_sprintf(&region_name, "%s", line);
    bu_vls_trimspace(&region_name);

    /* Sanity */
    if (bu_strncmp(bu_vls_cstr(&region_name), "solid", 5) && bu_strncmp(bu_vls_cstr(&region_name), "SOLID", 5)) {
	bu_log("Convert_part_ascii: Called for non-part\n%s\n", line);
	bu_vls_free(&region_name);
	return;
    }

    if (forced_name) {
	bu_vls_strcpy(&region_name, forced_name);
    } else {
	bu_vls_nibble(&region_name, 5);
	bu_vls_trimspace(&region_name);
    }

    if (!bu_vls_strlen(&region_name)) {
	/* build a name from the file name */
	bu_path_component(&region_name, input_file, BU_PATH_BASENAME_EXTLESS);
    }

    // Do the unique name check before adding the suffix to clean up the name
    // as much as possible
    mk_unique_brlcad_name(&region_name);

    // Add the suffix
    bu_vls_printf(&region_name, ".r");

    // Make sure we're still unique
    mk_unique_brlcad_name(&region_name);
    bu_log("Converting Part: %s\n", bu_vls_cstr(&region_name));

    solid_count++;
    bu_path_component(&solid_name, bu_vls_cstr(&region_name), BU_PATH_BASENAME_EXTLESS);
    mk_unique_brlcad_name(&solid_name);
    bu_vls_printf(&solid_name, ".s");
    mk_unique_brlcad_name(&solid_name);

    bu_log("\tUsing solid name: %s\n", bu_vls_cstr(&solid_name));

    while (bu_fgets(line1, MAX_LINE_SIZE, fd_in) != NULL) {
	start = (-1);
	while (isspace((int)line1[++start]));
	if (!bu_strncmp(&line1[start], "endsolid", 8) || !bu_strncmp(&line1[start], "ENDSOLID", 8)) {
	    break;
	} else if (!bu_strncmp(&line1[start], "color", 5) || !bu_strncmp(&line1[start], "COLOR", 5)) {
	    sscanf(&line1[start+5], "%f%f%f", &colr[0], &colr[1], &colr[2]);
	    for (i=0; i<3; i++)
		color[i] = (int)(colr[i] * 255.0);
	} else if (!bu_strncmp(&line1[start], "normal", 6) || !bu_strncmp(&line1[start], "NORMAL", 6)) {
	    float x, y, z;

	    start += 6;
	    sscanf(&line1[start], "%f%f%f", &x, &y, &z);
	    VSET(normal, x, y, z);
	} else if (!bu_strncmp(&line1[start], "facet", 5) || !bu_strncmp(&line1[start], "FACET", 5)) {
	    VSET(normal, 0.0, 0.0, 0.0);

	    start += 4;
	    while (line1[++start] && isspace((int)line1[start]));

	    if (line1[start]) {
		if (!bu_strncmp(&line1[start], "normal", 6) || !bu_strncmp(&line1[start], "NORMAL", 6)) {
		    float x, y, z;

		    start += 6;
		    sscanf(&line1[start], "%f%f%f", &x, &y, &z);
		    VSET(normal, x, y, z);
		}
	    }
	} else if (!bu_strncmp(&line1[start], "outer loop", 10) || !bu_strncmp(&line1[start], "OUTER LOOP", 10)) {
	    int endloop=0;
	    int vert_no=0;
	    int tmp_face[3] = {0, 0, 0};

	    while (!endloop) {
		if (bu_fgets(line1, MAX_LINE_SIZE, fd_in) == NULL)
		    bu_exit(EXIT_FAILURE, "Unexpected EOF while reading a loop in a part!\n");

		start = (-1);
		while (isspace((int)line1[++start]));

		if (!bu_strncmp(&line1[start], "endloop", 7) || !bu_strncmp(&line1[start], "ENDLOOP", 7))
		    endloop = 1;
		else if (!bu_strncmp(&line1[start], "vertex", 6) || !bu_strncmp(&line1[start], "VERTEX", 6)) {
		    double x, y, z;

		    sscanf(&line1[start+6], "%lf%lf%lf", &x, &y, &z);

		    if (vert_no > 2) {
			int n;

			bu_log("Non-triangular loop:\n");
			for (n=0; n<3; n++)
			    bu_log("\t(%g %g %g)\n", V3ARGS(&tree->the_array[tmp_face[n]]));

			bu_log("\t(%g %g %g)\n", x, y, z);
		    }
		    x *= conv_factor;
		    y *= conv_factor;
		    z *= conv_factor;
		    tmp_face[vert_no++] = bg_vert_tree_add(tree, x, y, z, tol.dist_sq);
		} else {
		    bu_log("Unrecognized line: %s\n", line1);
		}
	    }

	    /* check for degenerate faces */
	    if (tmp_face[0] == tmp_face[1]) {
		degenerate_count++;
		continue;
	    }

	    if (tmp_face[0] == tmp_face[2]) {
		degenerate_count++;
		continue;
	    }

	    if (tmp_face[1] == tmp_face[2]) {
		degenerate_count++;
		continue;
	    }

	    if (debug) {
		int n;

		bu_log("Making Face:\n");
		for (n=0; n<3; n++)
		    bu_log("\tvertex #%d: (%g %g %g)\n", tmp_face[n], V3ARGS(&tree->the_array[3*tmp_face[n]]));
		VPRINT(" normal", normal);
	    }

	    Add_face(tmp_face);
	    face_count++;
	}
    }

    /* Check if this part has any solid parts */
    if (face_count == 0) {
	bu_log("\t%s has no solid parts, ignoring\n", bu_vls_cstr(&region_name));
	if (degenerate_count)
	    bu_log("\t%d faces were degenerate\n", degenerate_count);
	bu_vls_free(&region_name);
	bu_vls_free(&solid_name);

	return;
    } else {
	if (degenerate_count)
	    bu_log("\t%d faces were degenerate\n", degenerate_count);
    }

    mk_bot(fd_out, bu_vls_cstr(&solid_name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, tree->curr_vert, bot_fcurr,
	   tree->the_array, bot_faces, NULL, NULL);
    bg_vert_tree_clean(tree);

    if (db5_update_attribute(bu_vls_cstr(&solid_name), "importer", "stl-g", fd_out->dbip))
	bu_bomb("db5_update_attribute() failed");

    if (face_count && !solid_in_region) {
	(void)mk_addmember(bu_vls_cstr(&solid_name), &head.l, NULL, WMOP_UNION);
    }

    bu_log("\tMaking region (%s)\n", bu_vls_cstr(&region_name));

    if (const_id >= 0) {
	mk_lrcomb(fd_out, bu_vls_cstr(&region_name), &head, 1, (char *)NULL,
		  (char *)NULL, color, const_id, 0, mat_code, 100, 0);
	if (face_count) {
	    (void)mk_addmember(bu_vls_cstr(&region_name), &all_head.l, NULL, WMOP_UNION);
	}
    } else {
	mk_lrcomb(fd_out, bu_vls_cstr(&region_name), &head, 1, (char *)NULL, (char *)NULL, color, id_no, 0, mat_code, 100, 0);
	if (face_count)
	    (void)mk_addmember(bu_vls_cstr(&region_name), &all_head.l, NULL, WMOP_UNION);
	id_no++;
    }

    bu_vls_free(&region_name);
    bu_vls_free(&solid_name);

    return;
}

/* Byte swaps a 4-byte val */
void lswap(unsigned int *v)
{
    unsigned int r;

    r = *v;
    *v = ((r & 0xff) << 24) | ((r & 0xff00) << 8) | ((r & 0xff0000) >> 8)
	| ((r & 0xff000000) >> 24);
}

static void
Convert_part_binary(void)
{
    unsigned char buf[51];
    unsigned long num_facets=0;
    float flts[12];
    vect_t normal;
    int tmp_face[3];
    struct wmember head;
    struct bu_vls solid_name = BU_VLS_INIT_ZERO;
    struct bu_vls region_name = BU_VLS_INIT_ZERO;
    int face_count=0;
    int degenerate_count=0;
    size_t ret;

    solid_count++;
    if (forced_name) {
	bu_vls_sprintf(&solid_name, "%s.s", forced_name);
	bu_vls_strcpy(&region_name, forced_name);
    } else {
	bu_vls_strcat(&solid_name, "stl.s");
	bu_vls_strcat(&region_name, "stl.r");
    }
    bu_log("\tUsing solid name: %s\n", bu_vls_cstr(&solid_name));


    ret = fread(buf, 4, 1, fd_in);
    if (ret != 1)
	perror("fread");

    /* swap bytes to convert from Little-endian to network order (big-endian) */
    lswap((unsigned int *)buf);

    /* now use our network to native host format conversion tools */
    num_facets = ntohl(*(uint32_t *)buf);

    bu_log("\t%ld facets\n", num_facets);
    while (fread(buf, 48, 1, fd_in)) {
	int i;
	double pt[3];

	/* swap bytes to convert from Little-endian to network order (big-endian) */
	for (i=0; i<12; i++) {
	    lswap((unsigned int *)&buf[i*4]);
	}

	/* now use our network to native host format conversion tools */
	bu_cv_ntohf((unsigned char *)flts, buf, 12);

	/* unused attribute byte count */
	ret = fread(buf, 2, 1, fd_in);
	if (ret != 1)
	    perror("fread");

	VMOVE(normal, flts);
	VSCALE(pt, &flts[3], conv_factor);
	tmp_face[0] = bg_vert_tree_add(tree, V3ARGS(pt), tol.dist_sq);
	VSCALE(pt, &flts[6], conv_factor);
	tmp_face[1] = bg_vert_tree_add(tree, V3ARGS(pt), tol.dist_sq);
	VSCALE(pt, &flts[9], conv_factor);
	tmp_face[2] = bg_vert_tree_add(tree, V3ARGS(pt), tol.dist_sq);

	/* check for degenerate faces */
	if (tmp_face[0] == tmp_face[1]) {
	    degenerate_count++;
	    continue;
	}

	if (tmp_face[0] == tmp_face[2]) {
	    degenerate_count++;
	    continue;
	}

	if (tmp_face[1] == tmp_face[2]) {
	    degenerate_count++;
	    continue;
	}

	if (debug) {
	    int n;

	    bu_log("Making Face:\n");
	    for (n=0; n<3; n++)
		bu_log("\tvertex #%d: (%g %g %g)\n", tmp_face[n], V3ARGS(&tree->the_array[3*tmp_face[n]]));
	    VPRINT(" normal", normal);
	}

	Add_face(tmp_face);
	face_count++;
    }

    /* Check if this part has any solid parts */
    if (face_count == 0) {
	bu_log("\tpart has no solid parts, ignoring\n");
	if (degenerate_count)
	    bu_log("\t%d faces were degenerate\n", degenerate_count);
	return;
    } else {
	if (degenerate_count)
	    bu_log("\t%d faces were degenerate\n", degenerate_count);
    }

    mk_bot(fd_out, bu_vls_cstr(&solid_name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0,
	   tree->curr_vert, bot_fcurr, tree->the_array, bot_faces, NULL, NULL);
    bg_vert_tree_clean(tree);

    if (db5_update_attribute(bu_vls_cstr(&solid_name), "importer", "stl-g", fd_out->dbip))
	bu_bomb("db5_update_attribute() failed");

    BU_LIST_INIT(&head.l);
    if (face_count) {
	(void)mk_addmember(bu_vls_cstr(&solid_name), &head.l, NULL, WMOP_UNION);
    }
    bu_log("\tMaking region (%s)\n", bu_vls_cstr(&region_name));

    if (const_id >= 0) {
	mk_lrcomb(fd_out, bu_vls_cstr(&region_name), &head, 1, (char *)NULL,
		  (char *)NULL, NULL, const_id, 0, mat_code, 100, 0);
	if (face_count) {
	    (void)mk_addmember(bu_vls_cstr(&region_name), &all_head.l, NULL, WMOP_UNION);
	}
    } else {
	mk_lrcomb(fd_out, bu_vls_cstr(&region_name), &head, 1, (char *)NULL, (char *)NULL, NULL, id_no, 0, mat_code, 100, 0);
	if (face_count)
	    (void)mk_addmember(bu_vls_cstr(&region_name), &all_head.l, NULL, WMOP_UNION);
	id_no++;
    }

    return;
}


static void
Convert_input(void)
{
    char line[ MAX_LINE_SIZE ];

    if (binary) {
	if (fread(line, 80, 1, fd_in) < 1) {
	    if (feof(fd_in)) {
		bu_exit(EXIT_FAILURE, "Unexpected EOF in input file!\n");
	    } else {
		bu_log("Error reading input file\n");
		perror("stl-g");
		bu_exit(EXIT_FAILURE, "Error reading input file\n");
	    }
	}
	line[80] = '\0';
	bu_log("header data:\n%s\n\n", line);
	Convert_part_binary();
    } else {
	while (bu_fgets(line, MAX_LINE_SIZE, fd_in) != NULL) {
	    int start = 0;
	    while (line[start] != '\0' && isspace((int)line[start])) {
		start++;
	    }
	    if (!bu_strncmp(&line[start], "solid", 5) || !bu_strncmp(&line[start], "SOLID", 5))
		Convert_part_ascii(line);
	    else
		bu_log("Unrecognized line:\n%s\n", line);
	}
    }
}


int
main(int argc, char *argv[])
{
    int c;

    bu_setprogname(argv[0]);

    tol.magic = BN_TOL_MAGIC;

    /* this value selected as a reasonable compromise between eliminating
     * needed faces and keeping degenerate faces
     */
    tol.dist = 0.0005;	/* default, same as MGED, RT, ... */
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    forced_name = NULL;

    conv_factor = 1.0;	/* default */

    if (argc < 2) {
	usage(argv[0]);
	bu_exit(1, NULL);
    }

    /* Get command line arguments. */
    /* Don't need to account for -h and -? ("default" takes care of them).  */
    while ((c = bu_getopt(argc, argv, "bt:i:I:m:dx:N:c:")) != -1) {
	double tmp;

	switch (c) {
	    case 'b':	/* binary format */
		binary = 1;
		break;
	    case 't':	/* tolerance */
		tmp = atof(bu_optarg);
		if (tmp <= 0.0) {
		    bu_log("Tolerance must be greater then zero, using default (%g)\n",
			   tol.dist);
		    break;
		}
		tol.dist = tmp;
		tol.dist_sq = tmp * tmp;
		break;
	    case 'c':	/* convert from units */
		conv_factor = bu_units_conversion(bu_optarg);
		if (ZERO(conv_factor)) {
		    bu_log("Illegal units: (%s)\n", bu_optarg);
		    bu_exit(EXIT_FAILURE, "Illegal units!\n");
		}
		else
		    bu_log("Converting units from %s to mm (conversion factor is %g)\n", bu_optarg, conv_factor);
		break;
	    case 'N':	/* force a name on this object */
		forced_name = bu_optarg;
		break;
	    case 'i':
		id_no = atoi(bu_optarg);
		break;
	    case  'I':
		const_id = atoi(bu_optarg);
		if (const_id < 0) {
		    bu_log("Illegal value for '-I' option, must be zero or greater!\n");
		    usage(argv[0]);
		    bu_exit(EXIT_FAILURE, "Illegal value for option '-I'\n");
		}
		break;
	    case 'm':
		mat_code = atoi(bu_optarg);
		break;
	    case 'd':
		debug = 1;
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&rt_debug);
		bu_printb("librt RT_G_DEBUG", RT_G_DEBUG, RT_DEBUG_FORMAT);
		bu_log("\n");
		break;
	    default:
		usage(argv[0]);
		bu_exit(1, NULL);
		break;
	}
    }

    input_file = argv[bu_optind];
    if ((fd_in=fopen(input_file, "rb")) == NULL) {
	bu_log("Cannot open input file (%s)\n", input_file);
	perror(argv[0]);
	bu_exit(1, NULL);
    }
    bu_optind++;
    brlcad_file = argv[bu_optind];
    if ((fd_out=wdb_fopen(brlcad_file)) == NULL) {
	bu_log("Cannot open BRL-CAD file (%s)\n", brlcad_file);
	perror(argv[0]);
	bu_exit(1, NULL);
    }

    mk_id_units(fd_out, "Conversion from Stereolithography format", "mm");

    BU_LIST_INIT(&all_head.l);

    /* create a tree structure to hold the input vertices */
    tree = bg_vert_tree_create();

    Convert_input();

    /* make a top level group */
    mk_lcomb(fd_out, "all", &all_head, 0, (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);

    fclose(fd_in);

    db_close(fd_out->dbip);

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
