/*                        C M D . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
/** @file rt/cmd.h
 *
 */

#ifndef RT_CMD_H
#define RT_CMD_H

#include "common.h"

/* system headers */
#include <stdio.h> /* for FILE */

/* interface headers */
#include "vmath.h"
#include "rt/defines.h"
#include "rt/rt_instance.h"

__BEGIN_DECLS

/**
 * Table for driving generic command-parsing routines
 */
struct command_tab {
    const char *ct_cmd;
    const char *ct_parms;
    const char *ct_comment;
    int (*ct_func)(const int, const char **);
    int ct_min;         /**< @brief  min number of words in cmd */
    int ct_max;         /**< @brief  max number of words in cmd */
};

/* cmd.c */
/* Read semi-colon terminated line */

/*
 * Read one semi-colon terminated string of arbitrary length from the
 * given file into a dynamically allocated buffer.  Various commenting
 * and escaping conventions are implemented here.
 *
 * Returns:
 * NULL on EOF
 * char * on good read
 */
RT_EXPORT extern char *rt_read_cmd(FILE *fp);
/* do cmd from string via cmd table */

/*
 * Slice up input buffer into whitespace separated "words", look up
 * the first word as a command, and if it has the correct number of
 * args, call that function.  Negative min/max values in the tp
 * command table effectively mean that they're not bounded.
 *
 * Expected to return -1 to halt command processing loop.
 *
 * Based heavily on mged/cmd.c by Chuck Kennedy.
 *
 * DEPRECATED: needs to migrate to libbu
 */
RT_EXPORT extern int rt_do_cmd(struct rt_i *rtip,
			       const char *ilp,
			       const struct command_tab *tp);


/**
 * @brief Convert an ASCII v5 "attr" command into a .g attribute
 *
 * Given an array of strings formatted as a BRL-CAD v5 ASCII "attr" command,
 * translate the information in that command into an attribute setting for the
 * dbip.
 *
 * The msg parameter is optional - if present, rt_cmd_attr will output
 * diagnostic messages to the vls in the event of an error.
 *
 * Returns BRLCAD_OK on success, BRLCAD_ERROR on failure.
 */
RT_EXPORT extern int rt_cmd_attr(struct bu_vls *msg, struct db_i *dbip, int argc, const char **argv);

/**
 * @brief Convert an ASCII v5 "color" command into a .g coloribute
 *
 * Given an array of strings formatted as a BRL-CAD v5 ASCII "color" command,
 * translate the information in that command into an coloribute setting for the
 * dbip.
 *
 * The msg parameter is optional - if present, rt_cmd_color will output
 * diagnostic messages to the vls in the event of an error.
 *
 * Returns BRLCAD_OK on success, BRLCAD_ERROR on failure.
 */
RT_EXPORT extern int rt_cmd_color(struct bu_vls *msg, struct db_i *dbip, int argc, const char **argv);

/**
 * @brief Convert an ASCII v5 "put" command into a .g object
 *
 * Given an array of strings formatted as a BRL-CAD v5 ASCII "put" command,
 * translate the information in that command into an object in the dbip.
 *
 * The msg parameter is optional - if present, rt_cmd_put will output
 * diagnostic messages to the vls in the event of an error.
 *
 * Returns BRLCAD_OK on success, BRLCAD_ERROR on failure.
 */
RT_EXPORT extern int rt_cmd_put(struct bu_vls *msg, struct db_i *dbip, int argc, const char **argv);

/**
 * @brief Convert an ASCII v5 "title" command into a .g title
 *
 * Given an array of strings formatted as a BRL-CAD v5 ASCII "title" command,
 * translate the information in that command into a new title for the dbip.
 *
 * The msg parameter is optional - if present, rt_cmd_title will output
 * diagnostic messages to the vls in the event of an error.
 *
 * Returns BRLCAD_OK on success, BRLCAD_ERROR on failure.
 */
RT_EXPORT extern int rt_cmd_title(struct bu_vls *msg, struct db_i *dbip, int argc, const char **argv);

/**
 * @brief Convert an ASCII v5 "units" command into a .g units
 *
 * Given an array of strings formatted as a BRL-CAD v5 ASCII "units" command,
 * translate the information in that command into a new units for the dbip.
 *
 * The msg parameter is optional - if present, rt_cmd_units will output
 * diagnostic messages to the vls in the event of an error.
 *
 * Returns BRLCAD_OK on success, BRLCAD_ERROR on failure.
 */
RT_EXPORT extern int rt_cmd_units(struct bu_vls *msg, struct db_i *dbip, int argc, const char **argv);


__END_DECLS

#endif /* RT_CMD_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
