/*                F A C E T I Z E _ L O G . C
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
/** @file libged/facetize_log.c
 *
 * logging mechanisms for the facetize command.
 *
 */

#include "common.h"

#include <string.h>

#include "bio.h"

#include "bu/hook.h"
#include "bu/vls.h"
#include "../ged_private.h"

static int
_ged_facetize_bomb_hook(void *cdata, void *str)
{
    struct _ged_facetize_opts *o = (struct _ged_facetize_opts *)cdata;
    if (o->nmg_log_print_header) {
	bu_vls_printf(o->nmg_log, "%s\n", bu_vls_addr(o->nmg_log_header));
	o->nmg_log_print_header = 0;
    }
    bu_vls_printf(o->nmg_log, "%s\n", (const char *)str);
    return 0;
}

static int
_ged_facetize_nmg_logging_hook(void *data, void *str)
{
    struct _ged_facetize_opts *o = (struct _ged_facetize_opts *)data;
    if (o->nmg_log_print_header) {
	bu_vls_printf(o->nmg_log, "%s\n", bu_vls_addr(o->nmg_log_header));
	o->nmg_log_print_header = 0;
    }
    bu_vls_printf(o->nmg_log, "%s\n", (const char *)str);
    return 0;
}

void
_ged_facetize_log_nmg(struct _ged_facetize_opts *o)
{
    if (fileno(stderr) < 0)
	return;

    /* Seriously, bu_bomb, we don't want you blathering
     * to stderr... shut down stderr temporarily, assuming
     * we can find /dev/null or something similar */
    o->fnull = open("/dev/null", O_WRONLY);
    if (o->fnull == -1) {
	/* https://gcc.gnu.org/ml/gcc-patches/2005-05/msg01793.html */
	o->fnull = open("nul", O_WRONLY);
    }
    if (o->fnull != -1) {
	o->serr = fileno(stderr);
	o->stderr_stashed = dup(o->serr);
	dup2(o->fnull, o->serr);
	close(o->fnull);
    }

    /* Set bu_log logging to capture in nmg_log, rather than the
     * application defaults */
    bu_log_hook_delete_all();
    bu_log_add_hook(_ged_facetize_nmg_logging_hook, (void *)o);

    /* Also engage the nmg bomb hooks */
    bu_bomb_delete_all_hooks();
    bu_bomb_add_hook(_ged_facetize_bomb_hook, (void *)o);
}


void
_ged_facetize_log_default(struct _ged_facetize_opts *o)
{
    if (fileno(stderr) < 0)
	return;

    /* Put stderr back */
    if (o->fnull != -1) {
	fflush(stderr);
	dup2(o->stderr_stashed, o->serr);
	close(o->stderr_stashed);
	o->fnull = -1;
    }

    /* Restore bu_bomb hooks to the application defaults */
    bu_bomb_delete_all_hooks();
    bu_bomb_restore_hooks(o->saved_bomb_hooks);

    /* Restore bu_log hooks to the application defaults */
    bu_log_hook_delete_all();
    bu_log_hook_restore_all(o->saved_log_hooks);
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
