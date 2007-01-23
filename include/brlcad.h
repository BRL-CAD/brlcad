/*                        B R L C A D . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @addtogroup fixme */
/** @{ */
/** @file brlcad.h
 *@brief
 *  Convenience header for the core BRL-CAD libraries
 *
 *  This header includes the headers provided by libbu, libbn, libwdb,
 *  and librt.  It may be used in leu of including all of the
 *  individual headers.
 *
 *  @author
 *	Christopher Sean Morrison
 *
 *  @par Source
 *	BRL-CAD Open Source
 *
 *  $Header$
 */

#ifndef __BRLCAD_H__
#define __BRLCAD_H__

#include "common.h"

/* system headers presumed to be available */
#include <stdio.h>
#include <math.h>

/* machine definitions such as smp parameters.  this header will
 * eventually go away.
 */
#include "machine.h"

/* basic utilities */
#include "bu.h"

/* vector mathematics */
#include "vmath.h"

/* non-manifold geometry */
#include "nmg.h"

/* basic numerics */
#include "bn.h"

/* database format storage types */
#include "db.h"

/* raytrace interface constructs */
#include "raytrace.h"

/* trimmed nurb routines */
#include "nurb.h"

/* the write-only database library interface */
#include "wdb.h"

/* in-memory representations of the database geometry objects.  these
 * are subject to change and should not be relied upon.
 */
#include "rtgeom.h"


#endif  /* __BRLCAD_H__ */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

