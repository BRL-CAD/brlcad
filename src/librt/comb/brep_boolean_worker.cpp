/*              B R E P _ B O O L E A N _ W O R K E R . C P P
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
/** @file brep_boolean_worker.cpp
 *
 * Persistent crash-isolated workers for CSG-to-B-Rep conversion and direct
 * B-Rep Boolean evaluation.  Requests name objects in one immutable database;
 * large OpenNURBS objects are returned through private staging files instead
 * of being allowed to block a control pipe.
 */

#include "common.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#if defined(HAVE_PTHREAD_H) && defined(HAVE_SIGNAL_H)
#  include <pthread.h>
#  include <signal.h>
#endif

#include "bu/app.h"
#include "bu/datetime.h"
#include "bu/env.h"
#include "bu/exit.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/snooze.h"
#include "bu/str.h"
#include "raytrace.h"
#include "rt/comb.h"
#include "rt/geom.h"

#include "../librt_private.h"


namespace {

const char REQUEST_MAGIC[] = "BREP_BOOLEAN_REQUEST";
const char RESULT_MAGIC[] = "BREP_BOOLEAN_RESULT";
const unsigned int PROTOCOL_VERSION = 5;
const size_t PROTOCOL_HEADER_MAX = 4096;
const size_t PROTOCOL_FIELD_MAX = 1024 * 1024;
const size_t PROTOCOL_OUTPUT_MAX = 1024 * 1024;
const size_t DIAGNOSTIC_MAX = 256 * 1024;
const size_t DEFAULT_MAX_JOBS = 32;
const size_t DEFAULT_MAX_RESIDENT = (size_t)2 << 30;
const unsigned int DEFAULT_TIMEOUT_MS = 300000;
const unsigned int PROCESS_STOP_TIMEOUT_MS = 5000;
const unsigned int PROCESS_POLL_USEC = 1000;
const size_t WORKER_COUNT_MAX = 64;

enum class RequestKind {
    Convert = 1,
    Boolean = 2
};

struct PendingRequest {
    uint64_t id = 0;
    RequestKind kind = RequestKind::Convert;
    std::string left_name;
    std::string right_name;
    op_type operation = BOOLEAN_UNION;
    struct bn_tol tolerance = BN_TOL_INIT_TOL;
    std::string result_path;
    int64_t start_time = 0;
};

struct CompletedRequest {
    uint64_t id = 0;
    int boolean_status = BREP_BOOLEAN_INTERNAL_ERROR;
    enum rt_brep_worker_transport_status transport_status =
	RT_BREP_WORKER_TRANSPORT_OK;
    struct rt_brep_conversion_info conversion_info =
	RT_BREP_CONVERSION_INFO_INIT;
    ON_Brep *brep = NULL;
    std::string diagnostic;
    int worker_pid = 0;
    size_t worker_job_count = 0;
    size_t worker_resident_bytes = 0;
};

struct WorkerResponse {
    uint64_t id = 0;
    int boolean_status = BREP_BOOLEAN_INTERNAL_ERROR;
    int fidelity = RT_BREP_CONVERSION_FIDELITY_UNKNOWN;
    double declared_relative_tolerance = 0.0;
    unsigned int approximation_flags = 0;
    unsigned int regularization_attempts = 0;
    unsigned int whole_tree_regularization_attempts = 0;
    unsigned int repaired_arb_count = 0;
    int initial_status = BREP_BOOLEAN_INTERNAL_ERROR;
    struct rt_brep_failure_info failure = RT_BREP_FAILURE_INFO_INIT;
    size_t resident_bytes = 0;
    size_t result_bytes = 0;
    int worker_pid = 0;
    size_t worker_job_count = 0;
};

struct WorkerState {
    struct bu_process *process = NULL;
    FILE *input = NULL;
    std::unique_ptr<PendingRequest> active;
    std::string pending_output;
    std::string diagnostic;
    bool protocol_error = false;
    bool response_received = false;
    WorkerResponse response;
    size_t completed_jobs = 0;
    size_t resident_bytes = 0;
};

static std::atomic<uint64_t> pool_serial(0);


class PipeWriteGuard
{
public:
    PipeWriteGuard()
    {
#if defined(HAVE_PTHREAD_H) && defined(HAVE_SIGNAL_H) && defined(SIGPIPE)
	sigemptyset(&blocked_signals);
	sigaddset(&blocked_signals, SIGPIPE);
	active = pthread_sigmask(SIG_BLOCK, &blocked_signals,
		&original_mask) == 0;
	sigset_t pending_signals;
	if (active && sigpending(&pending_signals) == 0)
	    had_pending_sigpipe = sigismember(&pending_signals, SIGPIPE) == 1;
#endif
    }

