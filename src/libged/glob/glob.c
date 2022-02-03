/*                         G L O B . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/glob.c
 *
 * The glob command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "bu/path.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "ged.h"

/* These are general globbing flags. */
#define _GED_GLOB_HIDDEN       0x1    /**< @brief include hidden objects in results */
#define _GED_GLOB_NON_GEOM     0x2    /**< @brief include non-geometry objects in results */
#define _GED_GLOB_SKIP_FIRST   0x4    /**< @brief do not expand the first item */

/**
 * unescapes various special characters
 */
HIDDEN void
_debackslash(struct bu_vls *dest, struct bu_vls *src)
{
    char *ptr;

    ptr = bu_vls_addr(src);
    while (*ptr) {
	if (*ptr == '\\')
	    ++ptr;
	if (*ptr == '\0')
	    break;
	bu_vls_putc(dest, *ptr++);
    }
}

/**
 * escapes various special characters
 */
HIDDEN void
_backslash_specials(struct bu_vls *dest, struct bu_vls *src)
{
    int backslashed;
    char *ptr, buf[2];

    buf[1] = '\0';
    backslashed = 0;
    for (ptr = bu_vls_addr(src); *ptr; ptr++) {
	if (*ptr == '[' && !backslashed)
	    bu_vls_strcat(dest, "\\[");
	else if (*ptr == ']' && !backslashed)
	    bu_vls_strcat(dest, "\\]");
	else if (backslashed) {
	    bu_vls_strcat(dest, "\\");
	    buf[0] = *ptr;
	    bu_vls_strcat(dest, buf);
	    backslashed = 0;
	} else if (*ptr == '\\')
	    backslashed = 1;
	else {
	    buf[0] = *ptr;
	    bu_vls_strcat(dest, buf);
	}
    }
}

HIDDEN int
_ged_expand_str_glob(struct bu_vls *dest, const char *input, struct db_i *dbip, int flags)
{
    char *start, *end;          /* Start and ends of words */
    int is_fnmatch;                 /* Set to TRUE when word is a is_fnmatch */
    int backslashed;
    int match_cnt = 0;
    int firstword = 1;
    int skip_first = (flags & _GED_GLOB_SKIP_FIRST) ? 1 : 0;
    struct bu_vls word = BU_VLS_INIT_ZERO;         /* Current word being processed */
    struct bu_vls temp = BU_VLS_INIT_ZERO;
    char *src = NULL;

    if (dbip == DBI_NULL) {
	bu_vls_sprintf(dest, "%s", input);
	return 0;
    }

    src = bu_strdup(input);

    end = src;
    while (*end != '\0') {
	/* Run through entire string */

	/* First, pass along leading whitespace. */

	start = end;                   /* Begin where last word ended */
	while (*start != '\0') {
	    if (*start == ' '  ||
		    *start == '\t' ||
		    *start == '\n')
		bu_vls_putc(dest, *start++);
	    else
		break;
	}
	if (*start == '\0')
	    break;
	/* Next, advance "end" pointer to the end of the word, while
	 * adding each character to the "word" vls.  Also make a note
	 * of any unbackslashed wildcard characters.
	 */

	end = start;
	bu_vls_trunc(&word, 0);
	is_fnmatch = 0;
	backslashed = 0;
	while (*end != '\0') {
	    if (*end == ' '  ||
		    *end == '\t' ||
		    *end == '\n')
		break;
	    if ((*end == '*' || *end == '?' || *end == '[') && !backslashed)
		is_fnmatch = 1;
	    if (*end == '\\' && !backslashed)
		backslashed = 1;
	    else
		backslashed = 0;
	    bu_vls_putc(&word, *end++);
	}

	/* If the client told us not to do expansion on the first word
	 * in the stream, disable matching */
	if (firstword && skip_first) {
	    is_fnmatch = 0;
	}

	/* Now, if the word was suspected of being a wildcard, try to
	 * match it to the database.
	 */
	/* Now, if the word was suspected of being a wildcard, try to
	 * match it to the database.
	 */
	if (is_fnmatch) {
	    register int i, num;
	    register struct directory *dp;
	    bu_vls_trunc(&temp, 0);
	    for (i = num = 0; i < RT_DBNHASH; i++) {
		for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		    if (bu_path_match(bu_vls_addr(&word), dp->d_namep, 0) != 0) continue;
		    if (!(flags & _GED_GLOB_HIDDEN) && (dp->d_flags & RT_DIR_HIDDEN)) continue;
		    if (!(flags & _GED_GLOB_NON_GEOM) && (dp->d_flags & RT_DIR_NON_GEOM)) continue;
		    if (num == 0)
			bu_vls_strcat(&temp, dp->d_namep);
		    else {
			bu_vls_strcat(&temp, " ");
			bu_vls_strcat(&temp, dp->d_namep);
		    }
		    match_cnt++;
		    ++num;
		}
	    }

	    if (num == 0) {
		_debackslash(&temp, &word);
		_backslash_specials(dest, &temp);
	    } else
		bu_vls_vlscat(dest, &temp);
	} else {
	    _debackslash(dest, &word);
	}

	firstword = 0;
    }

    bu_vls_free(&temp);
    bu_vls_free(&word);
    bu_free(src, "free input cpy");
    return match_cnt;
}

int
ged_glob_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "expression";
    int flags = 0;
    flags |= _GED_GLOB_HIDDEN;
    flags |= _GED_GLOB_NON_GEOM;
    flags |= _GED_GLOB_SKIP_FIRST;

    /* Silently return */
    if (gedp == GED_NULL)
	return BRLCAD_ERROR;

    /* Initialize result. This behavior is depended upon by mged - apparently
     * the interpretation is that if no database is open, all expressions match
     * nothing and the empty string is returned. */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* No database to match against, so return. */
    if (gedp->ged_wdbp == RT_WDB_NULL || gedp->dbip == DBI_NULL)
	return BRLCAD_OK;

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }


    (void)_ged_expand_str_glob(gedp->ged_result_str, argv[1], gedp->dbip, flags);


    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl glob_cmd_impl = {"glob", ged_glob_core, GED_CMD_DEFAULT};
const struct ged_cmd glob_cmd = { &glob_cmd_impl };

struct ged_cmd_impl db_glob_cmd_impl = {"db_glob", ged_glob_core, GED_CMD_DEFAULT};
const struct ged_cmd db_glob_cmd = { &db_glob_cmd_impl };

const struct ged_cmd *glob_cmds[] = { &glob_cmd, &db_glob_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  glob_cmds, 2 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
