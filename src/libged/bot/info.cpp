/*                        I N F O . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2025 United States Government as represented by
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
/** @file libged/bot/info.cpp
 *
 * LIBGED bot info subcommand - reports detailed information about
 * individual vertices and faces of a BoT mesh, modeled after the
 * brep info subcommand.
 *
 */

#include "common.h"

#include <set>

#include "bu/cmd.h"
#include "rt/geom.h"

#include "./ged_bot.h"

struct _ged_bot_iinfo {
    struct bu_vls *vls;
    const struct rt_bot_internal *bot;
    const struct bu_cmdtab *cmds;
};

static int
_bot_info_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_bot_iinfo *gb = (struct _ged_bot_iinfo *)bs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gb->vls, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->vls, "%s\n", ps);
	return 1;
    }
    return 0;
}


/* V - vertices */
extern "C" int
_bot_cmd_vertex_info(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] <objname> info V [[index][index-index]]";
    const char *purpose_string = "3D vertex coordinates and associated normals";
    if (_bot_info_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_bot_iinfo *gib = (struct _ged_bot_iinfo *)bs;
    const struct rt_bot_internal *bot = gib->bot;

    std::set<int> elements;
    if (_bot_face_specifiers(elements, gib->vls, argc, argv) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }
    /* If no indices specified, report all vertices */
    if (!elements.size()) {
	for (size_t i = 0; i < bot->num_vertices; i++) {
	    elements.insert((int)i);
	}
    }

    bool has_normals = (bot->num_normals > 0 && bot->num_face_normals > 0
		       && bot->normals && bot->face_normals
		       && (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS));

    std::set<int>::iterator e_it;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
	int vi = *e_it;
	if (vi < 0 || (size_t)vi >= bot->num_vertices) {
	    bu_vls_printf(gib->vls, "vertex index %d out of range (0..%zu)\n",
			 vi, bot->num_vertices - 1);
	    return BRLCAD_ERROR;
	}

	bu_vls_printf(gib->vls, "V[%d]: %g %g %g\n", vi,
		      bot->vertices[3*vi+0],
		      bot->vertices[3*vi+1],
		      bot->vertices[3*vi+2]);

	/* Report all faces that reference this vertex, and the associated
	 * surface normal for each (face, position) when normals are present. */
	for (size_t fi = 0; fi < bot->num_faces; fi++) {
	    for (int k = 0; k < 3; k++) {
		if (bot->faces[3*fi+k] != vi)
		    continue;
		if (has_normals && fi < bot->num_face_normals) {
		    int ni = bot->face_normals[3*fi+k];
		    if (ni >= 0 && (size_t)ni < bot->num_normals) {
			bu_vls_printf(gib->vls,
				      "   F[%zu] normal: %g %g %g\n",
				      fi,
				      bot->normals[3*ni+0],
				      bot->normals[3*ni+1],
				      bot->normals[3*ni+2]);
		    } else {
			bu_vls_printf(gib->vls, "   F[%zu]\n", fi);
		    }
		} else {
		    bu_vls_printf(gib->vls, "   F[%zu]\n", fi);
		}
		break; /* vertex appears at most once per face */
	    }
	}
    }

    return BRLCAD_OK;
}


