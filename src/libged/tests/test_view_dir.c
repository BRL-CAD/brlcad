/*                   T E S T _ V I E W _ D I R . C
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

#include "common.h"

#include "bu.h"
#include "ged.h"


static int
same_result(const char *label, struct ged *gedp, const char *expected)
{
    const char *result = bu_vls_cstr(gedp->ged_result_str);
    if (BU_STR_EQUAL(result, expected))
	return 1;

    bu_log("%s: expected <%s>, got <%s>\n", label, expected, result);
    return 0;
}


int
main(int argc, char *argv[])
{
    struct ged view_ged;
    struct ged qvrot_ged;
    const char *view_get[3] = {"view", "dir", NULL};
    const char *view_get_inverse[4] = {"view", "dir", "-i", NULL};
    const char *view_set[7] = {"view", "dir", "1", "2", "3", "25", NULL};
    const char *view_set_inverse[8] = {"view", "dir", "-i", "-1", "-2", "-3", "-25", NULL};
    const char *viewdir_get[2] = {"viewdir", NULL};
    const char *viewdir_get_inverse[3] = {"viewdir", "-i", NULL};
    const char *qvrot_set[6] = {"qvrot", "1", "2", "3", "25", NULL};
    char *direction = NULL;
    char *inverse_direction = NULL;
    int ret = 0;

    bu_setprogname(argv[0]);
    if (argc != 1) {
	bu_log("Usage: %s\n", argv[0]);
	return 1;
    }

    ged_init(&view_ged);
    ged_init(&qvrot_ged);
    view_ged.new_cmd_forms = 1;

    if (ged_exec_view(&view_ged, 2, view_get) != BRLCAD_OK ||
	ged_exec_viewdir(&qvrot_ged, 1, viewdir_get) != BRLCAD_OK ||
	!same_result("view dir query", &view_ged, bu_vls_cstr(qvrot_ged.ged_result_str))) {
	ret = 1;
	goto done;
    }

    if (ged_exec_view(&view_ged, 6, view_set) != BRLCAD_OK ||
	ged_exec_qvrot(&qvrot_ged, 5, qvrot_set) != BRLCAD_OK ||
	ged_exec_view(&view_ged, 2, view_get) != BRLCAD_OK) {
	ret = 1;
	goto done;
    }
    direction = bu_strdup(bu_vls_cstr(view_ged.ged_result_str));

    if (ged_exec_viewdir(&qvrot_ged, 1, viewdir_get) != BRLCAD_OK ||
	!same_result("view dir setter", &view_ged, bu_vls_cstr(qvrot_ged.ged_result_str))) {
	ret = 1;
	goto done;
    }

    if (ged_exec_view(&view_ged, 3, view_get_inverse) != BRLCAD_OK ||
	ged_exec_viewdir(&qvrot_ged, 2, viewdir_get_inverse) != BRLCAD_OK ||
	!same_result("view dir -i query", &view_ged, bu_vls_cstr(qvrot_ged.ged_result_str))) {
	ret = 1;
	goto done;
    }
    inverse_direction = bu_strdup(bu_vls_cstr(view_ged.ged_result_str));

    if (ged_exec_view(&view_ged, 7, view_set_inverse) != BRLCAD_OK ||
	ged_exec_view(&view_ged, 2, view_get) != BRLCAD_OK ||
	!same_result("view dir -i setter", &view_ged, direction)) {
	ret = 1;
	goto done;
    }

    if (ged_exec_view(&view_ged, 3, view_get_inverse) != BRLCAD_OK ||
	!same_result("view dir -i setter inverse", &view_ged, inverse_direction)) {
	ret = 1;
    }

done:
    if (direction)
	bu_free(direction, "view direction");
    if (inverse_direction)
	bu_free(inverse_direction, "inverse view direction");
    ged_free(&view_ged);
    ged_free(&qvrot_ged);
    return ret;
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
