/*                           C V T . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2026 United States Government as represented by
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
/** @file comgeom/cvt.c
 *
 * This is the mainline for converting COM-GEOM
 * cards to a GED style database.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/arg.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "wdb.h"


/* defined in region.c */
extern void group_init(void);
extern int getregion(void);
extern void region_register(int reg_num, int id, int air, int mat, int los);
extern void group_write(void);

/* defined in read.c */
extern int get_line(char *cp, int buflen, const char *title);

/* defined in solid.c */
extern void trim_trail_spaces(char *cp);
extern int getsolid(void);

struct wmember *wmp;	/* array indexed by region number */

int version = 5;	/* v4 or v5 ? */
int verbose = 0;	/* verbose = print a message on every item read */

char name_it[16];	/* stores argv if it exists and appends it
			   to each name generated.*/

size_t cur_col = 0;

FILE *infp;
struct rt_wdb *outfp;		/* Output file descriptor */

int sol_total, sol_work;	/* total num solids, num solids processed */
int reg_total;

extern void getid(void);

void col_pr(char *str);

struct comgeom_args {
    int help;
    int debug_level;
    const char *input_version;
    char name_suffix[sizeof(name_it)];
};

static int
comgeom_suffix_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    if (!arg || strlen(arg) >= sizeof(name_it)) {
	if (msg)
	    bu_vls_printf(msg, "name suffix must be at most %zu characters\n",
		sizeof(name_it) - 1);
	return -1;
    }
    if (storage)
	bu_strlcpy((char *)storage, arg, sizeof(name_it));
    return 0;
}

static const struct bu_arg_value_keyword comgeom_versions[] = {
    {"1", NULL, "DoE MORSE/RAFFLE format"},
    {"4", NULL, "GIFT COMGEOM version 4"},
    {"5", NULL, "GIFT COMGEOM version 5"},
    {NULL, NULL, NULL}
};

