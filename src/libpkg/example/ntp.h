/*                          N T P . H
 * BRL-CAD
 *
 * Copyright (c) 2006-2011 United States Government as represented by
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
/** @file libpkg/example/ntp.h
 *
 * Example file transfer protocol definition
 *
 */

/* simple network transport protocol. connection starts with a HELO,
 * then a variable number of GEOM/ARGS messages, then a CIAO to end.
 */
#define MAGIC_ID	"TPKG"
#define MSG_HELO	1
#define MSG_DATA	2
#define MSG_CIAO	3

/* maximum number of digits on a port number */
#define MAX_DIGITS      5

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
