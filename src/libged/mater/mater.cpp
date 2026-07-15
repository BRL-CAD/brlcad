/*                       M A T E R . C P P
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
/** @file libged/mater.c
 *
 * The mater command.
 *
 */

#include "common.h"

#include <map>
#include <set>
#include <regex>
#include <string>
#include <sstream>
#include <vector>

#include "bu/app.h"
#include "bu/cmdschema.h"
#include "bu/path.h"
#include "analyze.h"
#include "ged.h"
#include "../ged_private.h"

#include <string.h>
#include "../../librt/librt_private.h"


static const char *mater_usage = "Usage: mater [-s] object_name shader r [g b] inherit\n"
"              -d  source\n"
"              -d  clear\n"
"              -d  import [-v] file.density\n"
"              -d  export file.density\n"
"              -d  audit [file.density]\n"
"              -d  validate file.density\n"
"              -d  get [--tol <tolerance>] [[--id <pattern>] [--density <val>] [--name <pattern>] ...] [key]\n"
"              -d  set <id, density, name> [<id, density, name>] ...\n"
"              -d  map --ids-from-names [-d density_file] [-m map_file] [density_file] [map_file]\n"
"              -d  map --names-from-ids [density_file]\n";


static int
mater_shader(struct ged *gedp, size_t argc, const char *argv[])
{
    struct bu_attribute_value_set avs;
    static const char *prompt[] = {
	"Name of combination to edit? ", /* unused */
	"Specify shader.  Enclose spaces within quotes.  E.g., \"light invisible=1\"\nShader? ('del' to delete, '.' to skip) ",
	"R, G, B color values (0 to 255)? ('del' to delete, '.' to skip) ",
	"G component color value? ('.' to skip) ",
	"B component color value? ('.' to skip) ",
	"Should this object's shader override lower nodes? (y/n or '.' to skip) "
    };

    struct directory *dp = NULL;
    int r=0, g=0, b=0;
    struct rt_comb_internal *comb = NULL;
    struct rt_db_internal intern;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int offset = 0;

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", mater_usage);
	return GED_HELP;
    }

    GED_DB_LOOKUP(gedp, dp, argv[1], LOOKUP_NOISY, BRLCAD_ERROR);
    GED_CHECK_COMB(gedp, dp, BRLCAD_ERROR);
    GED_DB_GET_INTERN(gedp, &intern, dp, (fastf_t *)NULL, BRLCAD_ERROR);

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    /* deleting the color means we don't need to prompt for the green
     * and blue color channels, so offset our argument indices.
     */
    if (argc > 3) {
	struct bu_vls color = BU_VLS_INIT_ZERO;
	bu_vls_strcat(&color, argv[3]);
	bu_vls_trimspace(&color);
	if (bu_strncmp(bu_vls_addr(&color), "del", 3) == 0) {
	    offset=2;
	}
	bu_vls_free(&color);
    }

    /* need more arguments */
    if ((!offset && argc < 7) || (offset && argc < 5)) {
	/* help, let them know the old value */
	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "Current shader string = %s\n", bu_vls_addr(&comb->shader));
	} else if (argc == 3) {
	    if (!comb->rgb_valid)
		bu_vls_printf(gedp->ged_result_str, "Current color = (No color specified)\n");
	    else
		bu_vls_printf(gedp->ged_result_str, "Current color = %d %d %d\n", V3ARGS(comb->rgb));
	} else if (!offset && argc == 4) {
	    if (comb->rgb_valid)
		bu_vls_printf(gedp->ged_result_str, "Current green color value = %d\n", comb->rgb[1]);
	} else if (!offset && argc == 5) {
	    if (comb->rgb_valid)
		bu_vls_printf(gedp->ged_result_str, "Current blue color value = %d\n", comb->rgb[2]);
	} else if ((!offset && argc == 6) || (offset && argc == 4)) {
	    if (comb->inherit)
		bu_vls_printf(gedp->ged_result_str, "Current inheritance = 1: this node overrides lower nodes\n");
	    else
		bu_vls_printf(gedp->ged_result_str, "Current inheritance = 0: lower nodes (towards leaves) override\n");
	}

	bu_vls_printf(gedp->ged_result_str, "%s", prompt[argc+offset-1]);
	return GED_MORE;
    }


    /* too much */
    if ((!offset && argc > 7) || (offset && argc > 5)) {
	bu_vls_printf(gedp->ged_result_str, "Too many arguments.\n%s", mater_usage);
	return BRLCAD_ERROR;
    }

    /* Material */
    bu_vls_strcat(&vls, argv[2]);
    bu_vls_trimspace(&vls);
    if (bu_vls_strlen(&vls) == 0 || bu_strncmp(bu_vls_addr(&vls), "del", 3) == 0) {
	/* delete the current shader string */
	bu_vls_free(&comb->shader);
    } else {
	if (!BU_STR_EQUAL(bu_vls_addr(&vls), ".")) {
	    bu_vls_trunc(&comb->shader, 0);
	    if (rt_shader_to_list(bu_vls_addr(&vls), &comb->shader)) {
		bu_vls_printf(gedp->ged_result_str, "Problem with shader string [%s]", argv[2]);
		rt_db_free_internal(&intern);
		bu_vls_free(&vls);
		return BRLCAD_ERROR;
	    }
	}
    }
    bu_vls_free(&vls);

    /* Color */
    if (offset) {
	/* means argv[3] == "del" so remove the color */
	comb->rgb_valid = 0;
	comb->rgb[0] = comb->rgb[1] = comb->rgb[2] = 0;
    } else {
	struct bu_vls rgb = BU_VLS_INIT_ZERO;

	bu_vls_strcpy(&rgb, argv[3]); /* RED */
	bu_vls_trimspace(&rgb);

	if (BU_STR_EQUAL(bu_vls_addr(&rgb), ".")) {
	    if (!comb->rgb_valid) {
		bu_vls_printf(gedp->ged_result_str, "Color is not set, cannot skip by using existing RED color value");
		rt_db_free_internal(&intern);
		bu_vls_free(&rgb);
		return BRLCAD_ERROR;
	    }
	} else {
	    if (sscanf(bu_vls_addr(&rgb), "%d", &r) != 1 || r < 0 || 255 < r) {
		bu_vls_printf(gedp->ged_result_str, "Bad color value [%s]", argv[3]);
		rt_db_free_internal(&intern);
		bu_vls_free(&rgb);
		return BRLCAD_ERROR;
	    }
	    comb->rgb[0] = r;
	}

	bu_vls_strcpy(&rgb, argv[4]); /* GRN */
	bu_vls_trimspace(&rgb);

	if (BU_STR_EQUAL(bu_vls_addr(&rgb), ".")) {
	    if (!comb->rgb_valid) {
		bu_vls_printf(gedp->ged_result_str, "Color is not set, cannot skip by using existing GREEN color value");
		rt_db_free_internal(&intern);
		bu_vls_free(&rgb);
		return BRLCAD_ERROR;
	    }
	} else {
	    if (sscanf(bu_vls_addr(&rgb), "%d", &g) != 1 || g < 0 || 255 < g) {
		bu_vls_printf(gedp->ged_result_str, "Bad color value [%s]", argv[4]);
		rt_db_free_internal(&intern);
		bu_vls_free(&rgb);
		return BRLCAD_ERROR;
	    }
	    comb->rgb[1] = g;
	}

	bu_vls_strcpy(&rgb, argv[5]); /* BLU */
	bu_vls_trimspace(&rgb);

	if (BU_STR_EQUAL(bu_vls_addr(&rgb), ".")) {
	    if (!comb->rgb_valid) {
		bu_vls_printf(gedp->ged_result_str, "Color is not set, cannot skip by using existing BLUE color value");
		rt_db_free_internal(&intern);
		bu_vls_free(&rgb);
		return BRLCAD_ERROR;
	    }
	} else {
	    if (sscanf(bu_vls_addr(&rgb), "%d", &b) != 1 || b < 0 || 255 < b) {
		bu_vls_printf(gedp->ged_result_str, "Bad color value [%s]", argv[5]);
		rt_db_free_internal(&intern);
		bu_vls_free(&rgb);
		return BRLCAD_ERROR;
	    }
	    comb->rgb[2] = b;
	}

	bu_vls_free(&rgb);
	comb->rgb_valid = 1;
    }

    if (!BU_STR_EQUAL(argv[argc - 1], ".")) {
	comb->inherit = bu_str_true(argv[argc - 1]);
	if (comb->inherit > 1) {
	    bu_vls_printf(gedp->ged_result_str, "Inherit value should be 0 or 1");
	    rt_db_free_internal(&intern);
	    return BRLCAD_ERROR;
	}
    }

    bu_avs_init_empty(&avs);
    if (db5_get_attributes(gedp->dbip, &avs, dp)) {
	bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
	bu_avs_free(&avs);
	return BRLCAD_ERROR;
    }
    db5_sync_comb_to_attr(&avs, comb);

    GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);

    if (db5_update_attributes(dp, &avs, gedp->dbip)) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: failed to update attributes\n");
	bu_avs_free(&avs);
	return BRLCAD_ERROR;
    }

    bu_avs_free(&avs);

    return BRLCAD_OK;
}


