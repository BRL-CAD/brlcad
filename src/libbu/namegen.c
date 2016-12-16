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
_bu_namegen_next(struct bu_vls *next_incr, const char *incr_state, const char *inc_specifier)
{
    int i = 0;
    long ret = 0;
    /*char bsep = '\0';
    char esep = '\0';*/
    long vals[4] = {0}; /* 0 = width, 1 = min/init, 2 = max, 3 = increment */
    long state_val = 0;
    const char *wstr = inc_specifier;
    char *endptr;
    int spacer_cnt = 1;
    long spacer_val;

    if (!inc_specifier || !next_incr || !incr_state) return 0;

    errno = 0;
    state_val = strtol(incr_state, &endptr, 10);
    if (errno == ERANGE) {
	bu_log("ERANGE error reading current value\n");
	return ret;
    }

    for(i = 0; i < 4; i++) {
	errno = 0;
	if (!wstr) {
	    bu_log("Invalid incrementation specifier: %s\n", inc_specifier);
	    return -1;
	}
	vals[i] = strtol(wstr, &endptr, 10);
	if (errno == ERANGE) {
	    bu_log("ERANGE error reading name generation specifier\n");
	    return -1;
	}
	wstr = (strchr(wstr, ':')) ?  strchr(wstr, ':') + 1 : NULL;
    }

    /* increment */
    state_val = (vals[3]) ? state_val + vals[3] : state_val + 1;

    /* if we're below the minimum specified range, clamp to minimum */
    if (vals[1] && state_val < vals[1]) state_val = vals[1];

    /* if we're above the maximum specified range, roll over */
    if (vals[2] && state_val > vals[2]) {
	state_val = vals[1];
	ret = 1;
    }

    /* find out if we need padding zeros */
    if (vals[0]) {
	spacer_val = state_val;
	while ((spacer_val = spacer_val / 10)) spacer_cnt++;
    }

    if (wstr) {
	bu_vls_printf(next_incr, "%c", wstr[0]);
	if (vals[0]) {
	    for (i = 0; i < vals[0]-spacer_cnt; i++) {
		bu_vls_printf(next_incr, "%d", 0);
	    }
	    bu_vls_printf(next_incr, "%ld", state_val);
	} else {
	    bu_vls_printf(next_incr, "%ld", state_val);
	}
	wstr = (strchr(wstr, ':')) ?  strchr(wstr, ':') + 1 : NULL;
	if (wstr) bu_vls_printf(next_incr, "%c", wstr[0]);
    } else {
	if (vals[0]) {
	    for (i = 0; i < vals[0]-spacer_cnt; i++) {
		bu_vls_printf(next_incr, "%d", 0);
	    }
	    bu_vls_printf(next_incr, "%ld", state_val);
	} else {
	    bu_vls_printf(next_incr, "%ld", state_val);
	}
    }
    return 0;
}

int
bu_namegen(struct bu_vls *name, const char *regex_str, const char *incr_spec)
{
    int ret = 0;
    int i = 0;
    int j = 0;
    int offset = 0;
    regex_t compiled_regex;
    regmatch_t *incr_substrs;
    regmatch_t *num_substrs;
    struct bu_vls new_name = BU_VLS_INIT_ZERO;
    struct bu_vls curr_incr = BU_VLS_INIT_ZERO;
    struct bu_vls ispec = BU_VLS_INIT_ZERO;
    const char *num_regex = "([0-9]+)";
    struct bu_vls num_str = BU_VLS_INIT_ZERO;

    if (!name || !regex_str) return -1;

    /* Find incrementer. */
    ret = regcomp(&compiled_regex, regex_str, REG_EXTENDED);
    if (ret != 0) return -1;
    incr_substrs = (regmatch_t *)bu_calloc(bu_vls_strlen(name) + 1, sizeof(regmatch_t), "regex results");
    ret = regexec(&compiled_regex, bu_vls_addr(name), bu_vls_strlen(name) + 1, incr_substrs, 0);
    if (ret == REG_NOMATCH) {
	bu_free(incr_substrs, "free regex results");
	return -1;
    }
    i = bu_vls_strlen(name);
    while(incr_substrs[i].rm_so == -1 || incr_substrs[i].rm_eo == -1) i--;

    if (i != 1) return -1;

    /* Now we know where the incrementer is - process, find the number, and assemble the new string */
    bu_vls_trunc(&new_name, 0);
    bu_vls_substr(&curr_incr, name, incr_substrs[1].rm_so, incr_substrs[1].rm_eo - incr_substrs[1].rm_so);
    bu_vls_strncpy(&new_name, bu_vls_addr(name)+offset, incr_substrs[j].rm_so - offset);

    /* Find number. */
    ret = regcomp(&compiled_regex, num_regex, REG_EXTENDED);
    if (ret != 0) return -1;
    num_substrs = (regmatch_t *)bu_calloc(bu_vls_strlen(&curr_incr) + 1, sizeof(regmatch_t), "regex results");
    ret = regexec(&compiled_regex, bu_vls_addr(&curr_incr), bu_vls_strlen(&curr_incr) + 1, num_substrs, 0);
    if (ret == REG_NOMATCH) {
	bu_free(num_substrs, "free regex results");
	return -1;
    }
    bu_vls_substr(&num_str, &curr_incr, num_substrs[1].rm_so, num_substrs[1].rm_eo - num_substrs[1].rm_so);


    if (!incr_spec) {
	bu_vls_sprintf(&ispec, "%d:%d:%d:%d", strlen(bu_vls_addr(&num_str)), 0, 0, 1);
    } else {
	bu_vls_sprintf(&ispec, "%s", incr_spec);
    }

    ret = _bu_namegen_next(&new_name, bu_vls_addr(&num_str), bu_vls_addr(&ispec));
    bu_vls_printf(&new_name, "%s", bu_vls_addr(name)+incr_substrs[1].rm_eo);

    bu_vls_sprintf(name, "%s", bu_vls_addr(&new_name));

    bu_vls_free(&new_name);
    bu_vls_free(&ispec);
    bu_vls_free(&curr_incr);
    bu_free(incr_substrs, "free regex results");

    return ret;
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
