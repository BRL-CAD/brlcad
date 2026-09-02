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

#include <sstream>

#include <string.h>

#include "./worker.h"

static const char FACETIZE_REQUEST_MAGIC[] = "FACETIZE_REQUEST";
static const char FACETIZE_RESULT_MAGIC[] = "FACETIZE_RESULT";
static const char FACETIZE_WRITE_READY_MAGIC[] = "FACETIZE_WRITE_READY";
static const char FACETIZE_WRITE_PROCEED_MAGIC[] = "FACETIZE_WRITE_PROCEED";
static const char FACETIZE_WRITE_DONE_MAGIC[] = "FACETIZE_WRITE_DONE";
static const size_t FACETIZE_REQUEST_NAME_MAX = 1024u * 1024u;
static const size_t FACETIZE_PROTOCOL_HEADER_SIZE = 128u;

static bool
facetize_send_result(FILE *response_stream, const char *message_type, int result)
{
    if (!response_stream)
	return false;

    return fprintf(response_stream, "%s %d\n", message_type, result) >= 0 &&
	fflush(response_stream) == 0;
}

void
FacetizeWorkerClient::reset(FILE *stream)
{
    request_stream = stream;
    pending_output.clear();
}

bool
FacetizeWorkerClient::send_request(const char *object_name)
{
    if (!request_stream || !object_name || !object_name[0])
	return false;

    size_t name_length = strlen(object_name);
    if (name_length > FACETIZE_REQUEST_NAME_MAX)
	return false;

    return fprintf(request_stream, "%s %zu\n", FACETIZE_REQUEST_MAGIC,
	    name_length) >= 0 &&
	fwrite(object_name, 1, name_length, request_stream) == name_length &&
	fputc('\n', request_stream) != EOF && fflush(request_stream) == 0;
}

bool
FacetizeWorkerClient::send_write_proceed()
{
    if (!request_stream)
	return false;

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
	line_stream >> message_type;
	if (message_type == FACETIZE_WRITE_READY_MAGIC &&
		(line_stream >> parsed_payload_size) &&
		(line_stream >> std::ws).eof()) {
	    status.write_ready = true;
	    status.payload_size = parsed_payload_size;
	    continue;
	}
	if (message_type == FACETIZE_WRITE_DONE_MAGIC &&
		(line_stream >> parsed_result) &&
		(line_stream >> std::ws).eof()) {
	    status.result = parsed_result;
	    status.write_done = true;
	    status.result_received = true;
	    continue;
	}
	if (message_type == FACETIZE_RESULT_MAGIC &&
		(line_stream >> parsed_result) &&
		(line_stream >> std::ws).eof()) {
	    status.result = parsed_result;
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
FacetizeWorkerServer::receive_request(std::string &object_name)
{
    object_name.clear();
    if (!request_stream)
	return FacetizeWorkerReadResult::Error;

    char header[FACETIZE_PROTOCOL_HEADER_SIZE];
    if (!fgets(header, sizeof(header), request_stream))
	return feof(request_stream) ? FacetizeWorkerReadResult::End :
	    FacetizeWorkerReadResult::Error;

    size_t header_length = strlen(header);
    if (!header_length || header[header_length - 1] != '\n')
	return FacetizeWorkerReadResult::Error;

    size_t name_length = 0;
    std::string message_type;
    std::istringstream header_stream(header);
    if (!(header_stream >> message_type) ||
	    message_type != FACETIZE_REQUEST_MAGIC ||
	    !(header_stream >> name_length) ||
	    !(header_stream >> std::ws).eof() || !name_length ||
	    name_length > FACETIZE_REQUEST_NAME_MAX)
	return FacetizeWorkerReadResult::Error;

    object_name.assign(name_length, '\0');
    if (fread(&object_name[0], 1, name_length, request_stream) != name_length ||
	    fgetc(request_stream) != '\n') {
	object_name.clear();
	return FacetizeWorkerReadResult::Error;
    }

    return FacetizeWorkerReadResult::Request;
}

bool
FacetizeWorkerServer::send_write_ready(size_t payload_size)
{
    if (!response_stream)
	return false;

    return fprintf(response_stream, "%s %zu\n", FACETIZE_WRITE_READY_MAGIC,
	    payload_size) >= 0 && fflush(response_stream) == 0;
}

bool
FacetizeWorkerServer::receive_write_proceed()
{
    if (!request_stream)
	return false;

    char response[FACETIZE_PROTOCOL_HEADER_SIZE];
    if (!fgets(response, sizeof(response), request_stream))
	return false;

    std::string expected_response = std::string(FACETIZE_WRITE_PROCEED_MAGIC) + "\n";
    return expected_response == response;
}

bool
FacetizeWorkerServer::send_tessellation_result(int result)
{
    return facetize_send_result(response_stream, FACETIZE_RESULT_MAGIC, result);
}

bool
FacetizeWorkerServer::send_write_result(int result)
{
    return facetize_send_result(response_stream, FACETIZE_WRITE_DONE_MAGIC,
	    result);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
