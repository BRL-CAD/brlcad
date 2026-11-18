/*                      S H O W S H O T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @file nirt/showshot.c
 *
 * Filter output from NIRT(1) to generate an MGED(1) object
 * representing a shotline through some geometry.
 *
 */

#include "common.h"

#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/str.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "vmath.h"
#include "rt/db4.h"


#define BUF_LEN 128
#define RAY_COLOR "255 255 0"

struct showshot_args {
    int help;
    const char *name;
    fastf_t radius;
};

static int
showshot_name_validate(struct bu_vls *msg, const char *name)
{
    if (!name || strlen(name) + strlen(".s") > NAMESIZE) {
	if (msg)
	    bu_vls_printf(msg,
		"generated solid name may not exceed %d characters\n", NAMESIZE);
	return -1;
    }
    return 0;
}

static const struct bu_arg_option showshot_options[] = {
    BU_ARG_FLAG("h", "help", struct showshot_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_VALUE_VALIDATE("n", "name", struct showshot_args, name,
	BU_ARG_VALUE_STRING, showshot_name_validate, "root",
	"Set the generated object-name root"),
    BU_ARG_VALUE_VALIDATE("r", "radius", struct showshot_args, radius,
	BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate, "distance",
	"Set the positive ray-cylinder radius (default: 1)"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema showshot_schema = BU_ARG_SCHEMA(
    "showshot", "Generate MGED commands representing a NIRT ray",
    showshot_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar showshot_grammar = {
    &showshot_schema, NULL, NULL
};

int
main (int argc, char **argv)
{
    struct showshot_args args = {0, NULL, 1.0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    char buf[BUF_LEN];		/* Contents of current input line */
    char *bp;			/* Pointer into buf */
    char *nlp;			/* Location of newline in buf */
    char rname[BUF_LEN];	/* Name of current region */
    char rayname[BUF_LEN];	/* Name of ray */
    int i;			/* Index into rname */
    int line_nm = 0;		/* Number of current line of input */
    int pid;			/* Process ID for unique group name */
    point_t first_entryp = VINIT_ZERO;	/* Ray's entry into the entire geometry */

    double entryp[3] = VINIT_ZERO;	/* Ray's entry into current region */
    double exitp[3] = VINIT_ZERO;		/* Ray's exit from current region */

    bu_setprogname(argv[0]);
    pid = bu_pid();
    int introspection = bu_arg_introspect(&msg, &showshot_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    if (bu_arg_schema_parse_complete(&showshot_schema, &args, &msg, argc - 1,
	    (const char **)(argv + 1)) < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&showshot_schema, "showshot");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "showshot help");
	return 0;
    }

    /* Construct the names of the objects to add to the database */
    if (args.name)
	bu_strlcpy(rayname, args.name, BUF_LEN);
    else
	snprintf(rayname, BUF_LEN, "ray.%d", pid);

    bu_flog(stdout, "in %s.s sph 0 0 0 1\n", rayname);
    bu_flog(stdout, "r %s.r u %s.s\n", rayname, rayname);
    bu_flog(stdout, "mater %s.r\n\n\n%s\n\n", rayname, RAY_COLOR);
    bu_flog(stdout, "g %s", rayname);

    /* Read the input */
    while (bu_fgets(buf, BUF_LEN, stdin) != NULL) {
	++line_nm;
	bp = buf;
	if ((nlp = strchr(bp, '\n')) != 0)
	    *nlp = '\0';

	/* Skip initial white space */
	while (isspace((int)*bp))
	    ++bp;

	/* Get region name */
	for (i = 0; ! isspace((int)*bp) && (*bp != '\0'); ++i, ++bp)
	    rname[i] = *bp;
	rname[i] = '\0';

	/* Read entry and exit coordinates for this partition */
	if (sscanf(bp, "%lf%lf%lf%lf%lf%lf",
		   &entryp[X], &entryp[Y], &entryp[Z],
		   &exitp[X], &exitp[Y], &exitp[Z]) != 6) {
	    bu_exit(1, "Illegal data on line %d: '%s'\n", line_nm, bp);
	}

	bu_flog(stdout, " %s", rname);

	if (line_nm == 1) {
	    VMOVE(first_entryp, entryp);
	}
    }
    if (! feof(stdin)) {
	bu_exit(1, "Error from bu_fgets().  This shouldn't happen");
    }

    bu_flog(stdout, "\nkill %s.s\nin %s.s rcc\n\t%f %f %f\n\t%f %f %f\n\t%f\n",
	   rayname, rayname,
	   first_entryp[X], first_entryp[Y], first_entryp[Z],
	   exitp[X] - first_entryp[X],
	   exitp[Y] - first_entryp[Y],
	   exitp[Z] - first_entryp[Z],
	   args.radius);
    bu_flog(stdout, "g %s %s.r\n", rayname, rayname);
    bu_flog(stderr, "Group is '%s'\n", rayname);

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
