/*                     L I B T E R M I O . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup libtermio */
/*@{*/
/** @file libtermio.h
 *			T E R M I O . H
 *
 *  Externs for the BRL-CAD library LIBTERMIO
 *
 *  @author
 *	Gary S. Moss
 *
 *  @par Source
 *	The U. S. Army Research Laboratory
 *@n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *  @par Include Sequencing -
@code	#include "common.h"
	#include <stdio.h>
	#include "termio.h"
@endcode
 *  $Header$
 */

void clr_Cbreak( int fd );
void set_Cbreak( int fd );
void clr_Raw( int fd );
void set_Raw( int fd );
void set_Echo( int fd );
void clr_Echo( int fd );
void set_Tabs( int fd );
void clr_Tabs( int fd );
void set_HUPCL( int fd );
void clr_CRNL( int fd );
unsigned short get_O_Speed( int fd );
void save_Tty( int fd );
void reset_Tty( int fd );
int save_Fil_Stat( int fd );
int reset_Fil_Stat( int	fd );
int set_O_NDELAY( int fd );
void prnt_Tio();	/* misc. types of args */
/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

