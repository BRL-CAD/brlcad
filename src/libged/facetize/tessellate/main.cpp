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

static bool
bot_is_manifold(struct rt_bot_internal *bot)
{
    if (!bot)
	return false;

    manifold::Mesh bot_mesh;
    manifold::Manifold bot_manifold;

    // We have a BoT, but make sure we can get a Manifold before we accept it
    for (size_t j = 0; j < bot->num_vertices ; j++)
	bot_mesh.vertPos.push_back(glm::vec3(bot->vertices[3*j], bot->vertices[3*j+1], bot->vertices[3*j+2]));
    for (size_t j = 0; j < bot->num_faces; j++)
	bot_mesh.triVerts.push_back(glm::vec3(bot->faces[3*j], bot->faces[3*j+1], bot->faces[3*j+2]));
    bot_manifold = manifold::Manifold(bot_mesh);
    if (bot_manifold.Status() != manifold::Manifold::Error::NoError)
	return false;
    return true;
}

static int
_nmg_tessellate(struct rt_bot_internal **nbot, struct rt_db_internal *intern,  const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    int status = -1;

    if (!nbot || !intern || !intern->idb_meth || !ttol || !tol)
	return BRLCAD_ERROR;

    (*nbot) = NULL;

    struct model *m = nmg_mm();
    struct nmgregion *r1 = (struct nmgregion *)NULL;
    // Try the NMG routines (primary means of CSG implicit -> explicit mesh conversion)
    // TODO - needs to be separate process...
    if (!BU_SETJUMP) {
	status = intern->idb_meth->ft_tessellate(&r1, m, intern, ttol, tol);
    } else {
	BU_UNSETJUMP;
	status = -1;
	nbot = NULL;
    } BU_UNSETJUMP;

    if (status <= -1)
	return BRLCAD_ERROR;

    // NMG reports success, now get a BoT
    if (!BU_SETJUMP) {
	(*nbot) = (struct rt_bot_internal *)nmg_mdl_to_bot(m, &RTG.rtg_vlfree, tol);
    } else {
	BU_UNSETJUMP;
	(*nbot) = NULL;
    } BU_UNSETJUMP;

    if (!(*nbot))
	return BRLCAD_ERROR;

    if (!bot_is_manifold(*nbot)) {
	// Urk - we got an NMG mesh, but it's no good for a Manifold(??)
	if ((*nbot)->vertices)
	    bu_free((*nbot)->vertices, "verts");
	if ((*nbot)->faces)
	    bu_free((*nbot)->faces, "faces");
	BU_FREE((*nbot), struct rt_bot_internal);
	(*nbot) = NULL;
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

static void
method_enablement_check(struct tess_opts *s)
{
    if (!s)
	return;
    bool e = false;
    if (s->Co3Ne) e = true;
    if (s->ball_pivot) e = true;
    if (s->continuation) e = true;
    if (s->instant_mesh) e = true;
    if (s->nmg) e = true;
    if (s->screened_poisson) e = true;

    // If nothing was specified, try everything
    if (!e) {
	s->Co3Ne = 1;
	s->ball_pivot = 1;
	s->continuation = 1;
	s->instant_mesh = 1;
	s->nmg = 1;
	s->screened_poisson = 1;
    }
}

int
main(int argc, const char **argv)
{
    if (!argc || !argv)
	return BRLCAD_ERROR;

    bu_setprogname(argv[0]);

    // Done with prog name
    argc--; argv++;

    int print_help = 0;
    struct rt_pnts_internal *pnts = NULL;
    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct tess_opts s = TESS_OPTS_DEFAULT;
    s.ttol = &ttol;
    s.tol = &tol;

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

    // If no method(s) were specified, try everything
    method_enablement_check(&s);

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
	    if (!s.Co3Ne && !s.ball_pivot && !s.screened_poisson)
		return BRLCAD_OK;

	    // If we are going to try a pnts wrapping, there are only a few
	    // candidates in the fallback methods list that we can use.
	    s.nmg = 0;
	    s.continuation = 0;
	    s.instant_mesh = 0; // TODO - can this handle a point cloud?

	    // point the pnts arguments to the internal point data
	    pnts = (struct rt_pnts_internal *)intern.idb_ptr;

	    goto pnt_sampling_methods;
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
	    if (!bot_is_manifold(nbot)) {
		// Nope - try repairing
		ret = bot_repair(&obot, bot, &ttol, &tol);
	    } else {
		// Passed - we're good to go. For this case we should rename
		// rather than writing it out again - no need for two of the
		// same object

		/* Change object name in the in-memory directory. */
		if (db_rename(gedp->dbip, dp, argv[2]) < 0) {
		    rt_db_free_internal(&intern);
		    bu_log("BoT is suitable for boolean operations as-is, but error encountered in renaming to %s, aborting", argv[2]);
		    return BRLCAD_ERROR;
		}

		/* Re-write to the database.  New name is applied on the way out. */
		if (rt_db_put_internal(dp, gedp->dbip, &intern, &rt_uniresource) < 0) {
		    bu_log("BoT is suitable for boolean operations as-is, but error encountered in renaming to %s, aborting", argv[2]);
		    return BRLCAD_ERROR;
		}

		return BRLCAD_OK;
	    }
	case ID_BREP:
	    // TODO - need to handle plate mode NURBS the way we handle plate mode BoTs
	default:
	    break;
    }

    if (ret == BRLCAD_OK && obot) {
	// Already have the output bot?  Write it to disk and return
	goto write_obot;
    }

    // If we got this far, it's not a special case.  Start trying whatever tessellation methods
    // are enabled

    if (s.nmg) {
	// NMG is best, if it works
	ret = _nmg_tessellate(&obot, &intern, &ttol, &tol);
	if (ret == BRLCAD_OK)
	    goto write_obot;
    }

    if (s.continuation) {
	if (!pnts) {
	    pnts = _tess_pnts_sample(dp->d_namep, gedp->dbip, &s);
	}
	// CM is a marching method using an inside/outside test
	//ret = _cm_tessellate(&obot, &intern, &ttol, &tol);
	if (ret == BRLCAD_OK)
	    goto write_obot;
    }

pnt_sampling_methods:

    // TODO - all of the following methods require first sampling points from the solid
    // with the raytracer.  Do this once - if one method fails, we can use the same point
    // input for other methods.

    if (s.Co3Ne) {
	if (!pnts) {
	    pnts = _tess_pnts_sample(dp->d_namep, gedp->dbip, &s);
	}
	//ret = _co3ne_tessellate(&obot, &intern, &ttol, &tol);
	if (ret == BRLCAD_OK)
	    goto write_obot;
    }

    if (s.screened_poisson) {
	if (!pnts) {
	    pnts = _tess_pnts_sample(dp->d_namep, gedp->dbip, &s);
	}
	//ret = _spsr_tessellate(&obot, &intern, &ttol, &tol);
	if (ret == BRLCAD_OK)
	    goto write_obot;
    }

write_obot:

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

