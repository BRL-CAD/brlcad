/*                          M A I N . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2026 United States Government as represented by
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
/** @file iges/main.c
 *
 * IGES to BRL-CAD converter
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "bu/app.h"
#include "bu/debug.h"
#include "bu/opt.h"
#include "bu/str.h"
#include "bu/vls.h"
/* private */
#include "./iges_struct.h"
#include "./iges_types.h"
#include "./iges_extern.h"
#include "./iges_brep_import.h"
#include "./iges_import.h"
#include "brlcad_ident.h"


int do_projection = 1;
char eord = 0;
char eofd = -1;
char card[256] = {0};
fastf_t scale = 0.0;
fastf_t inv_scale = 0.0;
fastf_t conv_factor = 0.0;
int units = 0;
int counter = 0;
int pstart = 0;
int dstart = 0;
size_t totentities = 0;
size_t dirarraylen = 0;
FILE *fd = NULL;
struct rt_wdb *fdout = NULL;
char brlcad_file[256] = {0};
int reclen = 0;
b_off_t *rec_offset = NULL;
size_t nrecords = 0;
int currec = 0;
size_t ntypes = 0;
int brlcad_att_de = 0;
struct iges_directory **dir = NULL;
struct reglist *regroot = NULL;
struct iges_edge_list *edge_root = NULL;
struct iges_vertex_list *vertex_root = NULL;
struct bn_tol tol = BN_TOL_INIT_ZERO;
char *solid_name = NULL;
struct file_list iges_list = IGES_FILE_LIST_INIT_ZERO;
struct file_list *curr_file = NULL;
struct name_list *name_root = NULL;

char operators[] = {
    ' ',
    'u',
    '+',
    '-' };

mat_t *identity = NULL;


static int do_splines = 0;
static int do_drawings = 0;
static int trimmed_surf = 0;
int do_bots = 0;	/* -m: write boundary-rep as a BoT (triangle mesh) */
int do_brep = 1;	/* default: write boundary-rep as rt_brep (OpenNURBS) */

static char *iges_file = NULL;


static int
Parse_debug(struct bu_vls *message, size_t argc, const char **argv,
    void *destination)
{
    char *end = NULL;
    unsigned long value;

    BU_OPT_CHECK_ARGV0(message, argc, argv, "debug mask");
    errno = 0;
    value = strtoul(argv[0], &end, 16);
    if (errno || end == argv[0] || *end != '\0' || value > UINT_MAX) {
	if (message)
	    bu_vls_printf(message, "invalid hexadecimal debug mask: %s",
		argv[0]);
	return -1;
    }
    *((uint32_t *)destination) = (uint32_t)value;
    return 1;
}

static const char *msg1 =
"\nThis IGES file contains solid model entities, but your options do not permit\n\
converting them to BRL-CAD. You may want to try 'iges-g -o file.g %s' to\n\
convert the solid model elements\n";

static const char *msg2 =
"\nThis IGES file contains drawing entities, but no solid model entities. You may\n\
convert the drawing to BRL-CAD by 'iges-g -d -o file.g %s'. Note that the resulting\n\
BRL-CAD object will be a 2D drawing, not a solid object. You might also try the\n\
'-3' option to get 3D drawings\n";

static const char *msg3 =
"\nThis IGES file contains spline surfaces, but no solid model entities. All the spline\n\
surfaces in the IGES file may be combined into a single BRL-CAD spline solid by\n\
'iges-g -n -o file.g %s'\n";

static const char *msg4 =
"\nThis IGES file contains trimmed surfaces, but no solid model entities.\n\
Try the '-t' option to convert all the trimmed surfaces into one BRL-CAD solid.\n\
'iges-g -t -o file.g %s'\n";

