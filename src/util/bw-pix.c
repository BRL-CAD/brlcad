/*                        B W - P I X . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2022 United States Government as represented by
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
/** @file util/bw-pix.c
 *
 * Convert an 8-bit black and white file to a 24-bit
 * color one by replicating each value three times.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/str.h"
#include "bu/opt.h"
#include "bu/mime.h"
#include "bu/exit.h"
#include "icv.h"


void
open_file(FILE **fp, const char *name)
{
    /* check for special names */
    if (BU_STR_EQUAL(name, "-")) {
	*fp = stdin;
    } else if (BU_STR_EQUAL(name, ".")) {
	*fp = fopen("/dev/null", "rb");
    } else if ((*fp = fopen(name, "rb")) == NULL) {
	bu_exit(2, "bw3-pix: Can't open \"%s\"\n", name);
    }
}

int
main(int argc, char **argv)
{
    FILE *in_std = NULL;
    FILE *out_std = NULL;
    struct bu_vls in_fname = BU_VLS_INIT_ZERO;
    struct bu_vls out_fname = BU_VLS_INIT_ZERO;
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    int need_help = 0;
    int uac = 0;
    icv_image_t *img;
    const char *rfile = NULL;
    const char *gfile = NULL;
    const char *bfile = NULL;
    char usage[] = "Usage: bw-pix [-o out_file.pix] [file.bw] [file_green.bw file_blue.bw] [ > out_file.pix]\n";

    bu_setprogname(argv[0]);

    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",        "",         NULL,        &need_help, "Print help and exit");
    BU_OPT(d[1], "o", "output-file", "filename", &bu_opt_vls, &out_fname, "PIX output file name");
    BU_OPT_NULL(d[2]);

    /* Skip first arg */
    argv++; argc--;
    uac = bu_opt_parse(&optparse_msg, argc, (const char **)argv, d);

    if (uac == -1) {
	bu_exit(EXIT_FAILURE, "%s", bu_vls_addr(&optparse_msg));
    }
    bu_vls_free(&optparse_msg);

    if (need_help) {
	bu_vls_free(&out_fname);
	bu_exit(EXIT_SUCCESS, "%s", usage);
    }

    switch (uac) {
	case 0:
	    in_std = stdin;
	    if (!bu_vls_strlen(&out_fname)) {
		out_std = stdout;
	    }
	    break;
	case 1:
	    bu_vls_sprintf(&in_fname, "%s", argv[0]);
	    if (!bu_vls_strlen(&out_fname)) {
		out_std = stdout;
	    }
	    break;
	case 2:
	    if (bu_vls_strlen(&out_fname)) {
		bu_vls_free(&out_fname);
		bu_exit(EXIT_FAILURE, "%s", usage);
	    } else {
		bu_vls_sprintf(&in_fname, "%s", argv[0]);
		bu_vls_sprintf(&out_fname, "%s", argv[1]);
	    }
	    break;
	case 3:
	    rfile = argv[0];
	    gfile = argv[1];
	    bfile = argv[2];
	    if (!bu_vls_strlen(&out_fname)) {
		/* Assume stdio */
		out_std = stdout;
	    }
	    break;
	case 4:
	    if (bu_vls_strlen(&out_fname)) {
		bu_vls_free(&out_fname);
		bu_exit(EXIT_FAILURE, "%s", usage);
	    } else {
		rfile = argv[0];
		gfile = argv[1];
		bfile = argv[2];
		bu_vls_sprintf(&out_fname, "%s", argv[3]);
	    }
	    break;
	default:
	    break;
    }

    /* Don't do stdin/stdio if we've got the isatty condition */
    if (in_std && isatty(fileno(in_std))) bu_exit(EXIT_FAILURE, "%s", usage);
    if (out_std && isatty(fileno(out_std))) bu_exit(EXIT_FAILURE, "%s", usage);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    setmode(fileno(stderr), O_BINARY);

    /* Handle streaming cases locally, without involving libicv */
    if ((in_std || out_std) && !rfile) {
	unsigned char ibuf[1024], obuf[3*1024];
	size_t in, out, num;
	if (bu_vls_strlen(&in_fname)) {
	    if (in_std)	bu_exit(EXIT_FAILURE, "Tried to read input file and stdin at the same time");
	    if ((in_std = fopen(bu_vls_addr(&in_fname), "rb")) == NULL) {
		bu_exit(EXIT_FAILURE, "bw-pix: can't open \"%s\"\n", bu_vls_addr(&in_fname));
	    }
	}
	if (bu_vls_strlen(&out_fname)) {
	    if (out_std) bu_exit(EXIT_FAILURE, "Tried to write to file and stdout at the same time");
	    if ((out_std = fopen(bu_vls_addr(&out_fname), "wb")) == NULL) {
		bu_exit(EXIT_FAILURE, "bw-pix: can't open \"%s\"\n", bu_vls_addr(&out_fname));
	    }
	}
	while ((num = fread(ibuf, sizeof(char), 1024, in_std)) > 0) {
	    size_t ret;
	    for (in = out = 0; in < num; in++, out += 3) {
		obuf[out] = ibuf[in];
		obuf[out+1] = ibuf[in];
		obuf[out+2] = ibuf[in];
	    }
	    ret = fwrite(obuf, sizeof(char), 3*num, out_std);
	    if (ret == 0) {
		perror("fwrite");
		break;
	    }
	}
	if (bu_vls_strlen(&in_fname)) fclose(in_std);
	if (bu_vls_strlen(&out_fname)) fclose(out_std);
	return 0;
    }

    /* If we need to merge the files, handle that here as well */
    if (rfile && gfile && bfile) {
	FILE *rfp, *bfp, *gfp;
	if (bu_vls_strlen(&out_fname)) {
	    if (out_std) bu_exit(EXIT_FAILURE, "Tried to write to file and stdout at the same time");
	    if ((out_std = fopen(bu_vls_addr(&out_fname), "wb")) == NULL) {
		bu_exit(EXIT_FAILURE, "bw-pix: can't open \"%s\"\n", bu_vls_addr(&out_fname));
	    }
	}
	open_file(&rfp, rfile);
	open_file(&gfp, gfile);
	open_file(&bfp, bfile);

	while (1) {
	    unsigned char obuf[3*1024];
	    unsigned char red[1024], green[1024], blue[1024];
	    unsigned char *obufp;
	    int nr, ng, nb, num, i;
	    nr = fread(red, sizeof(char), 1024, rfp);
	    ng = fread(green, sizeof(char), 1024, gfp);
	    nb = fread(blue, sizeof(char), 1024, bfp);
	    if (nr <= 0 && ng <= 0 && nb <= 0)
		break;

	    /* find max */
	    num = (nr > ng) ? nr : ng;
	    if (nb > num) num = nb;
	    if (nr < num)
		memset((char *)&red[nr], 0, num-nr);
	    if (ng < num)
		memset((char *)&green[ng], 0, num-ng);
	    if (nb < num)
		memset((char *)&blue[nb], 0, num-nb);

	    obufp = &obuf[0];
	    for (i = 0; i < num; i++) {
		*obufp++ = red[i];
		*obufp++ = green[i];
		*obufp++ = blue[i];
	    }
	    num = fwrite(obuf, sizeof(char), num*3, out_std);
	    if (num <= 0) {
		perror("fwrite");
		break;
	    }
	}

	return 0;
    }

    /* If we've got the "normal" situation, use libicv */

    img = icv_read(bu_vls_addr(&in_fname), BU_MIME_IMAGE_BW, 0, 0);
    if (img == NULL)
	return 1;
    icv_gray2rgb(img);
    icv_write(img, bu_vls_addr(&out_fname), BU_MIME_IMAGE_PIX);
    icv_destroy(img);
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
