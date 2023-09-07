/*                       D P L O T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2010-2023 United States Government as represented by
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
/**
 *
 */
#include "common.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "bu/defines.h"
#include "bu/vls.h"

#include "../ged_private.h"
#include "./ged_brep.h"

enum {
    DPLOT_INITIAL,
    DPLOT_SSX_FIRST,
    DPLOT_SSX,
    DPLOT_SSX_EVENTS,
    DPLOT_ISOCSX_FIRST,
    DPLOT_ISOCSX,
    DPLOT_ISOCSX_EVENTS,
    DPLOT_FACE_CURVES,
    DPLOT_LINKED_CURVES,
    DPLOT_SPLIT_FACES
};

struct ssx {
    int brep1_surface;
    int brep2_surface;
    int final_curve_events;
    int face1_clipped_curves;
    int face2_clipped_curves;
    int intersecting_brep1_isocurves;
    int intersecting_isocurves;
    std::vector<int> isocsx;
};

struct split_face {
    int outerloop_curves;
    int innerloop_curves;
};

struct dplot_data {
    int brep1_surface_count;
    int brep2_surface_count;
    int linked_curve_count;
    int ssx_count;
    int split_face_count;
    std::vector<struct ssx *> ssx;
    struct split_face *face;
};

struct dplot_info {
    struct dplot_data fdata;
    std::string logfile;
    struct ged *gedp;
    int mode;
    char *prefix;
    int ssx_idx;
    int isocsx_idx;
    int brep1_surf_idx;
    int brep2_surf_idx;
    int brep1_surf_count;
    int brep2_surf_count;
    int event_idx;
    int event_count;
    int brep1_isocsx_count;
    int isocsx_count;
};

static void
process_surfaces(struct dplot_data *data, std::vector<std::string> &lv)
{
    if (!data)
	return;
    if (lv.size() != 3) {
	std::cerr << "malformed surfaces line\n";
	return;
    }

    data->brep1_surface_count = std::stoi(lv[1]);
    data->brep2_surface_count = std::stoi(lv[2]);
}

static void
process_ssx(struct dplot_data *data, std::vector<std::string> &lv)
{
    if (!data)
	return;
    if (lv.size() < 8) {
    	std::cerr << "malformed ssx line\n";
	return;
    }

    struct ssx *ssx;
    BU_GET(ssx, struct ssx);

    ssx->brep1_surface = std::stoi(lv[1]);
    ssx->brep2_surface = std::stoi(lv[2]);
    ssx->final_curve_events = std::stoi(lv[3]);
    ssx->face1_clipped_curves = std::stoi(lv[4]);
    ssx->face2_clipped_curves = std::stoi(lv[5]);
    ssx->intersecting_brep1_isocurves = std::stoi(lv[6]);
    ssx->intersecting_isocurves = std::stoi(lv[7]);

    for (size_t i = 8; i < lv.size(); i++) {
	int isocsx = std::stoi(lv[i]);
	ssx->isocsx.push_back(isocsx);
    }

    data->ssx.push_back(ssx);
}

static void
process_linkedcurves(struct dplot_data *data, std::vector<std::string> &lv)
{
    if (!data)
	return;
    if (lv.size() != 2) {
	std::cerr << "malformed linkedcurves line\n";
	return;
    }
    data->linked_curve_count = std::stoi(lv[1]);
}

static void
process_splitfaces(struct dplot_data *data, std::vector<std::string> &lv)
{
    if (!data)
	return;

    if (lv.size() != 2) {
	std::cerr << "malformed splitfaces line\n";
	return;
    }

    data->split_face_count = std::stoi(lv[1]);
    data->face = (struct split_face *)bu_malloc(data->split_face_count * sizeof(struct split_face), "split face array");
}

