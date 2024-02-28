/*                        M A I N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @file libged/facetize/tessellate/main.cpp
 *
 * Because the process of turning implicit solids into manifold meshes
 * has a wide variety of difficulties associated with it, we run the
 * actual per-primitive conversion as a sub-process managed by the
 * facetize command.
 */

#include "common.h"

#include <sstream>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/opt.h"
#include "rt/primitives/bot.h"
#include "ged.h"
#define TESS_OPTS_IMPLEMENTATION
#include "../tess_opts.h"
#include "./tessellate.h"

static void
method_setup(struct tess_opts *s)
{
    std::map<std::string,std::string>::iterator m_it;
    if (!s)
	return;

    if (!s->method_opts.methods.size()) {
	s->method_opts.methods.insert(std::string("NMG"));
	s->method_opts.methods.insert(std::string("CM"));
	s->method_opts.methods.insert(std::string("SPSR"));
    }

    // If we have the key word "SAMPLING" present in methods, it
    // signals that we have global pre-configured settings to
    // use as defaults for all sampling methods.  Process them
    // first
    std::map<std::string, std::map<std::string,std::string>>::iterator o_it = s->method_opts.options_map.find(std::string("SAMPLING"));
    if (o_it != s->method_opts.options_map.end()) {
	std::map<std::string,std::string> &s_opts = o_it->second;
	for (m_it = s_opts.begin(); m_it != s_opts.end(); m_it++) {
	    std::string key = m_it->first;
	    std::string val = m_it->second;
	    s->pnt_options.set_var(key, val);
	}

	s->cm_options.sync(s->pnt_options);
	s->spsr_options.sync(s->pnt_options);
    }

    // Now that we've set any default overrides for multiple types, get the
    // method specific options for each method set up
    s->nmg_options.sync(s->method_opts);
    s->cm_options.sync(s->method_opts);
    s->spsr_options.sync(s->method_opts);
}