void
Suggestions(void)
{
    int i;
    int csg = 0;
    int b_rep = 0;
    int splines = 0;
    int tsurfs = 0;
    int drawing = 0;

    /* categorize the elements in the IGES file as to whether they are
     * CSG, BREP, Trimmed surfaces, Spline surfaces, or drawing elements
     */
    for (i = 0; i < NTYPES; i++) {
	if ((typecount[i].type >= 150 && typecount[i].type <= 184) ||
	    typecount[i].type == 430)
	    csg += typecount[i].count;
	else if (typecount[i].type == 186 ||
		 (typecount[i].type >= 502 && typecount[i].type <= 514))
	    b_rep += typecount[i].count;
	else if (typecount[i].type == 128)
	    splines += typecount[i].count;
	else if (typecount[i].type == 144)
	    tsurfs += typecount[i].count;
	else if ((typecount[i].type >= 100 && typecount[i].type <= 112) ||
		 typecount[i].type == 126 ||
		 (typecount[i].type >= 202 && typecount[i].type <= 230) ||
		 typecount[i].type == 404 || typecount[i].type == 410)
	    drawing += typecount[i].count;
    }

    if ((csg || b_rep) && (do_splines || do_drawings || trimmed_surf))
	bu_log(msg1, iges_file);

    if (drawing && csg == 0 && b_rep == 0 && !do_drawings)
	bu_log(msg2, iges_file);

    if (splines && csg == 0 && b_rep == 0 && !do_splines)
	bu_log(msg3, iges_file);

    if (tsurfs && csg == 0 && b_rep == 0 && !trimmed_surf)
	bu_log(msg4, iges_file);
}


/*
 * Return 1 if the output database already holds at least one user-visible
 * geometry object (anything not named with a leading underscore, which
 * marks the internal _GLOBAL bookkeeping object), else 0.  Used to decide
 * whether a conversion produced anything renderable.
 */
static int
Have_geometry(void)
{
    struct directory *dp;

    if (!fdout || !fdout->dbip)
	return 0;

    FOR_ALL_DIRECTORY_START(dp, fdout->dbip) {
	if (dp->d_namep && dp->d_namep[0] != '_')
	    return 1;
    } FOR_ALL_DIRECTORY_END;

    return 0;
}


