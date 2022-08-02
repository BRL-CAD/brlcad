/*                     B A D M A G I C . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2022 United States Government as represented by
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu/defines.h"
#include "bu/app.h"

/* Normally, we mark bu_badmagic as a non-returning function for
 * static analyzers.  In this case, because we *do* return after
 * calling it due to the testing, we need to avoid assigning that
 * attribute (it causes interesting crashing behaviors if we leave
 * it in place with some compiler settings.)*/
#ifdef NORETURN
#  undef NORETURN
#  define NORETURN
#endif

#include "bu/magic.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"

static int
bomb_callback(const void *data, const char *str)
{
    char *expected_str;
    int result;
    if (!data) exit(1);
    expected_str = (char *)data;
    result = bu_strcmp(expected_str, str);
    exit(result);
}

int
main(int argc, char *argv[])
{
    unsigned char *misalign = NULL;
    uint32_t *ptr = (uint32_t *)bu_malloc(sizeof(uint32_t), "bu_badmagic.c");
    uint32_t magic;
    char *str = NULL;
    char *expected_str = (char *)bu_malloc(512, "bu_badmagic.c");
    const char *file = "bu_badmagic.c";
    int line = 42, testnum;

    // Normally this file is part of bu_test, so only set this if it looks like
    // the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    if (argc < 2) {
	bu_exit(1, "Usage: %s {number}\nMust specify a function number.\n", argv[0]);
    }

    bu_bomb_add_hook((bu_hook_t)&bomb_callback, (void *)expected_str);

    sscanf(argv[1], "%d", &testnum);
    switch (testnum) {
	case 1:
	    *ptr = BU_AVS_MAGIC;
	    magic = BU_BITV_MAGIC;
	    str = (char *)bu_identify_magic(*(ptr));
	    sprintf(expected_str, "ERROR: bad pointer %p: s/b %s(x%lx), was %s(x%lx), file %s, line %d\n",
		    (void *)ptr,
		    str, (unsigned long)magic,
		    bu_identify_magic(*(ptr)), (unsigned long)*(ptr),
		    file, line);
	    bu_badmagic(ptr, magic, str, file, line);
	    return 1;
	case 2:
	    return 0;
	case 3:
	    ptr = NULL;
	    magic = BU_VLS_MAGIC;
	    str = (char *)bu_identify_magic(magic);
	    sprintf(expected_str, "ERROR: NULL %s pointer, file %s, line %d\n",
		    str, file, line);
	    bu_badmagic(ptr, magic, str, file, line);
	    return 1;
	case 4:
	    misalign = (unsigned char *)ptr;
	    ptr = (uint32_t *)(misalign + 1);
	    magic = BU_EXTERNAL_MAGIC;
	    str = (char *)bu_identify_magic(magic);
	    sprintf(expected_str, "ERROR: %p mis-aligned %s pointer, file %s, line %d\n",
		    (void *)ptr, str, file, line);
	    bu_badmagic(ptr, magic, str, file, line);
	    return 1;
    }

    bu_log("Invalid function number %d specified. [%s]\n", testnum, argv[0]);
    return 1;
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
