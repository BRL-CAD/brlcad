/*                      D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

/** @addtogroup qged_defines
 *
 * @brief
 * These are definitions specific to libqged
 *
 */
/** @{ */
/** @file qged/defines.h */

#ifndef QGED_DEFINES_H
#define QGED_DEFINES_H

#include "common.h"
#include "brlcad_version.h"

#ifndef QGED_EXPORT
#  if defined(QGED_DLL_EXPORTS) && defined(QGED_DLL_IMPORTS)
#    error "Only QGED_DLL_EXPORTS or QGED_DLL_IMPORTS can be defined, not both."
#  elif defined(QGED_DLL_EXPORTS)
#    define QGED_EXPORT COMPILER_DLLEXPORT
#  elif defined(QGED_DLL_IMPORTS)
#    define QGED_EXPORT COMPILER_DLLIMPORT
#  else
#    define QGED_EXPORT
#  endif
#endif

struct qged_tool_impl;
struct qged_tool {
    struct qged_tool_impl *i;
};

struct qged_plugin {
    uint32_t api_version; /* must be first in struct */
    const struct qged_tool ** const cmds;
    int cmd_cnt;
};

#define QGED_CMD_PLUGIN  (3*1000000 + (BRLCAD_VERSION_MAJOR*10000) + (BRLCAD_VERSION_MINOR*100) + BRLCAD_VERSION_PATCH)
#define QGED_VC_TOOL_PLUGIN (4*1000000 + (BRLCAD_VERSION_MAJOR*10000) + (BRLCAD_VERSION_MINOR*100) + BRLCAD_VERSION_PATCH)
#define QGED_IC_TOOL_PLUGIN (5*1000000 + (BRLCAD_VERSION_MAJOR*10000) + (BRLCAD_VERSION_MINOR*100) + BRLCAD_VERSION_PATCH)
#define QGED_OC_TOOL_PLUGIN (6*1000000 + (BRLCAD_VERSION_MAJOR*10000) + (BRLCAD_VERSION_MINOR*100) + BRLCAD_VERSION_PATCH)

typedef void * (*qged_func_ptr)();
struct qged_tool_impl {
    qged_func_ptr tool_create;
};

#endif  /* QGED_DEFINES_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
