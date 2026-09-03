/*                 F A C E T I Z E _ W O R K E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file facetize/tests/worker.cpp
 *
 * Exercise facetize worker protocol framing and state parsing.
 */

#include "common.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "../worker.h"


static int failures = 0;


static void
expect(bool condition, const char *message)
{
    if (condition)
	return;

    std::fprintf(stderr, "FAIL: %s\n", message);
    failures++;
}


static FILE *
stream_with_contents(const std::string &contents)
{
    FILE *stream = tmpfile();
    if (!stream)
	return NULL;

    if (fwrite(contents.data(), 1, contents.size(), stream) != contents.size() ||
	fflush(stream) != 0 || fseek(stream, 0, SEEK_SET) != 0) {
	fclose(stream);
	return NULL;
    }
    return stream;
}


static std::string
read_stream(FILE *stream)
{
    std::string contents;
    char buffer[64];

    if (fflush(stream) != 0 || fseek(stream, 0, SEEK_SET) != 0)
	return contents;
    size_t bytes_read = 0;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), stream)) > 0)
	contents.append(buffer, bytes_read);
    return contents;
}


static void
test_request_round_trip()
{
    FILE *requests = tmpfile();
    expect(requests != NULL, "create request stream");
    if (!requests)
	return;

    const char object_name[] = "assembly/part\nwith spaces";
    FacetizeWorkerClient client;
    client.reset(requests);
    expect(client.send_request(object_name), "client sends a framed object name");
    expect(fflush(requests) == 0 && fseek(requests, 0, SEEK_SET) == 0,
	"rewind request stream");

    FacetizeWorkerServer server(requests, NULL);
    std::string received_name;
    expect(server.receive_request(received_name) == FacetizeWorkerReadResult::Request,
	"server accepts a complete request");
    expect(received_name == object_name, "request framing preserves object name bytes");
    expect(server.receive_request(received_name) == FacetizeWorkerReadResult::End,
	"clean request EOF ends the worker loop");

    fclose(requests);

    client.reset(NULL);
    expect(!client.send_request("part"), "client rejects a missing request stream");
    expect(!client.send_request(NULL), "client rejects a null object name");
    expect(!client.send_request(""), "client rejects an empty object name");
}


static void
test_malformed_requests()
{
    const char *invalid_requests[] = {
	"NOT_A_REQUEST 4\npart\n",
	"FACETIZE_REQUEST 0\n\n",
	"FACETIZE_REQUEST 4 trailing\npart\n",
	"FACETIZE_REQUEST 5\npart\n",
	"FACETIZE_REQUEST 4\npart!"
    };

    for (const char *request : invalid_requests) {
	FILE *stream = stream_with_contents(request);
	expect(stream != NULL, "create malformed request stream");
	if (!stream)
	    continue;
	FacetizeWorkerServer server(stream, NULL);
	std::string object_name;
	expect(server.receive_request(object_name) == FacetizeWorkerReadResult::Error,
	    "server rejects malformed request framing");
	expect(object_name.empty(), "malformed request does not expose a partial name");
	fclose(stream);
    }

    const size_t unterminated_header_size = 128;
    std::string long_header(unterminated_header_size, 'x');
    FILE *stream = stream_with_contents(long_header);
    expect(stream != NULL, "create overlong header stream");
    if (stream) {
	FacetizeWorkerServer server(stream, NULL);
	std::string object_name;
	expect(server.receive_request(object_name) == FacetizeWorkerReadResult::Error,
	    "server rejects a header without a terminator");
	fclose(stream);
    }
}


