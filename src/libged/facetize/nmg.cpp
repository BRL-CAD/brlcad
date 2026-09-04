/*                        N M G  . C P P
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
/** @file libged/facetize/nmg.cpp
 *
 * Parent-side launch policy for isolated NMG Boolean evaluation.
 */

#include "common.h"

#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/datetime.h"
#include "bu/file.h"
#include "bu/path.h"
#include "bu/process.h"
#include "bu/snooze.h"
#include "raytrace.h"

#include "./ged_facetize.h"
#include "./process.h"
#include "./transfer.h"
#include "./worker.h"

static const int FACETIZE_NMG_POLL_USEC = 1000;
static const char FACETIZE_NMG_RESULT_FILE[] = "nmg_boolean_result.g";

int
_ged_facetize_nmgeval(struct _ged_facetize_state *s,
	struct db_i *target_dbip, const char *database_path,
	const std::vector<std::string> &input_names, const char *output_name)
{
    if (!s || !target_dbip || !database_path || !database_path[0] ||
	    input_names.empty() || !output_name || !output_name[0] || !s->wdir)
	return BRLCAD_ERROR;

    FacetizeWorkerRequest request;
    request.operation = s->execution.writes_nmg() ?
	FacetizeWorkerOperation::NmgBooleanToNmg :
	FacetizeWorkerOperation::NmgBooleanToBot;
    request.input_names = input_names;
    request.output_name = output_name;
    if (!request.valid())
	return BRLCAD_ERROR;

    char result_file[MAXPATHLEN];
    bu_dir(result_file, MAXPATHLEN, s->wdir, FACETIZE_NMG_RESULT_FILE, NULL);
    (void)bu_file_delete(result_file);

    char executable[MAXPATHLEN];
    bu_dir(executable, MAXPATHLEN, BU_DIR_BIN, "ged_exec", BU_DIR_EXT, NULL);
    std::vector<std::string> command;
    command.push_back(executable);
    command.push_back("facetize_process");
    command.push_back(database_path);

    struct bu_process *process = NULL;
    FILE *process_input = NULL;
    FacetizeWorkerClient channel;
    if (facetize_process_start(&process, &process_input, channel, command,
	    result_file, "--nmg-server") != BRLCAD_OK) {
	(void)bu_file_delete(result_file);
	facetize_log(s, 0,
		"FACETIZE: unable to start the NMG Boolean worker for %s\n",
		output_name);
	return BRLCAD_ERROR;
    }
    if (!channel.send_request(request)) {
	(void)facetize_process_stop(s, &process, process_input, channel, true);
	(void)bu_file_delete(result_file);
	facetize_log(s, 0,
		"FACETIZE: unable to submit %s to the NMG Boolean worker\n",
		output_name);
	return BRLCAD_ERROR;
    }

    facetize_log(s, 1, "FACETIZE: evaluating %s with NMG Boolean...\n",
	    output_name);
    FacetizeWorkerStatus status;
    int64_t request_start = bu_gettime();
    int64_t write_deadline = 0;
    bool write_permitted = false;
    bool interrupted = false;
    bool evaluation_timed_out = false;
    bool write_timed_out = false;
    while (!status.result_received) {
	facetize_process_drain_stdout(s, process, channel, &status);
	facetize_process_drain_stderr(s, process);

	int64_t now = bu_gettime();
	if (status.write_ready && !write_permitted) {
	    if (s->max_time > 0 && now - request_start >=
		    BU_SEC2USEC(s->max_time)) {
		interrupted = true;
		evaluation_timed_out = true;
		break;
	    }
	    write_permitted = true;
	    double write_timeout = facetize_write_timeout_seconds(
		    status.payload_size, 0.0, 0.0);
	    write_deadline = now + BU_SEC2USEC(write_timeout);
	    if (!channel.send_write_proceed()) {
		interrupted = true;
		break;
	    }
	    continue;
	}
	if (status.result_received) {
	    if (status.write_started && !status.write_done)
		interrupted = true;
	    break;
	}
	if (bu_process_poll(process, NULL) != 0) {
	    /* The final status may reach the pipe just before process exit.
	     * Drain once more before deciding the worker was interrupted. */
	    facetize_process_drain_stdout(s, process, channel, &status);
	    facetize_process_drain_stderr(s, process);
	    interrupted = !status.result_received ||
		(status.write_started && !status.write_done);
	    break;
	}
	if (!write_permitted && s->max_time > 0 &&
		now - request_start >= BU_SEC2USEC(s->max_time)) {
	    interrupted = true;
	    evaluation_timed_out = true;
	    break;
	}
	if (write_permitted && now >= write_deadline) {
	    interrupted = true;
	    write_timed_out = true;
	    break;
	}
	(void)bu_snooze(FACETIZE_NMG_POLL_USEC);
    }

    int process_status = facetize_process_stop(s, &process, process_input,
	    channel, interrupted);
    int ret = BRLCAD_ERROR;
    if (!interrupted && process_status == BRLCAD_OK &&
	    status.result_received && status.write_done &&
	    status.result == BRLCAD_OK) {
	int expected_type = request.operation ==
	    FacetizeWorkerOperation::NmgBooleanToNmg ? ID_NMG : ID_BOT;
	ret = facetize_transfer_staged_object(target_dbip, result_file,
		output_name, expected_type);
    }
    (void)bu_file_delete(result_file);

    if (ret == BRLCAD_OK) {
	facetize_log(s, 1,
		"FACETIZE: NMG Boolean evaluation succeeded for %s\n",
		output_name);
	return BRLCAD_OK;
    }
    if (evaluation_timed_out) {
	facetize_log(s, 0,
		"FACETIZE: NMG Boolean evaluation timed out after %d seconds for %s\n",
		s->max_time, output_name);
    } else if (write_timed_out) {
	facetize_log(s, 0,
		"FACETIZE: staging the NMG Boolean result timed out for %s\n",
		output_name);
    } else if (interrupted) {
	facetize_log(s, 0,
		"FACETIZE: NMG Boolean worker exited while processing %s\n",
		output_name);
    } else if (process_status != BRLCAD_OK) {
	facetize_log(s, 0,
		"FACETIZE: NMG Boolean worker exited abnormally after processing %s\n",
		output_name);
    } else if (status.result != BRLCAD_OK) {
	facetize_log(s, 0,
		"FACETIZE: NMG Boolean evaluation failed for %s\n",
		output_name);
    } else {
	facetize_log(s, 0,
		"FACETIZE: unable to transfer the staged NMG Boolean result for %s\n",
		output_name);
    }
    return BRLCAD_ERROR;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
