/*                        P A R S E . C
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
/** @file parse.c
 *
 * Parsing of flat command strings into argc/argv form for execution.
 *
 */

#include "common.h"

#include "mcpcad.h"

struct mcpcad_cmd *
mcpcad_cmd_parse(const char *line)
{
    /* not yet implemented */
    if (!line)
	return NULL;

    return NULL;
}


void
mcpcad_cmd_free(struct mcpcad_cmd *c)
{
    /* not yet implemented */
    if (!c)
	return;
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
