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

extern const char FACETIZE_WORKER_RESULT_OBJECT[];

struct FacetizeWorkerStatus {
    bool write_ready = false;
    bool write_started = false;
    bool write_done = false;
    bool result_received = false;
    int result = BRLCAD_ERROR;
    size_t payload_size = 0;
    size_t resident_size = 0;
};

struct FacetizeCommitRequest {
    std::string result_file;
    std::string object_name;
    size_t payload_size = 0;
};

/**
 * Resource limits for a primitive tessellation worker pool.
 *
 * A zero requested_workers selects the conservative automatic limit.  Memory
 * values are current system-wide byte counts; zero means the platform could
 * not supply that measurement.  Unknown memory disables outer parallelism.
 */
class FacetizeWorkerPolicy
{
    public:
	FacetizeWorkerPolicy(size_t requested_workers, size_t work_count,
		size_t available_cpus, size_t total_memory,
		size_t available_memory);

	size_t worker_count() const;
	size_t threads_per_worker() const;
	bool can_dispatch(size_t active_workers, size_t available_memory,
		size_t observed_worker_resident) const;

    private:
	size_t workers = 1;
	size_t worker_threads = 1;
	size_t memory_headroom = 0;
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
	bool send_commit(const char *result_file, const char *object_name,
		size_t payload_size);
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
	FacetizeWorkerReadResult receive_commit(FacetizeCommitRequest &request);
	bool send_write_ready(size_t payload_size, size_t resident_size);
	bool receive_write_proceed();
	bool send_write_started();
	bool send_tessellation_result(int result, size_t resident_size);
	bool send_write_result(int result, size_t resident_size);

    private:
	FILE *request_stream = NULL;
	FILE *response_stream = NULL;
};

#endif /* LIBGED_FACETIZE_WORKER_H */
