/*                      W O R K E R . C P P
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/facetize/worker.cpp
 *
 * Message framing shared by the facetize parent and its persistent worker.
 */

#include "common.h"

#include <algorithm>
#include <sstream>

#include <errno.h>
#if defined(HAVE_PTHREAD_H) && defined(SIGPIPE)
#  include <pthread.h>
#  include <signal.h>
#  include <time.h>
#endif
#include <string.h>

#include "bu/log.h"
#include "./worker.h"

const char FACETIZE_WORKER_RESULT_OBJECT[] = "facetize_worker_result";

static const char FACETIZE_REQUEST_MAGIC[] = "FACETIZE_REQUEST";
static const char FACETIZE_COMMIT_MAGIC[] = "FACETIZE_COMMIT";
static const char FACETIZE_RESULT_MAGIC[] = "FACETIZE_RESULT";
static const char FACETIZE_WRITE_READY_MAGIC[] = "FACETIZE_WRITE_READY";
static const char FACETIZE_REGION_WRITE_READY_MAGIC[] =
    "FACETIZE_REGION_WRITE_READY";
static const char FACETIZE_WRITE_PROCEED_MAGIC[] = "FACETIZE_WRITE_PROCEED";
static const char FACETIZE_WRITE_STARTED_MAGIC[] = "FACETIZE_WRITE_STARTED";
static const char FACETIZE_WRITE_DONE_MAGIC[] = "FACETIZE_WRITE_DONE";
static const char FACETIZE_CSG_RESULT_MAGIC[] = "FACETIZE_CSG_RESULT";
static const unsigned int FACETIZE_PROTOCOL_VERSION = 2u;
static const size_t FACETIZE_PROTOCOL_FIELD_MAX = 1024u * 1024u;
static const size_t FACETIZE_PROTOCOL_LIST_MAX = 65536u;
static const size_t FACETIZE_PROTOCOL_HEADER_SIZE = 256u;
static const size_t FACETIZE_MIB = 1024u * 1024u;
static const size_t FACETIZE_MIN_MEMORY_HEADROOM = 1024u * FACETIZE_MIB;
static const size_t FACETIZE_MIN_WORKER_RESERVE = 512u * FACETIZE_MIB;
static const size_t FACETIZE_MEMORY_HEADROOM_DIVISOR = 4u;
static const size_t FACETIZE_AUTOMATIC_WORKER_LIMIT = 2u;

class FacetizePipeWriteGuard
{
    public:
	FacetizePipeWriteGuard()
	{
#if defined(HAVE_PTHREAD_H) && defined(SIGPIPE)
	    sigemptyset(&blocked_signals);
	    sigaddset(&blocked_signals, SIGPIPE);
	    active = pthread_sigmask(SIG_BLOCK, &blocked_signals,
		    &original_mask) == 0;
	    sigset_t pending_signals;
	    if (active && sigpending(&pending_signals) == 0)
		had_pending_sigpipe = sigismember(&pending_signals, SIGPIPE) == 1;
#endif
	}

	~FacetizePipeWriteGuard()
	{
#if defined(HAVE_PTHREAD_H) && defined(SIGPIPE)
	    if (!active)
		return;

	    int saved_errno = errno;
	    sigset_t pending_signals;
	    bool have_new_sigpipe = !had_pending_sigpipe &&
		sigpending(&pending_signals) == 0 &&
		sigismember(&pending_signals, SIGPIPE) == 1;
	    if (have_new_sigpipe) {
		struct timespec timeout = {0, 0};
		while (sigtimedwait(&blocked_signals, NULL, &timeout) < 0 &&
			errno == EINTR)
		    ;
	    }
	    (void)pthread_sigmask(SIG_SETMASK, &original_mask, NULL);
	    errno = saved_errno;
#endif
	}

    private:
#if defined(HAVE_PTHREAD_H) && defined(SIGPIPE)
	sigset_t blocked_signals;
	sigset_t original_mask;
	bool active = false;
	bool had_pending_sigpipe = false;
#endif
};

