/*                      D R A W _ C O L O R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "ged.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../../ged_private.h"

using Color = std::array<unsigned char, 3>;
using ExpectedColors = std::map<std::string, Color>;

static bool
write_group(struct rt_wdb *wdbp, const char *name,
    const std::vector<std::string> &children, const unsigned char *color, int inherit)
{
    struct wmember members;
    BU_LIST_INIT(&members.l);
    for (const auto &child : children) {
	if (!mk_addmember(child.c_str(), &members.l, NULL, WMOP_UNION)) {
	    mk_freemembers(&members.l);
	    return false;
	}
    }
    return mk_comb(wdbp, name, &members.l, 0, NULL, NULL, color,
	0, 0, 0, 0, inherit, 0, 0) == 0;
}

static bool
create_fixture(const char *path, ExpectedColors &expected)
{
    std::unique_ptr<struct rt_wdb, decltype(&wdb_close)> database(wdb_fopen(path), wdb_close);
    if (!database)
	return false;

    ON_PlaneSurface surface(ON_xy_plane);
    surface.SetExtents(0, ON_Interval(0.0, 1.0));
    surface.SetExtents(1, ON_Interval(0.0, 1.0));
    ON_Brep brep;
    if (!brep.NewFace(surface) || !brep.IsValid())
	return false;
    fastf_t vertices[] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    int faces[] = {0, 1, 2};

    const Color parent_color = {{20, 40, 60}};
    const struct {
	const char *name;
	const char *attribute;
	bool colored_parent;
	int inherit;
	Color expected;
    } cases[] = {
	{"default", NULL, false, 0, {{255, 0, 0}}},
	{"primitive", "126/137/141", false, 0, {{126, 137, 141}}},
	{"inherited", NULL, true, 1, parent_color},
	{"primitive_parent", "126/137/141", true, 0, {{126, 137, 141}}},
	{"primitive_inherit", "126/137/141", true, 1, {{126, 137, 141}}},
	{"invalid", "invalid", true, 0, parent_color},
	{"negative", "-1/20/30", true, 0, parent_color},
	{"clamped", "999/10/20", false, 0, {{255, 10, 20}}}
    };

    std::vector<std::string> groups;
    for (const auto &test : cases) {
	const std::string brep_name = std::string(test.name) + ".brep";
	const std::string bot_name = std::string(test.name) + ".bot";
	if (mk_brep(database.get(), brep_name.c_str(), &brep) < 0 ||
	    mk_bot(database.get(), bot_name.c_str(), RT_BOT_SURFACE, RT_BOT_CCW, 0,
		3, 1, vertices, faces, NULL, NULL) < 0)
	    return false;
	const std::vector<std::string> children = {brep_name, bot_name};
	for (const auto &child : children) {
	    expected.emplace(child, test.expected);
	    if (test.attribute && db5_update_attribute(child.c_str(), db5_standard_attribute(ATTR_COLOR),
		test.attribute, database->dbip) < 0)
		return false;
	}
	if (!write_group(database.get(), test.name, children,
	    test.colored_parent ? parent_color.data() : NULL, test.inherit))
	    return false;
	groups.emplace_back(test.name);
    }
    return write_group(database.get(), "all", groups, NULL, 0);
}

static bool
check_drawing(struct ged *gedp, const char *mode, const ExpectedColors &expected, bool override_color)
{
    const char *zap[] = {"zap", NULL};
    if (ged_exec_zap(gedp, 1, zap) != BRLCAD_OK)
	return false;

    const Color override_rgb = {{9, 80, 150}};
    const char *draw[] = {"draw", "-m", mode, "all", NULL};
    const char *draw_override[] = {"draw", "-m", mode, "-C", "9/80/150", "all", NULL};
    if ((override_color ? ged_exec_draw(gedp, 6, draw_override) : ged_exec_draw(gedp, 4, draw)) != BRLCAD_OK) {
	bu_log("draw mode %s failed: %s\n", mode, bu_vls_cstr(gedp->ged_result_str));
	return false;
    }

    ExpectedColors remaining = expected;
    bool passed = true;
    struct display_list *display;
    for (BU_LIST_FOR(display, display_list, gedp->i->ged_gdp->gd_headDisplay)) {
	struct bv_scene_obj *object;
	for (BU_LIST_FOR(object, bv_scene_obj, &display->dl_head_scene_obj)) {
	    const auto *data = static_cast<const struct ged_bv_data *>(object->s_u_data);
	    if (!data || data->s_fullpath.fp_len == 0)
		return false;
	    const char *name = DB_FULL_PATH_CUR_DIR(&data->s_fullpath)->d_namep;
	    const auto found = remaining.find(name);
	    if (found == remaining.end()) {
		bu_log("unexpected or duplicate drawn object: %s\n", name);
		return false;
	    }
	    const Color &color = override_color ? override_rgb : found->second;
	    if (BU_LIST_IS_EMPTY(&object->s_vlist) ||
		!std::equal(color.begin(), color.end(), object->s_color)) {
		bu_log("%s, mode %s, override %d: expected %d/%d/%d, got %d/%d/%d\n",
		    name, mode, override_color,
		    color[0], color[1], color[2], object->s_color[0], object->s_color[1], object->s_color[2]);
		passed = false;
	    }
	    remaining.erase(found);
	}
    }
    if (!remaining.empty())
	bu_log("draw mode %s omitted %zu fixture objects\n", mode, remaining.size());
    return passed && remaining.empty();
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;
    char path[MAXPATHLEN] = {0};
    FILE *temporary = bu_temp_file(path, sizeof(path));
    if (!temporary)
	return 1;
    if (std::fclose(temporary)) {
	bu_file_delete(path);
	return 1;
    }

    ON::Begin();
    ExpectedColors expected;
    bool passed = create_fixture(path, expected);
    std::unique_ptr<struct ged, decltype(&ged_close)> context(
	passed ? ged_open("db", path, 1) : NULL, ged_close);
    if (context) {
	// Inspect actual legacy display-list colors without a window or image
	// comparison.  Differently colored siblings also detect state leakage.
	context->new_cmd_forms = 0;
	for (const char *mode : {"0", "1", "2"})
	    for (bool override_color : {false, true})
		passed = check_drawing(context.get(), mode, expected, override_color) && passed;
    } else {
	bu_log("could not create or open drawing color fixture\n");
	passed = false;
    }
    context.reset();
    ON::End();
    bu_file_delete(path);
    return passed ? 0 : 1;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
