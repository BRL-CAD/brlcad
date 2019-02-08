/*                         M A T E R . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2019 United States Government as represented by
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
#include <string>
#include <sstream>

#include "analyze.h"
#include "ged.h"

#include <string.h>

static const char *usage = "Usage: mater [-s] object_name shader r [g b] inherit\n"
                           "              -m  density_file [map_file]\n"
                           "              -m  --names-from-ids density_file\n";

int
_ged_mater_shader(struct ged *gedp, int argc, const char *argv[])
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
	bu_vls_printf(gedp->ged_result_str, "%s", usage);
	return GED_HELP;
    }

    GED_DB_LOOKUP(gedp, dp, argv[1], LOOKUP_NOISY, GED_ERROR);
    GED_CHECK_COMB(gedp, dp, GED_ERROR);
    GED_DB_GET_INTERNAL(gedp, &intern, dp, (fastf_t *)NULL, &rt_uniresource, GED_ERROR);

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
	bu_vls_printf(gedp->ged_result_str, "Too many arguments.\n%s", usage);
	return GED_ERROR;
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
	    if (bu_shader_to_list(bu_vls_addr(&vls), &comb->shader)) {
		bu_vls_printf(gedp->ged_result_str, "Problem with shader string [%s]", argv[2]);
		rt_db_free_internal(&intern);
		bu_vls_free(&vls);
		return GED_ERROR;
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
		return GED_ERROR;
	    }
	} else {
	    if (sscanf(bu_vls_addr(&rgb), "%d", &r) != 1 || r < 0 || 255 < r) {
		bu_vls_printf(gedp->ged_result_str, "Bad color value [%s]", argv[3]);
		rt_db_free_internal(&intern);
		bu_vls_free(&rgb);
		return GED_ERROR;
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
		return GED_ERROR;
	    }
	} else {
	    if (sscanf(bu_vls_addr(&rgb), "%d", &g) != 1 || g < 0 || 255 < g) {
		bu_vls_printf(gedp->ged_result_str, "Bad color value [%s]", argv[4]);
		rt_db_free_internal(&intern);
		bu_vls_free(&rgb);
		return GED_ERROR;
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
		return GED_ERROR;
	    }
	} else {
	    if (sscanf(bu_vls_addr(&rgb), "%d", &b) != 1 || b < 0 || 255 < b) {
		bu_vls_printf(gedp->ged_result_str, "Bad color value [%s]", argv[5]);
		rt_db_free_internal(&intern);
		bu_vls_free(&rgb);
		return GED_ERROR;
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
	    return GED_ERROR;
	}
    }

    bu_avs_init_empty(&avs);
    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
	bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
	bu_avs_free(&avs);
	return GED_ERROR;
    }
    db5_standardize_avs(&avs);
    db5_sync_comb_to_attr(&avs, comb);
    db5_standardize_avs(&avs);

    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);

    if (db5_update_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: failed to update attributes\n");
	bu_avs_free(&avs);
	return GED_ERROR;
    }

    bu_avs_free(&avs);

    return GED_OK;
}

int
_ged_read_msmap(struct ged *gedp, const char *mbuff, std::set<std::string> &defined_materials, std::map<std::string,std::string> &listed_to_defined)
{
    int ret = GED_OK;
    std::string mb(mbuff);
    std::istringstream ss(mb);
    std::string line;
    std::string ws("\t ");
    std::string ws2("\t \"");
    std::string tc("\"");
    std::string sep("\t ,;:\"=");
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
	    ret = GED_ERROR;
	} else {
	    // Have names, process
	    if (listed_to_defined.find(m1) != listed_to_defined.end()) {
		bu_vls_printf(gedp->ged_result_str, "%s is multiply defined in the mapping file", m1.c_str());
		ret = GED_ERROR;
	    }
	    if (defined_materials.find(m2) == defined_materials.end()) {
		bu_vls_printf(gedp->ged_result_str, "%s is not known in the density file", m2.c_str());
		ret = GED_ERROR;
	    }
	    listed_to_defined.insert(std::pair<std::string, std::string>(m1, m2));
	}
    }
    return ret;
}

int
_ged_mater_mat_id(struct ged *gedp, int argc, const char *argv[])
{
    int names_from_ids = 0;
    struct analyze_densities *densities;
    struct bu_mapped_file *dfile = NULL;
    char *dbuff = NULL;
    struct bu_mapped_file *mfile = NULL;
    char *mbuff = NULL;
    std::set<std::string> listed_materials;
    std::set<std::string> defined_materials;
    std::map<std::string,std::string> listed_to_defined;

    if (BU_STR_EQUAL(argv[1], "--names-from-ids")) {
	argv[1] = argv[0];
	argc--; argv++;
	names_from_ids = 1;
    }

    if (argc < 2 || argc > 3) {
	bu_vls_printf(gedp->ged_result_str, "%s", usage);
	return GED_OK;
    }
    if (!bu_file_exists(argv[1], NULL)) {
	bu_vls_printf(gedp->ged_result_str, "density file %s not found", argv[1]);
	return GED_ERROR;
    }
    if (argc == 3 && names_from_ids) {
	bu_vls_printf(gedp->ged_result_str, "Warning - only the density file is used mapping from material ids to names, ignoring %s", argv[2]);
    }
    if (argc == 3 && !names_from_ids && !bu_file_exists(argv[2], NULL)) {
	bu_vls_printf(gedp->ged_result_str, "material simplification mapping file %s not found", argv[2]);
	return GED_ERROR;
    }

    /* OK, we know what we're doing - start reading inputs */

    analyze_densities_create(&densities);

    dfile = bu_open_mapped_file(argv[1], "densities file");
    if (!dfile) {
	bu_vls_printf(gedp->ged_result_str, "could not open density file %s", argv[1]);
	return GED_ERROR;
    }

    dbuff = (char *)(dfile->buf);
    struct bu_vls pbuff_msgs = BU_VLS_INIT_ZERO;
    if (analyze_densities_load(densities, dbuff, &pbuff_msgs) ==  0) {
	bu_vls_printf(gedp->ged_result_str, "could not parse density file %s: %s", argv[1], bu_vls_cstr(&pbuff_msgs));
	bu_close_mapped_file(dfile);
	bu_vls_free(&pbuff_msgs);
	return GED_ERROR;
    }
    bu_close_mapped_file(dfile);

    // For simplicity, let the complex -> simple map know to map known materials to themselves
    long int curr_id = -1;
    long int id_cnt = 0;
    while ((curr_id = analyze_densities_next(densities, curr_id)) != -1) {
	id_cnt++;
	char *cname = analyze_densities_name(densities, curr_id);
	listed_to_defined.insert(std::pair<std::string, std::string>(std::string(cname), std::string(cname)));
	defined_materials.insert(std::string(cname));
	bu_free(cname, "free name copy");
    }

    if (argc == 3 && !names_from_ids) {
	mfile = bu_open_mapped_file(argv[2], "material simplification mapping file");
	if (!mfile) {
	    bu_vls_printf(gedp->ged_result_str, "could not open material simplification mapping file %s", argv[2]);
	    return GED_ERROR;
	}
    }
    if (mfile) {
	/* Populate the complex name -> simple name map from the mapping file, if we have one. */
	mbuff = (char *)(mfile->buf);
	if (_ged_read_msmap(gedp, mbuff, defined_materials, listed_to_defined) !=  GED_OK) {
	    bu_vls_printf(gedp->ged_result_str, "error reading material simplification mapping file %s", argv[2]);
	    return GED_ERROR;
	}
	bu_close_mapped_file(mfile);
    }

    // Find the objects we need to work with (if any)
    const char *mname_search = "-attr material_name";
    const char *mid_search = "-attr material_id";
    struct bu_ptbl mn_objs = BU_PTBL_INIT_ZERO;
    struct bu_ptbl id_objs = BU_PTBL_INIT_ZERO;
    std::set<struct directory *> mns, ids;
    db_update_nref(gedp->ged_wdbp->dbip, &rt_uniresource);
    (void)db_search(&mn_objs, DB_SEARCH_TREE|DB_SEARCH_RETURN_UNIQ_DP, mname_search, 0, NULL, gedp->ged_wdbp->dbip, NULL);
    (void)db_search(&id_objs, DB_SEARCH_TREE|DB_SEARCH_RETURN_UNIQ_DP, mid_search, 0, NULL, gedp->ged_wdbp->dbip, NULL);
    for(size_t i = 0; i < BU_PTBL_LEN(&mn_objs); i++) {
	struct directory *d = (struct directory *)BU_PTBL_GET(&mn_objs, i);
	mns.insert(d);
    }
    for(size_t i = 0; i < BU_PTBL_LEN(&id_objs); i++) {
	struct directory *d = (struct directory *)BU_PTBL_GET(&id_objs, i);
	ids.insert(d);
    }
    db_search_free(&mn_objs);
    db_search_free(&id_objs);


    if (names_from_ids) {
	std::set<struct directory *>::iterator d_it;
	for (d_it = ids.begin(); d_it != ids.end(); d_it++) {
	    struct directory *dp = *d_it;
	    struct bu_attribute_value_set avs;
	    bu_avs_init_empty(&avs);
	    if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
		bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
		return GED_ERROR;
	    }
	    const char *mat_id = bu_avs_get(&avs, "material_id");
	    const char *oname = bu_avs_get(&avs, "material_name");
	    if (analyze_densities_density(densities, std::stol(mat_id)) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Warning: no name found in density file for material_id %s on object %s, skipping\n", mat_id, dp->d_namep);
		if (oname) {
		    long int found_id_cnt = analyze_densities_id(NULL, 0, densities, oname);
		    if (found_id_cnt) {
			bu_vls_printf(gedp->ged_result_str, "Material id collision: object %s has material_name \"%s\" and material_id %s, but the specified density file defines \"%s\" as a different material id.\n", dp->d_namep, oname, mat_id, oname);
		    }  else {
			bu_vls_printf(gedp->ged_result_str, "Unknown material: object %s has material_name %s, which is not a material defined in the specified density file.\n", dp->d_namep, oname);
		    }
		}
		bu_avs_free(&avs);
		continue;
	    }
	    // Found a name, assign it if it doesn't match
	    char *nname = analyze_densities_name(densities, std::stol(mat_id));
	    if (!oname || !BU_STR_EQUAL(nname,oname)) {
		(void)bu_avs_add(&avs, "material_name", nname);
		if (db5_update_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
		    bu_vls_printf(gedp->ged_result_str, "Error: failed to update object %s attributes\n", dp->d_namep);
		    bu_avs_free(&avs);
		    return GED_ERROR;
		}
	    } else {
		// Already has correct name, no need to update
		bu_avs_free(&avs);
	    }
	    bu_free(nname, "free name");
	}
	return GED_OK;
    } else {
	std::set<struct directory *> ids_wo_names;
	std::set<struct directory *>::iterator d_it;
	for (d_it = ids.begin(); d_it != ids.end(); d_it++) {
	    struct directory *dp = *d_it;
	    if (mns.find(dp) == mns.end()) {
		ids_wo_names.insert(dp);
	    }
	}
	if (ids_wo_names.size()) {
	    bu_vls_printf(gedp->ged_result_str, "Warning - the following object(s) have a material_id attribute but no material_name attribute (use the \"--names_from_ids\" option to assign them names using a density file):\n");
	    for (d_it = ids_wo_names.begin(); d_it != ids_wo_names.end(); d_it++) {
		bu_vls_printf(gedp->ged_result_str, "%s\n", (*d_it)->d_namep);
	    }
	}
    }

    std::set<struct directory *>::iterator dp_it;
    for (dp_it = mns.begin(); dp_it != mns.end(); dp_it++) {
	struct directory *dp = *dp_it;
	struct bu_attribute_value_set avs;
	bu_avs_init_empty(&avs);
	if (db5_get_attributes(gedp->ged_wdbp->dbip, &avs, dp)) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
	    return GED_ERROR;
	}
	const char *mat_id = bu_avs_get(&avs, "material_id");
	const char *oname = bu_avs_get(&avs, "material_name");
	if (listed_to_defined.find(std::string(oname)) == listed_to_defined.end()) {
	    bu_vls_printf(gedp->ged_result_str, "Warning - unknown material %s found on object %s\n", oname, dp->d_namep);
	    continue;
	}

	long int wids[1];
	int id_found_cnt = analyze_densities_id((long int *)wids, 1, densities, listed_to_defined[std::string(oname)].c_str());
	if (!id_found_cnt) {
	    bu_vls_printf(gedp->ged_result_str, "Error: failed to find ID for %s\n", oname);
	    return GED_ERROR;
	}
	int nid = wids[0];
	if (!mat_id || std::stoi(mat_id) != nid) {
	    (void)bu_avs_add(&avs, "material_id", std::to_string(nid).c_str());
	    if (db5_update_attributes(dp, &avs, gedp->ged_wdbp->dbip)) {
		bu_vls_printf(gedp->ged_result_str, "Error: failed to update object %s attributes\n", dp->d_namep);
		bu_avs_free(&avs);
		return GED_ERROR;
	    }
	} else {
	    // Already has correct name, no need to update
	    bu_avs_free(&avs);
	}
    }

    return GED_OK;
}

int
ged_mater(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", usage);
	return GED_HELP;
    }

    /* The -s option allows us to do mater on an object even in the unlikely
     * even the object is named "-m" */
    if (BU_STR_EQUAL(argv[1], "-s")) {
	argv[1] = argv[0];
	argc--; argv++;
	return _ged_mater_shader(gedp, argc, argv);
    }

    if (BU_STR_EQUAL(argv[1], "-m")) {
	argv[1] = argv[0];
	argc--; argv++;
	return _ged_mater_mat_id(gedp, argc, argv);
    }

    /* If we aren't instructed to do a mapping, proceed with normal behavior */
    return _ged_mater_shader(gedp, argc, argv);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

