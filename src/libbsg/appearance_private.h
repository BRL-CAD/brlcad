/*              A P P E A R A N C E _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file appearance_private.h
 *
 * Source-private raw-node appearance accessors.  Installed callers must use
 * typed refs, scene refs, or render records.
 */

#ifndef BSG_APPEARANCE_PRIVATE_H
#define BSG_APPEARANCE_PRIVATE_H

#include "common.h"
#include "node_fwd_private.h"
#include "bsg/appearance.h"
#include "bsg/appearance_action.h"

__BEGIN_DECLS

struct bsg_render_settings;

BSG_EXPORT extern void bsg_appearance_set_visible(bsg_node *node, int visible);
BSG_EXPORT extern int bsg_appearance_visible(const bsg_node *node);
BSG_EXPORT extern void bsg_appearance_set_force_draw(bsg_node *node, int force_draw);
BSG_EXPORT extern int bsg_appearance_force_draw(const bsg_node *node);
BSG_EXPORT extern void bsg_appearance_set_line_style(bsg_node *node, int dashed);
BSG_EXPORT extern int bsg_appearance_line_style(const bsg_node *node);
BSG_EXPORT extern void bsg_appearance_set_line_width(bsg_node *node, int line_width);
BSG_EXPORT extern int bsg_appearance_line_width(const bsg_node *node);
extern int bsg_appearance_apply_settings(bsg_node *node,
	const struct bsg_appearance_settings *settings);
extern void bsg_appearance_settings_for_node(const bsg_node *node,
	struct bsg_appearance_settings *settings);
BSG_EXPORT extern void bsg_appearance_set_inherited_by_children(bsg_node *node, int inherited);
BSG_EXPORT extern int bsg_appearance_inherited_by_children(const bsg_node *node);
BSG_EXPORT extern int bsg_appearance_color_override(const bsg_node *node, unsigned char color[3]);
BSG_EXPORT extern void bsg_appearance_set_color_override(bsg_node *node,
	const unsigned char color[3], int enabled);
BSG_EXPORT extern int bsg_appearance_uses_default_color(const bsg_node *node);
extern void bsg_appearance_set_work_flag(bsg_node *node, int wflag);
extern void bsg_appearance_set_legacy_color_info(bsg_node *node,
	const unsigned char basecolor[3], int user_color, int default_color);
BSG_EXPORT extern void bsg_appearance_legacy_basecolor(const bsg_node *node,
	unsigned char basecolor[3]);
BSG_EXPORT extern int bsg_appearance_legacy_user_color(const bsg_node *node);
BSG_EXPORT extern int bsg_appearance_legacy_default_color(const bsg_node *node);
BSG_EXPORT extern void bsg_appearance_set_legacy_uses_default_color(bsg_node *node,
	int default_color);
BSG_EXPORT extern void bsg_appearance_set_legacy_eval_flag(bsg_node *node, int eflag);
BSG_EXPORT extern int bsg_appearance_legacy_eval_flag(const bsg_node *node);
extern void bsg_appearance_set_legacy_region_id(bsg_node *node, int region_id);
BSG_EXPORT extern int bsg_appearance_legacy_region_id(const bsg_node *node);
BSG_EXPORT extern void bsg_appearance_set_highlighted(bsg_node *node, int highlighted);
BSG_EXPORT extern int bsg_appearance_is_highlighted(const bsg_node *node);
extern void bsg_appearance_set_highlight_state(bsg_node *node, char state);
extern char bsg_appearance_highlight_state(const bsg_node *node);
extern void bsg_appearance_set_changed(bsg_node *node, int changed);
extern int bsg_appearance_get_changed(const bsg_node *node);
BSG_EXPORT extern uint64_t bsg_appearance_drawn_rev(const bsg_node *node);
BSG_EXPORT extern int bsg_appearance_dmode(const bsg_node *node);
BSG_EXPORT extern void bsg_appearance_set_dmode(bsg_node *node, int draw_mode);
BSG_EXPORT extern int bsg_appearance_strict_fallback(const bsg_node *node);
BSG_EXPORT extern void bsg_appearance_set_strict_fallback(bsg_node *node, int strict_fallback);
BSG_EXPORT extern fastf_t bsg_appearance_transparency(const bsg_node *node);
BSG_EXPORT extern void bsg_appearance_set_transparency(bsg_node *node, fastf_t t);
BSG_EXPORT extern int bsg_appearance_resolve(const struct bsg_view *v,
	const bsg_node *node,
	const struct bsg_appearance_settings *inherited_os,
	struct bsg_resolved_appearance *out);
BSG_EXPORT extern int bsg_appearance_resolve_with_settings(
	const struct bsg_render_settings *settings,
	const bsg_node *node,
	const struct bsg_appearance_settings *inherited_os,
	struct bsg_resolved_appearance *out);

__END_DECLS

#endif /* BSG_APPEARANCE_PRIVATE_H */
