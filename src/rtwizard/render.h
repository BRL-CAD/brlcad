/*                        R E N D E R . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef RTWIZARD_RENDER_H
#define RTWIZARD_RENDER_H

#include "common.h"

struct rtwizard_settings;

__BEGIN_DECLS

struct rtwizard_render_callbacks {
    void (*log)(void *data, const char *message);
    void (*progress)(void *data, int completed, int total);
    void (*frame)(void *data, const char *filename, int frame, int total);
    int (*cancelled)(void *data);
};

/* Execute a validated rtwizard request without Tcl/Tk.  errmsg, when set, is
 * allocated with bu_malloc and is owned by the caller. */
int rtwizard_render(const struct rtwizard_settings *settings, char picture_type,
    const struct rtwizard_render_callbacks *callbacks, void *callback_data,
    char **errmsg);

__END_DECLS

#endif /* RTWIZARD_RENDER_H */
