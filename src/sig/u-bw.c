/*                          U - B W . C
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
/** @file u-bw.c
 *
 * convert unsigned shorts to unsigned char.
 *
 */

#include "common.h"

#include "bio.h"


unsigned short ibuf[512];
unsigned char obuf[512];

int main(int ac, char **av)
{
    size_t num, i;

    if (isatty(fileno(stdin)) || isatty(fileno(stdout))) {
	(void)fprintf(stderr, "Usage: %s < u_shorts > bwfile\n",
		      *av);
	return -1;
    }

    if (ac > 1 && *av[1] == '-' && *av[2] == 'l')
	while ((num = fread(&ibuf[0], sizeof(*ibuf), 512, stdin)) > 0 ) {
	    for (i=0; i < num; i++)
		obuf[i] = (unsigned char)ibuf[i];

	    if (fwrite(&obuf[0], sizeof(*obuf), num, stdout)!=num) {
		(void)fprintf(stderr, "%s: error writing output\n", *av);
		return -1;
	    }
	}
    else
	while ((num = fread(&ibuf[0], sizeof(*ibuf), 512, stdin)) > 0 ) {
	    for (i=0; i < num; i++)
		obuf[i] = (unsigned char)(ibuf[i] >> 8);

	    if (fwrite(&obuf[0], sizeof(*obuf), num, stdout)!=num) {
		(void)fprintf(stderr, "%s: error writing output\n", *av);
		return -1;
	    }
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