static void
test_commit_round_trip()
{
    FILE *requests = tmpfile();
    expect(requests != NULL, "create commit stream");
    if (!requests)
	return;

    const char result_file[] = "cache path/worker\nresult.g";
    const char object_name[] = "assembly/part with spaces";
    const size_t payload_size = 1234567;
    FacetizeWorkerClient client;
    client.reset(requests);
    expect(client.send_commit(result_file, object_name, payload_size),
	"client sends a framed commit request");
    expect(fflush(requests) == 0 && fseek(requests, 0, SEEK_SET) == 0,
	"rewind commit stream");

    FacetizeWorkerServer server(requests, NULL);
    FacetizeCommitRequest request;
    expect(server.receive_commit(request) == FacetizeWorkerReadResult::Request,
	"server accepts a complete commit request");
    expect(request.result_file == result_file,
	"commit framing preserves result path bytes");
    expect(request.object_name == object_name,
	"commit framing preserves object name bytes");
    expect(request.payload_size == payload_size,
	"commit framing preserves the payload size");
    expect(server.receive_commit(request) == FacetizeWorkerReadResult::End,
	"clean commit EOF ends the writer loop");
    fclose(requests);

    client.reset(NULL);
    expect(!client.send_commit("result.g", "part", 1),
	"client rejects a commit without a request stream");

    FILE *invalid_requests = tmpfile();
    expect(invalid_requests != NULL, "create invalid commit stream");
    if (invalid_requests) {
	client.reset(invalid_requests);
	expect(!client.send_commit(NULL, "part", 1),
	    "client rejects a null result path");
	expect(!client.send_commit("", "part", 1),
	    "client rejects an empty result path");
	expect(!client.send_commit("result.g", NULL, 1),
	    "client rejects a null object name");
	expect(!client.send_commit("result.g", "", 1),
	    "client rejects an empty object name");
	fclose(invalid_requests);
    }

    const char *malformed_commits[] = {
	"NOT_A_COMMIT 8 4 12\nresult.g\npart\n",
	"FACETIZE_COMMIT 0 4 12\n\npart\n",
	"FACETIZE_COMMIT 8 4 12 trailing\nresult.g\npart\n",
	"FACETIZE_COMMIT 9 4 12\nresult.g\npart\n",
	"FACETIZE_COMMIT 8 5 12\nresult.g\npart\n",
	"FACETIZE_COMMIT 8 4 12\nresult.g!part\n"
    };
    for (const char *commit : malformed_commits) {
	FILE *malformed = stream_with_contents(commit);
	expect(malformed != NULL, "create malformed commit stream");
	if (!malformed)
	    continue;
	FacetizeWorkerServer malformed_server(malformed, NULL);
	expect(malformed_server.receive_commit(request) ==
		FacetizeWorkerReadResult::Error,
	    "server rejects malformed commit framing");
	expect(request.result_file.empty() && request.object_name.empty(),
	    "malformed commit does not expose partial fields");
	fclose(malformed);
    }
}


static void
test_write_handshake()
{
    FILE *requests = tmpfile();
    FILE *responses = tmpfile();
    expect(requests != NULL && responses != NULL, "create handshake streams");
    if (!requests || !responses) {
	if (requests) fclose(requests);
	if (responses) fclose(responses);
	return;
    }

    FacetizeWorkerClient client;
    client.reset(requests);
    expect(client.send_write_proceed(), "client sends write acknowledgement");
    expect(fflush(requests) == 0 && fseek(requests, 0, SEEK_SET) == 0,
	"rewind write acknowledgement");
    FacetizeWorkerServer server(requests, responses);
    expect(server.receive_write_proceed(), "server accepts exact write acknowledgement");

    const size_t announced_payload_size = 987654;
    const size_t announced_resident_size = 456789;
    expect(server.send_write_ready(announced_payload_size,
	    announced_resident_size), "server sends write size");
    expect(server.send_write_started(), "server acknowledges write start");
    expect(server.send_write_result(BRLCAD_OK, announced_resident_size),
	    "server sends write completion");

    std::string output = read_stream(responses);
    FacetizeWorkerStatus status;
    std::vector<std::string> diagnostics;
    const size_t fragment_size = 3;
    for (size_t offset = 0; offset < output.size();) {
	const size_t chunk_size = std::min(fragment_size, output.size() - offset);
	client.consume_output(output.data() + offset, chunk_size, status, diagnostics);
	offset += chunk_size;
    }
    expect(status.write_ready, "client recognizes fragmented write-ready message");
    expect(status.payload_size == announced_payload_size,
	"client preserves announced write size");
    expect(status.resident_size == announced_resident_size,
	"client preserves worker resident memory");
    expect(status.write_started,
	"client recognizes fragmented write-start acknowledgement");
    expect(status.write_done, "client recognizes fragmented write completion");
    expect(status.result_received && status.result == BRLCAD_OK,
	"write completion result is retained");
    expect(diagnostics.empty(), "valid protocol output emits no diagnostics");

    fclose(requests);
    fclose(responses);
    client.reset(NULL);

    FILE *failure_response = tmpfile();
    expect(failure_response != NULL, "create tessellation result stream");
    if (failure_response) {
	FacetizeWorkerServer failure_server(NULL, failure_response);
	expect(failure_server.send_tessellation_result(BRLCAD_ERROR,
		announced_resident_size),
	    "server sends pre-write tessellation failure");
	std::string failure_output = read_stream(failure_response);
	FacetizeWorkerStatus failure_status;
	diagnostics.clear();
	client.consume_output(failure_output.data(), failure_output.size(),
	    failure_status, diagnostics);
	expect(failure_status.result_received &&
	    failure_status.result == BRLCAD_ERROR && !failure_status.write_done,
	    "client distinguishes pre-write failure from write completion");
	expect(failure_status.resident_size == announced_resident_size,
	    "pre-write result reports worker resident memory");
	expect(diagnostics.empty(), "valid tessellation failure emits no diagnostics");
	fclose(failure_response);
    }

    FILE *bad_ack = stream_with_contents("FACETIZE_WRITE_PROCEED trailing\n");
    expect(bad_ack != NULL, "create invalid acknowledgement stream");
    if (bad_ack) {
	FacetizeWorkerServer bad_server(bad_ack, NULL);
	expect(!bad_server.receive_write_proceed(),
	    "server rejects non-exact write acknowledgement");
	fclose(bad_ack);
    }
}


