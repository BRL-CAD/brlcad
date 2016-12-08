/*                        N A M E G E N . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2016 United States Government as represented by
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

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h> /* for strtol */
#include <limits.h> /* for LONG_MAX */
#include <errno.h> /* for errno */
#include <regex.h>

#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/namegen.h"
#include "bu/str.h"
#include "bu/uuid.h"

int
_bu_namegen_next(struct bu_vls *next_incr, const char *inc_str, long *s)
{
    int i = 0;
    long ret = 0;
    /*char bsep = '\0';
    char esep = '\0';*/
    const char *wstr = inc_str;
    char *endptr;
    int spacer_cnt = 1;
    long spacer_val;
    if (!inc_str || !next_incr || !s) return 0;
    bu_vls_trunc(next_incr, 0);

    /* Move the working string pointer past the type identifier */
    wstr = strchr(wstr, ':') + 1;

    if (inc_str[0] == 'd') {
	long vals[4] = {0}; /* 0 = width, 1 = min/init, 2 = max, 3 = increment */
	for(i = 0; i < 4; i++) {
	    errno = 0;
	    vals[i] = strtol(wstr, &endptr, 0);
	    if (errno == ERANGE) {
		bu_log("ERANGE error reading name generation specifier\n");
		return ret;
	    }
	    bu_log("%s value %d: %ld\n", inc_str, i, vals[i]);
	    wstr = (strchr(wstr, ':')) ?  strchr(wstr, ':') + 1 : NULL;
	}
	(*s) = (vals[3]) ? (*s) + vals[3] : (*s) + 1;
	if (*s < vals[1]) (*s) = vals[1];
	if (vals[0]) {
	    spacer_val = vals[0];
	    while ((spacer_val = spacer_val % 10)) spacer_cnt++;
	    bu_log("cols needed: %d\n", spacer_cnt);
	    if (spacer_cnt > vals[0]) {
		(*s) = vals[1];
	    }
	}
	if (*s >= vals[2]+1) (*s) = vals[1];
	if (wstr[0]) bu_vls_printf(next_incr, "%c", wstr[0]);
	wstr = (strchr(wstr, ':')) ?  strchr(wstr, ':') + 1 : NULL;
	bu_vls_printf(next_incr, "%ld", *s);
	if (wstr[0]) bu_vls_printf(next_incr, "%c", wstr[0]);
	return bu_vls_strlen(next_incr);
    } else if (inc_str[0] == 'u') {
	/* TODO - UUID */
	return 0;
    } else {
	bu_log("Error - unknown incrementer type specified: %c\n", inc_str[0]);
    }
    return ret;
}

int
bu_namegen(struct bu_vls *name, const char *input_str, const char *regex_str, const char **incrs, long *incrs_states)
{
    /*regex_t compiled_regex;*/

    if (!regex_str || !input_str || !regex_str || !incrs || !incrs_states) return -1;
    bu_vls_trunc(name, 0);


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
