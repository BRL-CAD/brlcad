/*                      C A N O N I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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
 *
 */
/** @file canonize.c
 *
 *  Queue an image to a Canon CLC500 via qpr/MDQS
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "fb.h"
#include "./canon.h"

char cmdbuf[64]="/usr/mdqs/bin/qpr -q "; /* queue name filled in by main() */

/*	Q U E U E
 *
 *	send an image off into the printer queuing system
 */
void
queue(fp)
    FILE *fp;
{
    char img_buffer[8 * 1024];
    int img_bytes, i, args, bytes_read, bytes_written;
    FILE *pfp;

    img_bytes = width * height * 3;

    /* open a pipe to the queuing program */
    if ((pfp = popen(cmdbuf, "w")) == (FILE *)NULL) {
	fprintf(stderr, "%s: ", progname);
	perror(cmdbuf);
	bu_exit(-1, NULL);
    }

    if (ipu_debug)
	fprintf(stderr, "pipe open\n");

    fprintf(pfp, "CLC500"); /* magic cookie */

    /* write the command line options out to the data stream */
    for (args=1; args < arg_c; args++) {

	if (BU_STR_EQUAL(arg_v[args], "-a")) {
	    fprintf(pfp, " -w %lu -n %lu", (unsigned long)width, (unsigned long)height);
	} if (BU_STR_EQUAL(arg_v[args], "-d")) {
	    args += 2;	/* skip device specification */
	} if (BU_STR_EQUAL(arg_v[args], "-v") ||
	      BU_STR_EQUAL(arg_v[args], "-V")) {
	    continue;	/* skip verbose specification */
	} else {
	    fprintf(pfp, " %s", arg_v[args]);
	}
    }
    fprintf(pfp, "\n");

    if (ipu_debug)
	fprintf(stderr, "args written\n");

    /* write the image down the pipe */
    for (bytes_read = 0, bytes_written=0; bytes_read < img_bytes && (i = fread(img_buffer, 1, sizeof(img_buffer), fp)); bytes_read += i) {
	bytes_written += fwrite(img_buffer, 1, i, pfp);
    }

    if (ipu_debug)
	fprintf(stderr, "image written (%d bytes)\n", bytes_written);

    pclose(pfp);

    if (ipu_debug)
	fprintf(stderr, "image queued\n");
}


int
main(int ac, char *av[])
{
    int arg_ind;
    FILE *fp;


    /* copy the relevant command line options */
    if ((arg_ind = parse_args(ac, av)) >= ac) {
	if (isatty(fileno(stdin)))
	    usage("Specify image on cmd line or redirect from standard input\n");

	if (autosize) usage("Cannot autosize stdin\n");

	bu_strlcat(cmdbuf, print_queue, sizeof(cmdbuf));
	cmdbuf[sizeof(cmdbuf)-1] = '\0';

	queue(stdin);

	return 0;
    }

    bu_strlcat(cmdbuf, print_queue, sizeof(cmdbuf));
    cmdbuf[sizeof(cmdbuf)-1] = '\0';

    for (; arg_ind < ac; arg_ind++) {
	if (autosize &&
	    !fb_common_file_size(&width, &height, av[arg_ind], 3)) {
	    fprintf(stderr,
		    "unable to autosize \"%s\"\n",
		    av[arg_ind]);
	    return -1;
	}

	if ((fp=fopen(av[arg_ind], "rb")) == (FILE *)NULL) {
	    fprintf(stderr, "%s: ", progname);
	    perror(av[arg_ind]);
	    return -1;
	}
	queue(fp);
	fclose(fp);
    }
    return 0;
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
