/*                         E X E C . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file exec.c
 *
 * Execution of parsed commands against a libged database instance.
 *
 */

#include "common.h"

#include "bu/vls.h"
#include "ged.h"
#include "mcpcad.h"


int
mcpcad_cmd_exec(struct ged *gedp, const struct mcpcad_cmd *c,
		struct bu_vls *result)
{
    int status;

    /* command validity comes before session state - a malformed call
     * is a caller bug regardless of whether a database is open */
    if (!c || c->argc < 1 || !c->argv || !c->argv[0]) {
	if (result)
	    bu_vls_strcpy(result, "no command to execute");
	return MCPCAD_ERR_NOCMD;
    }

    if (!gedp) {
	if (result)
	    bu_vls_strcpy(result, "no database is open");
	return MCPCAD_ERR_NODB;
    }

    /* start from a clean slate so result carries only this command's
     * output, not residue from a previous one */
    bu_vls_trunc(gedp->ged_result_str, 0);

    status = ged_exec(gedp, c->argc, (const char **)c->argv);

    if (result)
	bu_vls_strcpy(result, bu_vls_cstr(gedp->ged_result_str));

    return status;
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