    ~PipeWriteGuard()
    {
#if defined(HAVE_PTHREAD_H) && defined(HAVE_SIGNAL_H) && defined(SIGPIPE)
	if (!active)
	    return;
	int saved_errno = errno;
	sigset_t pending_signals;
	const bool generated_sigpipe = !had_pending_sigpipe &&
	    sigpending(&pending_signals) == 0 &&
	    sigismember(&pending_signals, SIGPIPE) == 1;
	if (generated_sigpipe) {
	    int signal_number = 0;
	    while (sigwait(&blocked_signals, &signal_number) == EINTR)
		;
	}
	(void)pthread_sigmask(SIG_SETMASK, &original_mask, NULL);
	errno = saved_errno;
#endif
    }

private:
#if defined(HAVE_PTHREAD_H) && defined(HAVE_SIGNAL_H) && defined(SIGPIPE)
    sigset_t blocked_signals;
    sigset_t original_mask;
    bool active = false;
    bool had_pending_sigpipe = false;
#endif
};


static bool
valid_boolean_status(int status)
{
    switch (status) {
	case BREP_BOOLEAN_FAILURE:
	case BREP_BOOLEAN_INVALID_INPUT:
	case BREP_BOOLEAN_INDETERMINATE:
	case BREP_BOOLEAN_BUDGET_EXCEEDED:
	case BREP_BOOLEAN_UNSUPPORTED:
	case BREP_BOOLEAN_INTERNAL_ERROR:
	case BREP_BOOLEAN_SUCCESS:
	case BREP_BOOLEAN_EMPTY:
	    return true;
	default:
	    return false;
    }
}


static bool
valid_operation(op_type operation)
{
    return operation == BOOLEAN_UNION || operation == BOOLEAN_INTERSECT ||
	operation == BOOLEAN_DIFF || operation == BOOLEAN_XOR;
}


static bool
valid_failure_stage(int stage)
{
    return stage >= RT_BREP_FAILURE_STAGE_NONE &&
	stage <= RT_BREP_FAILURE_STAGE_OUTPUT_VALIDATION;
}


static bool
valid_failure_reason(int reason)
{
    return reason >= RT_BREP_FAILURE_REASON_NONE &&
	reason <= RT_BREP_FAILURE_REASON_INTERNAL_ERROR;
}


static bool
valid_boolean_failure_stage(int stage)
{
    return stage >= BREP_BOOLEAN_FAILURE_STAGE_NONE &&
	stage <= BREP_BOOLEAN_FAILURE_STAGE_INTERNAL;
}


static bool
valid_boolean_failure_reason(int reason)
{
    return reason >= BREP_BOOLEAN_FAILURE_REASON_NONE &&
	reason <= BREP_BOOLEAN_FAILURE_REASON_SSI_EDGE_PARTITION;
}


static void
initialize_conversion_info(struct rt_brep_conversion_info *info)
{
    if (!info)
	return;
    const struct rt_brep_conversion_info initialized =
	RT_BREP_CONVERSION_INFO_INIT;
    *info = initialized;
}


static void
record_worker_failure(struct rt_brep_conversion_info *info,
    enum rt_brep_failure_stage stage, enum rt_brep_failure_reason reason,
    int status, const char *object_name, int operation,
    const ON_Brep *left = NULL, const ON_Brep *right = NULL,
    const ON_Brep *output = NULL,
    const struct brep_boolean_failure_info *boolean_failure = NULL)
{
    if (!info || info->failure.stage != RT_BREP_FAILURE_STAGE_NONE)
	return;
    info->failure.stage = stage;
    info->failure.reason = reason;
    info->failure.status = status;
    info->failure.operation = operation;
    info->failure.left_faces = left ? left->m_F.Count() : 0;
    info->failure.left_edges = left ? left->m_E.Count() : 0;
    info->failure.right_faces = right ? right->m_F.Count() : 0;
    info->failure.right_edges = right ? right->m_E.Count() : 0;
    info->failure.output_faces = output ? output->m_F.Count() : 0;
    info->failure.output_edges = output ? output->m_E.Count() : 0;
    if (output != NULL) {
	for (int edge_index = 0; edge_index < output->m_E.Count();
	     ++edge_index)
	{
	    if (output->m_E[edge_index].TrimCount() != 2)
		info->failure.output_naked_edges++;
	}
    }
    if (boolean_failure != NULL)
	info->failure.boolean_failure = *boolean_failure;
    bu_strlcpy(info->failure.tree_path, "r",
	sizeof(info->failure.tree_path));
    if (object_name != NULL) {
	bu_strlcpy(info->failure.object_name, object_name,
	    sizeof(info->failure.object_name));
    }
}


static void
append_diagnostic(std::string &destination, const char *data, size_t size)
{
    if (!data || !size || destination.size() >= DIAGNOSTIC_MAX)
	return;
    const size_t available = DIAGNOSTIC_MAX - destination.size();
    destination.append(data, std::min(size, available));
}


static void
append_diagnostic_line(std::string &destination, const std::string &line)
{
    append_diagnostic(destination, line.data(), line.size());
    append_diagnostic(destination, "\n", 1);
}


static bool
send_bytes(FILE *stream, const char *bytes, size_t count)
{
    return stream && (!count || (bytes && fwrite(bytes, 1, count, stream) == count));
}


static bool
send_sized_field(FILE *stream, const std::string &field)
{
    if (!stream || field.size() > PROTOCOL_FIELD_MAX)
	return false;
    std::ostringstream header;
    header.imbue(std::locale::classic());
    header << field.size() << '\n';
    const std::string header_text = header.str();
    return send_bytes(stream, header_text.data(), header_text.size()) &&
	send_bytes(stream, field.data(), field.size()) &&
	fputc('\n', stream) != EOF;
}


static bool
send_request(FILE *stream, const PendingRequest &request)
{
    if (!stream || !request.id || request.left_name.empty() ||
	request.left_name.size() > PROTOCOL_FIELD_MAX ||
	request.right_name.size() > PROTOCOL_FIELD_MAX ||
	request.result_path.empty() ||
	request.result_path.size() > PROTOCOL_FIELD_MAX ||
	!valid_operation(request.operation))
    {
	return false;
    }

    std::ostringstream header;
    header.imbue(std::locale::classic());
    header << std::setprecision(17) << REQUEST_MAGIC << ' ' <<
	PROTOCOL_VERSION << ' ' << request.id << ' ' <<
	static_cast<int>(request.kind) << ' ' <<
	static_cast<int>(request.operation) << ' ' << request.tolerance.dist <<
	' ' << request.tolerance.perp << ' ' << request.tolerance.para << '\n';
    const std::string header_text = header.str();

    PipeWriteGuard guard;
    return send_bytes(stream, header_text.data(), header_text.size()) &&
	send_sized_field(stream, request.result_path) &&
	send_sized_field(stream, request.left_name) &&
	send_sized_field(stream, request.right_name) && fflush(stream) == 0;
}


enum class ReadResult {
    Error = -1,
    End = 0,
    Request = 1
};


static ReadResult
read_line(FILE *stream, char *line, size_t line_size)
{
    if (!stream || !line || line_size < 2)
	return ReadResult::Error;
    if (!bu_fgets(line, line_size, stream))
	return feof(stream) ? ReadResult::End : ReadResult::Error;
    const size_t length = strlen(line);
    return length && line[length - 1] == '\n' ? ReadResult::Request :
	ReadResult::Error;
}


static bool
read_sized_field(FILE *stream, std::string &field)
{
    field.clear();
    char line[PROTOCOL_HEADER_MAX];
    if (read_line(stream, line, sizeof(line)) != ReadResult::Request)
	return false;

    size_t size = 0;
    std::istringstream parser(line);
    parser.imbue(std::locale::classic());
    if (!(parser >> size) || !(parser >> std::ws).eof() ||
	size > PROTOCOL_FIELD_MAX)
    {
	return false;
    }

    field.assign(size, '\0');
    if (size && fread(&field[0], 1, size, stream) != size) {
	field.clear();
	return false;
    }
    if (fgetc(stream) != '\n') {
	field.clear();
	return false;
    }
    return true;
}


static ReadResult
receive_request(FILE *stream, PendingRequest &request)
{
    char line[PROTOCOL_HEADER_MAX];
    const ReadResult line_result = read_line(stream, line, sizeof(line));
    if (line_result != ReadResult::Request)
	return line_result;

    std::string magic;
    unsigned int version = 0;
    int kind = 0;
    int operation = 0;
    double distance = 0.0;
    double perpendicular = 0.0;
    double parallel = 0.0;
    std::istringstream parser(line);
    parser.imbue(std::locale::classic());
    if (!(parser >> magic >> version >> request.id >> kind >> operation >>
	    distance >> perpendicular >> parallel) ||
	!(parser >> std::ws).eof() || magic != REQUEST_MAGIC ||
	version != PROTOCOL_VERSION || !request.id ||
	(kind != static_cast<int>(RequestKind::Convert) &&
	 kind != static_cast<int>(RequestKind::Boolean)) ||
	!valid_operation(static_cast<op_type>(operation)) ||
	!std::isfinite(distance) || distance < 0.0 ||
	!std::isfinite(perpendicular) || perpendicular < 0.0 ||
	!std::isfinite(parallel))
    {
	return ReadResult::Error;
    }

    request.kind = static_cast<RequestKind>(kind);
    request.operation = static_cast<op_type>(operation);
    request.tolerance.magic = BN_TOL_MAGIC;
    request.tolerance.dist = distance;
    request.tolerance.dist_sq = distance * distance;
    request.tolerance.perp = perpendicular;
    request.tolerance.para = parallel;
    if (!read_sized_field(stream, request.result_path) ||
	!read_sized_field(stream, request.left_name) ||
	!read_sized_field(stream, request.right_name) ||
	request.result_path.empty() || request.left_name.empty() ||
	(request.kind == RequestKind::Convert && !request.right_name.empty()) ||
	(request.kind == RequestKind::Boolean && request.right_name.empty()))
    {
	return ReadResult::Error;
    }
    return ReadResult::Request;
}


static bool
send_response(FILE *stream, const WorkerResponse &response)
{
    if (!stream)
	return false;
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(17) << RESULT_MAGIC << ' ' <<
	PROTOCOL_VERSION << ' ' << response.id << ' ' <<
	response.boolean_status << ' ' << response.fidelity << ' ' <<
	response.declared_relative_tolerance << ' ' <<
	response.approximation_flags << ' ' <<
	response.regularization_attempts << ' ' <<
	response.whole_tree_regularization_attempts << ' ' <<
	response.repaired_arb_count << ' ' << response.initial_status << ' ' <<
	static_cast<int>(response.failure.stage) << ' ' <<
	static_cast<int>(response.failure.reason) << ' ' <<
	response.failure.status << ' ' << response.failure.operation << ' ' <<
	response.failure.regularized << ' ' <<
	response.failure.regularization_scale << ' ' <<
	response.failure.left_faces << ' ' << response.failure.left_edges << ' ' <<
	response.failure.right_faces << ' ' << response.failure.right_edges << ' ' <<
	response.failure.output_faces << ' ' << response.failure.output_edges << ' ' <<
	response.failure.output_naked_edges << ' ' <<
	static_cast<int>(response.failure.boolean_failure.stage) << ' ' <<
	static_cast<int>(response.failure.boolean_failure.reason) << ' ' <<
	response.failure.boolean_failure.face_index << ' ' <<
	response.failure.boolean_failure.peer_face_index << ' ' <<
	response.failure.boolean_failure.edge_index << ' ' <<
	response.failure.boolean_failure.peer_edge_index << ' ' <<
	response.failure.boolean_failure.intersection_candidates << ' ' <<
	response.failure.boolean_failure.intersection_events << ' ' <<
	response.failure.boolean_failure.projection_failures << ' ' <<
	response.failure.boolean_failure.output_naked_edges << ' ' <<
	response.failure.boolean_failure.output_nonmanifold_edges << ' ' <<
	response.failure.boolean_failure.ambiguous_edge_matches << ' ' <<
	response.failure.boolean_failure.tolerance << ' ' <<
	std::quoted(std::string(response.failure.tree_path)) << ' ' <<
	std::quoted(std::string(response.failure.object_name)) << ' ' <<
	std::quoted(std::string(response.failure.snapshot_name)) << ' ' <<
	response.resident_bytes << ' ' << response.result_bytes << ' ' <<
	response.worker_pid << ' ' << response.worker_job_count << '\n';
    const std::string text = output.str();
    return send_bytes(stream, text.data(), text.size()) && fflush(stream) == 0;
}


static bool
parse_response(const std::string &line, WorkerResponse &response)
{
    std::string magic;
    std::string tree_path;
    std::string object_name;
    std::string snapshot_name;
    unsigned int version = 0;
    int failure_stage = RT_BREP_FAILURE_STAGE_NONE;
    int failure_reason = RT_BREP_FAILURE_REASON_NONE;
    int boolean_failure_stage = BREP_BOOLEAN_FAILURE_STAGE_NONE;
    int boolean_failure_reason = BREP_BOOLEAN_FAILURE_REASON_NONE;
    std::istringstream parser(line);
    parser.imbue(std::locale::classic());
    if (!(parser >> magic >> version >> response.id >>
	    response.boolean_status >> response.fidelity >>
	    response.declared_relative_tolerance >> response.approximation_flags >>
	    response.regularization_attempts >>
	    response.whole_tree_regularization_attempts >>
	    response.repaired_arb_count >> response.initial_status >>
	    failure_stage >> failure_reason >> response.failure.status >>
	    response.failure.operation >> response.failure.regularized >>
	    response.failure.regularization_scale >>
	    response.failure.left_faces >> response.failure.left_edges >>
	    response.failure.right_faces >> response.failure.right_edges >>
	    response.failure.output_faces >> response.failure.output_edges >>
	    response.failure.output_naked_edges >> boolean_failure_stage >>
	    boolean_failure_reason >> response.failure.boolean_failure.face_index >>
	    response.failure.boolean_failure.peer_face_index >>
	    response.failure.boolean_failure.edge_index >>
	    response.failure.boolean_failure.peer_edge_index >>
	    response.failure.boolean_failure.intersection_candidates >>
	    response.failure.boolean_failure.intersection_events >>
	    response.failure.boolean_failure.projection_failures >>
	    response.failure.boolean_failure.output_naked_edges >>
	    response.failure.boolean_failure.output_nonmanifold_edges >>
	    response.failure.boolean_failure.ambiguous_edge_matches >>
	    response.failure.boolean_failure.tolerance >> std::quoted(tree_path) >>
	    std::quoted(object_name) >> std::quoted(snapshot_name) >>
	    response.resident_bytes >> response.result_bytes >> response.worker_pid >>
	    response.worker_job_count) || !(parser >> std::ws).eof() ||
	magic != RESULT_MAGIC || version != PROTOCOL_VERSION || !response.id ||
	!valid_boolean_status(response.boolean_status) ||
	(response.fidelity < RT_BREP_CONVERSION_FIDELITY_UNKNOWN ||
	 response.fidelity > RT_BREP_CONVERSION_FIDELITY_APPROXIMATE) ||
	!std::isfinite(response.declared_relative_tolerance) ||
	response.declared_relative_tolerance < 0.0 ||
	(response.approximation_flags & ~0x0fu) ||
	!valid_boolean_status(response.initial_status) ||
	!valid_failure_stage(failure_stage) ||
	!valid_failure_reason(failure_reason) ||
	!valid_boolean_failure_stage(boolean_failure_stage) ||
	!valid_boolean_failure_reason(boolean_failure_reason) ||
	(response.failure.regularized != 0 &&
	 response.failure.regularized != 1) ||
	!std::isfinite(response.failure.regularization_scale) ||
	response.failure.regularization_scale < 0.0 ||
	response.failure.boolean_failure.face_index < -1 ||
	response.failure.boolean_failure.peer_face_index < -1 ||
	response.failure.boolean_failure.edge_index < -1 ||
	response.failure.boolean_failure.peer_edge_index < -1 ||
	!std::isfinite(response.failure.boolean_failure.tolerance) ||
	response.failure.boolean_failure.tolerance < 0.0 ||
	tree_path.size() >= sizeof(response.failure.tree_path) ||
	object_name.size() >= sizeof(response.failure.object_name) ||
	snapshot_name.size() >= sizeof(response.failure.snapshot_name) ||
	response.worker_pid <= 0 ||
	!response.worker_job_count)
    {
	return false;
    }
    response.failure.stage =
	static_cast<enum rt_brep_failure_stage>(failure_stage);
    response.failure.reason =
	static_cast<enum rt_brep_failure_reason>(failure_reason);
    response.failure.boolean_failure.stage =
	static_cast<enum brep_boolean_failure_stage>(boolean_failure_stage);
    response.failure.boolean_failure.reason =
	static_cast<enum brep_boolean_failure_reason>(boolean_failure_reason);
    bu_strlcpy(response.failure.tree_path, tree_path.c_str(),
	sizeof(response.failure.tree_path));
    bu_strlcpy(response.failure.object_name, object_name.c_str(),
	sizeof(response.failure.object_name));
    bu_strlcpy(response.failure.snapshot_name, snapshot_name.c_str(),
	sizeof(response.failure.snapshot_name));
    const struct brep_boolean_failure_info &boolean_failure =
	response.failure.boolean_failure;
    const bool empty_boolean_failure =
	boolean_failure.stage == BREP_BOOLEAN_FAILURE_STAGE_NONE &&
	boolean_failure.reason == BREP_BOOLEAN_FAILURE_REASON_NONE &&
	boolean_failure.face_index == -1 &&
	boolean_failure.peer_face_index == -1 &&
	boolean_failure.edge_index == -1 &&
	boolean_failure.peer_edge_index == -1 &&
	boolean_failure.intersection_candidates == 0 &&
	boolean_failure.intersection_events == 0 &&
	boolean_failure.projection_failures == 0 &&
	boolean_failure.output_naked_edges == 0 &&
	boolean_failure.output_nonmanifold_edges == 0 &&
	boolean_failure.ambiguous_edge_matches == 0 &&
	ZERO(boolean_failure.tolerance);
    if (response.failure.stage == RT_BREP_FAILURE_STAGE_NONE &&
	(response.failure.reason != RT_BREP_FAILURE_REASON_NONE ||
	 response.failure.status != BREP_BOOLEAN_SUCCESS ||
	 response.failure.operation != -1 || response.failure.regularized != 0 ||
	 !ZERO(response.failure.regularization_scale) ||
	 response.failure.left_faces != 0 || response.failure.left_edges != 0 ||
	 response.failure.right_faces != 0 || response.failure.right_edges != 0 ||
	 response.failure.output_faces != 0 ||
	 response.failure.output_edges != 0 ||
	 response.failure.output_naked_edges != 0 || !tree_path.empty() ||
	 !object_name.empty() || !snapshot_name.empty() ||
	 !empty_boolean_failure))
    {
	return false;
    }
    if (response.failure.stage != RT_BREP_FAILURE_STAGE_NONE &&
	(response.failure.reason == RT_BREP_FAILURE_REASON_NONE ||
	 !valid_boolean_status(response.failure.status) ||
	 response.failure.status >= BREP_BOOLEAN_SUCCESS ||
	 tree_path.empty() ||
	 (response.failure.regularized == 0 &&
	  !ZERO(response.failure.regularization_scale)) ||
	 (response.failure.operation != -1 &&
	  !valid_operation(static_cast<op_type>(response.failure.operation)))))
    {
	return false;
    }
    if ((boolean_failure.stage == BREP_BOOLEAN_FAILURE_STAGE_NONE) !=
	(boolean_failure.reason == BREP_BOOLEAN_FAILURE_REASON_NONE) ||
	(boolean_failure.stage == BREP_BOOLEAN_FAILURE_STAGE_NONE &&
	 !empty_boolean_failure) ||
	(boolean_failure.stage != BREP_BOOLEAN_FAILURE_STAGE_NONE &&
	 response.failure.stage != RT_BREP_FAILURE_STAGE_BOOLEAN_EVALUATION))
    {
	return false;
    }
    if (response.failure.left_faces >
	std::numeric_limits<size_t>::max() - response.failure.right_faces)
    {
	return false;
    }
    const size_t operand_faces = response.failure.left_faces +
	response.failure.right_faces;
    if ((boolean_failure.face_index >= 0 &&
	 static_cast<size_t>(boolean_failure.face_index) >= operand_faces) ||
	(boolean_failure.peer_face_index >= 0 &&
	 static_cast<size_t>(boolean_failure.peer_face_index) >= operand_faces) ||
	response.failure.output_naked_edges > response.failure.output_edges)
    {
	return false;
    }
    if (response.boolean_status == BREP_BOOLEAN_SUCCESS) {
	if (!response.result_bytes)
	    return false;
    } else if (response.result_bytes) {
	return false;
    }
    return true;
}


static size_t
resident_size()
{
    const ssize_t resident = bu_mem(BU_MEM_PROCESS_RESIDENT, NULL);
    return resident > 0 ? static_cast<size_t>(resident) : 0;
}


static void
delete_conversion(ON_Brep *converted, ON_Brep *owner)
{
    if (converted != owner)
	delete converted;
    delete owner;
}


static int
validate_result_brep(const ON_Brep *brep)
{
    if (!brep || !brep->m_F.Count())
	return BREP_BOOLEAN_INDETERMINATE;
    ON_wString messages;
    ON_TextLog log(messages);
    if (!brep->IsValid(&log) || !brep->IsSolid())
	return BREP_BOOLEAN_INDETERMINATE;
    return BREP_BOOLEAN_SUCCESS;
}


static int
convert_object(struct db_i *dbip, const PendingRequest &request,
    ON_Brep **result, struct rt_brep_conversion_info *info)
{
    if (!dbip || !result || !info)
	return BREP_BOOLEAN_INVALID_INPUT;
    *result = NULL;
    initialize_conversion_info(info);

    /* Route every named object through the combination-tree converter.  A
     * one-leaf wrapper gives primitive roots the same validation, bounded ARB
     * repair, transform handling, and fidelity reporting as leaves inside a
     * real combination. */
    struct rt_db_internal wrapper;
    RT_DB_INTERNAL_INIT(&wrapper);
    wrapper.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    wrapper.idb_type = ID_COMBINATION;
    wrapper.idb_meth = &OBJ[ID_COMBINATION];
    struct rt_comb_internal *combination;
    BU_ALLOC(combination, struct rt_comb_internal);
    RT_COMB_INTERNAL_INIT(combination);
    union tree *leaf;
    BU_GET(leaf, union tree);
    RT_TREE_INIT(leaf);
    leaf->tr_l.tl_op = OP_DB_LEAF;
    leaf->tr_l.tl_name = bu_strdup(request.left_name.c_str());
    leaf->tr_l.tl_mat = NULL;
    combination->tree = leaf;
    wrapper.idb_ptr = combination;

    ON_Brep *owner = ON_Brep::New();
    ON_Brep *converted = owner;
    const int status = rt_comb_brep_status_info(&converted, &wrapper,
	&request.tolerance, dbip, info);
    rt_db_free_internal(&wrapper);

    if (status == BREP_BOOLEAN_SUCCESS && converted) {
	if (converted != owner)
	    delete owner;
	*result = converted;
	return status;
    }
    delete_conversion(converted, owner);
    return status;
}


static bool
load_named_brep(struct db_i *dbip, const std::string &name, ON_Brep **brep)
{
    if (!dbip || !brep)
	return false;
    *brep = NULL;
    struct directory *entry = db_lookup(dbip, name.c_str(), LOOKUP_QUIET);
    if (!entry)
	return false;
    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    if (rt_db_get_internal(&internal, entry, dbip, bn_mat_identity) < 0)
	return false;
    bool success = false;
    if (internal.idb_type == ID_BREP) {
	const struct rt_brep_internal *brep_internal =
	    static_cast<const struct rt_brep_internal *>(internal.idb_ptr);
	if (brep_internal && brep_internal->magic == RT_BREP_INTERNAL_MAGIC &&
	    brep_internal->brep)
	{
	    *brep = ON_Brep::New(*brep_internal->brep);
	    success = *brep != NULL;
	}
    }
    rt_db_free_internal(&internal);
    return success;
}


static int
evaluate_boolean(struct db_i *dbip, const PendingRequest &request,
    ON_Brep **result, struct rt_brep_conversion_info *info)
{
    if (!dbip || !result || !info || !valid_operation(request.operation))
	return BREP_BOOLEAN_INVALID_INPUT;
    *result = NULL;
    initialize_conversion_info(info);

    ON_Brep *left = NULL;
    ON_Brep *right = NULL;
    if (!load_named_brep(dbip, request.left_name, &left)) {
	record_worker_failure(info, RT_BREP_FAILURE_STAGE_LEAF_LOOKUP,
	    db_lookup(dbip, request.left_name.c_str(), LOOKUP_QUIET) ==
	    RT_DIR_NULL ? RT_BREP_FAILURE_REASON_MISSING_OBJECT :
	    RT_BREP_FAILURE_REASON_INVALID_OBJECT,
	    BREP_BOOLEAN_INVALID_INPUT, request.left_name.c_str(),
	    request.operation);
	return BREP_BOOLEAN_INVALID_INPUT;
    }
    if (!load_named_brep(dbip, request.right_name, &right)) {
	record_worker_failure(info, RT_BREP_FAILURE_STAGE_LEAF_LOOKUP,
	    db_lookup(dbip, request.right_name.c_str(), LOOKUP_QUIET) ==
	    RT_DIR_NULL ? RT_BREP_FAILURE_REASON_MISSING_OBJECT :
	    RT_BREP_FAILURE_REASON_INVALID_OBJECT,
	    BREP_BOOLEAN_INVALID_INPUT, request.right_name.c_str(),
	    request.operation, left);
	delete left;
	return BREP_BOOLEAN_INVALID_INPUT;
    }

    ON_Brep *boolean_result = ON_Brep::New();
    struct brep_boolean_failure_info boolean_failure =
	BREP_BOOLEAN_FAILURE_INFO_INIT;
    int status = ON_Boolean(boolean_result, left, right, request.operation,
	&boolean_failure);
    info->initial_status = status;
    if (status == BREP_BOOLEAN_EMPTY) {
	info->fidelity = RT_BREP_CONVERSION_FIDELITY_EXACT;
	delete left;
	delete right;
	delete boolean_result;
	return status;
    }
    if (status == BREP_BOOLEAN_SUCCESS) {
	status = validate_result_brep(boolean_result);
	if (status == BREP_BOOLEAN_SUCCESS) {
	    info->fidelity = RT_BREP_CONVERSION_FIDELITY_EXACT;
	    delete left;
	    delete right;
	    *result = boolean_result;
	    return status;
	}
	record_worker_failure(info, RT_BREP_FAILURE_STAGE_OUTPUT_VALIDATION,
	    boolean_result->IsValid() ?
	    RT_BREP_FAILURE_REASON_OUTPUT_NOT_SOLID :
	    RT_BREP_FAILURE_REASON_OUTPUT_INVALID,
	    status, NULL, request.operation, left, right, boolean_result);
	} else {
	enum rt_brep_failure_reason reason =
	    RT_BREP_FAILURE_REASON_BOOLEAN_INDETERMINATE;
	if (status == BREP_BOOLEAN_INVALID_INPUT)
	    reason = RT_BREP_FAILURE_REASON_INVALID_OBJECT;
	else if (status == BREP_BOOLEAN_UNSUPPORTED)
	    reason = RT_BREP_FAILURE_REASON_UNSUPPORTED_OBJECT;
	else if (status == BREP_BOOLEAN_BUDGET_EXCEEDED)
	    reason = RT_BREP_FAILURE_REASON_WORK_BUDGET;
	else if (status == BREP_BOOLEAN_INTERNAL_ERROR)
	    reason = RT_BREP_FAILURE_REASON_INTERNAL_ERROR;
	record_worker_failure(info, RT_BREP_FAILURE_STAGE_BOOLEAN_EVALUATION,
	    reason, status, NULL, request.operation, left, right,
	    boolean_result, &boolean_failure);
    }
    delete left;
    delete right;
    delete boolean_result;
    return status;
}


static bool
write_brep_archive(const char *path, const ON_Brep *brep, size_t *byte_count)
{
    if (!path || !path[0] || !brep || !byte_count)
	return false;
    *byte_count = 0;

    struct rt_brep_internal brep_internal;
    brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
    brep_internal.brep = const_cast<ON_Brep *>(brep);
    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_BREP;
    internal.idb_minor_type = ID_BREP;
    internal.idb_meth = &OBJ[ID_BREP];
    internal.idb_ptr = &brep_internal;

    struct bu_external external = BU_EXTERNAL_INIT_ZERO;
    if (OBJ[ID_BREP].ft_export5(&external, &internal, 1.0, NULL) < 0)
	return false;
    FILE *output = fopen(path, "wb");
    bool success = output &&
	(!external.ext_nbytes || fwrite(external.ext_buf, 1,
	    external.ext_nbytes, output) == external.ext_nbytes) &&
	fflush(output) == 0;
    if (output && fclose(output) != 0)
	success = false;
    if (success)
	*byte_count = external.ext_nbytes;
    bu_free_external(&external);
    if (!success)
	(void)bu_file_delete(path);
    return success;
}


static ON_Brep *
read_brep_archive(const char *path, size_t expected_size)
{
    if (!path || !path[0] || !expected_size ||
	expected_size > static_cast<size_t>(std::numeric_limits<long>::max()))
    {
	return NULL;
    }
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input.is_open() || input.tellg() < 0 ||
	static_cast<uint64_t>(input.tellg()) != expected_size)
    {
	return NULL;
    }
    input.seekg(0, std::ios::beg);
    std::vector<unsigned char> bytes(expected_size);
    if (!input.read(reinterpret_cast<char *>(bytes.data()),
	static_cast<std::streamsize>(bytes.size())) || input.peek() != EOF)
    {
	return NULL;
    }

