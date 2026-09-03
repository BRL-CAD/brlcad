/*                        M A I N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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

#include <algorithm>
#include <climits>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <stdlib.h>

#include "bu/app.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/opt.h"
#include "bg/trimesh.h"
#include "rt/primitives/bot.h"
#include "ged.h"
#define TESS_OPTS_IMPLEMENTATION
#include "../ged_facetize.h"
#include "../tess_opts.h"
#include "../transfer.h"
#include "../validation.h"
#include "../worker.h"
#include "./nmg_boolean.h"
#include "./tessellate.h"

static const char FACETIZE_TEST_FAULT_ENV[] =
    "LIBGED_FACETIZE_TEST_FAULT";
static const char FACETIZE_TEST_FAULT_FILE_ENV[] =
    "LIBGED_FACETIZE_TEST_FAULT_FILE";
static const char FACETIZE_TEST_FAULT_TESS_READY[] = "tess-before-ready";
static const char FACETIZE_TEST_FAULT_STAGE_DONE[] = "stage-before-done";
static const char FACETIZE_TEST_FAULT_WRITER_PROCEED[] =
    "writer-before-proceed";
static const char FACETIZE_TEST_FAULT_WRITER_DONE[] = "writer-before-done";
static const char FACETIZE_TEST_FAULT_NMG_READY[] = "nmg-before-ready";

/**
 * Terminate at a named protocol boundary when explicitly armed by a test.
 * Requiring both an exact stage/object selector and a writable one-shot claim
 * file keeps this inaccessible during normal operation and prevents restarted
 * workers from repeatedly taking the same fault.
 */
static void
facetize_test_fault(const char *stage, const char *object_name)
{
    const char *fault = getenv(FACETIZE_TEST_FAULT_ENV);
    const char *claim_file = getenv(FACETIZE_TEST_FAULT_FILE_ENV);
    if (!fault || !fault[0] || !claim_file || !claim_file[0] ||
	    !stage || !object_name)
	return;

    std::string expected = std::string(stage) + ":" + object_name;
    if (expected != fault || bu_file_exists(claim_file, NULL))
	return;

    FILE *claim = fopen(claim_file, "wb");
    if (!claim)
	return;
    bool written = fwrite(expected.data(), 1, expected.size(), claim) ==
	expected.size();
    bool closed = fclose(claim) == 0;
    if (!written || !closed)
	return;

    bu_exit(BRLCAD_ERROR, "FACETIZE: injected test fault at %s for %s\n",
	    stage, object_name);
}

static void
facetize_payload_add(size_t *payload_size, size_t count, size_t item_size)
{
    if (!payload_size || !item_size)
	return;

    const size_t max_size = std::numeric_limits<size_t>::max();
    if (*payload_size == max_size)
	return;
    if (count > (max_size - *payload_size) / item_size) {
	*payload_size = max_size;
	return;
    }
    *payload_size += count * item_size;
}

static size_t
facetize_bot_payload_size(const struct rt_bot_internal *bot)
{
    if (!bot)
	return 0;

    size_t payload_size = sizeof(struct rt_bot_internal);
    facetize_payload_add(&payload_size, bot->num_faces,
	    3 * sizeof(*bot->faces));
    facetize_payload_add(&payload_size, bot->num_vertices,
	    3 * sizeof(*bot->vertices));
    if (bot->thickness)
	facetize_payload_add(&payload_size, bot->num_faces,
		sizeof(*bot->thickness));
    if (bot->face_mode) {
	size_t face_mode_bytes = bot->num_faces / CHAR_BIT;
	if (bot->num_faces % CHAR_BIT)
	    face_mode_bytes++;
	facetize_payload_add(&payload_size, face_mode_bytes, 1);
    }
    if (bot->normals)
	facetize_payload_add(&payload_size, bot->num_normals,
		3 * sizeof(*bot->normals));
    if (bot->face_normals)
	facetize_payload_add(&payload_size, bot->num_face_normals,
		3 * sizeof(*bot->face_normals));
    if (bot->uvs)
	facetize_payload_add(&payload_size, bot->num_uvs,
		3 * sizeof(*bot->uvs));
    if (bot->face_uvs)
	facetize_payload_add(&payload_size, bot->num_face_uvs,
		3 * sizeof(*bot->face_uvs));
    return payload_size;
}

static size_t
facetize_resident_size()
{
    ssize_t resident_size = bu_mem(BU_MEM_PROCESS_RESIDENT, NULL);
    return (resident_size > 0) ? (size_t)resident_size : 0;
}

static void
rt_pnts_free(struct rt_pnts_internal *pnts)
{
    struct pnt_normal *rpnt = (struct pnt_normal *)pnts->point;
    if (rpnt) {
	struct pnt_normal *entry;
	while (BU_LIST_WHILE(entry, pnt_normal, &(rpnt->l))) {
	    BU_LIST_DEQUEUE(&(entry->l));
	    BU_PUT(entry, struct pnt_normal);
	}
	BU_PUT(rpnt, struct pnt_normal);
    }
    BU_PUT(pnts, struct rt_pnts_internal);
}

