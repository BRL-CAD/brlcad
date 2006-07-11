/*                      C A N O N I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file canonize.c
 *			C A N O N I Z E
 *
 *  Queue an image to a Canon CLC500 via qpr/MDQS
 *
 *  Author -
 *	Lee A. Butler
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "./canon.h"

char cmdbuf[64]="/usr/mdqs/bin/qpr -q "; /* queue name filled in by main() */

/*	Q U E U E
 *
 *	send an image off into the printer queuing system
 */
void
queue(fd)
     FILE *fd;
{
    char img_buffer[8 * 1024];
    int img_bytes, i, args, bytes_read;
    FILE *pfd;

    img_bytes = width * height * 3;

    /* open a pipe to the queuing program */
    if ((pfd = popen(cmdbuf, "w")) == (FILE *)NULL) {
	fprintf(stderr, "%s: ", progname);
	perror(cmdbuf);
	exit(-1);
    }

    if (ipu_debug)
	fprintf(stderr, "pipe open\n");

    fprintf(pfd, "CLC500"); /* magic cookie */

    /* write the command line options out to the data stream */
    for (args=1 ; args < arg_c ; args++) {

	if (!strcmp(arg_v[args], "-a")) {
	    fprintf(pfd, " -w %d -n %d", width, height);
	} if (!strcmp(arg_v[args], "-d")) {
	    args += 2;	/* skip device specification */
	} if (!strcmp(arg_v[args], "-v") ||
	      !strcmp(arg_v[args], "-V")) {
	    continue;	/* skip verbose specification */
	} else {
	    fprintf(pfd, " %s", arg_v[args]);
	}
    }
    fprintf(pfd, "\n");

    if (ipu_debug)
	fprintf(stderr, "args written\n");

    /* write the image down the pipe */
    for( bytes_read = 0 ;
	 bytes_read < img_bytes &&
	     (i = fread(img_buffer, 1, sizeof(img_buffer), fd)) ;
	 bytes_read += i ) {
	fwrite(img_buffer, 1, i, pfd);
    }

    if (ipu_debug)
	fprintf(stderr, "image written\n");

    pclose(pfd);

    if (ipu_debug)
	fprintf(stderr, "image queued\n");
}


int
main(int ac, char *av[])
{
    int arg_ind;
    FILE *fd;


    /* copy the relevant command line options */
    if ((arg_ind = parse_args(ac, av)) >= ac) {
	if (isatty(fileno(stdin)))
	    usage("Specify image on cmd line or redirect from standard input\n");

	if (autosize) usage("Cannot autosize stdin\n");

	strncat(cmdbuf, print_queue, sizeof(cmdbuf)-strlen(cmdbuf)-1);

	queue(stdin);

	return(0);
    }

    strncat(cmdbuf, print_queue, sizeof(cmdbuf)-strlen(cmdbuf)-1);

    for ( ; arg_ind < ac ; arg_ind++) {
	if (autosize &&
	    !fb_common_file_size( &width, &height, av[arg_ind], 3)) {
	    fprintf(stderr,
		    "unable to autosize \"%s\"\n",
		    av[arg_ind]);
	    return(-1);
	}

	if ((fd=fopen(av[arg_ind], "r")) == (FILE *)NULL) {
	    fprintf(stderr, "%s: ", progname);
	    perror(av[arg_ind]);
	    return(-1);
	}
	queue(fd);
	fclose(fd);
    }
    return(0);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
