/*                         H E L P . C
 * BRL-CAD
 *
 * Copyright (c) 2017 United States Government as represented by
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

#include "common.h"

#include "ged.h"


int
ged_help(struct ged *gedp, int argc, const char *argv[])
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 1 || !argv)
	return -1;

    return 0;
}


HIDDEN int
help_load(struct ged *gedp)
{
    int ret = 0;
    extern const struct ged_cmd *help_cmd(void);
    const struct ged_cmd *cmds = help_cmd();
    struct ged_cmd *cmd;

    while (BU_LIST_WHILE(cmd, ged_cmd, &(cmds->l))) {
	ret += gedp->add(gedp, cmd);
    }
    return ret;
}


HIDDEN void
help_unload(struct ged *gedp)
{
    gedp->del(gedp, "help");
    gedp->del(gedp, "apropos");
    gedp->del(gedp, "info");
    gedp->del(gedp, "man");
    gedp->del(gedp, "?");
}


const struct ged_cmd *
help_cmd(void)
{
    static struct ged_cmd cmd[6] = {
	{
	    BU_LIST_INIT_ZERO, "help",
	    "the BRL-CAD help system",
	    "help",
	    &help_load, &help_unload, &ged_help
	}, {
	    BU_LIST_INIT_ZERO, "apropos",
	    "the BRL-CAD help system",
	    "help",
	    &help_load, &help_unload, &ged_help
	}, {
	    BU_LIST_INIT_ZERO, "info",
	    "the BRL-CAD help system",
	    "help",
	    &help_load, &help_unload, &ged_help
	}, {
	    BU_LIST_INIT_ZERO, "man",
	    "the BRL-CAD help system",
	    "help",
	    &help_load, &help_unload, &ged_help
	}, {
	    BU_LIST_INIT_ZERO, "?",
	    "the BRL-CAD help system",
	    "help",
	    &help_load, &help_unload, &ged_help
	}, {
	    BU_LIST_INIT_ZERO, NULL, {0}, NULL, NULL, NULL, NULL
	}
    };

    BU_LIST_INIT_MAGIC(&(cmd[0].l), GED_CMD_MAGIC);
    BU_LIST_MAGIC_SET(&(cmd[1].l), GED_CMD_MAGIC);
    BU_LIST_MAGIC_SET(&(cmd[2].l), GED_CMD_MAGIC);
    BU_LIST_MAGIC_SET(&(cmd[3].l), GED_CMD_MAGIC);
    BU_LIST_MAGIC_SET(&(cmd[4].l), GED_CMD_MAGIC);

    BU_LIST_PUSH(&(cmd[0].l), &(cmd[1].l));
    BU_LIST_PUSH(&(cmd[0].l), &(cmd[2].l));
    BU_LIST_PUSH(&(cmd[0].l), &(cmd[3].l));
    BU_LIST_PUSH(&(cmd[0].l), &(cmd[4].l));

    return &cmd[0];
}


#ifdef STANDALONE
int main(int ac, char *av[])
{
    struct ged ged = {0};

    GED_INIT(&ged, NULL);

    help_load(&ged);
    ged_help(&ged, ac, (const char **)av);
    help_unload(&ged);

    return 0;
}
#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
