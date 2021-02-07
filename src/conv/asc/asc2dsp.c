/*                       A S C 2 D S P . C
 * BRL-CAD
 *
 * Copyright (c) 2012-2021 United States Government as represented by
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


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "bnetwork.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/opt.h"


static char usage[] = "\
Usage: asc2dsp file.asc file.dsp\n\n\
Convert an ASCII DSP file to DSP binary form\n\
";

#define BUFSZ 6


static void
output_netshort(char *buf, unsigned *nchars, FILE *fpo)
{
    size_t ret = 0;
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
    int need_help = 0;

    char buf[BUFSZ] = {0};
    unsigned nchars = 0;

    bu_setprogname(argv[0]);

    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",        "",         NULL,        &need_help, "Print help   and exit");
    BU_OPT(d[1], "?", "",            "",         NULL,        &need_help, "");
    BU_OPT_NULL(d[2]);

    /* Skip first arg */
    argv++; argc--;
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    int uac = bu_opt_parse(&optparse_msg, argc, (const char **)argv, d);

    if (uac == -1) {
	bu_exit(EXIT_FAILURE, "%s", bu_vls_addr(&optparse_msg));
    }
    bu_vls_free(&optparse_msg);

    argc = uac;

    if (need_help) {
	bu_exit(EXIT_SUCCESS, "%s", usage);
    }

    if (argc != 2)
	bu_exit(1, "%s", usage);

    fpi = fopen(argv[0], "r");
    if (!fpi)
	perror(argv[0]);

    fpo = fopen(argv[1], "wb");
    if (!fpo)
	perror(argv[1]);
    if (fpi == NULL || fpo == NULL) {
	bu_exit(1, "asc2dsp: can't open files.");
    }

    buf[0] = '\0';
    nchars = 0;

    int cg;
    while ((cg = fgetc(fpi)) != EOF) {
	unsigned char c = (unsigned char) cg;
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

    printf("See output file '%s'.\n", argv[1]);

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
