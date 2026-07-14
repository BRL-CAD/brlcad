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
#include <thread>
#include <vector>

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


int
main(int argc, char *argv[])
{
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    if (argc != 2)
	bu_exit(1, "Usage: %s {mutation|parallel|recursive}\n", argv[0]);

    if (BU_STR_EQUAL(argv[1], "mutation"))
	return test_log_mutation();
    if (BU_STR_EQUAL(argv[1], "parallel"))
	return test_log_parallel();
    if (BU_STR_EQUAL(argv[1], "recursive"))
	return test_log_recursive();

    bu_exit(1, "Unknown log test %s\n", argv[1]);
    return BRLCAD_ERROR;
}
