/*                          I S S T . H
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
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


#define	ISST_OBSERVER_PORT	1982
#define	ISST_USE_COMPRESSION	1

#define ISST_OP_RENDER		0
#define ISST_OP_SHOT		1
#define ISST_OP_SPALL		2
#define ISST_OP_SELECT		3

#define	ISST_PIXEL_FMT		0	/* 0 == unsigned char, 1 ==  tfloat */

#define ISST_NET_OP_NOP		0
#define	ISST_NET_OP_INIT	1
#define	ISST_NET_OP_FRAME	2
#define ISST_NET_OP_MESG	3
#define	ISST_NET_OP_QUIT	4
#define	ISST_NET_OP_SHUTDOWN	5


#define ISST_VER_KEY		0
#define ISST_VER		"1.0.0"
#define ISST_VER_DETAIL		"ISST 1.0.0 - U.S. Army Research Lab (2003 - 2005)"

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