static void
process_splitface(struct dplot_data *data, std::vector<std::string> &lv)
{
    if (!data)
	return;

    if (lv.size() != 4) {
	std::cerr << "malformed splitface line\n";
	return;
    }

    int i = std::stoi(lv[1]);
    data->face[i].outerloop_curves = std::stoi(lv[2]);
    data->face[i].innerloop_curves = std::stoi(lv[3]);
}

int
dplot_load_file_data(struct dplot_info *info)
{
    if (!info || !info->logfile.size())
	return BRLCAD_ERROR;

    struct ged *gedp = info->gedp;

    std::ifstream infile(info->logfile);
    if (!infile.is_open()) {
	bu_vls_printf(gedp->ged_result_str, "Error: unable to open %s for reading.\n", info->logfile.c_str());
	return BRLCAD_ERROR;
    }

    info->fdata.brep1_surface_count = info->fdata.brep2_surface_count = 0;
 
    std::string line;
    while (std::getline(infile, line)) {
	std::string c;
	std::vector<std::string> lv;
	std::stringstream sl(line);
	while (std::getline(sl, c, ' ')) {
	    lv.push_back(c);
	}

	if (lv[0] == std::string("surfaces")) {
	    process_surfaces(&info->fdata, lv);
	    continue;
	}

	if (lv[0] == std::string("ssx")) {
	    process_ssx(&info->fdata, lv);
	    continue;
	}
	if (lv[0] == std::string("linkedcurves")) {
	    process_linkedcurves(&info->fdata, lv);
	    continue;
	}
	if (lv[0] == std::string("splitfaces")) {
	    process_splitfaces(&info->fdata, lv);
	    continue;
	}
	if (lv[0] == std::string("splitface")) {
	    process_splitface(&info->fdata, lv);
	    continue;
	}

	std::cerr << "Unknown dplot key: " << lv[0] << "\n";
    }

    infile.close();

#if 0
    // Cleanup
    for (size_t i = 0; i < data.ssx.size(); i++) {
	bu_free(data.ssx[i], "ssx");
    }
#endif

    return BRLCAD_OK;
}

#define CLEANUP \
    bu_free(info.prefix, "prefix"); \
    info.prefix = NULL; \
    for (size_t i = 0; i < info.fdata.ssx.size(); i++) bu_free(info.fdata.ssx[i], "ssx entry");

#define RETURN_MORE \
    CLEANUP \
    return GED_MORE;

#define RETURN_ERROR \
    CLEANUP \
    info.mode = DPLOT_INITIAL; \
    return BRLCAD_ERROR;

