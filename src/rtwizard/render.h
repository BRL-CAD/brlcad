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

int rtwizard_render(const struct rtwizard_settings *settings, char picture_type,
    const struct rtwizard_render_callbacks *callbacks, void *callback_data,
    char **errmsg);

int rtwizard_fbserv_start(char *target, size_t target_size, int width,
    int height, const char *device, int port, char **errmsg);
void rtwizard_fbserv_stop(void);

__END_DECLS

#endif /* RTWIZARD_RENDER_H */