static void
method_setup(tess_opts *s)
{
    if (!s)
	return;

    std::vector<std::string> *methods = &s->method_opts.methods;
    if (!methods->size())
	*methods = tess_default_methods();

    // Now that we've set any default overrides for multiple types, get the
    // method specific options for each method set up
    s->nmg_options.sync(s->method_opts);
    s->cm_options.sync(s->method_opts);
    s->spsr_options.sync(s->method_opts);

    // Set the sampling options.  If CM is active we will be using its settings
    // to sample first, so default to those values.
    bool sample_sync = false;
    if (std::find(methods->begin(), methods->end(), std::string("CM")) != methods->end()) {
	s->pnt_options.sync(s->cm_options);
	sample_sync = true;
    }
    if (!sample_sync && std::find(methods->begin(), methods->end(), std::string("SPSR")) != methods->end()) {
	s->pnt_options.sync(s->spsr_options);
    }
}

static int
dp_tessellate(struct rt_bot_internal **obot, struct bu_vls *method_flag, struct ged *gedp, struct directory *dp, tess_opts *s)
{
    if (!s || !obot || !method_flag || !gedp || !dp)
	return BRLCAD_ERROR;

    struct db_i *dbip = gedp->dbip;

    std::set<std::string> mset;
    for (size_t i = 0; i < s->method_opts.methods.size(); i++) {
	mset.insert(s->method_opts.methods[i]);
    }

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0) {
	bu_log("rt_db_get_internal failed for %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }

    struct rt_pnts_internal *pnts = NULL;
    bool free_pnts = false;
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
	    ret = BRLCAD_OK;
	    goto dp_tessellate_cleanup;
	case ID_PNTS:
	    // At this low level, allow point processing methods to have a
	    // crack at a point primitive to wrap it in a mesh.  If we don't
	    // want facetize generating a mesh from a pnts object during a
	    // general tree walk, we should skip point objects at the higher
	    // level.  Even if we don't want to have facetize turn pnts objects
	    // into meshes (which we probably don't), that is a capability we
	    // will most likely want to expose through the pnts command - which
	    // will make this executable useful to more than just facetize.
	    if (mset.find(std::string("SPSR")) == mset.end()) {
		ret = BRLCAD_OK;
		goto dp_tessellate_cleanup;
	    }

	    // If we are going to try a pnts wrapping, there are only a few
	    // candidates in the fallback methods list that we can use.
	    mset.erase(std::string("NMG"));
	    mset.erase(std::string("CM"));

	    // point the pnts arguments to the internal point data
	    pnts = (struct rt_pnts_internal *)intern.idb_ptr;

	    goto pnt_sampling_methods;
	case ID_HALF:
	    // Halfspace objects are handled specially by BRL-CAD.
	    ret = BRLCAD_OK;
	    goto dp_tessellate_cleanup;
	case ID_BOT:
	    bot = (struct rt_bot_internal *)(intern.idb_ptr);
	    propVal = (int)rt_bot_propget(bot, "type");
	    // Surface meshes are zero volume, and thus no-op
	    if (propVal == RT_BOT_SURFACE) {
		ret = BRLCAD_OK;
		goto dp_tessellate_cleanup;
	    }
	    // Plate mode BoTs need an explicit volume representation
	    if (propVal == RT_BOT_PLATE || propVal == RT_BOT_PLATE_NOCOS) {
		bu_vls_sprintf(method_flag, "PLATE");
		fastf_t bot_area = bg_trimesh_area(bot->faces, bot->num_faces, (const point_t *)bot->vertices, bot->num_vertices);
		ret = rt_bot_plate_to_vol(obot, bot, 0, 1, 0.1*bot_area, 0.2);
		goto dp_tessellate_cleanup;
	    }
	    // Volumetric bot - if it can be manifold we're good, but if
	    // not we need to try and repair it.
	    if (!bot_is_manifold(bot)) {
		// Nope - try repairing
		struct rt_bot_repair_info settings = RT_BOT_REPAIR_INFO_INIT;
		// We're aggressive preparing facetize inputs, since non-lint-passing
		// "repairs" may still be enough to allow booleans to succeed.
		settings.strict = 0;
		bu_vls_sprintf(method_flag, "REPAIR");
		ret = rt_bot_repair(obot, bot, &settings);
	    } else {
		// Already a valid BoT - tessellate is a no-op.
		*obot = NULL;
		ret = BRLCAD_OK;
		goto dp_tessellate_cleanup;
	    }
	case ID_BREP:
	    // TODO - need to handle plate mode NURBS the way we handle plate mode BoTs
	default:
	    break;
    }

    if (ret == BRLCAD_OK && *obot) {
	// If we already have the output bot, return
	goto dp_tessellate_cleanup;
    }

    // For brep in particular, we have a cheat we can try.  Do a brep->csg
    // conversion and see if the resulting CSG tree can be facetized.
    if (intern.idb_minor_type == ID_BREP) {
	ret = _brep_csg_tessellate(obot, gedp, dp, s);
	if (ret == BRLCAD_OK) {
	    bu_vls_sprintf(method_flag, "NMG_BREP_CSG");
	    goto dp_tessellate_cleanup;
	}
    }

    // If we got this far, it's not a special case.  Start trying whatever tessellation methods
    // are enabled

    if (mset.find(std::string("NMG")) != mset.end()) {
	// NMG is best, if it works
	ret = _nmg_tessellate(obot, &intern, s);
	if (ret == BRLCAD_OK) {
	    bu_vls_sprintf(method_flag, "NMG");
	    goto dp_tessellate_cleanup;
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
	    free_pnts = (pnts != NULL);
	}
	if (pnts) {
	    s->cm_options.sync(s->pnt_options);
	    struct pnt_normal *seed = BU_LIST_PNEXT(pnt_normal, (struct pnt_normal *)pnts->point);
	    ret = continuation_mesh(obot, dbip, dp->d_namep, s, seed->v);
	    if (ret == BRLCAD_OK) {
		bu_vls_sprintf(method_flag, "CM");
		goto dp_tessellate_cleanup;
	    }
	}
    }

pnt_sampling_methods:

    if (mset.find(std::string("SPSR")) != mset.end()) {
	if (!pnts) {
	    pnts = _tess_pnts_sample(dp->d_namep, dbip, s);
	    free_pnts = (pnts != NULL);
	} else {
	    if (!s->spsr_options.equals(s->pnt_options)) {
		s->pnt_options.sync(s->spsr_options);
		if (free_pnts)
		    rt_pnts_free(pnts);
		pnts = _tess_pnts_sample(dp->d_namep, dbip, s);
		free_pnts = (pnts != NULL);
	    }
	}
	if (pnts) {
	    s->spsr_options.sync(s->pnt_options);
	    ret = spsr_mesh(obot, dbip, pnts, s);
	    if (ret == BRLCAD_OK) {
		bu_vls_sprintf(method_flag, "SPSR");
		goto dp_tessellate_cleanup;
	    }
	}
    }

    bu_vls_sprintf(method_flag, "FAIL");

    {
	std::ostringstream methods;
	for (std::set<std::string>::iterator it = mset.begin(); it != mset.end(); ++it) {
	    if (it != mset.begin())
		methods << ", ";
	    methods << *it;
	}
	std::string method_list = methods.str();
	bu_log("FACETIZE_PROCESS: failed to tessellate %s (%s) with active method(s): %s. Try a different --methods list, increase the method max_time, or inspect the primitive with 'lint'.\n",
		dp->d_namep,
		intern.idb_meth ? intern.idb_meth->ft_label : "unknown",
		method_list.empty() ? "none" : method_list.c_str());
    }
    ret = BRLCAD_ERROR;

dp_tessellate_cleanup:
    if (free_pnts && pnts)
	rt_pnts_free(pnts);
    if (ret != BRLCAD_OK && *obot) {
	_tess_facetize_free_bot(*obot);
	*obot = NULL;
    }
    rt_db_free_internal(&intern);
    return ret;
}

