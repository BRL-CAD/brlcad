/*                      G L T F - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022-2026 United States Government as represented by
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

#include "common.h"

#include <string>

#include "bu/app.h"
#include "bu/opt.h"
#include "bu/units.h"
#include "wdb.h"

#include "gltf_read_util.h"
#include "gltf_scene.h"

int
main(int argc, char **argv)
{
    const char * const usage = "Usage: gltf-g [options] input_file [output_file.g]\n";
    long verbosity = 0;
    int print_help = 0;
    int extensions = 0;
    unsigned int reject_losses = 0;
    const char *output_path = nullptr;

    struct bu_opt_desc d[7];
    BU_OPT(d[0], "h", "help",       "", NULL,              &print_help,   "Print help and exit");
    BU_OPT(d[1], "?", "",           "", NULL,              &print_help,   "");
    BU_OPT(d[2], "v", "verbosity",  "", &bu_opt_incr_long, &verbosity,    "Increase verbosity level");
    BU_OPT(d[3], "o", "output", "path", &bu_opt_str,       &output_path,  "Set output filename");
    BU_OPT(d[4], "E", "extensions", "", NULL,              &extensions,   "Store original JSON for extensions");
    BU_OPT(d[5], "", "reject-loss", "category", &gcv_opt_loss_policy, &reject_losses,
	"Reject geometry, appearance, behavior, metadata or all losses (repeatable)");
    BU_OPT_NULL(d[6]);

    bu_setprogname(argv[0]);

    if (argc > 0) {
	--argc;
	++argv;
    }
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    int opt_ret = bu_opt_parse(&messages, argc, (const char **)argv, d);

    if (print_help) {
	char *help = bu_opt_describe(d, NULL);
	bu_log("%s\nOptions:\n%s", usage, help);
	if (help)
	    bu_free(help, "help str");
	bu_vls_free(&messages);
	return BRLCAD_OK;
    }

    if (opt_ret < 0) {
	bu_log("%s\n%s", bu_vls_cstr(&messages), usage);
	bu_vls_free(&messages);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&messages);
    argc = opt_ret;
    if (argc != (output_path ? 1 : 2)) {
	bu_log("Specify one input and one output filename.\n%s", usage);
	return BRLCAD_ERROR;
    }
    if (!output_path)
	output_path = argv[1];

    tinygltf3::Model model;
    tinygltf3::ErrorStack errors;
    brlcad_gltf::confined_file_reader reader(argv[0]);
    tg3_error_code parse_result = reader.parse(model.get(), errors.get(), extensions != 0);
    brlcad_gltf::log_errors(*errors.get());
    if (parse_result != TG3_OK) {
	bu_log("Failed to parse glTF input %s (error %d)\n", argv[0],
	    static_cast<int>(parse_result));
	return BRLCAD_ERROR;
    }

    if (!brlcad_gltf::profile_valid(*model.get(), argv[0]))
	return BRLCAD_ERROR;
    bool permitted = true;
    for (const auto &loss : brlcad_gltf::scene_losses(*model.get())) {
	bu_log("Partial conversion: %s: %s (%zu): %s\n", gcv_loss_kind_name(loss.kind), loss.feature, loss.count, loss.message);
	if (reject_losses & loss.kind) permitted = false;
    }
    if (!permitted) {
	bu_log("Conversion rejected by the requested loss policy\n");
	return BRLCAD_ERROR;
    }
    struct db_i *database = db_create_inmem();
    if (!database)
	return BRLCAD_ERROR;
    struct rt_wdb *writer = wdb_dbopen(database, RT_WDB_TYPE_DB_DEFAULT);
    std::string title = "gltf-g import of " + std::string(argv[0]);
    if (verbosity)
	bu_log("Importing the selected scene (%u available meshes)\n", model->meshes_count);
    bool converted = writer && mk_id_units(writer, title.c_str(), "mm") >= 0 &&
	brlcad_gltf::convert_scene(writer, *model.get(), bu_units_conversion("m"));
    if (!converted) {
	db_close(database);
	return BRLCAD_ERROR;
    }
    struct rt_wdb *output = wdb_fopen(output_path);
    bool written = output && db_dump(output, database) == 0;
    if (output)
	db_close(output->dbip);
    db_close(database);
    if (!written)
	bu_log("Unable to write glTF output %s\n", output_path);
    return written ? BRLCAD_OK : BRLCAD_ERROR;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
