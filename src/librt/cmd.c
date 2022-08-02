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
#include "bu/units.h"
#include "rt/db4.h"
#include "rt/mater.h"
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

/**
 * Used to create a database record and get it written out to a granule.
 * In some cases, storage will need to be allocated.
 */
static void
_rt_color_putrec(struct bu_vls *msg, struct db_i *dbip, struct mater *mp)
{
    struct directory dir;
    union record rec;

    /* we get here only if database is NOT read-only */

    rec.md.md_id = ID_COLORTAB;
    rec.md.md_low = mp->mt_low;
    rec.md.md_hi = mp->mt_high;
    rec.md.md_r = mp->mt_r;
    rec.md.md_g = mp->mt_g;
    rec.md.md_b = mp->mt_b;

    /* Fake up a directory entry for db_* routines */
    RT_DIR_SET_NAMEP(&dir, "color_putrec");
    dir.d_magic = RT_DIR_MAGIC;
    dir.d_flags = 0;

    if (mp->mt_daddr == MATER_NO_ADDR) {
        /* Need to allocate new database space */
        if (db_alloc(dbip, &dir, 1)) {
	    if (msg)
		bu_vls_printf(msg, "_rt_color_putrec: database alloc error, aborting");
            return;
        }
        mp->mt_daddr = dir.d_addr;
    } else {
        dir.d_addr = mp->mt_daddr;
        dir.d_len = 1;
    }

    if (db_put(dbip, &dir, &rec, 0, 1)) {
	if (msg)
	    bu_vls_printf(msg, "_rt_color_putrec: database write error, aborting");
        return;
    }
}

/**
 * Used to release database resources occupied by a material record.
 */
static void
_rt_color_zaprec(struct bu_vls *msg, struct db_i *dbip, struct mater *mp)
{
    struct directory dir;

    /* we get here only if database is NOT read-only */
    if (mp->mt_daddr == MATER_NO_ADDR)
        return;

    dir.d_magic = RT_DIR_MAGIC;
    RT_DIR_SET_NAMEP(&dir, "color_zaprec");
    dir.d_len = 1;
    dir.d_addr = mp->mt_daddr;
    dir.d_flags = 0;

    if (db_delete(dbip, &dir) != 0) {
	if (msg)
	    bu_vls_printf(msg, "_rt_color_zaprec: database delete error, aborting");
        return;
    }
    mp->mt_daddr = MATER_NO_ADDR;
}

int
rt_cmd_color(struct bu_vls *msg, struct db_i *dbip, int argc, const char **argv)
{
    if (dbip == DBI_NULL || dbip->dbi_wdbp == RT_WDB_NULL || !argv)
	return BRLCAD_ERROR;

    if (dbip->dbi_read_only) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_color: database is read-only\n");
	return BRLCAD_ERROR;
    }

    if (argc != 6 && argc != 2)
	return BRLCAD_ERROR | BRLCAD_HELP;

    if (!BU_STR_EQUAL(argv[0], "color")) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_color: incorrect command found: %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    /* The -e option is not supported by this portion of the command
     * implementation - see LIBGED's color command for how to handle the -e
     * (edcolor) option at a higher level. */
    if (argc == 2) {
        if (argv[1][0] == '-' && argv[1][1] == 'e' && argv[1][2] == '\0') {
	    if (msg)
		bu_vls_printf(msg, "rt_cmd_color: -e option is unsupported\n");
	    return BRLCAD_ERROR;
	} else {
	    if (msg)
		bu_vls_printf(msg, "rt_cmd_color: unknown option: %s\n", argv[1]);
            return BRLCAD_ERROR | BRLCAD_HELP;
        }
    }

    if (db_version(dbip) < 5) {
        /* Delete all color records from the database */
        struct mater *mp = rt_material_head();
	struct mater *newp;
	struct mater *next_mater;
        while (mp != MATER_NULL) {
            next_mater = mp->mt_forw;
            _rt_color_zaprec(msg, dbip, mp);
            mp = next_mater;
        }

        /* construct the new color record */
        BU_ALLOC(newp, struct mater);
        newp->mt_low = atoi(argv[1]);
        newp->mt_high = atoi(argv[2]);
        newp->mt_r = atoi(argv[3]);
        newp->mt_g = atoi(argv[4]);
        newp->mt_b = atoi(argv[5]);
        newp->mt_daddr = MATER_NO_ADDR;         /* not in database yet */

        /* Insert new color record in the in-memory list */
        rt_insert_color(newp);

        /* Write new color records for all colors in the list */
        mp = rt_material_head();
        while (mp != MATER_NULL) {
            next_mater = mp->mt_forw;
            _rt_color_putrec(msg, dbip, mp);
            mp = next_mater;
        }
    } else {
        struct bu_vls colors = BU_VLS_INIT_ZERO;

        /* construct the new color record */
	struct mater *newp;
        BU_ALLOC(newp, struct mater);
        newp->mt_low = atoi(argv[1]);
        newp->mt_high = atoi(argv[2]);
        newp->mt_r = atoi(argv[3]);
        newp->mt_g = atoi(argv[4]);
        newp->mt_b = atoi(argv[5]);
        newp->mt_daddr = MATER_NO_ADDR;         /* not in database yet */

        /* Insert new color record in the in-memory list */
        rt_insert_color(newp);
        /*
         * Gather color records from the in-memory list to build
         * the _GLOBAL objects regionid_colortable attribute.
         */
        rt_vls_color_map(&colors);

        db5_update_attribute("_GLOBAL", "regionid_colortable", bu_vls_addr(&colors), dbip);
        bu_vls_free(&colors);
    }

    return BRLCAD_OK;
}