static bool
facetize_send_field(FILE *stream, const char *field, size_t field_length)
{
    return stream && field &&
	fwrite(field, 1, field_length, stream) == field_length &&
	fputc('\n', stream) != EOF;
}

static bool
facetize_send_sized_field(FILE *stream, const std::string &field)
{
    if (!stream || field.size() > FACETIZE_PROTOCOL_FIELD_MAX)
	return false;

    return fprintf(stream, "%zu\n", field.size()) >= 0 &&
	facetize_send_field(stream, field.c_str(), field.size());
}

static FacetizeWorkerReadResult
facetize_read_header(FILE *stream, char *header, size_t header_size)
{
    if (!stream || !header || header_size < 2)
	return FacetizeWorkerReadResult::Error;
    if (!bu_fgets(header, header_size, stream))
	return feof(stream) ? FacetizeWorkerReadResult::End :
	    FacetizeWorkerReadResult::Error;

    size_t header_length = strlen(header);
    return (header_length && header[header_length - 1] == '\n') ?
	FacetizeWorkerReadResult::Request : FacetizeWorkerReadResult::Error;
}

static bool
facetize_read_field(FILE *stream, size_t field_length, std::string &field)
{
    field.assign(field_length, '\0');
    if ((!field_length ||
	    fread(&field[0], 1, field_length, stream) == field_length) &&
	    fgetc(stream) == '\n')
	return true;

    field.clear();
    return false;
}

static bool
facetize_read_sized_field(FILE *stream, std::string &field)
{
    field.clear();
    char header[FACETIZE_PROTOCOL_HEADER_SIZE];
    if (facetize_read_header(stream, header, sizeof(header)) !=
	    FacetizeWorkerReadResult::Request)
	return false;

    size_t field_length = 0;
    std::istringstream header_stream(header);
    if (!(header_stream >> field_length) ||
	    !(header_stream >> std::ws).eof() ||
	    field_length > FACETIZE_PROTOCOL_FIELD_MAX)
	return false;

    return facetize_read_field(stream, field_length, field);
}

static bool
facetize_operation_valid(FacetizeWorkerOperation operation)
{
    return operation == FacetizeWorkerOperation::TessellatePrimitive ||
	operation == FacetizeWorkerOperation::ValidateCsg ||
	operation == FacetizeWorkerOperation::EvaluateRegion;
}

static bool
facetize_send_result(FILE *response_stream, const char *message_type, int result,
	size_t resident_size)
{
    if (!response_stream)
	return false;

    return fprintf(response_stream, "%s %d %zu\n", message_type, result,
	    resident_size) >= 0 && fflush(response_stream) == 0;
}

FacetizeWorkerPolicy::FacetizeWorkerPolicy(size_t requested_workers,
	size_t work_count, size_t available_cpus, size_t total_memory,
	size_t available_memory)
{
    available_cpus = std::max((size_t)1, available_cpus);
    size_t worker_limit = requested_workers ? requested_workers :
	FACETIZE_AUTOMATIC_WORKER_LIMIT;
    worker_limit = std::min(worker_limit, available_cpus);
    if (work_count)
	worker_limit = std::min(worker_limit, work_count);

    if (total_memory && available_memory) {
	memory_headroom = std::max(FACETIZE_MIN_MEMORY_HEADROOM,
		total_memory / FACETIZE_MEMORY_HEADROOM_DIVISOR);
	if (available_memory > memory_headroom) {
	    size_t memory_workers = (available_memory - memory_headroom) /
		FACETIZE_MIN_WORKER_RESERVE;
	    worker_limit = std::min(worker_limit,
		    std::max((size_t)1, memory_workers));
	} else {
	    worker_limit = 1;
	}
    } else {
	worker_limit = 1;
    }

    workers = std::max((size_t)1, worker_limit);
    worker_threads = std::max((size_t)1, available_cpus / workers);
}

