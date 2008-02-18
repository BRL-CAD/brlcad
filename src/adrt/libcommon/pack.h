/*                     P A C K . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2008 United States Government as represented by
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
/** @file pack.h
 *
 *  Common Library - Parsing and Packing Header
 *
 */

#ifndef _COMMON_PACK_H
#define _COMMON_PACK_H

#include "cdb.h"

#define COMMON_PACK_ENV_RM		0x0300
#define COMMON_PACK_ENV_IMAGESIZE	0x0301

#define COMMON_PACK_MESH_NEW		0x0400


extern	int	common_pack(common_db_t *db, void **app_data, char *proj);
extern	void	common_pack_write(void** dest, int* ind, const void* src, size_t size);

extern	int	common_pack_trinum;

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
