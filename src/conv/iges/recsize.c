/*                       R E C S I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file iges/recsize.c
 *
 * Routines to validate that a file is IGES ASCII and to build a record
 * index for it.
 *
 * According to the spec every IGES record is 80 columns of data.  Real
 * files terminate records with LF, CR-LF, or (rarely) nothing at all, and
 * some non-conforming writers (e.g. Open CASCADE) leave the free-format
 * Start/Global records short of 80 columns rather than padding them.  To
 * read all of these uniformly we do not rely on a fixed on-disk record
 * stride; instead Build_rec_index() records the byte offset of every
 * record so Readrec() can seek to it directly and space-pad the data
 * columns.  reclen is therefore the logical data width (80), not the
 * on-disk byte count.
 *
 */

#include "common.h"

#include <errno.h>

#include "./iges_struct.h"
#include "./iges_extern.h"

#define NCHAR 256 /* Maximum characters to scan for a line terminator */
#define IGES_RECLEN 80 /* IGES records are 80 data columns */

/*
 * Probe the file to confirm it looks like ASCII IGES and return the logical
 * record length (IGES_RECLEN) if so, or 0 for an empty/unreadable file.
 * The file position is rewound to the beginning before returning.  Line
 * terminators (LF, CR-LF, or none) and short, unpadded records are all
 * tolerated; the actual record positions are resolved by Build_rec_index().
 */
int
Recsize(void)
{
    int ch;
    int saw_any = 0;

    if (bu_fseek(fd, 0, 0)) {
	bu_log("Cannot rewind file\n");
	perror("Recsize");
	bu_exit(1, NULL);
    }

    while ((ch = getc(fd)) != EOF) {
	saw_any = 1;
	break;
    }

    if (bu_fseek(fd, 0, 0)) {
	bu_log("Cannot rewind file\n");
	perror("Recsize");
	bu_exit(1, NULL);
    }

    return saw_any ? IGES_RECLEN : 0;
}


/*
 * Scan the whole file once and record the byte offset at which each on-disk
 * record begins into the global rec_offset[] array (nrecords entries).
 * Records are taken to be LF-delimited (a trailing CR is stripped later by
 * Readrec); if the file contains no LF at all it is treated as a stream of
 * fixed IGES_RECLEN-column records.  The file position is rewound before
 * returning.
 */
void
Build_rec_index(void)
{
    b_off_t pos = 0;
    int ch;
    int saw_lf = 0;
    size_t cap = 1024;
    size_t n = 0;

    Free_rec_index();

    if (bu_fseek(fd, 0, 0)) {
	bu_log("Cannot rewind file\n");
	perror("Build_rec_index");
	bu_exit(1, NULL);
    }

    rec_offset = (b_off_t *)bu_malloc(cap * sizeof(b_off_t), "rec_offset");
    rec_offset[n++] = 0;	/* first record begins at the start of the file */

    while ((ch = getc(fd)) != EOF) {
	pos++;
	if (ch == '\n') {
	    int nxt = getc(fd);
	    saw_lf = 1;
	    if (nxt == EOF)
		break;		/* trailing newline, no further record */
	    (void)ungetc(nxt, fd);
	    if (n >= cap) {
		cap *= 2;
		rec_offset = (b_off_t *)bu_realloc(rec_offset, cap * sizeof(b_off_t), "rec_offset");
	    }
	    rec_offset[n++] = pos;	/* next record starts just past the LF */
	}
    }

    if (!saw_lf) {
	/* No line terminators: fixed IGES_RECLEN-column records. */
	b_off_t filelen;
	b_off_t o;

	if (bu_fseek(fd, 0, 2)) {
	    bu_log("Cannot seek to end of file\n");
	    perror("Build_rec_index");
	    bu_exit(1, NULL);
	}
	filelen = bu_ftell(fd);

	n = 0;
	for (o = 0; o + IGES_RECLEN <= filelen; o += IGES_RECLEN) {
	    if (n >= cap) {
		cap *= 2;
		rec_offset = (b_off_t *)bu_realloc(rec_offset, cap * sizeof(b_off_t), "rec_offset");
	    }
	    rec_offset[n++] = o;
	}
	if (n == 0 && filelen > 0)
	    rec_offset[n++] = 0;
    }

    nrecords = n;

    if (bu_fseek(fd, 0, 0)) {
	bu_log("Cannot rewind file\n");
	perror("Build_rec_index");
	bu_exit(1, NULL);
    }
}


/*
 * Release the record index built by Build_rec_index().
 */
void
Free_rec_index(void)
{
    if (rec_offset) {
	bu_free(rec_offset, "rec_offset");
	rec_offset = NULL;
    }
    nrecords = 0;
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
