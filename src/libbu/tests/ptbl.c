/*                       P T B L . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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

#include "common.h"

#include "bu.h"

static int DEFAULT_HOW_MANY = 32;

static size_t
test_bu_ptbl_init(size_t blen)
{
    struct bu_ptbl b;
    size_t result;

    bu_ptbl_init(&b, blen, "test_bu_ptbl_init");

    /**
     * Test that the buffer was properly allocated (i.e., not null)
     * and that it is at least as large as was requested. This should
     * leave the door open for future changes to `bu_ptbl_init`, such
     * as allocating the buffer to be a power of 2 greater than the
     * requested size.
     */
    result = b.blen < blen || b.buffer == NULL ? BRLCAD_ERROR : BRLCAD_OK;

    bu_ptbl_free(&b);
    return result;
}

static size_t
test_bu_ptbl_reset(void)
{
    struct bu_ptbl b;
    long p;
    intmax_t locate_before, locate_after;
    size_t result;

    bu_ptbl_init(&b, 0, "test_bu_ptbl_reset");

    p = 0x12;
    bu_ptbl_ins(&b, &p);

    locate_before = bu_ptbl_locate(&b, &p);
    bu_ptbl_reset(&b);
    locate_after = bu_ptbl_locate(&b, &p);

    result = b.end == 0 && locate_before != -1 && locate_after == -1
	? BRLCAD_OK
	: BRLCAD_ERROR;

    bu_ptbl_free(&b);

    printf("\nbu_ptbl_reset ");
    printf(result == BRLCAD_OK ? "PASSED" : "FAILED");
    return result;
}

static intmax_t
test_bu_ptbl_ins(int uniq)
{
    struct bu_ptbl b;
    long p;
    intmax_t locate, result, added = -1;

    bu_ptbl_init(&b, 0, "test_bu_ptbl_ins");

    p = 0x12;
    bu_ptbl_ins(&b, &p);
    locate = bu_ptbl_locate(&b, &p);

    result = locate != -1 ? BRLCAD_OK : BRLCAD_ERROR;

    if (uniq) {
	result = bu_ptbl_ins_unique(&b, &p) == added ? BRLCAD_ERROR : BRLCAD_OK;
    }

    bu_ptbl_free(&b);

    printf("\nbu_ptbl_ins ");
    printf(result == BRLCAD_OK ? "PASSED" : "FAILED");
    return result;
}

/**
 * Test that `bu_ptbl_rm` deletes as many elements as we insert.
 *
 * If `type` is "cons", copies of the element to be deleted are
 * inserted on consecutive positions (i.e., ...A A A...).
 * If `type` is "mix", they are interspersed (i.e., ...A B A C A...).
 */
static size_t
test_bu_ptbl_rm(const char *type)
{
    struct bu_ptbl b;
    long p, q;
    size_t i;
    size_t how_many = DEFAULT_HOW_MANY;
    size_t result;

    bu_ptbl_init(&b, 0, "test_bu_ptbl_rm");

    p = 0x12;
    q = 0x56;

    for (i = 0; i < how_many; i++) {
	bu_ptbl_ins(&b, &p);
	if (BU_STR_EQUAL(type, "mix")) {
	    bu_ptbl_ins(&b, &q);
	}
    }

    result = bu_ptbl_rm(&b, &p) == how_many ? BRLCAD_OK : BRLCAD_ERROR;

    bu_ptbl_free(&b);

    printf("\nbu_ptbl_rm ");
    printf(result == BRLCAD_OK ? "PASSED" : "FAILED");
    return result;
}

/**
 * Test that appending works by merging two lists and
 * removing the element in src from dst. If unique merging
 * was done, only one deletion should take place.
 * Otherwise, as many elements as there were in src.
 */
static size_t
test_bu_ptbl_cat(int uniq)
{
    struct bu_ptbl src, dst;
    long p, q;
    size_t i;
    size_t how_many = DEFAULT_HOW_MANY;
    size_t result = BRLCAD_OK;

    bu_ptbl_init(&src, 0, "test_bu_ptbl_cat src");
    bu_ptbl_init(&dst, 0, "test_bu_ptbl_cat dst");

    p = 0x12;
    q = 0x56;

    for (i = 0; i < how_many; i++) {
	bu_ptbl_ins(&src, &p);
	bu_ptbl_ins(&dst, &q);
    }

    if (uniq) {
	bu_ptbl_cat_uniq(&dst, &src);
	result = bu_ptbl_rm(&dst, &p) == 1 ? BRLCAD_OK : BRLCAD_ERROR;
    } else {
	bu_ptbl_cat(&dst, &src);
	result = bu_ptbl_rm(&dst, &p) == how_many ? BRLCAD_OK : BRLCAD_ERROR;
    }

    bu_ptbl_free(&src);
    bu_ptbl_free(&dst);

    printf("\nbu_ptbl_cat ");
    printf(result == BRLCAD_OK ? "PASSED" : "FAILED");
    return result;
}

static size_t
test_bu_ptbl_trunc(void)
{
    struct bu_ptbl b;
    long p;
    size_t i, how_many = DEFAULT_HOW_MANY, result;

    bu_ptbl_init(&b, 0, "test_bu_ptbl_trunc");

    p = 0x12;

    for (i = 0; i < how_many; i++) {
	bu_ptbl_ins(&b, &p);
    }

    bu_ptbl_trunc(&b, 16);
    result = b.end == 16 ? BRLCAD_OK : BRLCAD_ERROR;

    bu_ptbl_free(&b);

    printf("\nbu_ptbl_trunc ");
    printf(result == BRLCAD_OK ? "PASSED" : "FAILED");
    return result;
}

int
main(int argc, char *argv[])
{
    int ret = BRLCAD_ERROR;
    size_t ptbl_size;

    bu_setprogname(argv[0]);

    if (argc < 2) {
	bu_exit(1, "Usage: %s (init|reset|ins|locate|rm|cat|trunc) [test_args...]\n", argv[0]);
    }

    if (BU_STR_EQUAL(argv[1], "init")) {
	ptbl_size = 100;
	if (argc > 2) {
	    if (bu_opt_long(NULL, 2, (const char **) argv, (void *) &ptbl_size) < 0) {
		bu_log("\nINFO: init: could not convert argument, using size=100\n");
	    }
	}

	ret = test_bu_ptbl_init(ptbl_size);
    } else if (BU_STR_EQUAL(argv[1], "reset")) {
	ret = test_bu_ptbl_reset();
    } else if (BU_STR_EQUAL(argv[1], "ins")) {
	ret = test_bu_ptbl_ins(argc > 2 ? BU_STR_EQUAL(argv[2], "uniq") : 0);
    } else if (BU_STR_EQUAL(argv[1], "locate")) {
	ret = test_bu_ptbl_ins(0);
    } else if (BU_STR_EQUAL(argv[1], "rm")) {
	ret = test_bu_ptbl_rm(argv[2]);
    } else if (BU_STR_EQUAL(argv[1], "cat")) {
	ret = test_bu_ptbl_cat(argc > 2 ? BU_STR_EQUAL(argv[2], "uniq") : 0);
    } else if (BU_STR_EQUAL(argv[1], "trunc")) {
	ret = test_bu_ptbl_trunc();
    }

    return ret;
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
