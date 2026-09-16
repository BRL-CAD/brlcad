/*                       W O R K E R . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef LIBGED_FACETIZE_WORKER_H
#define LIBGED_FACETIZE_WORKER_H

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include "bu/defines.h"

struct FacetizeWorkerStatus {
    bool write_ready = false;
    bool write_done = false;
    bool result_received = false;
    int result = BRLCAD_ERROR;
    size_t payload_size = 0;
};

/**
 * Parent-side interface to a facetize worker's stdin/stdout protocol.
 *
 * The channel borrows the request stream.  The caller retains ownership of
 * the FILE and is responsible for supervising the worker process.  Worker
 * output may arrive in arbitrary chunks; consume_output() retains incomplete
 * protocol lines until more data arrives.
 */
class FacetizeWorkerClient
{
    public:
	FacetizeWorkerClient() = default;

	void reset(FILE *request_stream);
	bool send_request(const char *object_name);
	bool send_write_proceed();
	void consume_output(const char *data, size_t data_size,
		FacetizeWorkerStatus &status,
		std::vector<std::string> &diagnostics);

    private:
	FILE *request_stream = NULL;
	std::string pending_output;
};

enum class FacetizeWorkerReadResult {
    Error = -1,
    End = 0,
    Request = 1
};

/**
 * Child-side interface to the facetize worker protocol.
 *
 * The channel borrows both streams.  Closing stdin is the normal request-loop
 * shutdown signal; malformed or incomplete messages are reported as errors.
 */
class FacetizeWorkerServer
{
    public:
	FacetizeWorkerServer(FILE *request_stream, FILE *response_stream);

	FacetizeWorkerReadResult receive_request(std::string &object_name);
	bool send_write_ready(size_t payload_size);
	bool receive_write_proceed();
	bool send_tessellation_result(int result);
	bool send_write_result(int result);

    private:
	FILE *request_stream = NULL;
	FILE *response_stream = NULL;
};

#endif /* LIBGED_FACETIZE_WORKER_H */