void
print_methods_info()
{
    nmg_opts nopts;
    cm_opts cmopts;
    spsr_opts spsropts;

    std::string info;
    info.append(nopts.print_options_help());
    info.append(std::string("\n"));
    info.append(cmopts.print_options_help());
    info.append(std::string("\n"));
    info.append(spsropts.print_options_help());
    fprintf(stdout, "%s\n", info.c_str());
}

void
print_tess_methods()
{
    fprintf(stdout, "NMG CM SPSR");
}

static int
facetize_server_request(struct ged *gedp, struct db_i *result_dbip,
	const FacetizeWorkerRequest &request,
	FacetizeWorkerServer &worker_channel, bool *result_sent)
{
    if (!gedp || !result_sent ||
	    request.operation != FacetizeWorkerOperation::TessellatePrimitive ||
	    request.input_names.size() != 1)
	return BRLCAD_ERROR;
    *result_sent = false;

    if (!request.primitive.cache_directory.empty()) {
	bu_mkdir(request.primitive.cache_directory.c_str());
	if (bu_setenv("BU_DIR_CACHE",
		request.primitive.cache_directory.c_str(), 1) != 0)
	    return BRLCAD_ERROR;
    }

    tess_opts options;
    options.method_opts.methods = request.primitive.methods;
    for (const std::string &option_string :
	    request.primitive.method_options) {
	const char *option = option_string.c_str();
	if (_tess_method_opts(NULL, 1, &option, &options.method_opts) != 1)
	    return BRLCAD_ERROR;
    }
    method_setup(&options);
    if (request.primitive.point_limit > 0) {
	options.pnt_options.max_pnts = request.primitive.point_limit;
	options.cm_options.max_pnts = request.primitive.point_limit;
	options.spsr_options.max_pnts = request.primitive.point_limit;
    }

    const char *object_name = request.input_names.front().c_str();
    struct directory *dp = db_lookup(gedp->dbip, object_name, LOOKUP_QUIET);
    if (!dp || dp->d_major_type != DB5_MAJORTYPE_BRLCAD)
	return BRLCAD_ERROR;

    struct rt_bot_internal *obot = NULL;
    struct bu_vls method = BU_VLS_INIT_ZERO;
    int ret = dp_tessellate(&obot, &method, gedp, dp, &options);
    if (ret == BRLCAD_OK && obot) {
	// Waiting here lets the parent stop an over-time tessellation without
	// risking a partially replaced object in the working database.
	facetize_test_fault(FACETIZE_TEST_FAULT_TESS_READY, object_name);
	size_t payload_size = facetize_bot_payload_size(obot);
	if (!worker_channel.send_write_ready(payload_size,
		facetize_resident_size()) ||
		!worker_channel.receive_write_proceed() ||
		!worker_channel.send_write_started()) {
	    _tess_facetize_free_bot(obot);
	    ret = BRLCAD_ERROR;
	} else {
	    struct db_i *output_dbip = result_dbip ? result_dbip : gedp->dbip;
	    const char *output_name = result_dbip ?
		FACETIZE_WORKER_RESULT_OBJECT : object_name;
	    ret = _tess_facetize_write_bot(output_dbip, obot, output_name,
		    bu_vls_cstr(&method));
	    if (result_dbip && ret == BRLCAD_OK)
		facetize_test_fault(FACETIZE_TEST_FAULT_STAGE_DONE,
			object_name);
	    if (!worker_channel.send_write_result(ret,
		    facetize_resident_size())) {
		ret = BRLCAD_ERROR;
	    } else {
		*result_sent = true;
	    }
	}
    } else if (obot) {
	_tess_facetize_free_bot(obot);
    }
    bu_vls_free(&method);
    return ret;
}

