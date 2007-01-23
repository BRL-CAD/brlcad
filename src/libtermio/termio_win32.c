/*                  T E R M I O _ W I N 3 2 . C
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
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
/** @file termio_win32.c
 *
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>

#include "machine.h"
#include "libtermio.h"

static int		fileStatus[FOPEN_MAX];
void			prnt_Tio();
static void		copy_Tio();

/*	c l r _ C b r e a k ( )
	Clear CBREAK mode, for file descriptor 'fd'.
 */
void
clr_Cbreak( fd )
int	fd;
	{
	return;
	}

/*	s e t _ C b r e a k ( )
	Set CBREAK mode, 'fd'.
 */
void
set_Cbreak( fd )
int	fd;
	{
	return;
	}

/*	c l r _ R a w ( )
	Set cooked mode, 'fd'.
 */
void
clr_Raw( fd )
int	fd;
	{
	return;
	}

/*	s e t _ R a w ( )
	Set raw mode, 'fd'.
 */
void
set_Raw( fd )
int	fd;
	{
	return;
	}

/*	s e t _ E c h o ( )
	Set echo mode, 'fd'.
 */
void
set_Echo( fd )
int	fd;
	{
	return;
	}

/*	c l r _ E c h o ( )
	Clear echo mode, 'fd'.
 */
void
clr_Echo( fd )
int	fd;
	{
	return;
	}

/*	s e t _ T a b s ( )
	Turn on tab expansion, 'fd'.
 */
void
set_Tabs( fd )
int	fd;
	{
	return;
	}

/*	c l r _ T a b s ( )
	Turn off tab expansion, 'fd'.
 */
void
clr_Tabs( fd )
int	fd;
	{
	return;
	}

/*	s e t _ H U P C L ( )
	Turn on "Hang up on last close", 'fd'.
 */
void
set_HUPCL( fd )
int	fd;
	{
	return;
	}

/*	c l r _ C R N L ( )
	Turn off CR/LF mapping, fd.
 */
void
clr_CRNL( fd )
	{
return;
	}

/*	g e t _ O _ S p e e d ( )
	Get the terminals output speed, 'fd'.
 */
unsigned short
get_O_Speed( fd )
	{

	return	1;
	}

/*	s a v e _ T t y ( )
	Get and save terminal parameters, 'fd'.
 */
void
save_Tty( fd )
int	fd;
	{
	return;
	}

/*	r e s e t _ T t y ( )
	Set the terminal back to the mode that the user had last time
	save_Tty() was called for 'fd'.
 */
void
reset_Tty( fd )
int	fd;
	{
	return;
	}

/*	s a v e _ F i l _ S t a t ( )
	Save file status flags for 'fd'.
 */
int
save_Fil_Stat( fd )
int	fd;
	{
	return	1;
	}

/*	r e s e t _ F i l _ S t a t ( )
	Restore file status flags for file desc. 'fd' to what they were the
	last time saveFilStat(fd) was called.
 */
int
reset_Fil_Stat( fd )
int	fd;
	{
	return	1;
	}

/*	s e t _ O _ N D E L A Y ( )
	Set non-blocking read on 'fd'.
 */
int
set_O_NDELAY( fd )
int	fd;
	{
	return	1;
	}

/*	c o p y _ T i o ( )						*/
static void
copy_Tio( to, from )
#ifdef BSD
struct sgttyb	*to, *from;
#endif
#ifdef SYSV
struct termio	*to, *from;
#endif
#ifdef HAVE_TERMIOS_H
struct termios	*to;
struct termios	*from;
#endif
	{
	return;
	}

/*	p r n t _ T i o ( )						*/
void
prnt_Tio( msg, tio_ptr )
char		*msg;
#ifdef BSD
struct sgttyb	*tio_ptr;
#endif
#ifdef SYSV
struct termio	*tio_ptr;
#endif
#ifdef HAVE_TERMIOS_H
struct termios	*tio_ptr;
#endif
	{
	return;
	}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