size_t
FacetizeWorkerPolicy::worker_count() const
{
    return workers;
}

size_t
FacetizeWorkerPolicy::threads_per_worker() const
{
    return worker_threads;
}

bool
FacetizeWorkerPolicy::can_dispatch(size_t active_workers,
	size_t available_memory, size_t observed_worker_resident) const
{
    if (!active_workers)
	return true;
    if (workers == 1 || !available_memory || !memory_headroom)
	return false;

    size_t worker_reserve = std::max(FACETIZE_MIN_WORKER_RESERVE,
	    observed_worker_resident);
    if (worker_reserve > SIZE_MAX - memory_headroom)
	return false;
    return available_memory > memory_headroom + worker_reserve;
}

void
FacetizeWorkerClient::reset(FILE *stream)
{
    request_stream = stream;
    pending_output.clear();
}

bool
FacetizePrimitiveSettings::empty() const
{
    return methods.empty() && method_options.empty() &&
	cache_directory.empty() && point_limit == 0;
}

bool
FacetizeRegionSettings::empty() const
{
    return !no_empty && !no_fixup && !tolerate_failures;
}

bool
FacetizeWorkerRequest::valid() const
{
    if (!facetize_operation_valid(operation) || input_names.size() != 1 ||
	    input_names.front().empty() ||
	    input_names.size() > FACETIZE_PROTOCOL_LIST_MAX ||
	    primitive.methods.size() > FACETIZE_PROTOCOL_LIST_MAX ||
	    primitive.method_options.size() > FACETIZE_PROTOCOL_LIST_MAX ||
	    primitive.cache_directory.size() > FACETIZE_PROTOCOL_FIELD_MAX ||
	    primitive.point_limit < 0)
	return false;

    for (const std::string &input_name : input_names)
	if (input_name.empty() || input_name.size() > FACETIZE_PROTOCOL_FIELD_MAX)
	    return false;
    for (const std::string &method : primitive.methods)
	if (method.empty() || method.size() > FACETIZE_PROTOCOL_FIELD_MAX)
	    return false;
    for (const std::string &options : primitive.method_options)
	if (options.empty() || options.size() > FACETIZE_PROTOCOL_FIELD_MAX)
	    return false;

    switch (operation) {
	case FacetizeWorkerOperation::TessellatePrimitive:
	    return region.empty();
	case FacetizeWorkerOperation::ValidateCsg:
	    return primitive.empty() && region.empty();
	case FacetizeWorkerOperation::EvaluateRegion:
	    return primitive.empty();
    }
    return false;
}

std::string
FacetizeWorkerRequest::label() const
{
    return input_names.empty() ? "(unknown)" : input_names.front();
}

bool
FacetizeWorkerClient::send_request(const FacetizeWorkerRequest &request)
{
    if (!request_stream || !request.valid())
	return false;

    FacetizePipeWriteGuard write_guard;
    if (fprintf(request_stream, "%s %u %d %zu %zu %zu %d %d %d %d\n",
	    FACETIZE_REQUEST_MAGIC, FACETIZE_PROTOCOL_VERSION,
	    static_cast<int>(request.operation), request.input_names.size(),
	    request.primitive.methods.size(),
	    request.primitive.method_options.size(),
	    request.primitive.point_limit, request.region.no_empty ? 1 : 0,
	    request.region.no_fixup ? 1 : 0,
	    request.region.tolerate_failures ? 1 : 0) < 0 ||
	    !facetize_send_sized_field(request_stream,
		request.primitive.cache_directory))
	return false;
    for (const std::string &input_name : request.input_names)
	if (!facetize_send_sized_field(request_stream, input_name))
	    return false;
    for (const std::string &method : request.primitive.methods)
	if (!facetize_send_sized_field(request_stream, method))
	    return false;
    for (const std::string &options : request.primitive.method_options)
	if (!facetize_send_sized_field(request_stream, options))
	    return false;
    return fflush(request_stream) == 0;
}