int
rt_cmd_put(struct bu_vls *msg, struct db_i *dbip, int argc, const char **argv)
{
    if (dbip == DBI_NULL || dbip->dbi_wdbp == RT_WDB_NULL || argc < 3 || !argv)
	return BRLCAD_ERROR;

    if (dbip->dbi_read_only) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_put: database is read-only\n");
	return BRLCAD_ERROR;
    }

    if (!BU_STR_EQUAL(argv[0], "put")) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_put: incorrect command found: %s\n", argv[0]);
	return BRLCAD_ERROR;
    }


    if (db_lookup(dbip, argv[1], LOOKUP_QUIET) != RT_DIR_NULL) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_put: %s already exists", argv[1]);
	return BRLCAD_ERROR;
    }

     int i;
     char type[16] = {0};
     for (i = 0; argv[2][i] != 0 && i < 15; i++) {
	 type[i] = isupper((int)argv[2][i]) ? tolower((int)argv[2][i]) : argv[2][i];
     }
     type[i] = 0;

     const struct rt_functab *ftp = rt_get_functab_by_label(type);
     if (ftp == NULL) {
	 if (msg)
	     bu_vls_printf(msg, "rt_cmd_put: %s is an unknown object type.", type);
	 return BRLCAD_ERROR;
     }

     RT_CK_FUNCTAB(ftp);

     struct rt_db_internal intern;
     RT_DB_INTERNAL_INIT(&intern);

     if (ftp->ft_make) {
	 ftp->ft_make(ftp, &intern);
     } else {
	 rt_generic_make(ftp, &intern);
     }

     /* The LIBBU struct parsing commands require a non-NULL logstr to work (??) -
      * if the caller hasn't provided us a logging buffer, make a temporary one */
     struct bu_vls *amsg;
     struct bu_vls tmpmsg = BU_VLS_INIT_ZERO;
     amsg = (msg) ? msg : &tmpmsg;

     if (!ftp->ft_adjust || ftp->ft_adjust(amsg, &intern, argc-3, argv+3) & BRLCAD_ERROR) {
	 if (msg)
	     bu_vls_printf(msg, "rt_cmd_put: error calling ft_adjust");
	 bu_vls_free(&tmpmsg);
	 rt_db_free_internal(&intern);
	 return BRLCAD_ERROR;
     }
     bu_vls_free(&tmpmsg);

     if (wdb_put_internal(dbip->dbi_wdbp, argv[1], &intern, 1.0) < 0) {
	 if (msg)
	     bu_vls_printf(msg, "rt_cmd_put: wdb_put_internal(%s)", argv[1]);
	 rt_db_free_internal(&intern);
	 return BRLCAD_ERROR;
     }

     rt_db_free_internal(&intern);

     return BRLCAD_OK;
}