    struct bu_external external = BU_EXTERNAL_INIT_ZERO;
    external.ext_nbytes = static_cast<long>(bytes.size());
    external.ext_buf = bytes.data();
    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    if (OBJ[ID_BREP].ft_import5(&internal, &external, NULL, NULL) < 0)
	return NULL;
    struct rt_brep_internal *brep_internal =
	static_cast<struct rt_brep_internal *>(internal.idb_ptr);
    ON_Brep *brep = NULL;
    if (brep_internal && brep_internal->magic == RT_BREP_INTERNAL_MAGIC &&
	brep_internal->brep)
    {
	brep = ON_Brep::New(*brep_internal->brep);
    }
    rt_db_free_internal(&internal);
    if (validate_result_brep(brep) != BREP_BOOLEAN_SUCCESS) {
	delete brep;
	return NULL;
    }
    return brep;
}


static bool
fault_claimed(const char *claim_file, const std::string &selector)
{
    if (!claim_file || !claim_file[0] || bu_file_exists(claim_file, NULL))
	return false;
    FILE *claim = fopen(claim_file, "wb");
    if (!claim)
	return false;
    const bool written = fwrite(selector.data(), 1, selector.size(), claim) ==
	selector.size();
    const bool closed = fclose(claim) == 0;
    return written && closed;
}


