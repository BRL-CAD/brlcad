/*                      O B S E R V E R . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2022 United States Government as represented by
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

#include <string.h>

#include "bu/cmd.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/observer.h"

#define NUM_INITIAL_OBSERVERS 8

HIDDEN void
bu_observer_list_init(struct bu_observer_list *all_observers)
{
    if (all_observers->capacity == 0) {
	all_observers->capacity = NUM_INITIAL_OBSERVERS;
	all_observers->observers = (struct bu_observer *)bu_malloc(all_observers->capacity * sizeof(struct bu_observer), "init observer list");
    } else if (all_observers->size == all_observers->capacity) {
	all_observers->capacity *= 2;
	all_observers->observers = (struct bu_observer *)bu_realloc(all_observers->observers, all_observers->capacity * sizeof(struct bu_observer), "resize of observer list");
    }
}

/**
 * Attach observer.
 *
 * Usage:
 * attach observer [cmd]
 *
 */
HIDDEN int
observer_attach(void *clientData, int argc, const char **argv)
{
    struct bu_observer_list *observers = (struct bu_observer_list *)clientData;
    struct bu_observer *op;
    size_t i;

    if (argc < 2 || 3 < argc) {
	bu_log("ERROR: expecting only three arguments\n");
	return BRLCAD_ERROR;
    }

    /* see if it already exists, if so, modify it */
    for (i = 0; i < observers->size; i++) {
	op = &observers->observers[i];
	if (BU_STR_EQUAL(bu_vls_addr(&op->observer), argv[1])) {
	    if (argc == 2)
		/* clobber cmd */
		bu_vls_init(&op->cmd);
	    else
		/* overwrite cmd */
		bu_vls_strcpy(&op->cmd, argv[2]);

	    return BRLCAD_OK;
	}
    }

    /* acquire bu_observer struct */
    bu_observer_list_init(observers);
    op = &observers->observers[observers->size];
    observers->size++;

    /* initialize observer */
    bu_vls_init(&op->observer);
    bu_vls_strcpy(&op->observer, argv[1]);
    bu_vls_init(&op->cmd);

    if (argc == 3)
	bu_vls_strcpy(&op->cmd, argv[2]);

    return BRLCAD_OK;
}


/**
 * Detach observer.
 *
 * Usage:
 * detach observer
 *
 */
HIDDEN int
observer_detach(void *clientData, int argc, const char **argv)
{
    struct bu_observer_list *observers = (struct bu_observer_list *)clientData;
    struct bu_observer *op;
    size_t i;

    if (argc != 2) {
	bu_log("ERROR: expecting two arguments\n");
	return BRLCAD_ERROR;
    }

    /* search for observer and remove from list */
    for (i = 0; i < observers->size; i++) {
	op = &observers->observers[i];
	if (BU_STR_EQUAL(bu_vls_addr(&op->observer), argv[1])) {
	    bu_vls_free(&op->observer);
	    bu_vls_free(&op->cmd);

	    memmove(op, op + 1, sizeof(struct bu_observer) * (observers->size - i - 1));
	    observers->size--;

	    return BRLCAD_OK;
	}
    }

    bu_log("detach: %s not found", argv[1]);
    return BRLCAD_ERROR;
}


/**
 * Show/list observers.
 *
 * Usage:
 * show
 *
 */
HIDDEN int
observer_show(void *clientData, int argc, const char **UNUSED(argv))
{
    struct bu_observer_list *observers = (struct bu_observer_list *)clientData;
    struct bu_observer *op;
    size_t i;

    if (argc != 1) {
	bu_log("ERROR: expecting only one argument\n");
	return BRLCAD_ERROR;
    }

    for (i = 0; i < observers->size; i++) {
	op = &observers->observers[i];
	bu_log("%s - %s\n", bu_vls_addr(&op->observer), bu_vls_addr(&op->cmd));
    }

    return BRLCAD_OK;
}

void
bu_observer_notify(void *context, struct bu_observer_list *observers, char *self, bu_observer_eval_t *cmd_eval)
{
    struct bu_observer *op;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    size_t i;

    for (i = 0; i < observers->size; i++) {
	op = &observers->observers[i];
	if (bu_vls_strlen(&op->cmd) > 0) {
	    /* Execute cmd */
	    bu_vls_strcpy(&vls, bu_vls_addr(&op->cmd));
	    if (cmd_eval)
		(*cmd_eval)(context, bu_vls_addr(&vls));
	} else {
	    /* Assume that observer is some object that has an update method */
	    bu_vls_trunc(&vls, 0);
	    bu_vls_printf(&vls, "%s update %s", bu_vls_addr(&op->observer), self);
	    if (cmd_eval)
		(*cmd_eval)(context, bu_vls_addr(&vls));
	}
    }
    bu_vls_free(&vls);
}

void
bu_observer_free(struct bu_observer_list *observers)
{
    size_t i;

    for (i = 0; i < observers->size; i++) {
	bu_vls_free(&observers->observers[i].observer);
	bu_vls_free(&observers->observers[i].cmd);
    }

    bu_free(observers->observers, "freeing observers");
}


/**
 * observer commands for libdm and librt's dg_obj, view_obj, and
 * wdb_obj interfaces.
 */
static struct bu_cmdtab bu_observer_cmds[] = {
    {"attach",	observer_attach},
    {"detach",	observer_detach},
    {"show",	observer_show},
    {(const char *)NULL, BU_CMD_NULL}
};

int
bu_observer_cmd(void *clientData, int argc, const char **argv)
{
    int ret;
    if (bu_cmd(bu_observer_cmds, argc, argv, 0, clientData, &ret) == BRLCAD_OK)
	return ret;

    bu_log("ERROR: '%s' command not found\n", argv[0]);
    return BRLCAD_ERROR;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