int
rt_cmd_title(struct bu_vls *msg, struct db_i *dbip, int argc, const char **argv)
{
    if (dbip == DBI_NULL || dbip->dbi_wdbp == RT_WDB_NULL || argc < 1 || !argv)
	return BRLCAD_ERROR;

    if (!BU_STR_EQUAL(argv[0], "title")) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_title: incorrect command found: %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    /* get title */
    if (argc == 1) {
	if (msg)
	    bu_vls_printf(msg, "%s", dbip->dbi_title);
	return BRLCAD_OK;
    }

    /* If doing anything other than reporting, we need to be able to write */
    if (dbip->dbi_read_only) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_title: database is read-only\n");
	return BRLCAD_ERROR;
    }

    /* set title */
    struct bu_vls title = BU_VLS_INIT_ZERO;
    bu_vls_from_argv(&title, argc-1, (const char **)argv+1);
    if (db_update_ident(dbip, bu_vls_addr(&title), dbip->dbi_local2base) < 0) {
        bu_vls_free(&title);
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_title: unable to change database title");
        return BRLCAD_ERROR;
    }
    bu_vls_free(&title);

    return BRLCAD_OK;
}

int
rt_cmd_units(struct bu_vls *msg, struct db_i *dbip, int argc, const char **argv)
{
    int sflag = 0;

    if (dbip == DBI_NULL || dbip->dbi_wdbp == RT_WDB_NULL || argc < 1 || argc > 2 || !argv)
	return BRLCAD_ERROR;

    if (!BU_STR_EQUAL(argv[0], "units")) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_units: incorrect command found: %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (argc == 2) {
        if (BU_STR_EQUAL(argv[1], "-s")) {
            --argc;
            ++argv;

            sflag = 1;
        } else if (BU_STR_EQUAL(argv[1], "-t")) {
            struct bu_vls *vlsp = bu_units_strings_vls();

	    if (msg)
		bu_vls_printf(msg, "%s", bu_vls_cstr(vlsp));
            bu_vls_free(vlsp);
            bu_free(vlsp, "rt_cmd_units: vlsp");

            return BRLCAD_OK;
        }
    }

    /* Get units */
    if (argc == 1) {
	const char *str = bu_units_string(dbip->dbi_local2base);
	if (!str) str = "Unknown_unit";

	if (msg) {
	    if (sflag)
		bu_vls_printf(msg, "%s", str);
	    else
		bu_vls_printf(msg, "You are editing in '%s'.  1 %s = %g mm \n",
			str, str, dbip->dbi_local2base);
	}

	return BRLCAD_OK;
    }

    if (dbip->dbi_read_only) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_units: database is read-only\n");
	return BRLCAD_ERROR;
    }

    double loc2mm;
    /* Set units */
    /* Allow inputs of the form "25cm" or "3ft" */
    if ((loc2mm = bu_mm_value(argv[1])) <= 0) {
	if (msg)
	    bu_vls_printf(msg,
		    "%s: unrecognized unit\nvalid units: <um|mm|cm|m|km|in|ft|yd|mi>\n",
		    argv[1]);
	return BRLCAD_ERROR;
    }

    if (db_update_ident(dbip, dbip->dbi_title, loc2mm) < 0) {
	if (msg)
	    bu_vls_printf(msg, "rt_cmd_units:  warning - unable to stash working units into database\n");
    }

    dbip->dbi_local2base = loc2mm;
    dbip->dbi_base2local = 1.0 / loc2mm;

    const char *str = bu_units_string(dbip->dbi_local2base);
    if (!str) str = "Unknown_unit";
    if (msg)
	bu_vls_printf(msg, "You are now editing in '%s'.  1 %s = %g mm \n",
		str, str, dbip->dbi_local2base);

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