static int
facetize_server(const char *work_file, const char *result_file)
{
    int server_status = BRLCAD_OK;
    setvbuf(stdout, NULL, _IONBF, 0);

    struct ged *gedp = ged_open("db", work_file, 1);
    if (!gedp)
	return BRLCAD_ERROR;

    struct db_i *result_dbip = result_file ?
	db_create(result_file, BRLCAD_DB_FORMAT_LATEST) : NULL;
    if (result_file && !result_dbip) {
	ged_close(gedp);
	return BRLCAD_ERROR;
    }

    FacetizeWorkerServer worker_channel(stdin, stdout);
    while (true) {
	FacetizeWorkerRequest request;
	FacetizeWorkerReadResult read_result =
	    worker_channel.receive_request(request);
	if (read_result == FacetizeWorkerReadResult::End)
	    break;
	if (read_result == FacetizeWorkerReadResult::Error) {
	    server_status = BRLCAD_ERROR;
	    break;
	}

	bool result_sent = false;
	int ret = facetize_server_request(gedp, result_dbip,
		request, worker_channel, &result_sent);
	if (!result_sent && !worker_channel.send_tessellation_result(ret,
		facetize_resident_size())) {
	    server_status = BRLCAD_ERROR;
	    break;
	}
    }

    if (result_dbip)
	db_close(result_dbip);
    ged_close(gedp);
    return server_status;
}

static int
facetize_validation_server(const char *source_file)
{
    if (!source_file)
	return BRLCAD_ERROR;

    setvbuf(stdout, NULL, _IONBF, 0);
    struct db_i *dbip = db_open(source_file, DB_OPEN_READONLY);
    if (!dbip)
	return BRLCAD_ERROR;
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	return BRLCAD_ERROR;
    }

    int server_status = BRLCAD_OK;
    FacetizeWorkerServer channel(stdin, stdout);
    while (true) {
	FacetizeWorkerRequest request;
	FacetizeWorkerReadResult read_result =
	    channel.receive_request(request);
	if (read_result == FacetizeWorkerReadResult::End)
	    break;
	if (read_result == FacetizeWorkerReadResult::Error ||
		request.operation != FacetizeWorkerOperation::ValidateCsg ||
		request.input_names.size() != 1) {
	    server_status = BRLCAD_ERROR;
	    break;
	}

	const std::string &object_name = request.input_names.front();
	double surface_area = -1.0;
	double volume = -1.0;
	long crossings = facetize_csg_metrics(dbip, object_name.c_str(),
		&surface_area, &volume);
	int result = (crossings >= 0) ? BRLCAD_OK : BRLCAD_ERROR;
	if (!channel.send_csg_result(result, crossings, surface_area, volume,
		facetize_resident_size())) {
	    server_status = BRLCAD_ERROR;
	    break;
	}
    }

    db_close(dbip);
    return server_status;
}