static void
inject_test_fault(const PendingRequest &request)
{
    const char *fault = getenv("LIBRT_BREP_BOOLEAN_TEST_FAULT");
    const char *claim_file = getenv("LIBRT_BREP_BOOLEAN_TEST_FAULT_FILE");
    if (!fault || !fault[0] || !claim_file || !claim_file[0])
	return;

    const std::string crash_selector = "crash:" + request.left_name;
    const std::string hang_selector = "hang:" + request.left_name;
    const std::string protocol_selector = "protocol:" + request.left_name;
    const std::string detailed_protocol_selector =
	"protocol-detail:" + request.left_name;
    const std::string requested(fault);
    if (requested != crash_selector && requested != hang_selector &&
	requested != protocol_selector &&
	requested != detailed_protocol_selector)
	return;
    if (!fault_claimed(claim_file, requested))
	return;
    if (requested == crash_selector)
	bu_bomb("Injected B-Rep worker crash\n");
    if (requested == protocol_selector) {
	(void)fputs("BREP_BOOLEAN_RESULT malformed\n", stdout);
	(void)fflush(stdout);
    }
    if (requested == detailed_protocol_selector) {
	WorkerResponse malformed;
	malformed.id = request.id;
	malformed.boolean_status = BREP_BOOLEAN_INVALID_INPUT;
	malformed.initial_status = BREP_BOOLEAN_INVALID_INPUT;
	malformed.failure.stage = RT_BREP_FAILURE_STAGE_BOOLEAN_EVALUATION;
	malformed.failure.reason = RT_BREP_FAILURE_REASON_INVALID_OBJECT;
	malformed.failure.status = BREP_BOOLEAN_INVALID_INPUT;
	malformed.failure.operation = -1;
	malformed.failure.boolean_failure.stage =
	    static_cast<enum brep_boolean_failure_stage>(
		BREP_BOOLEAN_FAILURE_STAGE_INTERNAL + 1);
	malformed.failure.boolean_failure.reason =
	    static_cast<enum brep_boolean_failure_reason>(
		BREP_BOOLEAN_FAILURE_REASON_SSI_EDGE_PARTITION + 1);
	bu_strlcpy(malformed.failure.tree_path, "r",
	    sizeof(malformed.failure.tree_path));
	malformed.worker_pid = bu_pid();
	malformed.worker_job_count = 1;
	(void)send_response(stdout, malformed);
    }
    while (true)
	(void)bu_snooze(100000);
}


