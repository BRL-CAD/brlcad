/*                       P I P E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2023 United States Government as represented by
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
#include <thread>

#include "vmath.h"
#include "bu/app.h"
#include "bu/opt.h"
#include "bu/process.h"
#include "bu/time.h"
#include "raytrace.h"
#include "../librt_private.h"

struct bu_external *
ext_receive(struct bu_process *p)
{
    if (!p)
	return NULL;

    struct bu_external *ext = NULL;
    rt_read_external(&ext, bu_process_fileno(p, BU_PROCESS_STDOUT), 0, 0);

    return ext;
}

int
ext_send(struct bu_process *p, struct bu_external *ext)
{
    if (!p || !ext)
	return BRLCAD_ERROR;

    rt_send_external(bu_process_open(p, BU_PROCESS_STDIN), ext);

    return BRLCAD_OK;
}

int
dp_send(struct bu_process *p, struct db_i *dbip, struct directory *dp)
{
    if (!p || !dbip || !dp)
	return BRLCAD_ERROR;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    int ret = rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);
    if (ret < 0) {
	bu_log("rt_db_get_internal failed for %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }

    struct bu_external ext;
    if (rt_db_cvt_to_external5(&ext, dp->d_namep, &intern, 1, dbip, &rt_uniresource, intern.idb_major_type) < 0) {
	bu_log("rt_db_cvt_to_external5 failed for %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }

    ext_send(p, &ext);
    rt_db_free_internal(&intern);
    bu_free_external(&ext);
    return BRLCAD_OK;
}

int
dp_cmp(struct db_i *dbip, struct directory *dp, struct bu_external *ext)
{
    if (!ext || !dbip || !dp)
	return -1;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    int ret = rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource);
    if (ret < 0) {
	bu_log("rt_db_get_internal failed for %s\n", dp->d_namep);
	return -1;
    }

    struct bu_external ctrl_ext;
    if (rt_db_cvt_to_external5(&ctrl_ext, dp->d_namep, &intern, 1, dbip, &rt_uniresource, intern.idb_major_type) < 0) {
	bu_log("rt_db_cvt_to_external5 failed for %s\n", dp->d_namep);
	return -1;
    }

    if (ext->ext_magic != ctrl_ext.ext_magic)
	return 1;
    if (ext->ext_nbytes != ctrl_ext.ext_nbytes)
	return 1;
    if (memcmp(ext->ext_buf, ctrl_ext.ext_buf, ctrl_ext.ext_nbytes))
	return 1;

    return 0;
}

int
main(int argc, char *argv[])
{
    int64_t start, elapsed;
    struct db_i *dbip;
    struct directory *dp;
    fastf_t seconds;
    int client_mode = 0;
    int server_mode = 0;
    int print_help = 0;
    int ret = BRLCAD_OK;
    const char *usage = "Usage: rt_pipe [options] [file.g]\n";

    bu_setprogname(argv[0]);
    const char *execfp = argv[0];

    // Done with prog name
    argc--; argv++;

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h",    "help",  "", NULL, &print_help,  "Print help and exit");
    BU_OPT(d[1], "c",  "client",  "", NULL, &client_mode, "Receive data from server");
    BU_OPT(d[2], "s",  "server",  "", NULL, &server_mode, "Send data to client");
    BU_OPT_NULL(d[3]);

    /* parse options */
    struct bu_vls omsg = BU_VLS_INIT_ZERO;
    argc = bu_opt_parse(&omsg, argc, (const char **)argv, d);
    if (argc < 0) {
	bu_log("Option parsing error: %s\n", bu_vls_cstr(&omsg));
	bu_vls_free(&omsg);
	bu_exit(BRLCAD_ERROR, "%s failed", bu_getprogname());
    }
    bu_vls_free(&omsg);

    if (server_mode && !argc)
	ret = BRLCAD_ERROR;

    if (print_help || (server_mode && !argc)) {
	struct bu_vls str = BU_VLS_INIT_ZERO;
	char *option_help;

	bu_vls_sprintf(&str, "%s", usage);

	if ((option_help = bu_opt_describe(d, NULL))) {
	    bu_vls_printf(&str, "Options:\n%s\n", option_help);
	    bu_free(option_help, "help str");
	}

	bu_log("%s\n", bu_vls_cstr(&str));
	bu_vls_free(&str);
	return ret;
    }

    if (server_mode) {
	start = bu_gettime();

	dbip = db_open(argv[0], DB_OPEN_READONLY);
	if (dbip == DBI_NULL) {
	    bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
	}

	if (db_dirbuild(dbip) < 0) {
	    bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
	}

	db_update_nref(dbip, &rt_uniresource);

	elapsed = bu_gettime() - start;
	seconds = elapsed / 1000000.0;
	bu_log("Initialization time: %0.6f seconds\n", seconds);


	for (int i = 0; i < RT_DBNHASH; i++) {
	    for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		start = bu_gettime();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		const char *client_cmd[MAXPATHLEN] = {NULL};
		client_cmd[0] = execfp;
		client_cmd[1] = "-c";

		struct bu_process *p = NULL;
		bu_process_exec(&p, client_cmd[0], 2, client_cmd, 0, 0);
		if (bu_process_pid(p) == -1) {
		    bu_log("client process launch failed, skipping %s\n", dp->d_namep);
		    continue;
		}


		if (dp_send(p, dbip, dp) != BRLCAD_OK) {
		    bu_log("%s failed, skipping\n", dp->d_namep);
		    bu_terminate(bu_process_pid(p));
		    continue;
		}

		struct bu_external *ext = ext_receive(p);
		if (!ext) {
		    bu_log("ext receiving failed for %s, skipping\n", dp->d_namep);
		    continue;
		}

		if (dp_cmp(dbip, dp, ext)) {
		    bu_log("ERROR: round trip check failed for %s\n", dp->d_namep);
		    continue;
		}

		elapsed = bu_gettime() - start;
		seconds = elapsed / 1000000.0;
		bu_log("%s processing time: %0.6f seconds\n", dp->d_namep, seconds);
	    }
	}

	db_close(dbip);
    }

    if (client_mode) {

	bu_log("client running\n");
	// read bu_external from stdin
	struct bu_external *ext = NULL;
	rt_read_external(&ext, fileno(stdin), 0, 0);

	// TODO - unpack, generate new bu_external from internal
	// send new bu_external to stdout
	rt_send_external(stdout, ext);
	bu_free_external(ext);
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

