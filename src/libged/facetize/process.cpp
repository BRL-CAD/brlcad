/*                  P R O C E S S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "common.h"

#include <algorithm>
#include <fstream>

#include "bu/datetime.h"
#include "bu/file.h"
#include "bu/process.h"
#include "bu/snooze.h"

#include "./ged_facetize.h"
#include "./process.h"

static const size_t FACETIZE_PROCESS_IO_BUFFER_SIZE = 4096u;
static const size_t FACETIZE_FILE_COPY_BUFFER_SIZE = 1024u * 1024u;
static const int FACETIZE_PROCESS_POLL_USEC = 1000;
static const int FACETIZE_PROCESS_SHUTDOWN_TIMEOUT_SEC = 5;
static const double FACETIZE_USEC_TO_SEC_DIVISOR = 1.0e6;
static const double FACETIZE_WRITE_TIMEOUT_FACTOR = 10.0;
static const double FACETIZE_WRITE_TIMEOUT_MIN_SEC = 5.0;
static const double FACETIZE_WRITE_TIMEOUT_MAX_SEC = 300.0;
const size_t FACETIZE_WRITE_PROFILE_MIN_BYTES = 1024u * 1024u;

void
facetize_process_drain_stdout(struct _ged_facetize_state *state,
	struct bu_process *process, FacetizeWorkerClient &channel,
	FacetizeWorkerStatus *status)
{
    char buffer[FACETIZE_PROCESS_IO_BUFFER_SIZE];
    int fd = bu_process_fileno(process, BU_PROCESS_STDOUT);
    FacetizeWorkerStatus ignored_status;
    FacetizeWorkerStatus *output_status = status ? status : &ignored_status;
    std::vector<std::string> diagnostics;
    while (fd >= 0 && bu_process_pending(fd)) {
	int count = bu_process_read_n(process, BU_PROCESS_STDOUT,
		(int)sizeof(buffer), buffer);
	if (count <= 0)
	    break;
	channel.consume_output(buffer, (size_t)count, *output_status,
		diagnostics);
    }
    for (const std::string &diagnostic : diagnostics)
	facetize_log(state, 1, "%s\n", diagnostic.c_str());
}

void
facetize_process_drain_stderr(struct _ged_facetize_state *state,
	struct bu_process *process)
{
    char buffer[FACETIZE_PROCESS_IO_BUFFER_SIZE];
    int fd = bu_process_fileno(process, BU_PROCESS_STDERR);
    while (fd >= 0 && bu_process_pending(fd)) {
	int count = bu_process_read_n(process, BU_PROCESS_STDERR,
		(int)sizeof(buffer), buffer);
	if (count <= 0)
	    break;
	facetize_log(state, 1, "%.*s", count, buffer);
    }
}

int
facetize_process_reap(struct _ged_facetize_state *state,
	struct bu_process **process, FacetizeWorkerClient &channel,
	bool terminate)
{
    if (!process || !*process)
	return BRLCAD_ERROR;
    if (terminate)
	(void)bu_process_terminate(*process);

    int64_t deadline = bu_gettime() +
	BU_SEC2USEC(FACETIZE_PROCESS_SHUTDOWN_TIMEOUT_SEC);
    int poll_result = bu_process_poll(*process, NULL);
    while (poll_result == 0 && bu_gettime() < deadline) {
	facetize_process_drain_stdout(state, *process, channel, NULL);
	facetize_process_drain_stderr(state, *process);
	(void)bu_snooze(FACETIZE_PROCESS_POLL_USEC);
	poll_result = bu_process_poll(*process, NULL);
    }
    facetize_process_drain_stdout(state, *process, channel, NULL);
    facetize_process_drain_stderr(state, *process);
    if (poll_result != 1)
	(void)bu_process_terminate(*process);
    return bu_process_wait_n(process, 0);
}

int
facetize_process_stop(struct _ged_facetize_state *state,
	struct bu_process **process, FILE *&process_input,
	FacetizeWorkerClient &channel, bool terminate)
{
    int status = facetize_process_reap(state, process, channel, terminate);
    process_input = NULL;
    channel.reset(NULL);
    return status;
}

int
facetize_process_start(struct bu_process **process, FILE **process_input,
	FacetizeWorkerClient &channel,
	const std::vector<std::string> &command,
	const std::string &result_file, const char *mode)
{
    if (!process || !process_input || !mode)
	return BRLCAD_ERROR;

    std::vector<const char *> process_command;
    for (const std::string &argument : command)
	process_command.push_back(argument.c_str());
    if (!result_file.empty()) {
	process_command.push_back("--result-file");
	process_command.push_back(result_file.c_str());
    }
    process_command.push_back(mode);
    process_command.push_back(NULL);

    *process = NULL;
    *process_input = NULL;
    channel.reset(NULL);
    bu_process_create(process, process_command.data(), BU_PROCESS_HIDE_WINDOW);
    if (*process) {
	*process_input = bu_process_file_open(*process, BU_PROCESS_STDIN);
	channel.reset(*process_input);
    }
    return (*process && *process_input) ? BRLCAD_OK : BRLCAD_ERROR;
}

double
facetize_write_timeout_seconds(size_t payload_size,
	double profiled_write_bytes, double profiled_write_usec)
{
    if (profiled_write_bytes <= 0.0 || profiled_write_usec <= 0.0)
	return FACETIZE_WRITE_TIMEOUT_MAX_SEC;

    double bytes_per_second = profiled_write_bytes *
	FACETIZE_USEC_TO_SEC_DIVISOR / profiled_write_usec;
    double projected_seconds = (double)payload_size / bytes_per_second;
    double timeout_seconds = projected_seconds * FACETIZE_WRITE_TIMEOUT_FACTOR;
    return std::min(FACETIZE_WRITE_TIMEOUT_MAX_SEC,
	    std::max(FACETIZE_WRITE_TIMEOUT_MIN_SEC, timeout_seconds));
}

int
facetize_file_copy(const char *source, const char *destination,
	FacetizeFileCopyProgress progress, void *progress_data)
{
    if (!source || !destination)
	return BRLCAD_ERROR;

    std::ifstream input(source, std::ios::binary);
    if (!input.is_open())
	return BRLCAD_ERROR;
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output.is_open())
	return BRLCAD_ERROR;

    std::vector<char> buffer(FACETIZE_FILE_COPY_BUFFER_SIZE);
    uint64_t bytes_copied = 0;
    bool copy_succeeded = true;
    while (input) {
	input.read(buffer.data(), (std::streamsize)buffer.size());
	std::streamsize count = input.gcount();
	if (count <= 0)
	    break;
	output.write(buffer.data(), count);
	if (!output.good()) {
	    copy_succeeded = false;
	    break;
	}
	bytes_copied += (uint64_t)count;
	if (progress)
	    progress(bytes_copied, progress_data);
    }
    copy_succeeded = copy_succeeded && input.eof() && !input.bad();
    if (copy_succeeded) {
	output.flush();
	copy_succeeded = output.good();
    }
    input.close();
    output.close();
    if (!copy_succeeded)
	bu_file_delete(destination);
    return copy_succeeded ? BRLCAD_OK : BRLCAD_ERROR;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
