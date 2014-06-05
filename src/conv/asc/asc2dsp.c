/*                       A S C 2 D S P . C
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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

/** @file asc2dsp.c
 *
 *  Convert ASCII (decimal integer) cell (point) files to the binary
 *  form of a file in BRL-CAD's DSP (Displacement map) format.  Pixels
 *  are defined by whitespace-delimited, unsigned decimal integers in
 *  the range 0 to 2^16-1 (65535).
 *
 *  The user must ensure that the input file is valid as far as width
 *  x length = number of cells (points).
 *
 *  See dsp(5) for details of the dsp format.
 *
 *  Usage: asc2dsp file.asc file.dsp
 *
 */

#include "common.h"

#include "bu.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "bin.h"


static char usage[] = "\
Usage: asc2dsp file.asc file.dsp\n\n\
Convert an ASCII DSP file to DSP binary form\n\
";

#define BUFSZ 6


static void
output_netshort(char *buf, unsigned *nchars, FILE *fpo)
{
    int ret = 0;
    unsigned long val;
    uint16_t hostshort;
    uint16_t netshort;

    if (!*nchars)
	return;

    if (*nchars > BUFSZ - 1) {
	bu_log("asc2dsp: nchars (%ud) > BUFSZ - 1 (%d)\n", *nchars, BUFSZ - 1);
	bu_exit(1, "asc2dsp: FATAL");
    }

    /* close buffer to use as string */
    buf[*nchars] = '\0';

    /* get the value as a host unsigned short int */
    /* note strtoul should ignore leading zeroes: verified by testing with '011' input */
    val = strtoul(buf, 0, 10);
    if (val > UINT16_MAX) {
	bu_log("asc2dsp: hostshort (%lu) > UINT16_MAX (%d)\n", val, UINT16_MAX);
	bu_exit(1, "asc2dsp: FATAL");
    }

    hostshort = (uint16_t)strtoul(buf, 0, 10);

    /* convert  to network order for the dsp */
    netshort = htons(hostshort);

    /* now output it */
    ret = fwrite(&netshort, sizeof(uint16_t), 1, fpo);
    if (UNLIKELY(ret != 1)) {
       perror("fwrite failed");
       bu_bomb("output_netshort: write error");
    }

    /* prep buffer for next value */
    buf[0] = '\0';
    *nchars = 0;
}

int
main(int argc, char **argv)
{
    FILE *fpi = NULL;
    FILE *fpo = NULL;
    int d;

    char buf[BUFSZ] = {0};
    unsigned nchars = 0;

    if (argc != 3)
	bu_exit(1, "%s", usage);

    fpi = fopen(argv[1], "r");
    if (!fpi)
	perror(argv[1]);

    fpo = fopen(argv[2], "wb");
    if (!fpo)
	perror(argv[2]);
    if (fpi == NULL || fpo == NULL) {
	bu_exit(1, "asc2dsp: can't open files.");
    }

    buf[0] = '\0';
    nchars = 0;

    while ((d = fgetc(fpi)) != EOF) {
      unsigned char c = (unsigned char)d;
      if (isspace(c)) {
	  /* may be end of a chunk of digits indicating need to process buffer */
	  /* there should be nchars > 0 if anything is there */
	  if (nchars) {
	      /* note that the following call resets the buffer and nchars */
	      output_netshort(buf, &nchars, fpo);
	  }
	  continue;
      } else if (c < '0' || c > '9') {
	  /* invalid char--bail */
	  bu_log("asc2dsp: invalid char '%c'\n", c);
	  bu_exit(1, "asc2dsp: FATAL");
      }

      /* copy the 0-9 to the buffer's next spot */
      buf[nchars++] = c;
    }
    fclose(fpi);

    /* there may be data in the buffer at EOF */
    if (nchars) {
	output_netshort(buf, &nchars, fpo);
    }
    fclose(fpo);

    printf("See output file '%s'.\n", argv[2]);

    exit(0);
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
