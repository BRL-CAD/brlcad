/*                   C R E O - B R L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017 United States Government as represented by
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
/** @file creo-brl.cpp
 *
 */

#include "common.h"

#include <set>

#ifndef _WSTUDIO_DEFINED
# define _WSTUDIO_DEFINED
#endif
#include "ProToolkit.h"
#include <ProUiDialog.h>

extern "C" int user_initialize()
{
    ProError perr;
    perr = ProUIDialogCreate("Do Something!", NULL);
    perr = ProUIDialogActivate("Do Something!", NULL);
    perr = ProUIDialogExit("Do Something!", NULL);
    return 0;
}
extern "C" void user_terminate()
{
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