static int
mater_source(struct ged *gedp)
{
    struct bu_vls d_path_dir = BU_VLS_INIT_ZERO;

    /* Check in priority order */
    if (db_lookup(gedp->dbip, GED_DB_DENSITY_OBJECT, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", gedp->dbip->dbi_filename);
	return BRLCAD_OK;
    }

    /* Try .density file in database path first */
    if (bu_path_component(&d_path_dir, gedp->dbip->dbi_filename, BU_PATH_DIRNAME)) {

	bu_vls_printf(&d_path_dir, "/.density");

	if (bu_file_exists(bu_vls_cstr(&d_path_dir), NULL)) {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&d_path_dir));
	    bu_vls_free(&d_path_dir);
	    return BRLCAD_OK;
	}
    }

    /* Try HOME */
    if (bu_dir(NULL, 0, BU_DIR_HOME, NULL)) {

	bu_vls_sprintf(&d_path_dir, "%s/.density", bu_dir(NULL, 0, BU_DIR_HOME, NULL));

	if (bu_file_exists(bu_vls_cstr(&d_path_dir), NULL)) {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&d_path_dir));
	    bu_vls_free(&d_path_dir);
	    return BRLCAD_OK;
	}
    }

    return BRLCAD_OK;
}


static int
mater_clear(struct ged *gedp)
{
    struct directory *dp;
    if ((dp = db_lookup(gedp->dbip, GED_DB_DENSITY_OBJECT, LOOKUP_QUIET)) != RT_DIR_NULL) {
	if (db_delete(gedp->dbip, dp) != 0 || db_dirdelete(gedp->dbip, dp) != 0) {
	    bu_vls_printf(gedp->ged_result_str, "Error removing density information from database.");
	    return BRLCAD_ERROR;
	}
	db_update_nref(gedp->dbip);
    }
    return BRLCAD_OK;
}


static int
mater_validate(struct ged *gedp, size_t argc, const char *argv[])
{
    int ecnt = 0;
    struct analyze_densities *a = NULL;
    long int tb_cnt = 0;
    struct bu_vls msgs = BU_VLS_INIT_ZERO;
    struct bu_mapped_file *dfile = NULL;
    char *buf = NULL;

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "%s", mater_usage);
	return BRLCAD_OK;
    }

    if (!bu_file_exists(argv[1], NULL)) {
	bu_vls_printf(gedp->ged_result_str, "Specified density file %s not found", argv[1]);
	return BRLCAD_ERROR;
    }

    dfile = bu_open_mapped_file(argv[1], "densities file");
    if (!dfile) {
	bu_vls_printf(gedp->ged_result_str, "Could not open density file - %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    buf = (char *)(dfile->buf);

    analyze_densities_create(&a);
    tb_cnt = analyze_densities_load(a, buf, &msgs, &ecnt);
    bu_close_mapped_file(dfile);

    if (!tb_cnt) {
	bu_vls_printf(gedp->ged_result_str, "no density information found in %s:\n%s", argv[1], bu_vls_cstr(&msgs));
	bu_vls_free(&msgs);
	analyze_densities_destroy(a);
	return BRLCAD_ERROR;
    }
    if (ecnt) {
	bu_vls_printf(gedp->ged_result_str, "errors parsing density file %s:\n%s", argv[1], bu_vls_cstr(&msgs));
	analyze_densities_destroy(a);
	bu_vls_free(&msgs);
	return BRLCAD_ERROR;
    }

    bu_vls_free(&msgs);
    analyze_densities_destroy(a);
    return BRLCAD_OK;
}


/* TODO - need --json or --csv or --output=fmt option(s) to this to
 * get information in machine readable form.  don't want people
 * parsing textual output...  Probably some specific options too, like
 * reporting all material names without ids or vice versa.
 */
static int
mater_audit(struct ged *gedp, size_t argc, const char *argv[])
{
    char *dsource = NULL;
    struct analyze_densities *a = NULL;
    struct directory *dp, *gddp;
    const char *densities_filename = NULL;

    if (argc == 2) {
	if (!bu_file_exists(argv[1], NULL)) {
	    bu_vls_printf(gedp->ged_result_str, "Specified density file %s not found", argv[1]);
	    return BRLCAD_ERROR;
	} else {
	    densities_filename = argv[1];
	}
    }
    gddp = db_lookup(gedp->dbip, GED_DB_DENSITY_OBJECT, LOOKUP_QUIET);

    if (densities_filename && gddp != RT_DIR_NULL) {
	// This is OK, but let the user know if we are doing a file based
	// run but there is a density database present.
	bu_vls_printf(gedp->ged_result_str, "\nNote: using specified density file %s for audit, but database contains imported density information.\nThe imported data will be the default for most tools unless a density file is explicitly specified.\n\n", densities_filename);
    }

    // As a first cut, do a fault intolerant read - if there are any
    // problems we want to know
    if (_ged_read_densities(&a, &dsource, gedp, densities_filename, 0) & BRLCAD_ERROR) {
	a = NULL;
    }

    if (!a) {
	// If that didn't work, do a fault tolerant read so we can proceed
	// with the evaluation
	if (_ged_read_densities(&a, &dsource, gedp, densities_filename, 1) == BRLCAD_ERROR) {
	    bu_vls_printf(gedp->ged_result_str, "Failed to locate density data.\n");
	    a = NULL;
	}
    }

    if (dsource) {
	bu_vls_printf(gedp->ged_result_str, " *** Using density data from %s ***\n", dsource);
	bu_free(dsource, "free dsource");
    }

    // Now, find out if anything in the database is interested in
    // material information
    struct bu_ptbl mn_objs = BU_PTBL_INIT_ZERO;
    struct bu_ptbl id_objs = BU_PTBL_INIT_ZERO;
    std::set<struct directory *> mns, ids;
    std::set<struct directory *>::iterator dp_it;
    db_update_nref(gedp->dbip);
    const char *mname_search = "-attr material_name";
    const char *mid_search = "-attr material_id";
    (void)db_search(&mn_objs, DB_SEARCH_TREE|DB_SEARCH_RETURN_UNIQ_DP, mname_search, 0, NULL, gedp->dbip, NULL, NULL, NULL);
    (void)db_search(&id_objs, DB_SEARCH_TREE|DB_SEARCH_RETURN_UNIQ_DP, mid_search, 0, NULL, gedp->dbip, NULL, NULL, NULL);
    for(size_t i = 0; i < BU_PTBL_LEN(&mn_objs); i++) {
	dp = (struct directory *)BU_PTBL_GET(&mn_objs, i);
	mns.insert(dp);
    }
    for(size_t i = 0; i < BU_PTBL_LEN(&id_objs); i++) {
	dp = (struct directory *)BU_PTBL_GET(&id_objs, i);
	ids.insert(dp);
    }
    db_search_free(&mn_objs);
    db_search_free(&id_objs);

    // If nothing in the database cares, we're done
    if (!mns.size() && !ids.size()) {
	if (gddp != RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "\n*** Density data present in database, but no material_id or material_name attributes set. ***\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\n*** No material_id or material_name attributes set in database. ***\n");
	}
	if (a) {
	    analyze_densities_destroy(a);
	}
	return BRLCAD_OK;
    }

    // Collect information about what is set in the database
    std::set<std::string> mat_names;
    std::set<long int> mat_ids;
    std::map<long int, std::set<std::string>> ids_to_mats;

    for (dp_it = ids.begin(); dp_it != ids.end(); dp_it++) {
	const char *mat_id = NULL;
	const char *oname = NULL;
	long int curr_id = -1;
	struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
	dp = *dp_it;
	if (db5_get_attributes(gedp->dbip, &avs, dp)) {
	    bu_vls_printf(gedp->ged_result_str, "Warning: cannot get attributes for object %s\n", dp->d_namep);
	    continue;
	}
	mat_id = bu_avs_get(&avs, "material_id");
	oname = bu_avs_get(&avs, "material_name");
	if (oname) {
	    mat_names.insert(std::string(oname));
	}
	if (mat_id) {
	    if (!bu_cmd_long_from_str(&curr_id, mat_id) || curr_id < 0) {
		bu_vls_printf(gedp->ged_result_str, "Object %s has an invalid value set for it's material_id attribute: %s\n", dp->d_namep, mat_id);
		bu_avs_free(&avs);
		continue;
	    }
	    mat_ids.insert(curr_id);
	}
	if (oname && mat_id) {
	    ids_to_mats[curr_id].insert(std::string(oname));
	}
	bu_avs_free(&avs);
    }

    // If the database has material_name or material_id information but
    // we don't have any density info, we are pretty limited in what we
    // can validate.
    if (!a) {
	if (mat_names.size()) {
	    std::set<std::string>::iterator n_it;
	    /* Unique Material Names: m1, m2, m3, ... \n */
	    bu_vls_printf(gedp->ged_result_str, "Unique Material Names:");
	    for (n_it = mat_names.begin(); n_it != mat_names.end(); n_it++) {
		if (n_it == mat_names.begin()) {
		    bu_vls_printf(gedp->ged_result_str, " %s", n_it->c_str());
		} else {
		    bu_vls_printf(gedp->ged_result_str, ", %s", n_it->c_str());
		}
	    }
	    bu_vls_printf(gedp->ged_result_str, "\n");
	}
	if (ids.size()) {
	    /* Unique Material Ids: id1, id2, id3, ... \n */
	    std::set<long int>::iterator l_it;
	    bu_vls_printf(gedp->ged_result_str, "Unique Material Ids:");
	    for (l_it = mat_ids.begin(); l_it != mat_ids.end(); l_it++) {
		if (l_it == mat_ids.begin()) {
		    bu_vls_printf(gedp->ged_result_str, " %ld", *l_it);
		} else {
		    bu_vls_printf(gedp->ged_result_str, ", %ld", *l_it);
		}
	    }
	    bu_vls_printf(gedp->ged_result_str, "\n");
	}

	if (ids_to_mats.size()) {
	    std::set<long int>::iterator l_it;
	    std::set<std::string>::iterator s_it;
	    // If we have id numbers and names matched up, we can at least
	    // audit to be sure the database is internally consistent -
	    // report any collisions
	    for (l_it = mat_ids.begin(); l_it != mat_ids.end(); l_it++) {
		if (ids_to_mats.find(*l_it) != ids_to_mats.end() && ids_to_mats[*l_it].size() > 1) {
		    // For each pair type, report the pair and which objects
		    // have it assigned.
		    long int active_id = *l_it;
		    bu_vls_printf(gedp->ged_result_str, "Material ID %ld has multiple associated material_name attributes:\n", active_id);
		    for (s_it = ids_to_mats[*l_it].begin(); s_it != ids_to_mats[*l_it].end(); s_it++) {
			const char *active_name = (*s_it).c_str();
			std::set<std::string> objs;
			std::set<std::string>::iterator objs_it;
			bu_vls_printf(gedp->ged_result_str, "  %s:", active_name);
			for (dp_it = ids.begin(); dp_it != ids.end(); dp_it++) {
			    const char *mat_id = NULL;
			    const char *oname = NULL;
			    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
			    dp = *dp_it;
			    if (db5_get_attributes(gedp->dbip, &avs, dp)) {
				continue;
			    }
			    mat_id = bu_avs_get(&avs, "material_id");
			    oname = bu_avs_get(&avs, "material_name");
			    if (std::stol(mat_id) == active_id && BU_STR_EQUAL(active_name, oname)) {
				objs.insert(std::string(dp->d_namep));
			    }
			    bu_avs_free(&avs);
			}
			for (objs_it = objs.begin(); objs_it != objs.end(); objs_it++) {
			    bu_vls_printf(gedp->ged_result_str, " %s", (*objs_it).c_str());
			}
			bu_vls_printf(gedp->ged_result_str, "\n");
		    }
		}
	    }
	}
	return BRLCAD_OK;
    }

    // We've got some combination of density information and material
    // attributes.  Now things get interesting.

    if (mns.size()) {
	// Check for any names that aren't present in the database and
	// report.  If there is an associated material id with object that
	// corresponds to a known name, report that as well.
	for (dp_it = mns.begin(); dp_it != mns.end(); dp_it++) {
	    const char *mat_id = NULL;
	    char *id_name = NULL;
	    long int curr_id = -1;
	    int id_found_cnt;
	    long int wids[1];
	    const char *oname = NULL;
	    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
	    dp = *dp_it;
	    if (db5_get_attributes(gedp->dbip, &avs, dp)) {
		continue;
	    }
	    oname = bu_avs_get(&avs, "material_name");
	    id_found_cnt = analyze_densities_id((long int *)wids, 1, a, oname);
	    if (!id_found_cnt) {
		bu_vls_printf(gedp->ged_result_str, "%s[material_name:%s] unknown material", (*dp_it)->d_namep, oname);

		mat_id = bu_avs_get(&avs, "material_id");
		if (mat_id) {
		    if (!bu_cmd_long_from_str(&curr_id, mat_id) || curr_id < 0) {
			bu_avs_free(&avs);
			continue;
		    }
		    id_name = analyze_densities_name(a, curr_id);
		    if (id_name) {
			bu_vls_printf(gedp->ged_result_str, " (material_id:%ld -> %s)\n", curr_id, id_name);
			bu_free(id_name, "id name");
		    } else {
			bu_vls_printf(gedp->ged_result_str, "\n");
		    }
		} else {
		    bu_vls_printf(gedp->ged_result_str, "\n");
		}

	    }

	}
    }

    if (ids.size()) {
	// Check for any material ids that aren't present in the database
	// and report.  If there is an associated material name that
	// corresponds to a known id, report that as well.
	for (dp_it = ids.begin(); dp_it != ids.end(); dp_it++) {
	    const char *mat_id = NULL;
	    char *id_name = NULL;
	    long int curr_id = -1;
	    int id_found_cnt;
	    long int wids[1];
	    const char *oname = NULL;
	    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
	    dp = *dp_it;
	    if (db5_get_attributes(gedp->dbip, &avs, dp)) {
		continue;
	    }

	    mat_id = bu_avs_get(&avs, "material_id");
	    if (!mat_id || !bu_cmd_long_from_str(&curr_id, mat_id) || curr_id < 0) {
		bu_avs_free(&avs);
		continue;
	    }
	    id_name = analyze_densities_name(a, curr_id);
	    if (!id_name) {
		bu_vls_printf(gedp->ged_result_str, "%s[material_id:%ld] unknown material_id", (*dp_it)->d_namep, curr_id);
		oname = bu_avs_get(&avs, "material_name");
		if (oname) {
		    id_found_cnt = analyze_densities_id((long int *)wids, 1, a, oname);
		    if (id_found_cnt) {
			bu_vls_printf(gedp->ged_result_str, " (material_name:%s -> %ld)\n", oname, wids[0]);
		    } else {
			bu_vls_printf(gedp->ged_result_str, "\n");
		    }
		} else {
		    bu_vls_printf(gedp->ged_result_str, "\n");
		}
	    }
	}
    }

    analyze_densities_destroy(a);
    return BRLCAD_OK;
}


