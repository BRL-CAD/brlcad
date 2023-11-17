/*                        M A I N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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

#include "bu/app.h"
#include "bu/opt.h"

#include "ged.h"
#include "./tessellate.h"

int
main(int argc, const char **argv)
{
    if (!argc || !argv)
	return BRLCAD_ERROR;

    bu_setprogname(argv[0]);

    // Done with prog name
    argc--; argv++;

    int status = -1;
    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    int print_help = 0;
    struct tess_opts s = TESS_OPTS_DEFAULT;

    struct bu_opt_desc d[19];
    BU_OPT(d[ 0],  "h",                  "help",  "",            NULL,                  &print_help, "Print help and exit");
    BU_OPT(d[ 1],   "",               "tol-abs", "#", &bu_opt_fastf_t,                  &(ttol.abs), "Absolute distance tolerance");
    BU_OPT(d[ 2],   "",               "tol-rel", "#", &bu_opt_fastf_t,                  &(ttol.rel), "Relative distance tolerance");
    BU_OPT(d[ 3],   "",              "tol-norm", "#", &bu_opt_fastf_t,                 &(ttol.norm), "Normal tolerance");
    BU_OPT(d[ 4],   "",                   "nmg",  "",            NULL,                     &(s.nmg), "Enable use of the N-Manifold Geometry (NMG) meshing method");
    BU_OPT(d[ 5],   "",          "instant-mesh",  "",            NULL,            &(s.instant_mesh), "Enable use of the Instant Mesh remeshing method");
    BU_OPT(d[ 6],   "",                    "cm",  "",            NULL,            &(s.continuation), "Enable use of the Continuation Method (CM) meshing method");
    BU_OPT(d[ 7],   "",            "ball-pivot",  "",            NULL,              &(s.ball_pivot), "Enable use of the Ball Pivot (BP) sampling-based meshing method");
    BU_OPT(d[ 8],   "",                 "co3ne",  "",            NULL,                   &(s.Co3Ne), "Enable use of the Co3Ne sampling-based meshing method");
    BU_OPT(d[ 9],   "",                  "spsr",  "",            NULL,        &(s.screened_poisson), "Enable Screened Poisson Surface Reconstruction (SPSR) sampling-based meshing method");
    BU_OPT(d[10],  "F",                "fscale", "#", &bu_opt_fastf_t,           &(s.feature_scale), "Percentage of the average thickness observed by the raytracer to use for a targeted feature size with sampling based methods.  Defaults to 0.15, overridden   by --fsize");
    BU_OPT(d[11],   "",                 "fsize", "#", &bu_opt_fastf_t,            &(s.feature_size), "Explicit feature length to try for sampling based methods - overrides feature-scale.");
    BU_OPT(d[12],   "",                "fsized", "#", &bu_opt_fastf_t,          &(s.d_feature_size), "Initial feature length to try for decimation in sampling based methods.  By default, this value is set to 1.5x the feature size.");
    BU_OPT(d[13],   "",              "max-time", "#",     &bu_opt_int,                &(s.max_time), "Maximum time to spend per processing step (in seconds).  Default is 30.  Zero means either the default (for routines which could run indefinitely) or run to   completion (if there is a theoretical termination point for the algorithm).  Be careful when specifying zero - it can produce very long runs!.");
    BU_OPT(d[14],   "",              "max-pnts", "#",     &bu_opt_int,                &(s.max_pnts), "Maximum number of pnts to use when applying ray sampling methods.");
    BU_OPT(d[15],   "",            "spsr-depth", "#",     &bu_opt_int,            &(s.s_opts.depth), "Maximum reconstruction depth (default 8)");
    BU_OPT(d[16],  "w",      "spsr-interpolate", "#", &bu_opt_fastf_t,     &(s.s_opts.point_weight), "Lower values (down to 0.0) bias towards a smoother mesh, higher values bias towards interpolation accuracy. (Default 2.0)");
    BU_OPT(d[17],   "", "spsr-samples-per-node", "#", &bu_opt_fastf_t, &(s.s_opts.samples_per_node), "How many samples should go into a cell before it is refined. (Default 1.5)");
    BU_OPT_NULL(d[18]);

    /* parse options */
    struct bu_vls omsg = BU_VLS_INIT_ZERO;
    argc = bu_opt_parse(&omsg, argc, argv, d);
    if (argc < 0) {
	bu_log("Option parsing error: %s\n", bu_vls_cstr(&omsg));
	bu_vls_free(&omsg);
	bu_exit(BRLCAD_ERROR, "%s failed", bu_getprogname());
    }
    bu_vls_free(&omsg);

    struct ged *gedp = ged_open("db", argv[0], 1);
    if (!gedp)
	return BRLCAD_ERROR;

    struct directory *dp = db_lookup(gedp->dbip, argv[2], LOOKUP_QUIET);
    if (!dp)
	return BRLCAD_ERROR;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, gedp->dbip, NULL, &rt_uniresource) < 0) {
	bu_log("rt_db_get_internal failed for %s\n", argv[2]);
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = NULL;
    struct rt_bot_internal *nbot = NULL;
    struct rt_bot_internal *obot = NULL;
    manifold::Mesh bot_mesh;
    manifold::Manifold bot_manifold;
    int propVal;
    int ret = 0;

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
	    // At this low level, allow Ball Pivot and SPSR to have a crack at
	    // a point primitive to wrap it in a mesh.  If we don't want
	    // facetize generating a mesh from a pnts object during a general
	    // tree walk, we should implement that at the higher level.  Even
	    // if we don't want to have facetize turn pnts objects into meshes
	    // (which we probably don't), that is a capability we will most
	    // likely want to expose through the pnts command - which will make
	    // this executable useful to more than just facetize.
	    if (!s.Co3Ne && !s.ball_pivot && !s.screened_poisson)
		return BRLCAD_OK;

	    // If we are going to try a pnts wrapping, there are only a few
	    // candidates in the fallback methods list that we can use.
	    s.nmg = 0;
	    s.continuation = 0;
	    s.instant_mesh = 0;
	    goto fallback_methods;
	case ID_HALF:
	    // Halfspace objects get a large arb.
	    ret = half_to_bot(&obot, &intern, &ttol, &tol);
	    break;
	case ID_BOT:
	    bot = (struct rt_bot_internal *)(intern.idb_ptr);
	    propVal = (int)rt_bot_propget(bot, "type");
	    // Surface meshes are zero volume, and thus no-op
	    if (propVal == RT_BOT_SURFACE)
		return BRLCAD_OK;
	    // Plate mode BoTs need an explicit volume representation
	    if (propVal == RT_BOT_PLATE || propVal == RT_BOT_PLATE_NOCOS) {
		ret = plate_eval(&obot, bot, &ttol, &tol);
	    }
	    // Volumetric bot - if it can be manifold we're good, but if
	    // not we need to try and repair it.
	    for (size_t j = 0; j < bot->num_vertices ; j++)
		bot_mesh.vertPos.push_back(glm::vec3(bot->vertices[3*j], bot->vertices[3*j+1], bot->vertices[3*j+2]));
	    for (size_t j = 0; j < bot->num_faces; j++)
		bot_mesh.triVerts.push_back(glm::vec3(bot->faces[3*j], bot->faces[3*j+1], bot->faces[3*j+2]));
	    bot_manifold = manifold::Manifold(bot_mesh);
	    if (bot_manifold.Status() != manifold::Manifold::Error::NoError) {
		// Nope - try repairing
		ret = bot_repair(&obot, bot, &ttol, &tol);
	    }
	case ID_BREP:
	    // TODO - need to handle plate mode NURBS the way we handle plate mode BoTs
	default:
	    break;
    }

    if (ret == BRLCAD_OK && obot) {
	// TODO - have output bot?  write it to disk and return
	return BRLCAD_OK;
    }

    // If we got this far, it's not a special case - see if we can do "normal"
    // tessellation
    if (intern.idb_meth) {
	struct model *m = nmg_mm();
	struct nmgregion *r1 = (struct nmgregion *)NULL;
	// Try the NMG routines (primary means of CSG implicit -> explicit mesh conversion)
	// TODO - needs to be separate process...
	if (!BU_SETJUMP) {
	    status = intern.idb_meth->ft_tessellate(&r1, m, &intern, &ttol, &tol);
	} else {
	    BU_UNSETJUMP;
	    status = -1;
	    nbot = NULL;
	} BU_UNSETJUMP;
	if (status > -1) {
	    // NMG reports success, now get a BoT
	    if (!BU_SETJUMP) {
		nbot = (struct rt_bot_internal *)nmg_mdl_to_bot(m, &RTG.rtg_vlfree, &tol);
	    } else {
		BU_UNSETJUMP;
		nbot = NULL;
	    } BU_UNSETJUMP;
	    if (nbot) {
		// We got a BoT, now see if we can get a Manifold
		for (size_t j = 0; j < nbot->num_vertices ; j++)
		    bot_mesh.vertPos.push_back(glm::vec3(nbot->vertices[3*j], nbot->vertices[3*j+1], nbot->vertices[3*j+2]));
		for (size_t j = 0; j < nbot->num_faces; j++)
		    bot_mesh.triVerts.push_back(glm::vec3(nbot->faces[3*j], nbot->faces[3*j+1], nbot->faces[3*j+2]));
		bot_manifold = manifold::Manifold(bot_mesh);
		if (bot_manifold.Status() != manifold::Manifold::Error::NoError) {
		    // Urk - we got an NMG mesh, but it's no good for a Manifold(??)
		    if (nbot->vertices)
			bu_free(nbot->vertices, "verts");
		    if (nbot->faces)
			bu_free(nbot->faces, "faces");
		    BU_FREE(nbot, struct rt_bot_internal);
		    nbot = NULL;
		}
	    }
	}
    }

    if (status >= 0)
	return BRLCAD_OK;

    // TODO - nothing worked - try fallback methods, if enabled
fallback_methods:

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

