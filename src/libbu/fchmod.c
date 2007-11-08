/*                        F C H M O D . C
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
/** @addtogroup win_compat */
/** @{ */
/**
 * @file fchmod.c
 *
 * @brief
 *  Wrapper around fchmod.
 *
 * @par  Functions
 *	bu_fchmod  	Change file permissions
 *
 *  @author     Bob Parker
 *
 * @par  Source -
 *	The U. S. Army Research Laboratory
 * @n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#include "common.h"
#include "machine.h"
#include "bu.h"

/*XXX 
 * For the moment we're passing filename. There should
 * be a way to get this from FILE * on Windows. A quick
 * look yielded nada.
 */
int
bu_fchmod(const char *filename,
	  FILE	     *fp,
	  int	     pmode)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    return chmod(filename, pmode);
#else
    return fchmod(fileno(fp), pmode);
#endif    
}
