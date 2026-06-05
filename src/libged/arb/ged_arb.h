/*                    G E D _ A R B . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
#ifndef LIBGED_ARB_GED_PRIVATE_H
#define LIBGED_ARB_GED_PRIVATE_H

#include "common.h"
#include "bu/cmd.h"
#include "rt/db4.h"
#include "ged.h"

__BEGIN_DECLS

struct _ged_arb_info {
    struct ged *gedp;
    const struct bu_cmdtab *cmds;
};

extern int _arb_cmd_create(void *bs, int argc, const char *argv[]);
extern int _arb_cmd_repair(void *bs, int argc, const char *argv[]);

__END_DECLS

#endif /* LIBGED_ARB_GED_PRIVATE_H */
