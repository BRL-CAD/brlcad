/*                         D E B U G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

#ifndef ANALYZE_DEBUG_H
#define ANALYZE_DEBUG_H

#include "common.h"
#include "analyze/defines.h"

__BEGIN_DECLS

/** @addtogroup libanalyze
 *
 * @brief
 * Debugging definitions.
 *
 */
/** @{ */
/** @file analyze/debug.h */

/**
 * controls the libanalyze debug reporting
 */
ANALYZE_EXPORT extern unsigned int analyze_debug;

/**
 * Section for ANALYZE_DEBUG values
 *
 * These definitions are each for one bit.
 */
#define ANALYZE_DEBUG_OFF 0  /* No debugging */

#define ANALYZE_DEBUG_NIRT_BACKOUT   0x00000001      /* report on backout calculations */
#define ANALYZE_DEBUG_NIRT_HITS      0x00000002      /* report on nirt hits*/
#define ANALYZE_DEBUG_UNUSED_0       0x00000004      /* Unallocated */
#define ANALYZE_DEBUG_UNUSED_1       0x00000008      /* Unallocated */

/** @} */

__END_DECLS

#endif  /* ANALYZE_DEBUG_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