static const struct bu_arg_option comgeom_options[] = {
    BU_ARG_FLAG("h", "help", struct comgeom_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_VALUE("d", "debug-level", struct comgeom_args, debug_level,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "level",
	"Set conversion diagnostic verbosity"),
    BU_ARG_KEYWORD_VALUES("v", "input-version", struct comgeom_args,
	input_version, "version", "Select the COMGEOM input format version",
	comgeom_versions),
    BU_ARG_CUSTOM("s", "name-suffix", struct comgeom_args, name_suffix,
	comgeom_suffix_parse, "suffix",
	"Append a suffix of at most 15 characters to generated names"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand comgeom_operands[] = {
    BU_ARG_OPERAND("input-file", BU_ARG_VALUE_FILE, 1, 1,
	"Input GIFT COMGEOM file", NULL),
    BU_ARG_OPERAND("output.g", BU_ARG_VALUE_FILE, 1, 1,
	"Output BRL-CAD database", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema comgeom_schema = BU_ARG_SCHEMA(
    "comgeom-g", "Convert GIFT COMGEOM geometry to BRL-CAD",
    comgeom_options, comgeom_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar comgeom_grammar = {
    &comgeom_schema, NULL, NULL
};

static int
comgeom_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&comgeom_schema, "comgeom-g");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "comgeom-g help");
    return status;
}


int
main(int argc, char **argv)
{
    struct comgeom_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int i;
    int introspection;
    int operand_index;
    const char *input_file;
    const char *output_file;
    char ctitle[132] = {'\0'};
    char *title = NULL;
    char units[16] = {'\0'};

    bu_setprogname(argv[0]);
    args.input_version = "5";
    introspection = bu_arg_introspect(&msg, &comgeom_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    operand_index = bu_arg_schema_parse_complete(&comgeom_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return comgeom_help(stderr, EXIT_FAILURE);
    }
    bu_vls_free(&msg);
    if (args.help)
	return comgeom_help(stdout, EXIT_SUCCESS);

    version = args.input_version[0] - '0';
    verbose = args.debug_level;
    bu_strlcpy(name_it, args.name_suffix, sizeof(name_it));
    input_file = argv[operand_index + 1];
    output_file = argv[operand_index + 2];

    if (!bu_file_exists(input_file, NULL)) {
	perror(input_file);
	return EXIT_FAILURE;
    }
    infp = fopen(input_file, "rb");
    if (!infp) {
	perror(input_file);
	return EXIT_FAILURE;
    }
    if (bu_file_exists(output_file, NULL))
	bu_log("Warning: appending to existing database\n");
    outfp = wdb_fopen(output_file);
    if (!outfp) {
	perror(output_file);
	return EXIT_FAILURE;
    }

    printf("Reading version %d COMGEOM file\n", version);

    if (verbose) {
	printf("COMGEOM input file (version %d) must have this format:\n", version);
	switch (version) {
	    case 1:
		printf("     1.  title card\n");
		printf("     2.  solid table\n");
		printf("     3.  END\n");
		printf("     4.  region table\n");
		printf("     5.  END\n");
		break;
	    case 4:
		printf("     1.  units & title card\n");
		printf("     2.  solid & region count card\n");
		printf("     3.  solid table\n");
		printf("     4.  region table\n");
		printf("     5.  -1\n");
		printf("     6.  blank\n");
		printf("     7.  region ident table\n\n");
		break;
	    case 5:
		printf("     1.  units & title card\n");
		printf("     2.  solid & region count card\n");
		printf("     3.  solid table\n");
		printf("     4.  region table\n");
		printf("     5.  -1\n");
		printf("     6.  region ident table\n\n");
		break;
	}
    }

    group_init();

    /*
     * Read title card
     */
    if (get_line(ctitle, sizeof(ctitle), "title card") == EOF) {
	printf("Empty input file:  no title record\n");
	return 10;
    }

    title = ctitle+3;
    switch (version) {
	case 1:
	    title = ctitle;
	    bu_strlcpy(units, "in", sizeof(units));
	    break;
	case 4:
	case 5:
	    /* First 2 chars are units */
	    units[0] = ctitle[0];
	    units[1] = ctitle[1];
	    units[2] = '\0';
	    break;
    }

    /* Drop leading blanks in title */
    while (isspace((int)*title))
	title++;
    trim_trail_spaces(title);
    trim_trail_spaces(units);

    /* Convert units to lower case */
    {
	char *cp = units;
	while (*cp) {
	    if (isupper((int)*cp))
		*cp = tolower((int)*cp);
	    cp++;
	}
    }

    printf("Title: %s\n", title);
    printf("Units: %s\n", units);

    /* Before converting any geometry, establish the units conversion
     * factor which all mk_* routines will apply.
     */
    if (mk_conversion(units) < 0) {
	printf("WARNING:  unknown units '%s', using inches\n", units);
	bu_strlcpy(units, "in", sizeof(units));
	(void)mk_conversion(units);
    }

    /* Output the MGED database header */
    if (mk_id_units(outfp, title, units) < 0) {
	printf("Unable to write database ID, units='%s'\n", units);
	return 1;
    }

    /*
     * Read control card, if present
     */
    sol_total = reg_total = 0;
    switch (version) {
	case 1:
	    sol_total = reg_total = 9999;	/* Reads until 'END' rec */
	    break;

	case 4:
	    if (get_line(ctitle, sizeof(ctitle), "control card") == EOF) {
		printf("No control card .... STOP\n");
		return 10;
	    }
	    if (sscanf(ctitle, "%20d%10d", &sol_total, &reg_total) != 2) {
		printf("ctitle sscanf failed .... STOP\n");
		return 10;
	    }
	    break;
	case 5:
	    if (get_line(ctitle, sizeof(ctitle), "control card") == EOF) {
		printf("No control card .... STOP\n");
		return 10;
	    }
	    if (sscanf(ctitle, "%5d%5d", &sol_total, &reg_total) != 2) {
		printf("ctitle sscanf failed .... STOP\n");
		return 10;
	    }
	    break;
    }

    if (verbose) printf("Expecting %d solids, %d regions\n", sol_total, reg_total);


    /*
     * SOLID TABLE
     */
    if (verbose) printf("Primitive table\n");
    sol_work = 0;
    while (sol_work < sol_total) {
	i = getsolid();
	if (i < 0) {
	    printf("error converting primitive %d\n", sol_work);
	    /* Should we abort here? */
	    continue;
	}
	if (i > 0) {
	    printf("\nprocessed %d of %d solids\n\n", sol_work, sol_total);
	    if (sol_work < sol_total && version > 1) {
		printf("some solids are missing, aborting\n");
		return 1;
	    }
	    break;		/* done */
	}
    }

    /* REGION TABLE */

    if (verbose)printf("\nRegion table\n");

    wmp = (struct wmember *)bu_calloc(reg_total+2, sizeof(struct wmember), "wmp");

    for (i = reg_total + 1; i >= 0; i--) {
	BU_LIST_INIT(&wmp[i].l);
    }

    cur_col = 0;
    if (getregion() < 0) {
	return 10;
    }

    if (version == 1) {
	for (i = 1; i < reg_total; i++) {
	    region_register(i, 0, 0, 0, 0);
	}
    } else {
	if (version == 4) {
	    char dummy[88];
	    /* read the blank card (line) */
	    (void)get_line(dummy, sizeof(dummy), "blank card");
	}

	if (verbose) printf("\nRegion ident table\n");
	getid();
    }

    if (verbose) printf("\nGroups\n");
    cur_col = 0;
    group_write();
    if (verbose) printf("\n");

    return EXIT_SUCCESS;
}


void
col_pr(char *str)
{
    if (!str) return;
    printf("%s", str);
    cur_col += strlen(str);
    while (cur_col < 78 && ((cur_col%10) > 0)) {
	putchar(' ');
	cur_col++;
    }
    if (cur_col >= 78) {
	printf("\n");
	cur_col = 0;
    }
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
