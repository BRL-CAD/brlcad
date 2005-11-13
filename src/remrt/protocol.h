/*                      P R O T O C O L . H
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file protocol.h
 *
 *  Definitions pertaining to the Remote RT protocol.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 *
 *  $Header$
 */

/* For use in MSG_VERSION exchanges */
#define PROTOCOL_VERSION	"BRL-CAD REMRT Protocol v2.0"

#define MSG_MATRIX	2
#define MSG_OPTIONS	3
#define MSG_LINES	4	/* request pixel interval be computed */
#define MSG_END		5
#define MSG_PRINT	6
#define MSG_PIXELS	7	/* response to server MSG_LINES */
#define MSG_RESTART	8	/* recycle server */
#define MSG_LOGLVL	9	/* enable/disable logging */
#define	MSG_CD		10	/* change directory */
#define MSG_CMD		11	/* server sends command to dispatcher */
#define MSG_VERSION	12	/* server sends version to dispatcher */
#define MSG_DB		13	/* request/send database lines 16k max */
#define MSG_CHECK	14	/* check database files for match */
#define MSG_DIRBUILD		15	/* request rt_dirbuild() be called */
#define MSG_DIRBUILD_REPLY	16	/* response to MSG_DIRBUILD */
#define MSG_GETTREES		17	/* request rt_gettrees() be called */
#define MSG_GETTREES_REPLY	18	/* response to MSG_GETTREES */

#define REMRT_MAX_PIXELS	(32*1024)	/* Max MSG_LINES req */

/*
 *  This structure is used for MSG_PIXELS messages
 */
struct line_info  {
	int	li_startpix;
	int	li_endpix;
	int	li_frame;
	int	li_nrays;
	double	li_cpusec;
	double	li_percent;	/* percent of system actually consumed */
	/* A scanline is attached after here */
};

#define LINE_O(x)	offsetof(struct  line_info, x)

struct bu_structparse desc_line_info[] =  {
	{"%d", 1, "li_startpix", LINE_O(li_startpix),	FUNC_NULL },
	{"%d", 1, "li_endpix",	LINE_O(li_endpix),	FUNC_NULL },
	{"%d", 1, "li_frame",	LINE_O(li_frame),	FUNC_NULL },
	{"%d", 1, "li_nrays",	LINE_O(li_nrays),	FUNC_NULL },
	{"%f", 1, "li_cpusec",	LINE_O(li_cpusec),	FUNC_NULL },
	{"%f", 1, "li_percent",	LINE_O(li_percent),	FUNC_NULL },
	{"",	0,			0 }
};

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
