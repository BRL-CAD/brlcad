/*                          R I S E . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2005-2007 United States Government as represented by
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
/** @file rise.h
 *
 *  Author -
 *      Justin L. Shumaker
 *
 */

#ifndef _RISE_H
#define _RISE_H

#include "tie.h"
#include "adrt_common.h"

#define	RISE_OP_CAMERA		0

#define	RISE_PIXEL_FMT		0	/* 0 == unsigned char, 1 ==  TFLOAT */

#define	RISE_OBSERVER_PORT	1982

#define	RISE_USE_COMPRESSION	0

#define	RISE_NET_OP_INIT	0
#define	RISE_NET_OP_FRAME	1
#define	RISE_NET_OP_QUIT	2
#define	RISE_NET_OP_SHUTDOWN	3

#define RISE_VER_KEY		0
#define RISE_VER		"0.1.3"
#define RISE_VER_DETAIL		"RISE 0.1.3 - Copyright (C) U.S Army Research Laboratory (2003 - 2005)"

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