static int
mater_import(struct ged *gedp, size_t argc, const char *argv[])
{
    int validate_input = 0;
    if (argc < 2 || argc > 3) {
	bu_vls_printf(gedp->ged_result_str, "%s", mater_usage);
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[1], "-v")) {
	argv[1] = argv[0];
	argc--; argv++;
	validate_input = 1;
    }

    if (!bu_file_exists(argv[1], NULL)) {
	bu_vls_printf(gedp->ged_result_str, "Specified density file %s not found", argv[1]);
	return BRLCAD_ERROR;
    }

    if (mater_clear(gedp) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }

    if (validate_input) {
	if (mater_validate(gedp, 2, argv) != BRLCAD_OK) {
	    return BRLCAD_ERROR;
	}
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (rt_mk_binunif (wdbp, GED_DB_DENSITY_OBJECT, argv[1], DB5_MINORTYPE_BINU_8BITINT, 0)) {
	bu_vls_printf(gedp->ged_result_str, "Error reading density file %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* Mark it hidden */
    {
	const char *av[2] = {"hide", GED_DB_DENSITY_OBJECT};
	(void)ged_exec_hide(gedp, 2, (const char **)av);
    }

    return BRLCAD_OK;
}


static int
mater_export(struct ged *gedp, size_t argc, const char *argv[])
{
    FILE *fp;
    struct rt_binunif_internal *bip;
    struct rt_db_internal intern;
    struct directory *dp;

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "%s", mater_usage);
	return BRLCAD_OK;
    }

    if ((dp = db_lookup(gedp->dbip, GED_DB_DENSITY_OBJECT, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "no density information found in database\n");
	return BRLCAD_ERROR;
    }

    if (bu_file_exists(argv[1], NULL)) {
	bu_vls_printf(gedp->ged_result_str, "specified output density file %s already exists", argv[1]);
	return BRLCAD_ERROR;
    }

    fp = fopen(argv[1], "w+b");

    if (fp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "cannot open file %s for writing", argv[1]);
	return BRLCAD_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, gedp->dbip, NULL) < 0) {
	bu_vls_printf(gedp->ged_result_str, "error reading %s from database", dp->d_namep);
	fclose(fp);
	return BRLCAD_ERROR;
    }

    RT_CK_DB_INTERNAL(&intern);

    bip = (struct rt_binunif_internal *)intern.idb_ptr;
    if (bip->count < 1) {
	bu_vls_printf(gedp->ged_result_str, "%s has no contents", GED_DB_DENSITY_OBJECT);
	fclose(fp);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    if (fwrite(bip->u.int8, bip->count * db5_type_sizeof_h_binu(bip->type), 1, fp) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Error writing contents to file");
	fclose(fp);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    fclose(fp);
    rt_db_free_internal(&intern);
    return BRLCAD_OK;
}


enum mater_filter_operator {
    MATER_FILTER_EQUAL = 0,
    MATER_FILTER_LESS,
    MATER_FILTER_LESS_EQUAL,
    MATER_FILTER_GREATER,
    MATER_FILTER_GREATER_EQUAL
};

struct mater_id_filter {
    mater_filter_operator op;
    long int value;
    std::string pattern;
    int is_pattern;
};

struct mater_density_filter {
    mater_filter_operator op;
    fastf_t value;
};

struct mater_get_args {
    std::vector<mater_id_filter> id_filters;
    std::vector<mater_density_filter> density_filters;
    std::vector<std::string> name_patterns;
    fastf_t tolerance = BN_TOL_DIST;
};


static const char *
mater_filter_operator_parse(const char *arg, mater_filter_operator *op)
{
    if (!arg || !op)
	return NULL;

    *op = MATER_FILTER_EQUAL;
    if (arg[0] == '<') {
	*op = arg[1] == '=' ? MATER_FILTER_LESS_EQUAL : MATER_FILTER_LESS;
	return arg + (arg[1] == '=' ? 2 : 1);
    }
    if (arg[0] == '>') {
	*op = arg[1] == '=' ? MATER_FILTER_GREATER_EQUAL : MATER_FILTER_GREATER;
	return arg + (arg[1] == '=' ? 2 : 1);
    }
    if (arg[0] == '=')
	return arg + 1;
    return arg;
}


static int
mater_get_id_from_str(struct bu_vls *msg, const char *arg, void *storage)
{
    mater_filter_operator op;
    const char *value = mater_filter_operator_parse(arg, &op);
    long int id = 0;

    if (!arg || !arg[0] || !value || !value[0]) {
	if (msg)
	    bu_vls_printf(msg, "material id filter requires an id or pattern\n");
	return -1;
    }

    if (!bu_cmd_long_from_str(&id, value)) {
	if (storage) {
	    mater_id_filter filter = {MATER_FILTER_EQUAL, 0, arg, 1};
	    static_cast<std::vector<mater_id_filter> *>(storage)->push_back(filter);
	}
	return 0;
    }

    if (storage) {
	mater_id_filter filter = {op, id, std::string(), 0};
	static_cast<std::vector<mater_id_filter> *>(storage)->push_back(filter);
    }
    return 0;
}


static int
mater_get_density_from_str(struct bu_vls *msg, const char *arg, void *storage)
{
    mater_filter_operator op;
    const char *value = mater_filter_operator_parse(arg, &op);
    fastf_t density = 0.0;

    if (!arg || !arg[0] || !value || !value[0] ||
	!bu_cmd_number_from_str(&density, value)) {
	if (msg)
	    bu_vls_printf(msg, "invalid density filter: %s\n", arg ? arg : "");
	return -1;
    }

    if (storage) {
	mater_density_filter filter = {op, density};
	static_cast<std::vector<mater_density_filter> *>(storage)->push_back(filter);
    }
    return 0;
}


static int
mater_get_name_from_str(struct bu_vls *msg, const char *arg, void *storage)
{
    if (!arg || !arg[0]) {
	if (msg)
	    bu_vls_printf(msg, "material name filter requires a pattern\n");
	return -1;
    }
    if (storage)
	static_cast<std::vector<std::string> *>(storage)->push_back(std::string(arg));
    return 0;
}


static int
mater_filter_long_match(long int candidate, const mater_id_filter &filter)
{
    if (filter.is_pattern)
	return 0;
    switch (filter.op) {
	case MATER_FILTER_LESS: return candidate < filter.value;
	case MATER_FILTER_LESS_EQUAL: return candidate <= filter.value;
	case MATER_FILTER_GREATER: return candidate > filter.value;
	case MATER_FILTER_GREATER_EQUAL: return candidate >= filter.value;
	case MATER_FILTER_EQUAL: return candidate == filter.value;
	}
    return 0;
}


static int
mater_filter_density_match(fastf_t candidate, const mater_density_filter &filter,
	fastf_t tolerance)
{
    switch (filter.op) {
	case MATER_FILTER_LESS: return candidate < filter.value;
	case MATER_FILTER_LESS_EQUAL: return candidate <= filter.value;
	case MATER_FILTER_GREATER: return candidate > filter.value;
	case MATER_FILTER_GREATER_EQUAL: return candidate >= filter.value;
	case MATER_FILTER_EQUAL: return NEAR_EQUAL(candidate, filter.value, tolerance);
	}
    return 0;
}


static const struct bu_cmd_option mater_get_options[] = {
    BU_CMD_CUSTOM(NULL, "id", struct mater_get_args, id_filters, mater_get_id_from_str,
	"id-or-pattern", "Match a material id, range, or glob pattern"),
    BU_CMD_CUSTOM(NULL, "density", struct mater_get_args, density_filters,
	mater_get_density_from_str, "comparison", "Match a density value or range"),
    BU_CMD_CUSTOM(NULL, "name", struct mater_get_args, name_patterns,
	mater_get_name_from_str, "pattern", "Match a material name glob pattern"),
    BU_CMD_NUMBER(NULL, "tol", struct mater_get_args, tolerance, "tolerance",
	"Density equality tolerance (default is BN_TOL_DIST)"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand mater_get_operands[] = {
    BU_CMD_OPERAND("pattern", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Material id or name glob pattern", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema mater_get_schema = {
    "get", "Query density records", mater_get_options, mater_get_operands,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


static int
mater_get(struct ged *gedp, size_t argc, const char *argv[])
{
    struct bu_vls msgs = BU_VLS_INIT_ZERO;
    struct directory *dp;
    struct rt_binunif_internal *bip;
    struct rt_db_internal intern;
    struct analyze_densities *a = NULL;
    char *buf = NULL;
    mater_get_args args;
    std::vector<std::string> greedy_patterns;
    int operand_start;
    int ret = 0;
    int ecnt = 0;
    int result = BRLCAD_ERROR;
    long int curr_id = -1;

    argc--; argv++;
    if (argc < 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", mater_usage);
	goto done;
    }

    operand_start = bu_cmd_schema_parse(&mater_get_schema, &args,
	gedp->ged_result_str, (int)argc, argv);
    if (operand_start < 0)
	goto done;
    for (size_t i = (size_t)operand_start; i < argc; i++)
	greedy_patterns.push_back(argv[i]);

    if ((dp = db_lookup(gedp->dbip, GED_DB_DENSITY_OBJECT, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "no density information found in database.  To insert density information, use the mater -d set and/or mater -d import commands.\n");
	goto done;
    }

    if (rt_db_get_internal(&intern, dp, gedp->dbip, NULL) < 0) {
	bu_vls_printf(gedp->ged_result_str, "error reading %s from database", dp->d_namep);
	goto done;
    }

    RT_CK_DB_INTERNAL(&intern);
    bip = (struct rt_binunif_internal *)intern.idb_ptr;
    if (bip->count < 1) {
	bu_vls_printf(gedp->ged_result_str, "%s has no contents", GED_DB_DENSITY_OBJECT);
	rt_db_free_internal(&intern);
	goto done;
    }

    buf = (char *)bu_calloc(bip->count + 1, sizeof(char), "density buffer");
    memcpy(buf, bip->u.int8, bip->count);
    rt_db_free_internal(&intern);
    analyze_densities_create(&a);
    ret = analyze_densities_load(a, buf, &msgs, &ecnt);
    bu_free(buf, "free density buffer");
    if (!ret) {
	bu_vls_printf(gedp->ged_result_str, "no density information found when reading database:\n%s\nTo insert density information, use the mater -d set and/or mater -d import commands.\n", bu_vls_cstr(&msgs));
	goto done;
    }
    if (ecnt) {
	bu_vls_printf(gedp->ged_result_str, "errors found when reading database:\n%s\n", bu_vls_cstr(&msgs));
	goto done;
    }

    while ((curr_id = analyze_densities_next(a, curr_id)) != -1) {
	int have_id_match = args.id_filters.empty() ? -1 : 0;
	int have_density_match = args.density_filters.empty() ? -1 : 0;
	int have_name_match = args.name_patterns.empty() ? -1 : 0;
	int have_greedy_match = 0;
	fastf_t curr_density = analyze_densities_density(a, curr_id);
	char *curr_name = analyze_densities_name(a, curr_id);
	struct bu_vls curr_id_str = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&curr_id_str, "%ld", curr_id);

	for (const std::string &pattern : greedy_patterns) {
	    if (!bu_path_match(pattern.c_str(), bu_vls_cstr(&curr_id_str), 0) ||
		!bu_path_match(pattern.c_str(), curr_name, 0)) {
		have_greedy_match = 1;
		break;
	    }
	}
	for (const mater_id_filter &filter : args.id_filters) {
	    if ((filter.is_pattern && !bu_path_match(filter.pattern.c_str(),
		bu_vls_cstr(&curr_id_str), 0)) ||
		(!filter.is_pattern && mater_filter_long_match(curr_id, filter))) {
		have_id_match = 1;
		break;
	    }
	}
	for (const mater_density_filter &filter : args.density_filters) {
	    if (mater_filter_density_match(curr_density, filter, args.tolerance)) {
		have_density_match = 1;
		break;
	    }
	}
	for (const std::string &pattern : args.name_patterns) {
	    if (!bu_path_match(pattern.c_str(), curr_name, 0)) {
		have_name_match = 1;
		break;
	    }
	}

	if (((have_id_match && have_density_match && have_name_match) &&
		(have_id_match > 0 || have_density_match > 0 || have_name_match > 0)) ||
	    (have_greedy_match && (have_id_match != 0 && have_density_match != 0 &&
		 have_name_match != 0))) {
	    bu_vls_printf(gedp->ged_result_str, "%ld\t%g\t%s\n", curr_id,
		curr_density, curr_name);
	}
	bu_free(curr_name, "name copy");
	bu_vls_free(&curr_id_str);
    }
    result = BRLCAD_OK;

done:
    if (a)
	analyze_densities_destroy(a);
    bu_vls_free(&msgs);
    return result;
}


static int
mater_set(struct ged *gedp, size_t argc, const char *argv[])
{
    struct directory *dp;
    struct analyze_densities *a = NULL;

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "%s", mater_usage);
	return BRLCAD_ERROR;
    }

    dp = db_lookup(gedp->dbip, GED_DB_DENSITY_OBJECT, LOOKUP_QUIET);
    if (dp != RT_DIR_NULL) {
	if (_ged_read_densities(&a, NULL, gedp, NULL, 0) != BRLCAD_OK) {
	    return BRLCAD_ERROR;
	}
    } else {
	/* Starting from scratch */
	analyze_densities_create(&a);
    }

    /* Parse argv density arguments */
    std::regex d_reg("([0-9]+)[\\s, ]+([-+]?[0-9]*[\\.]?[0-9eE+-]*)[\\s, ]+(.*)");
    for (size_t i = 1; i < argc; i++) {
	long int id = 0;
	fastf_t density;
	std::smatch sm;
	std::string dstr(argv[i]);
	if (std::regex_search(dstr, sm, d_reg)) {
	    if (sm.size() == 4) {
		int parse_error = 0;
		std::string id_str = sm[1].str();
		if (!bu_cmd_long_from_str(&id, id_str.c_str())) {
		    bu_vls_printf(gedp->ged_result_str, "Error parsing material id: %s\n", id_str.c_str());
		    parse_error = 1;
		}
		std::string den_str = sm[2].str();
		if (!bu_cmd_number_from_str(&density, den_str.c_str())) {
		    bu_vls_printf(gedp->ged_result_str, "Error parsing density: %s\n", den_str.c_str());
		    parse_error = 1;
		}
		std::string name_str = sm[3].str();
		if (!parse_error) {
		    analyze_densities_set(a, id, density, name_str.c_str(), NULL);
		} else {
		    bu_vls_printf(gedp->ged_result_str, "Error parsing density specifier: %s\n", argv[i]);
		    analyze_densities_destroy(a);
		    return BRLCAD_ERROR;
		}
	    } else {
		bu_vls_printf(gedp->ged_result_str, "Error parsing density specifier: %s\n", argv[i]);
		analyze_densities_destroy(a);
		return BRLCAD_ERROR;
	    }
	} else {
	    bu_vls_printf(gedp->ged_result_str, "Error parsing density specifier: %s\n", argv[i]);
	    analyze_densities_destroy(a);
	    return BRLCAD_ERROR;
	}
    }

    char *new_buf = NULL;
    long int buf_len = analyze_densities_write(&new_buf, a);
    analyze_densities_destroy(a);

    if (buf_len <= 0 || !new_buf) {
	bu_vls_printf(gedp->ged_result_str, "Error generating density buffer\n");
	return BRLCAD_ERROR;
    }

    // Got through parsing, make a buffer and replace the existing density object
    if (mater_clear(gedp) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }

    struct rt_binunif_internal *bip = NULL;
    BU_ALLOC(bip, struct rt_binunif_internal);
    bip->magic = RT_BINUNIF_INTERNAL_MAGIC;
    bip->type = DB5_MINORTYPE_BINU_8BITINT;
    bip->count = buf_len;
    bip->u.int8 = (char *)bu_malloc(buf_len, "binary uniform object");
    memcpy(bip->u.int8, new_buf, buf_len);

    /* create the rt_internal form */
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BINARY_UNIF;
    intern.idb_minor_type = DB5_MINORTYPE_BINU_8BITINT;
    intern.idb_ptr = (void *)bip;
    intern.idb_meth = &OBJ[ID_BINUNIF];

    /* create body portion of external form */
    struct bu_external body;
    struct bu_external bin_ext;
    int ret = -1;
    if (intern.idb_meth->ft_export5) {
	ret = intern.idb_meth->ft_export5(&body, &intern, 1.0, gedp->dbip);
    }
    if (ret != 0) {
	bu_vls_printf(gedp->ged_result_str, "Error while attempting to export %s\n", GED_DB_DENSITY_OBJECT);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    /* create entire external form */
    db5_export_object3(&bin_ext, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
		       GED_DB_DENSITY_OBJECT, 0, NULL, &body,
		       intern.idb_major_type, intern.idb_minor_type,
		       DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED);

    rt_db_free_internal(&intern);
    bu_free_external(&body);

    /* make sure the database directory is initialized */
    if (gedp->dbip->i->dbi_eof == RT_DIR_PHONY_ADDR) {
	if (db_dirbuild(gedp->dbip)) {
	    return BRLCAD_ERROR;
	}
    }

    /* add this (phony until written) object to the directory */
    dp = db_diradd5(gedp->dbip, GED_DB_DENSITY_OBJECT, RT_DIR_PHONY_ADDR, intern.idb_major_type, intern.idb_minor_type, 0, 0, NULL);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Error while attempting to add new name (%s) to the database", GED_DB_DENSITY_OBJECT);
	bu_free_external(&bin_ext);
	return BRLCAD_ERROR;
    }

    /* and write it to the database */
    if (db_put_external5(&bin_ext, dp, gedp->dbip)) {
	bu_vls_printf(gedp->ged_result_str, "Error while adding new binary object (%s) to the database", GED_DB_DENSITY_OBJECT);
	bu_free_external(&bin_ext);
	return BRLCAD_ERROR;
    }
    bu_free_external(&bin_ext);

    /* Mark it hidden */
    {
	const char *av[2] = {"hide", GED_DB_DENSITY_OBJECT};
	(void)ged_exec_hide(gedp, 2, (const char **)av);
    }

    return BRLCAD_OK;
}


static int
_ged_read_msmap(struct ged *gedp, const char *mbuff, std::set<std::string> &defined_materials, std::map<std::string, std::string> &listed_to_defined)
{
    int ret = BRLCAD_OK;
    std::string mb(mbuff);
    std::istringstream ss(mb);
    std::string line;
    std::string ws("\t ");
    std::string ws2("\t \"");
    std::string tc("\"");
    std::string sep("\t , ;:\"=");
    while (std::getline(ss, line)) {
	size_t curr_char = 0;
	std::string m1, m2;
	while (line[curr_char] && (ws.find(line[curr_char]) != std::string::npos)) curr_char++;
	if (line[curr_char] == '"') {
	    curr_char++;
	    // In first name and quoted, go until matching quote found
	    while (line[curr_char] && line[curr_char] != '"') {
		m1.push_back(line[curr_char]);
		curr_char++;
	    }
	} else {
	    // In first name and not quoted, go until we find a non-sep character
	    while (line[curr_char] && (sep.find(line[curr_char]) == std::string::npos)) {
		m1.push_back(line[curr_char]);
		curr_char++;
	    }
	}
	// Eat the spacer chars
	while (line[curr_char] && (sep.find(line[curr_char]) != std::string::npos)) curr_char++;
	// In second name
	if (curr_char > 0 && line[curr_char-1] == '"') {
	    while (line[curr_char] && (tc.find(line[curr_char]) == std::string::npos)) {
		m2.push_back(line[curr_char]);
		curr_char++;
	    }
	} else {
	    while (line[curr_char] && (ws2.find(line[curr_char]) == std::string::npos)) {
		m2.push_back(line[curr_char]);
		curr_char++;
	    }
	}
	if (!m1.length() && !m2.length()) {
	    continue;
	}
	if (!m1.length() || !m2.length()) {
	    bu_vls_printf(gedp->ged_result_str, "%s -> %s: invalid specifier in mapping file", m1.c_str(), m2.c_str());
	    ret = BRLCAD_ERROR;
	} else {
	    // Have names, process
	    if (listed_to_defined.find(m1) != listed_to_defined.end()) {
		bu_vls_printf(gedp->ged_result_str, "%s is multiply defined in the mapping file", m1.c_str());
		ret = BRLCAD_ERROR;
	    }
	    if (defined_materials.find(m2) == defined_materials.end()) {
		bu_vls_printf(gedp->ged_result_str, "%s is not known in the density file", m2.c_str());
		ret = BRLCAD_ERROR;
	    }
	    listed_to_defined.insert(std::pair<std::string, std::string>(m1, m2));
	}
    }
    return ret;
}


static int
mater_try_densities_load(struct ged *gedp, struct analyze_densities **pa, std::set<std::string> &defined_materials,
			 std::map<std::string, std::string> &listed_to_defined, const char *fname)
{
    long int curr_id = -1;
    int dcnt = 0;
    int ecnt = 0;
    struct analyze_densities *densities;
    struct bu_vls pbuff_msgs = BU_VLS_INIT_ZERO;
    struct bu_mapped_file *ifile = NULL;
    char *ibuff = NULL;

    if (!bu_file_exists(fname, NULL)) {
	bu_vls_printf(gedp->ged_result_str, "file %s not found", fname);
	return BRLCAD_ERROR;
    }

    ifile = bu_open_mapped_file(fname, "densities file");
    if (!ifile) {
	bu_vls_printf(gedp->ged_result_str, "could not open file %s", fname);
	return BRLCAD_ERROR;
    }
    ibuff = (char *)(ifile->buf);

    analyze_densities_create(&densities);
    dcnt = analyze_densities_load(densities, ibuff, &pbuff_msgs, &ecnt);
    if (!dcnt || ecnt) {
	analyze_densities_destroy(densities);
	bu_close_mapped_file(ifile);
	return 0;
    }
    bu_close_mapped_file(ifile);
    (*pa) = densities;
    while ((curr_id = analyze_densities_next((*pa), curr_id)) != -1) {
	char *cname = analyze_densities_name((*pa), curr_id);
	listed_to_defined.insert(std::pair<std::string, std::string>(std::string(cname), std::string(cname)));
	defined_materials.insert(std::string(cname));
	bu_free(cname, "free name copy");
    }
    return 1;
}


struct mater_map_args {
    int ids_from_names;
    int names_from_ids;
    struct bu_vls density_file;
    struct bu_vls map_file;
};


static int
mater_vls_replace_from_str(struct bu_vls *msg, const char *arg, void *storage)
{
    if (!arg || !arg[0]) {
	if (msg)
	    bu_vls_strcat(msg, "file name is required\n");
	return -1;
    }
    if (storage)
	bu_vls_strcpy((struct bu_vls *)storage, arg);
    return 0;
}


static const char * const mater_map_mode_options[] = {
    "ids-from-names", "names-from-ids", NULL
};

static const struct bu_cmd_constraint mater_map_constraints[] = {
    BU_CMD_CONSTRAINT_OPTIONS(mater_map_mode_options, 1, 1,
	"exactly one mapping mode is required"),
    BU_CMD_CONSTRAINT_NULL
};

static const struct bu_cmd_option mater_map_options[] = {
    BU_CMD_FLAG(NULL, "ids-from-names", struct mater_map_args, ids_from_names,
	"Assign material_id values from material_name attributes"),
    BU_CMD_FLAG(NULL, "names-from-ids", struct mater_map_args, names_from_ids,
	"Assign material_name values from material_id attributes"),
    BU_CMD_CUSTOM("d", "density-file", struct mater_map_args, density_file,
	mater_vls_replace_from_str, "file", "Use this density data file"),
    BU_CMD_CUSTOM("m", "map-file", struct mater_map_args, map_file,
	mater_vls_replace_from_str, "file", "Use this material-name mapping file"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand mater_map_operands[] = {
    BU_CMD_OPERAND("file", BU_CMD_VALUE_FILE, 0, 2,
	"Optional density and mapping files", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema mater_map_schema = {
    "map", "Synchronize material_id and material_name attributes", mater_map_options,
    mater_map_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, mater_map_constraints)
};


static int
mater_mat_id(struct ged *gedp, size_t argc, const char *argv[])
{
    struct mater_map_args args = {0, 0, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO};
    int ac;
    struct directory *dp;
    struct analyze_densities *a = NULL;
    std::set<std::string> listed_materials;
    std::set<std::string> defined_materials;
    std::map<std::string, std::string> listed_to_defined;
    const char *mname_search = "-attr material_name";
    const char *mid_search = "-attr material_id";
    struct bu_ptbl mn_objs = BU_PTBL_INIT_ZERO;
    struct bu_ptbl id_objs = BU_PTBL_INIT_ZERO;
    std::set<struct directory *> mns, ids;
    struct bu_mapped_file *mfile = NULL;
    char *mbuff = NULL;
    long int curr_id = -1;
    struct bu_attribute_value_set *avs;
    const char *mat_id;
    const char *oname;
    std::set<struct directory *> ids_wo_names;
    std::set<struct directory *>::iterator dp_it;
    long int wids[1];
    int id_found_cnt;
    int nid;

    argc--; argv++;

    ac = bu_cmd_schema_parse(&mater_map_schema, &args, gedp->ged_result_str,
	(int)argc, argv);
    if (ac < 0) {
	goto ged_mater_mat_id_fail;
    }

    argc -= (size_t)ac;
    argv += ac;
    ac = (int)argc;

	if ((args.names_from_ids + args.ids_from_names) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", mater_usage);
	goto ged_mater_mat_id_fail;
    }

	if (ac > 2) {
	bu_vls_printf(gedp->ged_result_str, "%s", mater_usage);
	goto ged_mater_mat_id_fail;
    }

	if (ac == 2 && (bu_vls_strlen(&args.density_file) || bu_vls_strlen(&args.map_file))) {
	bu_vls_printf(gedp->ged_result_str, "%s", mater_usage);
	goto ged_mater_mat_id_fail;
    }

	if (ac == 1 && bu_vls_strlen(&args.density_file) && args.names_from_ids) {
	bu_vls_printf(gedp->ged_result_str, "%s", mater_usage);
	goto ged_mater_mat_id_fail;
    }

	if (bu_vls_strlen(&args.density_file) && !bu_file_exists(bu_vls_cstr(&args.density_file), NULL)) {
	bu_vls_printf(gedp->ged_result_str, "density file %s doesn't exist", bu_vls_cstr(&args.density_file));
	goto ged_mater_mat_id_fail;
    }

	if (bu_vls_strlen(&args.map_file) && !bu_file_exists(bu_vls_cstr(&args.map_file), NULL)) {
	bu_vls_printf(gedp->ged_result_str, "mapping file %s doesn't exist", bu_vls_cstr(&args.map_file));
	goto ged_mater_mat_id_fail;
    }

    if (ac) {
	if (!bu_file_exists(argv[0], NULL)) {
	    bu_vls_printf(gedp->ged_result_str, "file %s doesn't exist", argv[0]);
	    goto ged_mater_mat_id_fail;
	}
	if (ac > 1) {
	    if (!bu_file_exists(argv[1], NULL)) {
		bu_vls_printf(gedp->ged_result_str, "file %s doesn't exist", argv[1]);
		goto ged_mater_mat_id_fail;
	    }
	}
    }

	if (ac && !bu_vls_strlen(&args.density_file)) {
	if (!mater_try_densities_load(gedp, &a, defined_materials, listed_to_defined, argv[0])) {
	    if (ac == 2 || args.names_from_ids) {
		bu_vls_printf(gedp->ged_result_str, "density file %s did not load successfully", argv[0]);
		goto ged_mater_mat_id_fail;
	    } else {
	    // May be a mapping file, but we can't try loading it until we
	    // have the density information
	}
    } else {
	bu_vls_sprintf(&args.density_file, "%s", argv[0]);
	ac--; argv++;
    }
    }

	if (bu_vls_strlen(&args.density_file) && !a) {
	if (!mater_try_densities_load(gedp, &a, defined_materials, listed_to_defined, bu_vls_cstr(&args.density_file))) {
	    bu_vls_printf(gedp->ged_result_str, "density file %s did not load successfully", bu_vls_cstr(&args.density_file));
	    goto ged_mater_mat_id_fail;

	}
    }

	if (!bu_vls_strlen(&args.density_file)) {
	// If we don't have a density file, we can't proceed unless the
	// database has density information.
	dp = db_lookup(gedp->dbip, GED_DB_DENSITY_OBJECT, LOOKUP_QUIET);
	if (dp != RT_DIR_NULL) {
	    if (_ged_read_densities(&a, NULL, gedp, NULL, 0) != BRLCAD_OK) {
		bu_vls_printf(gedp->ged_result_str, "No density information found and no density file specified, cannot proceed.");
		goto ged_mater_mat_id_fail;
	    }

	    curr_id = -1;
	    while ((curr_id = analyze_densities_next(a, curr_id)) != -1) {
		char *cname = analyze_densities_name(a, curr_id);
		listed_to_defined.insert(std::pair<std::string, std::string>(std::string(cname), std::string(cname)));
		defined_materials.insert(std::string(cname));
		bu_free(cname, "free name copy");
	    }
	} else {
	    bu_vls_printf(gedp->ged_result_str, "No density information found and no density file specified, cannot proceed.");
	    goto ged_mater_mat_id_fail;
	}
    }

	if (ac && bu_vls_strlen(&args.map_file)) {
	bu_vls_printf(gedp->ged_result_str, "more than one mapping file supplied.");
	goto ged_mater_mat_id_fail;
    }
    if (ac) {
	bu_vls_sprintf(&args.map_file, "%s", argv[0]);
    }

	if (bu_vls_strlen(&args.map_file)) {
	mfile = bu_open_mapped_file(bu_vls_cstr(&args.map_file), "mapping file");
	if (!mfile) {
	    bu_vls_printf(gedp->ged_result_str, "could not open mapping file %s", bu_vls_cstr(&args.map_file));
	    goto ged_mater_mat_id_fail;
	}

	mbuff = (char *)(mfile->buf);

	if (_ged_read_msmap(gedp, mbuff, defined_materials, listed_to_defined) !=  BRLCAD_OK) {
	    bu_vls_printf(gedp->ged_result_str, "file %s is not a valid mapping file, aborting", bu_vls_cstr(&args.map_file));
	    bu_close_mapped_file(mfile);
	    goto ged_mater_mat_id_fail;
	} else {
	    bu_close_mapped_file(mfile);
	}
    }

    // Find the objects we need to work with (if any)
    db_update_nref(gedp->dbip);
    (void)db_search(&mn_objs, DB_SEARCH_TREE|DB_SEARCH_RETURN_UNIQ_DP, mname_search, 0, NULL, gedp->dbip, NULL, NULL, NULL);
    (void)db_search(&id_objs, DB_SEARCH_TREE|DB_SEARCH_RETURN_UNIQ_DP, mid_search, 0, NULL, gedp->dbip, NULL, NULL, NULL);
    for(size_t i = 0; i < BU_PTBL_LEN(&mn_objs); i++) {
	dp = (struct directory *)BU_PTBL_GET(&mn_objs, i);
	mns.insert(dp);
    }
    for(size_t i = 0; i < BU_PTBL_LEN(&id_objs); i++) {
	dp = (struct directory *)BU_PTBL_GET(&id_objs, i);
	ids.insert(dp);
    }
    db_search_free(&mn_objs);
    db_search_free(&id_objs);


    if (args.names_from_ids) {
	for (dp_it = ids.begin(); dp_it != ids.end(); dp_it++) {
	    BU_GET(avs, struct bu_attribute_value_set);
	    dp = *dp_it;
	    bu_avs_init_empty(avs);
	    if (db5_get_attributes(gedp->dbip, avs, dp)) {
		bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
		goto ged_mater_mat_id_fail;
	    }
	    mat_id = bu_avs_get(avs, "material_id");
	    oname = bu_avs_get(avs, "material_name");
	    if (analyze_densities_density(a, std::stol(mat_id)) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Warning: no name found in density file for material_id %s on object %s, skipping\n", mat_id, dp->d_namep);
		if (oname) {
		    long int found_id_cnt = analyze_densities_id(NULL, 0, a, oname);
		    if (found_id_cnt) {
			bu_vls_printf(gedp->ged_result_str, "Material id collision: object %s has material_name \"%s\" and material_id %s, but the specified density file defines \"%s\" as a different material id.\n", dp->d_namep, oname, mat_id, oname);
		    } else {
			bu_vls_printf(gedp->ged_result_str, "Unknown material: object %s has material_name %s, which is not a material defined in the specified density file.\n", dp->d_namep, oname);
		    }
		}
		bu_avs_free(avs);
		BU_PUT(avs, struct bu_attribute_value_set);
		continue;
	    }
	    // Found a name, assign it if it doesn't match
	    char *nname = analyze_densities_name(a, std::stol(mat_id));
	    if (!oname || !BU_STR_EQUAL(nname, oname)) {
		(void)bu_avs_add(avs, "material_name", nname);
		if (db5_update_attributes(dp, avs, gedp->dbip)) {
		    bu_vls_printf(gedp->ged_result_str, "Error: failed to update object %s attributes\n", dp->d_namep);
		    bu_avs_free(avs);
		    BU_PUT(avs, struct bu_attribute_value_set);
		    goto ged_mater_mat_id_fail;
		}
	    } else {
		// Already has correct name, no need to update
		bu_avs_free(avs);
		BU_PUT(avs, struct bu_attribute_value_set);
	    }
	    bu_free(nname, "free name");
	}

	bu_vls_free(&args.density_file);
	bu_vls_free(&args.map_file);
	analyze_densities_destroy(a);
	return BRLCAD_OK;
    } else {
	for (dp_it = ids.begin(); dp_it != ids.end(); dp_it++) {
	    dp = *dp_it;
	    if (mns.find(dp) == mns.end()) {
		ids_wo_names.insert(dp);
	    }
	}
	if (ids_wo_names.size()) {
	    bu_vls_printf(gedp->ged_result_str, "WARNING: the following object(s) have a material_id attribute but no material_name attribute (use the \"--names_from_ids\" option to assign names using a density file):\n");
	    for (dp_it = ids_wo_names.begin(); dp_it != ids_wo_names.end(); dp_it++) {
		bu_vls_printf(gedp->ged_result_str, "%s\n", (*dp_it)->d_namep);
	    }
	}
    }

    for (dp_it = mns.begin(); dp_it != mns.end(); dp_it++) {
	dp = *dp_it;
	BU_GET(avs, struct bu_attribute_value_set);
	bu_avs_init_empty(avs);
	if (db5_get_attributes(gedp->dbip, avs, dp)) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
	    analyze_densities_destroy(a);
	    bu_avs_free(avs);
	    BU_PUT(avs, struct bu_attribute_value_set);
	    goto ged_mater_mat_id_fail;
	}
	mat_id = bu_avs_get(avs, "material_id");
	oname = bu_avs_get(avs, "material_name");
	if (listed_to_defined.find(std::string(oname)) == listed_to_defined.end()) {
	    bu_vls_printf(gedp->ged_result_str, "WARNING: unknown material %s found on object %s\n", oname, dp->d_namep);
	    bu_avs_free(avs);
	    BU_PUT(avs, struct bu_attribute_value_set);
	    continue;
	}

	id_found_cnt = analyze_densities_id((long int *)wids, 1, a, listed_to_defined[std::string(oname)].c_str());
	if (!id_found_cnt) {
	    bu_vls_printf(gedp->ged_result_str, "ERROR: failed to find ID for %s\n", oname);
	    goto ged_mater_mat_id_fail;
	}
	nid = wids[0];
	if (!mat_id || std::stoi(mat_id) != nid) {
	    (void)bu_avs_add(avs, "material_id", std::to_string(nid).c_str());
	    if (db5_update_attributes(dp, avs, gedp->dbip)) {
		bu_vls_printf(gedp->ged_result_str, "ERROR: failed to update object %s attributes\n", dp->d_namep);
		bu_avs_free(avs);
		BU_PUT(avs, struct bu_attribute_value_set);
		goto ged_mater_mat_id_fail;
	    }
	} else {
	    // Already has correct name, no need to update
	    bu_avs_free(avs);
	    BU_PUT(avs, struct bu_attribute_value_set);
	}
    }

    bu_vls_free(&args.density_file);
    bu_vls_free(&args.map_file);
    analyze_densities_destroy(a);
    return BRLCAD_OK;

ged_mater_mat_id_fail:
    if (a) {
	analyze_densities_destroy(a);
    }
    bu_vls_free(&args.density_file);
    bu_vls_free(&args.map_file);
    return BRLCAD_ERROR;
}


struct mater_import_args {
    int validate_input;
};

static const struct bu_cmd_option mater_import_options[] = {
    BU_CMD_FLAG("v", NULL, struct mater_import_args, validate_input,
	"Validate the density file before importing it"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand mater_file_operand[] = {
    BU_CMD_OPERAND("density_file", BU_CMD_VALUE_FILE, 1, 1,
	"Density data file", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_operand mater_audit_operands[] = {
    BU_CMD_OPERAND("density_file", BU_CMD_VALUE_FILE, 0, 1,
	"Optional density data file", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_operand mater_set_operands[] = {
    BU_CMD_OPERAND("record", BU_CMD_VALUE_RAW, 1, BU_CMD_COUNT_UNLIMITED,
	"Quoted id, density, name record", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema mater_source_schema = {
    "source", "Report the active density-data source", NULL, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema mater_clear_schema = {
    "clear", "Remove the database density table", NULL, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema mater_import_schema = {
    "import", "Import a density table into the database", mater_import_options,
    mater_file_operand, BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema mater_export_schema = {
    "export", "Export the database density table", NULL, mater_file_operand,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema mater_audit_schema = {
    "audit", "Audit material attributes against density data", NULL, mater_audit_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema mater_validate_schema = {
    "validate", "Validate a density data file", NULL, mater_file_operand,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema mater_set_schema = {
    "set", "Add or replace density records", NULL, mater_set_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema mater_density_schema = {
    "density", "Manage the database density table", NULL, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static const struct bu_cmd_tree_node mater_density_subcommands[] = {
    BU_CMD_TREE_NODE(&mater_audit_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&mater_clear_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&mater_export_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&mater_get_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&mater_import_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&mater_map_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&mater_set_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&mater_source_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&mater_validate_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE_NULL
};

static const struct bu_cmd_option mater_short_density_options[] = {
    BU_CMD_FLAG_UNBOUND("d", NULL, "density",
	"Select density-table management mode"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema mater_short_density_root_schema = {
    "mater", "Manage legacy material properties", mater_short_density_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema mater_long_density_root_schema = {
    "mater", "Manage legacy material properties", NULL, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_tree_node mater_long_density_subcommands[] = {
    BU_CMD_TREE_NODE(&mater_density_schema, NULL, mater_density_subcommands,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree mater_short_density_tree = {
    &mater_short_density_root_schema, mater_density_subcommands,
    BU_CMD_TREE_CHILD_AFTER_OPTIONS
};
static const struct bu_cmd_tree mater_long_density_tree = {
    &mater_long_density_root_schema, mater_long_density_subcommands,
    BU_CMD_TREE_CHILD_AFTER_OPTIONS
};

static const struct bu_cmd_option mater_shader_options[] = {
    BU_CMD_FLAG_UNBOUND("s", NULL, "shader",
	"Force shader mode when the object name resembles a density subcommand"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand mater_shader_operands[] = {
    BU_CMD_OPERAND("shader_argument", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Combination, shader, RGB, and inheritance arguments", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema mater_shader_schema = {
    "mater", "Assign shader, RGB, and inheritance properties", mater_shader_options,
    mater_shader_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

/* A short-option prefix has two possible command forms.  It is intentionally
 * separate so completion can offer both -d and -s before the choice is made. */
static const struct bu_cmd_option mater_prefix_options[] = {
    BU_CMD_FLAG_UNBOUND("d", NULL, "density", "Select density-table management mode"),
    BU_CMD_FLAG_UNBOUND("s", NULL, "shader", "Force shader mode"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema mater_prefix_schema = {
    "mater", "Select a mater command form", mater_prefix_options, mater_shader_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


static const struct bu_cmd_schema *
mater_density_schema_for_command(const char *command)
{
    const struct bu_cmd_tree_node *node = bu_cmd_tree_find_subcommand(
	&mater_short_density_tree, command);
    return node ? node->schema : NULL;
}


static int
mater_density_preflight(struct ged *gedp, const struct bu_cmd_schema *schema,
	size_t argc, const char *argv[])
{
    struct bu_cmd_validate_result result = BU_CMD_VALIDATE_RESULT_NULL;

    if (!schema || argc < 2)
	return BRLCAD_ERROR;
    if (bu_cmd_schema_validate(schema, argc - 2, argv + 2, argc - 2, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_VALID) {
	if (result.hint)
	    bu_vls_printf(gedp->ged_result_str, "%s\n", result.hint);
	bu_cmd_validate_result_clear(&result);
	return BRLCAD_ERROR;
    }
    bu_cmd_validate_result_clear(&result);
    return BRLCAD_OK;
}


static int
mater_density(struct ged *gedp, size_t argc, const char *argv[])
{

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", mater_usage);
	return GED_HELP;
    }

    const struct bu_cmd_schema *schema = mater_density_schema_for_command(argv[1]);
    if (!schema) {
	bu_vls_printf(gedp->ged_result_str, "Unknown -d subcommand: %s\n%s", argv[1], mater_usage);
	return BRLCAD_ERROR;
    }
    if (mater_density_preflight(gedp, schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (BU_STR_EQUAL(argv[1], "get")) {
	argv[1] = argv[0];
	argc--; argv++;
	return mater_get(gedp, argc, argv);
    }

    if (BU_STR_EQUAL(argv[1], "set")) {
	argv[1] = argv[0];
	argc--; argv++;
	return mater_set(gedp, argc, argv);
    }

    if (BU_STR_EQUAL(argv[1], "import")) {
	argv[1] = argv[0];
	argc--; argv++;
	return mater_import(gedp, argc, argv);
    }

    if (BU_STR_EQUAL(argv[1], "export")) {
	argv[1] = argv[0];
	argc--; argv++;
	return mater_export(gedp, argc, argv);
    }

    if (BU_STR_EQUAL(argv[1], "audit")) {
	argv[1] = argv[0];
	argc--; argv++;
	return mater_audit(gedp, argc, argv);
    }

    if (BU_STR_EQUAL(argv[1], "validate")) {
	argv[1] = argv[0];
	argc--; argv++;
	return mater_validate(gedp, argc, argv);
    }

    if (BU_STR_EQUAL(argv[1], "source")) {
	return mater_source(gedp);
    }

    if (BU_STR_EQUAL(argv[1], "clear")) {
	return mater_clear(gedp);
    }

    if (BU_STR_EQUAL(argv[1], "map")) {
	argv[1] = argv[0];
	argc--; argv++;
	return mater_mat_id(gedp, argc, argv);
    }

	return BRLCAD_ERROR;
}


extern "C" int
ged_mater_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", mater_usage);
	return GED_HELP;
    }

    /* The -s option allows us to do mater on an object even if the
     * object has a name matching a subcommand or option */
    if (BU_STR_EQUAL(argv[1], "-s")) {
	argv[1] = argv[0];
	argc--; argv++;
	return mater_shader(gedp, argc, argv);
    }

    if (BU_STR_EQUAL(argv[1], "-d") || BU_STR_EQUAL(argv[1], "density")) {
	argv[1] = argv[0];
	argc--; argv++;
	return mater_density(gedp, argc, argv);
    }

    /* If we aren't doing a mapping, proceed with normal behavior */
    return mater_shader(gedp, argc, argv);
}

#include "../include/plugin.h"

static const struct ged_cmd_native_form mater_native_forms[] = {
    {"density", NULL, &mater_long_density_tree},
    {"density_short", NULL, &mater_short_density_tree},
    {"shader", &mater_shader_schema, NULL},
    {"option_prefix", &mater_prefix_schema, NULL},
    {NULL, NULL, NULL}
};


static const struct ged_cmd_native_form *
mater_select_native_form(const struct ged *UNUSED(gedp), size_t argc,
	const char * const *argv)
{
    const char *word = argc > 1 && argv[1] ? argv[1] : "";
    size_t length = strlen(word);

    if (!length)
	return &mater_native_forms[0];
    if (BU_STR_EQUAL(word, "-"))
	return &mater_native_forms[3];
    if (!strncmp("-d", word, length))
	return &mater_native_forms[1];
    if (!strncmp("density", word, length))
	return &mater_native_forms[0];
    return &mater_native_forms[2];
}


static int
mater_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_native_forms_validate(gedp, mater_native_forms,
	mater_select_native_form, input, cursor_pos, result);
}


static int
mater_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_native_forms_analyze(gedp, mater_native_forms,
	mater_select_native_form, input, analysis);
}


static char *
mater_grammar_json(void)
{
    return ged_cmd_native_forms_describe_json("mater",
	"Query and assign legacy material properties", mater_native_forms);
}


static int
mater_grammar_lint(struct bu_vls *msgs)
{
    return ged_cmd_native_forms_lint("mater", mater_native_forms, msgs);
}


static const struct ged_cmd_grammar mater_grammar = {
    "mater", "Query and assign legacy material properties", mater_grammar_validate,
    mater_grammar_analyze, mater_grammar_json, mater_grammar_lint
};

#define GED_MATER_COMMANDS(X, XID) \
    X(mater, ged_mater_core, GED_CMD_DEFAULT, &mater_grammar) \

GED_DECLARE_COMMAND_SET_WITH_GRAMMAR(GED_MATER_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_GRAMMAR("libged_mater", 1, GED_MATER_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
