/*                     A N I M A T I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef RTWIZARD_ANIMATION_H
#define RTWIZARD_ANIMATION_H

#include "tcl.h"

struct rtwizard_settings;

#ifdef __cplusplus
extern "C" {
#endif

/* Register private Tcl commands used to evaluate versioned render
 * specifications and capture camera keyframes. */
void rtwizard_animation_init(Tcl_Interp *interp);

/* Load a declarative render specification into initialized settings. */
int rtwizard_spec_apply(const char *path, struct rtwizard_settings *settings, char **errmsg);

/* Overlay an animation-only JSON specification on initialized settings. */
int rtwizard_animation_spec_apply(const char *path, struct rtwizard_settings *settings, char **errmsg);

#ifdef __cplusplus
}
#endif

#endif /* RTWIZARD_ANIMATION_H */
