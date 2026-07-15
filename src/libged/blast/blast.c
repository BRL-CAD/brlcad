/*                         B L A S T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/blast.c
 *
 * The blast command.
 *
 */

#include <string.h>

#include "ged.h"

/*
 * Erase all currently displayed geometry and draw the specified object(s)
 *
 * Usage:
 * blast object(s)
 *
 */
int
ged_blast_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "object(s)";

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* First, clear the screen */
    {
	const char *av[1] = {"zap"};
	ged_exec_zap(gedp, 1, av);
    }

    /* Draw the new object(s) */
    argv[0] = "draw";
    return ged_exec_draw(gedp, argc, argv);
}

#include "../include/plugin.h"


static int
blast_draw_line(struct bu_vls *line, const char *input, size_t *command_len)
{
    size_t len = 0;

    if (!line || !input)
	return -1;
    while (input[len] && input[len] != ' ' && input[len] != '\t')
	len++;
    if (!len)
	return -1;
    bu_vls_trunc(line, 0);
    bu_vls_strcat(line, "draw");
    bu_vls_strcat(line, input + len);
    if (command_len)
	*command_len = len;
    return 0;
}


static int
blast_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    struct bu_vls line = BU_VLS_INIT_ZERO;
    struct ged_cmd_validate_result delegated = GED_CMD_VALIDATE_RESULT_NULL;
    size_t command_len = 0;
    size_t draw_cursor = cursor_pos;
    int ret = -1;

    if (!input || !result || blast_draw_line(&line, input, &command_len) != 0)
	return -1;
    if (cursor_pos >= command_len)
	draw_cursor = cursor_pos - command_len + strlen("draw");
    ret = ged_cmd_validate(gedp, bu_vls_cstr(&line), draw_cursor, &delegated);
    if (!ret) {
	ged_cmd_validate_result_clear(result);
	*result = delegated;
	delegated.completion_count = 0;
	delegated.completion_candidates = NULL;
	result->hint = result->hint ? result->hint : "draw argument";
    }
    ged_cmd_validate_result_clear(&delegated);
    bu_vls_free(&line);
    return ret;
}


static int
blast_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    struct bu_vls line = BU_VLS_INIT_ZERO;
    struct ged_cmd_analysis delegated = {0, NULL};
    int ret = -1;

    if (!input || !analysis || !analysis->tokens || blast_draw_line(&line, input, NULL) != 0)
	return -1;
    ret = ged_cmd_analyze(gedp, bu_vls_cstr(&line), &delegated);
    if (!ret) {
	for (size_t i = 0; i < analysis->token_count && i < delegated.token_count; i++) {
	    analysis->tokens[i].role = delegated.tokens[i].role;
	    analysis->tokens[i].value_type = delegated.tokens[i].value_type;
	    analysis->tokens[i].semantic_state = delegated.tokens[i].semantic_state;
	    analysis->tokens[i].validator = delegated.tokens[i].validator;
	    analysis->tokens[i].hint = delegated.tokens[i].hint;
	}
	if (analysis->token_count) {
	    analysis->tokens[0].role = GED_CMD_TOKEN_COMMAND;
	    analysis->tokens[0].semantic_state = GED_CMD_SEMANTIC_VALID;
	    analysis->tokens[0].hint = "draw-compatible command";
	}
    }
    ged_cmd_analysis_clear(&delegated);
    bu_vls_free(&line);
    return ret;
}


static char *
blast_grammar_json(void)
{
    return bu_strdup("{\"kind\":\"delegated_grammar\",\"delegate\":\"draw\",\"help\":\"Clear the display and draw objects\"}");
}


static int
blast_grammar_lint(struct bu_vls *msgs)
{
    if (!ged_cmd_exists("draw")) {
	if (msgs)
	    bu_vls_strcat(msgs, "blast: delegated draw command is unavailable\n");
	return 1;
    }
    return 0;
}


static const struct ged_cmd_grammar blast_grammar = {
    "blast", "Clear the display and draw objects", blast_grammar_validate,
    blast_grammar_analyze, blast_grammar_json, blast_grammar_lint
};

#define GED_BLAST_COMMANDS(X, XID) \
    X(B, ged_blast_core, GED_CMD_DEFAULT, &blast_grammar) \
    X(blast, ged_blast_core, GED_CMD_DEFAULT, &blast_grammar)

GED_DECLARE_COMMAND_SET_WITH_GRAMMAR(GED_BLAST_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_GRAMMAR("libged_blast", 1, GED_BLAST_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
