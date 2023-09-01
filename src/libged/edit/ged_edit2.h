/*                   G E D _ E D I T 2 . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @file ged_edit2.h
 *
 * Private header for libged edit2 cmd.
 *
 */

#ifndef LIBGED_EDIT2_GED_PRIVATE_H
#define LIBGED_EDIT2_GED_PRIVATE_H

#include "common.h"

#include <set>
#include <string>
#include <vector>
#include <time.h>

#include "ged.h"

__BEGIN_DECLS

#define HELPFLAG "--print-help"
#define PURPOSEFLAG "--print-purpose"

struct _ged_edit2_info {
    struct ged *gedp = NULL;
    int verbosity;
    const struct bu_cmdtab *cmds = NULL;
    struct bu_opt_desc *gopts = NULL;
    std::vector<std::string> geom_objs;  // TODO - probably will pre-digest this at the top level into comb instances and directory pointers, since that should be the same for all commands...
};

__END_DECLS

#endif /* LIBGED_EDIT2_GED_PRIVATE_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