static int
worker_server(const char *database_path)
{
    if (!database_path || !database_path[0])
	return BRLCAD_ERROR;
    setvbuf(stdout, NULL, _IONBF, 0);

    struct db_i *dbip = db_open(database_path, DB_OPEN_READONLY);
    if (!dbip || db_dirbuild(dbip) < 0) {
	if (dbip)
	    db_close(dbip);
	return BRLCAD_ERROR;
    }
    db_update_nref(dbip);

    size_t completed_jobs = 0;
    int server_status = BRLCAD_OK;
    while (true) {
	PendingRequest request;
	const ReadResult read_result = receive_request(stdin, request);
	if (read_result == ReadResult::End)
	    break;
	if (read_result == ReadResult::Error) {
	    server_status = BRLCAD_ERROR;
	    break;
	}

	inject_test_fault(request);
	ON_Brep *result = NULL;
	struct rt_brep_conversion_info conversion_info =
	    RT_BREP_CONVERSION_INFO_INIT;
	int boolean_status = BREP_BOOLEAN_INTERNAL_ERROR;
	try {
	    boolean_status = request.kind == RequestKind::Convert ?
		convert_object(dbip, request, &result, &conversion_info) :
		evaluate_boolean(dbip, request, &result, &conversion_info);
	} catch (const std::exception &error) {
	    bu_log("B-Rep worker request %llu failed: %s\n",
		static_cast<unsigned long long>(request.id), error.what());
	    boolean_status = BREP_BOOLEAN_INTERNAL_ERROR;
	} catch (...) {
	    bu_log("B-Rep worker request %llu failed with an unknown exception\n",
		static_cast<unsigned long long>(request.id));
	    boolean_status = BREP_BOOLEAN_INTERNAL_ERROR;
	}
	if (boolean_status < BREP_BOOLEAN_SUCCESS &&
	    conversion_info.failure.stage == RT_BREP_FAILURE_STAGE_NONE)
	{
	    record_worker_failure(&conversion_info,
		request.kind == RequestKind::Boolean ?
		RT_BREP_FAILURE_STAGE_BOOLEAN_EVALUATION :
		RT_BREP_FAILURE_STAGE_LEAF_CONVERSION,
		boolean_status == BREP_BOOLEAN_INTERNAL_ERROR ?
		RT_BREP_FAILURE_REASON_INTERNAL_ERROR :
		RT_BREP_FAILURE_REASON_INVALID_OBJECT,
		boolean_status, request.left_name.c_str(),
		request.kind == RequestKind::Boolean ? request.operation : -1);
	}

	size_t result_bytes = 0;
	if (boolean_status == BREP_BOOLEAN_SUCCESS &&
	    !write_brep_archive(request.result_path.c_str(), result,
		&result_bytes))
	{
	    boolean_status = BREP_BOOLEAN_INTERNAL_ERROR;
	    initialize_conversion_info(&conversion_info);
	    record_worker_failure(&conversion_info,
		RT_BREP_FAILURE_STAGE_OUTPUT_VALIDATION,
		RT_BREP_FAILURE_REASON_INTERNAL_ERROR, boolean_status, NULL,
		-1);
	}
	delete result;
	if (boolean_status != BREP_BOOLEAN_SUCCESS)
	    (void)bu_file_delete(request.result_path.c_str());

	completed_jobs++;
	WorkerResponse response;
	response.id = request.id;
	response.boolean_status = boolean_status;
	if (conversion_info.initial_status == BREP_BOOLEAN_INTERNAL_ERROR &&
	    boolean_status != BREP_BOOLEAN_INTERNAL_ERROR)
	{
	    conversion_info.initial_status = boolean_status;
	}
	response.fidelity = conversion_info.fidelity;
	response.declared_relative_tolerance =
	    conversion_info.declared_relative_tolerance;
	response.approximation_flags = conversion_info.approximation_flags;
	response.regularization_attempts =
	    conversion_info.regularization_attempts;
	response.whole_tree_regularization_attempts =
	    conversion_info.whole_tree_regularization_attempts;
	response.repaired_arb_count = conversion_info.repaired_arb_count;
	response.initial_status = conversion_info.initial_status;
	response.failure = conversion_info.failure;
	response.resident_bytes = resident_size();
	response.result_bytes = result_bytes;
	response.worker_pid = bu_pid();
	response.worker_job_count = completed_jobs;
	if (!send_response(stdout, response)) {
	    server_status = BRLCAD_ERROR;
	    break;
	}
    }

    db_close(dbip);
    return server_status;
}


