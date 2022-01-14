/*                     V L S _ I N C R . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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

#include <set>

#include <limits.h>
#include <stdlib.h> /* for strtol */
#include <ctype.h>
#include <errno.h> /* for errno */

#include "bu.h"
#include "bn.h"
#include "string.h"


struct StrCmp {
    bool operator()(const char *str1, const char *str2) const {
	return (bu_strcmp(str1, str2) < 0);
    }
};


int
uniq_test(struct bu_vls *n, void *data)
{
    std::set<const char *, StrCmp> *sset = (std::set<const char *, StrCmp> *)data;
    if (sset->find(bu_vls_addr(n)) == sset->end())
	return 1;
    return 0;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);

    int ret = 1;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    std::set<const char *, StrCmp> *sset = new std::set<const char *, StrCmp>;
    const char *str1 = "test.r2";
    sset->insert(str1);

    /* Sanity check */
    if (argc < 3)
	bu_exit(1, "Usage: %s {initial} {expected}\n", argv[0]);

    bu_vls_sprintf(&name, "%s", argv[1]);
    (void)bu_vls_incr(&name, NULL, NULL, &uniq_test, (void *)sset);

    if (BU_STR_EQUAL(bu_vls_addr(&name), argv[2]))
	ret = 0;

    bu_log("output: %s\n", bu_vls_addr(&name));

    delete sset;

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
