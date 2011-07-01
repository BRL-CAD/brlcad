
/*                         F O C U S . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
/** @file libdm/focus.c
 *
 * Routines for setting the current application focus, particularly
 * useful on Mac OS X where X11 applications are not necessarily given
 * focus after the application is launched.
 *
 */

#include "common.h"

#ifdef HAVE_CARBON_CARBON_H
#  define Cursor MyCursor
#  include <Carbon/Carbon.h>
#  undef Cursor
/* undef __QUICKDRAW__ is needed to prevent gl.h from declaring an
 * unrecognized pragma export warning on Mac OS X (10.5).
 */
#  ifdef __QUICKDRAW__
#    undef __QUICKDRAW__
#  endif
#endif /* HAVE_CARBON_CARBON_H */

#include "dm.h"

static void
dm_x11_applicationfocus() {
#ifdef HAVE_CARBON_CARBON_H
    OSStatus status;
    ProcessSerialNumber psn = {kNoProcess, kNoProcess};
    CFStringRef processName = NULL;

    do {
	status = GetNextProcess(&psn);

	/* Is this the psn for X11? */
	CopyProcessName(&psn, &processName);
	if (processName == NULL) {
	    break;
	}

	if (CFStringCompare(processName, CFSTR("X11"), 0) == kCFCompareEqualTo) {
	    /* focus X11 */
	    SetFrontProcess(&psn);
	}
    } while (status == noErr);
#endif
}


/**
 * attempt to bring the current application to focus
 */
void
dm_applicationfocus() {
#ifdef DM_X
    dm_x11_applicationfocus();
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