bool
FacetizeWorkerClient::send_commit(const char *result_file,
	const char *object_name, size_t payload_size)
{
    if (!request_stream || !result_file || !result_file[0] ||
	    !object_name || !object_name[0])
	return false;

    size_t file_length = strlen(result_file);
    size_t name_length = strlen(object_name);
    if (file_length > FACETIZE_PROTOCOL_FIELD_MAX ||
	    name_length > FACETIZE_PROTOCOL_FIELD_MAX)
	return false;

    FacetizePipeWriteGuard write_guard;
    return fprintf(request_stream, "%s %zu %zu %zu\n",
	    FACETIZE_COMMIT_MAGIC, file_length, name_length, payload_size) >= 0 &&
	facetize_send_field(request_stream, result_file, file_length) &&
	facetize_send_field(request_stream, object_name, name_length) &&
	fflush(request_stream) == 0;
}

bool
FacetizeWorkerClient::send_write_proceed()
{
    if (!request_stream)
	return false;

    FacetizePipeWriteGuard write_guard;
    return fprintf(request_stream, "%s\n", FACETIZE_WRITE_PROCEED_MAGIC) >= 0 &&
	fflush(request_stream) == 0;
}

void
FacetizeWorkerClient::consume_output(const char *data, size_t data_size,
	FacetizeWorkerStatus &status, std::vector<std::string> &diagnostics)
{
    if (!data && data_size)
	return;

    if (data_size)
	pending_output.append(data, data_size);

    size_t line_end = std::string::npos;
    while ((line_end = pending_output.find('\n')) != std::string::npos) {
	std::string line = pending_output.substr(0, line_end);
	pending_output.erase(0, line_end + 1);
	std::istringstream line_stream(line);
	std::string message_type;
	int parsed_result = BRLCAD_ERROR;
	size_t parsed_payload_size = 0;
	size_t parsed_resident_size = 0;
	long parsed_crossings = -1;
	int parsed_tolerated_failures = 0;
	double parsed_surface_area = -1.0;
	double parsed_volume = -1.0;
	line_stream >> message_type;
	if (message_type == FACETIZE_WRITE_READY_MAGIC &&
		(line_stream >> parsed_payload_size >> parsed_resident_size) &&
		(line_stream >> std::ws).eof()) {
	    status.write_ready = true;
	    status.payload_size = parsed_payload_size;
	    status.resident_size = parsed_resident_size;
	    continue;
	}
	if (message_type == FACETIZE_REGION_WRITE_READY_MAGIC &&
		(line_stream >> parsed_payload_size >> parsed_resident_size >>
		 parsed_tolerated_failures) &&
		(line_stream >> std::ws).eof() &&
		parsed_tolerated_failures >= 0) {
	    status.write_ready = true;
	    status.payload_size = parsed_payload_size;
	    status.resident_size = parsed_resident_size;
	    status.tolerated_failures = parsed_tolerated_failures;
	    continue;
	}
	if (message_type == FACETIZE_WRITE_STARTED_MAGIC &&
		(line_stream >> std::ws).eof()) {
	    status.write_started = true;
	    continue;
	}
	if (message_type == FACETIZE_WRITE_DONE_MAGIC &&
		(line_stream >> parsed_result >> parsed_resident_size) &&
		(line_stream >> std::ws).eof()) {
	    status.result = parsed_result;
	    status.resident_size = parsed_resident_size;
	    status.write_done = true;
	    status.result_received = true;
	    continue;
	}
	if (message_type == FACETIZE_RESULT_MAGIC &&
		(line_stream >> parsed_result >> parsed_resident_size) &&
		(line_stream >> std::ws).eof()) {
	    status.result = parsed_result;
	    status.resident_size = parsed_resident_size;
	    status.result_received = true;
	    continue;
	}
	if (message_type == FACETIZE_CSG_RESULT_MAGIC &&
		(line_stream >> parsed_result >> parsed_crossings >>
		 parsed_surface_area >> parsed_volume >> parsed_resident_size) &&
		(line_stream >> std::ws).eof()) {
	    status.result = parsed_result;
	    status.csg_crossings = parsed_crossings;
	    status.csg_surface_area = parsed_surface_area;
	    status.csg_volume = parsed_volume;
	    status.resident_size = parsed_resident_size;
	    status.result_received = true;
	    continue;
	}
	if (!line.empty())
	    diagnostics.push_back(line);
    }
}