static void
test_client_diagnostics_and_reset()
{
    FacetizeWorkerClient client;
    FacetizeWorkerStatus status;
    std::vector<std::string> diagnostics;

    const char malformed_output[] =
	"ordinary diagnostic\nFACETIZE_RESULT 0 trailing\n"
	"FACETIZE_WRITE_STARTED trailing\n";
    client.consume_output(malformed_output, sizeof(malformed_output) - 1,
	status, diagnostics);
    expect(!status.result_received, "malformed result is not accepted as protocol");
    expect(diagnostics.size() == 3, "ordinary and malformed lines remain diagnostics");

    diagnostics.clear();
    const char partial_frame[] = "FACETIZE_WRITE_RE";
    client.consume_output(partial_frame, sizeof(partial_frame) - 1,
	status, diagnostics);
    client.reset(NULL);
    const char post_reset[] = "ADY 42\n";
    client.consume_output(post_reset, sizeof(post_reset) - 1,
	status, diagnostics);
    expect(!status.write_ready, "reset discards a previous channel's partial frame");
    expect(diagnostics.size() == 1 && diagnostics[0] == "ADY 42",
	"post-reset bytes are reported independently");

    FacetizeWorkerServer null_server(NULL, NULL);
    std::string object_name;
    FacetizeCommitRequest commit_request;
    expect(null_server.receive_request(object_name) == FacetizeWorkerReadResult::Error,
	"server rejects a missing request stream");
    expect(null_server.receive_commit(commit_request) ==
	    FacetizeWorkerReadResult::Error,
	"writer rejects a missing request stream");
    expect(!null_server.receive_write_proceed(),
	"server cannot receive acknowledgement without a stream");
    expect(!null_server.send_write_ready(1, 1),
	"server cannot announce a write without a response stream");
    expect(!null_server.send_write_started(),
	"server cannot acknowledge a write without a response stream");
    expect(!null_server.send_tessellation_result(BRLCAD_OK, 1),
	"server cannot send a result without a response stream");
    expect(!null_server.send_write_result(BRLCAD_OK, 1),
	"server cannot send write completion without a response stream");
}


static void
test_worker_policy()
{
    const size_t mib = (size_t)1024 * 1024;
    const size_t gib = (size_t)1024 * mib;

    FacetizeWorkerPolicy automatic(0, 20, 16, 3 * gib, 2560 * mib);
    expect(automatic.worker_count() == 2,
	"automatic policy conservatively permits two workers");
    expect(automatic.threads_per_worker() == 8,
	"automatic policy divides CPUs between workers");
    expect(automatic.can_dispatch(1, 2 * gib, 512 * mib),
	"automatic policy admits work while its memory reserve is intact");
    expect(!automatic.can_dispatch(1, 1536 * mib, 512 * mib),
	"automatic policy preserves system memory headroom");
    expect(!automatic.can_dispatch(1, 2 * gib, 1280 * mib),
	"observed worker memory increases the next-job reserve");
    expect(!automatic.can_dispatch(1, 0, 0),
	"missing current memory data pauses additional dispatches");
    expect(automatic.can_dispatch(0, 0, 0),
	"one job may make progress when current memory data is missing");

    FacetizeWorkerPolicy serial(1, 20, 16, 3 * gib, 2560 * mib);
    expect(serial.worker_count() == 1 && serial.threads_per_worker() == 16,
	"one requested worker retains the full CPU budget");
    expect(!serial.can_dispatch(1, 2 * gib, 0),
	"serial policy does not admit a second active task");

    FacetizeWorkerPolicy requested(8, 3, 12, 3 * gib, 2560 * mib);
    expect(requested.worker_count() == 3,
	"requested policy is capped by available work");
    expect(requested.threads_per_worker() == 4,
	"requested policy divides the CPU budget by effective workers");

    FacetizeWorkerPolicy unknown_memory(0, 20, 16, 0, 0);
    expect(unknown_memory.worker_count() == 1,
	"unknown memory availability falls back to one worker");

    FacetizeWorkerPolicy low_memory(4, 20, 16, 3 * gib, gib);
    expect(low_memory.worker_count() == 1,
	"low memory availability falls back to one worker");
}


int
main()
{
    test_request_round_trip();
    test_malformed_requests();
    test_commit_round_trip();
    test_write_handshake();
    test_client_diagnostics_and_reset();
    test_worker_policy();

    if (failures)
	std::fprintf(stderr, "%d facetize worker protocol test(s) failed\n", failures);
    return failures ? 1 : 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
