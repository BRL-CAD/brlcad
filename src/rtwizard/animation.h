/*                     A N I M A T I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef RTWIZARD_ANIMATION_H
#define RTWIZARD_ANIMATION_H

#include "common.h"

struct rtwizard_settings;

__BEGIN_DECLS

int rtwizard_spec_apply(const char *path, struct rtwizard_settings *settings, char **errmsg);
int rtwizard_animation_spec_apply(const char *path, struct rtwizard_settings *settings, char **errmsg);

__END_DECLS

#ifdef __cplusplus
#include "animation_plan.h"

AnimationPlan rtwizard_track_plan(const rtwizard_settings *settings, const AnimationFrame &base, double local2base);
std::string rtwizard_keyframe_document(const rtwizard_settings *settings, const AnimationFrame &view, double local2base);
std::string operation_command(const DatabaseOperation &operation);
#endif

#endif /* RTWIZARD_ANIMATION_H */
