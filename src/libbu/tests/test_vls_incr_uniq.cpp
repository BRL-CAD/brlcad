/*          T E S T _ V L S _ I N C R _ U N I Q . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
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
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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


static int
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


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
