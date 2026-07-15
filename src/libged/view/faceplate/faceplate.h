/*                    F A C E P L A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file faceplate.h
 *
 * Private header for libged faceplate subcmds.
 *
 */

#ifndef GED_FACEPLATE_H
#define GED_FACEPLATE_H

#include "common.h"

struct bu_cmd_schema;
struct bu_cmd_tree;
struct bu_cmd_tree_node;
struct bu_cmdtab;
struct ged;

struct ged_faceplate_subcommand_args {
    int help;
};

__BEGIN_DECLS
extern int _fp_cmd_model_axes(void *bs, int argc, const char **argv);
extern int _fp_cmd_view_axes(void *bs, int argc, const char **argv);
extern int _fp_cmd_grid(void *bs, int argc, const char **argv);
extern int _fp_cmd_irect(void *bs, int argc, const char **argv);
extern const struct bu_cmd_schema ged_faceplate_subcommand_schema;
extern const struct bu_cmd_schema ged_faceplate_native_schema;
extern const struct bu_cmd_tree_node ged_faceplate_subcommands[];
extern const struct bu_cmd_tree ged_faceplate_tree;
extern int _fp_subcmd_exec(struct ged *gedp, const struct bu_cmd_schema *schema,
	const struct bu_cmdtab *cmds, const char *cmdname, const char *cmdargs,
	void *context, int argc, const char **argv, int help);
__END_DECLS

#endif /* GED_FACEPLATE_H */

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
