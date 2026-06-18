/*  T E S T _ A P I _ H Y G I E N E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file libbsg/tests/test_api_hygiene.c
 *
 * Source-tree guard tests for the public drawing API boundary.
 */

#include "common.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"

struct hygiene_rule {
    const char *root;
    const char *pattern;
    const char *message;
    const char *allowed_path_a;
    const char *allowed_path_b;
    const char *allowed_prefix;
};

#define DRAWING_HEADER_RULES(_pattern, _message) \
    { "include/bsg", _pattern, _message, NULL, NULL, NULL }, \
    { "include/ged", _pattern, _message, NULL, NULL, NULL }, \
    { "include/dm", _pattern, _message, NULL, NULL, NULL }, \
    { "include/tclcad", _pattern, _message, NULL, NULL, NULL }

#define DRAWING_SOURCE_RULES(_pattern, _message) \
    { "src/libbsg", _pattern, _message, "src/libbsg/tests/test_api_hygiene.c", NULL, NULL }, \
    { "src/libged", _pattern, _message, NULL, NULL, NULL }, \
    { "src/libdm", _pattern, _message, NULL, NULL, NULL }, \
    { "src/libtclcad", _pattern, _message, NULL, NULL, NULL }, \
    { "src/libqtcad", _pattern, _message, NULL, NULL, NULL }, \
    { "src/mged", _pattern, _message, NULL, NULL, NULL }, \
    { "src/qged", _pattern, _message, NULL, NULL, NULL }

static int g_failures = 0;

/*
 * OPENINVENTOR_MIGRATION_STATUS
 *
 * Stage 0 locks the current post-Stage-D API boundary while the public BSG
 * model moves toward a C OpenInventor-style scene graph.  Installed headers may
 * still expose current opaque refs, typed drawing records, view records, and
 * typed payload records needed by in-tree producers.  They must not re-expose
 * raw node-kind bit definitions, bsg_node storage layout, generic lifecycle or
 * payload callback slots, draw-root lifecycle APIs, raw payload binding, or
 * node-owned vlist mutation helpers.
 *
 * Later stages replace the remaining scene-ref/payload authoring surface with
 * typed objects, nodes, fields, actions, and traversal state.
 */
static const char * const OPENINVENTOR_MIGRATION_STATUS[] = {
    "Stage 0: lock the post-Stage-D public drawing boundary",
    "allow current opaque refs and typed drawing records until their named migration stages",
    "reject public raw node taxonomy, storage, root lifecycle, payload binding, and node-vlist mutation"
};

static int
_has_suffix(const char *s, const char *suffix)
{
    size_t slen;
    size_t tlen;

    if (!s || !suffix)
	return 0;
    slen = strlen(s);
    tlen = strlen(suffix);
    return slen >= tlen && bu_strcmp(s + slen - tlen, suffix) == 0;
}

static int
_is_source_file(const char *path)
{
    return _has_suffix(path, ".h") ||
	_has_suffix(path, ".c") ||
	_has_suffix(path, ".cpp") ||
	_has_suffix(path, ".cxx") ||
	_has_suffix(path, ".hpp") ||
	_has_suffix(path, ".md") ||
	_has_suffix(path, ".txt");
}

static int
_path_matches_list(const char *rel, const char *allowed)
{
    const char *start = allowed;
    if (!rel || !allowed)
	return 0;
    while (start && *start) {
	const char *end = strchr(start, ';');
	size_t len = end ? (size_t)(end - start) : strlen(start);
	if (strlen(rel) == len && bu_strncmp(rel, start, len) == 0)
	    return 1;
	start = end ? end + 1 : NULL;
    }
    return 0;
}

static int
_path_allowed(const char *rel, const struct hygiene_rule *rule)
{
    if (!rule || !rel)
	return 0;
    if (_path_matches_list(rel, rule->allowed_path_a))
	return 1;
    if (_path_matches_list(rel, rule->allowed_path_b))
	return 1;
    if (rule->allowed_prefix &&
	    bu_strncmp(rel, rule->allowed_prefix, strlen(rule->allowed_prefix)) == 0)
	return 1;
    return 0;
}

static void
_check_line(const char *rel, const char *line, size_t lineno,
	    const struct hygiene_rule *rule)
{
    if (!rel || !line || !rule || !rule->pattern)
	return;
    if (!strstr(line, rule->pattern))
	return;
    if (_path_allowed(rel, rule))
	return;
    fprintf(stderr, "%s:%zu: %s: found '%s'\n",
	    rel, lineno, rule->message, rule->pattern);
    g_failures++;
}

static void
_scan_file(const char *abs, const char *rel,
	   const struct hygiene_rule *rules, size_t rule_count)
{
    FILE *fp;
    char line[4096];
    size_t lineno = 0;

    if (!_is_source_file(abs))
	return;

    fp = fopen(abs, "r");
    if (!fp)
	return;

    while (bu_fgets(line, sizeof(line), fp)) {
	lineno++;
	for (size_t i = 0; i < rule_count; i++)
	    _check_line(rel, line, lineno, &rules[i]);
    }

    fclose(fp);
}

static void
_scan_tree(const char *srcroot, const char *relroot,
	   const struct hygiene_rule *rules, size_t rule_count)
{
    char abs[4096];
    DIR *dirp;
    struct dirent *dp;

    snprintf(abs, sizeof(abs), "%s/%s", srcroot, relroot);
    dirp = opendir(abs);
    if (!dirp) {
	fprintf(stderr, "Unable to open %s: %s\n", abs, strerror(errno));
	g_failures++;
	return;
    }

    while ((dp = readdir(dirp)) != NULL) {
	char child_abs[4096];
	char child_rel[4096];
	struct stat sb;

	if (bu_strcmp(dp->d_name, ".") == 0 || bu_strcmp(dp->d_name, "..") == 0)
	    continue;
	if (bu_strcmp(dp->d_name, ".git") == 0 || bu_strcmp(dp->d_name, "build") == 0)
	    continue;

	snprintf(child_rel, sizeof(child_rel), "%s/%s", relroot, dp->d_name);
	snprintf(child_abs, sizeof(child_abs), "%s/%s", srcroot, child_rel);
	if (stat(child_abs, &sb) != 0)
	    continue;
	if (S_ISDIR(sb.st_mode)) {
	    _scan_tree(srcroot, child_rel, rules, rule_count);
	} else if (S_ISREG(sb.st_mode)) {
	    _scan_file(child_abs, child_rel, rules, rule_count);
	}
    }

    closedir(dirp);
}

static int
_path_in_list(const char *rel, const char * const *paths, size_t path_count)
{
    if (!rel || !paths)
	return 0;
    for (size_t i = 0; i < path_count; i++) {
	if (paths[i] && bu_strcmp(rel, paths[i]) == 0)
	    return 1;
    }
    return 0;
}

static int
_path_prefix_in_list(const char *rel, const char * const *prefixes,
		     size_t prefix_count)
{
    if (!rel || !prefixes)
	return 0;
    for (size_t i = 0; i < prefix_count; i++) {
	size_t len;
	if (!prefixes[i])
	    continue;
	len = strlen(prefixes[i]);
	if (bu_strncmp(rel, prefixes[i], len) == 0)
	    return 1;
    }
    return 0;
}

static void
_scan_file_for_patterns_outside_allowed(const char *abs,
					const char *rel,
					const char * const *patterns,
					size_t pattern_count,
					const char * const *allowed_paths,
					size_t allowed_path_count,
					const char * const *allowed_prefixes,
					size_t allowed_prefix_count,
					const char *message)
{
    FILE *fp;
    char line[4096];
    size_t lineno = 0;

    if (!_is_source_file(abs))
	return;
    if (_path_in_list(rel, allowed_paths, allowed_path_count) ||
	    _path_prefix_in_list(rel, allowed_prefixes, allowed_prefix_count))
	return;

    fp = fopen(abs, "r");
    if (!fp)
	return;

    while (bu_fgets(line, sizeof(line), fp)) {
	lineno++;
	for (size_t i = 0; i < pattern_count; i++) {
	    if (!patterns[i] || !strstr(line, patterns[i]))
		continue;
	    fprintf(stderr, "%s:%zu: %s: found '%s'\n",
		    rel, lineno, message, patterns[i]);
	    g_failures++;
	}
    }

    fclose(fp);
}

static void
_scan_tree_for_patterns_outside_allowed(const char *srcroot,
					const char *relroot,
					const char * const *patterns,
					size_t pattern_count,
					const char * const *allowed_paths,
					size_t allowed_path_count,
					const char * const *allowed_prefixes,
					size_t allowed_prefix_count,
					const char *message)
{
    char abs[4096];
    DIR *dirp;
    struct dirent *dp;

    snprintf(abs, sizeof(abs), "%s/%s", srcroot, relroot);
    dirp = opendir(abs);
    if (!dirp) {
	fprintf(stderr, "Unable to open %s: %s\n", abs, strerror(errno));
	g_failures++;
	return;
    }

    while ((dp = readdir(dirp)) != NULL) {
	char child_abs[4096];
	char child_rel[4096];
	struct stat sb;

	if (bu_strcmp(dp->d_name, ".") == 0 || bu_strcmp(dp->d_name, "..") == 0)
	    continue;
	if (bu_strcmp(dp->d_name, ".git") == 0 || bu_strcmp(dp->d_name, "build") == 0)
	    continue;

	snprintf(child_rel, sizeof(child_rel), "%s/%s", relroot, dp->d_name);
	snprintf(child_abs, sizeof(child_abs), "%s/%s", srcroot, child_rel);
	if (stat(child_abs, &sb) != 0)
	    continue;
	if (S_ISDIR(sb.st_mode)) {
	    _scan_tree_for_patterns_outside_allowed(srcroot, child_rel,
		    patterns, pattern_count,
		    allowed_paths, allowed_path_count,
		    allowed_prefixes, allowed_prefix_count,
		    message);
	} else if (S_ISREG(sb.st_mode)) {
	    _scan_file_for_patterns_outside_allowed(child_abs, child_rel,
		    patterns, pattern_count,
		    allowed_paths, allowed_path_count,
		    allowed_prefixes, allowed_prefix_count,
		    message);
	}
    }

    closedir(dirp);
}

static void
_scan_feature_bridge_file(const char *abs, const char *rel,
			  const char * const *allowed_paths,
			  size_t allowed_path_count)
{
    static const char *patterns[] = {
	"feature_private.h",
	"bsg_feature_node(",
	"bsg_feature_ref_from_node(",
	"bsg_feature_visit_nodes(",
	"bsg_feature_vlist(",
	"bsg_feature_vlist_command_count(",
	"bsg_scene_node_find"
    };
    FILE *fp;
    char line[4096];
    size_t lineno = 0;

    if (!_is_source_file(abs))
	return;

    fp = fopen(abs, "r");
    if (!fp)
	return;

    while (bu_fgets(line, sizeof(line), fp)) {
	lineno++;
	for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++) {
	    if (!strstr(line, patterns[i]))
		continue;
	    if (_path_in_list(rel, allowed_paths, allowed_path_count))
		continue;
	    fprintf(stderr,
		    "%s:%zu: feature node/vlist bridge is confined to private storage code: found '%s'\n",
		    rel, lineno, patterns[i]);
	    g_failures++;
	}
    }

    fclose(fp);
}

static void
_scan_feature_bridge_tree(const char *srcroot, const char *relroot,
			  const char * const *allowed_paths,
			  size_t allowed_path_count)
{
    char abs[4096];
    DIR *dirp;
    struct dirent *dp;

    snprintf(abs, sizeof(abs), "%s/%s", srcroot, relroot);
    dirp = opendir(abs);
    if (!dirp) {
	fprintf(stderr, "Unable to open %s: %s\n", abs, strerror(errno));
	g_failures++;
	return;
    }

    while ((dp = readdir(dirp)) != NULL) {
	char child_abs[4096];
	char child_rel[4096];
	struct stat sb;

	if (bu_strcmp(dp->d_name, ".") == 0 || bu_strcmp(dp->d_name, "..") == 0)
	    continue;
	if (bu_strcmp(dp->d_name, ".git") == 0 || bu_strcmp(dp->d_name, "build") == 0)
	    continue;

	snprintf(child_rel, sizeof(child_rel), "%s/%s", relroot, dp->d_name);
	snprintf(child_abs, sizeof(child_abs), "%s/%s", srcroot, child_rel);
	if (stat(child_abs, &sb) != 0)
	    continue;
	if (S_ISDIR(sb.st_mode)) {
	    _scan_feature_bridge_tree(srcroot, child_rel, allowed_paths, allowed_path_count);
	} else if (S_ISREG(sb.st_mode)) {
	    _scan_feature_bridge_file(child_abs, child_rel, allowed_paths, allowed_path_count);
	}
    }

    closedir(dirp);
}

static void
_scan_exact_file_for_patterns(const char *srcroot, const char *rel,
			      const char * const *patterns,
			      size_t pattern_count,
			      const char *message)
{
    char abs[4096];
    FILE *fp;
    char line[4096];
    size_t lineno = 0;

    if (!srcroot || !rel || !patterns || !message)
	return;

    snprintf(abs, sizeof(abs), "%s/%s", srcroot, rel);
    fp = fopen(abs, "r");
    if (!fp) {
	fprintf(stderr, "Unable to open %s: %s\n", abs, strerror(errno));
	g_failures++;
	return;
    }

    while (bu_fgets(line, sizeof(line), fp)) {
	lineno++;
	for (size_t i = 0; i < pattern_count; i++) {
	    if (!patterns[i] || !strstr(line, patterns[i]))
		continue;
	    fprintf(stderr, "%s:%zu: %s: found '%s'\n",
		    rel, lineno, message, patterns[i]);
	    g_failures++;
	}
    }

    fclose(fp);
}

