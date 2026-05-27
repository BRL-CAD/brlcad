/*            T E S T _ B I T V _ B E N C H M A R K . C
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

#include <stdio.h>
#include <stdlib.h>

#include "bu.h"
#include "bu/bitv.h"
#include "bu/time.h"

#define BITS_TO_TEST 100000000 /* 100 million bits */
#define NUM_ITERATIONS 10


static void
test_bitv_benchmark_macros(void)
{
    struct bu_bitv *bv;
    int64_t start, end;
    size_t i;
    volatile size_t counter = 0;
    int iter;

    bu_log("Benchmarking BU_BITSET, BU_BITCLR, BU_BITTEST on %d bits over %d iterations...\n",
	   BITS_TO_TEST, NUM_ITERATIONS);

    bv = bu_bitv_new(BITS_TO_TEST);

    start = bu_gettime();
    for (iter = 0; iter < NUM_ITERATIONS; iter++) {
	for (i = 0; i < BITS_TO_TEST; i++) {
	    BU_BITSET(bv, i);
	}
    }
    end = bu_gettime();
    bu_log("  BU_BITSET:   %10ld us\n", (long)(end - start));

    start = bu_gettime();
    for (iter = 0; iter < NUM_ITERATIONS; iter++) {
	for (i = 0; i < BITS_TO_TEST; i++) {
	    if (BU_BITTEST(bv, i)) {
		counter++;
	    }
	}
    }
    end = bu_gettime();
    bu_log("  BU_BITTEST:  %10ld us (counter: %zu)\n", (long)(end - start), counter);

    start = bu_gettime();
    for (iter = 0; iter < NUM_ITERATIONS; iter++) {
	for (i = 0; i < BITS_TO_TEST; i++) {
	    BU_BITCLR(bv, i);
	}
    }
    end = bu_gettime();
    bu_log("  BU_BITCLR:   %10ld us\n", (long)(end - start));

    bu_bitv_free(bv);
}


static void
test_bitv_benchmark_dummy_callback(size_t bit, void *data)
{
    size_t *counter = (size_t *)data;
    (void)bit;

    (*counter)++;
}


static void
test_bitv_benchmark_loop(void)
{
    struct bu_bitv *bv;
    int64_t start, end;
    volatile size_t counter = 0;
    int iter;

    bv = bu_bitv_new(BITS_TO_TEST);

    printf("Benchmarking bu_bitv_foreach on %d bits over %d iterations...\n",
	   BITS_TO_TEST, NUM_ITERATIONS);
    start = bu_gettime();
    for (iter = 0; iter < NUM_ITERATIONS; iter++) {
	bu_bitv_foreach(bv, test_bitv_benchmark_dummy_callback, (void *)&counter);
    }
    end = bu_gettime();
    printf("  bu_bitv_foreach: %10ld us (counter: %zu)\n", (long)(end - start), counter);

    bu_bitv_free(bv);
}


static void
test_bitv_benchmark_bulk(void)
{
    struct bu_bitv *bv1, *bv2;
    int64_t start, end;
    size_t i;
    int iter;

    bu_log("Benchmarking bu_bitv_and / bu_bitv_or on %d bits over %d iterations...\n",
	   BITS_TO_TEST, NUM_ITERATIONS);

    bv1 = bu_bitv_new(BITS_TO_TEST);
    bv2 = bu_bitv_new(BITS_TO_TEST);

    for (i = 0; i < BITS_TO_TEST; i += 2) {
	BU_BITSET(bv1, i);
    }
    for (i = 0; i < BITS_TO_TEST; i += 3) {
	BU_BITSET(bv2, i);
    }

    start = bu_gettime();
    for (iter = 0; iter < NUM_ITERATIONS; iter++) {
	bu_bitv_and(bv1, bv2);
    }
    end = bu_gettime();
    bu_log("  bu_bitv_and: %10ld us\n", (long)(end - start));

    start = bu_gettime();
    for (iter = 0; iter < NUM_ITERATIONS; iter++) {
	bu_bitv_or(bv1, bv2);
    }
    end = bu_gettime();
    bu_log("  bu_bitv_or:  %10ld us\n", (long)(end - start));

    bu_bitv_free(bv1);
    bu_bitv_free(bv2);
}


int
main(int UNUSED(argc), char *argv[])
{
    if (bu_getprogname()[0] == '\0') {
	bu_setprogname(argv[0]);
    }

    test_bitv_benchmark_macros();
    test_bitv_benchmark_loop();
    test_bitv_benchmark_bulk();

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
