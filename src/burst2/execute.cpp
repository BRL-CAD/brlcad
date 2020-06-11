/*                      E X E C U T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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
/** @file burst/execute.cpp
 *
 */

#include "common.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <regex>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "vmath.h"
#include "raytrace.h"

#include "./burst.h"

#define WHITESPACE	" \t"

static void
prntTimer(struct burst_state *s, const char *str)
{
    (void) rt_read_timer(s->timer, TIMER_LEN-1);
    bu_log("%s:\t%s\n", str == NULL ? "(null)" : str, s->timer);
}


void
execute_run(struct burst_state *s)
{
    static int gottree = 0;
    int loaderror = 0;
    if (!bu_vls_strlen(&s->gedfile)) {
	bu_log("No target file has been specified.");
	return;
    }
    bu_log("Reading target data base");
    rt_prep_timer();
    if (s->rtip == RTI_NULL) {
	s->rtip = rt_dirbuild(bu_vls_cstr(&s->gedfile), s->title, TITLE_LEN);
    }
    if (s->rtip == RTI_NULL) {
	bu_log("Ray tracer failed to read the target file.");
	return;
    }
    prntTimer(s, "dir");
    /* Add air into int trees, must be set after rt_dirbuild() and before
     * rt_gettree(). */
    s->rtip->useair = 1;
    if (!gottree) {
	char *objline = bu_strdup(bu_vls_cstr(&s->objects));
	char **av = (char **)bu_calloc(strlen(objline) + 1, sizeof(char *), "argv array");
	int ac = bu_argv_from_string(av, strlen(objline), objline);

	rt_prep_timer();
	for (int i = 0; i < ac; i++) {
	    const char *obj = av[i];
	    bu_log("Loading \"%s\"", obj);
	    if (rt_gettree(s->rtip, obj) != 0) {
		bu_log("Bad object \"%s\".", obj);
		loaderror = 1;
	    }
	}
	bu_free(objline, "objline copy");
	bu_free(av, "av");

	gottree = 1;
	prntTimer(s, "load");
    }
    if (loaderror)
	return;
    if (s->rtip->needprep) {
	bu_log("Prepping solids");
	rt_prep_timer();
	rt_prep(s->rtip);
	prntTimer(s, "prep");
    }
    gridInit(s);
    if (s->nriplevels > 0) {
	spallInit(s);
    }
    gridModel(s);
    return;
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
