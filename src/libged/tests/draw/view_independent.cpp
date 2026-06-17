/*         V I E W _ I N D E P E N D E N T . C P P
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
/** @file view_independent.cpp
 *
 * Phase V6 regression test: `view independent` is backed by a private
 * structural BSG scope rather than the legacy mode bit alone.
 */

#include "common.h"

#include <algorithm>
#include <string>
#include <vector>

#include <bu.h>
#include <bsg.h>
#include <bsg/lod.h>
#include <ged.h>
#include "ged/bsg_ged_draw.h"

#define ASSERT(cond) do { \
    nchecks++; \
    if (!(cond)) { \
	bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
	nfails++; \
    } \
} while (0)

static int nchecks = 0;
static int nfails = 0;

static std::vector<std::string>
drawn_paths(struct ged *gedp, struct bsg_view *v)
{
    std::vector<std::string> ret;
    struct bu_vls paths = BU_VLS_INIT_ZERO;
    ged_draw_list_paths(gedp, v, -1, 0, &paths);

    const char *s = bu_vls_cstr(&paths);
    const char *start = s;
    for (const char *p = s; p && *p; p++) {
	if (*p != '\n')
	    continue;
	ret.push_back(std::string(start, p - start));
	start = p + 1;
    }
    if (start && *start)
	ret.push_back(std::string(start));

    bu_vls_free(&paths);
    return ret;
}

static int
has_path(const std::vector<std::string> &paths, const char *path)
{
    return (std::find(paths.begin(), paths.end(), std::string(path)) != paths.end()) ? 1 : 0;
}

static std::string
who_view(struct ged *gedp, const char *view_name)
{
    const char *av[4] = {"who", "-V", view_name, NULL};
    bu_vls_trunc(gedp->ged_result_str, 0);
    ASSERT(ged_exec_who(gedp, 3, av) == BRLCAD_OK);
    return std::string(bu_vls_cstr(gedp->ged_result_str));
}

static int
who_has_path(const std::string &who, const char *path)
{
    size_t start = 0;
    while (start < who.size()) {
	size_t end = who.find('\n', start);
	if (end == std::string::npos)
	    end = who.size();
	if (who.compare(start, end - start, path) == 0)
	    return 1;
	start = end + 1;
    }
    return 0;
}

static int
set_view_independent(struct ged *gedp, const char *view_name, int independent)
{
    const char *av[5] = {"view", "independent", view_name, independent ? "1" : "0", NULL};
    return ged_exec_view(gedp, 4, av);
}

static int
draw_shared(struct ged *gedp, const char *path)
{
    const char *av[4] = {"draw", "-R", path, NULL};
    return ged_exec_draw(gedp, 3, av);
}

static int
draw_shared_autoview(struct ged *gedp, const char *path)
{
    const char *av[3] = {"draw", path, NULL};
    return ged_exec_draw(gedp, 2, av);
}

static int
draw_view(struct ged *gedp, const char *view_name, const char *path)
{
    const char *av[6] = {"draw", "-R", "-V", view_name, path, NULL};
    return ged_exec_draw(gedp, 5, av);
}

static int
redraw_view(struct ged *gedp, const char *view_name)
{
    const char *av[4] = {"redraw", "-V", view_name, NULL};
    return ged_exec_redraw(gedp, 3, av);
}

static int
erase_view(struct ged *gedp, const char *view_name, const char *path)
{
    const char *av[5] = {"erase", "-V", view_name, path, NULL};
    return ged_exec_erase(gedp, 4, av);
}

static int
erase_current(struct ged *gedp, const char *path)
{
    const char *av[3] = {"erase", path, NULL};
    return ged_exec_erase(gedp, 2, av);
}

static int
zap_view_db(struct ged *gedp, const char *view_name)
{
    const char *av[5] = {"zap", "-V", view_name, "-g", NULL};
    return ged_exec_zap(gedp, 4, av);
}

