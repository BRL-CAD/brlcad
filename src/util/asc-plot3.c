/*                     A S C - P L O T 3 . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2014 United States Government as represented by
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
/** @file util/asc-plot3.c
 *
 * Produce UNIX plot commands in PLOT3(5) format from ASCII
 * representation.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "bio.h"

#include "bn/plot3.h"

#include "bu/log.h"
#include "bu/str.h"

#define BUF_LEN 512
#define FP_IN 0
#define FP_OUT 1


static void
printusage (void)
{
    bu_exit(1, "Usage: asc-plot3 [file.in [file.plot3]]\n");
}


static int
check_syntax (char cmd, int needed, int got, int line)
{
    if (got < needed) {
	bu_exit(1, "Too few arguments for '%c' command on line %d\n", cmd, line);
    }
    return 1;
}


int
main (int argc, char **argv)
{
    char *bp;
    char buf[BUF_LEN];
    char sarg[BUF_LEN];
    static char *fm[] = { "r", "w" };
    double darg[6] = {0.0};
    static FILE *fp[2];
    int i;
    int iarg[6] = {0};
    int line_nm;
    int nm_args = 0;


    if (BU_STR_EQUAL(argv[1], "-h") || BU_STR_EQUAL(argv[1], "-?")) {
	printusage();
    }

    /* Handle command-line syntax */
    if (argc > 3) {
	printusage();
    }
    fp[0] = stdin;
    fp[1] = stdout;
    for (i = 0; (i < 2) && (--argc > 0); ++i) {
	if ((**++argv == '-') && (*(*argv + 1) == '\0'))
	    continue;
	if ((fp[i] = fopen(*argv, fm[i])) == NULL) {
	    bu_log("Cannot open file '%s'\n", *argv);
	    printusage();
	}
    }
    if (isatty(fileno(fp[FP_OUT]))) {
	bu_log("asc-plot3: Will not write to a TTY\n");
	printusage();
	return 1;
    }

    for (line_nm = 1; bu_fgets(buf, BUF_LEN - 1, fp[FP_IN]) != NULL; ++line_nm) {
	for (bp = buf; *bp != '\0'; ++bp)
	    ;
	*bp = '\n';
	*(bp + 1) = '\0';
	for (bp = buf; (*bp == ' ') || (*bp == '\t'); ++bp)
	    ;
	if (*bp == '\n')
	    continue;
	if (strchr("aclmnpsCLMNPS", *bp))
	    nm_args = sscanf(bp + 1, "%d%d%d%d%d%d",
			     &iarg[0], &iarg[1], &iarg[2],
			     &iarg[3], &iarg[4], &iarg[5]);
	else if (strchr("ioqrvwxOQVWX", *bp))
	    nm_args = sscanf(bp + 1, "%lf%lf%lf%lf%lf%lf",
			     &darg[0], &darg[1], &darg[2],
			     &darg[3], &darg[4], &darg[5]);
	else if (strchr("ft", *bp))
	    nm_args = sscanf(bp, "%*[^\"]\"%[^\"]\"", sarg);

	switch (*bp) {
	    case '#':
		break;
	    case 'a':
		if (check_syntax(*bp, 6, nm_args, line_nm))
		    pl_arc(fp[FP_OUT], iarg[0], iarg[1], iarg[2], iarg[3],
			   iarg[4], iarg[5]);
		break;
	    case 'c':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pl_circle(fp[FP_OUT], iarg[0], iarg[1], iarg[2]);
		break;
	    case 'l':
		if (check_syntax(*bp, 4, nm_args, line_nm))
		    pl_line(fp[FP_OUT], iarg[0], iarg[1], iarg[2], iarg[3]);
		break;
	    case 'm':
		if (check_syntax(*bp, 2, nm_args, line_nm))
		    pl_move(fp[FP_OUT], iarg[0], iarg[1]);
		break;
	    case 'n':
		if (check_syntax(*bp, 2, nm_args, line_nm))
		    pl_cont(fp[FP_OUT], iarg[0], iarg[1]);
		break;
	    case 'p':
		if (check_syntax(*bp, 2, nm_args, line_nm))
		    pl_point(fp[FP_OUT], iarg[0], iarg[1]);
		break;
	    case 's':
		if (check_syntax(*bp, 4, nm_args, line_nm))
		    pl_space(fp[FP_OUT], iarg[0], iarg[1], iarg[2], iarg[3]);
		break;
	    case 'C':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pl_color(fp[FP_OUT], iarg[0], iarg[1], iarg[2]);
		break;
	    case 'L':
		if (check_syntax(*bp, 6, nm_args, line_nm))
		    pl_3line(fp[FP_OUT], iarg[0], iarg[1], iarg[2], iarg[3],
			     iarg[4], iarg[5]);
		break;
	    case 'M':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pl_3move(fp[FP_OUT], iarg[0], iarg[1], iarg[2]);
		break;
	    case 'N':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pl_3cont(fp[FP_OUT], iarg[0], iarg[1], iarg[2]);
		break;
	    case 'P':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pl_3point(fp[FP_OUT], iarg[0], iarg[1], iarg[2]);
		break;
	    case 'S':
		if (check_syntax(*bp, 6, nm_args, line_nm))
		    pl_3space(fp[FP_OUT], iarg[0], iarg[1], iarg[2], iarg[3],
			      iarg[4], iarg[5]);
		break;
	    case 'i':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pd_circle(fp[FP_OUT], darg[0], darg[1], darg[2]);
		break;
	    case 'o':
		if (check_syntax(*bp, 2, nm_args, line_nm))
		    pd_move(fp[FP_OUT], darg[0], darg[1]);
		break;
	    case 'q':
		if (check_syntax(*bp, 2, nm_args, line_nm))
		    pd_cont(fp[FP_OUT], darg[0], darg[1]);
		break;
	    case 'r':
		if (check_syntax(*bp, 6, nm_args, line_nm))
		    pd_arc(fp[FP_OUT], darg[0], darg[1], darg[2], darg[3],
			   darg[4], darg[5]);
		break;
	    case 'v':
		if (check_syntax(*bp, 4, nm_args, line_nm))
		    pd_line(fp[FP_OUT], darg[0], darg[1], darg[2], darg[3]);
		break;
	    case 'w':
		if (check_syntax(*bp, 4, nm_args, line_nm))
		    pd_space(fp[FP_OUT], darg[0], darg[1], darg[2], darg[3]);
		break;
	    case 'x':
		if (check_syntax(*bp, 2, nm_args, line_nm))
		    pd_point(fp[FP_OUT], darg[0], darg[1]);
		break;
	    case 'O':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pd_3move(fp[FP_OUT], darg[0], darg[1], darg[2]);
		break;
	    case 'Q':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pd_3cont(fp[FP_OUT], darg[0], darg[1], darg[2]);
		break;
	    case 'V':
		if (check_syntax(*bp, 6, nm_args, line_nm))
		    pd_3line(fp[FP_OUT], darg[0], darg[1], darg[2], darg[3],
			     darg[4], darg[5]);
		break;
	    case 'W':
		if (check_syntax(*bp, 6, nm_args, line_nm))
		    pd_3space(fp[FP_OUT], darg[0], darg[1], darg[2], darg[3],
			      darg[4], darg[5]);
		break;
	    case 'X':
		if (check_syntax(*bp, 3, nm_args, line_nm))
		    pd_3point(fp[FP_OUT], darg[0], darg[1], darg[2]);
		break;
	    case 'F':
		pl_flush(fp[FP_OUT]);
		break;
	    case 'e':
		pl_erase(fp[FP_OUT]);
		break;
	    case 'f':
		if (check_syntax(*bp, 1, nm_args, line_nm))
		    pl_linmod(fp[FP_OUT], sarg);
		break;
	    case 't':
		if (check_syntax(*bp, 1, nm_args, line_nm))
		    pl_label(fp[FP_OUT], sarg);
		break;
	    default:
		bu_exit(1, "Unknown PLOT3 command: '%c' (o%o) on line %d\n",
			*bp, *bp, line_nm);
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
