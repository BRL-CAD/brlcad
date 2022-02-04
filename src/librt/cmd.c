/*                           C M D . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup ray */
/** @{ */
/** @file librt/cmd.c
 *
 * Read and parse a viewpoint-control command stream.
 *
 * This module is intended to be common to all programs which read
 * this type of command stream; the routines to handle the various
 * keywords should go in application-specific modules.
 *
 */
/** @} */

#include "common.h"

#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "raytrace.h"

char *
rt_read_cmd(register FILE *fp)
{
    register int c;
    register char *buf;
    register int curpos;
    register int curlen;

    curpos = 0;
    curlen = 400;
    buf = (char *)bu_malloc(curlen, "rt_read_cmd command buffer");

    do {
	c = fgetc(fp);
	if (c == EOF) {
	    c = '\0';
	} else if (c == '#') {
	    /* All comments run to the end of the line */
	    while ((c = fgetc(fp)) != EOF && c != '\n')
		;
	    continue;
	} else if (c == '\n') {
	    c = ' ';
	} else if (c == ';') {
	    c = '\0';
	} else if (c == '\\') {
	    /* Backslash takes next character literally.
	     * EOF detection here is not a problem, next
	     * pass will detect it.
	     */
	    c = fgetc(fp);
	}
	if (c != '\0' && curpos == 0 && isspace(c)) {
	    /* Dispose of leading white space.
	     * Necessary to slurp up what newlines turn into.
	     */
	    continue;
	}
	if (curpos >= curlen) {
	    curlen *= 2;
	    buf = (char *)bu_realloc(buf, curlen, "rt_read_cmd command buffer");
	}
	buf[curpos++] = c;
    } while (c != '\0');
    if (curpos <= 1) {
	bu_free(buf, "rt_read_cmd command buffer (EOF)");
	return (char *)0;		/* EOF */
    }
    return buf;				/* OK */
}


#define MAXWORDS 4096	/* Max # of args per command */


int
rt_do_cmd(struct rt_i *rtip, const char *ilp, register const struct command_tab *tp)
/* FUTURE:  for globbing */


{
    register int nwords;			/* number of words seen */
    char *cmd_args[MAXWORDS+1];	/* array of ptrs to args */
    char *lp;
    int retval;

    if (rtip)
	RT_CK_RTI(rtip);

    if (ilp[0] == '{') {
	int tcl_argc;
	const char **tcl_argv;
	if(bu_argv_from_tcl_list(ilp, &tcl_argc, &tcl_argv) || tcl_argc != 1) {
	    bu_log("rt_do_cmd:  invalid input %s\n", ilp);
	    return -1; /* Looked like a tcl list, but apparently not */
	} else {
	    lp = bu_strdup(tcl_argv[0]);
	}
    } else {
	lp = bu_strdup(ilp);
    }

    nwords = bu_argv_from_string(cmd_args, MAXWORDS, lp);
    if (nwords <= 0)
	return 0;	/* No command to process */


    for (; tp->ct_cmd != (char *)0; tp++) {
	if (cmd_args[0][0] != tp->ct_cmd[0] ||
	    /* the length of "n" is not significant, just needs to be big enough */
	   bu_strncmp(cmd_args[0], tp->ct_cmd, MAXWORDS) != 0)
	    continue;
	if ((nwords >= tp->ct_min)
	    && ((tp->ct_max < 0) || (nwords <= tp->ct_max)))
	{
	    retval = tp->ct_func(nwords, (const char **)cmd_args);
	    bu_free(lp, "rt_do_cmd lp");
	    return retval;
	}
	bu_log("rt_do_cmd Usage: %s %s\n\t%s\n",
	       tp->ct_cmd, tp->ct_parms, tp->ct_comment);
	bu_free(lp, "rt_do_cmd lp");
	return -1;		/* ERROR */
    }
    bu_log("rt_do_cmd(%s):  command not found\n", cmd_args[0]);
    bu_free(lp, "rt_do_cmd lp");
    return -1;			/* ERROR */
}

/* Note - see attr.cpp for the rt_cmd_attr implementation */

int
rt_cmd_put(struct bu_vls *msg, struct db_i *dbip, int argc, const char **argv)
{
    int ret = BRLCAD_ERROR;

    if (dbip == DBI_NULL || dbip->dbi_wdbp == RT_WDB_NULL || argc < 3 || !argv)
	return ret;

    if (dbip->dbi_read_only) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_put: database is read-only\n");
	return ret;
    }

    if (!BU_STR_EQUAL(argv[0], "put")) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_put: incorrect command found: %s\n", argv[0]);
	return ret;
    }


    if (db_lookup(dbip, argv[1], LOOKUP_QUIET) != RT_DIR_NULL) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_put: %s already exists", argv[1]);
	return ret;
    }

     int i;
     char type[16];
     for (i = 0; argv[2][i] != 0 && i < 15; i++) {
	 type[i] = isupper((int)argv[2][i]) ? tolower((int)argv[2][i]) : argv[2][i];
     }
     type[i] = 0;

     const struct rt_functab *ftp = rt_get_functab_by_label(type);
     if (ftp == NULL) {
	 if (msg)
	     bu_vls_printf(msg, "rt_cmd_put: %s is an unknown object type.", type);
	 return ret;
     }

     RT_CK_FUNCTAB(ftp);

     struct rt_db_internal intern;
     RT_DB_INTERNAL_INIT(&intern);

     if (ftp->ft_make) {
	 ftp->ft_make(ftp, &intern);
     } else {
	 rt_generic_make(ftp, &intern);
     }

     if (!ftp->ft_adjust || ftp->ft_adjust(msg, &intern, argc-3, argv+3) & BRLCAD_ERROR) {
	 rt_db_free_internal(&intern);
	 return ret;
     }

     if (wdb_put_internal(dbip->dbi_wdbp, argv[1], &intern, 1.0) < 0) {
	 if (msg)
	     bu_vls_printf(msg, "rt_cmd_put: wdb_put_internal(%s)", argv[1]);
	 rt_db_free_internal(&intern);
	 return ret;
     }

     rt_db_free_internal(&intern);

     return BRLCAD_OK;
}

int
rt_cmd_title(struct bu_vls *msg, struct db_i *dbip, int argc, const char **argv)
{
    int ret = BRLCAD_ERROR;

    if (dbip == DBI_NULL || dbip->dbi_wdbp == RT_WDB_NULL || argc < 2 || !argv)
	return ret;

    if (dbip->dbi_read_only) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_title: database is read-only\n");
	return ret;
    }

    if (!BU_STR_EQUAL(argv[0], "title")) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_title: incorrect command found: %s\n", argv[0]);
	return ret;
    }


    return BRLCAD_OK;

}

int
rt_cmd_units(struct bu_vls *msg, struct db_i *dbip, int argc, const char **argv)
{
    int ret = BRLCAD_ERROR;

    if (dbip == DBI_NULL || dbip->dbi_wdbp == RT_WDB_NULL || argc < 2 || !argv)
	return ret;

    if (dbip->dbi_read_only) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_units: database is read-only\n");
	return ret;
    }

    if (!BU_STR_EQUAL(argv[0], "units")) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_units: incorrect command found: %s\n", argv[0]);
	return ret;
    }


    return BRLCAD_OK;

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