int
main(int argc, char *argv [])
{
    int i;
    int file_count = 0;
    int drawing_3d = 0;
    int mesh_output = 0;
    int polygon_output = 0;
    int strict_import = 0;
    int exact_import = 0;
    int legacy_drawings = 0;
    char *output_file = (char *)NULL;
    char *report_file = (char *)NULL;
    char *repair_mode = (char *)NULL;
    const char *program_name = argv[0];
    struct bu_list *vlfree = &rt_vlfree;
    struct bu_vls option_messages = BU_VLS_INIT_ZERO;
    struct bu_opt_desc options[] = {
	{"3", "3d-drawings", "", NULL, &drawing_3d,
	    "preserve drawing model-space planes instead of projecting to XY"},
	{"d", "drawings", "", NULL, &do_drawings,
	    "import drawings as native sketch/annotation objects"},
	{"m", "mesh", "", NULL, &mesh_output,
	    "write boundary representations as BoT meshes"},
	{"n", "nurbs", "", NULL, &do_splines,
	    "combine rational B-spline surfaces into one solid"},
	{"t", "trimmed-surfaces", "", NULL, &trimmed_surf,
	    "combine trimmed surfaces into one solid"},
	{"p", "polygonal", "", NULL, &polygon_output,
	    "write boundary representations as polygonal NMG solids"},
	{"o", "output", "FILE", bu_opt_str, &output_file,
	    "BRL-CAD output database"},
	{"N", "name", "NAME", bu_opt_str, &solid_name,
	    "name of the single requested output object"},
	{"x", "rt-debug", "HEX", Parse_debug, &rt_debug,
	    "librt hexadecimal debug mask"},
	{"X", "nmg-debug", "HEX", Parse_debug, &nmg_debug,
	    "NMG hexadecimal debug mask"},
	{"", "exact", "", NULL, &exact_import,
	    "disallow source-data repairs during direct import"},
	{"", "strict", "", NULL, &strict_import,
	    "reject repaired or partial direct imports"},
	{"", "repair", "MODE", bu_opt_str, &repair_mode,
	    "none or safe (default: safe)"},
	{"", "report", "FILE", bu_opt_str, &report_file,
	    "structured JSON import report"},
	{"", "legacy-drawings", "", NULL, &legacy_drawings,
	    "request the historical NMG wire drawing path"},
	BU_OPT_DESC_NULL
    };

    bu_setprogname(program_name);
    ++argv;
    --argc;
    argc = bu_opt_parse(&option_messages, argc, (const char **)argv, options);
    if (bu_vls_strlen(&option_messages))
	bu_log("%s\n", bu_vls_cstr(&option_messages));
    if (drawing_3d) {
	do_drawings = 1;
	do_projection = 0;
    }
    if (bu_vls_strlen(&option_messages) || argc != 1 || !output_file ||
	    do_drawings + do_splines + trimmed_surf > 1 ||
	    mesh_output + polygon_output > 1 ||
	    (repair_mode && !BU_STR_EQUAL(repair_mode, "none") &&
		!BU_STR_EQUAL(repair_mode, "safe"))) {
	bu_vls_free(&option_messages);
	usage(program_name);
    }
    bu_vls_free(&option_messages);
    if (mesh_output) {
	do_brep = 0;
	do_bots = 1;
    } else if (polygon_output) {
	do_brep = 0;
	do_bots = 0;
    }

    bu_log("%s", brlcad_ident("IGES to BRL-CAD Translator"));
    bu_log("Please direct bug reports to <bugs@brlcad.org>\n\n");

    /* Initialize some variables */
    ntypes = NTYPES;
    regroot = NULL;
    edge_root = NULL;
    vertex_root = NULL;
    name_root = NULL;
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    Initstack();	/* Initialize node stack */

    BU_ALLOC(identity, mat_t);
    for (i = 0; i < 16; i++) {
	if (!(i%5))
	    (*identity)[i] = 1.0;
	else
	    (*identity)[i] = 0.0;
    }

    if ((fdout = wdb_fopen(output_file)) == NULL) {
	bu_log("Cannot open %s\n", output_file);
	perror("iges-g");
	usage(program_name);
    }
    bu_strlcpy(brlcad_file,  output_file, sizeof(brlcad_file));

    /* Keep the semantic importer independent of the legacy parser.  Besides
     * avoiding duplicate work, this ensures bounded repairs in the modern
     * parser are not rejected first by historical global parser state. */
    if (do_drawings && !legacy_drawings) {
	const int semantic_result = iges_import_annotations(argv[0], fdout,
	    do_projection, exact_import, strict_import,
	    repair_mode ? repair_mode : "safe", solid_name, report_file);
	if (semantic_result < 0) {
	    wdb_close(fdout);
	    bu_exit(BRLCAD_ERROR,
		"Semantic IGES drawing import failed for %s\n", argv[0]);
	}
	if (semantic_result > 0) {
	    wdb_close(fdout);
	    return BRLCAD_OK;
	}
    }

    /* A supported type 186 is assembled directly in OpenNURBS.  Returning
     * zero deliberately hands unsupported or mixed legacy content to the
     * established CSG/NMG handlers below. */
    if (!do_drawings && !trimmed_surf && !do_splines && do_brep) {
	const int direct_result = iges_import_breps(argv[0], fdout,
	    exact_import, strict_import, repair_mode ? repair_mode : "safe",
	    report_file);
	if (direct_result < 0) {
	    wdb_close(fdout);
	    bu_exit(BRLCAD_ERROR,
		"Direct IGES B-Rep import failed for %s\n", argv[0]);
	}
	if (direct_result > 0) {
	    wdb_close(fdout);
	    return BRLCAD_OK;
	}
    }

    BU_LIST_INIT(&iges_list.l);

    BU_ALLOC(curr_file, struct file_list);


    char dname[MAXPATHLEN] = "all";
    int snull = 0;
    if (!solid_name) {
	solid_name = (char *)dname;
	snull = 1;
    }

    bu_strlcpy(curr_file->obj_name, Make_unique_brl_name(solid_name), NAMESIZE+1);

    if (snull)
	solid_name = NULL;

    curr_file->file_name = (char *)bu_malloc(strlen(argv[0])+1, "iges-g: curr_file->file_name");
    bu_strlcpy(curr_file->file_name, argv[0], strlen(argv[0])+1);
    BU_LIST_APPEND(&iges_list.l, &curr_file->l);

    while (BU_LIST_NON_EMPTY(&iges_list.l)) {
	curr_file = BU_LIST_FIRST(file_list, &iges_list.l);
	iges_file = curr_file->file_name;

	fd = fopen(iges_file, "rb");	/* open IGES file */
	if (fd == NULL) {
	    bu_log("Cannot open %s\n", iges_file);
	    perror("iges-g");
	    usage(program_name);
	}

	bu_log("\n\n\nIGES FILE: %s\n", iges_file);

	reclen = Recsize() * sizeof(char); /* Check length of records */
	if (reclen == 0)
	    bu_exit(1, "File (%s) not in IGES ASCII format\n", iges_file);

	Build_rec_index();	/* Index the byte offset of every record */

	Freestack();	/* Set node stack to empty */

	Zero_counts();	/* Set summary information to all zeros */

	Readstart();	/* Read start section */

	Readglobal(file_count);	/* Read global section */

	pstart = Findp();	/* Find start of parameter section */

	Makedir();	/* Read directory section and build a linked list of entries */

	Summary();	/* Print a summary of what is in the IGES file */

	Docolor();	/* Get color info from color definition entities */

	Get_att();	/* Look for a BRL-CAD attribute definition */

	Evalxform();	/* Accumulate the transformation matrices */

	Check_names();	/* Look for name entities */

	if (do_drawings)
	    Conv_drawings(vlfree); /* non-planar/unsupported wire fallback */
	else if (trimmed_surf) {
	    Do_subfigs();		/* Look for Singular Subfigure Instances */

	    Convtrimsurfs(vlfree);	/* try to convert trimmed surfaces to a single solid */
	} else if (do_splines)
	    Convsurfs();		/* Convert NURBS to a single solid */
	else {
	    Convinst();	/* Handle Instances */

	    Convsolids(vlfree);	/* Convert solid entities */

	    Convtree();	/* Convert Boolean Trees */

	    Convassem();	/* Convert solid assemblies */

	    /* Also import any Trimmed Parametric Surfaces (IGES 144) present.
	     * These are how faithful boundary-rep geometry (e.g. g-iges brep
	     * output) is expressed, so importing them here lets such files
	     * round-trip without requiring the -t option. */
	    for (i = 0; (size_t)i < totentities; i++) {
		if (dir[i]->type == 144) {
		    Do_subfigs();	/* Look for Singular Subfigure Instances */
		    Convtrimsurfs(vlfree);
		    break;
		}
	    }
	}

	/* Fallback: if brep output was requested but the faithful conversion
	 * produced nothing renderable (common for trimmed-surface or
	 * manifold-BREP files whose face loops we cannot yet fully
	 * reconstruct), import the raw NURBS/analytic surfaces as an
	 * untrimmed brep so the geometry is still usable.  (The -n path has
	 * already done this, so skip it there.) */
	if (do_brep && !do_drawings && !do_splines && !Have_geometry()) {
	    int have_surf = 0;
	    for (i = 0; (size_t)i < totentities; i++) {
		int t = dir[i]->type;
		if (t == 128 || t == 114 || t == 118 ||
		    t == 120 || t == 122 || t == 140) {
		    have_surf = 1;
		    break;
		}
	    }
	    if (have_surf) {
		bu_log("\nNo faithful solids were produced; "
		       "importing raw surfaces as an untrimmed brep\n");
		Convsurfs();
	    }
	}

	Free_dir();
	Free_rec_index();

	BU_LIST_DEQUEUE(&curr_file->l);
	bu_free(curr_file->file_name, "iges-g: curr_file->file_name");
	bu_free(curr_file, "iges-g: curr_file");
	file_count++;
    }

    iges_file = argv[0];
    Suggestions();
    wdb_close(fdout);
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
