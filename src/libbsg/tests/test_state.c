/*                       T E S T _ S T A T E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file libbsg/tests/test_state.c
 *
 * Public traversal-state API coverage.
 */

#include "common.h"

#include <math.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bsg/state.h"

#define CHECK(_expr, _msg) do { if (!(_expr)) { bu_log("FAIL: %s\n", (_msg)); return 1; } } while (0)

static int
test_state_push_copy_transform(void)
{
    bsg_state_ref root = bsg_state_ref_create();
    CHECK(!bsg_state_ref_is_null(root), "state create");
    CHECK(bsg_state_ref_visible(root) == 1, "default visible");
    CHECK(bsg_state_ref_lod_level(root) == -1, "default lod level");

    mat_t xf;
    MAT_IDN(xf);
    MAT_DELTAS(xf, 3.0, 4.0, 5.0);
    bsg_state_ref_set_transform(root, xf);
    bsg_state_ref_set_visibility(root, 1, 0, 1);
    bsg_state_ref_set_render_phase(root, 2);
    bsg_state_ref_set_source_identity(root, 12345);
    bsg_state_ref_set_backend_capabilities(root, 0x55u);

    bsg_state_ref child = bsg_state_ref_push(root);
    CHECK(!bsg_state_ref_is_null(child), "state push");
    CHECK(bsg_state_ref_visible(child) == 0, "pushed visible copy");
    CHECK(bsg_state_ref_force_draw(child) == 1, "pushed force draw copy");
    CHECK(bsg_state_ref_render_phase(child) == 2, "pushed render phase");
    CHECK(bsg_state_ref_source_identity(child) == 12345, "pushed source identity");
    CHECK(bsg_state_ref_backend_capabilities(child) == 0x55u, "pushed backend capabilities");

    mat_t local;
    MAT_IDN(local);
    MAT_DELTAS(local, 2.0, 0.0, 0.0);
    bsg_state_ref_multiply_transform(child, local);
    mat_t got;
    bsg_state_ref_transform(child, got);
    point_t origin = VINIT_ZERO;
    point_t moved = VINIT_ZERO;
    MAT4X3PNT(moved, got, origin);
    CHECK(fabs(moved[X] - 5.0) < 0.0001, "push transform multiply");
    CHECK(fabs(moved[Y] - 4.0) < 0.0001, "push transform Y preserved");

    bsg_state_ref_set_selection(child, 1, 1);
    CHECK(bsg_state_ref_selected(child) == 1, "selected state");
    CHECK(bsg_state_ref_highlighted(child) == 1, "highlighted state");

    unsigned char color[3] = {10, 20, 30};
    bsg_state_ref_set_material_color(child, color);
    bsg_state_ref_material_color(child, color);
    CHECK(color[0] == 10 && color[1] == 20 && color[2] == 30, "material color");
    bsg_state_ref_set_draw_style(child, 3, 4, 1);
    CHECK(bsg_state_ref_draw_mode(child) == 3, "draw mode");
    CHECK(bsg_state_ref_line_width(child) == 4, "line width");
    CHECK(bsg_state_ref_line_style(child) == 1, "line style");
    bsg_state_ref_set_complexity(child, 0.75);
    CHECK(fabs(bsg_state_ref_complexity(child) - 0.75) < 0.0001, "complexity");

    bsg_state_ref_pop(child);
    bsg_state_ref_destroy(root);
    return 0;
}

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);
    if (test_state_push_copy_transform())
	return 1;
    bu_log("state tests passed\n");
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