static bool
make_workspace(std::string &workspace)
{
    for (size_t attempt = 0; attempt < 100; ++attempt) {
	const uint64_t serial = pool_serial.fetch_add(1,
	    std::memory_order_relaxed) + 1;
	std::ostringstream leaf;
	leaf << "brlcad_brep_boolean_" << bu_pid() << '_' << serial;
	char path[MAXPATHLEN] = {0};
	if (!bu_dir(path, sizeof(path), BU_DIR_TEMP, leaf.str().c_str(), NULL))
	    return false;
	if (bu_file_exists(path, NULL))
	    continue;
	bu_mkdir(path);
	if (bu_file_directory(path)) {
	    workspace = path;
	    return true;
	}
    }
    return false;
}


static std::string
result_path(const std::string &workspace, uint64_t request_id)
{
    std::ostringstream leaf;
    leaf << "result_" << request_id << ".brep";
    char path[MAXPATHLEN] = {0};
    if (!bu_dir(path, sizeof(path), workspace.c_str(), leaf.str().c_str(),
	    NULL))
    {
	return std::string();
    }
    return path;
}


static bool
resolve_worker_executable(std::string &executable, const char *override_path)
{
    if (override_path && override_path[0]) {
	executable = override_path;
    } else {
	char path[MAXPATHLEN] = {0};
	if (!bu_dir(path, sizeof(path), BU_DIR_BIN, "brep_boolean_worker",
		BU_DIR_EXT, NULL))
	{
	    return false;
	}
	executable = path;
    }
    return bu_file_exists(executable.c_str(), NULL) &&
	bu_file_executable(executable.c_str());
}


static void
drain_stderr(WorkerState &worker)
{
    if (!worker.process)
	return;
    char buffer[4096];
    const int descriptor = bu_process_fileno(worker.process,
	BU_PROCESS_STDERR);
    while (descriptor >= 0 && bu_process_pending(descriptor)) {
	const int count = bu_process_read_n(worker.process, BU_PROCESS_STDERR,
	    static_cast<int>(sizeof(buffer)), buffer);
	if (count <= 0)
	    break;
	append_diagnostic(worker.diagnostic, buffer, static_cast<size_t>(count));
    }
}


static void
consume_stdout(WorkerState &worker, const char *data, size_t size)
{
    if (!data && size)
	return;
    if (size)
	worker.pending_output.append(data, size);
    if (worker.pending_output.size() > PROTOCOL_OUTPUT_MAX) {
	worker.protocol_error = true;
	return;
    }

    size_t newline = std::string::npos;
    while ((newline = worker.pending_output.find('\n')) !=
	    std::string::npos)
    {
	std::string line = worker.pending_output.substr(0, newline);
	worker.pending_output.erase(0, newline + 1);
	WorkerResponse response;
	if (line.compare(0, strlen(RESULT_MAGIC), RESULT_MAGIC) == 0) {
	    if (worker.response_received || !parse_response(line, response) ||
		!worker.active || response.id != worker.active->id ||
		response.worker_job_count != worker.completed_jobs + 1)
	    {
		worker.protocol_error = true;
		return;
	    }
	    worker.response = response;
	    worker.response_received = true;
	} else {
	    append_diagnostic_line(worker.diagnostic, line);
	}
    }
}


static void
drain_stdout(WorkerState &worker)
{
    if (!worker.process)
	return;
    char buffer[4096];
    const int descriptor = bu_process_fileno(worker.process,
	BU_PROCESS_STDOUT);
    while (descriptor >= 0 && bu_process_pending(descriptor)) {
	const int count = bu_process_read_n(worker.process, BU_PROCESS_STDOUT,
	    static_cast<int>(sizeof(buffer)), buffer);
	if (count <= 0)
	    break;
	consume_stdout(worker, buffer, static_cast<size_t>(count));
	if (worker.protocol_error)
	    break;
    }
}