static int
facetize_region_server_request(struct _ged_facetize_state *state,
	struct db_i *work_dbip, struct rt_wdb *work_wdbp,
	struct db_i *staging_dbip, struct rt_wdb *staging_wdbp,
	const char *object_name, FacetizeWorkerServer &channel,
	bool *result_sent)
{
    if (!state || !work_dbip || !work_wdbp || !staging_dbip ||
	    !staging_wdbp || !object_name || !result_sent)
	return BRLCAD_ERROR;
    *result_sent = false;

    struct db_i *result_dbip = db_create_inmem();
    if (!result_dbip)
	return BRLCAD_ERROR;

    const char *inputs[] = {object_name};
    int ret = _ged_facetize_booleval_tri_to_db(state, work_dbip,
	    work_wdbp, 1, inputs, FACETIZE_WORKER_RESULT_OBJECT, &rt_vlfree,
	    result_dbip, 0, 0);
    struct rt_db_internal result_internal;
    RT_DB_INTERNAL_INIT(&result_internal);
    bool result_internal_consumed = false;
    struct directory *result_dp = (ret == BRLCAD_OK) ?
	db_lookup(result_dbip, FACETIZE_WORKER_RESULT_OBJECT, LOOKUP_QUIET) :
	RT_DIR_NULL;
    if (!result_dp || result_dp->d_minor_type != ID_BOT ||
	    rt_db_get_internal(&result_internal, result_dp, result_dbip,
		NULL) < 0) {
	ret = BRLCAD_ERROR;
    }

    if (ret == BRLCAD_OK) {
	struct rt_bot_internal *result_bot =
	    (struct rt_bot_internal *)result_internal.idb_ptr;
	size_t payload_size = facetize_bot_payload_size(result_bot);
	if (!channel.send_region_write_ready(payload_size,
		facetize_resident_size(), state->tolerated_failures) ||
		!channel.receive_write_proceed() ||
		!channel.send_write_started()) {
	    ret = BRLCAD_ERROR;
	} else {
	    struct directory *old_dp = db_lookup(staging_dbip,
		    FACETIZE_WORKER_RESULT_OBJECT, LOOKUP_QUIET);
	    if (old_dp && (db_delete(staging_dbip, old_dp) != 0 ||
		    db_dirdelete(staging_dbip, old_dp) != 0)) {
		ret = BRLCAD_ERROR;
	    } else {
		ret = wdb_put_internal(staging_wdbp,
			FACETIZE_WORKER_RESULT_OBJECT, &result_internal,
			1.0);
		result_internal_consumed = true;
	    }
	    if (!channel.send_write_result(ret, facetize_resident_size()))
		ret = BRLCAD_ERROR;
	    else
		*result_sent = true;
	}
    }

    if (!result_internal_consumed && result_internal.idb_ptr)
	rt_db_free_internal(&result_internal);
    db_close(result_dbip);
    return ret;
}

static int
facetize_region_server(const char *work_file, const char *source_file,
	const char *result_file)
{
    if (!work_file || !source_file || !result_file)
	return BRLCAD_ERROR;

    setvbuf(stdout, NULL, _IONBF, 0);
    struct db_i *work_dbip = db_open(work_file, DB_OPEN_READONLY);
    struct db_i *source_dbip = db_open(source_file, DB_OPEN_READONLY);
    if (!work_dbip || !source_dbip) {
	if (work_dbip)
	    db_close(work_dbip);
	if (source_dbip)
	    db_close(source_dbip);
	return BRLCAD_ERROR;
    }
    if (db_dirbuild(work_dbip) < 0 || db_dirbuild(source_dbip) < 0) {
	db_close(source_dbip);
	db_close(work_dbip);
	return BRLCAD_ERROR;
    }
    db_update_nref(work_dbip);

    struct rt_wdb *work_wdbp = wdb_dbopen(work_dbip,
	    RT_WDB_TYPE_DB_DEFAULT);
    if (!work_wdbp) {
	db_close(source_dbip);
	db_close(work_dbip);
	return BRLCAD_ERROR;
    }

    (void)bu_file_delete(result_file);
    struct db_i *staging_dbip = db_create(result_file,
	    BRLCAD_DB_FORMAT_LATEST);
    struct rt_wdb *staging_wdbp = staging_dbip ?
	wdb_dbopen(staging_dbip, RT_WDB_TYPE_DB_DEFAULT) : NULL;
    if (!staging_dbip || !staging_wdbp) {
	if (staging_dbip)
	    db_close(staging_dbip);
	db_close(source_dbip);
	db_close(work_dbip);
	return BRLCAD_ERROR;
    }

    struct bu_vls failure_message = BU_VLS_INIT_ZERO;
    struct bu_vls tolerated_failure_log = BU_VLS_INIT_ZERO;
    struct _ged_facetize_state state = {};
    state.verbosity = -1;
    state.failure_msg = &failure_message;
    state.tolerated_failure_log = &tolerated_failure_log;
    state.dbip = source_dbip;

    int server_status = BRLCAD_OK;
    FacetizeWorkerServer channel(stdin, stdout);
    while (true) {
	FacetizeWorkerRequest request;
	FacetizeWorkerReadResult read_result =
	    channel.receive_request(request);
	if (read_result == FacetizeWorkerReadResult::End)
	    break;
	if (read_result == FacetizeWorkerReadResult::Error ||
		request.operation != FacetizeWorkerOperation::EvaluateRegion ||
		request.input_names.size() != 1) {
	    server_status = BRLCAD_ERROR;
	    break;
	}

	const std::string &object_name = request.input_names.front();
	state.no_empty = request.region.no_empty;
	state.no_fixup = request.region.no_fixup;
	state.tolerate_failures = request.region.tolerate_failures;
	state.error_flag = 0;
	state.facetize_tree = NULL;
	state.tolerated_failures = 0;
	state.tolerated_failure_details = 0;
	state.tolerated_failure_omitted = 0;
	bu_vls_trunc(&failure_message, 0);
	bu_vls_trunc(&tolerated_failure_log, 0);
	bool result_sent = false;
	int ret = facetize_region_server_request(&state, work_dbip,
		work_wdbp, staging_dbip, staging_wdbp, object_name.c_str(),
		channel, &result_sent);
	if (ret != BRLCAD_OK && bu_vls_strlen(&failure_message))
	    bu_log("FACETIZE: region Boolean evaluation failed for %s: %s\n",
		    object_name.c_str(), bu_vls_cstr(&failure_message));
	if (!result_sent && !channel.send_tessellation_result(ret,
		facetize_resident_size())) {
	    server_status = BRLCAD_ERROR;
	    break;
	}
    }

    bu_vls_free(&tolerated_failure_log);
    bu_vls_free(&failure_message);
    db_close(staging_dbip);
    db_close(source_dbip);
    db_close(work_dbip);
    return server_status;
}

