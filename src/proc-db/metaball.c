/*                      M E T A B A L L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2008 United States Government as represented by
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
 */

/** @file metaball.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"


static const char usage[] = "Usage:\n\t%s []\n";

struct rt_wdb	*outfp;

int
main(int argc, char **argv)
{
    char title[64] = "metaball";

    fprintf(stderr, "This is not the proc-db you are looking for\n");
    exit(-42);

    /* probably need a list or something
    BU_LIST_INIT( &head.l );
    */
    
    outfp = wdb_fopen( "metaball.g" );
    mk_id( outfp, title );
    /* do something
    mk_lfcomb( outfp, "mball.g", &head, 0 );
    */

    wdb_close(outfp);
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
