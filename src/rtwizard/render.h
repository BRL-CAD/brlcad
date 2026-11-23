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
struct rtwizard_preview;

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

struct rtwizard_preview *rtwizard_preview_start(const struct rtwizard_settings *settings,
    char picture_type, char **errmsg);
int rtwizard_preview_poll(struct rtwizard_preview *preview, int *running, int *ready,
    int *total, char **errmsg);
const char *rtwizard_preview_directory(const struct rtwizard_preview *preview);
int rtwizard_preview_cancel(struct rtwizard_preview *preview, char **errmsg);
void rtwizard_preview_destroy(struct rtwizard_preview *preview);

int rtwizard_fbserv_start(char *target, size_t target_size, int width,
    int height, const char *device, int port, char **errmsg);
void rtwizard_fbserv_stop(void);

__END_DECLS

#endif /* RTWIZARD_RENDER_H */
