/*                   B U _ B A D M A G I C . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
#include <string.h>

#include "bu.h"

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
    unsigned char *misalign = (unsigned char *)bu_malloc(1, "bu_badmagic.c");
    uint32_t *ptr = (uint32_t *)bu_malloc(sizeof(uint32_t), "bu_badmagic.c");
    uint32_t magic;
    char *str = (char *)bu_malloc(20, "bu_badmagic.c");
    char *expected_str = (char *)bu_malloc(512, "bu_badmagic.c");
    char *file = "bu_badmagic.c";
    int line = 42, testnum;

    if (argc < 2) {
	bu_exit(1, "Must specify a function number. [%s]\n", argv[0]);
    }

    bu_bomb_add_hook((bu_hook_t)&bomb_callback, (genptr_t)expected_str);

    sscanf(argv[1], "%d", &testnum);
    switch(testnum) {
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
	*ptr = BU_BITV_MAGIC;
	magic = BU_BITV_MAGIC;
	str = (char *)bu_identify_magic(*ptr);
	expected_str = "\0";
	bu_badmagic(ptr, magic, str, file, line);
	return 0;
    case 3:
	ptr = NULL;
	magic = BU_COLOR_MAGIC;
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
