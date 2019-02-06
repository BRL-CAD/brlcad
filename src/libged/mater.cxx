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

#include <map>
#include <set>
#include <string>
#include <sstream>

#include "analyze.h"
#include "ged.h"

#include <string.h>

static const char *usage = "mater [-s] object_name shader r [g b] inherit\n              -m  density_file [map_file]";

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
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", usage);
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
	bu_vls_printf(gedp->ged_result_str, "Too many arguments.\nUsage: %s", usage);
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
_ged_read_msmap(struct ged *gedp, const char *mbuff, std::set<std::string> &known_materials, std::map<std::string,std::string> &complex_to_known)
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
	    bu_log("m1: %s", m1.c_str());
	    bu_log("m2: %s", m2.c_str());
	    if (complex_to_known.find(m1) != complex_to_known.end()) {
		bu_vls_printf(gedp->ged_result_str, "%s is multiply defined in the mapping file", m1.c_str());
		ret = GED_ERROR;
	    }
	    if (known_materials.find(m2) == known_materials.end()) {
		bu_vls_printf(gedp->ged_result_str, "%s is not known in the density file", m2.c_str());
		ret = GED_ERROR;
	    }
	    complex_to_known.insert(std::pair<std::string, std::string>(m1, m2));
	}
    }
    return ret;
}

int
_ged_mater_mat_id(struct ged *gedp, int argc, const char *argv[])
{
    int num_densities = 128;
    struct density_entry *densities = (struct density_entry *)bu_calloc(num_densities, sizeof(struct density_entry), "density entries");
    struct bu_mapped_file *dfile = NULL;
    char *dbuff = NULL;
    struct bu_mapped_file *mfile = NULL;
    char *mbuff = NULL;
    std::map<std::string, int> n2d;
    std::set<std::string> known_materials;
    std::map<std::string,std::string> complex_to_known;

    if (argc < 2 || argc > 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", usage);
	return GED_OK;
    }
    if (!bu_file_exists(argv[1], NULL)) {
	bu_vls_printf(gedp->ged_result_str, "density file %s not found", argv[1]);
	return GED_ERROR;
    }
    if (argc == 3 && !bu_file_exists(argv[2], NULL)) {
	bu_vls_printf(gedp->ged_result_str, "material simplification mapping file %s not found", argv[1]);
	return GED_ERROR;
    }

    dfile = bu_open_mapped_file(argv[1], "densities file");
    if (!dfile) {
	bu_vls_printf(gedp->ged_result_str, "could not open density file %s", argv[1]);
	return GED_ERROR;
    }

    dbuff = (char *)(dfile->buf);
    if (parse_densities_buffer(dbuff, dfile->buflen, &densities, NULL, &num_densities) !=  ANALYZE_OK) {
	bu_vls_printf(gedp->ged_result_str, "could not parse density file %s", argv[1]);
	bu_close_mapped_file(dfile);
	return GED_ERROR;
    } else {
	/* Populate the name->material_id map from the density file */
	int found_duplicate = 0;
	for (int idx = 0; idx < num_densities; idx++) {
	    if (densities[idx].magic == DENSITY_MAGIC) {
		if (n2d.find(std::string(densities[idx].name)) != n2d.end()) {
		    found_duplicate = 1;
		    bu_vls_printf(gedp->ged_result_str, "density file contains multiple densities for %s", densities[idx].name);
		}
		n2d[std::string(densities[idx].name)] = idx;
		known_materials.insert(std::string(densities[idx].name));
		bu_free(densities[idx].name, "free density name");
	    }
	}
	bu_free(densities, "density_entry array");
	bu_close_mapped_file(dfile);
	if (found_duplicate) {
	    return GED_ERROR;
	}
    }

    if (argc == 3) {
	mfile = bu_open_mapped_file(argv[2], "material simplification mapping file");
	if (!mfile) {
	    bu_vls_printf(gedp->ged_result_str, "could not open material simplification mapping file %s", argv[2]);
	    return GED_ERROR;
	}
    }
    if (mfile) {
	mbuff = (char *)(mfile->buf);
	if (_ged_read_msmap(gedp, mbuff, known_materials, complex_to_known) !=  GED_OK) {
	    bu_vls_printf(gedp->ged_result_str, "error reading material simplification mapping file %s", argv[2]);
	    return GED_ERROR;
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
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", usage);
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