/* F - triangle faces */
extern "C" int
_bot_cmd_face_info(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] <objname> info F [[index][index-index]]";
    const char *purpose_string = "face vertex indices and v1->v2->v3 triangle structure";
    if (_bot_info_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_bot_iinfo *gib = (struct _ged_bot_iinfo *)bs;
    const struct rt_bot_internal *bot = gib->bot;

    std::set<int> elements;
    if (_bot_face_specifiers(elements, gib->vls, argc, argv) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }
    /* If no indices specified, report all faces */
    if (!elements.size()) {
	for (size_t i = 0; i < bot->num_faces; i++) {
	    elements.insert((int)i);
	}
    }

    std::set<int>::iterator e_it;
    for (e_it = elements.begin(); e_it != elements.end(); e_it++) {
	int fi = *e_it;
	if (fi < 0 || (size_t)fi >= bot->num_faces) {
	    bu_vls_printf(gib->vls, "face index %d out of range (0..%zu)\n",
			 fi, bot->num_faces - 1);
	    return BRLCAD_ERROR;
	}

	int v0 = bot->faces[3*fi+0];
	int v1 = bot->faces[3*fi+1];
	int v2 = bot->faces[3*fi+2];

	bu_vls_printf(gib->vls, "F[%d]: V[%d] -> V[%d] -> V[%d]\n", fi, v0, v1, v2);
	bu_vls_printf(gib->vls, "   V[%d]: %g %g %g\n", v0,
		      bot->vertices[3*v0+0], bot->vertices[3*v0+1], bot->vertices[3*v0+2]);
	bu_vls_printf(gib->vls, "   V[%d]: %g %g %g\n", v1,
		      bot->vertices[3*v1+0], bot->vertices[3*v1+1], bot->vertices[3*v1+2]);
	bu_vls_printf(gib->vls, "   V[%d]: %g %g %g\n", v2,
		      bot->vertices[3*v2+0], bot->vertices[3*v2+1], bot->vertices[3*v2+2]);
    }

    return BRLCAD_OK;
}


static void
_bot_info_help(struct _ged_bot_iinfo *bs, int argc, const char **argv)
{
    struct _ged_bot_iinfo *gib = (struct _ged_bot_iinfo *)bs;
    if (!argc || !argv) {
	bu_vls_printf(gib->vls, "bot [options] <objname> info <subcommand> [args]\n");
	bu_vls_printf(gib->vls, "Available subcommands:\n");
	const struct bu_cmdtab *ctp = NULL;
	int ret;
	const char *helpflag[2];
	helpflag[1] = PURPOSEFLAG;
	for (ctp = gib->cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    bu_vls_printf(gib->vls, "  %s\t\t", ctp->ct_name);
	    helpflag[0] = ctp->ct_name;
	    bu_cmd(gib->cmds, 2, helpflag, 0, (void *)gib, &ret);
	}
    } else {
	int ret;
	const char *helpflag[2];
	helpflag[0] = argv[0];
	helpflag[1] = HELPFLAG;
	bu_cmd(gib->cmds, 2, helpflag, 0, (void *)gib, &ret);
    }
}

const struct bu_cmdtab _bot_info_cmds[] = {
    { "F",          _bot_cmd_face_info},
    { "V",          _bot_cmd_vertex_info},
    { (char *)NULL, NULL}
};

int
bot_info(struct _ged_bot_info *gb, int argc, const char **argv)
{
    const struct rt_bot_internal *bot =
	(const struct rt_bot_internal *)(gb->intern->idb_ptr);

    struct _ged_bot_iinfo gib;
    gib.vls = gb->gedp->ged_result_str;
    gib.bot = bot;
    gib.cmds = _bot_info_cmds;

    if (!argc) {
	/* No subcommand: print summary counts */
	bu_vls_printf(gib.vls, "vertices:     %zu\n", bot->num_vertices);
	bu_vls_printf(gib.vls, "faces:        %zu\n", bot->num_faces);
	bu_vls_printf(gib.vls, "normals:      %zu\n", bot->num_normals);
	bu_vls_printf(gib.vls, "face normals: %zu\n", bot->num_face_normals);
	return BRLCAD_OK;
    }

    if (argc > 1 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	argc--; argv++;
	argc--; argv++;
	_bot_info_help(&gib, argc, argv);
	return BRLCAD_OK;
    }

    /* Must have a valid subcommand */
    if (bu_cmd_valid(_bot_info_cmds, argv[0]) != BRLCAD_OK) {
	bu_vls_printf(gib.vls, "invalid subcommand \"%s\" specified\n", argv[0]);
	_bot_info_help(&gib, 0, NULL);
	return BRLCAD_ERROR;
    }

    int ret;
    if (bu_cmd(_bot_info_cmds, argc, argv, 0, (void *)&gib, &ret) == BRLCAD_OK) {
	return ret;
    }

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
