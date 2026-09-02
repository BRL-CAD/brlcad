/*              N I R T _ A N N O T A T I O N S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include <cstdio>
#include <cstdlib>

#include "analyze.h"
#include "bu/app.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "wdb.h"


static constexpr fastf_t FRONT_ANNOTATION_X = 18.0;
static constexpr fastf_t REAR_ANNOTATION_X = 10.0;
static constexpr fastf_t SCREEN_ANNOTATION_X = 19.0;
static constexpr fastf_t OCCLUDER_MINIMUM_X = 15.0;
static constexpr fastf_t OCCLUDER_MAXIMUM_X = 16.0;
static constexpr fastf_t RAY_ORIGIN_X = 20.0;
static constexpr fastf_t TEST_HALF_EXTENT = 1.0;


static int
capture_output(struct nirt_state *state, void *data)
{
    struct bu_vls current = BU_VLS_INIT_ZERO;
    struct bu_vls *captured = (struct bu_vls *)data;

    nirt_log(&current, state, NIRT_OUT);
    bu_vls_vlscat(captured, &current);
    bu_vls_free(&current);
    return 0;
}


static void
write_annotation(struct rt_wdb *wdbp, const char *name, fastf_t x,
	bool screen_space)
{
    struct rt_annot_internal annotation = {};
    struct line_seg line = {};
    struct rt_annot_seg_style style = {};
    int reverse = 0;
    void *segment = &line;
    point2d_t vertices[2] = {{0.0, 0.0}, {TEST_HALF_EXTENT, 0.0}};

    annotation.magic = RT_ANNOT_INTERNAL_MAGIC;
    VSET(annotation.V, x, 0.0, 0.0);
    annotation.flags = screen_space ? RT_ANNOT_SCREEN_SPACE :
	RT_ANNOT_MODEL_SPACE;
    VSET(annotation.u_vec, 0.0, 1.0, 0.0);
    VSET(annotation.v_vec, 0.0, 0.0, 1.0);
    annotation.vert_count = 2;
    annotation.verts = vertices;
    annotation.ant.count = 1;
    annotation.ant.reverse = &reverse;
    annotation.ant.segments = &segment;
    annotation.styles = &style;

    line.magic = CURVE_LSEG_MAGIC;
    line.start = 0;
    line.end = 1;
    style.role = RT_ANNOT_ROLE_LEADER;
    style.line_pattern = RT_ANNOT_LINE_CONTINUOUS;

    if (rt_annot_validate(&annotation, NULL) ||
	    mk_annot(wdbp, name, &annotation) < 0)
	bu_exit(EXIT_FAILURE, "unable to write %s\n", name);
}


static void
clear_standard_formats(struct nirt_state *state)
{
    const char *commands[] = {
	"fmt r \"\"", "fmt h \"\"", "fmt p \"\"", "fmt f \"\"",
	"fmt m \"\"", "fmt o \"\"", "fmt g \"\""
    };

    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i)
	if (nirt_exec(state, commands[i]))
	    bu_exit(EXIT_FAILURE, "NIRT rejected command: %s\n", commands[i]);
}


static void
get_output(struct bu_vls *output, struct bu_vls *captured)
{
    bu_vls_sprintf(output, "%s", bu_vls_cstr(captured));
    bu_vls_trunc(captured, 0);
}


int
main(int argc, char **argv)
{
    char path[MAXPATHLEN];
    struct db_i *dbip;
    struct rt_wdb *wdbp;
    struct nirt_state *state;
    struct bu_vls captured = BU_VLS_INIT_ZERO;
    struct bu_vls output = BU_VLS_INIT_ZERO;
    struct wmember members;
    point_t minimum, maximum;

    bu_setprogname(argv[0]);
    if (argc != 1)
	return EXIT_FAILURE;
    {
	FILE *fp = bu_temp_file(path, sizeof(path));
	if (!fp)
	    bu_exit(EXIT_FAILURE, "unable to create temporary database path\n");
	fclose(fp);
    }
    dbip = db_create(path, 5);
    if (!dbip)
	bu_exit(EXIT_FAILURE, "unable to create temporary database\n");
    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    if (!wdbp)
	bu_exit(EXIT_FAILURE, "unable to open temporary database for writing\n");

    write_annotation(wdbp, "front.annot", FRONT_ANNOTATION_X, false);
    write_annotation(wdbp, "rear.annot", REAR_ANNOTATION_X, false);
    write_annotation(wdbp, "screen.annot", SCREEN_ANNOTATION_X, true);
    VSET(minimum, OCCLUDER_MINIMUM_X, -TEST_HALF_EXTENT,
	-TEST_HALF_EXTENT);
    VSET(maximum, OCCLUDER_MAXIMUM_X, TEST_HALF_EXTENT,
	TEST_HALF_EXTENT);
    if (mk_rpp(wdbp, "occluder.s", minimum, maximum) < 0)
	bu_exit(EXIT_FAILURE, "unable to write occluder\n");
    BU_LIST_INIT(&members.l);
    if (!mk_addmember("occluder.s", &members.l, NULL, WMOP_UNION) ||
	    mk_lcomb(wdbp, "occluder.r", &members, 1, NULL, NULL, NULL, 0))
	bu_exit(EXIT_FAILURE, "unable to write occluding region\n");

    BU_GET(state, struct nirt_state);
    if (nirt_init(state))
	bu_exit(EXIT_FAILURE, "unable to initialize NIRT\n");
    nirt_udata(state, &captured);
    nirt_hook(state, capture_output, NIRT_OUT);
    if (nirt_exec(state,
	    "draw front.annot rear.annot screen.annot occluder.r") ||
	    nirt_init_dbip(state, dbip))
	bu_exit(EXIT_FAILURE, "unable to prepare NIRT test database\n");
    clear_standard_formats(state);
    if (nirt_exec(state,
	    "fmt a \"%s|%d|%s|%.1f|%.1f|%d\\n\" annot_path annot_segment annot_role annot_x annot_d annot_visible"))
	bu_exit(EXIT_FAILURE, "unable to set annotation report format\n");
    struct bu_vls shot = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&shot, "backout 0;xyz %g 0 0;dir -1 0 0;s",
	RAY_ORIGIN_X);
    if (nirt_exec(state, bu_vls_cstr(&shot)))
	bu_exit(EXIT_FAILURE, "unable to shoot default NIRT ray\n");
    bu_vls_free(&shot);
    get_output(&output, &captured);
    if (bu_vls_strlen(&output))
	bu_exit(EXIT_FAILURE,
	    "annotations were reported without useannot: %s\n",
	    bu_vls_cstr(&output));

    if (nirt_exec(state, "useannot 1;s"))
	bu_exit(EXIT_FAILURE, "unable to shoot annotation NIRT ray\n");
    get_output(&output, &captured);
    struct bu_vls expected = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&expected,
	"/front.annot|0|leader|%.1f|%.1f|1\n"
	"/rear.annot|0|leader|%.1f|%.1f|0\n",
	FRONT_ANNOTATION_X, FRONT_ANNOTATION_X,
	REAR_ANNOTATION_X, REAR_ANNOTATION_X);
    if (!BU_STR_EQUAL(bu_vls_cstr(&output), bu_vls_cstr(&expected)))
	bu_exit(EXIT_FAILURE, "unexpected annotation report:\n%s",
	    bu_vls_cstr(&output));
    bu_vls_free(&expected);

    nirt_destroy(state);
    BU_PUT(state, struct nirt_state);
    bu_vls_free(&captured);
    bu_vls_free(&output);
    db_close(dbip);
    bu_file_delete(path);
    return EXIT_SUCCESS;
}
