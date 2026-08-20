/*                         T E S T _ L O G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "bio.h"
#include "bu.h"


static std::atomic<size_t> test_log_calls(0);


static int
test_log_count_hook(void *UNUSED(clientdata), void *UNUSED(msg))
{
    test_log_calls.fetch_add(1, std::memory_order_relaxed);
    return 0;
}


static int
test_log_count_hook_b(void *UNUSED(clientdata), void *UNUSED(msg))
{
    test_log_calls.fetch_add(1, std::memory_order_relaxed);
    return 0;
}


static int
test_log_recursive_hook(void *UNUSED(clientdata), void *msg)
{
    test_log_calls.fetch_add(1, std::memory_order_relaxed);
    if (BU_STR_EQUAL((const char *)msg, "outer"))
	bu_log("nested\n");
    return 0;
}


static int
test_log_parallel(void)
{
    const size_t thread_cnt = 8;
    const size_t logs_per_thread = 1000;
    std::vector<std::thread> threads;

    test_log_calls.store(0, std::memory_order_relaxed);
    bu_log_add_hook(test_log_count_hook, NULL);

    for (size_t i = 0; i < thread_cnt; i++) {
	threads.emplace_back([=]() {
	    for (size_t j = 0; j < logs_per_thread; j++)
		bu_log("parallel log %zu %zu\n", i, j);
	});
    }
    for (std::thread &thread : threads)
	thread.join();

    bu_log_delete_hook(test_log_count_hook, NULL);

    if (test_log_calls.load(std::memory_order_relaxed) != thread_cnt * logs_per_thread) {
	printf("\nbu_log_parallel FAILED");
	return BRLCAD_ERROR;
    }

    printf("\nbu_log_parallel PASSED");
    return BRLCAD_OK;
}


static int
test_log_recursive(void)
{
    test_log_calls.store(0, std::memory_order_relaxed);
    bu_log_add_hook(test_log_recursive_hook, NULL);
    bu_log("outer");
    bu_log_delete_hook(test_log_recursive_hook, NULL);

    if (test_log_calls.load(std::memory_order_relaxed) != 1) {
	printf("\nbu_log_recursive FAILED");
	return BRLCAD_ERROR;
    }

    printf("\nbu_log_recursive PASSED");
    return BRLCAD_OK;
}


static int
test_log_mutation(void)
{
    const size_t logs_per_thread = 10000;
    const size_t swap_count = 10000;
    std::vector<std::thread> threads;

    test_log_calls.store(0, std::memory_order_relaxed);
    bu_log_add_hook(test_log_count_hook, NULL);

    for (size_t i = 0; i < 2; i++) {
	threads.emplace_back([=]() {
	    for (size_t j = 0; j < logs_per_thread; j++)
		bu_log("mutating log %zu %zu\n", i, j);
	});
    }
    threads.emplace_back([=]() {
	for (size_t i = 0; i < swap_count; i++) {
	    bu_log_add_hook(test_log_count_hook_b, NULL);
	    bu_log_delete_hook(test_log_count_hook, NULL);
	    bu_log_add_hook(test_log_count_hook, NULL);
	    bu_log_delete_hook(test_log_count_hook_b, NULL);
	}
    });

    for (std::thread &thread : threads)
	thread.join();

    bu_log_delete_hook(test_log_count_hook, NULL);

    if (test_log_calls.load(std::memory_order_relaxed) < 2 * logs_per_thread) {
	printf("\nbu_log_mutation FAILED");
	return BRLCAD_ERROR;
    }

    printf("\nbu_log_mutation PASSED");
    return BRLCAD_OK;
}


/* A terminal or pipe can be made non-blocking by another subsystem.  bu_log
 * must wait for room and preserve the complete message rather than bombing
 * after a short write. */
static int
test_log_nonblocking(void)
{
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__)
    printf("\nbu_log_nonblocking SKIPPED");
    return BRLCAD_OK;
#else
    int pipe_fd[2] = {-1, -1};
    int saved_stderr = -1;
    int flags;
    int ret = BRLCAD_ERROR;
    bool reader_started = false;
    const size_t msg_len = 128 * 1024;
    std::string message(msg_len, 'x');
    std::string received;
    std::thread reader;

    message[msg_len - 1] = '\n';
    if (pipe(pipe_fd) != 0)
	goto cleanup;

    saved_stderr = dup(fileno(stderr));
    if (saved_stderr < 0 || dup2(pipe_fd[1], fileno(stderr)) < 0)
	goto cleanup;
    close(pipe_fd[1]);
    pipe_fd[1] = -1;

    flags = fcntl(fileno(stderr), F_GETFL);
    if (flags < 0 || fcntl(fileno(stderr), F_SETFL, flags | O_NONBLOCK) < 0)
	goto cleanup;

    reader = std::thread([&]() {
	char buffer[4096];
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	while (received.size() < message.size()) {
	    size_t remain = message.size() - received.size();
	    ssize_t count = read(pipe_fd[0], buffer,
		remain < sizeof(buffer) ? remain : sizeof(buffer));
	    if (count <= 0)
		return;
	    received.append(buffer, (size_t)count);
	}
    });
    reader_started = true;

    bu_log("%s", message.c_str());
    reader.join();
    reader_started = false;

    if (received == message)
	ret = BRLCAD_OK;

cleanup:
    if (reader_started && reader.joinable())
	reader.join();
    if (saved_stderr >= 0) {
	(void)dup2(saved_stderr, fileno(stderr));
	close(saved_stderr);
	clearerr(stderr);
    }
    if (pipe_fd[0] >= 0)
	close(pipe_fd[0]);
    if (pipe_fd[1] >= 0)
	close(pipe_fd[1]);

    if (ret != BRLCAD_OK) {
	printf("\nbu_log_nonblocking FAILED");
	return BRLCAD_ERROR;
    }

    printf("\nbu_log_nonblocking PASSED");
    return BRLCAD_OK;
#endif
}


int
main(int argc, char *argv[])
{
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    if (argc != 2)
	bu_exit(1, "Usage: %s {mutation|nonblocking|parallel|recursive}\n", argv[0]);

    if (BU_STR_EQUAL(argv[1], "mutation"))
	return test_log_mutation();
    if (BU_STR_EQUAL(argv[1], "nonblocking"))
	return test_log_nonblocking();
    if (BU_STR_EQUAL(argv[1], "parallel"))
	return test_log_parallel();
    if (BU_STR_EQUAL(argv[1], "recursive"))
	return test_log_recursive();

    bu_exit(1, "Unknown log test %s\n", argv[1]);
    return BRLCAD_ERROR;
}
