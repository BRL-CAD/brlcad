/*                        B U F F E R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file util/buffer.c
 *
 * This program is intended to be use as part of a complex pipeline.
 * It serves somewhat the same purpose as the Prolog "cut" operator.
 * Data from stdin is read and buffered until EOF is detected, and
 * then all the buffered data is written to stdout.  An arbitrary
 * amount of data may need to be buffered, so a combination of a 1
 * Mbyte memory buffer and a temporary file is used.
 *
 * The use of read() and write() is prefered over fread() and fwrite()
 * for reasons of efficiency, given the large buffer size in use.
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu.h"


#define SIZE (1024*1024)


char template[512] = {0};
char buf[SIZE] = {0};


int
main(int argc, char *argv[])
{
    FILE *fp;
    long count;
    int tfd;

    if (argc > 1)
	bu_log("%s: unrecognized argument(s)\n", argv[0]);

    if ((count = bu_mread(0, buf, sizeof(buf))) < (long)sizeof(buf)) {
	if (count < 0) {
	    perror("buffer: mem read");
	    exit(1);
	}
	/* Entire input sequence fit into buf */
	if (write(1, buf, count) != count) {
	    perror("buffer: stdout write 1");
	    exit(1);
	}
	exit(0);
    }

    /* Create temporary file to hold data, get r/w file descriptor */
    fp = bu_temp_file(template, 512);
    if ((tfd = fileno(fp)) < 0) {
	perror(template);
	goto err;
    }

    /* Stash away first buffer full */
    if (write(tfd, buf, count) != count) {
	perror("buffer: tmp write1");
	goto err;
    }

    /* Continue reading and writing additional buffer loads to temp file */
    while ((count = bu_mread(0, buf, sizeof(buf))) > 0) {
	if (write(tfd, buf, count) != count) {
	    perror("buffer: tmp write2");
	    goto err;
	}
    }
    if (count < 0) {
	perror("buffer: read");
	goto err;
    }

    /* All input read, regurgitate it all on stdout */
    if (lseek(tfd, 0L, 0) < 0) {
	perror("buffer: lseek");
	goto err;
    }
    while ((count = bu_mread(tfd, buf, sizeof(buf))) > 0) {
	if (write(1, buf, count) != count) {
	    perror("buffer: stdout write 2");
	    goto err;
	}
    }
    if (count < 0) {
	perror("buffer: tmp read");
	goto err;
    }
    (void)unlink(template);
    return 0;

 err:
    if (fp) {
	fclose(fp);
	fp = NULL;
    }
    unlink(template);
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