static int
dplot_overlay(
	struct ged *gedp,
	const char *prefix,
	const char *infix,
	int idx,
	const char *name)
{
    const char *cmd_av[] = {"overlay", "[filename]", "1.0", "[name]"};
    int ret, cmd_ac = sizeof(cmd_av) / sizeof(char *);
    struct bu_vls overlay_name = BU_VLS_INIT_ZERO;

    bu_vls_printf(&overlay_name, "%s%s%d.plot3", prefix, infix, idx);
    cmd_av[1] = cmd_av[3] = bu_vls_cstr(&overlay_name);
    if (name) {
	cmd_av[3] = name;
    }
    ret = ged_exec(gedp, cmd_ac, cmd_av);
    bu_vls_free(&overlay_name);

    if (ret != BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str, "error overlaying plot\n");
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
dplot_erase_overlay(
	struct dplot_info *info,
	const char *name)
{
    const int NUM_EMPTY_PLOTS = 13;
    int i;

    /* We can't actually erase the old plot without its real name,
     * which is unknown. Instead, we'll write a plot with the same
     * base name and color, which will overwrite the old one. We
     * don't actually know the color either, so we resort to writing
     * an empty plot with the given name using every color we created
     * plots with.
     */
    for (i = 0; i < NUM_EMPTY_PLOTS; ++i) {
	int ret = dplot_overlay(info->gedp, info->prefix, "_empty", i, name);
	if (ret != BRLCAD_OK) {
	    return ret;
	}
    }
    return BRLCAD_OK;
}

static int
dplot_ssx(
	struct dplot_info *info)
{
    int i, ret;

    /* draw surfaces, skipping intersecting surfaces if in SSI mode */
    /* TODO: need to name these overlays so I can selectively erase them */
    for (i = 0; i < info->brep1_surf_count; ++i) {
	struct bu_vls name = BU_VLS_INIT_ZERO;
	bu_vls_printf(&name, "%s_brep1_surface%d", info->prefix, i);
	dplot_erase_overlay(info, bu_vls_cstr(&name));
	bu_vls_free(&name);
    }
    for (i = 0; i < info->brep2_surf_count; ++i) {
	struct bu_vls name = BU_VLS_INIT_ZERO;
	bu_vls_printf(&name, "%s_brep2_surface%d", info->prefix, i);
	dplot_erase_overlay(info, bu_vls_cstr(&name));
	bu_vls_free(&name);
    }
    if (info->mode == DPLOT_SSX_FIRST || info->mode == DPLOT_SSX || info->mode == DPLOT_SSX_EVENTS) {
	for (i = 0; i < info->brep1_surf_count; ++i) {
	    if (info->mode != DPLOT_SSX_FIRST && i == info->brep1_surf_idx) {
		continue;
	    }
	    ret = dplot_overlay(info->gedp, info->prefix, "_brep1_surface", i, NULL);
	    if (ret != BRLCAD_OK) {
		return BRLCAD_ERROR;
	    }
	}
	for (i = 0; i < info->brep2_surf_count; ++i) {
	    if (info->mode != DPLOT_SSX_FIRST && i == info->brep2_surf_idx) {
		continue;
	    }
	    ret = dplot_overlay(info->gedp, info->prefix, "_brep2_surface", i, NULL);
	    if (ret != BRLCAD_OK) {
		return BRLCAD_ERROR;
	    }
	}
    }

    /* draw highlighted surface-surface intersection pair */
    if (info->mode == DPLOT_SSX || info->mode == DPLOT_SSX_EVENTS) {
	ret = dplot_overlay(info->gedp, info->prefix, "_highlight_brep1_surface",
		info->brep1_surf_idx, "dplot_ssx1");
	if (ret != BRLCAD_OK) {
	    return BRLCAD_ERROR;
	}
	ret = dplot_overlay(info->gedp, info->prefix, "_highlight_brep2_surface",
		info->brep2_surf_idx, "dplot_ssx2");
	if (ret != BRLCAD_OK) {
	    return BRLCAD_ERROR;
	}
	if (info->mode == DPLOT_SSX) {
	    /* advance past the completed pair */
	    ++info->ssx_idx;
	}
    }
    if (info->mode == DPLOT_SSX_FIRST) {
	info->mode = DPLOT_SSX;
    }
    if (info->mode == DPLOT_SSX && info->ssx_idx < info->fdata.ssx_count) {
	bu_vls_printf(info->gedp->ged_result_str, "Press [Enter] to show surface-"
		"surface intersection %d", info->ssx_idx);
	return GED_MORE;
    }
    return BRLCAD_OK;
}

void
dplot_print_event_legend(struct dplot_info *info)
{
    bu_vls_printf(info->gedp->ged_result_str, "yellow = transverse\n");
    bu_vls_printf(info->gedp->ged_result_str, "white  = tangent\n");
    bu_vls_printf(info->gedp->ged_result_str, "green  = overlap\n");
}

static int
dplot_ssx_events(
	struct dplot_info *info)
{
    int ret;

    /* erase old event plots */
    ret = dplot_erase_overlay(info, "curr_event");
    if (ret != BRLCAD_OK) {
	return ret;
    }

    if (info->mode != DPLOT_SSX_EVENTS) {
	return BRLCAD_OK;
    }

    if (info->event_count > 0) {
	/* convert event ssx_idx to string */
	struct bu_vls infix = BU_VLS_INIT_ZERO;
	bu_vls_printf(&infix, "_ssx%d_event", info->ssx_idx);

	/* plot overlay */
	ret = dplot_overlay(info->gedp, info->prefix, bu_vls_cstr(&infix),
		info->event_idx, "curr_event");
	bu_vls_free(&infix);

	if (ret != BRLCAD_OK) {
	    return ret;
	}
	if (info->event_idx == 0) {
	    dplot_print_event_legend(info);
	}
    }
    /* advance to next event, or return to initial state */
    if (++info->event_idx < info->event_count) {
	bu_vls_printf(info->gedp->ged_result_str, "Press [Enter] to show next event\n");
	return GED_MORE;
    }
    info->mode = DPLOT_INITIAL;
    return BRLCAD_OK;
}

static int
dplot_isocsx(
	struct dplot_info *info)
{
    if (info->mode != DPLOT_ISOCSX &&
	    info->mode != DPLOT_ISOCSX_FIRST &&
	    info->mode != DPLOT_ISOCSX_EVENTS)
    {
	return BRLCAD_OK;
    }

    if (info->fdata.ssx[info->ssx_idx].isocsx_events == NULL) {
	bu_vls_printf(info->gedp->ged_result_str, "The isocurves of neither "
		"surface intersected the opposing surface in surface-surface"
		" intersection %d.\n", info->ssx_idx);
	info->mode = DPLOT_INITIAL;
	return BRLCAD_OK;
    }

    dplot_overlay(info->gedp, info->prefix, "_brep1_surface",
	    info->brep1_surf_idx, "isocsx_b1");
    dplot_overlay(info->gedp, info->prefix, "_brep2_surface",
	    info->brep2_surf_idx, "isocsx_b2");

    if (info->mode == DPLOT_ISOCSX) {
	struct bu_vls infix = BU_VLS_INIT_ZERO;
	/* plot surface and the isocurve that intersects it */
	if (info->isocsx_idx < info->brep1_isocsx_count) {
	    dplot_overlay(info->gedp, info->prefix, "_highlight_brep2_surface",
		    info->brep2_surf_idx, "isocsx_b2");
	} else {
	    dplot_overlay(info->gedp, info->prefix, "_highlight_brep1_surface",
		    info->brep1_surf_idx, "isocsx_b1");
	}
	bu_vls_printf(&infix, "_highlight_ssx%d_isocurve", info->ssx_idx);
	dplot_overlay(info->gedp, info->prefix, bu_vls_cstr(&infix), info->isocsx_idx, "isocsx_isocurve");
	bu_vls_free(&infix);
    }

    if (info->mode == DPLOT_ISOCSX_FIRST ||
	    info->mode == DPLOT_ISOCSX)
    {
	if (info->mode == DPLOT_ISOCSX_FIRST ||
		++info->isocsx_idx < info->isocsx_count)
	{
	    bu_vls_printf(info->gedp->ged_result_str, "Press [Enter] to show "
		    "isocurve-surface intersection %d", info->isocsx_idx);
	    info->mode = DPLOT_ISOCSX;
	    return GED_MORE;
	} else {
	    info->mode = DPLOT_INITIAL;
	}
    }
    return BRLCAD_OK;
}

static int
dplot_isocsx_events(struct dplot_info *info)
{
    int ret;

    if (info->mode != DPLOT_ISOCSX_EVENTS) {
	return BRLCAD_OK;
    }
    ret = dplot_erase_overlay(info, "curr_event");
    if (ret != BRLCAD_OK) {
	return ret;
    }
    if (info->event_count > 0) {
	/* convert event ssx_idx to string */
	struct bu_vls infix = BU_VLS_INIT_ZERO;
	bu_vls_printf(&infix, "_ssx%d_isocsx%d_event", info->ssx_idx,
		info->isocsx_idx);

	/* plot overlay */
	ret = dplot_overlay(info->gedp, info->prefix, bu_vls_cstr(&infix),
		info->event_idx, "curr_event");
	bu_vls_free(&infix);

	if (ret != BRLCAD_OK) {
	    bu_vls_printf(info->gedp->ged_result_str,
		    "error overlaying plot\n");
	    return ret;
	}
	if (info->event_idx == 0) {
	    dplot_print_event_legend(info);
	}
    }
    /* advance to next event, or return to initial state */
    if (++info->event_idx < info->event_count) {
	bu_vls_printf(info->gedp->ged_result_str,
		"Press [Enter] to show next event\n");
	return GED_MORE;
    }

    info->mode = DPLOT_INITIAL;
    return BRLCAD_OK;
}

static int
dplot_face_curves(struct dplot_info *info)
{
    int f1_curves, f2_curves;
    if (info->mode != DPLOT_FACE_CURVES) {
	return BRLCAD_OK;
    }

    f1_curves = info->fdata.ssx[info->ssx_idx]->face1_clipped_curves;
    f2_curves = info->fdata.ssx[info->ssx_idx]->face2_clipped_curves;
    info->event_count = f1_curves + f2_curves;

    if (info->event_count == 0) {
	bu_vls_printf(info->gedp->ged_result_str, "No clipped curves for ssx"
		" pair %d.\n", info->ssx_idx);
	return BRLCAD_OK;
    }

    if (info->event_idx < info->event_count) {
	struct bu_vls prefix;

	dplot_overlay(info->gedp, info->prefix, "_brep1_surface",
		info->brep1_surf_idx, "face_b1");
	dplot_overlay(info->gedp, info->prefix, "_brep2_surface",
		info->brep2_surf_idx, "face_b2");

	BU_VLS_INIT(&prefix);
	bu_vls_printf(&prefix, "%s_ssx%d", info->prefix, info->ssx_idx);
	dplot_erase_overlay(info, "clipped_fcurve");
	if (info->event_idx < f1_curves) {
	    bu_vls_printf(&prefix, "_brep1face_clipped_curve");
	    dplot_overlay(info->gedp, bu_vls_cstr(&prefix), "",
		    info->event_idx, "clipped_fcurve");
	} else {
	    bu_vls_printf(&prefix, "_brep2face_clipped_curve");
	    dplot_overlay(info->gedp, bu_vls_cstr(&prefix), "",
		    info->event_idx - f1_curves, "clipped_fcurve");
	}
	++info->event_idx;
	if (info->event_idx < info->event_count) {
	    bu_vls_printf(info->gedp->ged_result_str, "Press [Enter] to show the"
		    " next curve.");
	    return GED_MORE;
	}
    }

    info->mode = DPLOT_INITIAL;
    return BRLCAD_OK;
}

static int
dplot_split_faces(
	struct dplot_info *info)
{
    int i, j;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_vls short_name = BU_VLS_INIT_ZERO;
    struct split_face split_face;

    if (info->mode != DPLOT_SPLIT_FACES) {
	return BRLCAD_OK;
    }

    if (info->event_idx >= info->fdata.split_face_count) {
	for (i = 0; i < info->fdata.split_face_count; ++i) {
	    split_face = info->fdata.face[i];

	    bu_vls_trunc(&name, 0);
	    bu_vls_printf(&name, "_split_face%d_outerloop_curve", i);
	    for (j = 0; j < split_face.outerloop_curves; ++j) {
		bu_vls_trunc(&short_name, 0);
		bu_vls_printf(&short_name, "sf%do%d", i, j);
		dplot_overlay(info->gedp, info->prefix, bu_vls_cstr(&name), j,
			bu_vls_cstr(&short_name));
	    }

	    bu_vls_trunc(&name, 0);
	    bu_vls_printf(&name, "_split_face%d_innerloop_curve", i);
	    for (j = 0; j < split_face.innerloop_curves; ++j) {
		bu_vls_trunc(&short_name, 0);
		bu_vls_printf(&short_name, "sf%di%d", i, j);
		dplot_overlay(info->gedp, info->prefix, bu_vls_cstr(&name), j,
			bu_vls_cstr(&short_name));
	    }
	}
    } else {
	if (info->event_idx > 0) {
	    /* erase curves of previous split face */
	    split_face = info->fdata.face[info->event_idx - 1];
	    for (i = 0; i < split_face.outerloop_curves; ++i) {
		bu_vls_trunc(&short_name, 0);
		bu_vls_printf(&short_name, "sfo%d", i);
		dplot_erase_overlay(info, bu_vls_cstr(&short_name));
	    }
	    for (i = 0; i < split_face.innerloop_curves; ++i) {
		bu_vls_trunc(&short_name, 0);
		bu_vls_printf(&short_name, "sfi%d", i);
		dplot_erase_overlay(info, bu_vls_cstr(&short_name));
	    }
	}

	split_face = info->fdata.face[info->event_idx];
	bu_vls_printf(&name, "_split_face%d_outerloop_curve", info->event_idx);
	for (i = 0; i < split_face.outerloop_curves; ++i) {
	    bu_vls_trunc(&short_name, 0);
	    bu_vls_printf(&short_name, "sfo%d", i);

	    dplot_erase_overlay(info, bu_vls_cstr(&short_name));
	    dplot_overlay(info->gedp, info->prefix, bu_vls_cstr(&name), i,
		    bu_vls_cstr(&short_name));
	}

	bu_vls_trunc(&name, 0);
	bu_vls_printf(&name, "_split_face%d_innerloop_curve", info->event_idx);
	for (i = 0; i < split_face.innerloop_curves; ++i) {
	    bu_vls_trunc(&short_name, 0);
	    bu_vls_printf(&short_name, "sfi%d", i);

	    dplot_erase_overlay(info, bu_vls_cstr(&short_name));
	    dplot_overlay(info->gedp, info->prefix, bu_vls_cstr(&name), i,
		    bu_vls_cstr(&short_name));
	}

	bu_vls_printf(info->gedp->ged_result_str, "Press [Enter] to show "
		"split face %d", ++info->event_idx);
	return GED_MORE;
    }
    return BRLCAD_OK;
}

static int
dplot_linked_curves(
	struct dplot_info *info)
{
    int i;
    if (info->mode != DPLOT_LINKED_CURVES) {
	return BRLCAD_OK;
    }

    if (info->event_idx >= info->fdata.linked_curve_count) {
	for (i = 0; i < info->fdata.linked_curve_count; ++i) {
	    dplot_overlay(info->gedp, info->prefix, "_linked_curve", i, NULL);
	}
    } else {
	dplot_overlay(info->gedp, info->prefix, "_linked_curve",
		info->event_idx, "linked_curve");
	bu_vls_printf(info->gedp->ged_result_str, "Press [Enter] to show "
		"linked curve %d", ++info->event_idx);
	return GED_MORE;
    }
    return BRLCAD_OK;
}


int
ged_dplot_core(struct ged *gedp, int argc, const char *argv[])
{
    static struct dplot_info info;
    int ret;
    const char *filename, *cmd;
    char *dot;

    info.gedp = gedp;
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "usage: %s logfile cmd\n",
		argv[0]);
	bu_vls_printf(gedp->ged_result_str, "  where cmd is one of:\n");
	bu_vls_printf(gedp->ged_result_str,
		"      ssx     (show intersecting surface pairs)\n");
	bu_vls_printf(gedp->ged_result_str,
		"      ssx N   (show intersections of ssx pair N)\n");
	bu_vls_printf(gedp->ged_result_str,
		"   isocsx N   (show intersecting isocurve-surface pairs of ssx pair N)\n");
	bu_vls_printf(gedp->ged_result_str,
		"   isocsx N M (show intersections of ssx pair N, isocsx pair M)\n");
	bu_vls_printf(gedp->ged_result_str,
		"  fcurves N   (show clipped face curves of ssx pair N)\n");
	bu_vls_printf(gedp->ged_result_str,
		"  lcurves     (show linked ssx curves used to split faces)\n");
	bu_vls_printf(gedp->ged_result_str,
		"    faces     (show split faces used to construct result)\n");
	return GED_HELP;
    }
    filename = argv[1];
    cmd = argv[2];

    if (info.mode == DPLOT_INITIAL) {
	if (BU_STR_EQUAL(cmd, "ssx") && argc == 3) {
	    info.mode = DPLOT_SSX_FIRST;
	    info.ssx_idx = 0;
	} else if (BU_STR_EQUAL(cmd, "ssx") && argc == 4) {
	    /* parse surface pair index */
	    const char *idx_str = argv[3];
	    ret = bu_sscanf(idx_str, "%d", &info.ssx_idx);
	    if (ret != 1) {
		bu_vls_printf(gedp->ged_result_str, "%s is not a valid "
			"surface pair (must be a non-negative integer)\n", idx_str);
		return BRLCAD_ERROR;
	    }
	    info.mode = DPLOT_SSX_EVENTS;
	    info.event_idx = 0;
	} else if (BU_STR_EQUAL(cmd, "isocsx") && argc == 4) {
	    const char *idx_str = argv[3];
	    ret = bu_sscanf(idx_str, "%d", &info.ssx_idx);
	    if (ret != 1) {
		bu_vls_printf(gedp->ged_result_str, "%s is not a valid "
			"surface pair (must be a non-negative integer)\n", idx_str);
		return BRLCAD_ERROR;
	    }
	    info.mode = DPLOT_ISOCSX_FIRST;
	    info.isocsx_idx = 0;
	} else if (BU_STR_EQUAL(cmd, "isocsx") && argc == 5) {
	    /* parse surface pair index */
	    const char *idx_str = argv[3];
	    ret = bu_sscanf(idx_str, "%d", &info.ssx_idx);
	    if (ret != 1) {
		bu_vls_printf(gedp->ged_result_str, "%s is not a valid "
			"surface pair (must be a non-negative integer)\n", idx_str);
		return BRLCAD_ERROR;
	    }
	    idx_str = argv[4];
	    ret = bu_sscanf(idx_str, "%d", &info.isocsx_idx);
	    if (ret != 1) {
		bu_vls_printf(gedp->ged_result_str, "%s is not a valid "
			"isocurve-surface pair (must be a non-negative integer)\n", idx_str);
		return BRLCAD_ERROR;
	    }
	    info.mode = DPLOT_ISOCSX_EVENTS;
	    info.event_idx = 0;
	} else if (BU_STR_EQUAL(cmd, "fcurves") && argc == 4) {
	    const char *idx_str = argv[3];
	    ret = bu_sscanf(idx_str, "%d", &info.ssx_idx);
	    if (ret != 1) {
		bu_vls_printf(gedp->ged_result_str, "%s is not a valid "
			"surface pair (must be a non-negative integer)\n", idx_str);
		return BRLCAD_ERROR;
	    }
	    info.mode = DPLOT_FACE_CURVES;
	    info.event_idx = 0;
	} else if (BU_STR_EQUAL(cmd, "lcurves") && argc == 3) {
	    info.mode = DPLOT_LINKED_CURVES;
	    info.event_idx = 0;
	} else if (BU_STR_EQUAL(cmd, "faces") && argc == 3) {
	    info.mode = DPLOT_SPLIT_FACES;
	    info.event_idx = 0;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%s is not a recognized "
		    "command or was given the wrong number of arguments\n",
		    cmd);
	    return BRLCAD_ERROR;
	}
    }

    /* open dplot log file */
    info.logfile = std::string(filename);
 
    if (dplot_load_file_data(&info) != BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str, "couldn't open log file \"%s\"\n", filename);
	return BRLCAD_ERROR;
    }   

    /* filename before '.' is assumed to be the prefix for all
     * plot-file names
     */
    info.prefix = bu_strdup(filename);
    dot = strchr(info.prefix, '.');
    if (dot) {
	*dot = '\0';
    }

    if (info.mode == DPLOT_SSX_FIRST	||
	    info.mode == DPLOT_SSX		||
	    info.mode == DPLOT_SSX_EVENTS	||
	    info.mode == DPLOT_ISOCSX_FIRST	||
	    info.mode == DPLOT_ISOCSX	||
	    info.mode == DPLOT_ISOCSX_EVENTS)
    {
	if (info.fdata.ssx_count == 0) {
	    bu_vls_printf(info.gedp->ged_result_str, "no surface surface"
		    "intersections");
	    RETURN_ERROR;
	} else if (info.ssx_idx < 0 ||
		info.ssx_idx > (info.fdata.ssx_count - 1))
	{
	    bu_vls_printf(info.gedp->ged_result_str, "no surface pair %d (valid"
		    " range is [0, %d])\n", info.ssx_idx, info.fdata.ssx_count - 1);
	    RETURN_ERROR;
	}
    }
    if (info.fdata.ssx_count > 0) {
	info.brep1_surf_idx = info.fdata.ssx[info.ssx_idx]->brep1_surface;
	info.brep2_surf_idx = info.fdata.ssx[info.ssx_idx]->brep2_surface;
	info.event_count = info.fdata.ssx[info.ssx_idx]->final_curve_events;
	info.brep1_isocsx_count =
	    info.fdata.ssx[info.ssx_idx]->intersecting_brep1_isocurves;
	info.isocsx_count =
	    info.fdata.ssx[info.ssx_idx]->intersecting_isocurves;
    }
    info.brep1_surf_count = info.fdata.brep1_surface_count;
    info.brep2_surf_count = info.fdata.brep2_surface_count;

    if (info.mode == DPLOT_ISOCSX_EVENTS && info.fdata.ssx_count > 0) {
	int *isocsx_events = info.fdata.ssx[info.ssx_idx].isocsx_events;

	info.event_count = 0;
	if (isocsx_events) {
	    info.event_count = isocsx_events[info.event_idx];
	}
    }

    ret = dplot_ssx(&info);
    if (ret == BRLCAD_ERROR) {
	RETURN_ERROR;
    } else if (ret == GED_MORE) {
	RETURN_MORE;
    }

    ret = dplot_ssx_events(&info);
    if (ret == BRLCAD_ERROR) {
	RETURN_ERROR;
    } else if (ret == GED_MORE) {
	RETURN_MORE;
    }

    ret = dplot_isocsx(&info);
    if (ret == BRLCAD_ERROR) {
	RETURN_ERROR;
    } else if (ret == GED_MORE) {
	RETURN_MORE;
    }

    ret = dplot_isocsx_events(&info);
    if (ret == BRLCAD_ERROR) {
	RETURN_ERROR;
    } else if (ret == GED_MORE) {
	RETURN_MORE;
    }

    ret = dplot_face_curves(&info);
    if (ret == BRLCAD_ERROR) {
	RETURN_ERROR;
    } else if (ret == GED_MORE) {
	RETURN_MORE;
    }

    ret = dplot_split_faces(&info);
    if (ret == BRLCAD_ERROR) {
	RETURN_ERROR;
    } else if (ret == GED_MORE) {
	RETURN_MORE;
    }

    ret = dplot_linked_curves(&info);
    if (ret == BRLCAD_ERROR) {
	RETURN_ERROR;
    } else if (ret == GED_MORE) {
	RETURN_MORE;
    }

    info.mode = DPLOT_INITIAL;
    CLEANUP;

    return BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

