/*                     T E S T _ L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2026 United States Government as represented by
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

#include "bu.h"


struct test_list {
    struct bu_list l;
    long i;
};


static
int build_list_append(struct test_list *list, long lcnt)
{
    if (!list || lcnt < 0)
	return -1;

    for (long i = 0; i < lcnt; i++) {
	struct test_list *le;
	BU_ALLOC(le, struct test_list);
	le->i = i;
	BU_LIST_APPEND(&list->l, &le->l);
    }
    return lcnt;
}


int
main(int argc, char *argv[])
{
    long lcnt;
    struct test_list tlist;

    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    argc--;argv++;

    if (argc != 1)
	bu_exit(1, "Usage: bu_test list list_element_cnt");

    if (bu_opt_long(NULL, 1, (const char **) argv, (void *) &lcnt) < 0 || lcnt < 0)
	bu_exit(1, "Error reading list element count\n");

    BU_LIST_INIT(&(tlist.l));

    if (!BU_LIST_IS_EMPTY(&tlist.l))
	bu_exit(1, "Error - list reports non-empty after initialization\n");

    if (build_list_append(&tlist, lcnt) < 0)
	bu_exit(1, "Error initializing test list with %ld elements\n", lcnt);

    bu_list_free(&tlist.l);

    if (!BU_LIST_IS_EMPTY(&tlist.l))
	bu_exit(1, "Error - list reports non-empty after clean-up\n");


    return bu_list_len(&tlist.l);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
