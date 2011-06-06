/*                        I O U T I L . C
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
/** @file ioutil.c
 *
 * Helper I/O routines for a few functions common to some commands
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "bu.h"
#include "fb.h"


HIDDEN void
VMessage(const char *format, va_list ap)
{
    struct bu_vls str;
    char *tmp_basename;

    bu_vls_init(&str);

    bu_vls_printf(&str, format, ap);
    tmp_basename = bu_basename(bu_getprogname());

    bu_log("%s: %V\n", tmp_basename, &str);

    bu_vls_free(&str);
    bu_free(tmp_basename, "bu_basename");
}


void
Message(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    VMessage(format, ap);
    va_end(ap);
}


void
Fatal(FBIO *fbp, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    VMessage(format, ap);
    va_end(ap);
    
    if (fbp != FBIO_NULL && fb_close(fbp) == -1) {
	Message("Error closing frame buffer");
	fbp = FBIO_NULL;
    }
    
    bu_exit(EXIT_FAILURE, NULL);
    /* NOT REACHED */
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