static int
zap_current(struct ged *gedp)
{
    const char *av[2] = {"zap", NULL};
    return ged_exec_zap(gedp, 1, av);
}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);

    if (argc != 2)
	bu_exit(EXIT_FAILURE, "Usage: ged_test_view_independent <directory-containing-moss.g>\n");

    struct bu_vls gpath = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&gpath, "%s/moss.g", argv[1]);

    struct ged *gedp = ged_open("db", bu_vls_cstr(&gpath), 1);
    ASSERT(gedp != NULL);
    if (!gedp)
	return EXIT_FAILURE;

    bsg_set_rm_view(&gedp->ged_views, NULL);
    struct bsg_view *views[2] = {NULL, NULL};
    for (int i = 0; i < 2; i++) {
	BU_GET(views[i], struct bsg_view);
	bsg_init(views[i], &gedp->ged_views);
	bu_vls_sprintf(&views[i]->gv_name, "V%d", i);
	bsg_set_add_view(&gedp->ged_views, views[i]);
	bu_ptbl_ins(&gedp->ged_free_views, (long *)views[i]);
	if (!i)
	    gedp->ged_gvp = views[i];
    }

    for (int i = 0; i < 2; i++) {
	views[i]->gv_size = 1.0e9;
	views[i]->gv_scale = 1.0e9;
    }
    ASSERT(draw_shared_autoview(gedp, "all.g") == BRLCAD_OK);
    ASSERT(views[0]->gv_bounds_update == &bsg_view_bounds);
    ASSERT(views[1]->gv_bounds_update == &bsg_view_bounds);
    ASSERT(views[0]->gv_size < 1.0e8);
    ASSERT(views[1]->gv_size < 1.0e8);
    ASSERT(zap_current(gedp) == BRLCAD_OK);
    ASSERT(drawn_paths(gedp, views[0]).size() == 0);
    ASSERT(drawn_paths(gedp, views[1]).size() == 0);

    ASSERT(draw_shared(gedp, "all.g") == BRLCAD_OK);
    ASSERT(!bsg_view_is_independent(views[0]));
    ASSERT(drawn_paths(gedp, views[0]).size() == 1);
    ASSERT(has_path(drawn_paths(gedp, views[0]), "all.g"));
    ASSERT(drawn_paths(gedp, views[1]).size() == 1);

    ASSERT(set_view_independent(gedp, "V0", 1) == BRLCAD_OK);
    ASSERT(bsg_view_is_independent(views[0]));
    ASSERT(!bsg_scene_ref_is_null(bsg_view_independent_scope_ref(views[0], 0)));
    ASSERT(drawn_paths(gedp, views[0]).size() == 1);
    ASSERT(has_path(drawn_paths(gedp, views[0]), "all.g"));

    ASSERT(draw_shared(gedp, "box.r") == BRLCAD_OK);
    ASSERT(drawn_paths(gedp, views[1]).size() == 2);
    ASSERT(has_path(drawn_paths(gedp, views[1]), "box.r"));
    ASSERT(drawn_paths(gedp, views[0]).size() == 1);
    ASSERT(!has_path(drawn_paths(gedp, views[0]), "box.r"));
    ASSERT(who_has_path(who_view(gedp, "V1"), "box.r"));
    ASSERT(!who_has_path(who_view(gedp, "V0"), "box.r"));

    ASSERT(draw_view(gedp, "V0", "tor.r") == BRLCAD_OK);
    ASSERT(drawn_paths(gedp, views[0]).size() == 2);
    ASSERT(has_path(drawn_paths(gedp, views[0]), "tor.r"));
    ASSERT(!has_path(drawn_paths(gedp, views[1]), "tor.r"));
    ASSERT(who_has_path(who_view(gedp, "V0"), "tor.r"));
    ASSERT(!who_has_path(who_view(gedp, "V1"), "tor.r"));
    ASSERT(redraw_view(gedp, "V0") == BRLCAD_OK);
    ASSERT(has_path(drawn_paths(gedp, views[0]), "tor.r"));
    ASSERT(!has_path(drawn_paths(gedp, views[1]), "tor.r"));
    ASSERT(erase_view(gedp, "V0", "tor.r") == BRLCAD_OK);
    ASSERT(!has_path(drawn_paths(gedp, views[0]), "tor.r"));
    ASSERT(has_path(drawn_paths(gedp, views[0]), "all.g"));
    ASSERT(has_path(drawn_paths(gedp, views[1]), "box.r"));

    ASSERT(draw_view(gedp, "V0", "tor.r") == BRLCAD_OK);
    ASSERT(has_path(drawn_paths(gedp, views[0]), "tor.r"));
    ASSERT(zap_view_db(gedp, "V0") == BRLCAD_OK);
    ASSERT(drawn_paths(gedp, views[0]).size() == 0);
    ASSERT(has_path(drawn_paths(gedp, views[1]), "all.g"));
    ASSERT(has_path(drawn_paths(gedp, views[1]), "box.r"));
    ASSERT(!has_path(drawn_paths(gedp, views[1]), "tor.r"));

    ASSERT(set_view_independent(gedp, "V0", 0) == BRLCAD_OK);
    ASSERT(!bsg_view_is_independent(views[0]));
    ASSERT(bsg_scene_ref_is_null(bsg_view_independent_scope_ref(views[0], 0)));
    ASSERT(drawn_paths(gedp, views[0]).size() == 2);
    ASSERT(has_path(drawn_paths(gedp, views[0]), "all.g"));
    ASSERT(has_path(drawn_paths(gedp, views[0]), "box.r"));
    ASSERT(!has_path(drawn_paths(gedp, views[0]), "tor.r"));
    ASSERT(erase_current(gedp, "box.r") == BRLCAD_OK);
    ASSERT(!has_path(drawn_paths(gedp, views[0]), "box.r"));
    ASSERT(!has_path(drawn_paths(gedp, views[1]), "box.r"));
    ASSERT(has_path(drawn_paths(gedp, views[0]), "all.g"));
    ASSERT(zap_current(gedp) == BRLCAD_OK);
    ASSERT(drawn_paths(gedp, views[0]).size() == 0);
    ASSERT(drawn_paths(gedp, views[1]).size() == 0);

    bu_vls_free(&gpath);
    ged_close(gedp);

    bu_log("view_independent: %d checks, %d failures\n", nchecks, nfails);
    return nfails ? EXIT_FAILURE : EXIT_SUCCESS;
}
