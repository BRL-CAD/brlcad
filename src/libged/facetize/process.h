/*                     P R O C E S S . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef LIBGED_FACETIZE_PROCESS_H
#define LIBGED_FACETIZE_PROCESS_H

#include <cstdio>
#include <string>
#include <vector>

#include "bu/defines.h"

#include "./worker.h"

struct bu_process;
struct _ged_facetize_state;

extern const size_t FACETIZE_WRITE_PROFILE_MIN_BYTES;

void
facetize_process_drain_stdout(struct _ged_facetize_state *state,
	struct bu_process *process, FacetizeWorkerClient &channel,
	FacetizeWorkerStatus *status);

void
facetize_process_drain_stderr(struct _ged_facetize_state *state,
	struct bu_process *process);

int
facetize_process_start(struct bu_process **process, FILE **process_input,
	FacetizeWorkerClient &channel,
	const std::vector<std::string> &command,
	const std::string &result_file, const char *mode);

int
facetize_process_reap(struct _ged_facetize_state *state,
	struct bu_process **process, FacetizeWorkerClient &channel,
	bool terminate);

int
facetize_process_stop(struct _ged_facetize_state *state,
	struct bu_process **process, FILE *&process_input,
	FacetizeWorkerClient &channel, bool terminate);

double
facetize_write_timeout_seconds(size_t payload_size,
	double profiled_write_bytes, double profiled_write_usec);

int
facetize_file_copy(const char *source, const char *destination);

#endif /* LIBGED_FACETIZE_PROCESS_H */
