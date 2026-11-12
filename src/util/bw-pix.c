/*                        B W - P I X . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
#include "bu/file.h"
#include "bu/arg.h"
#include "bu/str.h"
#include "bu/mime.h"
#include "bu/exit.h"
#include "icv.h"

struct bw_pix_args {
    int help;
    const char *output_file;
};

static const struct bu_arg_option bw_pix_options[] = {
    BU_ARG_FLAG("h", "help", struct bw_pix_args, help, "Print help and exit"),
    BU_ARG_VALUE("o", "output-file", struct bw_pix_args, output_file,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "filename", "PIX output file name"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand bw_pix_operands[] = {
    BU_ARG_OPERAND("file", BU_ARG_VALUE_FILE, 0, 4,
	"One grayscale input, optional output, or three channel inputs and optional output", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema bw_pix_schema = BU_ARG_SCHEMA(
    "bw-pix", "Convert one or three grayscale images to PIX RGB data",
    bw_pix_options, bw_pix_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));


void
open_file(FILE **fp, const char *name)
{
    /* check for special names */
    if (BU_STR_EQUAL(name, "-")) {
	*fp = stdin;
    } else if (BU_STR_EQUAL(name, ".")) {
	*fp = fopen(bu_file_null(), "rb");
    } else if ((*fp = fopen(name, "rb")) == NULL) {
	bu_exit(2, "bw3-pix: Can't open \"%s\"\n", name);
    }
}

static int
duplicate_stdout(const struct bu_vls *out_fname)
{
    int fno = fileno(stdout);

    return (bu_vls_strlen(out_fname) > 0 && fno > 0 && !isatty(fno));
}

static int
write_output(FILE *outfp, const unsigned char *buf, size_t len, int dup_stdout)
{
    if (fwrite(buf, sizeof(char), len, outfp) != len) {
	perror("fwrite");
	return 0;
    }

    if (dup_stdout && fwrite(buf, sizeof(char), len, stdout) != len) {
	perror("fwrite");
	return 0;
    }

    return 1;
}

int
main(int argc, char **argv)
{
    const struct bu_arg_grammar grammar = {&bw_pix_schema, NULL, NULL};
    FILE *in_std = NULL;
    FILE *out_std = NULL;
    struct bu_vls in_fname = BU_VLS_INIT_ZERO;
    struct bu_vls out_fname = BU_VLS_INIT_ZERO;
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct bw_pix_args args = {0};
    int uac = 0;
    icv_image_t *img;
    const char *rfile = NULL;
    const char *gfile = NULL;
    const char *bfile = NULL;
    char usage[] = "Usage: bw-pix [-o out_file.pix] [file.bw] [file_green.bw file_blue.bw] [ > out_file.pix]\n";

    bu_setprogname(argv[0]);

    int schema_status = bu_arg_introspect(&optparse_msg, &grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (schema_status != 0) {
	bu_flog(schema_status > 0 ? stdout : stderr, "%s", bu_vls_cstr(&optparse_msg));
	bu_vls_free(&optparse_msg);
	return schema_status > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    /* Skip first arg */
    argv++; argc--;
    int operand_index = bu_arg_schema_parse_complete(&bw_pix_schema, &args,
	&optparse_msg, argc, (const char **)argv);

    if (operand_index < 0) {
	bu_exit(EXIT_FAILURE, "%s", bu_vls_addr(&optparse_msg));
    }
    bu_vls_free(&optparse_msg);

    if (args.help) {
	bu_vls_free(&out_fname);
	char *help = bu_arg_schema_help(&bw_pix_schema, "bw-pix");
	bu_exit(EXIT_SUCCESS, "%s", help ? help : "");
    }

    argc -= operand_index;
    argv += operand_index;
    uac = argc;
    if (args.output_file)
	bu_vls_sprintf(&out_fname, "%s", args.output_file);

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
	if (!in_std || !out_std)
	    bu_exit(EXIT_FAILURE, "%s", usage);
	while ((num = fread(ibuf, sizeof(char), 1024, in_std)) > 0) {
	    int dup_stdout = duplicate_stdout(&out_fname);
	    for (in = out = 0; in < num; in++, out += 3) {
		obuf[out] = ibuf[in];
		obuf[out+1] = ibuf[in];
		obuf[out+2] = ibuf[in];
	    }
	    if (!write_output(out_std, obuf, 3*num, dup_stdout)) {
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
	if (!out_std)
	    bu_exit(EXIT_FAILURE, "%s", usage);

	while (1) {
	    unsigned char obuf[3*1024];
	    unsigned char red[1024], green[1024], blue[1024];
	    unsigned char *obufp;
	    int dup_stdout = duplicate_stdout(&out_fname);
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
	    if (!write_output(out_std, obuf, (size_t)num*3, dup_stdout)) {
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
    if (duplicate_stdout(&out_fname)) {
	icv_write(img, NULL, BU_MIME_IMAGE_PIX);
    }
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
