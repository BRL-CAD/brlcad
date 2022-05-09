/*                      V L S _ I N C R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

#include <stdlib.h> /* for strtol */

#include <regex>
#include <cstring>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"

static int
vls_incr_next(struct bu_vls *next_incr, const char *incr_state, const char *inc_specifier)
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
	return -1;
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
	ret = -2;
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
    return ret;
}

int
bu_vls_incr(struct bu_vls *name, const char *regex_str, const char *incr_spec, bu_vls_uniq_t ut, void *data)
{
    if (!name) return -1;

    int ret = 0;
    struct bu_vls new_name = BU_VLS_INIT_ZERO;
    struct bu_vls num_str = BU_VLS_INIT_ZERO;
    struct bu_vls ispec = BU_VLS_INIT_ZERO;
    const char *last_regex = "([0-9]+)[^0-9]*$";
    const char *rs = (regex_str) ? regex_str : last_regex;

    try {
	std::regex cregex(rs, std::regex_constants::extended);
	int success = 0;

	while (!success) {
	    std::string sline(bu_vls_cstr(name));
	    std::string incr_str, prefix, suffix;
	    /* Find incrementer. */
	    std::smatch ivar;
	    if (!std::regex_search(sline, ivar, cregex)) {
		/* No incrementer string according to the regex - add as a suffix */
		bu_vls_sprintf(&num_str, "0");
		prefix = sline;
		suffix = std::string("");
	    } else {
		/* We have an incrementer string according to the regex */
		std::string incr_str_pre = ivar.str(1);
		prefix = sline.substr(0, ivar.position(1));
		suffix = sline.substr(ivar.position(1)+ivar.length(1), std::string::npos);
		std::regex nregex("([0-9]+)", std::regex_constants::extended);
		std::smatch nvar;
		std::regex_search(incr_str_pre, nvar, nregex);
		bu_vls_sprintf(&num_str, "%s", nvar.str(1).c_str());
	    }

	    /* Either used the supplied incrementing specification or initialize with the default */
	    if (!incr_spec) {
		bu_vls_sprintf(&ispec, "%zu:%d:%d:%d", strlen(bu_vls_cstr(&num_str)), 0, 0, 1);
	    } else {
		bu_vls_sprintf(&ispec, "%s", incr_spec);
	    }

	    bu_vls_sprintf(&new_name, "%s", prefix.c_str());

	    /* Do incrementation */
	    ret = vls_incr_next(&new_name, bu_vls_addr(&num_str), bu_vls_addr(&ispec));
	    bu_vls_printf(&new_name, "%s", suffix.c_str());
	    bu_vls_sprintf(name, "%s", bu_vls_cstr(&new_name));

	    if (ret < 0) {
		goto incr_cleanup;
	    }

	    /* If we need to, test for uniqueness */
	    if (ut) {
		success = (*ut)(name,data);
	    } else {
		success = 1;
	    }

	    bu_vls_trunc(&num_str, 0);
	    bu_vls_trunc(&ispec, 0);
	    bu_vls_trunc(&new_name, 0);
	}
    }

    catch (const std::regex_error& e) {
	bu_log("bu_vls_incr regex error: %s\n", e.what());
	ret = -1;
    }

incr_cleanup:
    bu_vls_free(&num_str);
    bu_vls_free(&ispec);
    bu_vls_free(&new_name);

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
