/*                           G L O B A L . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2026 United States Government as represented by
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

#include "tclcad.h"
#include "./tclcad_private.h"


struct tclcad_interp_object_list {
    struct bu_list objects;
    const char *description;
};


static void
tclcad_interp_objects_delete(ClientData clientData, Tcl_Interp *UNUSED(interp))
{
    struct tclcad_interp_object_list *state =
	(struct tclcad_interp_object_list *)clientData;

    if (BU_LIST_NON_EMPTY(&state->objects)) {
	bu_log("libtclcad %s objects remain during interpreter deletion\n",
		state->description);
	/* Command client data still references the head; do not leave it dangling. */
	return;
    }
    BU_PUT(state, struct tclcad_interp_object_list);
}


struct bu_list *
tclcad_interp_objects(Tcl_Interp *interp, const char *key,
	const char *description, int *created)
{
    struct tclcad_interp_object_list *state =
	(struct tclcad_interp_object_list *)Tcl_GetAssocData(interp, key, NULL);

    if (state) {
	if (created)
	    *created = 0;
	return &state->objects;
    }

    BU_GET(state, struct tclcad_interp_object_list);
    BU_LIST_INIT(&state->objects);
    state->description = description;
    Tcl_SetAssocData(interp, key, tclcad_interp_objects_delete,
	    (ClientData)state);
    if (created)
	*created = 1;

    return &state->objects;
}


struct tclcad_thread_state {
    struct tclcad_obj *top;
};

static Tcl_ThreadDataKey tclcad_thread_state_key;


struct tclcad_obj **
tclcad_current_top(void)
{
    struct tclcad_thread_state *state = (struct tclcad_thread_state *)
	Tcl_GetThreadData(&tclcad_thread_state_key, sizeof(struct tclcad_thread_state));
    return &state->top;
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