static int
stop_worker(WorkerState &worker, bool terminate)
{
    if (!worker.process)
	return BRLCAD_OK;
    if (worker.input) {
	bu_process_file_close(worker.process, BU_PROCESS_STDIN);
	worker.input = NULL;
    }
    if (terminate)
	(void)bu_process_terminate(worker.process);

    const int64_t deadline = bu_gettime() +
	static_cast<int64_t>(PROCESS_STOP_TIMEOUT_MS) * 1000;
    int poll_result = bu_process_poll(worker.process, NULL);
    while (poll_result == 0 && bu_gettime() < deadline) {
	drain_stdout(worker);
	drain_stderr(worker);
	(void)bu_snooze(PROCESS_POLL_USEC);
	poll_result = bu_process_poll(worker.process, NULL);
    }
    drain_stdout(worker);
    drain_stderr(worker);
    if (poll_result != 1)
	(void)bu_process_terminate(worker.process);
    const int status = bu_process_wait_n(&worker.process, 0);
    worker.input = NULL;
    worker.pending_output.clear();
    worker.protocol_error = false;
    worker.response_received = false;
    worker.response = WorkerResponse();
    worker.completed_jobs = 0;
    worker.resident_bytes = 0;
    return status;
}

} // namespace


struct rt_brep_worker_pool {
    std::string database_path;
    std::string executable;
    std::string workspace;
    size_t max_jobs_per_worker = DEFAULT_MAX_JOBS;
    size_t max_worker_resident_bytes = DEFAULT_MAX_RESIDENT;
    unsigned int request_timeout_ms = DEFAULT_TIMEOUT_MS;
    uint64_t next_request_id = 1;
    size_t outstanding = 0;
    std::deque<PendingRequest> queued;
    std::deque<CompletedRequest> completed;
    std::vector<WorkerState> workers;
};


namespace {

static bool
start_worker(struct rt_brep_worker_pool *pool, WorkerState &worker)
{
    if (!pool || worker.process)
	return false;

    const char *arguments[] = {
	pool->executable.c_str(), "--server", pool->database_path.c_str(), NULL
    };
    bu_process_create(&worker.process, arguments, BU_PROCESS_HIDE_WINDOW);
    if (worker.process)
	worker.input = bu_process_file_open(worker.process, BU_PROCESS_STDIN);
    if (worker.process && worker.input &&
	setvbuf(worker.input, NULL, _IONBF, 0) == 0)
    {
	worker.pending_output.clear();
	worker.diagnostic.clear();
	worker.protocol_error = false;
	worker.response_received = false;
	worker.completed_jobs = 0;
	worker.resident_bytes = 0;
	return true;
    }
    (void)stop_worker(worker, true);
    return false;
}


static void
queue_completed(struct rt_brep_worker_pool *pool, WorkerState &worker,
	enum rt_brep_worker_transport_status transport_status,
	int boolean_status, ON_Brep *brep = NULL)
{
    if (!pool || !worker.active) {
	delete brep;
	return;
    }
    CompletedRequest completed;
    completed.id = worker.active->id;
    completed.boolean_status = boolean_status;
    completed.transport_status = transport_status;
    completed.brep = brep;
    completed.diagnostic = worker.diagnostic;
    completed.worker_pid = worker.response_received ?
	worker.response.worker_pid :
	(worker.process ? bu_process_pid(worker.process) : 0);
    completed.worker_job_count = worker.response_received ?
	worker.response.worker_job_count : worker.completed_jobs;
    completed.worker_resident_bytes = worker.response_received ?
	worker.response.resident_bytes : worker.resident_bytes;
    if (worker.response_received) {
	completed.conversion_info.fidelity =
	    static_cast<enum rt_brep_conversion_fidelity>(
		worker.response.fidelity);
	completed.conversion_info.declared_relative_tolerance =
	    worker.response.declared_relative_tolerance;
	completed.conversion_info.approximation_flags =
	    worker.response.approximation_flags;
	completed.conversion_info.regularization_attempts =
	    worker.response.regularization_attempts;
	completed.conversion_info.whole_tree_regularization_attempts =
	    worker.response.whole_tree_regularization_attempts;
	completed.conversion_info.repaired_arb_count =
	    worker.response.repaired_arb_count;
	completed.conversion_info.initial_status =
	    worker.response.initial_status;
	completed.conversion_info.failure = worker.response.failure;
    }
    if (!worker.active->result_path.empty())
	(void)bu_file_delete(worker.active->result_path.c_str());
    worker.active.reset();
    worker.diagnostic.clear();
    worker.protocol_error = false;
    worker.response_received = false;
    worker.response = WorkerResponse();
    pool->completed.push_back(std::move(completed));
}


static void
complete_transport_failure(struct rt_brep_worker_pool *pool,
	WorkerState &worker, enum rt_brep_worker_transport_status status,
	const char *message, bool terminate)
{
    if (message)
	append_diagnostic_line(worker.diagnostic, message);
    const int process_status = stop_worker(worker, terminate);
    if (process_status != BRLCAD_OK) {
	std::ostringstream detail;
	detail << "worker process status " << process_status;
	append_diagnostic_line(worker.diagnostic, detail.str());
    }
    queue_completed(pool, worker, status, BREP_BOOLEAN_INTERNAL_ERROR);
}


static bool
dispatch_request(struct rt_brep_worker_pool *pool, WorkerState &worker)
{
    if (!pool || pool->queued.empty() || worker.active)
	return false;

    worker.active.reset(new PendingRequest(std::move(pool->queued.front())));
    pool->queued.pop_front();
    worker.diagnostic.clear();
    worker.protocol_error = false;
    worker.response_received = false;
    worker.response = WorkerResponse();

    if (!worker.process && !start_worker(pool, worker)) {
	queue_completed(pool, worker, RT_BREP_WORKER_START_FAILED,
	    BREP_BOOLEAN_INTERNAL_ERROR);
	return true;
    }
    if (!send_request(worker.input, *worker.active)) {
	complete_transport_failure(pool, worker,
	    RT_BREP_WORKER_WRITE_FAILED,
	    "unable to submit request to B-Rep worker", true);
	return true;
    }
    worker.active->start_time = bu_gettime();
    return true;
}


static void
complete_response(struct rt_brep_worker_pool *pool, WorkerState &worker)
{
    ON_Brep *brep = NULL;
    enum rt_brep_worker_transport_status transport =
	RT_BREP_WORKER_TRANSPORT_OK;
    int status = worker.response.boolean_status;
    if (status == BREP_BOOLEAN_SUCCESS) {
	try {
	    brep = read_brep_archive(worker.active->result_path.c_str(),
		worker.response.result_bytes);
	} catch (const std::exception &error) {
	    append_diagnostic_line(worker.diagnostic, error.what());
	} catch (...) {
	    append_diagnostic_line(worker.diagnostic,
		"unknown exception while importing worker result");
	}
	if (!brep) {
	    transport = RT_BREP_WORKER_RESULT_ERROR;
	    status = BREP_BOOLEAN_INTERNAL_ERROR;
	    append_diagnostic_line(worker.diagnostic,
		"unable to import the staged B-Rep worker result");
	}
    }

    worker.completed_jobs = worker.response.worker_job_count;
    worker.resident_bytes = worker.response.resident_bytes;
    const bool retire =
	(pool->max_jobs_per_worker &&
	 worker.completed_jobs >= pool->max_jobs_per_worker) ||
	(pool->max_worker_resident_bytes && worker.resident_bytes >=
	 pool->max_worker_resident_bytes);
    queue_completed(pool, worker, transport, status, brep);
    if (retire)
	(void)stop_worker(worker, false);
}


static bool
service_pool(struct rt_brep_worker_pool *pool)
{
    bool progress = false;
    for (WorkerState &worker : pool->workers) {
	if (!worker.process || !worker.active)
	    continue;
	drain_stdout(worker);
	drain_stderr(worker);
	if (worker.protocol_error) {
	    complete_transport_failure(pool, worker,
		RT_BREP_WORKER_PROTOCOL_ERROR,
		"malformed response from B-Rep worker", true);
	    progress = true;
	    continue;
	}

	const int process_state = bu_process_poll(worker.process, NULL);
	const bool timed_out = pool->request_timeout_ms &&
	    bu_gettime() - worker.active->start_time >=
	    static_cast<int64_t>(pool->request_timeout_ms) * 1000;
	if (worker.response_received) {
	    complete_response(pool, worker);
	    if (process_state != 0)
		(void)stop_worker(worker, false);
	    progress = true;
	} else if (timed_out) {
	    complete_transport_failure(pool, worker,
		RT_BREP_WORKER_TIMED_OUT,
		"B-Rep worker request timed out", true);
	    progress = true;
	} else if (process_state != 0) {
	    complete_transport_failure(pool, worker,
		RT_BREP_WORKER_CRASHED,
		"B-Rep worker exited before reporting a result", false);
	    progress = true;
	}
    }

    for (WorkerState &worker : pool->workers) {
	if (pool->queued.empty())
	    break;
	if (!worker.active && dispatch_request(pool, worker))
	    progress = true;
    }
    return progress;
}


static uint64_t
submit_request(struct rt_brep_worker_pool *pool, RequestKind kind,
	const char *left_name, const char *right_name, op_type operation,
	const struct bn_tol *tolerance)
{
    if (!pool || !left_name || !left_name[0] ||
	strlen(left_name) > PROTOCOL_FIELD_MAX ||
	(right_name && strlen(right_name) > PROTOCOL_FIELD_MAX) ||
	!valid_operation(operation) || !pool->next_request_id)
    {
	return 0;
    }
    if (kind == RequestKind::Boolean && (!right_name || !right_name[0]))
	return 0;

    PendingRequest request;
    request.id = pool->next_request_id++;
    request.kind = kind;
    request.left_name = left_name;
    if (right_name)
	request.right_name = right_name;
    request.operation = operation;
    request.tolerance = tolerance ? *tolerance : bn_tol{
	BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST,
	SMALL_FASTF, 1.0 - SMALL_FASTF};
    if (request.tolerance.magic != BN_TOL_MAGIC ||
	!std::isfinite(request.tolerance.dist) || request.tolerance.dist < 0.0 ||
	!std::isfinite(request.tolerance.perp) ||
	request.tolerance.perp < 0.0 ||
	!std::isfinite(request.tolerance.para))
    {
	return 0;
    }
    request.result_path = result_path(pool->workspace, request.id);
    if (request.result_path.empty())
	return 0;
    pool->queued.push_back(std::move(request));
    pool->outstanding++;
    return pool->queued.back().id;
}

} // namespace