static int
dp_tessellate(struct rt_bot_internal **obot, int *method_flag, struct db_i *dbip, struct directory *dp, struct tess_opts *s)
{
    std::set<std::string> &mset = s->method_opts.methods;
    if (!obot || !method_flag || !dbip || !dp)
	return BRLCAD_ERROR;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0) {
	bu_log("rt_db_get_internal failed for %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }

    struct rt_pnts_internal *pnts = NULL;
    struct rt_bot_internal *bot = NULL;
    int propVal;
    int ret = BRLCAD_OK;

    switch (intern.idb_minor_type) {
	// If we've got no-volume objects, they get an empty Manifold -
	// they can be safely treated as a no-op in any of the booleans
	case ID_ANNOT:
	case ID_BINUNIF:
	case ID_CONSTRAINT:
	case ID_DATUM:
	case ID_GRIP:
	case ID_JOINT:
	case ID_MATERIAL:
	case ID_SCRIPT:
	case ID_SKETCH:
	    return BRLCAD_OK;
	case ID_PNTS:
	    // At this low level, allow point processing methods to have a
	    // crack at a point primitive to wrap it in a mesh.  If we don't
	    // want facetize generating a mesh from a pnts object during a
	    // general tree walk, we should skip point objects at the higher
	    // level.  Even if we don't want to have facetize turn pnts objects
	    // into meshes (which we probably don't), that is a capability we
	    // will most likely want to expose through the pnts command - which
	    // will make this executable useful to more than just facetize.
	    if (mset.find(std::string("SPSR")) == mset.end())
		return BRLCAD_OK;

	    // If we are going to try a pnts wrapping, there are only a few
	    // candidates in the fallback methods list that we can use.
	    mset.erase(std::string("NMG"));
	    mset.erase(std::string("CM"));

	    // point the pnts arguments to the internal point data
	    pnts = (struct rt_pnts_internal *)intern.idb_ptr;

	    goto pnt_sampling_methods;
	case ID_HALF:
	    // Halfspace objects are handled specially by BRL-CAD.
	    return BRLCAD_OK;
	case ID_BOT:
	    bot = (struct rt_bot_internal *)(intern.idb_ptr);
	    propVal = (int)rt_bot_propget(bot, "type");
	    // Surface meshes are zero volume, and thus no-op
	    if (propVal == RT_BOT_SURFACE)
		return BRLCAD_OK;
	    // Plate mode BoTs need an explicit volume representation
	    if (propVal == RT_BOT_PLATE || propVal == RT_BOT_PLATE_NOCOS) {
		return rt_bot_plate_to_vol(obot, bot, 0, 0);
	    }
	    // Volumetric bot - if it can be manifold we're good, but if
	    // not we need to try and repair it.
	    if (!bot_is_manifold(bot)) {
		// Nope - try repairing
		*method_flag = FACETIZE_METHOD_REPAIR;
		ret = rt_bot_repair(obot, bot);
	    } else {
		// Already a valid BoT - tessellate is a no-op.
		*obot = NULL;
		return BRLCAD_OK;
	    }
	case ID_DSP:
	    // TODO - need to create a Triangulated Irregular Network for these - the
	    // current NMG routine apparently fails even on the sample terra.g solid.
	    //
	    // Would be VERY interesting to try and port the following code to
	    // work with DSP data:
	    // http://mgarland.org/software/terra.html
	    // http://mgarland.org/software/scape.html
	    //
	    // Note that the CM methodology actually seems to succeed fairly
	    // well and quickly with terra.g, so it might be worth defaulting
	    // the tessellate call in the parent facetize command to just using
	    // CM for dsp objects rather than trying the long and unsuccessful
	    // NMG algorithm.  We need to allow all the various methods to have a crack
	    // at the data here - unlike ID_PNTS, in THEORY nmg should successfully
	    // produce output, so there's no justification for disabling it.
	case ID_BREP:
	    // TODO - need to handle plate mode NURBS the way we handle plate mode BoTs
	default:
	    break;
    }

    if (ret == BRLCAD_OK && *obot) {
	// If we already have the output bot, return
	return BRLCAD_OK;
    }

    // If we got this far, it's not a special case.  Start trying whatever tessellation methods
    // are enabled

    if (mset.find(std::string("NMG")) != mset.end()) {
	// NMG is best, if it works
	ret = _nmg_tessellate(obot, &intern, s);
	if (ret == BRLCAD_OK) {
	    *method_flag = FACETIZE_METHOD_NMG;
	    return BRLCAD_OK;
	}
    }

    if (mset.find(std::string("CM")) != mset.end()) {
	// The continuation method (CM) is a marching algorithm using an
	// inside/outside test, building from a seed point on the surface.
	//
	// CM needs some awareness of properties of the solid, so we use the
	// raytrace interrogation to build up that data.  Unlike the sampling
	// methods we don't make direct use of the points beyond using one of
	// them for the seed, but we do use information collected during the
	// sampling process.
	if (!pnts) {
	    pnts = _tess_pnts_sample(dp->d_namep, dbip, s);
	}
	struct pnt_normal *seed = BU_LIST_PNEXT(pnt_normal, (struct pnt_normal *)pnts->point);
	ret = continuation_mesh(obot, dbip, dp->d_namep, s, seed->v);
	if (ret == BRLCAD_OK) {
	    *method_flag = FACETIZE_METHOD_CONTINUATION;
	    return BRLCAD_OK;
	}
    }

pnt_sampling_methods:

    if (mset.find(std::string("Co3Ne")) != mset.end()) {
	if (!pnts) {
	    pnts = _tess_pnts_sample(dp->d_namep, dbip, s);
	}
	//ret = co3ne_mesh(obot, &intern, s->ttol, s->tol);
	if (ret == BRLCAD_OK)
	    return ret;
    }

    if (mset.find(std::string("BALL_PIVOT")) != mset.end()) {
	if (!pnts) {
	    pnts = _tess_pnts_sample(dp->d_namep, dbip, s);
	}
	//ret = ball_pivot_mesh(obot, &intern, s->ttol, s->tol);
	if (ret == BRLCAD_OK)
	    return ret;
    }

    if (mset.find(std::string("SPSR")) != mset.end()) {
	if (!pnts) {
	    pnts = _tess_pnts_sample(dp->d_namep, dbip, s);
	}
	//ret = spsr_mesh(obot, &intern, s->ttol, s->tol);
	if (ret == BRLCAD_OK) {
	    *method_flag = FACETIZE_METHOD_SPSR;
	    return ret;
	}
    }

    return BRLCAD_ERROR;
}

void
print_tess_methods()
{
    fprintf(stdout, "NMG CM SPSR\n");
}

int
main(int argc, const char **argv)
{
    if (!argc || !argv)
	return BRLCAD_ERROR;

    bu_setprogname(argv[0]);

    // Done with prog name
    argc--; argv++;

    static const char *usage = "Usage: ged_tessellate [options] file.g input_obj [input_object_2 ...]\n";
    int print_help = 0;
    struct tess_opts s;

    int list_methods = 0;
    int max_time = 0;
    int max_pnts = 0;

    struct bu_opt_desc d[ 8];
    BU_OPT(d[ 0],  "h",         "help",                         "",                  NULL,           &print_help, "Print help and exit");
    BU_OPT(d[ 1],   "", "list-methods",                         "",                  NULL,         &list_methods, "List available tessellation methods.  When used with -h, print an informational summary of each method.");
    BU_OPT(d[ 2],  "O",    "overwrite",                         "",                  NULL,    &(s.overwrite_obj), "Replace original object with BoT");
    BU_OPT(d[ 3],   "",      "methods",                "m1 m2 ...", &_tess_active_methods,        &s.method_opts, "List of active methods to use for this tessellation attempt");
    BU_OPT(d[ 4],   "",  "method-opts",  "M opt1=val opt2=val ...",    &_tess_method_opts,        &s.method_opts, "Set options for method M.  If specified just a method M and the -h option, print documentation about method options.");
    BU_OPT(d[ 5],   "",     "max-time",                        "#",           &bu_opt_int,             &max_time, "Maximum number of seconds to allow for runtime (not supported by all methods).");
    BU_OPT(d[ 6],   "",     "max-pnts",                        "#",           &bu_opt_int,             &max_pnts, "Maximum number of pnts to use when applying ray sampling methods.");
    BU_OPT_NULL(d[ 7]);

    /* parse options */
    struct bu_vls omsg = BU_VLS_INIT_ZERO;
    argc = bu_opt_parse(&omsg, argc, argv, d);
    if (argc < 0) {
	bu_log("Option parsing error: %s\n", bu_vls_cstr(&omsg));
	bu_vls_free(&omsg);
	bu_exit(BRLCAD_ERROR, "%s failed", bu_getprogname());
    }
    bu_vls_free(&omsg);

    if (list_methods) {
	print_tess_methods();
	return BRLCAD_OK;
    }

    if (print_help) {
	struct bu_vls str = BU_VLS_INIT_ZERO;
	char *option_help;

	bu_vls_sprintf(&str, "%s", usage);

	if ((option_help = bu_opt_describe(d, NULL))) {
	    bu_vls_printf(&str, "Options:\n%s\n", option_help);
	    bu_free(option_help, "help str");
	}

	bu_log("%s\n", bu_vls_cstr(&str));
	bu_vls_free(&str);
        return BRLCAD_OK;
    }

    // Do the setup for the various methods
    method_setup(&s);

    // Open the database
    struct ged *gedp = ged_open("db", argv[0], 1);
    if (!gedp)
	return BRLCAD_ERROR;

    // Translate specified object names to directory pointers
    struct bu_ptbl dps = BU_PTBL_INIT_ZERO;
    for (int i = 1; i < argc; i++) {
	struct directory *dp = db_lookup(gedp->dbip, argv[i], LOOKUP_NOISY);
	if (!dp)
	    return BRLCAD_ERROR;
	bu_ptbl_ins(&dps, (long *)dp);
    }

    // Tessellate each object.  Note that we're doing this in series rather
    // than parallel because of the risks of high memory consumption and/or
    // CPU utilization for individual object operations.
    for (size_t i = 0; i < BU_PTBL_LEN(&dps); i++) {

	// If this isn't a proper BRL-CAD object, tessellation is a no-op
	struct directory *dp = (struct directory *)BU_PTBL_GET(&dps, i);
	if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD)
	    continue;

	// Trigger the core tessellation routines
	struct rt_bot_internal *obot = NULL;
	int method_flag = FACETIZE_METHOD_NULL;
	if (dp_tessellate(&obot, &method_flag, gedp->dbip, dp, &s) != BRLCAD_OK)
	    return BRLCAD_ERROR;

	// If we didn't get anything and we had an OK code, just keep going
	if (!obot)
	    continue;

	// If we've got something to write, handle it
	struct bu_vls obot_name = BU_VLS_INIT_ZERO;
	if (s.overwrite_obj) {
	    bu_vls_sprintf(&obot_name, "%s", dp->d_namep);
	} else {
	    bu_vls_sprintf(&obot_name, "%s_tess.bot", dp->d_namep);
	}
	// NOTE: _tess_facetize_write_bot frees obot
	int ret = _tess_facetize_write_bot(gedp->dbip, obot, bu_vls_cstr(&obot_name), method_flag);
	bu_vls_free(&obot_name);
	if (ret != BRLCAD_OK)
	    return BRLCAD_ERROR;

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

