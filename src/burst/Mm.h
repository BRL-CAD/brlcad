/*                            M M . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

#ifndef BURST_MM_H
#define BURST_MM_H

/* Emulate MUVES Mm package using malloc. */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/malloc.h"
#include "bu/str.h"

#define MmAllo(typ)		(typ *) bu_malloc(sizeof(typ), CPP_FILELINE)
#define MmFree(typ, ptr) bu_free((char *) ptr, CPP_FILELINE)
#define MmVAllo(ct, typ)	(typ *) bu_malloc((ct)*sizeof(typ), CPP_FILELINE)
#define MmVFree(ct, typ, ptr) bu_free((char *) ptr, CPP_FILELINE)
#define MmStrDup(str) bu_strdup(str)
#define MmStrFree(str) bu_free(str, CPP_FILELINE)

#endif  /* BURST_MM_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