extern "C" RT_EXPORT int
_rt_brep_boolean_worker_server(int argc, const char **argv)
{
    if (argc != 3 || !argv || !argv[1] || !argv[2] ||
	!BU_STR_EQUAL(argv[1], "--server"))
    {
	return BRLCAD_ERROR;
    }
    return worker_server(argv[2]);
}


extern "C" struct rt_brep_worker_pool *
rt_brep_worker_pool_create(const char *database_path,
    const struct rt_brep_worker_options *options)
{
    if (!database_path || !database_path[0] ||
	!bu_file_exists(database_path, NULL))
    {
	return NULL;
    }

    std::unique_ptr<struct rt_brep_worker_pool> pool(
	new rt_brep_worker_pool());
    pool->database_path = database_path;
    const size_t requested_workers = options && options->worker_count ?
	options->worker_count : 1;
    if (requested_workers > WORKER_COUNT_MAX ||
	!resolve_worker_executable(pool->executable,
	    options ? options->worker_executable : NULL) ||
	!make_workspace(pool->workspace))
    {
	return NULL;
    }

    pool->request_timeout_ms = options && options->request_timeout_ms ?
	options->request_timeout_ms : DEFAULT_TIMEOUT_MS;
    pool->max_jobs_per_worker = options && options->max_jobs_per_worker ?
	options->max_jobs_per_worker : DEFAULT_MAX_JOBS;
    pool->max_worker_resident_bytes =
	options && options->max_worker_resident_bytes ?
	options->max_worker_resident_bytes : DEFAULT_MAX_RESIDENT;
    pool->workers.resize(requested_workers);
    return pool.release();
}


extern "C" uint64_t
rt_brep_worker_pool_submit_conversion(struct rt_brep_worker_pool *pool,
    const char *object_name, const struct bn_tol *tol)
{
    return submit_request(pool, RequestKind::Convert, object_name, NULL,
	BOOLEAN_UNION, tol);
}


extern "C" uint64_t
rt_brep_worker_pool_submit_boolean(struct rt_brep_worker_pool *pool,
    const char *left_name, const char *right_name, enum op_type operation)
{
    return submit_request(pool, RequestKind::Boolean, left_name, right_name,
	operation, NULL);
}


extern "C" int
rt_brep_worker_pool_poll(struct rt_brep_worker_pool *pool,
    struct rt_brep_worker_result *result, unsigned int wait_ms)
{
    if (!pool || !result)
	return -1;

    const int64_t start = bu_gettime();
    const int64_t deadline = wait_ms >
	static_cast<unsigned int>(std::numeric_limits<int64_t>::max() / 1000) ?
	std::numeric_limits<int64_t>::max() :
	start + static_cast<int64_t>(wait_ms) * 1000;
    do {
	(void)service_pool(pool);
	if (!pool->completed.empty()) {
	    CompletedRequest completed = std::move(pool->completed.front());
	    pool->completed.pop_front();
	    struct rt_brep_worker_result initialized =
		RT_BREP_WORKER_RESULT_INIT;
	    *result = initialized;
	    result->request_id = completed.id;
	    result->boolean_status = completed.boolean_status;
	    result->transport_status = completed.transport_status;
	    result->conversion_info = completed.conversion_info;
	    result->brep = completed.brep;
	    completed.brep = NULL;
	    result->diagnostic = completed.diagnostic.empty() ? NULL :
		bu_strdup(completed.diagnostic.c_str());
	    result->worker_pid = completed.worker_pid;
	    result->worker_job_count = completed.worker_job_count;
	    result->worker_resident_bytes = completed.worker_resident_bytes;
	    pool->outstanding--;
	    return 1;
	}
	if (!wait_ms || !pool->outstanding || bu_gettime() >= deadline)
	    return 0;
	(void)bu_snooze(PROCESS_POLL_USEC);
    } while (true);
}


extern "C" size_t
rt_brep_worker_pool_pending(const struct rt_brep_worker_pool *pool)
{
    return pool ? pool->outstanding : 0;
}


extern "C" void
rt_brep_worker_result_destroy(struct rt_brep_worker_result *result)
{
    if (!result)
	return;
    delete result->brep;
    if (result->diagnostic)
	bu_free(result->diagnostic, "B-Rep worker diagnostic");
    struct rt_brep_worker_result initialized = RT_BREP_WORKER_RESULT_INIT;
    *result = initialized;
}


extern "C" void
rt_brep_worker_pool_destroy(struct rt_brep_worker_pool *pool)
{
    if (!pool)
	return;
    for (WorkerState &worker : pool->workers) {
	if (worker.active && !worker.active->result_path.empty())
	    (void)bu_file_delete(worker.active->result_path.c_str());
	worker.active.reset();
	(void)stop_worker(worker, true);
    }
    for (const PendingRequest &request : pool->queued)
	if (!request.result_path.empty())
	    (void)bu_file_delete(request.result_path.c_str());
    for (CompletedRequest &completed : pool->completed)
	delete completed.brep;
    if (!pool->workspace.empty())
	(void)bu_file_delete(pool->workspace.c_str());
    delete pool;
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
