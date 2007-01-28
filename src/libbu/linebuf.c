/*                       L I N E B U F . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @addtogroup bu_log */
/** @{ */
/** @file linebuf.c
 *
 *  @brief
 *  A portable way of doing setlinebuf().
 *
 *	A portable way of doing setlinebuf().
 *
 *  @author	Doug A. Gwyn
 *  @author	Michael John Muuss
 *  @author	Christopher Sean Morrison
 *
 *  @par Source -
 *  @n	The U. S. Army Research Laboratory
 *  @n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */


#ifndef lint
static const char libbu_linebuf_RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>

#include "machine.h"
#include "bu.h"


/** deprecated call for compatibility
 */
void
port_setlinebuf(FILE *fp)
{
    bu_log("\
WARNING: port_setlinebuf is deprecated and will be removed in a\n\
future release of BRL-CAD.  Use bu_setlinebuf instead.\n");
    bu_setlinebuf(fp);
    return ;
}

void
bu_setlinebuf(FILE *fp)
{
#if defined(HAVE_SETVBUF)
    /* prefer this one */
    if (setvbuf( fp, (char *) NULL, _IOLBF, 2) != 0) {
	perror("bu_setlinebuf");
    }
#elif defined(HAVE_SETLINEBUF)
    /* setlinebuf() returns int on bsd, void on linux */
    (void)setlinebuf( fp );
#else
#  error "Do not know how to set line buffered mode for this platform"
#endif
}
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