static int
facetize_writer(const char *work_file)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    struct db_i *dbip = db_open(work_file, DB_OPEN_READWRITE);
    if (!dbip)
	return BRLCAD_ERROR;
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	return BRLCAD_ERROR;
    }

    int writer_status = BRLCAD_OK;
    FacetizeWorkerServer channel(stdin, stdout);
    while (true) {
	FacetizeCommitRequest request;
	FacetizeWorkerReadResult read_result = channel.receive_commit(request);
	if (read_result == FacetizeWorkerReadResult::End)
	    break;
	if (read_result == FacetizeWorkerReadResult::Error ||
		!channel.send_write_ready(request.payload_size,
		    facetize_resident_size())) {
	    writer_status = BRLCAD_ERROR;
	    break;
	}
	facetize_test_fault(FACETIZE_TEST_FAULT_WRITER_PROCEED,
		request.object_name.c_str());
	if (!channel.receive_write_proceed()) {
	    writer_status = BRLCAD_ERROR;
	    break;
	}
	if (!channel.send_write_started()) {
	    writer_status = BRLCAD_ERROR;
	    break;
	}

	int ret = facetize_transfer_staged_object(dbip,
		request.result_file.c_str(), request.object_name.c_str(), ID_BOT);
	if (ret == BRLCAD_OK)
	    facetize_test_fault(FACETIZE_TEST_FAULT_WRITER_DONE,
		    request.object_name.c_str());
	if (!channel.send_write_result(ret, facetize_resident_size())) {
	    writer_status = BRLCAD_ERROR;
	    break;
	}
    }

    db_close(dbip);
    return writer_status;
}

static int
facetize_nmg_server(const char *source_file, const char *result_file)
{
    if (!source_file || !result_file)
	return BRLCAD_ERROR;

    setvbuf(stdout, NULL, _IONBF, 0);
    struct ged *gedp = ged_open("db", source_file, 1);
    if (!gedp)
	return BRLCAD_ERROR;

    struct db_i *result_dbip = db_create(result_file,
	    BRLCAD_DB_FORMAT_LATEST);
    if (!result_dbip) {
	ged_close(gedp);
	return BRLCAD_ERROR;
    }

    FacetizeWorkerServer channel(stdin, stdout);
    FacetizeWorkerRequest request;
    FacetizeWorkerReadResult read_result = channel.receive_request(request);
    int server_status = BRLCAD_OK;
    FacetizeNmgBooleanResult *result = NULL;
    int ret = (read_result == FacetizeWorkerReadResult::Request) ?
	facetize_nmg_boolean_evaluate(gedp, request, &result) : BRLCAD_ERROR;
    if (read_result != FacetizeWorkerReadResult::Request) {
	server_status = BRLCAD_ERROR;
    } else if (ret == BRLCAD_OK) {
	size_t payload_size = facetize_nmg_boolean_payload_size(result);
	facetize_test_fault(FACETIZE_TEST_FAULT_NMG_READY,
		request.output_name.c_str());
	if (!channel.send_write_ready(payload_size,
		facetize_resident_size()) ||
		!channel.receive_write_proceed() ||
		!channel.send_write_started()) {
	    server_status = BRLCAD_ERROR;
	} else {
	    ret = facetize_nmg_boolean_write(result_dbip, result);
	    if (!channel.send_write_result(ret, facetize_resident_size()))
		server_status = BRLCAD_ERROR;
	}
    } else if (!channel.send_tessellation_result(ret,
	    facetize_resident_size())) {
	server_status = BRLCAD_ERROR;
    }

    facetize_nmg_boolean_destroy(result);
    db_close(result_dbip);
    ged_close(gedp);
    return server_status;
}

