/*                         D E B U G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

/**  @defgroup io Input/Output */
/**   @defgroup debug Debugging */

/** @file debug.h
 *
 */
#ifndef BU_DEBUG_H
#define BU_DEBUG_H

#include "common.h"
#include "bu/defines.h"

/** @addtogroup debug Debugging */
/** @{ */

/**
 * controls the libbu debug level
 */
BU_EXPORT extern int bu_debug;

/**
 * Section for BU_DEBUG values
 *
 * These can be set from the command-line of RT-compatible programs
 * using the "-!" option.
 *
 * These definitions are each for one bit.
 */
#define BU_DEBUG_OFF 0  /* No debugging */

#define BU_DEBUG_COREDUMP       0x00000001      /* bu_bomb() should dump core on exit */
#define BU_DEBUG_MEM_CHECK      0x00000002      /* Mem barrier & leak checking */
#define BU_DEBUG_MEM_LOG        0x00000004      /* Print all dynamic memory operations */
#define BU_DEBUG_UNUSED_0       0x00000008      /* unused */

#define BU_DEBUG_PARALLEL       0x00000010      /* Parallel debug logging */
#define BU_DEBUG_MEM_QCHECK     0x00000020      /* Fast mem leak checking (won't work with corruption) */
#define BU_DEBUG_BACKTRACE      0x00000040      /* Log backtrace details during abnormal exit */
#define BU_DEBUG_ATTACH         0x00000080      /* Waits for a debugger to attach during a crash */

#define BU_DEBUG_MATH           0x00000100      /* Fundamental math routines (plane.c, mat.c) */
#define BU_DEBUG_PTBL           0x00000200      /* bu_ptbl_*() logging */
#define BU_DEBUG_AVS            0x00000400      /* bu_avs_*() logging */
#define BU_DEBUG_MAPPED_FILE    0x00000800      /* bu_mapped_file logging */

#define BU_DEBUG_PATHS          0x00001000      /* File and path debug logging */
#define BU_DEBUG_UNUSED_1       0x00002000      /* unused */
#define BU_DEBUG_UNUSED_2       0x00004000      /* unused */
#define BU_DEBUG_UNUSED_3       0x00008000      /* unused */

#define BU_DEBUG_TABDATA        0x00010000      /* LIBBN: tabdata */
#define BU_DEBUG_UNUSED_4       0x00020000      /* unused */
#define BU_DEBUG_UNUSED_5       0x00040000      /* unused */
#define BU_DEBUG_UNUSED_6       0x00080000      /* unused */

/* Format string for bu_printb() */
#define BU_DEBUG_FORMAT \
    "\020\
\025TABDATA\
\015?\
\014MAPPED_FILE\013AVS\012PTBL\011MATH\010?\7?\6MEM_QCHECK\5PARALLEL\
\4?\3MEM_LOG\2MEM_CHECK\1COREDUMP"

/** @} */

#endif  /* BU_DEBUG_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