int
main(int argc, char **argv)
{
    const char *srcroot;
	const struct hygiene_rule rules[] = {
	{
	    "include/rt",
	    "primitive_lod_vlist",
	    "primitive LoD realization must publish typed line-set arrays, not vlist storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/librt",
	    "primitive_lod_vlist",
	    "primitive LoD realization must publish typed line-set arrays, not vlist storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libged",
	    "primitive_lod_vlist",
	    "primitive LoD realization must publish typed line-set arrays, not vlist storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/rt",
	    "has_vlist",
	    "primitive LoD realization must track typed line-set availability, not vlist state",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/librt",
	    "has_vlist",
	    "primitive LoD realization must track typed line-set availability, not vlist state",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libged",
	    "has_vlist",
	    "primitive LoD realization must track typed line-set availability, not vlist state",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/rt",
	    "realization->vlist",
	    "primitive LoD realization must not expose vlist storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/librt",
	    "realization->vlist",
	    "primitive LoD realization must not expose vlist storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libged",
	    "realization->vlist",
	    "primitive LoD realization must not expose vlist storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/rt",
	    "realization.vlist",
	    "primitive LoD realization must not expose vlist storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/librt",
	    "realization.vlist",
	    "primitive LoD realization must not expose vlist storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libged",
	    "realization.vlist",
	    "primitive LoD realization must not expose vlist storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/librt",
	    "lod_plot_vlist",
	    "adaptive primitive LoD producers must write typed line-set arrays directly",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include",
	    "bsg/transitional.h",
	    "public drawing headers must not include the retired catch-all bridge",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include",
	    "bsg_node_vlist_head",
	    "public drawing headers must not expose transitional node storage APIs",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include",
	    "bsg_node_set_internal_data",
	    "public drawing headers must not expose transitional node storage APIs",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include",
	    "bsg_node_get_internal_data",
	    "public drawing headers must not expose transitional node storage APIs",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include",
	    "bsg_view_objs",
	    "public drawing headers must not expose old view-object query APIs",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "free_scene_obj",
	    "installed BSG headers must not expose view scene-store storage fields",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "gv_vlfree",
	    "installed BSG headers must not expose view scene-store storage fields",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "db_objs",
	    "installed BSG headers must not expose view scene-store storage fields",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_feature_node(",
	    "installed BSG headers must not expose feature-node bridge helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_feature_ref_from_node(",
	    "installed BSG headers must not expose feature-node bridge helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_feature_visit_nodes(",
	    "installed BSG headers must not expose feature-node traversal helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_feature_vlist(",
	    "installed BSG headers must not expose feature-owned mutable geometry",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_feature_vlist_command_count(",
	    "installed BSG headers must not expose feature-owned mutable geometry",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_shape_vlist",
	    "installed BSG headers must not expose node-owned mutable geometry",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_shape_set_vlist_command_count",
	    "installed BSG headers must not expose node-owned mutable geometry",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_shape_vlist_pool",
	    "installed BSG headers must not expose node-owned mutable geometry",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_set_realize_callback",
	    "installed BSG headers must not expose generic realization callbacks",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_realize_callback",
	    "installed BSG headers must not expose generic realization callbacks",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_set_release_callback",
	    "installed BSG headers must not expose generic lifecycle callbacks",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_invoke_release_callback",
	    "installed BSG headers must not expose generic lifecycle callbacks",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_set_lifecycle_data",
	    "installed BSG headers must not expose generic lifecycle data slots",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_lifecycle_data",
	    "installed BSG headers must not expose generic lifecycle data slots",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_set_kind",
	    "installed BSG headers must not expose node taxonomy mutation",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_set_user_data",
	    "installed BSG headers must not expose generic node user data",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_user_data",
	    "installed BSG headers must not expose generic node user data",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_settings",
	    "installed BSG headers must not expose mutable node settings storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_revision",
	    "installed BSG headers must not expose node revision storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_touch",
	    "installed BSG headers must not expose node revision storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_identity_",
	    "installed BSG headers must not expose raw node identity storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_set_payload_type",
	    "installed BSG headers must not expose payload taxonomy mutation",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_get_payload_type",
	    "installed BSG headers must not expose payload taxonomy storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_payload_prepare(",
	    "installed BSG headers must not expose private payload realization hooks",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "BSG_PL_LIVE",
	    "installed BSG headers must expose edit-preview payload records, not live-source payload types",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "BSG_REALIZE_LIVE_SOURCE",
	    "installed BSG headers must expose edit-preview realization records, not live-source realization types",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_payload_live_",
	    "installed BSG headers must expose edit-preview payload APIs, not live-source payload internals",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_live_source",
	    "installed BSG headers must expose edit-preview records, not live-source payload internals",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "BSG_VIEW_SETS_H",
	    "legacy bsg/view_sets.h include bridge must not reappear",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_set_source_payload",
	    "installed BSG headers must not expose draw-source payload storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_get_source_payload",
	    "installed BSG headers must not expose draw-source payload storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_take_source_payload",
	    "installed BSG headers must not expose draw-source payload storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_vlist_payload",
	    "installed BSG headers must not expose node-owned vlist payload mutation",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_vlblock_to_objs",
	    "installed BSG headers must not expose vlblock-to-node conversion",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_vlblock_obj",
	    "installed BSG headers must not expose vlblock-to-node conversion",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_feature_replace_vlblock",
	    "installed BSG headers must not expose vlblock feature replacement",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_feature_update_geometry",
	    "installed BSG headers must not expose vlist-shaped feature geometry update",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_feature_clear_geometry",
	    "installed BSG headers must not expose compatibility feature geometry clear wrappers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_feature_geometry_update",
	    "installed BSG headers must not expose vlist-shaped feature geometry update records",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "BSG_FEATURE_GEOMETRY_UPDATE_INIT",
	    "installed BSG headers must not expose vlist-shaped feature geometry update initializers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "size_t vlist_command_count;",
	    "feature records must report typed geometry command counts, not vlist command counts",
	    "include/bsg/export.h",
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_line_layer_builder_to_vlblock",
	    "installed BSG headers must not expose line-layer-to-vlblock export",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_line_layer_add_vlist_cmd",
	    "installed BSG headers must not expose line-layer vlist ingestion",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_line_layer_builder_add_vlist_cmd",
	    "installed BSG headers must not expose line-layer vlist ingestion",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_line_layer_builder_append_vlist",
	    "installed BSG headers must not expose line-layer vlist ingestion",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_line_layer_to_vlist",
	    "installed BSG headers must not expose line-layer vlist export",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/nmg",
	    "nmg_vlblock_",
	    "installed NMG headers must expose typed line-layer display APIs, not vlblock assembly",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/nmg",
	    "struct bsg_vlblock",
	    "installed NMG headers must not expose vlblock assembly records",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/nmg",
	    "bsg/vlist.h",
	    "installed NMG plot headers must not expose vlist/vlblock dependencies",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/analyze",
	    "bsg_vlblock",
	    "installed analyze/NIRT headers must not expose legacy vlblock segment output",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/analyze",
	    "nirt_line_segments(",
	    "dead NIRT vlblock segment accessor must not return to installed API",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libbsg",
	    "bsg_vlblock_to_objs",
	    "deleted vlblock-to-node conversion bridge must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src/libbsg",
	    "bsg_vlblock_obj",
	    "deleted vlblock-to-node conversion bridge must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src/libbsg",
	    "bsg_feature_replace_vlblock",
	    "deleted vlblock feature replacement bridge must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src/libbsg",
	    "bsg_feature_update_geometry",
	    "deleted vlist-shaped feature geometry update bridge must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src/libbsg",
	    "bsg_feature_clear_geometry",
	    "deleted compatibility feature geometry clear wrapper must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src/libbsg",
	    "bsg_feature_geometry_update",
	    "deleted vlist-shaped feature geometry update record must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src/libbsg",
	    "BSG_FEATURE_GEOMETRY_UPDATE_INIT",
	    "deleted vlist-shaped feature geometry update initializer must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src/libbsg",
	    "bsg_line_layer_builder_to_vlblock",
	    "deleted line-layer-to-vlblock bridge must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src/libbsg",
	    "bsg_line_layer_add_vlist_cmd",
	    "deleted line-layer vlist ingestion bridge must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src/libbsg",
	    "bsg_line_layer_builder_add_vlist_cmd",
	    "deleted line-layer vlist ingestion bridge must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src/libbsg",
	    "bsg_line_layer_builder_append_vlist",
	    "deleted line-layer vlist ingestion bridge must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src/libbsg",
	    "bsg_line_layer_to_vlist",
	    "deleted line-layer vlist export bridge must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_find_overlay_group",
	    "installed BSG headers must not expose overlay group storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_ensure_overlay_group",
	    "installed BSG headers must not expose overlay group storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_erase_overlay_by_name",
	    "installed BSG headers must not expose overlay group storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_overlay_register_owner",
	    "installed BSG headers must not expose overlay node ownership mutation",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_overlay_replace",
	    "installed BSG headers must not expose overlay node ownership mutation",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_overlay_query_by_role",
	    "installed BSG headers must not expose overlay node queries",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_overlay_auto_remove",
	    "installed BSG headers must not expose overlay node ownership cleanup",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_overlay_info_get",
	    "installed BSG headers must not expose overlay node metadata storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_overlay_info_clear",
	    "installed BSG headers must not expose overlay node metadata storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_source_realization",
	    "installed BSG headers must not expose source-realization storage fields",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_set_source_realization",
	    "installed BSG headers must not expose source-realization storage mutation",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_source_realization_set_view_policy",
	    "installed BSG headers must not expose source-realization storage mutation",
	    NULL,
	    NULL,
	    NULL
	},
	DRAWING_HEADER_RULES(
	    "bsg_view_adaptive_plot",
	    "installed drawing headers must expose LoD policy records, not adaptive plot toggles"),
	DRAWING_HEADER_RULES(
	    "bsg_view_set_adaptive_plot",
	    "installed drawing headers must expose LoD policy records, not adaptive plot toggles"),
	DRAWING_HEADER_RULES(
	    "bsg_view_redraw_on_zoom",
	    "installed drawing headers must expose LoD policy records, not redraw-on-zoom toggles"),
	DRAWING_HEADER_RULES(
	    "bsg_view_set_redraw_on_zoom",
	    "installed drawing headers must expose LoD policy records, not redraw-on-zoom toggles"),
	DRAWING_HEADER_RULES(
	    "bsg_view_traverse",
	    "installed drawing headers must expose action/render products, not traversal entry points"),
	DRAWING_SOURCE_RULES(
	    "bsg_node_draw_cache",
	    "drawing code must use source-realization records, not node draw-cache mirrors"),
	DRAWING_SOURCE_RULES(
	    "adaptive_plot",
	    "drawing code must use LoD source-realization records, not adaptive-plot APIs"),
	DRAWING_SOURCE_RULES(
	    "redraw_on_zoom",
	    "drawing code must use LoD policy records, not redraw-on-zoom toggles"),
	DRAWING_SOURCE_RULES(
	    "bsg_view_traverse",
	    "application drawing code must consume action/render products, not traversal entry points"),
	{
	    "include/rt",
	    "ft_adaptive_plot",
	    "installed RT headers must expose primitive LoD realization, not adaptive plot hooks",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/librt",
	    "ft_adaptive_plot",
	    "RT primitives must expose primitive LoD realization, not adaptive plot hooks",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/librt",
	    "adaptive_plot",
	    "RT primitives must keep adaptive plotting out of the public realization path",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_field_touch",
	    "installed BSG headers must not expose field revision storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_set_flag",
	    "installed BSG headers must not expose node field flag storage mutation",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node_get_flag",
	    "installed BSG headers must not expose node field flag storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_polygon_vlist",
	    "installed BSG headers must not expose raw polygon vlist rebuilding",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_update_polygon(",
	    "installed BSG headers must not expose raw polygon node update",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_select_polygon(",
	    "installed BSG headers must not expose polygon node selectors",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_view_select_polygon(",
	    "installed BSG headers must not expose polygon node selectors",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_view_polygon_find(",
	    "installed BSG headers must not expose polygon node selectors",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_view_polygon_dup(",
	    "installed BSG headers must not expose polygon node selectors",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_view_polygon_snap_objs",
	    "installed BSG headers must not expose polygon node-table snapshots",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_view_snap_objects",
	    "installed BSG headers must not expose snap object tables",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_move_polygon(",
	    "installed BSG headers must not expose raw polygon node movement",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_dup_view_polygon",
	    "installed BSG headers must not expose polygon node duplication",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_polygon_csg(",
	    "installed BSG headers must not expose polygon node CSG",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_scene_node_create",
	    "installed BSG headers must not expose old create-by-type scene-node helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_scene_node_create_registered",
	    "installed BSG headers must not expose old create-by-type scene-node helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_scene_node_create_detached",
	    "installed BSG headers must not expose old create-by-type scene-node helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_scene_node_create_child",
	    "installed BSG headers must not expose old create-by-type scene-node helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_scene_node_reset",
	    "installed BSG headers must not expose scene-node storage lifecycle helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_scene_node_release",
	    "installed BSG headers must not expose scene-node storage lifecycle helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_scene_node_sync",
	    "installed BSG headers must not expose scene-node storage lifecycle helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_scene_node_invalidate",
	    "installed BSG headers must not expose scene-node storage lifecycle helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_scene_child_find",
	    "installed BSG headers must not expose implementation node lookup helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_scene_node_find",
	    "installed BSG headers must not expose implementation node lookup helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_scene_flat_db_table",
	    "installed BSG headers must not expose flat object tables",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_scene_node_view_depth",
	    "installed BSG headers must not expose node-depth helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include",
	    "legacy_compat",
	    "public drawing headers must not add legacy compatibility switches",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "drawing_stack_modernization",
	    "installed BSG headers must describe architecture, not modernization plan stages",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "drawing_modernization",
	    "installed BSG headers must describe architecture, not modernization plan stages",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_modernize",
	    "installed BSG headers must describe architecture, not modernization plan stages",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "migration",
	    "installed BSG headers must not describe public APIs as migration layers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "transitional",
	    "installed BSG headers must not describe public APIs as transitional storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "BV_",
	    "installed BSG headers must use BSG vocabulary for drawing APIs",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "Phase D",
	    "installed BSG headers must not expose implementation-stage labels",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node          *node;",
	    "public render items must not expose source graph-node pointers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_node          *lod_node;",
	    "public render items must carry LoD identity, not LoD graph-node pointers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg/lod_ops.h",
	    "installed BSG headers must not expose LoD node callback headers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "struct bsg_lod_ops",
	    "installed BSG headers must expose typed LoD fields, not node callback ops",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_lod_node_",
	    "installed BSG headers must expose typed LoD fields, not LoD node helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_lod_update(",
	    "installed BSG headers must update LoD through actions",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_lod_stale(",
	    "installed BSG headers must query LoD state through typed fields/actions",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_lod_view_cursor",
	    "installed BSG headers must not expose LoD node user-data cursor storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "lod_payload",
	    "installed BSG headers must not expose LoD node user-data storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libged",
	    "bsg_lod_node_",
	    "GED drawing code must consume LoD source records, not LoD node helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libqtcad",
	    "bsg_lod_node_",
	    "QtCAD drawing code must consume LoD source records, not LoD node helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/qged",
	    "bsg_lod_node_",
	    "qged drawing code must consume LoD source records, not LoD node helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "node_revision;",
	    "public render items must carry graph revision through source records",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "drawn_revision;",
	    "public render items must not expose node draw-frame storage",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "payload_flags; /**<",
	    "public render items must carry typed payload records, not node payload flag mirrors",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include",
	    "dm_draw_scene_axes(struct dm *dmp, struct bsg_node",
	    "DM helpers must consume render items or payload views, not BSG graph nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/mged",
	    "mged_highlight_shape(",
	    "MGED highlight identity must be a GED draw ref/record, not a BSG graph node",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/mged",
	    "mged_highlight_group(",
	    "MGED highlight group caches must not expose BSG graph nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/mged",
	    "mged_highlight_set_shape(",
	    "MGED highlight updates must enter through GED draw refs",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "Phase V",
	    "installed BSG headers must not expose implementation-stage labels",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "Phase 7",
	    "installed BSG headers must not expose implementation-stage labels",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_root",
	    "installed GED drawing headers must not expose BSG draw-tree roots",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_shape_data",
	    "installed GED drawing headers must not expose node-owned draw-shape data",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_shape_fullpath",
	    "installed GED drawing headers must expose draw records instead of node data helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_shape_leaf_dp",
	    "installed GED drawing headers must expose draw records instead of node data helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_shape_display_name",
	    "installed GED drawing headers must expose draw records instead of node data helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_shape_path_hash",
	    "installed GED drawing headers must expose draw records instead of node data helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_dl(",
	    "installed GED drawing headers must not expose the BSG draw root",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "draw_scene(",
	    "installed GED drawing headers must not expose node-based draw helpers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_insert_overlay_vlist",
	    "installed GED drawing headers must not expose overlay vlist insertion",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_set_highlight_flag",
	    "installed GED drawing headers must use highlight state records instead of flag sweeps",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "BSG_PAYLOAD_OVERLAY",
	    "installed GED drawing headers must not expose payload flag implementation details",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "s_type_flags",
	    "installed GED drawing headers must not expose node payload flag storage details",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_ensure_root",
	    "installed GED drawing headers must not expose BSG draw-tree roots",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_group_lookup_or_create",
	    "installed GED drawing headers must not expose graph mutation bridges",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_append_shape_to_group",
	    "installed GED drawing headers must not expose graph mutation bridges",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_set_highlighted_shape(",
	    "installed GED drawing headers must use highlight refs instead of shape nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_highlighted_shape(",
	    "installed GED drawing headers must use highlight refs instead of shape nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_highlight_shape_by_name",
	    "installed GED drawing headers must use highlight refs instead of shape nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_shape_node_from_ref",
	    "installed GED drawing headers must not expose draw-ref to node conversion",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_group_node_from_ref",
	    "installed GED drawing headers must not expose draw-ref to node conversion",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_foreach_shape(",
	    "installed GED drawing headers must expose shape records instead of node iteration",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/ged",
	    "ged_draw_foreach_group(",
	    "installed GED drawing headers must expose group records instead of node iteration",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_selection_add(",
	    "installed BSG selection headers must store interaction records, not graph nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_selection_remove(",
	    "installed BSG selection headers must store interaction records, not graph nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_selection_contains(",
	    "installed BSG selection headers must test interaction records, not graph nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_selection_nodes(",
	    "installed BSG selection headers must not expose node-table compatibility",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_selection_highlight(",
	    "selection highlighting must be render-derived, not stored node mutation",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_selection_unhighlight(",
	    "selection highlighting must be render-derived, not stored node mutation",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_interaction_record_node(",
	    "installed BSG interaction headers must expose stable refs and paths, not graph nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_interaction_from_node(",
	    "node-to-interaction conversion is a private producer bridge",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "bsg_pick_result_to_ptbl",
	    "installed BSG pick headers must not expose node-table compatibility",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "sc_node",
	    "installed BSG snap candidates must expose stable source identity, not graph nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src",
	    "node_private.h",
	    "private BSG node layout includes are confined to libbsg",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src/libged",
	    "node_api_private.h",
	    "private BSG raw-node API includes are not allowed in libged",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libged",
	    "bsg_private.h",
	    "private BSG implementation includes are not allowed in libged",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libged",
	    "libbsg/object_private.h",
	    "private BSG object/raw-node conversion includes are not allowed in libged",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libged",
	    "libbsg/",
	    "libged must use installed BSG headers, not libbsg implementation paths",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src",
	    "bsg_ged_draw_private.h",
	    "private GED draw bridge includes are confined to libged",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    "src/libged/"
	},
	{
	    "src",
	    "scene_object_private.h",
	    "private BSG scene-object storage includes are confined to libbsg",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "node_storage_private.h",
	    "private BSG node storage includes are confined to libbsg and the private GED bridge",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "payload_private.h",
	    "private BSG payload storage includes are confined to libbsg and the private GED bridge",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "overlay_private.h",
	    "private BSG overlay storage includes are confined to libbsg",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "identity_private.h",
	    "private BSG identity storage includes are confined to libbsg",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "node_access_private.h",
	    "deleted BSG raw-node accessor private header must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src",
	    "vlist_private.h",
	    "deleted BSG vlblock-to-node private header must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src",
	    "util_private.h",
	    "deleted BSG raw utility private header must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src",
	    "visit_private.h",
	    "deleted BSG raw traversal private header must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src/libbsg",
	    "visit.c",
	    "deleted BSG raw traversal implementation must not reappear",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src",
	    "draw_source_private.h",
	    "private BSG draw-source cache includes are confined to libbsg",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "geometry_private.h",
	    "private BSG geometry mutation includes are confined to libbsg",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "field_private.h",
	    "private BSG field storage includes are confined to libbsg and the private GED bridge",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "polygon_private.h",
	    "private BSG polygon node selectors are confined to libbsg",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "include/bsg",
	    "bsg_settings_init",
	    "view settings storage initialization is private to libbsg",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "*gv_s;",
	    "installed BSG headers must not expose legacy mutable view settings storage names",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "include/bsg",
	    "*gv_ls;",
	    "installed BSG headers must not expose legacy mutable view settings storage names",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src",
	    "gv_s->",
	    "direct bsg_view settings storage access is confined to libbsg view-state internals",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "->gv_s->",
	    "direct bsg_view settings storage access is confined to libbsg view-state internals",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    ".gv_s->",
	    "direct bsg_view settings storage access is confined to libbsg view-state internals",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "->gv_s =",
	    "direct bsg_view settings storage access is confined to libbsg view-state internals",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    ".gv_s =",
	    "direct bsg_view settings storage access is confined to libbsg view-state internals",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "bsg_view_settings_active",
	    "active settings storage access is private to libbsg view-state internals",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "bsg_view_adc(",
	    "mutable ADC record access is private to libbsg; apps must use get/set records",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "bsg_view_center_dot(",
	    "mutable center-dot record access is private to libbsg; apps must use get/set records",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "bsg_view_grid(",
	    "mutable grid record access is private to libbsg; apps must use get/set records",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "bsg_view_model_axes(",
	    "mutable model-axes record access is private to libbsg; apps must use get/set records",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "bsg_view_view_axes(",
	    "mutable view-axes record access is private to libbsg; apps must use get/set records",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "bsg_view_scale_state(",
	    "mutable scale record access is private to libbsg; apps must use get/set records",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "bsg_view_params(",
	    "mutable params record access is private to libbsg; apps must use get/set records",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "bsg_view_interactive_rect(",
	    "mutable interactive-rect record access is private to libbsg; apps must use get/set records",
	    NULL,
	    NULL,
	    "src/libbsg/"
	},
	{
	    "src",
	    "struct _adc_state",
	    "MGED ADC state must be the canonical bsg_adc_state view record",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src",
	    "->dm_adc_state",
	    "MGED ADC state must not be display-manager-owned storage",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src",
	    ".dm_adc_state",
	    "MGED ADC state must not be display-manager-owned storage",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src",
	    "->dm_grid_state",
	    "MGED grid state must not be display-manager-owned storage",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src",
	    ".dm_grid_state",
	    "MGED grid state must not be display-manager-owned storage",
	    "src/libbsg/tests/test_api_hygiene.c",
	    NULL,
	    NULL
	},
	{
	    "src/libdm",
	    "item->node ",
	    "render backends must consume render-item fields instead of probing source nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libged",
	    "item->node ",
	    "render backends must consume render-item fields instead of probing source nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libtclcad",
	    "item->node ",
	    "render backends must consume render-item fields instead of probing source nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libqtcad",
	    "item->node ",
	    "render backends must consume render-item fields instead of probing source nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/qged",
	    "item->node ",
	    "render backends must consume render-item fields instead of probing source nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libged/view",
	    "struct bsg_node *",
	    "GED view commands must use typed refs/records, not raw BSG nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libged/view",
	    "bsg_node *",
	    "GED view commands must use typed refs/records, not raw BSG nodes",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libged/view",
	    "ged_draw_node_",
	    "GED view commands must use typed refs/records, not raw draw-node wrappers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libged/view",
	    "ged_draw_scene_node_",
	    "GED view commands must use typed refs/records, not raw draw-node wrappers",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libged/view",
	    "bsg_create_polygon(",
	    "GED view polygon commands must create polygons through bsg_polygon_ref APIs",
	    NULL,
	    NULL,
	    NULL
	},
	{
	    "src/libged/view",
	    "bsg_node_polygon(",
	    "GED view polygon commands must inspect polygons through records/data refs",
	    NULL,
	    NULL,
	    NULL
	},
	DRAWING_HEADER_RULES("BSG_OBJ_",
	    "installed drawing headers must use source-scope enums, not legacy object flags"),
	DRAWING_HEADER_RULES("BSG_SHAPE_",
	    "installed drawing headers must use geometry-role enums, not legacy shape flags"),
	DRAWING_HEADER_RULES("bsg_obj_settings",
	    "installed drawing headers must expose appearance settings, not legacy object settings"),
	DRAWING_HEADER_RULES("display_object",
	    "installed drawing headers must expose source scope, not display-object flags"),
	DRAWING_HEADER_RULES("s_type_flags",
	    "installed drawing headers must expose taxonomy records, not node storage fields"),
	DRAWING_HEADER_RULES("s_flag",
	    "installed drawing headers must expose visibility records, not node storage fields"),
	DRAWING_HEADER_RULES("s_iflag",
	    "installed drawing headers must expose interaction records, not node storage fields"),
	DRAWING_HEADER_RULES("view object",
	    "installed drawing headers must use view-feature/source terminology"),
	DRAWING_HEADER_RULES("view-only",
	    "installed drawing headers must use view-scoped feature terminology"),
	DRAWING_HEADER_RULES("adornment",
	    "installed drawing headers must use overlay terminology"),
	DRAWING_HEADER_RULES("solid list",
	    "installed drawing headers must not describe drawing state as solid lists"),
	DRAWING_HEADER_RULES("gv_draw_root",
	    "installed drawing headers must not expose application-visible draw roots"),
	DRAWING_HEADER_RULES("bsg_root",
	    "installed drawing headers must not expose BSG scene-root aliases"),
	DRAWING_HEADER_RULES("gd_draw_root",
	    "installed drawing headers must not expose GED draw-root aliases"),
	DRAWING_HEADER_RULES("ged_draw_root",
	    "installed drawing headers must not expose draw-root accessors"),
	DRAWING_SOURCE_RULES("BSG_OBJ_",
	    "drawing production source must use source-scope enums, not legacy object flags"),
	DRAWING_SOURCE_RULES("BSG_SHAPE_",
	    "drawing production source must use geometry-role enums, not legacy shape flags"),
	DRAWING_SOURCE_RULES("bsg_obj_settings",
	    "drawing production source must use appearance settings, not legacy object settings"),
	DRAWING_SOURCE_RULES("bsg_shape_is_display_object",
	    "drawing production source must use non-database source terminology"),
	DRAWING_SOURCE_RULES("bsg_shape_set_display_object",
	    "drawing production source must use non-database source terminology"),
	DRAWING_SOURCE_RULES("display_object",
	    "drawing production source must expose source scope, not display-object flags"),
	DRAWING_SOURCE_RULES("s_type_flags",
	    "drawing production source must use node taxonomy records, not legacy storage fields"),
	DRAWING_SOURCE_RULES("->s_flag",
	    "drawing production source must use visibility records, not legacy storage fields"),
	DRAWING_SOURCE_RULES("->s_iflag",
	    "drawing production source must use interaction records, not legacy storage fields"),
	DRAWING_SOURCE_RULES("view object",
	    "drawing production source must use view-feature/source terminology"),
	DRAWING_SOURCE_RULES("view-only",
	    "drawing production source must use view-scoped feature terminology"),
	DRAWING_SOURCE_RULES("adornment",
	    "drawing production source must use overlay terminology"),
	DRAWING_SOURCE_RULES("solid list",
	    "drawing production source must not describe drawing state as solid lists"),
	DRAWING_SOURCE_RULES("tcl_adornment_bsg",
	    "drawing test targets must use overlay terminology"),
	DRAWING_SOURCE_RULES("gv_draw_root",
	    "drawing production source must not read or write application-visible draw roots"),
	DRAWING_SOURCE_RULES("bsg_root",
	    "drawing production source must not read or write BSG scene-root aliases"),
	DRAWING_SOURCE_RULES("gd_draw_root",
	    "drawing production source must not read or write GED draw-root aliases"),
	DRAWING_SOURCE_RULES("ged_draw_root",
	    "drawing production source must not use draw-root accessors"),
	DRAWING_SOURCE_RULES("ged_draw_shape_node_from_ref",
	    "drawing production source must not convert draw refs back to BSG nodes"),
	DRAWING_SOURCE_RULES("ged_draw_group_node_from_ref",
	    "drawing production source must not convert draw refs back to BSG nodes"),
	DRAWING_SOURCE_RULES("ged_draw_foreach_shape(",
	    "drawing production source must iterate draw records, not shape nodes"),
	DRAWING_SOURCE_RULES("ged_draw_foreach_group(",
	    "drawing production source must iterate draw records, not group nodes"),
	DRAWING_SOURCE_RULES("ged_draw_group_lookup_or_create(",
	    "drawing production source must mutate draw groups through records"),
	DRAWING_SOURCE_RULES("ged_draw_append_shape_to_group(",
	    "drawing production source must mutate draw groups through records"),
	DRAWING_SOURCE_RULES("ged_draw_shape_data",
	    "drawing production source must store shape metadata in draw records"),
	DRAWING_SOURCE_RULES("BSG_PL_LIVE",
	    "drawing production source must use edit-preview payload records, not live-source payload types"),
	DRAWING_SOURCE_RULES("BSG_REALIZE_LIVE_SOURCE",
	    "drawing production source must use edit-preview realization records, not live-source realization types"),
	DRAWING_SOURCE_RULES("bsg_payload_live_",
	    "drawing production source must use edit-preview payload APIs, not live-source payload internals"),
	DRAWING_SOURCE_RULES("bsg_view_snap_objects",
	    "drawing production source must use snap-source and interaction records, not snap object tables"),
	DRAWING_SOURCE_RULES("gv_snap_objs",
	    "drawing production source must use snap-source records, not snap object ptbls")
    };
    const char *feature_bridge_allowed[] = {
	"src/libbsg/feature_private.h",
	"src/libbsg/feature.c",
	"src/libbsg/hash.c",
	"src/libbsg/interaction.c",
	"src/libbsg/polygon.c",
	"src/libbsg/scene_object_private.h",
	"src/libbsg/snap.c",
	"src/libbsg/snap_action.c",
	"src/libbsg/tests/test_api_hygiene.c",
	"src/libbsg/util.cpp",
	"src/libbsg/vlist.c"
    };
    const char *vlist_producer_files[] = {
	"src/libged/bsg_ged_draw.c",
	"src/libged/draw.cpp",
	"src/libged/draw/draw.c",
	"src/libged/points_eval.c",
	"src/libged/wireframe_eval.c",
	"src/librt/primitives/generic.c",
	"src/librt/primitives/sample.c",
	"src/mged/dodraw.c",
	"src/mged/rtif.c",
	"src/qged/plugins/edit/bot/QBot.cpp",
	"src/qged/plugins/edit/ell/QEll.cpp",
	"src/qged/plugins/edit/extrude/QExtrude.cpp",
	"src/qged/plugins/edit/revolve/QRevolve.cpp"
    };
    const char *vlist_producer_patterns[] = {
	"bsg_shape_vlist(",
	"bsg_shape_vlist_pool(",
	"bsg_shape_set_vlist_command_count",
	"BU_LIST_APPEND_LIST(bsg_shape_vlist"
    };
    const char *retired_librt_plot_hook_files[] = {
	"src/librt/primitives/bot/bot_plot.cpp",
	"src/librt/primitives/brep/brep.cpp",
	"src/librt/primitives/poly/poly.c",
	"src/librt/primitives/sample.c"
    };
    const char *retired_librt_plot_hook_patterns[] = {
	"rt_bot_plot_poly(",
	"rt_brep_plot_poly(",
	"rt_pg_plot_poly(",
	"rt_sample_pnts("
    };
    const char *stage14b_realization_files[] = {
	"src/libged/draw.cpp",
	"src/libged/wireframe_eval.c",
	"src/libged/points_eval.c"
    };
    const char *stage14b_realization_patterns[] = {
	"struct bsg_node *",
	"bsg_node *",
	"ged_draw_node_",
	"ged_draw_scene_node_",
	"bsg_node_",
	"bsg_node_get_draw_intent",
	"bsg_draw_intent_collect",
	"draw_scene("
    };
    const char *stage14c_tree_bridge_files[] = {
	"src/libged/bsg_ged_draw_tree.c",
	"src/libged/bsg_ged_draw_private.h"
    };
    const char *stage14c_tree_bridge_patterns[] = {
	"struct bsg_node *",
	"bsg_node *",
	"bsg/node.h",
	".opaque",
	"bsg_group_find_child",
	"bsg_group_ensure_child",
	"bsg_free_group_contents",
	"bsg_free_group(",
	"bsg_draw_tree_depth",
	"bsg_erase_nested_subpath",
	"bsg_subtree_bbox",
	"bsg_node_bbox_invalidate",
	"bsg_node_get_draw_intent",
	"ged_draw_append_shape_to_last_group",
	"ged_draw_group_set_dbpath",
	"ged_draw_group_ref_append_shape"
    };
    const char *stage14c_independent_scope_files[] = {
	"src/libged/bsg_ged_draw_tree.c",
	"src/libged/view/view.c"
    };
    const char *stage14c_independent_scope_patterns[] = {
	"bsg_view_independent_scope("
    };
    const char *stage14d_boundary_files[] = {
	"include/rt/primitives/sketch.h",
	"src/mged/mged.h"
    };
    const char *stage14d_boundary_patterns[] = {
	"struct bsg_node",
	"bsg_node *",
	"bsg/node.h",
	"bsg/visit.h",
	"db_sketch_to_scene_obj",
	"db_sketch_to_view_polygon(",
	"db_scene_obj_to_sketch"
    };
    const char *stage15_qged_edit_preview_files[] = {
	"src/qged/plugins/edit/bot/QBot.cpp",
	"src/qged/plugins/edit/bot/QBot.h",
	"src/qged/plugins/edit/bot/EditBotFactory.h",
	"src/qged/plugins/edit/ell/QEll.cpp",
	"src/qged/plugins/edit/ell/QEll.h",
	"src/qged/plugins/edit/ell/EditEllFactory.h",
	"src/qged/plugins/edit/extrude/QExtrude.cpp",
	"src/qged/plugins/edit/extrude/QExtrude.h",
	"src/qged/plugins/edit/extrude/EditExtrudeFactory.h",
	"src/qged/plugins/edit/revolve/QRevolve.cpp",
	"src/qged/plugins/edit/revolve/QRevolve.h",
	"src/qged/plugins/edit/revolve/EditRevolveFactory.h",
	"src/qged/plugins/edit/qged_edit_preview_util.h"
    };
    const char *stage15_qged_edit_preview_patterns[] = {
	"bsg/payload_typed.h",
	"bsg_payload_edit_preview",
	"bsg_payload_bump_revision",
	"bsg_feature_payload",
	"bsg_feature_set_payload",
	"struct bsg_payload",
	"BSG_PL_EDIT_PREVIEW",
	"bsg/vlist.h",
	"bsg_vlist",
	"BSG_VLIST_",
	"BSG_FREE_VLIST",
	"ft_plot",
	"qged_edit_feature_replace_vlist",
	"qged_edit_feature_replace_plot",
	"struct bu_list vhead",
	"BU_LIST_INIT(&vhead"
    };
    const char *stage15_feature_header_payload_patterns[] = {
	"bsg_feature_payload",
	"bsg_feature_set_payload",
	"struct bsg_payload *",
	"bsg_payload"
    };
    const char *stage15_render_item_payload_patterns[] = {
	"struct bsg_payload *",
	"item->payload->",
	"item->payload =",
	"item->payload)",
	"payload_has_bounds"
    };
    const char *stage15_public_payload_header_patterns[] = {
	"struct bsg_payload;",
	"struct bsg_payload *",
	"bsg_payload_create",
	"bsg_payload_free",
	"bsg_payload_bump_revision",
	"bsg_scene_set_payload",
	"bsg_node_ref_set_payload",
	"bsg_scene_payload",
	"bsg_node_ref_payload",
	"bsg_payload_typed.h"
    };
    const char *stage15_no_payload_typed_header_patterns[] = {
	"payload_typed.h"
    };
    const char *stage15_rt_lod_payload_patterns[] = {
	"struct bsg_payload",
	"bsg_payload",
	"payload_typed.h"
    };
    const char *stage15_backend_payload_files[] = {
	"src/libdm/view.c",
	"src/libdm/dm-gl_lod.cpp"
    };
    const char *stage15_backend_payload_patterns[] = {
	"bsg/payload_typed.h",
	"item->payload->",
	"item->payload =",
	"item->payload)",
	"bsg_payload_line_set_get(",
	"bsg_payload_text_get(",
	"bsg_payload_hud_text_get(",
	"bsg_payload_axes_get(",
	"bsg_payload_grid_get(",
	"bsg_payload_image_get(",
	"bsg_payload_framebuffer_get(",
	"bsg_payload_annotation_get(",
	"geometry.data.opaque"
    };
    const char *ged_segment_export_consumer_files[] = {
	"src/libged/plot/plot.c",
	"src/libged/ps/ps.c",
	"src/libged/png/png.c",
	"src/libged/select/select.c"
    };
    const char *ged_segment_export_consumer_patterns[] = {
	"bsg/vlist.h",
	"BSG_RENDER_GEOMETRY_VLIST",
	"geometry.data.vlist",
	"bsg_vlist",
	"BSG_VLIST_"
    };
    const char *dm_arrow_segment_patterns[] = {
	"BSG_RENDER_GEOMETRY_VLIST",
	"geometry.data.vlist",
	"bsg_vlist",
	"BSG_VLIST_"
    };
    const char *render_export_vlist_contract_files[] = {
	"include/bsg/render_item.h",
	"include/bsg/export.h",
	"src/libbsg/render_item.c",
	"src/libbsg/export.c",
	"src/libdm/dm-gl_lod.cpp"
    };
    const char *render_export_vlist_contract_patterns[] = {
	"BSG_RENDER_GEOMETRY_VLIST",
	"geometry.data.vlist",
	"BSG_EXPORT_RECORD_VLIST"
    };
    const char *private_vlist_payload_files[] = {
	"include/bsg/payload.h",
	"src/libbsg/payload_private.h",
	"src/libbsg/payload_typed_private.h",
	"src/libbsg/payload_typed.c",
	"src/libbsg/geometry.c",
	"src/libbsg/render.c",
	"src/libbsg/util.cpp",
	"src/libbsg/hash.c",
	"src/libbsg/snap.c",
	"src/libbsg/snap_action.c",
	"src/libbsg/feature.c",
	"src/libbsg/tests/test_export.c",
	"src/libbsg/tests/test_feature.c",
	"src/libbsg/tests/test_geometry_nodes.c",
	"src/libbsg/tests/test_payload_typed.c"
    };
    const char *private_vlist_payload_patterns[] = {
	"BSG_PL_VLIST",
	"bsg/vlist.h",
	"bsg_vlist",
	"BSG_VLIST_",
	"struct bsg_payload_vlist",
	"bsg_payload_vlist_create",
	"bsg_payload_vlist_get",
	"bsg_node_vlist_payload",
	"bsg_node_vlist_payload_revision",
	"bsg_node_vlist_payload_command_count",
	"_copy_legacy_vlist_geometry",
	"_legacy_vlist_line_cmd",
	"_legacy_vlist_face_set_stats"
    };
    const char *render_bounds_payload_name_files[] = {
	"include/bsg/render_item.h",
	"include/bsg/export.h",
	"include/bsg/backend_scene.h",
	"src/libbsg/render.c",
	"src/libbsg/export.c",
	"src/libbsg/backend_scene.c",
	"src/libged/view/view.c"
    };
    const char *render_bounds_payload_name_patterns[] = {
	"payload_has_bounds"
    };
    const char *ged_preview_eye_path_patterns[] = {
	"preview_vbp",
	"bsg_vlblock",
	"_ged_cvt_vlblock_to_solids",
	"BSG_VLIST_",
	"BSG_ADD_VLIST",
	"rt_vlfree"
    };
    const char *ged_typed_overlay_files[] = {
	"src/libged/check/check_overlaps.c",
	"src/libged/gqa/gqa.cpp"
    };
    const char *ged_typed_overlay_patterns[] = {
	"bsg_vlblock",
	"_ged_cvt_vlblock_to_solids",
	"BSG_VLIST_",
	"BSG_ADD_VLIST",
	"rt_vlfree",
	"rt_uplot_to_vlist"
    };
    const char *ged_nirt_typed_uplot_patterns[] = {
	"bsg_vlblock",
	"_ged_cvt_vlblock_to_solids",
	"BSG_VLIST_",
	"BSG_ADD_VLIST",
	"rt_vlfree",
	"rt_uplot_to_vlist",
	"DB_LS_PHONY",
	"db_ls(",
	"ged_draw_apply_erase_path"
    };
    const char *ged_qray_typed_patterns[] = {
	"qray_data_to_vlist",
	"bsg_vlblock",
	"BSG_VLIST_",
	"BSG_ADD_VLIST",
	"rt_vlfree"
    };
    const char *ged_rtcheck_frontend_patterns[] = {
	"bsg_vlblock",
	"_ged_cvt_vlblock_to_solids",
	"BSG_VLIST_",
	"BSG_ADD_VLIST",
	"rt_vlfree",
	"rt_process_uplot_value",
	"ged_draw_apply_erase_path"
    };
    const char *ged_overlay_typed_uplot_patterns[] = {
	"bsg/vlist.h",
	"bsg_vlblock",
	"_ged_cvt_vlblock_to_solids",
	"BSG_VLIST_",
	"BSG_ADD_VLIST",
	"rt_vlfree",
	"rt_uplot_to_vlist"
    };
    const char *ged_util_typed_uplot_patterns[] = {
	"bsg/vlist.h",
	"bsg_vlist",
	"BSG_VLIST_",
	"BSG_ADD_VLIST",
	"rt_vlfree",
	"rt_uplot_to_vlist"
    };
    const char *ged_vdraw_typed_storage_patterns[] = {
	"bsg/vlist.h",
	"bsg_vlist",
	"BSG_VLIST_",
	"BSG_ADD_VLIST",
	"BSG_GET_VLIST",
	"BSG_FREE_VLIST",
	"rt_vlfree",
	"vdc_vhd"
    };
    const char *ged_typed_overlay_insert_files[] = {
	"src/libged/bsg_ged_draw_overlay.c",
	"src/libged/tests/draw/ged_draw_scene.cpp"
    };
    const char *ged_typed_overlay_insert_patterns[] = {
	"ged_draw_overlay_vlist_insert",
	"bsg/vlist.h",
	"BSG_VLIST_",
	"BSG_ADD_VLIST",
	"rt_vlfree"
    };
    const char *ged_draw_ref_typed_geometry_patterns[] = {
	"bsg/vlist.h",
	"BSG_VLIST_",
	"BSG_ADD_VLIST",
	"bsg_vlist",
	"ged_draw_scene_ref_geometry_current_list("
    };
    const char *ged_draw_geometry_publish_files[] = {
	"src/libged/bsg_ged_draw_source.c",
	"src/libged/bsg_ged_draw_private.h",
	"src/libged/bsg_ged_draw_draft.c",
	"src/libged/bsg_ged_draw_refs.c",
	"src/libged/draw.cpp",
	"src/libged/wireframe_eval.c",
	"src/libged/points_eval.c",
	"src/libged/draw/draw.c",
	"src/mged/dodraw.c"
    };
    const char *ged_draw_geometry_publish_patterns[] = {
	"_ged_draw_source_line_geometry_cmd_to_vlist",
	"ged_draw_scene_ref_geometry_publish_list",
	"ged_draw_shape_draft_publish_geometry",
	"ged_draw_shape_ref_geometry_publish_list",
	"ged_draw_scene_ref_primitive_plot_to_list",
	"ged_draw_shape_draft_primitive_plot_to_list",
	"ged_draw_shape_ref_primitive_plot_to_list",
	"ged_draw_scene_ref_geometry_current_list(",
	"ged_draw_scene_ref_geometry_copy(",
	"ged_draw_scene_ref_geometry_append_list(",
	"ged_draw_shape_draft_append_geometry(",
	"ged_draw_shape_ref_geometry_append_list(",
	"BU_LIST_APPEND_LIST",
	"BSG_ADD_VLIST(",
	"rt_ell_plot(",
	"rt_tgc_plot(",
	"rt_arb_plot(",
	"rt_arbn_plot(",
	"rt_ars_plot(",
	"rt_grp_plot(",
	"rt_hlf_plot(",
	"rt_cline_plot(",
	"rt_part_plot(",
	"rt_joint_plot(",
	"rt_metaball_plot(",
	"rt_hyp_plot(",
	"rt_pnts_plot(",
	"rt_script_plot(",
	"rt_pipe_plot(",
	"rt_tor_plot(",
	"rt_rpc_plot(",
	"rt_rhc_plot(",
	"rt_epa_plot(",
	"rt_ehy_plot(",
	"rt_eto_plot(",
	"rt_superell_plot(",
	"rt_hrt_plot(",
	"rt_datum_plot(",
	"rt_sketch_plot(",
	"rt_extrude_plot(",
	"rt_revolve_plot(",
	"rt_ebm_plot(",
	"rt_vol_plot(",
	"rt_dsp_plot(",
	"rt_hf_plot(",
	"rt_nmg_plot(",
	"rt_nurb_plot(",
	"rt_submodel_plot(",
	"rt_annot_plot(",
	"rt_bot_plot(",
	"rt_pg_plot(",
	"rt_brep_plot(",
	"rt_bot_plot_poly(",
	"rt_brep_plot_poly(",
	"rt_pg_plot_poly(",
	"rt_sample_pnts_face_set(",
	"rt_sample_pnts("
    };
    const char *ged_draw_shaded_face_dispatch_patterns[] = {
	"DB5_MINORTYPE_BRLCAD_BOT",
	"DB5_MINORTYPE_BRLCAD_POLY",
	"DB5_MINORTYPE_BRLCAD_BREP",
	"ged_draw_shape_draft_publish_bot_face_set",
	"ged_draw_shape_draft_publish_brep_cdt_face_set",
	"ged_draw_shape_draft_publish_poly_face_set"
    };
    const char *ged_line_layer_bridge_patterns[] = {
	"_ged_cvt_line_layer_builder_to_solids",
	"_ged_line_layer_geometry_cmd_to_vlist"
    };
    const char *ged_brep_typed_plot_patterns[] = {
	"_ged_cvt_line_layer_builder_to_solids",
	"brep_plot_indexed_faces_wire",
	"rt_vlfree",
	"bsg_vlblock",
	"_ged_cvt_vlblock_to_solids"
    };
    const char *ged_nmg_edgeuse_typed_patterns[] = {
	"_ged_cvt_line_layer_builder_to_solids",
	"nmg_plot_eu(struct ged *gedp, struct edgeuse *es_eu, const struct bn_tol *tol, struct bu_list",
	"nmg_plot_eu(s->gedp, es_eu, &s->tol.tol, s->vlfree)"
    };
    const char *ged_typed_diagnostic_overlay_files[] = {
	"src/libged/bot/bot_fuse.c",
	"src/libged/joint/joint.c",
	"src/libged/gdiff/gdiff.c"
    };
    const char *ged_typed_diagnostic_overlay_patterns[] = {
	"bsg_vlblock",
	"_ged_cvt_vlblock_to_solids",
	"BSG_VLIST_",
	"BSG_ADD_VLIST",
	"ged_draw_apply_erase_path",
	"bsg_vlist_cleanup"
    };
    const char *ged_lint_typed_plot_files[] = {
	"src/libged/lint/lint.cpp",
	"src/libged/lint/ged_lint.h"
    };
    const char *ged_lint_typed_plot_patterns[] = {
	"plot_use_bsg",
	"bsg_vlblock",
	"_ged_cvt_vlblock_to_solids",
	"BSG_VLIST_",
	"BSG_ADD_VLIST",
	"rt_vlfree",
	"vlfree",
	"vbp",
	"plot_prepare"
    };
    const char *ged_bot_typed_plot_files[] = {
	"src/libged/bot/bot.cpp",
	"src/libged/bot/check.cpp",
	"src/libged/bot/ged_bot.h",
	"src/libged/bot/exterior.cpp"
    };
    const char *ged_bot_typed_plot_patterns[] = {
	"_bot_vlblock_plot",
	"bsg_vlblock",
	"_ged_cvt_vlblock_to_solids",
	"BSG_VLIST_",
	"BSG_ADD_VLIST",
	"bsg_vlist_cleanup",
	"vlfree",
	"vbp"
    };
    const char *ged_draw_source_modern_plot_patterns[] = {
	"ft_plot"
    };
    const char *qtcad_sketch_typed_preview_patterns[] = {
	"ft_plot",
	"bsg/vlist.h",
	"bsg_vlist",
	"BSG_VLIST_",
	"bsg_vlist_cleanup"
    };
    const char *scene_obj_hook_files[] = {
	"include/rt/functab.h",
	"src/librt/primitives/table.cpp",
	"src/librt/primitives/generic.c"
    };
    const char *scene_obj_hook_patterns[] = {
	"ft_scene_obj",
	"RTFUNCTAB_FUNC_SCENE_OBJ",
	"rt_generic_scene_obj"
    };
    const char *draw_source_lifecycle_patterns[] = {
	"bsg_node_lifecycle_data",
	"bsg_node_set_lifecycle_data"
    };
    const char *draw_source_release_patterns[] = {
	"bsg_node_set_release_callback("
    };
    const char *renderer_root_boundary_patterns[] = {
	"bsg_node        *root",
	"bsg_node *root,",
	"req->root",
	"request->root",
	"root subtree"
    };
    const char *renderer_root_boundary_files[] = {
	"include/bsg/render.h",
	"include/bsg/export.h",
	"include/bsg/backend_scene.h",
	"include/dm/view.h",
	"src/libdm/view.c",
	"src/libbsg/export.c",
	"src/libbsg/backend_scene.c"
    };
    const char *renderer_execute_patterns[] = {
	"bsg_render_request_execute("
    };
    const char *renderer_execute_files[] = {
	"src/libdm/view.c",
	"src/libbsg/export.c",
	"src/libbsg/backend_scene.c"
    };
    const char *render_record_taxonomy_patterns[] = {
	"node_kind",
	"payload_flags",
	"payload_id",
	"payload_kind",
	"payload_type",
	"const void *opaque",
	"graph_id",
	"graph_revision",
	"frame_id"
    };
    const char *render_record_taxonomy_files[] = {
	"include/bsg/render_item.h",
	"include/bsg/export.h",
	"include/bsg/backend_scene.h"
    };
    const char *qtcad_quad_bridge_patterns[] = {
	"bsg_ged_draw_private.h",
	"bsg_scene_visit_db",
	"draw_scene("
    };
    const char *scene_builder_header_patterns[] = {
	"bsg/node.h",
	"bsg_node_create",
	"bsg_node_destroy",
	"bsg_node_add_child",
	"bsg_node_remove_child",
	"bsg_node *",
	"struct bsg_node"
    };
    const char *stage2_typed_node_authoring_files[] = {
	"src/libdm/tests/test_backend_draw_item.c",
	"src/libged/ged.cpp",
	"src/libged/tests/draw/bsg_quad_stability.cpp",
	"src/libged/tests/draw/ged_draw_scene.cpp",
	"src/libged/tests/draw/rtwizard_bsg.cpp",
	"src/libqtcad/QgGL.cpp",
	"src/libqtcad/QgSW.cpp",
	"src/libqtcad/tests/qsketch.cpp",
	"src/librt/tests/lod.c",
	"src/libtclcad/commands.c",
	"src/mged/setup.c"
    };
    const char *stage2_raw_node_authoring_files[] = {
	"src/libdm/tests/test_backend_draw_item.c",
	"src/libqtcad/tests/qsketch.cpp",
	"src/librt/tests/lod.c"
    };
    const char *stage2_scene_ref_authoring_patterns[] = {
	"bsg/scene_builder.h",
	"bsg_scene_ref",
	"BSG_SCENE_REF",
	"bsg_scene_group_create",
	"bsg_scene_shape_create",
	"bsg_scene_transform_create",
	"bsg_scene_view_scope_create",
	"bsg_scene_root_create_ref",
	"bsg_scene_ref_destroy",
	"bsg_scene_append_child",
	"bsg_scene_set_payload",
	"bsg_scene_payload",
	"bsg_lod_source_insert_above_scene_ref",
	"bsg_lod_sources_update_scene_ref"
    };
    const char *stage2_raw_node_authoring_patterns[] = {
	"bsg/node_group.h",
	"bsg/node_shape.h",
	"bsg/node_transform.h",
	"bsg_node_create",
	"bsg_node_destroy",
	"bsg_node_add_child",
	"bsg_node_remove_child",
	"bsg_node_set_payload",
	"bsg_node_get_payload"
    };
    const char *stage2_node_header_boundary_patterns[] = {
	"bsg_node *",
	"struct bsg_node *",
	"bsg_node_create",
	"bsg_node_destroy",
	"bsg_node_kind",
	"bsg_node_is_kind",
	"unsigned long long kind",
	"taxonomy",
	"bsg_scene_ref",
	"scene_builder",
	"BSG_NODE_ROOT",
	"BSG_NODE_GROUP",
	"BSG_NODE_SHAPE",
	"BSG_NODE_SENSOR",
	"BSG_NODE_LOD"
    };
    const char *stage3_node_header_boundary_patterns[] = {
	"bsg_node_ref_child_count",
	"bsg_node_ref_child_at",
	"bsg_node_ref_append_child",
	"bsg_node_ref_remove_child"
    };
    const char *stage3_group_header_boundary_patterns[] = {
	"bsg_node *",
	"struct bsg_node",
	"bsg_scene_ref",
	"taxonomy",
	"BSG_NODE_"
    };
    const char *stage3_group_header_files[] = {
	"include/bsg/group.h",
	"include/bsg/separator.h",
	"include/bsg/switch.h"
    };
    const char *stage4_field_header_boundary_patterns[] = {
	"bsg_field_id_t",
	"BSG_FIELD_FLAG",
	"BSG_FIELD_COLOR",
	"BSG_FIELD_VISIBILITY",
	"BSG_FIELD_TRANSFORM",
	"BSG_FIELD_CHILDREN",
	"BSG_FIELD_NAME",
	"BSG_FIELD_BOUNDS",
	"BSG_FIELD_SWITCH_WHICH",
	"bsg_node *",
	"struct bsg_node",
	"bsg_node_field_touch",
	"bsg_node_set_flag",
	"bsg_node_get_flag",
	"bsg_node_set_color",
	"bsg_node_get_color",
	"bsg_node_set_visible",
	"bsg_node_get_visible"
    };
    const char *stage5_sensor_header_boundary_patterns[] = {
	"bsg_node *",
	"struct bsg_node *",
	"bsg_field_id_t",
	"BSG_FIELD_FLAG",
	"BSG_FIELD_COLOR",
	"BSG_FIELD_VISIBILITY",
	"BSG_FIELD_TRANSFORM",
	"BSG_FIELD_CHILDREN",
	"BSG_FIELD_NAME",
	"BSG_FIELD_BOUNDS",
	"BSG_FIELD_SWITCH_WHICH",
	"BSG_NODE_SENSOR",
	"BSG_SENSOR_FIELD",
	"BSG_SENSOR_NODE",
	"BSG_SENSOR_TIMER",
	"bsg_field_sensor_create",
	"bsg_node_sensor_create",
	"bsg_timer_sensor_create",
	"bsg_sensor_destroy",
	"bsg_sensor_target",
	"bsg_sensor_notify_field"
    };
    const char *stage5_sensor_taxonomy_files[] = {
	"include/bsg/util.h",
	"src/libbsg/action.c",
	"src/libbsg/object.c",
	"src/libbsg/scene_graph.cpp",
	"src/libbsg/tests/test_scene_graph.c"
    };
    const char *stage5_sensor_taxonomy_patterns[] = {
	"BSG_NODE_SENSOR",
	"bsg_field_sensor_create",
	"bsg_node_sensor_create",
	"bsg_timer_sensor_create",
	"bsg_sensor_destroy",
	"bsg_sensor_target",
	"bsg_sensor_notify_field"
    };
    const char *stage6_state_header_boundary_patterns[] = {
	"struct bsg_state {",
	"bsg_node *",
	"struct bsg_node *",
	"bsg_scene_ref",
	"taxonomy",
	"BSG_NODE_"
    };
    const char *stage6_state_consumer_files[] = {
	"src/libbsg/render.c",
	"src/libbsg/pick.c",
	"src/libbsg/snap_action.c"
    };
    const char *stage6_state_consumer_patterns[] = {
	"state->model_mat",
	"state->visible",
	"state->force_draw",
	"state->lod_source",
	"state->lod_level",
	"state->lod_level_count"
    };
    const char *stage7_action_header_boundary_patterns[] = {
	"struct bsg_action {",
	"struct bsg_action_state",
	"struct bsg_action_traversal",
	"bsg_action_traverse",
	"bsg_action_create",
	"bsg_action_execute",
	"bsg_action_destroy",
	"BSG_ACTION_BBOX",
	"BSG_ACTION_COLLECT",
	"BSG_ACTION_EXPORT",
	"bsg_node *",
	"struct bsg_node *",
	"taxonomy"
    };
    const char *stage7_action_wrapper_files[] = {
	"src/libbsg/backend_scene.c",
	"src/libbsg/export.c",
	"src/libbsg/measure.c"
    };
    const char *stage7_action_wrapper_patterns[] = {
	"bsg_action_traverse("
    };
    const char *stage8_state_node_header_files[] = {
	"include/bsg/complexity.h",
	"include/bsg/draw_style.h"
    };
    const char *stage8_state_node_header_patterns[] = {
	"bsg_node *",
	"struct bsg_node",
	"bsg_field_id_t",
	"BSG_FIELD_",
	"taxonomy",
	"BSG_NODE_"
    };
    const char *stage9_geometry_header_patterns[] = {
	"bsg_node *",
	"struct bsg_node",
	"bsg_field_id_t",
	"BSG_FIELD_",
	"taxonomy",
	"BSG_NODE_",
	"payload",
	"vlist",
	"bsg_payload",
	"bsg_node_set_payload",
	"bsg_node_get_payload"
    };
    const char *stage9_qsketch_payload_patterns[] = {
	"bsg_payload_sketch",
	"bsg_node_ref_payload",
	"bsg_node_ref_set_payload",
	"BSG_PL_SKETCH",
	"bsg_shape_ref_create",
	"bsg_shape_ref"
    };
    const char *stage10_lod_header_patterns[] = {
	"struct bsg_lod_source_policy",
	"bsg_lod_source_select_cb",
	"bsg_lod_source_activate_cb",
	"bsg_lod_source_stale_cb",
	"bsg_lod_source_record",
	"bsg_lod_source_insert_above",
	"bsg_lod_sources_update",
	"bsg_lod_source_view_state"
    };
    const char *stage_c_install_boundary_patterns[] = {
	"  node_group.h",
	"  node_shape.h",
	"  node_transform.h"
    };
    const char *stage_c_payload_header_patterns[] = {
	"bsg_node_realize_payload",
	"bsg_node_set_payload",
	"bsg_node_get_payload",
	"bsg_payload_update(bsg_node",
	"bsg_node *pl_owner",
	"struct bsg_node *node",
	"BSG_PL_GED_DRAW_SOURCE",
	"bsg_ged_draw_source",
	"bsg_payload_ged_draw_source"
    };
    const char *stage16_public_final_boundary_patterns[] = {
	"typedef struct bsg_node bsg_node",
	"struct bsg_node *",
	"bsg_node *",
	"#define BSG_PAYLOAD_VLIST",
	"#define BSG_PAYLOAD_CSG",
	"#define BSG_PAYLOAD_MESH",
	"#define BSG_PAYLOAD_BREP",
	"#define BSG_PAYLOAD_OVERLAY",
	"#define BSG_PAYLOAD_MASK",
	"BSG_PAYLOAD_OVERLAY shapes",
	"bsg_node_get_draw_intent",
	"bsg_node_set_draw_intent",
	"bsg_view_find_by_type",
	"bsg_view_independent_scope(",
	"bsg_hud_root_get",
	"bsg_hud_node_get_meta",
	"bsg_appearance_resolve("
    };
    const char *stage16_deleted_private_bridge_patterns[] = {
	"node_access_private.h",
	"node_api_private.h",
	"node_taxonomy_private.h",
	"util_private.h",
	"visit_private.h",
	"  node_access_private.h",
	"  node_api_private.h",
	"  node_taxonomy_private.h",
	"  util_private.h",
	"  visit_private.h"
    };
    const char *stage_c_lod_header_patterns[] = {
	"bsg_mesh_lod_memshrink(struct bsg_node",
	"bsg_mesh_lod_view(struct bsg_node",
	"bsg_mesh_lod_level(struct bsg_node",
	"bsg_mesh_lod_free(struct bsg_node",
	"bsg_lod_source_insert_above(bsg_node",
	"bsg_lod_source_from_node(bsg_node",
	"bsg_lod_sources_update(bsg_node"
    };
    const char *stage_c_util_header_patterns[] = {
	"bsg_scene_root_create(",
	"bsg_scene_root_sync(",
	"bsg_scene_root_destroy(",
	"bsg_scene_root_create_ref",
	"bsg_scene_root_destroy_ref",
	"bsg_separator_ref_create_root",
	"bsg_view_scene_root_ensure(",
	"bsg_view_scene_root_sync(",
	"bsg_view_scene_root_detach_from_root("
    };
    const char *stage_d_defines_header_patterns[] = {
	"#define bsg_scene_group",
	"typedef struct bsg_node bsg_shape",
	"#define BSG_NODE_"
    };
    const char *stage0_public_header_boundary_patterns[] = {
	"struct bsg_node {",
	"struct bsg_node_internal {",
	"bsg_node_set_realize_callback",
	"bsg_node_realize_callback",
	"bsg_node_set_release_callback",
	"bsg_node_set_lifecycle_data",
	"bsg_node_lifecycle_data",
	"bsg_node_set_payload(",
	"bsg_node_get_payload(",
	"bsg_node_realize_payload(",
	"bsg_scene_root_create(",
	"bsg_scene_root_sync(",
	"bsg_scene_root_destroy(",
	"bsg_scene_root_create_ref",
	"bsg_scene_root_destroy_ref",
	"bsg_separator_ref_create_root",
	"bsg_view_scene_root_ensure(",
	"bsg_view_scene_root_sync(",
	"bsg_view_scene_root_detach_from_root(",
	"bsg_node_vlist_head",
	"bsg_node_vlist_payload",
	"bsg_shape_vlist(",
	"bsg_shape_vlist_pool(",
	"bsg_shape_set_vlist_command_count"
    };
    const char *database_source_header_patterns[] = {
	"struct bsg_node",
	"payload",
	"callback"
    };
    const char *stage0_installed_bsg_header_files[] = {
	"include/bsg.h",
	"include/bsg/action.h",
	"include/bsg/adc.h",
	"include/bsg/appearance.h",
	"include/bsg/backend_adapter.h",
	"include/bsg/backend_scene.h",
	"include/bsg/camera.h",
	"include/bsg/complexity.h",
	"include/bsg/database_source.h",
	"include/bsg/defines.h",
	"include/bsg/draw_ctx.h",
	"include/bsg/draw_intent.h",
	"include/bsg/draw_style.h",
	"include/bsg/draw_set.h",
	"include/bsg/draw_source.h",
	"include/bsg/events.h",
	"include/bsg/export.h",
	"include/bsg/faceplate.h",
	"include/bsg/feature.h",
	"include/bsg/field.h",
	"include/bsg/geometry.h",
	"include/bsg/group.h",
	"include/bsg/hud.h",
	"include/bsg/identity.h",
	"include/bsg/interaction.h",
	"include/bsg/light.h",
	"include/bsg/lod.h",
	"include/bsg/material.h",
	"include/bsg/measure.h",
	"include/bsg/node.h",
	"include/bsg/object.h",
	"include/bsg/obol_node.h",
	"include/bsg/overlay.h",
	"include/bsg/payload.h",
	"include/bsg/pick.h",
	"include/bsg/plot3.h",
	"include/bsg/polygon.h",
	"include/bsg/render.h",
	"include/bsg/render_item.h",
	"include/bsg/scene_builder.h",
	"include/bsg/scene_object.h",
	"include/bsg/scene_set.h",
	"include/bsg/selection.h",
	"include/bsg/separator.h",
	"include/bsg/sensor.h",
	"include/bsg/snap.h",
	"include/bsg/snap_action.h",
	"include/bsg/state.h",
	"include/bsg/switch.h",
	"include/bsg/tcl_data.h",
	"include/bsg/util.h",
	"include/bsg/view_scope.h",
	"include/bsg/view_set.h",
	"include/bsg/view_state.h",
	"include/bsg/vlist.h"
    };
    const char *stage0_taxonomy_definition_patterns[] = {
	"enum bsg_node_kind",
	"bsg_node_kind_t",
	"#define BSG_NODE_ROOT",
	"#define BSG_NODE_GROUP",
	"#define BSG_NODE_SHAPE",
	"#define BSG_NODE_SENSOR",
	"#define BSG_NODE_LOD",
	"BSG_NODE_ROOT =",
	"BSG_NODE_GROUP =",
	"BSG_NODE_SHAPE =",
	"BSG_NODE_SENSOR =",
	"BSG_NODE_LOD ="
    };
    const char *stage1_object_header_boundary_patterns[] = {
	"BSG_NODE_",
	"taxonomy",
	"node_taxonomy_private",
	"node_private",
	"bsg_scene_ref",
	"bsg_node_kind",
	"bsg_node_is_kind",
	"unsigned long long kind"
    };
    const char *stage1_private_object_bridge_patterns[] = {
	"bsg_object_ref_from_node",
	"bsg_object_ref_node",
	"bsg_node_type_from_taxonomy",
	"bsg_node_set_object_type",
	"bsg_object_ref_from_scene_ref",
	"bsg_scene_ref_from_object_ref"
    };
    const char *stage17_view_storage_files[] = {
	"src/libged",
	"src/libdm",
	"src/libtclcad",
	"src/libqtcad",
	"src/mged",
	"src/qged"
    };
    const char *stage17_view_storage_patterns[] = {
	"->gv_scene",
	".gv_scene",
	"->gv_hud_root",
	".gv_hud_root",
	"->gv_render_settings",
	".gv_render_settings"
    };
    const char *stage17_render_request_patterns[] = {
	"req->flags",
	"req->adapter",
	"req->settings",
	"req->items",
	"req->batch",
	"batch->items"
    };
    const char *stage17_render_header_patterns[] = {
	"struct bsg_render_request {",
	"struct bsg_render_batch {",
	"req->",
	"batch->items"
    };
    const char *stage17_action_header_patterns[] = {
	"bsg_action_method_cb",
	"bsg_action_method_register",
	"bsg_action_ref_create(",
	"bsg_vlist_export_action"
    };
    const char *stage17_umbrella_legacy_patterns[] = {
	"bsg/vlist.h",
	"bsg/plot3.h",
	"bsg/draw_ctx.h"
    };
    const char *bsg_umbrella_stability_warning_patterns[] = {
	"THIS API IS HIGHLY EXPERIMENTAL",
	"SHOULD *NOT* BE CONSIDERED STABLE"
    };
    const char *modern_installed_vlist_patterns[] = {
	"bsg/vlist.h",
	"rt/vlist.h",
	"bsg_vlist",
	"bsg_vlblock",
	"BSG_VLIST_",
	"BSG_VLBLOCK",
	"rt_uplot_to_vlist",
	"rt_process_uplot_value",
	"dm_draw("
    };
    const char *modern_installed_vlist_allowed_paths[] = {
	"include/bsg/vlist.h",
	"include/dm/vlist.h"
    };
    const char *modern_installed_vlist_roots[] = {
	"include/analyze",
	"include/bsg",
	"include/dm",
	"include/ged",
	"include/tclcad"
    };
    const char *modern_installed_vlist_exact_files[] = {
	"include/analyze.h",
	"include/bsg.h",
	"include/dm.h",
	"include/ged.h",
	"include/tclcad.h"
    };
    const char *raytrace_umbrella_vlist_patterns[] = {
	"bsg/vlist.h",
	"rt/vlist.h",
	"bsg_vlist",
	"bsg_vlblock",
	"BSG_VLIST_",
	"BSG_VLBLOCK",
	"rt_uplot_to_vlist",
	"rt_process_uplot_value"
    };
    const char *stage18_raw_authority_patterns[] = {
	"bu_data_hash_update(state, s, sizeof(struct bsg_node)",
	"taxonomy.bits",
	"->s_os",
	"s_inherit_settings",
	"s_force_draw",
	"s_old.",
	"visibility.state",
	"s_soldash",
	"->s_name",
	"->s_v",
	"->s_center",
	"->s_size",
	"s_non_db_source",
	"free_scene_obj",
	"gv_objs.db_objs",
	"gv_objs.gv_vlfree",
	"shared_db_objs",
	"shared_db_index",
	"bsg_scene_flat_db_table",
	"bsg_scene_store_node_register_db_index",
	"bsg_scene_store_node_unregister_db_index",
	"db_index_bound"
    };
    const char *stage18_raw_authority_allowed_paths[] = {
	"src/libbsg/appearance.c",
	"src/libbsg/draw_source.c",
	"src/libbsg/field.c",
	"src/libbsg/identity.c",
	"src/libbsg/node.c",
	"src/libbsg/node_private.h",
	"src/libbsg/node_shape.c",
	"src/libbsg/payload.c",
	"src/libbsg/vlist.c"
    };
    const char *stage18_raw_authority_allowed_prefixes[] = {
	"src/libbsg/tests/"
    };
    const char *nmg_umbrella_plot_patterns[] = {
	"nmg/plot.h"
    };
    const char *nmg_plot_typed_display_patterns[] = {
	"nmg_line_layer_"
    };
    const char *node_recycle_owner_patterns[] = {
	"struct bsg_scene_recycle_pool *recycle_pool",
	"default_vlfree",
	"bsg_scene_node_set_recycle_pool"
    };

    bu_setprogname(argv[0]);
    if (argc != 2) {
	fprintf(stderr, "Usage: %s SOURCE_ROOT\n", argv[0]);
	return 2;
    }
    srcroot = argv[1];
    (void)OPENINVENTOR_MIGRATION_STATUS;

    for (size_t i = 0; i < sizeof(rules) / sizeof(rules[0]); i++)
	_scan_tree(srcroot, rules[i].root, &rules[i], 1);

    _scan_feature_bridge_tree(srcroot, "src",
	    feature_bridge_allowed,
	    sizeof(feature_bridge_allowed) / sizeof(feature_bridge_allowed[0]));

    for (size_t i = 0; i < sizeof(vlist_producer_files) / sizeof(vlist_producer_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, vlist_producer_files[i],
		vlist_producer_patterns,
		sizeof(vlist_producer_patterns) / sizeof(vlist_producer_patterns[0]),
		"draw producers must build typed vlist payload geometry");
    }

    for (size_t i = 0; i < sizeof(retired_librt_plot_hook_files) / sizeof(retired_librt_plot_hook_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, retired_librt_plot_hook_files[i],
		retired_librt_plot_hook_patterns,
		sizeof(retired_librt_plot_hook_patterns) / sizeof(retired_librt_plot_hook_patterns[0]),
		"retired librt primitive plot/vlist hooks must not be reintroduced");
    }

    _scan_exact_file_for_patterns(srcroot, "include/nmg.h",
	    nmg_umbrella_plot_patterns,
	    sizeof(nmg_umbrella_plot_patterns) / sizeof(nmg_umbrella_plot_patterns[0]),
	    "top-level NMG umbrella must not expose legacy plot/vlist APIs");

    _scan_exact_file_for_patterns(srcroot, "include/nmg/plot.h",
	    nmg_plot_typed_display_patterns,
	    sizeof(nmg_plot_typed_display_patterns) / sizeof(nmg_plot_typed_display_patterns[0]),
	    "legacy NMG plot/vlist header must not expose typed display APIs");

    _scan_exact_file_for_patterns(srcroot, "src/libbsg/bsg_private.h",
	    node_recycle_owner_patterns,
	    sizeof(node_recycle_owner_patterns) / sizeof(node_recycle_owner_patterns[0]),
	    "node internals must not own scene-store recycle pools or vlist free lists");

    for (size_t i = 0; i < sizeof(stage14b_realization_files) / sizeof(stage14b_realization_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, stage14b_realization_files[i],
		stage14b_realization_patterns,
		sizeof(stage14b_realization_patterns) / sizeof(stage14b_realization_patterns[0]),
		"Stage 14B realization/evaluation code must use typed scene/source refs, not raw BSG nodes");
    }

    for (size_t i = 0; i < sizeof(stage14c_tree_bridge_files) / sizeof(stage14c_tree_bridge_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, stage14c_tree_bridge_files[i],
		stage14c_tree_bridge_patterns,
		sizeof(stage14c_tree_bridge_patterns) / sizeof(stage14c_tree_bridge_patterns[0]),
		"Stage 14C draw-tree bridge must use typed scene refs, not raw BSG node helpers");
    }

    for (size_t i = 0; i < sizeof(stage14c_independent_scope_files) / sizeof(stage14c_independent_scope_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, stage14c_independent_scope_files[i],
		stage14c_independent_scope_patterns,
		sizeof(stage14c_independent_scope_patterns) / sizeof(stage14c_independent_scope_patterns[0]),
		"Stage 14C independent view setup must use scene-ref scope helpers");
    }

    for (size_t i = 0; i < sizeof(stage14d_boundary_files) / sizeof(stage14d_boundary_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, stage14d_boundary_files[i],
		stage14d_boundary_patterns,
		sizeof(stage14d_boundary_patterns) / sizeof(stage14d_boundary_patterns[0]),
		"Stage 14D public/application headers must not expose raw BSG nodes");
    }

    for (size_t i = 0; i < sizeof(stage15_qged_edit_preview_files) / sizeof(stage15_qged_edit_preview_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, stage15_qged_edit_preview_files[i],
		stage15_qged_edit_preview_patterns,
		sizeof(stage15_qged_edit_preview_patterns) / sizeof(stage15_qged_edit_preview_patterns[0]),
		"Stage 15 qged edit plugins must use typed edit-preview feature APIs, not public payload compatibility");
    }

    _scan_exact_file_for_patterns(srcroot, "include/bsg/feature.h",
	    stage15_feature_header_payload_patterns,
	    sizeof(stage15_feature_header_payload_patterns) / sizeof(stage15_feature_header_payload_patterns[0]),
	    "Stage 15 public feature API must expose typed feature operations, not payload accessors");
    _scan_exact_file_for_patterns(srcroot, "include/bsg/render_item.h",
	    stage15_render_item_payload_patterns,
	    sizeof(stage15_render_item_payload_patterns) / sizeof(stage15_render_item_payload_patterns[0]),
	    "Stage 15 render item must expose resolved geometry, not raw payload handles");

    _scan_exact_file_for_patterns(srcroot, "include/bsg/payload.h",
	    stage15_public_payload_header_patterns,
	    sizeof(stage15_public_payload_header_patterns) / sizeof(stage15_public_payload_header_patterns[0]),
	    "Stage 15 public payload metadata must not expose the generic payload owner or mutation API");

    _scan_exact_file_for_patterns(srcroot, "include/bsg/defines.h",
	    stage15_public_payload_header_patterns,
	    sizeof(stage15_public_payload_header_patterns) / sizeof(stage15_public_payload_header_patterns[0]),
	    "Stage 15 public defines must not forward-declare or expose the generic payload owner");

    _scan_exact_file_for_patterns(srcroot, "include/bsg.h",
	    stage15_no_payload_typed_header_patterns,
	    sizeof(stage15_no_payload_typed_header_patterns) / sizeof(stage15_no_payload_typed_header_patterns[0]),
	    "Stage 15 umbrella header must not include the private typed payload header");

    _scan_exact_file_for_patterns(srcroot, "include/bsg/CMakeLists.txt",
	    stage15_no_payload_typed_header_patterns,
	    sizeof(stage15_no_payload_typed_header_patterns) / sizeof(stage15_no_payload_typed_header_patterns[0]),
	    "Stage 15 installed BSG header list must not install the private typed payload header");

    _scan_exact_file_for_patterns(srcroot, "src/libbsg/tests/CMakeLists.txt",
	    stage15_no_payload_typed_header_patterns,
	    sizeof(stage15_no_payload_typed_header_patterns) / sizeof(stage15_no_payload_typed_header_patterns[0]),
	    "Stage 15 public-header isolation list must not include the private typed payload header");

    _scan_exact_file_for_patterns(srcroot, "include/rt/functab.h",
	    stage15_rt_lod_payload_patterns,
	    sizeof(stage15_rt_lod_payload_patterns) / sizeof(stage15_rt_lod_payload_patterns[0]),
	    "Stage 15 primitive LoD realization API must not expose generic BSG payload owners");

    for (size_t i = 0; i < sizeof(stage15_backend_payload_files) / sizeof(stage15_backend_payload_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, stage15_backend_payload_files[i],
		stage15_backend_payload_patterns,
		sizeof(stage15_backend_payload_patterns) / sizeof(stage15_backend_payload_patterns[0]),
		"Stage 15 dm backends must consume resolved render-item geometry, not public payload compatibility");
    }

    for (size_t i = 0; i < sizeof(ged_segment_export_consumer_files) / sizeof(ged_segment_export_consumer_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, ged_segment_export_consumer_files[i],
		ged_segment_export_consumer_patterns,
		sizeof(ged_segment_export_consumer_patterns) / sizeof(ged_segment_export_consumer_patterns[0]),
		"GED plot/ps/png/select must consume semantic export segment/point iterators, not vlist records");
    }

    _scan_exact_file_for_patterns(srcroot, "src/libdm/view.c",
	    dm_arrow_segment_patterns,
	    sizeof(dm_arrow_segment_patterns) / sizeof(dm_arrow_segment_patterns[0]),
	    "libdm view arrows must consume render-item segment iterators, not vlist records");

    for (size_t i = 0; i < sizeof(render_export_vlist_contract_files) / sizeof(render_export_vlist_contract_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, render_export_vlist_contract_files[i],
		render_export_vlist_contract_patterns,
		sizeof(render_export_vlist_contract_patterns) / sizeof(render_export_vlist_contract_patterns[0]),
		"Render/export/backend contracts must expose typed geometry records, not backend-visible vlist geometry");
    }

    for (size_t i = 0; i < sizeof(private_vlist_payload_files) / sizeof(private_vlist_payload_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, private_vlist_payload_files[i],
		private_vlist_payload_patterns,
		sizeof(private_vlist_payload_patterns) / sizeof(private_vlist_payload_patterns[0]),
		"Private node-owned vlist payload adapters must stay deleted");
    }

    for (size_t i = 0; i < sizeof(render_bounds_payload_name_files) / sizeof(render_bounds_payload_name_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, render_bounds_payload_name_files[i],
		render_bounds_payload_name_patterns,
		sizeof(render_bounds_payload_name_patterns) / sizeof(render_bounds_payload_name_patterns[0]),
		"Render/export bounds records must use geometry-neutral bounds naming");
    }

    _scan_exact_file_for_patterns(srcroot, "src/libged/draw/preview.cpp",
	    ged_preview_eye_path_patterns,
	    sizeof(ged_preview_eye_path_patterns) / sizeof(ged_preview_eye_path_patterns[0]),
	    "GED preview eye-path overlays must publish typed line features, not vlblock fallbacks");

    for (size_t i = 0; i < sizeof(ged_typed_overlay_files) / sizeof(ged_typed_overlay_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, ged_typed_overlay_files[i],
		ged_typed_overlay_patterns,
		sizeof(ged_typed_overlay_patterns) / sizeof(ged_typed_overlay_patterns[0]),
		"GED analysis overlays must publish typed line-layer features, not vlblock fallback solids");
    }

    _scan_exact_file_for_patterns(srcroot, "src/libged/nirt/nirt.cpp",
	    ged_nirt_typed_uplot_patterns,
	    sizeof(ged_nirt_typed_uplot_patterns) / sizeof(ged_nirt_typed_uplot_patterns[0]),
	    "GED nirt query-ray graphics must publish typed uplot features, not vlblock fallback solids");

    _scan_exact_file_for_patterns(srcroot, "src/libged/qray.cpp",
	    ged_qray_typed_patterns,
	    sizeof(ged_qray_typed_patterns) / sizeof(ged_qray_typed_patterns[0]),
	    "GED qray support must not expose legacy vlist plotting helpers");

    _scan_exact_file_for_patterns(srcroot, "src/libged/qray.h",
	    ged_qray_typed_patterns,
	    sizeof(ged_qray_typed_patterns) / sizeof(ged_qray_typed_patterns[0]),
	    "GED qray support must not expose legacy vlist plotting helpers");

    _scan_exact_file_for_patterns(srcroot, "src/libged/rtcheck/rtcheck.c",
	    ged_rtcheck_frontend_patterns,
	    sizeof(ged_rtcheck_frontend_patterns) / sizeof(ged_rtcheck_frontend_patterns[0]),
	    "GED rtcheck front-end must dispatch to typed uplot feature handling, not legacy vlblock readers");

    _scan_exact_file_for_patterns(srcroot, "src/libged/overlay/overlay.c",
	    ged_overlay_typed_uplot_patterns,
	    sizeof(ged_overlay_typed_uplot_patterns) / sizeof(ged_overlay_typed_uplot_patterns[0]),
	    "GED plot overlays must import uplot as typed line-layer features, not legacy vlblock solids");

    for (size_t i = 0; i < sizeof(ged_typed_overlay_insert_files) / sizeof(ged_typed_overlay_insert_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, ged_typed_overlay_insert_files[i],
		ged_typed_overlay_insert_patterns,
		sizeof(ged_typed_overlay_insert_patterns) / sizeof(ged_typed_overlay_insert_patterns[0]),
		"GED overlay insertion must publish typed geometry, not vlist overlays");
    }

    _scan_exact_file_for_patterns(srcroot, "src/libged/bsg_ged_draw_refs.c",
	    ged_draw_ref_typed_geometry_patterns,
	    sizeof(ged_draw_ref_typed_geometry_patterns) / sizeof(ged_draw_ref_typed_geometry_patterns[0]),
	    "GED draw-ref geometry mutations must use typed geometry fields, not vlist round trips");

    for (size_t i = 0; i < sizeof(ged_draw_geometry_publish_files) / sizeof(ged_draw_geometry_publish_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, ged_draw_geometry_publish_files[i],
		ged_draw_geometry_publish_patterns,
		sizeof(ged_draw_geometry_publish_patterns) / sizeof(ged_draw_geometry_publish_patterns[0]),
		"GED draw geometry publication must replace vlist streams with typed geometry, not rebuild typed geometry into vlist append buffers");
    }

    _scan_exact_file_for_patterns(srcroot, "src/libged/ged_util.cpp",
	    ged_line_layer_bridge_patterns,
	    sizeof(ged_line_layer_bridge_patterns) / sizeof(ged_line_layer_bridge_patterns[0]),
	    "GED must not keep a line-layer-to-vlist overlay bridge");

    _scan_exact_file_for_patterns(srcroot, "src/libged/ged_util.cpp",
	    ged_util_typed_uplot_patterns,
	    sizeof(ged_util_typed_uplot_patterns) / sizeof(ged_util_typed_uplot_patterns[0]),
	    "GED uplot utilities must import text and line data as typed feature geometry, not temporary vlists");

    _scan_exact_file_for_patterns(srcroot, "src/libged/vdraw/vdraw.c",
	    ged_vdraw_typed_storage_patterns,
	    sizeof(ged_vdraw_typed_storage_patterns) / sizeof(ged_vdraw_typed_storage_patterns[0]),
	    "GED vdraw command storage must stay typed and must not return to vlist chunks");

    _scan_exact_file_for_patterns(srcroot, "src/libged/ged_private.h",
	    ged_vdraw_typed_storage_patterns,
	    sizeof(ged_vdraw_typed_storage_patterns) / sizeof(ged_vdraw_typed_storage_patterns[0]),
	    "GED private vdraw state must not expose vlist chunk storage");

    _scan_exact_file_for_patterns(srcroot, "src/libged/ged_private.h",
	    ged_line_layer_bridge_patterns,
	    sizeof(ged_line_layer_bridge_patterns) / sizeof(ged_line_layer_bridge_patterns[0]),
	    "GED private API must not expose a line-layer-to-vlist overlay bridge");

    _scan_exact_file_for_patterns(srcroot, "src/libged/brep/plot.cpp",
	    ged_brep_typed_plot_patterns,
	    sizeof(ged_brep_typed_plot_patterns) / sizeof(ged_brep_typed_plot_patterns[0]),
	    "GED BREP plotting must publish typed line-layer/face-set features, not legacy fallback solids");

    _scan_exact_file_for_patterns(srcroot, "src/libged/vutil.c",
	    ged_nmg_edgeuse_typed_patterns,
	    sizeof(ged_nmg_edgeuse_typed_patterns) / sizeof(ged_nmg_edgeuse_typed_patterns[0]),
	    "GED NMG edge-use diagnostics must publish typed line-layer features without vlist free-list plumbing");

    _scan_exact_file_for_patterns(srcroot, "include/ged/view.h",
	    ged_nmg_edgeuse_typed_patterns,
	    sizeof(ged_nmg_edgeuse_typed_patterns) / sizeof(ged_nmg_edgeuse_typed_patterns[0]),
	    "GED NMG edge-use diagnostics must publish typed line-layer features without vlist free-list plumbing");

    _scan_exact_file_for_patterns(srcroot, "src/mged/menu.c",
	    ged_nmg_edgeuse_typed_patterns,
	    sizeof(ged_nmg_edgeuse_typed_patterns) / sizeof(ged_nmg_edgeuse_typed_patterns[0]),
	    "MGED NMG edge-use diagnostics must call the typed line-layer API without vlist free-list plumbing");

    _scan_exact_file_for_patterns(srcroot, "src/libged/draw/draw.c",
	    ged_line_layer_bridge_patterns,
	    sizeof(ged_line_layer_bridge_patterns) / sizeof(ged_line_layer_bridge_patterns[0]),
	    "GED draw must publish NMG edge-use line layers without legacy line-layer-to-vlist conversion");

    _scan_exact_file_for_patterns(srcroot, "src/libged/draw/draw.c",
	    ged_draw_shaded_face_dispatch_patterns,
	    sizeof(ged_draw_shaded_face_dispatch_patterns) / sizeof(ged_draw_shaded_face_dispatch_patterns[0]),
	    "GED shaded primitive dispatch must use ft_indexed_face_set capability checks, not primitive-specific type lists");

    for (size_t i = 0; i < sizeof(ged_typed_diagnostic_overlay_files) / sizeof(ged_typed_diagnostic_overlay_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, ged_typed_diagnostic_overlay_files[i],
		ged_typed_diagnostic_overlay_patterns,
		sizeof(ged_typed_diagnostic_overlay_patterns) / sizeof(ged_typed_diagnostic_overlay_patterns[0]),
		"GED diagnostic overlays must publish typed line features, not legacy vlblock fallback solids");
    }

    for (size_t i = 0; i < sizeof(ged_lint_typed_plot_files) / sizeof(ged_lint_typed_plot_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, ged_lint_typed_plot_files[i],
		ged_lint_typed_plot_patterns,
		sizeof(ged_lint_typed_plot_patterns) / sizeof(ged_lint_typed_plot_patterns[0]),
		"GED lint visualization must use typed line points, not legacy vlblock fallback state");
    }

    for (size_t i = 0; i < sizeof(ged_bot_typed_plot_files) / sizeof(ged_bot_typed_plot_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, ged_bot_typed_plot_files[i],
		ged_bot_typed_plot_patterns,
		sizeof(ged_bot_typed_plot_patterns) / sizeof(ged_bot_typed_plot_patterns[0]),
		"GED bot plot/check visualization must use typed line features, not legacy vlblock fallback state");
    }

    _scan_exact_file_for_patterns(srcroot, "src/libged/bsg_ged_draw_source.c",
	    ged_draw_source_modern_plot_patterns,
	    sizeof(ged_draw_source_modern_plot_patterns) / sizeof(ged_draw_source_modern_plot_patterns[0]),
	    "GED draw-source primitive publication must use typed publishers, not primitive ft_plot bridges");

    _scan_exact_file_for_patterns(srcroot, "src/libqtcad/tests/qsketch.cpp",
	    qtcad_sketch_typed_preview_patterns,
	    sizeof(qtcad_sketch_typed_preview_patterns) / sizeof(qtcad_sketch_typed_preview_patterns[0]),
	    "qsketch preview must publish typed sketch line sets, not primitive ft_plot/vlist conversions");

    for (size_t i = 0; i < sizeof(scene_obj_hook_files) / sizeof(scene_obj_hook_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, scene_obj_hook_files[i],
		scene_obj_hook_patterns,
		sizeof(scene_obj_hook_patterns) / sizeof(scene_obj_hook_patterns[0]),
		"primitive scene-object hooks have been retired");
    }

    _scan_exact_file_for_patterns(srcroot, "src/libged/bsg_ged_draw.c",
	    draw_source_lifecycle_patterns,
	    sizeof(draw_source_lifecycle_patterns) / sizeof(draw_source_lifecycle_patterns[0]),
	    "draw-source code must not use generic lifecycle data slots");

    _scan_exact_file_for_patterns(srcroot, "src/libged/draw.cpp",
	    draw_source_release_patterns,
	    sizeof(draw_source_release_patterns) / sizeof(draw_source_release_patterns[0]),
	    "DB draw-source realization must use source-payload cleanup, not generic release callbacks");

    for (size_t i = 0; i < sizeof(renderer_root_boundary_files) / sizeof(renderer_root_boundary_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, renderer_root_boundary_files[i],
		renderer_root_boundary_patterns,
		sizeof(renderer_root_boundary_patterns) / sizeof(renderer_root_boundary_patterns[0]),
		"renderer/export/backend public consumers must not expose graph-root render inputs");
    }

    for (size_t i = 0; i < sizeof(renderer_execute_files) / sizeof(renderer_execute_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, renderer_execute_files[i],
		renderer_execute_patterns,
		sizeof(renderer_execute_patterns) / sizeof(renderer_execute_patterns[0]),
		"production renderer/export/backend consumers must use render batches, not the compatibility executor");
    }

    for (size_t i = 0; i < sizeof(render_record_taxonomy_files) / sizeof(render_record_taxonomy_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, render_record_taxonomy_files[i],
		render_record_taxonomy_patterns,
		sizeof(render_record_taxonomy_patterns) / sizeof(render_record_taxonomy_patterns[0]),
		"public render/export/backend records must expose typed semantic fields, not graph taxonomy mirrors");
    }

    _scan_exact_file_for_patterns(srcroot, "src/libqtcad/QgQuadView.cpp",
	    qtcad_quad_bridge_patterns,
	    sizeof(qtcad_quad_bridge_patterns) / sizeof(qtcad_quad_bridge_patterns[0]),
	    "libqtcad quadrant mirroring must use retained export records, not private draw-scene bridges");

    _scan_exact_file_for_patterns(srcroot, "include/bsg/scene_builder.h",
	    scene_builder_header_patterns,
	    sizeof(scene_builder_header_patterns) / sizeof(scene_builder_header_patterns[0]),
	    "typed scene builder headers must not expose generic node construction");

    for (size_t i = 0; i < sizeof(stage2_typed_node_authoring_files) / sizeof(stage2_typed_node_authoring_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, stage2_typed_node_authoring_files[i],
		stage2_scene_ref_authoring_patterns,
		sizeof(stage2_scene_ref_authoring_patterns) / sizeof(stage2_scene_ref_authoring_patterns[0]),
		"Stage 2 migrated callers must use typed node refs instead of scene-ref authoring");
    }

    for (size_t i = 0; i < sizeof(stage2_raw_node_authoring_files) / sizeof(stage2_raw_node_authoring_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, stage2_raw_node_authoring_files[i],
		stage2_raw_node_authoring_patterns,
		sizeof(stage2_raw_node_authoring_patterns) / sizeof(stage2_raw_node_authoring_patterns[0]),
		"Stage 2 migrated callers must use typed node refs instead of raw node construction/mutation");
    }

    _scan_exact_file_for_patterns(srcroot, "include/bsg/node.h",
	    stage2_node_header_boundary_patterns,
	    sizeof(stage2_node_header_boundary_patterns) / sizeof(stage2_node_header_boundary_patterns[0]),
	    "Stage 2 public node API must expose typed refs, not raw nodes, taxonomy bits, or scene-ref bridge details");

    _scan_exact_file_for_patterns(srcroot, "include/bsg/node.h",
	    stage3_node_header_boundary_patterns,
	    sizeof(stage3_node_header_boundary_patterns) / sizeof(stage3_node_header_boundary_patterns[0]),
	    "Stage 3 public child-list APIs must live on typed group/separator/switch headers, not base node refs");

    for (size_t i = 0; i < sizeof(stage3_group_header_files) / sizeof(stage3_group_header_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, stage3_group_header_files[i],
		stage3_group_header_boundary_patterns,
		sizeof(stage3_group_header_boundary_patterns) / sizeof(stage3_group_header_boundary_patterns[0]),
		"Stage 3 group/separator/switch headers must expose typed refs, not raw nodes or taxonomy");
    }

    _scan_exact_file_for_patterns(srcroot, "include/bsg/field.h",
	    stage4_field_header_boundary_patterns,
	    sizeof(stage4_field_header_boundary_patterns) / sizeof(stage4_field_header_boundary_patterns[0]),
	    "Stage 4 public field API must expose opaque field refs, not raw field IDs or raw node mutation");

    _scan_exact_file_for_patterns(srcroot, "include/bsg/sensor.h",
	    stage5_sensor_header_boundary_patterns,
	    sizeof(stage5_sensor_header_boundary_patterns) / sizeof(stage5_sensor_header_boundary_patterns[0]),
	    "Stage 5 public sensor API must expose typed sensor refs, not raw sensor nodes or raw field IDs");

    for (size_t i = 0; i < sizeof(stage5_sensor_taxonomy_files) / sizeof(stage5_sensor_taxonomy_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, stage5_sensor_taxonomy_files[i],
		stage5_sensor_taxonomy_patterns,
		sizeof(stage5_sensor_taxonomy_patterns) / sizeof(stage5_sensor_taxonomy_patterns[0]),
		"Stage 5 migrated code must use typed sensor refs, not sensor taxonomy nodes");
    }

    _scan_exact_file_for_patterns(srcroot, "include/bsg/state.h",
	    stage6_state_header_boundary_patterns,
	    sizeof(stage6_state_header_boundary_patterns) / sizeof(stage6_state_header_boundary_patterns[0]),
	    "Stage 6 public state API must expose opaque state refs and typed refs, not raw graph storage");

    for (size_t i = 0; i < sizeof(stage6_state_consumer_files) / sizeof(stage6_state_consumer_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, stage6_state_consumer_files[i],
		stage6_state_consumer_patterns,
		sizeof(stage6_state_consumer_patterns) / sizeof(stage6_state_consumer_patterns[0]),
		"Stage 6 migrated render/pick/snap consumers must read traversal state through bsg_state_ref accessors");
    }

    _scan_exact_file_for_patterns(srcroot, "include/bsg/action.h",
	    stage7_action_header_boundary_patterns,
	    sizeof(stage7_action_header_boundary_patterns) / sizeof(stage7_action_header_boundary_patterns[0]),
	    "Stage 7 public action API must expose opaque action refs, not raw traversal or legacy action storage");

    for (size_t i = 0; i < sizeof(stage7_action_wrapper_files) / sizeof(stage7_action_wrapper_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, stage7_action_wrapper_files[i],
		stage7_action_wrapper_patterns,
		sizeof(stage7_action_wrapper_patterns) / sizeof(stage7_action_wrapper_patterns[0]),
		"Stage 7 migrated wrappers must apply actions rather than performing private traversal directly");
    }

    for (size_t i = 0; i < sizeof(stage8_state_node_header_files) / sizeof(stage8_state_node_header_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, stage8_state_node_header_files[i],
		stage8_state_node_header_patterns,
		sizeof(stage8_state_node_header_patterns) / sizeof(stage8_state_node_header_patterns[0]),
		"Stage 8 state-node headers must expose typed refs and field refs, not raw node storage or private field IDs");
    }

    _scan_exact_file_for_patterns(srcroot, "include/bsg/geometry.h",
	    stage9_geometry_header_patterns,
	    sizeof(stage9_geometry_header_patterns) / sizeof(stage9_geometry_header_patterns[0]),
	    "Stage 9 public geometry API must expose typed refs and fields, not payload/vlist/raw-node authoring");

    _scan_exact_file_for_patterns(srcroot, "src/libqtcad/tests/qsketch.cpp",
	    stage9_qsketch_payload_patterns,
	    sizeof(stage9_qsketch_payload_patterns) / sizeof(stage9_qsketch_payload_patterns[0]),
	    "Stage 9 qsketch authoring must use typed geometry nodes, not sketch payload or generic shape authoring");

    _scan_exact_file_for_patterns(srcroot, "include/bsg/lod.h",
	    stage10_lod_header_patterns,
	    sizeof(stage10_lod_header_patterns) / sizeof(stage10_lod_header_patterns[0]),
	    "Stage 10 public LoD API must expose typed LoD fields, not source-policy callbacks");

    _scan_exact_file_for_patterns(srcroot, "include/bsg/CMakeLists.txt",
	    stage_c_install_boundary_patterns,
	    sizeof(stage_c_install_boundary_patterns) / sizeof(stage_c_install_boundary_patterns[0]),
	    "Stage C installed BSG headers must not include raw node/field authoring wrappers");

    _scan_exact_file_for_patterns(srcroot, "include/bsg/payload.h",
	    stage_c_payload_header_patterns,
	    sizeof(stage_c_payload_header_patterns) / sizeof(stage_c_payload_header_patterns[0]),
	    "Stage C public payload API must not expose raw-node storage binding");

    _scan_exact_file_for_patterns(srcroot, "include/bsg/lod.h",
	    stage_c_lod_header_patterns,
	    sizeof(stage_c_lod_header_patterns) / sizeof(stage_c_lod_header_patterns[0]),
	    "Stage C public LoD API must use scene refs instead of raw nodes");

    _scan_exact_file_for_patterns(srcroot, "include/bsg/util.h",
	    stage_c_util_header_patterns,
	    sizeof(stage_c_util_header_patterns) / sizeof(stage_c_util_header_patterns[0]),
	    "Stage C public util API must not expose raw scene-root storage");

    for (size_t i = 0; i < sizeof(stage0_installed_bsg_header_files) / sizeof(stage0_installed_bsg_header_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, stage0_installed_bsg_header_files[i],
		stage16_public_final_boundary_patterns,
		sizeof(stage16_public_final_boundary_patterns) / sizeof(stage16_public_final_boundary_patterns[0]),
		"Stage 16 installed BSG headers must expose typed refs/records, not raw node or payload/taxonomy internals");
    }

    _scan_exact_file_for_patterns(srcroot, "src/libbsg/CMakeLists.txt",
	    stage16_deleted_private_bridge_patterns,
	    sizeof(stage16_deleted_private_bridge_patterns) / sizeof(stage16_deleted_private_bridge_patterns[0]),
	    "Stage 16 deleted private raw-node bridge headers must not be listed");

    _scan_exact_file_for_patterns(srcroot, "include/bsg/defines.h",
	    stage_d_defines_header_patterns,
	    sizeof(stage_d_defines_header_patterns) / sizeof(stage_d_defines_header_patterns[0]),
	    "Stage D public defines must not expose raw node-kind bits or scene-node aliases");

    for (size_t i = 0; i < sizeof(stage0_public_header_boundary_patterns) / sizeof(stage0_public_header_boundary_patterns[0]); i++) {
	struct hygiene_rule rule = {
	    "include/bsg",
	    stage0_public_header_boundary_patterns[i],
	    "Stage 0 installed BSG headers must not expose raw node storage, scene-root lifecycle, raw payload binding, or node-vlist mutation",
	    NULL,
	    NULL,
	    NULL
	};
	_scan_tree(srcroot, rule.root, &rule, 1);
    }

    _scan_exact_file_for_patterns(srcroot, "include/bsg/database_source.h",
	    database_source_header_patterns,
	    sizeof(database_source_header_patterns) / sizeof(database_source_header_patterns[0]),
	    "database-source public API must use typed refs, fields, and records");

    for (size_t i = 0; i < sizeof(stage0_installed_bsg_header_files) / sizeof(stage0_installed_bsg_header_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, stage0_installed_bsg_header_files[i],
		stage0_taxonomy_definition_patterns,
		sizeof(stage0_taxonomy_definition_patterns) / sizeof(stage0_taxonomy_definition_patterns[0]),
		"Stage 0 installed BSG headers must not define raw node taxonomy bits");
    }

    _scan_exact_file_for_patterns(srcroot, "include/bsg/object.h",
	    stage1_object_header_boundary_patterns,
	    sizeof(stage1_object_header_boundary_patterns) / sizeof(stage1_object_header_boundary_patterns[0]),
	    "Stage 1 public object API must expose type IDs and object refs, not node taxonomy or scene-ref bridge details");

    for (size_t i = 0; i < sizeof(stage0_installed_bsg_header_files) / sizeof(stage0_installed_bsg_header_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot, stage0_installed_bsg_header_files[i],
		stage1_private_object_bridge_patterns,
		sizeof(stage1_private_object_bridge_patterns) / sizeof(stage1_private_object_bridge_patterns[0]),
		"Stage 1 scene-ref/object conversion and node type mutation helpers must remain source-private");
    }

    for (size_t i = 0; i < sizeof(stage17_view_storage_files) / sizeof(stage17_view_storage_files[0]); i++) {
	for (size_t j = 0; j < sizeof(stage17_view_storage_patterns) / sizeof(stage17_view_storage_patterns[0]); j++) {
	    struct hygiene_rule rule = {
		stage17_view_storage_files[i],
		stage17_view_storage_patterns[j],
		"Stage 17 non-libbsg code must use scene refs, view-state records, and render APIs instead of BSG view drawing storage",
		NULL,
		NULL,
		NULL
	    };
	    _scan_tree(srcroot, rule.root, &rule, 1);
	}
	for (size_t j = 0; j < sizeof(stage17_render_request_patterns) / sizeof(stage17_render_request_patterns[0]); j++) {
	    struct hygiene_rule rule = {
		stage17_view_storage_files[i],
		stage17_render_request_patterns[j],
		"Stage 17 non-libbsg code must configure render requests and batches through public accessors",
		NULL,
		NULL,
		NULL
	    };
	    _scan_tree(srcroot, rule.root, &rule, 1);
	}
    }

    _scan_exact_file_for_patterns(srcroot, "include/bsg/render.h",
	    stage17_render_header_patterns,
	    sizeof(stage17_render_header_patterns) / sizeof(stage17_render_header_patterns[0]),
	    "Stage 17 public render API must expose opaque request/batch handles, not execution storage");

    _scan_exact_file_for_patterns(srcroot, "include/bsg/action.h",
	    stage17_action_header_patterns,
	    sizeof(stage17_action_header_patterns) / sizeof(stage17_action_header_patterns[0]),
	    "Stage 17 public action API must expose concrete action products, not custom method registration hooks");

    _scan_exact_file_for_patterns(srcroot, "include/bsg.h",
	    stage17_umbrella_legacy_patterns,
	    sizeof(stage17_umbrella_legacy_patterns) / sizeof(stage17_umbrella_legacy_patterns[0]),
	    "Stage 17 umbrella header must not pull in legacy vlist/plot/TIG/draw-context utility headers");

    _scan_exact_file_for_patterns(srcroot, "include/bsg.h",
	    bsg_umbrella_stability_warning_patterns,
	    sizeof(bsg_umbrella_stability_warning_patterns) / sizeof(bsg_umbrella_stability_warning_patterns[0]),
	    "BSG umbrella header must describe stable public/private boundaries instead of a blanket experimental warning");

    for (size_t i = 0; i < sizeof(modern_installed_vlist_roots) / sizeof(modern_installed_vlist_roots[0]); i++) {
	_scan_tree_for_patterns_outside_allowed(srcroot,
		modern_installed_vlist_roots[i],
		modern_installed_vlist_patterns,
		sizeof(modern_installed_vlist_patterns) / sizeof(modern_installed_vlist_patterns[0]),
		modern_installed_vlist_allowed_paths,
		sizeof(modern_installed_vlist_allowed_paths) / sizeof(modern_installed_vlist_allowed_paths[0]),
		NULL,
		0,
		"Modern installed drawing headers must not expose legacy vlist/vlblock APIs outside explicit vlist compatibility headers");
    }

    for (size_t i = 0; i < sizeof(modern_installed_vlist_exact_files) / sizeof(modern_installed_vlist_exact_files[0]); i++) {
	_scan_exact_file_for_patterns(srcroot,
		modern_installed_vlist_exact_files[i],
		modern_installed_vlist_patterns,
		sizeof(modern_installed_vlist_patterns) / sizeof(modern_installed_vlist_patterns[0]),
		"Modern installed drawing umbrella headers must not include legacy vlist/vlblock APIs");
    }

    _scan_exact_file_for_patterns(srcroot, "include/raytrace.h",
	    raytrace_umbrella_vlist_patterns,
	    sizeof(raytrace_umbrella_vlist_patterns) / sizeof(raytrace_umbrella_vlist_patterns[0]),
	    "raytrace umbrella must not expose legacy vlist/vlblock APIs; include rt/vlist.h explicitly");

    _scan_tree_for_patterns_outside_allowed(srcroot, "src/libbsg",
	    stage18_raw_authority_patterns,
	    sizeof(stage18_raw_authority_patterns) / sizeof(stage18_raw_authority_patterns[0]),
	    stage18_raw_authority_allowed_paths,
	    sizeof(stage18_raw_authority_allowed_paths) / sizeof(stage18_raw_authority_allowed_paths[0]),
	    stage18_raw_authority_allowed_prefixes,
	    sizeof(stage18_raw_authority_allowed_prefixes) / sizeof(stage18_raw_authority_allowed_prefixes[0]),
	    "Stage 18 legacy raw storage authority must stay inside named compatibility adapters");

    if (g_failures) {
	fprintf(stderr, "%d API hygiene issue(s) found\n", g_failures);
	return 1;
    }

    printf("API hygiene checks passed\n");
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
