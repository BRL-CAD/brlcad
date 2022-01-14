/*                   B O O L E A N I Z E . C
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

#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#include "bu.h"

// TODO - not portable to linux yet
#if 0
#undef HAVE_FMEMOPEN
#define HAVE_FUNOPEN 1


#if !defined(HAVE_FMEMOPEN) && defined(HAVE_FUNOPEN)

struct memory {
    size_t size;
    size_t used;
    uint8_t *data;
};


static int
reader(void *buf, char *data, int size)
{
    struct memory *mem = (struct memory *)buf;
    bu_log("reader\n");

    while (mem->size - mem->used < (size_t)size) {
	bu_log("reallocating to %zu\n", mem->size * mem->size);
	mem->data = (uint8_t *)bu_realloc(mem->data, mem->size * 2, "fmemopen");
	memset(mem->data + mem->size, 0, mem->size);
	mem->size *= 2;
    }
    memcpy(data, mem->data + mem->used, size);
    mem->used += size;

    return size;
}


static int
writer(void *buf, const char *data, int size)
{
    struct memory *mem = (struct memory *)buf;
    bu_log("writer\n");

    while (mem->size - mem->used < (size_t)size) {
	bu_log("reallocating to %zu\n", mem->size * mem->size);
	mem->data = (uint8_t *)bu_realloc(mem->data, mem->size * 2, "realloc");
	memset(mem->data + mem->size, 0, mem->size);
	mem->size *= 2;
    }
    memcpy(mem->data + mem->used, data, size);
    mem->used += size;

    return size;
}


static fpos_t
seeker(void *buf, fpos_t offset, int whence)
{
    size_t pos = 0;
    struct memory *mem = (struct memory *)buf;
    bu_log("seeking to %zd\n", (ssize_t)offset);

    switch (whence) {
	case SEEK_SET: {
	    if (offset > 0) {
		pos = (size_t)offset;
	    }
	    break;
	}
	case SEEK_CUR:
	case SEEK_END: {
	    if (offset > 0) {
		pos = mem->used + (size_t)offset;
	    } else if (offset < 0) {
		pos = mem->used - (ssize_t)offset;
	    }
	    break;
	}
	default:
	    return -1;
    }

    if (pos > mem->size) {
	return -1;
    }

    mem->used = pos;
    return (fpos_t)pos;
}


static int
closer(void *buf)
{
    bu_free(buf, "fmemopen");
    return 0;
}


FILE *
fmemopen(void *data, size_t size, const char *UNUSED(mode))
{
    struct memory* mem = (struct memory *)bu_malloc(sizeof(struct memory), "fmemopen");

#  ifndef HAVE_DECL_FUNOPEN
    extern FILE *funopen(const void *cookie,
			 int (*reader)(void *, char *, int),
			 int (*writer)(void *, const char *, int),
			 fpos_t (*seeker)(void *, fpos_t, int),
			 int (*closer)(void *));
#  endif

    mem->size = size;
    mem->used = 0;
    mem->data = (uint8_t *)data;

    return funopen(mem, reader, writer, seeker, closer);
}
#  define HAVE_DECL_FMEMOPEN 1
#endif

#if defined(HAVE_FMEMOPEN) && !defined(HAVE_DECL_FMEMOPEN)
extern FILE *fmemopen(void *, size_t, const char *);
#endif


/* should be at least a few lines with some words */
#define BACKTRACE_MINIMUM 64


static int
go_deeper(int depth, FILE *fp)
{
    if (depth > 1)
	return go_deeper(--depth, fp);

    return bu_backtrace(fp);
}


static int
go_deep(int depth, FILE *fp)
{
    return go_deeper(depth, fp);
}

#endif

int
main(int UNUSED(argc), char **UNUSED(argv))
{
#if 0
    char *buffer = NULL;
    const char *output = NULL;
    FILE *fp = NULL;
    int result;
    size_t size = 0;

    bu_setprogname(av[0]);

    if (argc > 2) {
	fprintf(stderr, "Usage: %s [file]\n", argv[0]);
	return 1;
    }

    if (argc > 1)
	output = argv[1];
    bu_file_delete(output);
    if (bu_file_exists(output, NULL))
	bu_exit(1, "ERROR: backtrace output [%s] already exists and couldn't be deleted\n", output);
    if (!output) {
	bu_log("Using a memory buffer\n");
	buffer = (char *)bu_calloc(MAXPATHLEN, MAXPATHLEN, "memory buffer");
	fp = fmemopen(buffer, MAXPATHLEN * MAXPATHLEN, "w");
	output = "IN_MEMORY_BUFFER";
	fprintf(fp, "this is a test\n");
    } else {
	fp = fopen(output, "w");
    }

    if (!fp)
	bu_exit(2, "ERROR: Unable to open backtrace output [%s]\n", output);

    bu_debug |= BU_DEBUG_BACKTRACE;

    result = go_deep(3, fp);
    fclose(fp);

    /* display the backtrace */
    printf("BEGIN Backtrace {\n");
    if (buffer) {
	printf("%s\n", buffer);
	size = strlen(buffer);
    } else {
	char fgetsbuf[MAXPATHLEN] = {0};
	fp = fopen(output, "r");
	while(bu_fgets(fgetsbuf, MAXPATHLEN, fp)) {
	    printf("%s", fgetsbuf);
	    size += strlen(fgetsbuf);
	}
	fclose(fp);
    }
    printf("} END Backtrace\n");

    /* check the backtrace */
    if (result != 1) {
	printf("bu_backtrace: [FAILED] returned error [%d]\n", result);
	return 1;
    }
    if (!buffer) {
	if (!bu_file_exists(output, NULL)) {
	    printf("bu_backtrace: [FAILED] expecting file\n");
	    return 2;
	}
    }

    if (size < (size_t)BACKTRACE_MINIMUM) {
	printf("bu_backtrace: [FAILED] short trace (%zu < %zu bytes)\n", size, (size_t)BACKTRACE_MINIMUM);
	return 4;
    }
    /* TODO: check trace for main+go_deep+go_deeper */

    bu_free(buffer, "memory buffer");
    printf("bu_backtrace: [PASSED]\n");

    return 0;
#endif
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
