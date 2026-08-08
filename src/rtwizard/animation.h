/*                     A N I M A T I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef RTWIZARD_ANIMATION_H
#define RTWIZARD_ANIMATION_H

#include "tcl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Register private Tcl commands used to evaluate versioned render
 * specifications and capture camera keyframes. */
void rtwizard_animation_init(Tcl_Interp *interp);

/* Convert a declarative render specification into ordinary rtwizard options.
 * General animation tracks retain the specification path for later typed
 * evaluation.  The caller owns the returned vector. */
int rtwizard_spec_to_argv(const char *path, int *argc, char ***argv, char **errmsg);
void rtwizard_spec_argv_free(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* RTWIZARD_ANIMATION_H */
