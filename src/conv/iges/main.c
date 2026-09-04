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
#include <math.h>
#include <stdlib.h>

#include "bu/app.h"
#include "bu/avs.h"
#include "bu/debug.h"
#include "bu/malloc.h"
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

static void
usage(const char *argv0, const struct bu_opt_desc *options)
{
    char *description = bu_opt_describe(options, NULL);
    bu_log("Usage: %s [options] -o output.g input.iges\n%s", argv0,
	description ? description : "");
    if (description)
	bu_free(description, "iges-g option description");
    bu_log("The nurbs, drawings (or 3d-drawings), and trimmed-surfaces modes "
	"are mutually exclusive. OpenNURBS B-Rep output is the default; mesh "
	"and polygonal output are explicit fallbacks.\n");
    bu_log("The name option sets the imported geometry root name.\n");
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
"\nThis IGES file contains spline surfaces, but no solid model entities. Import the\n\
surfaces directly as OpenNURBS B-Rep geometry with\n\
'iges-g -n -o file.g %s'\n";

static const char *msg4 =
"\nThis IGES file contains trimmed surfaces, but no solid model entities.\n\
Import the trimmed faces directly as OpenNURBS B-Rep geometry with\n\
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


static void
Mark_direct_imports(void)
{
    struct directory *dp;

    if (!fdout || !fdout->dbip || !dir)
	return;
    FOR_ALL_DIRECTORY_START(dp, fdout->dbip) {
	struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
	if (db5_get_attributes(fdout->dbip, &attributes, dp) == 0) {
	    const char *value = bu_avs_get(&attributes, "iges.entity");
	    char *end = NULL;
	    long parsed = 0;
	    if (value) {
		errno = 0;
		parsed = strtol(value, &end, 10);
	    }
	    if (value && !errno && end != value && *end == '\0' &&
		    parsed > 0 && parsed <= INT_MAX) {
		const int directory_id = (int)parsed;
		const int index = IGES_DE2INDEX(directory_id);
		if (index >= 0 && (size_t)index < totentities && dir[index] &&
			dir[index]->direct == directory_id)
		    dir[index]->direct_imported = 1;
	    }
	}
	bu_avs_free(&attributes);
    } FOR_ALL_DIRECTORY_END;
}


int
main(int argc, char *argv [])
{
    int i;
    int file_count = 0;
    int help = 0;
    int drawing_3d = 0;
    int mesh_output = 0;
    int polygon_output = 0;
    int strict_import = 0;
    int exact_import = 0;
    int legacy_drawings = 0;
    int direct_brep_imported = 0;
    fastf_t default_plate_thickness = 0.0;
    char *output_file = (char *)NULL;
    char *report_file = (char *)NULL;
    char *repair_mode = (char *)NULL;
    const char *program_name = argv[0];
    struct bu_list *vlfree = &rt_vlfree;
    struct bu_vls option_messages = BU_VLS_INIT_ZERO;
    struct bu_opt_desc options[] = {
	{"h", "help", "", NULL, &help, "print help and exit"},
	{"?", "", "", NULL, &help, ""},
	{"3", "3d-drawings", "", NULL, &drawing_3d,
	    "preserve drawing model-space planes instead of projecting to XY"},
	{"d", "drawings", "", NULL, &do_drawings,
	    "import drawings as native sketch/annotation objects"},
	{"m", "mesh", "", NULL, &mesh_output,
	    "write boundary representations as BoT meshes"},
	{"n", "nurbs", "", NULL, &do_splines,
	    "import spline surfaces directly as OpenNURBS B-Rep geometry"},
	{"t", "trimmed-surfaces", "", NULL, &trimmed_surf,
	    "import trimmed surfaces directly as OpenNURBS B-Rep geometry"},
	{"p", "polygonal", "", NULL, &polygon_output,
	    "write boundary representations as polygonal NMG solids"},
	{"o", "output", "FILE", bu_opt_str, &output_file,
	    "BRL-CAD output database"},
	{"N", "name", "NAME", bu_opt_str, &solid_name,
	    "name of the imported geometry root"},
	{"x", "rt-debug", "HEX", Parse_debug, &rt_debug,
	    "librt hexadecimal debug mask"},
	{"X", "nmg-debug", "HEX", Parse_debug, &nmg_debug,
	    "NMG hexadecimal debug mask"},
	{"", "exact", "", NULL, &exact_import,
	    "disallow source-data repairs during direct import"},
	{"", "strict", "", NULL, &strict_import,
	    "reject repaired or partial direct imports"},
	{"", "default-plate-thickness", "MM", bu_opt_fastf_t,
	    &default_plate_thickness,
	    "assign this thickness to imported non-solid B-Reps"},
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
    if (help) {
	usage(program_name, options);
	bu_vls_free(&option_messages);
	return BRLCAD_OK;
    }
    if (drawing_3d) {
	do_drawings = 1;
	do_projection = 0;
    }
    if (!isfinite(default_plate_thickness) || default_plate_thickness < 0.0)
	bu_vls_printf(&option_messages,
	    "default plate thickness must be a finite non-negative value");
    else if (default_plate_thickness > 0.0 &&
	    (do_drawings || mesh_output || polygon_output))
	bu_vls_printf(&option_messages,
	    "default plate thickness requires OpenNURBS B-Rep output");
    if (bu_vls_strlen(&option_messages) || argc != 1 || !output_file ||
	    do_drawings + do_splines + trimmed_surf > 1 ||
	    mesh_output + polygon_output > 1 ||
	    (repair_mode && !BU_STR_EQUAL(repair_mode, "none") &&
		!BU_STR_EQUAL(repair_mode, "safe"))) {
	usage(program_name, options);
	bu_vls_free(&option_messages);
	return BRLCAD_ERROR;
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
	return BRLCAD_ERROR;
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

    /* Import all supported boundary representations directly in OpenNURBS.
     * Native IGES CSG may still be handled below, but direct B-Reps must never
     * enter the legacy NMG NURBS paths. */
    if (!do_drawings && do_brep) {
	const int direct_result = iges_import_breps(argv[0], fdout,
	    exact_import, strict_import, repair_mode ? repair_mode : "safe",
	    default_plate_thickness, solid_name, report_file);
	if (direct_result < 0) {
	    wdb_close(fdout);
	    bu_exit(BRLCAD_ERROR,
		"Direct IGES B-Rep import failed for %s\n", argv[0]);
	}
	if (direct_result == 1 ||
		(direct_result == 2 && (do_splines || trimmed_surf))) {
	    wdb_close(fdout);
	    return BRLCAD_OK;
	}
	direct_brep_imported = direct_result == 2;
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
	    wdb_close(fdout);
	    return BRLCAD_ERROR;
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
	const int direct_entities_imported = direct_brep_imported && file_count == 0;
	if (direct_entities_imported)
	    Mark_direct_imports();

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

	    Convsolids(vlfree, direct_entities_imported); /* Convert solid entities */

	    Convtree();	/* Convert Boolean Trees */

	    Convassem();	/* Convert solid assemblies */

	    /* Also import any Trimmed Parametric Surfaces (IGES 144) present.
	     * These are how faithful boundary-rep geometry (e.g. g-iges brep
	     * output) is expressed, so importing them here lets such files
	     * round-trip without requiring the -t option. */
	    for (i = 0; !direct_entities_imported && (size_t)i < totentities; i++) {
		if (dir[i]->type == 144) {
		    Do_subfigs();	/* Look for Singular Subfigure Instances */
		    Convtrimsurfs(vlfree);
		    break;
		}
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
