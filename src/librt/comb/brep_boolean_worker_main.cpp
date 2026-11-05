/*          B R E P _ B O O L E A N _ W O R K E R _ M A I N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include "bu/app.h"
#include "rt/defines.h"

extern "C" RT_EXPORT int
_rt_brep_boolean_worker_server(int argc, const char **argv);


int
main(int argc, const char **argv)
{
    bu_setprogname(argv && argv[0] ? argv[0] : "brep_boolean_worker");
    return _rt_brep_boolean_worker_server(argc, argv);
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
