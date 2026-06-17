/*                  C M D _ S C H E M A S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024-2026 United States Government as represented by
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
/** @file libged/cmd_schemas.cpp
 *
 * Command option schema definitions for libged commands.
 *
 */

#include "common.h"

#include "bu/opt.h"
#include "bu/str.h"
#include "ged.h"

#include "./ged_private.h"

namespace {

static int open_force_create = 0;
static int open_flip_endian = 0;
static int open_help = 0;
static int open_only = 0;

static int draw_help = 0;
static int draw_mode = 0;
static int draw_wireframe = 0;
static int draw_shaded = 0;
static int draw_shaded_all = 0;
static int draw_evaluate = 0;
static int draw_hidden_line = 0;
static int draw_add_mode = 0;
static fastf_t draw_transparency = 0.0;
static int draw_bot_threshold = 0;
static int draw_no_subtract = 0;
static int draw_no_dash = 0;
static struct bu_color draw_color = BU_COLOR_INIT_ZERO;
static int draw_line_width = 0;
static int draw_no_autoview = 0;
static int draw_strict = 0;
static struct bu_vls draw_view_name = BU_VLS_INIT_ZERO;

static int ls_help = 0;
static int ls_all = 0;
static int ls_combs = 0;
static int ls_regions = 0;
static int ls_primitives = 0;
static int ls_quiet = 0;
static int ls_long = 0;
static int ls_human = 0;
static int ls_sort = 0;
static int ls_attr = 0;
static int ls_or = 0;

static struct bu_vls erase_view_name = BU_VLS_INIT_ZERO;
static int erase_mode = 0;
static int erase_recursive = 0;
static int erase_attributes = 0;
static int erase_or = 0;

static int who_help = 0;
static struct bu_vls who_view_name = BU_VLS_INIT_ZERO;
static int who_mode = 0;
static int who_expand = 0;
static int who_solids_help = 0;
static struct bu_vls who_solids_view_name = BU_VLS_INIT_ZERO;
static int who_solids_mode = 0;

static int select_help = 0;
static struct bu_vls select_set = BU_VLS_INIT_ZERO;

static int view_help = 0;
static int view_verbose = 0;
static struct bu_vls view_name = BU_VLS_INIT_ZERO;

static int dm_help = 0;
static int dm_verbose = 0;

static const struct bu_opt_operand_desc raw_args_operands[] = {
    {"args", BU_OPT_VAL_RAW, 0, BU_OPT_COUNT_UNLIMITED, "Additional subcommand arguments", NULL},
    BU_OPT_OPERAND_DESC_NULL
};

static const struct bu_opt_operand_desc open_operands[] = {
    {"filename", BU_OPT_VAL_FILE_PATH, 1, 1, "Database filename", NULL},
    BU_OPT_OPERAND_DESC_NULL
};

static const struct bu_opt_desc open_opts[] = {
    {"c", "create", "", NULL, (void *)&open_force_create, "Creates a new database if not already extant (default)."},
    {"f", "flip-endian", "", NULL, (void *)&open_flip_endian, "Opens file as a binary-incompatible v4 geometry database."},
    {"h", "help", "", NULL, (void *)&open_help, "Print help."},
    {"o", "open", "", NULL, (void *)&open_only, "Do not force creation of non-extant database.  Overridden by -c."},
    BU_OPT_DESC_NULL
};

static const struct bu_opt_desc_meta open_meta[] = {
    {"c", "create", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"f", "flip-endian", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"h", "help", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"o", "open", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    BU_OPT_DESC_META_NULL
};

static const struct bu_opt_cmd_desc open_cmd = {
    "open", "Open a geometry database", open_opts, open_meta, open_operands, NULL
};

static const struct bu_opt_cmd_desc opendb_cmd = {
    "opendb", "Open a geometry database", open_opts, open_meta, open_operands, NULL
};

static const struct bu_opt_cmd_desc reopen_cmd = {
    "reopen", "Reopen a geometry database", NULL, NULL, open_operands, NULL
};

static const struct bu_opt_operand_desc draw_operands[] = {
    {"path", BU_OPT_VAL_DB_PATH, 1, BU_OPT_COUNT_UNLIMITED, "Database object path to draw", NULL},
    BU_OPT_OPERAND_DESC_NULL
};

static const struct bu_opt_desc draw_opts[] = {
    {"V", "view", "name", &bu_opt_vls, (void *)&draw_view_name, "Specify view to draw on"},
    {"", "help", "", NULL, (void *)&draw_help, "Print help and exit"},
    {"?", "", "", NULL, (void *)&draw_help, ""},
    {"m", "mode", "#", &bu_opt_int, (void *)&draw_mode, "0=wireframe;1=shaded bots;2=shaded;3=evaluated"},
    {"", "wireframe", "", NULL, (void *)&draw_wireframe, "Draw using only wireframes (mode = 0)"},
    {"", "shaded", "", NULL, (void *)&draw_shaded, "Shade bots, breps and polysolids (mode = 1)"},
    {"", "shaded-all", "", NULL, (void *)&draw_shaded_all, "Shade all solids, not evaluated (mode = 2)"},
    {"E", "evaluate", "", NULL, (void *)&draw_evaluate, "Wireframe with evaluate booleans (mode = 3)"},
    {"h", "hidden-line", "", NULL, (void *)&draw_hidden_line, "Hidden line wireframes"},
    {"A", "add-mode", "", NULL, (void *)&draw_add_mode, "Don't erase other drawn modes for specified paths"},
    {"t", "transparency", "#", &bu_opt_fastf_t, (void *)&draw_transparency, "Set transparency level in drawing"},
    {"x", "", "#", &bu_opt_fastf_t, (void *)&draw_transparency, ""},
    {"L", "", "#", &bu_opt_int, (void *)&draw_bot_threshold, "Set face count level for drawing bounding boxes instead of BoT triangles"},
    {"S", "no-subtract", "", NULL, (void *)&draw_no_subtract, "Do not draw subtraction solids"},
    {"s", "no-dash", "", NULL, (void *)&draw_no_dash, "Use solid lines rather than dashed for subtraction solids"},
    {"C", "color", "r/g/b", &bu_opt_color, (void *)&draw_color, "Override object colors"},
    {"", "line-width", "#", &bu_opt_int, (void *)&draw_line_width, "Override default line width"},
    {"R", "no-autoview", "", NULL, (void *)&draw_no_autoview, "Do not calculate automatic view"},
    {"", "strict", "", NULL, (void *)&draw_strict, "Do not fall back to wireframe when shaded or hidden-line tessellation fails"},
    BU_OPT_DESC_NULL
};

static const struct bu_opt_desc_meta draw_meta[] = {
    {"V", "view", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_STRING, 0, NULL},
    {"", "help", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"?", "", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"m", "mode", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_INTEGER, 0, NULL},
    {"", "wireframe", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"", "shaded", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"", "shaded-all", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"E", "evaluate", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"h", "hidden-line", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"A", "add-mode", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"t", "transparency", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_NUMBER, 0, NULL},
    {"x", "", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_NUMBER, 0, NULL},
    {"L", "", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_INTEGER, 0, NULL},
    {"S", "no-subtract", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"s", "no-dash", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"C", "color", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_COLOR, 0, NULL},
    {"", "line-width", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_INTEGER, 0, NULL},
    {"R", "no-autoview", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"", "strict", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    BU_OPT_DESC_META_NULL
};

static const struct bu_opt_cmd_desc draw_cmd = {
    "draw", "Draw database objects", draw_opts, draw_meta, draw_operands, NULL
};

static const struct bu_opt_cmd_desc draw_alias_cmd = {
    "e", "Draw database objects", draw_opts, draw_meta, draw_operands, NULL
};

static const struct bu_opt_operand_desc ls_operands[] = {
    {"pattern", BU_OPT_VAL_DB_OBJECT, 0, BU_OPT_COUNT_UNLIMITED, "Object name or path pattern", NULL},
    BU_OPT_OPERAND_DESC_NULL
};

static const struct bu_opt_desc ls_opts[] = {
    {"h", "help", "", NULL, (void *)&ls_help, "Print help and exit"},
    {"a", "all", "", NULL, (void *)&ls_all, "Do not ignore static objects."},
    {"c", "combs", "", NULL, (void *)&ls_combs, "List combinations"},
    {"r", "regions", "", NULL, (void *)&ls_regions, "List regions"},
    {"p", "primitives", "", NULL, (void *)&ls_primitives, "List primitives"},
    {"s", "", "", NULL, (void *)&ls_primitives, ""},
    {"q", "quiet", "", NULL, (void *)&ls_quiet, "Suppress informational lookup messages"},
    {"l", "", "", NULL, (void *)&ls_long, "Use long reporting format"},
    {"H", "human-readable", "", NULL, (void *)&ls_human, "Use human readable sizes in long format"},
    {"S", "sort", "", NULL, (void *)&ls_sort, "Sort using object size"},
    {"A", "attributes", "", NULL, (void *)&ls_attr, "List objects matching attribute name/value pairs"},
    {"o", "or", "", NULL, (void *)&ls_or, "In attribute mode, match one or more patterns"},
    BU_OPT_DESC_NULL
};

static const struct bu_opt_desc_meta ls_meta[] = {
    {"h", "help", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"a", "all", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"c", "combs", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"r", "regions", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"p", "primitives", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"s", "", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"q", "quiet", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"l", "", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"H", "human-readable", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"S", "sort", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"A", "attributes", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"o", "or", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    BU_OPT_DESC_META_NULL
};

static const struct bu_opt_cmd_desc ls_cmd = {
    "ls", "List database objects", ls_opts, ls_meta, ls_operands, NULL
};

static const struct bu_opt_cmd_desc ls_alias_cmd = {
    "t", "List database objects", ls_opts, ls_meta, ls_operands, NULL
};

static const struct bu_opt_operand_desc erase_operands[] = {
    {"path", BU_OPT_VAL_DB_PATH, 1, BU_OPT_COUNT_UNLIMITED, "Database object path to erase", NULL},
    BU_OPT_OPERAND_DESC_NULL
};

static const struct bu_opt_desc erase_opts[] = {
    {"V", "view", "name", &bu_opt_vls, (void *)&erase_view_name, "Specify view to erase from"},
    {"m", "mode", "#", &bu_opt_int, (void *)&erase_mode, "Erase objects drawn in the specified drawing mode"},
    {"r", "recursive", "", NULL, (void *)&erase_recursive, "Erase displayed paths below the specified object path prefixes"},
    {"A", "attributes", "", NULL, (void *)&erase_attributes, "Erase displayed objects matching attribute name/value pairs"},
    {"o", "or", "", NULL, (void *)&erase_or, "In attribute mode, match any one attribute pair"},
    BU_OPT_DESC_NULL
};

static const struct bu_opt_desc_meta erase_meta[] = {
    {"V", "view", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_STRING, 0, NULL},
    {"m", "mode", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_INTEGER, 0, NULL},
    {"r", "recursive", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"A", "attributes", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"o", "or", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    BU_OPT_DESC_META_NULL
};

static const struct bu_opt_cmd_desc erase_cmd = {
    "erase", "Erase database objects from the scene", erase_opts, erase_meta, erase_operands, NULL
};

static const struct bu_opt_cmd_desc erase_alias_cmd = {
    "d", "Erase database objects from the scene", erase_opts, erase_meta, erase_operands, NULL
};

static const struct bu_opt_operand_desc who_level_operands[] = {
    {"level", BU_OPT_VAL_INTEGER, 0, 1, "Reporting detail level", NULL},
    BU_OPT_OPERAND_DESC_NULL
};

static const struct bu_opt_desc who_root_opts[] = {
    {"h", "help", "", NULL, (void *)&who_help, "Print help and exit"},
    {"?", "", "", NULL, (void *)&who_help, ""},
    {"V", "view", "name", &bu_opt_vls, (void *)&who_view_name, "Specify view to work with"},
    {"m", "mode", "#", &bu_opt_int, (void *)&who_mode, "Only report paths drawn in the specified drawing mode"},
    {"E", "expand", "", NULL, (void *)&who_expand, "Report individual drawn objects"},
    BU_OPT_DESC_NULL
};

static const struct bu_opt_desc_meta who_root_meta[] = {
    {"h", "help", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"?", "", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"V", "view", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_STRING, 0, NULL},
    {"m", "mode", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_INTEGER, 0, NULL},
    {"E", "expand", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    BU_OPT_DESC_META_NULL
};

static const struct bu_opt_desc who_solids_opts[] = {
    {"h", "help", "", NULL, (void *)&who_solids_help, "Print help and exit"},
    {"?", "", "", NULL, (void *)&who_solids_help, ""},
    {"V", "view", "name", &bu_opt_vls, (void *)&who_solids_view_name, "Specify view to report"},
    {"m", "mode", "#", &bu_opt_int, (void *)&who_solids_mode, "Only report objects drawn in the specified drawing mode"},
    BU_OPT_DESC_NULL
};

static const struct bu_opt_desc_meta who_solids_meta[] = {
    {"h", "help", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"?", "", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"V", "view", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_STRING, 0, NULL},
    {"m", "mode", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_INTEGER, 0, NULL},
    BU_OPT_DESC_META_NULL
};

static const struct bu_opt_cmd_desc who_subcommands[] = {
    {"report", "Report drawn solids", who_solids_opts, who_solids_meta, who_level_operands, NULL},
    {"solids", "Report drawn solids", who_solids_opts, who_solids_meta, who_level_operands, NULL},
    BU_OPT_CMD_DESC_NULL
};

static const struct bu_opt_cmd_desc who_cmd = {
    "who", "List drawn paths", who_root_opts, who_root_meta, NULL, who_subcommands
};

static const struct bu_opt_operand_desc select_set_operands[] = {
    {"set_name_pattern", BU_OPT_VAL_STRING, 0, 1, "Selection set name or pattern", NULL},
    BU_OPT_OPERAND_DESC_NULL
};

static const struct bu_opt_operand_desc select_path_operands[] = {
    {"path", BU_OPT_VAL_DB_PATH, 1, BU_OPT_COUNT_UNLIMITED, "Database path to add or remove", NULL},
    BU_OPT_OPERAND_DESC_NULL
};

static const struct bu_opt_desc select_opts[] = {
    {"h", "help", "", NULL, (void *)&select_help, "Print help"},
    {"S", "set", "name", &bu_opt_vls, (void *)&select_set, "Specify set to operate on"},
    BU_OPT_DESC_NULL
};

static const struct bu_opt_desc_meta select_meta[] = {
    {"h", "help", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"S", "set", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_STRING, 0, NULL},
    BU_OPT_DESC_META_NULL
};

static const struct bu_opt_cmd_desc select_subcommands[] = {
    {"add", "Add paths to the active selection set", NULL, NULL, select_path_operands, NULL},
    {"clear", "Clear one or more selection sets", NULL, NULL, select_set_operands, NULL},
    {"collapse", "Collapse one or more selection sets", NULL, NULL, select_set_operands, NULL},
    {"expand", "Expand one or more selection sets", NULL, NULL, select_set_operands, NULL},
    {"list", "List selection sets or their contents", NULL, NULL, select_set_operands, NULL},
    {"rm", "Remove paths from the active selection set", NULL, NULL, select_path_operands, NULL},
    BU_OPT_CMD_DESC_NULL
};

static const struct bu_opt_cmd_desc select_cmd = {
    "select", "Manage selection sets", select_opts, select_meta, NULL, select_subcommands
};

static const struct bu_opt_desc view_opts[] = {
    {"h", "help", "", NULL, (void *)&view_help, "Print help"},
    {"v", "verbose", "", &bu_opt_incr_long, (void *)&view_verbose, "Verbose output"},
    {"V", "view", "name", &bu_opt_vls, (void *)&view_name, "Specify view (default is GED current)"},
    BU_OPT_DESC_NULL
};

static const struct bu_opt_desc_meta view_meta[] = {
    {"h", "help", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"v", "verbose", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 1, NULL},
    {"V", "view", BU_OPT_ARG_REQUIRED, BU_OPT_VAL_STRING, 0, NULL},
    BU_OPT_DESC_META_NULL
};

static const struct bu_opt_cmd_desc view_subcommands[] = {
    {"ae", "Get or set azimuth, elevation, and twist", NULL, NULL, raw_args_operands, NULL},
    {"aet", "Get or set azimuth, elevation, and twist", NULL, NULL, raw_args_operands, NULL},
    {"center", "Get or set the view center", NULL, NULL, raw_args_operands, NULL},
    {"eye", "Get or set the view eye point", NULL, NULL, raw_args_operands, NULL},
    {"faceplate", "Manage faceplate view elements", NULL, NULL, raw_args_operands, NULL},
    {"height", "Get or set the view height", NULL, NULL, raw_args_operands, NULL},
    {"independent", "Toggle view independence", NULL, NULL, raw_args_operands, NULL},
    {"knob", "Low level rotate, translate, and scale operations", NULL, NULL, raw_args_operands, NULL},
    {"list", "List available views", NULL, NULL, raw_args_operands, NULL},
    {"lod", "Manage level-of-detail settings", NULL, NULL, raw_args_operands, NULL},
    {"obj", "Manage view features", NULL, NULL, raw_args_operands, NULL},
    {"objs", "Manage view features", NULL, NULL, raw_args_operands, NULL},
    {"quat", "Get or set the view quaternion", NULL, NULL, raw_args_operands, NULL},
    {"selections", "Manage view selections", NULL, NULL, raw_args_operands, NULL},
    {"size", "Get or set the view size", NULL, NULL, raw_args_operands, NULL},
    {"snap", "Snap the view to a canonical orientation", NULL, NULL, raw_args_operands, NULL},
    {"vZ", "Report or set scene depth range state", NULL, NULL, raw_args_operands, NULL},
    {"width", "Get or set the view width", NULL, NULL, raw_args_operands, NULL},
    {"ypr", "Get or set yaw, pitch, and roll", NULL, NULL, raw_args_operands, NULL},
    BU_OPT_CMD_DESC_NULL
};

static const struct bu_opt_cmd_desc view_cmd = {
    "view", "Manage views", view_opts, view_meta, NULL, view_subcommands
};

static const struct bu_opt_cmd_desc view2_cmd = {
    "view2", "Manage views", view_opts, view_meta, NULL, view_subcommands
};

static const struct bu_opt_cmd_desc view_func_cmd = {
    "view_func", "Manage views", view_opts, view_meta, NULL, view_subcommands
};

static const struct bu_opt_desc dm_opts[] = {
    {"h", "help", "", NULL, (void *)&dm_help, "Print help"},
    {"v", "verbose", "", &bu_opt_incr_long, (void *)&dm_verbose, "Verbose output"},
    BU_OPT_DESC_NULL
};

static const struct bu_opt_desc_meta dm_meta[] = {
    {"h", "help", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 0, NULL},
    {"v", "verbose", BU_OPT_ARG_FLAG, BU_OPT_VAL_BOOL, 1, NULL},
    BU_OPT_DESC_META_NULL
};

static const struct bu_opt_cmd_desc dm_subcommands[] = {
    {"attach", "Attach a display manager", NULL, NULL, raw_args_operands, NULL},
    {"bg", "Get or set the display manager background", NULL, NULL, raw_args_operands, NULL},
    {"debug", "Get or set display manager debugging", NULL, NULL, raw_args_operands, NULL},
    {"get", "Query display manager settings", NULL, NULL, raw_args_operands, NULL},
    {"height", "Get display manager height", NULL, NULL, raw_args_operands, NULL},
    {"initmsg", "Show display manager initialization messages", NULL, NULL, raw_args_operands, NULL},
    {"list", "List display managers", NULL, NULL, raw_args_operands, NULL},
    {"set", "Set display manager parameters", NULL, NULL, raw_args_operands, NULL},
    {"type", "Report display manager type", NULL, NULL, raw_args_operands, NULL},
    {"types", "List available display manager types", NULL, NULL, raw_args_operands, NULL},
    {"width", "Get display manager width", NULL, NULL, raw_args_operands, NULL},
    BU_OPT_CMD_DESC_NULL
};

static const struct bu_opt_cmd_desc dm_cmd = {
    "dm", "Manage display managers", dm_opts, dm_meta, NULL, dm_subcommands
};

static const struct bu_opt_cmd_desc *
open_schema(const char *cmd)
{
    if (!cmd)
        return NULL;
    if (BU_STR_EQUAL(cmd, "reopen"))
        return &reopen_cmd;
    if (BU_STR_EQUAL(cmd, "opendb"))
        return &opendb_cmd;
    return &open_cmd;
}

static const struct bu_opt_cmd_desc *
draw_schema(const char *cmd)
{
    if (cmd && BU_STR_EQUAL(cmd, "e"))
        return &draw_alias_cmd;
    return &draw_cmd;
}

static const struct bu_opt_cmd_desc *
ls_schema(const char *cmd)
{
    if (cmd && BU_STR_EQUAL(cmd, "t"))
        return &ls_alias_cmd;
    return &ls_cmd;
}

static const struct bu_opt_cmd_desc *
erase_schema(const char *cmd)
{
    if (cmd && BU_STR_EQUAL(cmd, "d"))
        return &erase_alias_cmd;
    return &erase_cmd;
}

static const struct bu_opt_cmd_desc *
view_schema(const char *cmd)
{
    if (cmd && BU_STR_EQUAL(cmd, "view2"))
        return &view2_cmd;
    if (cmd && BU_STR_EQUAL(cmd, "view_func"))
        return &view_func_cmd;
    return &view_cmd;
}

struct ged_schema_binding {
    const char *name;
    const struct bu_opt_cmd_desc *(*lookup)(const char *cmd);
};

static const struct ged_schema_binding ged_schema_bindings[] = {
    {"d", erase_schema},
    {"dm", [](const char *) -> const struct bu_opt_cmd_desc * { return &dm_cmd; }},
    {"draw", draw_schema},
    {"e", draw_schema},
    {"erase", erase_schema},
    {"ls", ls_schema},
    {"open", open_schema},
    {"opendb", open_schema},
    {"reopen", open_schema},
    {"select", [](const char *) -> const struct bu_opt_cmd_desc * { return &select_cmd; }},
    {"t", ls_schema},
    {"view", view_schema},
    {"view2", view_schema},
    {"view_func", view_schema},
    {"who", [](const char *) -> const struct bu_opt_cmd_desc * { return &who_cmd; }},
    {NULL, NULL}
};

}

extern "C" const struct bu_opt_cmd_desc *
_ged_cmd_schema(const char *cmd)
{
    if (!cmd)
        return NULL;

    const char *lookup = cmd;
    if (bu_strncmp(lookup, "_mged_", 6) == 0)
        lookup += 6;

    for (size_t i = 0; ged_schema_bindings[i].name; i++) {
        if (BU_STR_EQUAL(lookup, ged_schema_bindings[i].name))
            return ged_schema_bindings[i].lookup(lookup);
    }

    return NULL;
}
