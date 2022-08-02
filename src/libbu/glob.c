/*                          G L O B . C
 * BRL-CAD
 *
 * Copyright (c) 2019-2022 United States Government as represented by
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

/*
 * NOTE: this implementation is an exploratory work-in-progress, not
 * yet intended for any use.
 */

#include "common.h"

#ifdef HAVE_GLOB_H
# include <glob.h>
#endif

#include "bu/log.h"
#include "bu/glob.h"




int
bu_glob(const char *pattern, int flags, struct bu_glob_context *gp)
{
    if (!pattern || !gp)
	return 1;

#ifdef HAVE_GLOB_H
    glob_t g;

    g.gl_pathc = gp->gl_pathc;
    //g.gl_matchc = gp->gl_matchc;
/*
    if (gp->gl_pathv)
	g.gl_pathv = bu_vls_argv(gp->gl_pathv);
*/
    if (flags)
	bu_log("INTERNAL ERROR: glob flags unsupported\n");

    return glob(pattern, flags, NULL, &g);
#else
    return 1;
#endif
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
