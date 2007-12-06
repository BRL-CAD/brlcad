/*                          I S S T . H
 * BRL-CAD / ADRT
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
/** @file isst.h
 *
 * Author -
 *   Justin Shumaker
 */

#ifndef _ISST_H
#define _ISST_H

#include "tie.h"
#include "adrt_common.h"
#include "adrt.h"


#define	ISST_OBSERVER_PORT	ADRT_PORT
#define	ISST_USE_COMPRESSION	ADRT_USE_COMPRESSION

#define ISST_OP_RENDER		ADRT_WORK_FRAME
#define ISST_OP_SHOT		ADRT_WORK_SHOTLINE
#define ISST_OP_SPALL		ADRT_WORK_SPALL
#define ISST_OP_SELECT		ADRT_WORK_SELECT

#define	ISST_PIXEL_FMT		ADRT_PIXEL_FMT

#define ISST_NET_OP_NOP		ADRT_NETOP_NOP
#define	ISST_NET_OP_INIT	ADRT_NETOP_INIT
#define	ISST_NET_OP_FRAME	ADRT_NETOP_WORK
#define ISST_NET_OP_MESG	ADRT_NETOP_MESG
#define	ISST_NET_OP_QUIT	ADRT_NETOP_QUIT
#define	ISST_NET_OP_SHUTDOWN	ADRT_NETOP_SHUTDOWN


#define ISST_VER_KEY		ADRT_VER_KEY
#define ISST_VER		"1.0.0"
#define ISST_VER_DETAIL		ADRT_VER_DETAIL

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
