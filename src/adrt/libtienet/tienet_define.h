/*                 T I E N E T _ D E F I N E . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2007 United States Government as represented by
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
/** @file tienet_define.h
 *
 *  Comments -
 *      TIE Networking Definitions
 *
 *  Author -
 *      Justin L. Shumaker
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 * $Id$
 */

#ifndef _TIENET_DEFINE_H
#define _TIENET_DEFINE_H


#define	TN_COMPRESSION		0		/* 0 = off, 1 = on.  Compress the result buffer */

#define	TN_MASTER_PORT		1980
#define	TN_SLAVE_PORT		1981

#define	TN_OP_PREP		0x0010
#define	TN_OP_REQWORK		0x0011
#define	TN_OP_SENDWORK		0x0012
#define	TN_OP_RESULT		0x0013
#define	TN_OP_COMPLETE		0x0014
#define	TN_OP_SHUTDOWN		0x0015
#define TN_OP_OKAY		0x0016
#define	TN_OP_MESSAGE		0x0017

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
