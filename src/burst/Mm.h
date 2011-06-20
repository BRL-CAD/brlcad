/*                            M M . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file burst/Mm.h
 *
 */

#ifndef __MM_H__
#define __MM_H__

/* Emulate MUVES Mm package using malloc. */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu.h"

#define MmAllo(typ)		(typ *) bu_malloc(sizeof(typ), BU_FLSTR)
#define MmFree(typ, ptr) bu_free((char *) ptr, BU_FLSTR)
#define MmVAllo(ct, typ)	(typ *) bu_malloc((ct)*sizeof(typ), BU_FLSTR)
#define MmVFree(ct, typ, ptr) bu_free((char *) ptr, BU_FLSTR)
#define MmStrDup(str) bu_strdup(str)
#define MmStrFree(str) bu_free(str, BU_FLSTR)

#endif  /* __MM_H__ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
