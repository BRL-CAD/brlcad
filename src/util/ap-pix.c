/*                        A P - P I X . C
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
/** @file util/ap-pix.c
 *
 * Applicon color ink jet printer to .pix file converter.
 *
 */

#include "common.h"

#include <stdio.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"


struct ap_pix_args {
    int help;
    int verbose;
};

static const struct bu_arg_option ap_pix_options[] = {
    BU_ARG_FLAG("h", "help", struct ap_pix_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct ap_pix_args, verbose,
	"Report each output scanline"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand ap_pix_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 1, 1,
	"Applicon input file", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema ap_pix_schema = BU_ARG_SCHEMA(
    "ap-pix", "Convert an Applicon image to a PIX stream",
    ap_pix_options, ap_pix_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar ap_pix_grammar = {
    &ap_pix_schema, NULL, NULL
};


int
main(int argc, char **argv)
{
    static const int separation_records = 50;

    /* Dots are least most significant bit first in increasing index */
    struct app_record {
	unsigned char ml[432];
	unsigned char yl[432];
	unsigned char cl[432];
    } magline, yelline, cyaline;

    struct {
	unsigned char red, green, blue;
    } out;

    FILE *magfp, *yelfp, *cyafp;

    int i, bit;
    int line;
    struct ap_pix_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &ap_pix_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&ap_pix_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&ap_pix_schema, "ap-pix");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "ap-pix help");
	return 0;
    }

    setmode(fileno(stdout), O_BINARY);

    const char *input_path = argv[operand_index + 1];
    magfp = fopen(input_path, "rb");
    if (!magfp)
	bu_exit(2, "ap-pix: can't open \"%s\"\n", input_path);
    yelfp = fopen(input_path, "rb");
    if (!yelfp)
	bu_exit(2, "ap-pix: can't open \"%s\"\n", input_path);
    cyafp = fopen(input_path, "rb");
    if (!cyafp)
	bu_exit(2, "ap-pix: can't open \"%s\"\n", input_path);
    if (bu_fseek(yelfp, separation_records * sizeof(yelline), SEEK_SET) != 0 ||
	bu_fseek(cyafp, 2 * separation_records * sizeof(cyaline), SEEK_SET) != 0)
	bu_exit(1, "ap-pix: seek failure\n");

    line = 0;
    size_t cyan_bytes;
    while ((cyan_bytes = fread(&cyaline, 1, sizeof(cyaline), cyafp)) == sizeof(cyaline)) {
	if (fread(&magline, sizeof(magline), 1, magfp) != 1 ||
	    fread(&yelline, sizeof(yelline), 1, yelfp) != 1)
	    bu_exit(1, "ap-pix: incomplete color separation data\n");

	line++;

	for (i = 0; i < 432; i++) {
	    for (bit = 7; bit >= 0; bit--) {
		out.red = ((cyaline.cl[i]>>bit)&1) ? 0 : 255;
		out.green = ((magline.ml[i]>>bit)&1) ? 0 : 255;
		out.blue = ((yelline.yl[i]>>bit)&1) ? 0 : 255;
		if (fwrite(&out, sizeof(out), 1, stdout) != 1)
		    bu_exit(1, "ap-pix: write failure\n");
	    }
	}
	if (args.verbose)
	    bu_log("wrote line %d\n", line);
    }
    if (cyan_bytes != 0)
	bu_exit(1, "ap-pix: incomplete cyan separation record\n");
    if (ferror(cyafp))
	bu_exit(1, "ap-pix: read failure\n");
    int close_failed = 0;
    if (fclose(magfp) == EOF)
	close_failed = 1;
    if (fclose(yelfp) == EOF)
	close_failed = 1;
    if (fclose(cyafp) == EOF)
	close_failed = 1;
    if (close_failed)
	bu_exit(1, "ap-pix: input close failure\n");
    if (fflush(stdout) == EOF)
	bu_exit(1, "ap-pix: write failure\n");
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