FacetizeWorkerServer::FacetizeWorkerServer(FILE *input, FILE *output) :
    request_stream(input), response_stream(output)
{
}

FacetizeWorkerReadResult
FacetizeWorkerServer::receive_request(FacetizeWorkerRequest &request)
{
    request = FacetizeWorkerRequest();
    if (!request_stream)
	return FacetizeWorkerReadResult::Error;

    char header[FACETIZE_PROTOCOL_HEADER_SIZE];
    FacetizeWorkerReadResult read_result = facetize_read_header(request_stream,
	    header, sizeof(header));
    if (read_result != FacetizeWorkerReadResult::Request)
	return read_result;

    unsigned int protocol_version = 0;
    int operation_value = 0;
    size_t input_count = 0;
    size_t method_count = 0;
    size_t method_option_count = 0;
    int point_limit = 0;
    int no_empty = 0;
    int no_fixup = 0;
    int tolerate_failures = 0;
    std::string message_type;
    std::istringstream header_stream(header);
    if (!(header_stream >> message_type) ||
	    message_type != FACETIZE_REQUEST_MAGIC ||
	    !(header_stream >> protocol_version) ||
	    protocol_version != FACETIZE_PROTOCOL_VERSION ||
	    !(header_stream >> operation_value >> input_count >> method_count >>
		method_option_count >> point_limit >> no_empty >> no_fixup >>
		tolerate_failures) ||
	    !(header_stream >> std::ws).eof() || !input_count ||
	    input_count > FACETIZE_PROTOCOL_LIST_MAX ||
	    method_count > FACETIZE_PROTOCOL_LIST_MAX ||
	    method_option_count > FACETIZE_PROTOCOL_LIST_MAX || point_limit < 0 ||
	    (no_empty != 0 && no_empty != 1) ||
	    (no_fixup != 0 && no_fixup != 1) ||
	    (tolerate_failures != 0 && tolerate_failures != 1))
	return FacetizeWorkerReadResult::Error;


    request.operation = static_cast<FacetizeWorkerOperation>(operation_value);
    request.primitive.point_limit = point_limit;
    request.region.no_empty = no_empty != 0;
    request.region.no_fixup = no_fixup != 0;
    request.region.tolerate_failures = tolerate_failures != 0;
    if (!facetize_read_sized_field(request_stream,
	    request.primitive.cache_directory))
	return FacetizeWorkerReadResult::Error;

    request.input_names.reserve(input_count);
    for (size_t i = 0; i < input_count; i++) {
	std::string input_name;
	if (!facetize_read_sized_field(request_stream, input_name)) {
	    request = FacetizeWorkerRequest();
	    return FacetizeWorkerReadResult::Error;
	}
	request.input_names.push_back(input_name);
    }
    request.primitive.methods.reserve(method_count);
    for (size_t i = 0; i < method_count; i++) {
	std::string method;
	if (!facetize_read_sized_field(request_stream, method)) {
	    request = FacetizeWorkerRequest();
	    return FacetizeWorkerReadResult::Error;
	}
	request.primitive.methods.push_back(method);
    }
    request.primitive.method_options.reserve(method_option_count);
    for (size_t i = 0; i < method_option_count; i++) {
	std::string options;
	if (!facetize_read_sized_field(request_stream, options)) {
	    request = FacetizeWorkerRequest();
	    return FacetizeWorkerReadResult::Error;
	}
	request.primitive.method_options.push_back(options);
    }

    if (!request.valid()) {
	request = FacetizeWorkerRequest();
	return FacetizeWorkerReadResult::Error;
    }

    return FacetizeWorkerReadResult::Request;
}

