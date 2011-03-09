/*                          D 2 - C . C
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
/** @file d2-c.c
 *
 * Merge two double files into one complex file.
 *
 */

#include "common.h"

#include <string.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "bu.h"


void
open_file(FILE **fp, char *name)
{
    /* check for special names */
    if (BU_STR_EQUAL(name, "-")) {
	*fp = stdin;
	return;
    } else if (BU_STR_EQUAL(name, ".")) {
	*fp = fopen("/dev/null", "r");
	return;
    }

    if ((*fp = fopen(name, "r")) == NULL) {
	bu_exit(2, "d2-c: Can't open \"%s\"\n", name);
    }
}


int
main(int argc, char *argv[])
{
    int i;
    int nr, ni, num;
    double obuf[2*1024];
    double *obufp;
    FILE *rfp, *ifp;
    double real[1024], imag[1024];
    size_t ret;

    if (argc != 3 || isatty(fileno(stdout))) {
	bu_exit(1, "Usage: d2-c real_file imag_file > complex (- stdin, . skip)\n");
    }

    open_file(&rfp, argv[1]);
    open_file(&ifp, argv[2]);

    while (1) {
	nr = fread(real, sizeof(double), 1024, rfp);
	ni = fread(real, sizeof(double), 1024, ifp);
	if (nr <= 0 && ni <= 0)
	    break;

	/* find max */
	num = (nr > ni) ? nr : ni;
	if (nr < num)
	    memset(&real[nr], 0, (num-nr)*sizeof(double));
	if (ni < num)
	    memset(&imag[ni], 0, (num-ni)*sizeof(double));

	obufp = &obuf[0];
	for (i = 0; i < num; i++) {
	    *obufp++ = real[i];
	    *obufp++ = imag[i];
	}
	ret = fwrite(obuf, sizeof(double), num*2, stdout);
	if (ret != (size_t)(num*2))
	    perror("fwrite");
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
