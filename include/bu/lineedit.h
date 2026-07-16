/*                    L I N E E D I T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the U.S.
 * Army Research Laboratory.
 */

/** @file bu/lineedit.h
 * Generic policy names used by interactive command-line editors.
 */

#ifndef BU_LINEEDIT_H
#define BU_LINEEDIT_H

#include "common.h"
#include <stddef.h>
#include "bu/defines.h"

__BEGIN_DECLS

typedef enum {
    BU_CMD_COMPLETE_FILTER = 0, /**< Preview candidates; refining edits apply to the original seed. */
    BU_CMD_COMPLETE_CYCLE,     /**< Preview candidates; edits accept the current candidate. */
    BU_CMD_COMPLETE_PREFIX,    /**< Insert the longest common candidate prefix. */
    BU_CMD_COMPLETE_LEGACY,    /**< Preserve an application's historical behavior. */
    BU_CMD_COMPLETE_OFF        /**< Disable context-sensitive completion. */
} bu_cmd_completion_mode_t;

#define BU_CMD_COMPLETION_MODE_ENV "BRLCAD_COMPLETION_MODE"

/**
 * Semantic roles used by BRL-CAD interactive command-line editors.  These
 * roles deliberately describe command language meaning rather than a
 * particular application's user-interface theme.
 */
typedef enum {
    BU_LINEEDIT_ROLE_COMMAND = 0,
    BU_LINEEDIT_ROLE_OPTION,
    BU_LINEEDIT_ROLE_VALID,
    BU_LINEEDIT_ROLE_INVALID,
    BU_LINEEDIT_ROLE_INCOMPLETE,
    BU_LINEEDIT_ROLE_COMPLETION_PREVIEW,
    BU_LINEEDIT_ROLE_COUNT
} bu_lineedit_role_t;

/** A configured foreground color and text style for one semantic role. */
struct bu_lineedit_style {
    unsigned char rgb[3];
    unsigned int flags;
};

/** @name bu_lineedit_style flags */
/** @{ */
#define BU_LINEEDIT_STYLE_COLOR   0x1u /**< rgb contains an explicit color */
#define BU_LINEEDIT_STYLE_DIM_SET 0x2u /**< dim has been explicitly selected */
#define BU_LINEEDIT_STYLE_DIM     0x4u /**< render this role dimly */
/** @} */

/** A set of optional overrides for all line-editor semantic roles. */
struct bu_lineedit_palette {
    struct bu_lineedit_style roles[BU_LINEEDIT_ROLE_COUNT];
};

#define BU_LINEEDIT_PALETTE_INIT_ZERO {{{{0, 0, 0}, 0}}}

/** Global override path for the user line-editor palette. */
#define BU_LINEEDIT_COLORS_ENV "BRLCAD_LINEEDIT_COLORS"

/**
 * A terminal-width-aware presentation of a completion candidate set.
 *
 * Short candidate sets are arranged in columns using no more than the
 * requested number of lines.  Larger sets use a complete multi-line frontier
 * of compact prefix bins such as "aab (3)  aac (20)" when possible, with a
 * one-line partial frontier as a last resort.  Candidate insertion and
 * cycling continue to use the original, unmodified candidate set; this
 * structure is presentation only.
 */
struct bu_cmd_completion_layout {
    size_t line_count;
    char **lines;
    int summarized;
};

#define BU_CMD_COMPLETION_LAYOUT_INIT_ZERO {0, NULL, 0}

/** Parse a stable, case-insensitive completion mode name. */
BU_EXPORT extern int bu_cmd_completion_mode_parse(const char *name, bu_cmd_completion_mode_t *mode);

/** Return the stable lower-case name for @p mode. */
BU_EXPORT extern const char *bu_cmd_completion_mode_name(bu_cmd_completion_mode_t mode);

/**
 * Resolve a mode from @p frontend_env, then BRLCAD_COMPLETION_MODE.
 * Invalid and empty values are skipped; @p fallback is returned if neither
 * environment variable contains a recognized mode.
 */
BU_EXPORT extern bu_cmd_completion_mode_t bu_cmd_completion_mode_from_env(const char *frontend_env, bu_cmd_completion_mode_t fallback);

/** Set all of @p palette's roles to inherit their frontend defaults. */
BU_EXPORT extern void bu_lineedit_palette_init(struct bu_lineedit_palette *palette);

/**
 * Merge the JSON color configuration at @p path into @p palette.
 *
 * The file must be a version 1 object whose optional "colors" object names
 * semantic roles.  A role value is either a #RRGGBB (or #RGB) string or an
 * object with optional "color" and "style" ("dim" or "normal") members.
 * The palette is not changed if the file is invalid.
 */
BU_EXPORT extern int bu_lineedit_palette_load_file(struct bu_lineedit_palette *palette, const char *path);

/**
 * Merge the shared user palette into @p palette, if present.
 *
 * BRLCAD_LINEEDIT_COLORS names an explicit configuration file; the special
 * values "off" and "none" disable user palette loading.  Otherwise the
 * default is $XDG_CONFIG_HOME/brlcad/lineedit.json when XDG_CONFIG_HOME is
 * set, or BRL-CAD's user configuration directory with brlcad/lineedit.json
 * appended.  A present but invalid selected file is reported and ignored.
 */
BU_EXPORT extern int bu_lineedit_palette_load_user(struct bu_lineedit_palette *palette);

/** Return a stable lower-case name for @p role, or NULL for an invalid role. */
BU_EXPORT extern const char *bu_lineedit_role_name(bu_lineedit_role_t role);

/**
 * Lay out completion candidates for a transient frontend display.
 *
 * @p width is the available number of character cells.  Zero selects 80.
 * @p max_lines is the maximum number of lines used for a full candidate list
 * or complete prefix-bin frontier; zero selects five.  Complete multi-line
 * frontiers use compact "prefix (count)" labels.  If neither can fit, the best
 * partial prefix frontier that fits on one line is returned with an explicit
 * omitted-candidate count.  Empty and duplicate candidates are ignored.
 * Returns BRLCAD_OK on success, including an empty result.
 */
BU_EXPORT extern int bu_cmd_completion_layout_create(
	struct bu_cmd_completion_layout *layout,
	const char * const *candidates, size_t candidate_count,
	size_t width, size_t max_lines);

/** Release all strings owned by @p layout and reset it to empty. */
BU_EXPORT extern void bu_cmd_completion_layout_clear(
	struct bu_cmd_completion_layout *layout);

__END_DECLS

#endif /* BU_LINEEDIT_H */