extern "C" int
facetize_process(int argc, const char **argv)
{
    if (!argc || !argv)
	return BRLCAD_ERROR;

    bu_setprogname(argv[0]);

    // Done with prog name
    argc--; argv++;

    static const char *usage = "Usage: ged_exec facetize_process [options] file.g input_obj [input_object_2 ...]\n";
    int print_help = 0;
    struct bu_vls cache_dir = BU_VLS_INIT_ZERO;
    tess_opts s;

    int list_methods = 0;
    int server_mode = 0;
    int writer_mode = 0;
    int validation_server_mode = 0;
    int region_server_mode = 0;
    int nmg_server_mode = 0;
    int max_time = 0;
    int max_pnts = 0;
    int worker_threads = 0;
    struct bu_vls result_file = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[16];
    BU_OPT(d[ 0],  "h",         "help",                         "",                  NULL,           &print_help, "Print help and exit");
    BU_OPT(d[ 1],   "", "list-methods",                         "",                  NULL,         &list_methods, "List available tessellation methods.  When used with -h, print an informational summary of each method.");
    BU_OPT(d[ 2],  "O",    "overwrite",                         "",                  NULL,    &(s.overwrite_obj), "Replace original object with BoT");
    BU_OPT(d[ 3],   "",      "methods",                "m1 m2 ...", &_tess_active_methods,        &s.method_opts, "List of active methods to use for this tessellation attempt");
    BU_OPT(d[ 4],   "",  "method-opts",  "M opt1=val opt2=val ...",    &_tess_method_opts,        &s.method_opts, "Set options for method M.  If specified just a method M and the -h option, print documentation about method options.");
    BU_OPT(d[ 5],   "",     "max-time",                        "#",           &bu_opt_int,             &max_time, "Maximum number of seconds to allow for runtime (not supported by all methods).");
    BU_OPT(d[ 6],   "",     "max-pnts",                        "#",           &bu_opt_int,             &max_pnts, "Maximum number of pnts to use when applying ray sampling methods.");
    BU_OPT(d[ 7],   "",     "cache-dir",                     "dir",           &bu_opt_vls,            &cache_dir, "Directory to use for cached outputs (default is libbu cache directory).");
    BU_OPT(d[ 8],   "",          "server",                        "",                  NULL,          &server_mode, "Run as a persistent worker.");
    BU_OPT(d[ 9],   "",         "threads",                       "#",           &bu_opt_int,       &worker_threads, "Maximum CPU threads available to this process.");
    BU_OPT(d[10],   "",     "result-file",                  "file.g",           &bu_opt_vls,          &result_file, "Write server results to a staging database.");
    BU_OPT(d[11],   "",          "writer",                        "",                  NULL,          &writer_mode, "Run as a persistent staged-result writer.");
    BU_OPT(d[12],   "", "validation-server",                        "",                  NULL, &validation_server_mode, "Run as a persistent CSG validation worker.");
    BU_OPT(d[13],   "",     "region-server",                        "",                  NULL,     &region_server_mode, "Run as a persistent region Boolean worker.");
    BU_OPT(d[14],   "",        "nmg-server",                        "",                  NULL,        &nmg_server_mode, "Run one isolated NMG Boolean request.");
    BU_OPT_NULL(d[15]);

    /* parse options */
    struct bu_vls omsg = BU_VLS_INIT_ZERO;
    argc = bu_opt_parse(&omsg, argc, argv, d);
    if (argc < 0) {
	bu_log("Option parsing error: %s\n", bu_vls_cstr(&omsg));
	bu_vls_free(&omsg);
	bu_exit(BRLCAD_ERROR, "%s failed", bu_getprogname());
    }
    bu_vls_free(&omsg);

    if (worker_threads < 0 || worker_threads > MAX_PSW) {
	bu_vls_free(&cache_dir);
	bu_vls_free(&result_file);
	return BRLCAD_ERROR;
    }
    if (worker_threads)
	bu_avail_cpus_set((size_t)worker_threads);

    int worker_mode_count = server_mode + writer_mode +
	validation_server_mode + region_server_mode + nmg_server_mode;
    if (worker_mode_count > 1) {
	bu_vls_free(&cache_dir);
	bu_vls_free(&result_file);
	return BRLCAD_ERROR;
    }

    if (list_methods && print_help) {
	print_methods_info();
	bu_vls_free(&cache_dir);
	bu_vls_free(&result_file);
	return BRLCAD_OK;
    }

    if (list_methods) {
	print_tess_methods();
	bu_vls_free(&cache_dir);
	bu_vls_free(&result_file);
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
	bu_vls_free(&cache_dir);
	bu_vls_free(&result_file);
        return BRLCAD_OK;
    }

    if (writer_mode) {
	int ret = (argc == 1 && !bu_vls_strlen(&result_file)) ?
	    facetize_writer(argv[0]) : BRLCAD_ERROR;
	bu_vls_free(&cache_dir);
	bu_vls_free(&result_file);
	return ret;
    }

    if (validation_server_mode) {
	int ret = (argc == 1 && !bu_vls_strlen(&result_file)) ?
	    facetize_validation_server(argv[0]) : BRLCAD_ERROR;
	bu_vls_free(&cache_dir);
	bu_vls_free(&result_file);
	return ret;
    }

    if (region_server_mode) {
	int ret = (argc == 2 && bu_vls_strlen(&result_file)) ?
	    facetize_region_server(argv[0], argv[1],
		    bu_vls_cstr(&result_file)) : BRLCAD_ERROR;
	bu_vls_free(&cache_dir);
	bu_vls_free(&result_file);
	return ret;
    }

    if (nmg_server_mode) {
	int ret = (argc == 1 && bu_vls_strlen(&result_file)) ?
	    facetize_nmg_server(argv[0], bu_vls_cstr(&result_file)) :
	    BRLCAD_ERROR;
	bu_vls_free(&cache_dir);
	bu_vls_free(&result_file);
	return ret;
    }

    if (server_mode) {
	int ret = (argc == 1) ?
	    facetize_server(argv[0], bu_vls_strlen(&result_file) ?
		    bu_vls_cstr(&result_file) : NULL) :
	    BRLCAD_ERROR;
	bu_vls_free(&cache_dir);
	bu_vls_free(&result_file);
	return ret;
    }

    // If we have a non-default cache directory specified, set it up
    if (bu_vls_strlen(&cache_dir)) {
	// Make sure it's there first
	bu_mkdir(bu_vls_cstr(&cache_dir));
	// Set the environment variable
	bu_setenv("BU_DIR_CACHE", bu_vls_cstr(&cache_dir), 1);
    }

    // Do the setup for the various tessellation methods.
    method_setup(&s);

    if (argc < 2) {
	bu_log("%s", usage);
	bu_vls_free(&cache_dir);
	bu_vls_free(&result_file);
	return BRLCAD_ERROR;
    }

    // Open the database
    struct ged *gedp = ged_open("db", argv[0], 1);
    if (!gedp) {
	bu_vls_free(&cache_dir);
	bu_vls_free(&result_file);
	return BRLCAD_ERROR;
    }

    // Translate specified object names to directory pointers
    struct bu_ptbl dps = BU_PTBL_INIT_ZERO;
    for (int i = 1; i < argc; i++) {
	struct directory *dp = db_lookup(gedp->dbip, argv[i], LOOKUP_NOISY);
	if (!dp) {
	    bu_ptbl_free(&dps);
	    ged_close(gedp);
	    bu_vls_free(&cache_dir);
	    bu_vls_free(&result_file);
	    return BRLCAD_ERROR;
	}
	bu_ptbl_ins(&dps, (long *)dp);
    }

    // Tessellate each object.  Note that we're doing this in series rather
    // than parallel because of the risks of high memory consumption and/or
    // CPU utilization for individual object operations.
    int process_ret = BRLCAD_OK;
    for (size_t i = 0; i < BU_PTBL_LEN(&dps); i++) {

	// If this isn't a proper BRL-CAD object, tessellation is a no-op
	struct directory *dp = (struct directory *)BU_PTBL_GET(&dps, i);
	if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD)
	    continue;

	// Trigger the core tessellation routines
	struct rt_bot_internal *obot = NULL;
	struct bu_vls method_flag = BU_VLS_INIT_ZERO;
	if (dp_tessellate(&obot, &method_flag, gedp, dp, &s) != BRLCAD_OK) {
	    bu_vls_free(&method_flag);
	    process_ret = BRLCAD_ERROR;
	    break;
	}

	// If we didn't get anything and we had an OK code, just keep going
	if (!obot) {
	    bu_vls_free(&method_flag);
	    continue;
	}

	// If we've got something to write, handle it
	struct bu_vls obot_name = BU_VLS_INIT_ZERO;
	if (s.overwrite_obj) {
	    bu_vls_sprintf(&obot_name, "%s", dp->d_namep);
	} else {
	    bu_vls_sprintf(&obot_name, "%s_tess.bot", dp->d_namep);
	}
	// NOTE: _tess_facetize_write_bot frees obot
	int ret = _tess_facetize_write_bot(gedp->dbip, obot, bu_vls_cstr(&obot_name), bu_vls_cstr(&method_flag));
	bu_vls_free(&method_flag);
	bu_vls_free(&obot_name);
	if (ret != BRLCAD_OK) {
	    process_ret = BRLCAD_ERROR;
	    break;
	}

    }

    bu_ptbl_free(&dps);
    ged_close(gedp);
    bu_vls_free(&cache_dir);
    bu_vls_free(&result_file);

    return process_ret;
}

#include "../../include/plugin.h"
extern "C" {
struct ged_cmd_process_impl fp_impl = {
    facetize_process
};

const struct ged_cmd_process fp = { &fp_impl };
static const struct ged_process_plugin pinfo = { GED_API,  &fp };

COMPILER_DLLEXPORT const struct ged_process_plugin *ged_process_info(void)
{
    return &pinfo;
}
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
