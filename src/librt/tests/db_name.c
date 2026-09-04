/*              D B _ N A M E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"
#include "rt/db_io.h"


static int
expect_name(const char *input, const char *expected)
{
    struct bu_vls actual = BU_VLS_INIT_ZERO;
    int failed = 0;

    if (db_sanitize_name(&actual, input) != 0 ||
	    !BU_STR_EQUAL(bu_vls_cstr(&actual), expected)) {
	bu_log("db_sanitize_name(\"%s\") returned \"%s\", expected \"%s\"\n",
	    input, bu_vls_cstr(&actual), expected);
	failed = 1;
    }
    bu_vls_free(&actual);
    return failed;
}


int
main(int argc, char **argv)
{
    int failed = 0;

    bu_setprogname(argv[0]);
    if (argc != 1) {
	bu_log("Usage: %s\n", argv[0]);
	return 1;
    }

    failed += expect_name(" Steering rack (v3) ", "Steering_rack_v3");
    failed += expect_name("Cr\303\250me br\303\273l\303\251e / \316\251",
	"Creme_brulee_u3A9");
    failed += expect_name("a()[]///b", "a_b");
    failed += expect_name("___", "");
    failed += expect_name("part", "part");

    return failed ? 1 : 0;
}
