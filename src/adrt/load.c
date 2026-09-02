/*                         L O A D . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2026 United States Government as represented by
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
/** @file load.c
 *
 * Slave-side dispatch for ADRT load messages: decodes the load opcode from an
 * incoming buffer and routes to the appropriate loader (.g file, region soup,
 * or KD-tree).
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rt/tie.h"
#include "load.h"

uint32_t slave_load_mesh_num;
adrt_mesh_t *slave_load_mesh_list;

void
slave_load_free(void)
{
}

int
slave_load_region(struct tie_s *UNUSED(tie), char *UNUSED(data))
{
    /*
     * data contains a region name and the triangle soup.
     * Meant to be called several times, with slave_load_kdtree called at the
     * end to finally prep it up.
     */
    return -1;
}

int
slave_load_kdtree(struct tie_s *UNUSED(tie), char *UNUSED(data))
{
    /* after slave_load_region calls have filled in all the geometry, this loads
     * a tree or requests a tree generation if data is NULL
     */
    return -1;
}

static const char *
load_string(const char **cursor, const char *end)
{
    const char *start = *cursor;
    const char *terminator;

    if (start >= end)
	return NULL;

    terminator = (const char *)memchr(start, '\0', (size_t)(end - start));
    if (!terminator)
	return NULL;

    *cursor = terminator + 1;
    return start;
}

int
slave_load(struct tie_s *tie, void *data, size_t data_len)
{
    const size_t header_size = 3;
    const char *cursor = (const char *)data;
    const char *end = cursor + data_len;
    uint8_t format;

    tie_check_degenerate = 0;

    if (data_len < header_size + 1) {
	fprintf(stderr, "ADRT load message is truncated\n");
	return 1;
    }

    cursor += header_size;
    format = (uint8_t)*cursor++;

    switch (format) {
	case ADRT_LOAD_FORMAT_G:	/* given a filename and 1 toplevel region, recursively load from a .g file */
	    {
		const char **objects;
		const char *db;
		int argc;
		int i;
		int ret;

		if ((size_t)(end - cursor) < sizeof(argc)) {
		    fprintf(stderr, "ADRT .g load message has no object count\n");
		    return 1;
		}
		memcpy(&argc, cursor, sizeof(argc));
		cursor += sizeof(argc);
		if (argc < 1 || (size_t)argc > (size_t)(end - cursor)) {
		    fprintf(stderr, "ADRT .g load message has an invalid object count\n");
		    return 1;
		}

		db = load_string(&cursor, end);
		if (!db) {
		    fprintf(stderr, "ADRT .g load message has no database path\n");
		    return 1;
		}

		objects = (const char **)calloc((size_t)argc + 1, sizeof(char *));
		if (!objects) {
		    perror("ADRT .g object list");
		    return 1;
		}
		for (i = 0; i < argc; i++) {
		    objects[i] = load_string(&cursor, end);
		    if (!objects[i]) {
			fprintf(stderr, "ADRT .g load message has a truncated object list\n");
			free(objects);
			return 1;
		    }
		}

		ret = load_g(tie, db, argc, objects, &slave_load_mesh_list);
		free(objects);
		return ret;
	    }
	case ADRT_LOAD_FORMAT_REG:	/* special magic for catching data on the pipe */
	    return slave_load_region(tie, (char *)cursor);
	case ADRT_LOAD_FORMAT_KDTREE:	/* more special magic */
	    return slave_load_kdtree(tie, (char *)cursor);
	default:
	    fprintf(stderr, "Unknown load format\n");
	    return 1;
    }

    return -1;
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
