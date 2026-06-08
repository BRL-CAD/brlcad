/*                    T E S T _ V E R S I O N . C
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

#include "common.h"

#include "test_util.h"


static int
test_version(void)
{
    int failures = 0;
    const char *test = "version";
    const char *ver = bn_version();

    if (!ver || !ver[0]) {
	report_failure(test, "bn_version returned an empty string");
	failures++;
    }

    if (!ver || !strpbrk(ver, "0123456789")) {
	report_failure(test, "bn_version returned a string without digits: \"%s\"", ver ? ver : "(null)");
	failures++;
    }

    return failures;
}


int
main(int argc, char *argv[])
{
    return bn_api_single(argc, argv, "version", test_version);
}
