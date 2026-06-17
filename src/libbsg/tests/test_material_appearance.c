/* T E S T _ M A T E R I A L _ A P P E A R A N C E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libbsg/tests/test_material_appearance.c
 *
 * Phase 3 unit tests for material/appearance API.
 */

#include "common.h"

#include <stdio.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bsg/appearance.h"
#include "../appearance_private.h"
#include "bsg/material.h"
#include "../material_private.h"
#include "../node_private.h"
#include "bsg/util.h"

#define CHECK(_expr, _msg) \
    do { \
	if (!(_expr)) { \
	    printf("FAIL: %s\n", (_msg)); \
	    return 1; \
	} \
    } while (0)

static struct bsg_view *
make_view(void)
{
    struct bsg_view *v = NULL;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "test_mat_app_view");
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "test mat/app view");
}

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);

    struct bsg_view *v = make_view();
    bsg_node *n = bsg_shape_create(v);
    CHECK(n != NULL, "create node");

    bsg_material_set_rgb(n, 11, 22, 33);
    unsigned char r = 0, g = 0, b = 0;
    bsg_material_get_rgb(n, &r, &g, &b);
    CHECK(r == 11 && g == 22 && b == 33, "material rgb");

    bsg_material_set_revision(n, 77);
    CHECK(bsg_material_revision(n) == 77, "material revision");

    bsg_appearance_set_visible(n, 0);
    CHECK(!bsg_appearance_visible(n), "appearance visible");
    bsg_appearance_set_visible(n, 1);
    CHECK(bsg_appearance_visible(n), "appearance visible on");

    bsg_appearance_set_force_draw(n, 1);
    CHECK(bsg_appearance_force_draw(n), "force draw");

    bsg_appearance_set_line_style(n, 1);
    CHECK(bsg_appearance_line_style(n), "line style");

    bsg_appearance_set_line_width(n, 4);
    CHECK(bsg_appearance_line_width(n) == 4, "line width");

    bsg_shape_destroy(n);
    free_view(v);
    printf("PASS: test_material_appearance\n");
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