FacetizeWorkerReadResult
FacetizeWorkerServer::receive_commit(FacetizeCommitRequest &request)
{
    request = FacetizeCommitRequest();
    if (!request_stream)
	return FacetizeWorkerReadResult::Error;

    char header[FACETIZE_PROTOCOL_HEADER_SIZE];
    FacetizeWorkerReadResult read_result = facetize_read_header(request_stream,
	    header, sizeof(header));
    if (read_result != FacetizeWorkerReadResult::Request)
	return read_result;

    size_t file_length = 0;
    size_t name_length = 0;
    std::string message_type;
    std::istringstream header_stream(header);
    if (!(header_stream >> message_type) ||
	    message_type != FACETIZE_COMMIT_MAGIC ||
	    !(header_stream >> file_length >> name_length >> request.payload_size) ||
	    !(header_stream >> std::ws).eof() || !file_length || !name_length ||
	    file_length > FACETIZE_PROTOCOL_FIELD_MAX ||
	    name_length > FACETIZE_PROTOCOL_FIELD_MAX)
	return FacetizeWorkerReadResult::Error;

    if (!facetize_read_field(request_stream, file_length,
	    request.result_file) ||
	    !facetize_read_field(request_stream, name_length,
		request.object_name)) {
	request = FacetizeCommitRequest();
	return FacetizeWorkerReadResult::Error;
    }

    return FacetizeWorkerReadResult::Request;
}

bool
FacetizeWorkerServer::send_write_ready(size_t payload_size,
	size_t resident_size)
{
    if (!response_stream)
	return false;

    return fprintf(response_stream, "%s %zu %zu\n",
	    FACETIZE_WRITE_READY_MAGIC, payload_size, resident_size) >= 0 &&
	fflush(response_stream) == 0;
}

bool
FacetizeWorkerServer::send_region_write_ready(size_t payload_size,
	size_t resident_size, int tolerated_failures)
{
    if (!response_stream || tolerated_failures < 0)
	return false;

    return fprintf(response_stream, "%s %zu %zu %d\n",
	    FACETIZE_REGION_WRITE_READY_MAGIC, payload_size, resident_size,
	    tolerated_failures) >= 0 && fflush(response_stream) == 0;
}

bool
FacetizeWorkerServer::receive_write_proceed()
{
    if (!request_stream)
	return false;

    char response[FACETIZE_PROTOCOL_HEADER_SIZE];
    if (!bu_fgets(response, sizeof(response), request_stream))
	return false;

    std::string expected_response = std::string(FACETIZE_WRITE_PROCEED_MAGIC) + "\n";
    return expected_response == response;
}

bool
FacetizeWorkerServer::send_write_started()
{
    if (!response_stream)
	return false;

    return fprintf(response_stream, "%s\n", FACETIZE_WRITE_STARTED_MAGIC) >= 0 &&
	fflush(response_stream) == 0;
}

bool
FacetizeWorkerServer::send_tessellation_result(int result,
	size_t resident_size)
{
    return facetize_send_result(response_stream, FACETIZE_RESULT_MAGIC, result,
	    resident_size);
}

bool
FacetizeWorkerServer::send_write_result(int result, size_t resident_size)
{
    return facetize_send_result(response_stream, FACETIZE_WRITE_DONE_MAGIC,
	    result, resident_size);
}

bool
FacetizeWorkerServer::send_csg_result(int result, long crossings,
	double surface_area, double volume, size_t resident_size)
{
    if (!response_stream)
	return false;

    return fprintf(response_stream, "%s %d %ld %.17g %.17g %zu\n",
	    FACETIZE_CSG_RESULT_MAGIC, result, crossings, surface_area, volume,
	    resident_size) >= 0 && fflush(response_stream) == 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
