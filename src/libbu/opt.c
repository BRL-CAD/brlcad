/*                        O P T . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2026 United States Government as represented by
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

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h> /* for strtol */
#include <limits.h> /* for INT_MAX */
#include <float.h> /* for FLT_MAX */
#include <errno.h> /* for errno */

#include "vmath.h"
#include "bu/color.h"
#include "bu/cmdschema.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/str.h"
#include "bu/vls.h"

#include "cmdschema_private.h"


static void
wrap_help(struct bu_vls *help, size_t indent, size_t offset, size_t len)
{
    size_t i = 0;
    char *input = NULL;
    char **argv = NULL;
    size_t argc = 0;
    struct bu_vls new_help = BU_VLS_INIT_ZERO;
    struct bu_vls working = BU_VLS_INIT_ZERO;
    bu_vls_trunc(&working, 0);
    bu_vls_trunc(&new_help, 0);

    input = bu_strdup(bu_vls_addr(help));
    argv = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
    argc = bu_argv_from_string(argv, strlen(input), input);

    for (i = 0; i < argc; i++) {
	size_t avl = strlen(argv[i]);
	if (bu_vls_strlen(&working) + avl + 1 > len) {
	    bu_vls_printf(&new_help, "%s\n%*s", bu_vls_addr(&working), (int)(offset+indent), " ");
	    bu_vls_trunc(&working, 0);
	}
	bu_vls_printf(&working, "%s ", argv[i]);
    }
    bu_vls_printf(&new_help, "%s", bu_vls_addr(&working));

    bu_vls_sprintf(help, "%s", bu_vls_addr(&new_help));
    bu_vls_free(&new_help);
    bu_vls_free(&working);
    bu_free(input, "input");
    bu_free(argv, "argv");
}


static int
opt_desc_is_null(const struct bu_opt_desc *ds)
{
    int non_null = 0;
    if (!ds)
	return 1;

    if (ds->shortopt)
	non_null++;
    if (ds->longopt)
	non_null++;
    if (ds->arg_process)
	non_null++;
    if (ds->arg_helpstr)
	non_null++;
    if (ds->help_string)
	non_null++;
    if (ds->set_var)
	non_null++;

    return (non_null > 0) ? 0 : 1;
}


static size_t
opt_desc_count(const struct bu_opt_desc *descs)
{
    size_t count = 0;

    if (!descs)
	return 0;
    while (!opt_desc_is_null(&descs[count]))
	count++;
    return count;
}


bu_opt_value_t
bu_opt_desc_value_type(const struct bu_opt_desc *desc)
{
    if (!desc)
	return BU_OPT_VALUE_UNKNOWN;
    if (!desc->arg_process)
	return BU_OPT_VALUE_FLAG;
    if (desc->arg_process == bu_opt_bool)
	return BU_OPT_VALUE_BOOL;
    if (desc->arg_process == bu_opt_int)
	return BU_OPT_VALUE_INTEGER;
    if (desc->arg_process == bu_opt_long)
	return BU_OPT_VALUE_LONG;
    if (desc->arg_process == bu_opt_long_hex)
	return BU_OPT_VALUE_HEX_LONG;
    if (desc->arg_process == bu_opt_incr_long)
	return BU_OPT_VALUE_INCREMENT;
    if (desc->arg_process == bu_opt_fastf_t)
	return BU_OPT_VALUE_NUMBER;
    if (desc->arg_process == bu_opt_char)
	return BU_OPT_VALUE_CHAR;
    if (desc->arg_process == bu_opt_str)
	return BU_OPT_VALUE_STRING;
    if (desc->arg_process == bu_opt_vls)
	return BU_OPT_VALUE_VLS;
    if (desc->arg_process == bu_opt_color)
	return BU_OPT_VALUE_COLOR;
    if (desc->arg_process == bu_opt_vect_t)
	return BU_OPT_VALUE_VECTOR;
    if (desc->arg_process == bu_opt_lang)
	return BU_OPT_VALUE_LANGUAGE;
    if (desc->arg_process == bu_opt_man_section)
	return BU_OPT_VALUE_MAN_SECTION;
    return BU_OPT_VALUE_UNKNOWN;
}


static const struct bu_opt_value_spec *
opt_value_spec(const struct bu_opt_desc *desc,
	const struct bu_opt_value_spec *specs)
{
    const char *name;

    if (!desc || !specs)
	return NULL;
    name = !BU_STR_EMPTY(desc->longopt) ? desc->longopt : desc->shortopt;
    if (BU_STR_EMPTY(name))
	return NULL;
    for (size_t i = 0; specs[i].option; i++)
	if (BU_STR_EQUAL(specs[i].option, name) ||
	    (!BU_STR_EMPTY(desc->shortopt) &&
	     BU_STR_EQUAL(specs[i].option, desc->shortopt)) ||
	    (!BU_STR_EMPTY(desc->longopt) &&
	     BU_STR_EQUAL(specs[i].option, desc->longopt)))
	    return &specs[i];
    return NULL;
}


static const struct bu_opt_value_spec *
opt_effective_value_spec(const struct bu_opt_desc *desc,
	const struct bu_opt_desc *descs, const struct bu_opt_value_spec *specs)
{
    const struct bu_opt_value_spec *spec = opt_value_spec(desc, specs);
    size_t limit = opt_desc_count(descs);

    for (size_t depth = 0; spec && !BU_STR_EMPTY(spec->alias_of) &&
	    depth < limit; depth++) {
	const struct bu_opt_desc *target = NULL;
	for (size_t i = 0; i < limit; i++) {
	    if ((!BU_STR_EMPTY(descs[i].shortopt) &&
		    BU_STR_EQUAL(descs[i].shortopt, spec->alias_of)) ||
		(!BU_STR_EMPTY(descs[i].longopt) &&
		    BU_STR_EQUAL(descs[i].longopt, spec->alias_of))) {
		target = &descs[i];
		break;
	    }
	}
	if (!target)
	    return NULL;
	spec = opt_value_spec(target, specs);
    }
    return spec;
}


static int
opt_value_specs_valid(const struct bu_opt_desc *descs,
	const struct bu_opt_value_spec *specs)
{
    for (size_t i = 0; specs && specs[i].option; i++) {
	size_t matches = 0;
	int alias_found = BU_STR_EMPTY(specs[i].alias_of);

	if (BU_STR_EMPTY(specs[i].option) ||
	    specs[i].value_type < BU_OPT_VALUE_UNKNOWN ||
	    specs[i].value_type > BU_OPT_VALUE_MAN_SECTION ||
	    (specs[i].max_args && specs[i].max_args < specs[i].min_args) ||
	    (specs[i].arg_count && !specs[i].min_args && !specs[i].max_args))
	    return 0;
	for (size_t prior = 0; prior < i; prior++)
	    if (BU_STR_EQUAL(specs[prior].option, specs[i].option))
		return 0;
	for (size_t di = 0; !opt_desc_is_null(&descs[di]); di++) {
	    if ((!BU_STR_EMPTY(descs[di].shortopt) &&
		 BU_STR_EQUAL(specs[i].option, descs[di].shortopt)) ||
		(!BU_STR_EMPTY(descs[di].longopt) &&
		 BU_STR_EQUAL(specs[i].option, descs[di].longopt)))
		matches++;
	    if (!alias_found &&
		((!BU_STR_EMPTY(descs[di].shortopt) &&
		  BU_STR_EQUAL(specs[i].alias_of, descs[di].shortopt)) ||
		 (!BU_STR_EMPTY(descs[di].longopt) &&
		  BU_STR_EQUAL(specs[i].alias_of, descs[di].longopt))))
		alias_found = 1;
	}
	if (matches != 1 || !alias_found)
	    return 0;
    }
    return 1;
}


bu_cmd_value_t
bu_opt_cmd_type(bu_opt_value_t type)
{
    switch (type) {
	case BU_OPT_VALUE_FLAG: return BU_CMD_VALUE_FLAG;
	case BU_OPT_VALUE_BOOL: return BU_CMD_VALUE_BOOL;
	case BU_OPT_VALUE_INTEGER: return BU_CMD_VALUE_INTEGER;
	case BU_OPT_VALUE_LONG: return BU_CMD_VALUE_LONG;
	case BU_OPT_VALUE_HEX_LONG: return BU_CMD_VALUE_HEX_LONG;
	case BU_OPT_VALUE_INCREMENT: return BU_CMD_VALUE_LONG;
	case BU_OPT_VALUE_NUMBER: return BU_CMD_VALUE_NUMBER;
	case BU_OPT_VALUE_CHAR:
	case BU_OPT_VALUE_MAN_SECTION: return BU_CMD_VALUE_CHAR;
	case BU_OPT_VALUE_COLOR: return BU_CMD_VALUE_COLOR;
	case BU_OPT_VALUE_VECTOR: return BU_CMD_VALUE_VECTOR;
	case BU_OPT_VALUE_LANGUAGE:
	case BU_OPT_VALUE_VLS: return BU_CMD_VALUE_VLS;
	case BU_OPT_VALUE_STRING:
	case BU_OPT_VALUE_UNKNOWN:
	default: return BU_CMD_VALUE_STRING;
    }
}


static const char * const opt_bool_candidates[] = {"false", "true", NULL};
static const char * const opt_man_section_candidates[] = {"1", "3", "5", "n", NULL};


const char * const *
bu_opt_desc_candidates(const struct bu_opt_desc *desc)
{
    if (!desc)
	return NULL;
    if (desc->arg_process == bu_opt_bool)
	return opt_bool_candidates;
    if (desc->arg_process == bu_opt_man_section)
	return opt_man_section_candidates;
    return NULL;
}


static struct bu_cmd_option *
opt_schema_options(const struct bu_opt_desc *descs,
	const struct bu_opt_value_spec *specs,
	struct bu_cmd_parse_binding **bindings_out,
	struct bu_cmd_arg_shape **shapes_out)
{
    size_t count = opt_desc_count(descs);
    struct bu_cmd_option *options;
    struct bu_cmd_parse_binding *bindings = NULL;
    struct bu_cmd_arg_shape *shapes = NULL;

    options = (struct bu_cmd_option *)bu_calloc(count + 1, sizeof(*options),
	"bu_opt schema adapter options");
    if (bindings_out)
	bindings = (struct bu_cmd_parse_binding *)bu_calloc(count + 1,
	    sizeof(*bindings), "bu_opt schema adapter bindings");
    if (shapes_out)
	shapes = (struct bu_cmd_arg_shape *)bu_calloc(count + 1,
	    sizeof(*shapes), "bu_opt schema adapter shapes");

    for (size_t i = 0; i < count; i++) {
	const struct bu_opt_value_spec *spec = opt_value_spec(&descs[i], specs);
	bu_opt_value_t type = spec && spec->value_type != BU_OPT_VALUE_UNKNOWN ?
	    spec->value_type : bu_opt_desc_value_type(&descs[i]);
	int takes_argument = descs[i].arg_process != NULL &&
	    descs[i].arg_process != bu_opt_incr_long;
	if (spec && (spec->value_type == BU_OPT_VALUE_FLAG ||
		spec->value_type == BU_OPT_VALUE_INCREMENT))
	    takes_argument = 0;

	options[i].shortopt = descs[i].shortopt;
	options[i].longopt = descs[i].longopt;
	options[i].canonical = spec && !BU_STR_EMPTY(spec->alias_of) ?
	    spec->alias_of : (!BU_STR_EMPTY(descs[i].longopt) ?
	    descs[i].longopt : descs[i].shortopt);
	options[i].alias_of = spec ? spec->alias_of : NULL;
	options[i].argument = descs[i].arg_helpstr;
	options[i].help = descs[i].help_string;
	options[i].value_type = bu_opt_cmd_type(type);
	options[i].storage_offset = BU_CMD_STORAGE_NONE;
	options[i].value_keywords = spec ? spec->candidates : NULL;
	if (!options[i].value_keywords)
	    options[i].value_keywords = bu_opt_desc_candidates(&descs[i]);
	options[i].arg_requirement = takes_argument ? BU_CMD_ARG_REQUIRED : BU_CMD_ARG_NONE;
	if (takes_argument && descs[i].arg_helpstr && descs[i].arg_helpstr[0] == '[')
	    options[i].arg_requirement = BU_CMD_ARG_OPTIONAL;
	if (descs[i].arg_process == bu_opt_incr_long ||
	    (spec && spec->value_type == BU_OPT_VALUE_INCREMENT))
	    options[i].repeat = 1;
	if (descs[i].arg_process == bu_opt_color) {
	    options[i].arg_shape = &bu_cmd_color_arg_shape;
	    options[i].consume = bu_cmd_color_consume;
	}
	if (descs[i].arg_process == bu_opt_vect_t) {
	    options[i].arg_shape = &bu_cmd_vector3_arg_shape;
	    options[i].consume = bu_cmd_vector3_consume;
	}
	if (descs[i].arg_process == bu_opt_lang)
	    options[i].validate = bu_cmd_iso639_1_validate;
	if (descs[i].arg_process == bu_opt_man_section)
	    options[i].validate = bu_cmd_man_section_validate;
	if (spec && (spec->min_args || spec->max_args)) {
	    size_t maximum = spec->max_args ? spec->max_args : spec->min_args;
	    if (maximum >= spec->min_args) {
		shapes[i].kind = BU_CMD_ARG_SHAPE_CUSTOM;
		shapes[i].min_tokens = spec->min_args;
		shapes[i].max_tokens = maximum;
		shapes[i].description = spec->hint ? spec->hint : descs[i].arg_helpstr;
		shapes[i].token_count = spec->arg_count;
		options[i].arg_shape = &shapes[i];
		options[i].arg_requirement = spec->min_args ?
		    BU_CMD_ARG_REQUIRED : BU_CMD_ARG_OPTIONAL;
	    }
	}
	if (bindings) {
	    bindings[i].storage = descs[i].set_var;
	    bindings[i].opt_process =
		(bu_cmd_opt_process_t)descs[i].arg_process;
	}
    }
    if (bindings_out)
	*bindings_out = bindings;
    if (shapes_out)
	*shapes_out = shapes;
    return options;
}


static int
opt_cmd_alias_target(const struct bu_opt_cmd *cmd, size_t alias_index,
	size_t *target_index)
{
    const struct bu_cmd_option *alias = &cmd->options[alias_index];
    size_t found = 0;
    size_t match = 0;

    if (BU_STR_EMPTY(alias->alias_of))
	return -1;
    for (size_t i = 0; i < cmd->option_count; i++) {
	const struct bu_cmd_option *candidate = &cmd->options[i];
	if ((!BU_STR_EMPTY(candidate->shortopt) &&
		BU_STR_EQUAL(candidate->shortopt, alias->alias_of)) ||
	    (!BU_STR_EMPTY(candidate->longopt) &&
		BU_STR_EQUAL(candidate->longopt, alias->alias_of))) {
	    found++;
	    match = i;
	}
    }
    if (found != 1 || match == alias_index)
	return -1;
    *target_index = match;
    return 0;
}


static int
opt_cmd_resolve_alias(struct bu_opt_cmd *cmd, size_t index,
	unsigned char *state)
{
    struct bu_cmd_option *alias = &cmd->options[index];
    struct bu_cmd_option *canonical;
    size_t target;

    if (BU_STR_EMPTY(alias->alias_of)) {
	state[index] = 2;
	return 0;
    }
    if (state[index] == 1)
	return -1;
    if (state[index] == 2)
	return 0;
    state[index] = 1;
    if (opt_cmd_alias_target(cmd, index, &target) ||
	opt_cmd_resolve_alias(cmd, target, state))
	return -1;
    canonical = &cmd->options[target];

    /* An alias is a spelling, not a separate argument contract.  Preserve
     * only presentation/storage fields belonging to the spelling itself. */
    alias->canonical = bu_cmd_option_canonical(canonical);
    alias->argument = canonical->argument;
    if (BU_STR_EMPTY(alias->help))
	alias->help = canonical->help;
    alias->value_type = canonical->value_type;
    alias->custom_parse = canonical->custom_parse;
    alias->validate = canonical->validate;
    alias->semantic_provider = canonical->semantic_provider;
    alias->repeat = canonical->repeat;
    alias->value_keywords = canonical->value_keywords;
    alias->arg_requirement = canonical->arg_requirement;
    alias->arg_shape = canonical->arg_shape;
    alias->consume = canonical->consume;
    alias->keyword_values = canonical->keyword_values;
    alias->range = canonical->range;
    state[index] = 2;
    return 0;
}


int
bu_opt_cmd_aliases(struct bu_opt_cmd *cmd)
{
    unsigned char *state;
    int ret = 0;

    if (!cmd || (!cmd->options && cmd->option_count))
	return -1;
    state = (unsigned char *)bu_calloc(cmd->option_count ? cmd->option_count : 1,
	sizeof(*state), "bu_opt alias states");
    for (size_t i = 0; i < cmd->option_count; i++) {
	if (opt_cmd_resolve_alias(cmd, i, state)) {
	    ret = -1;
	    break;
	}
    }
    bu_free(state, "bu_opt alias states");
    return ret;
}


void
bu_opt_cmd_clear(struct bu_opt_cmd *cmd)
{
    if (!cmd)
	return;
    if (cmd->shapes)
	bu_free(cmd->shapes, "bu_opt schema adapter shapes");
    if (cmd->options)
	bu_free(cmd->options, "bu_opt schema adapter options");
    *cmd = (struct bu_opt_cmd)BU_OPT_CMD_INIT_ZERO;
}


int
bu_opt_cmd_create(struct bu_opt_cmd *cmd, const struct bu_opt_desc *descs,
	const struct bu_opt_value_spec *specs)
{
    struct bu_opt_cmd built = BU_OPT_CMD_INIT_ZERO;

    if (!cmd || !descs || cmd->options || cmd->shapes || cmd->option_count ||
	!opt_value_specs_valid(descs, specs))
	return -1;
    built.option_count = opt_desc_count(descs);
    built.options = opt_schema_options(descs, specs, NULL, &built.shapes);
    if (!built.options || !built.shapes || bu_opt_cmd_aliases(&built)) {
	bu_opt_cmd_clear(&built);
	return -1;
    }
    *cmd = built;
    return 0;
}


static int
opt_is_filtered(const struct bu_opt_desc *d, size_t f_ac, char **f_av, int accept)
{
    size_t i = 0;
    if (!d || !f_av || f_ac == 0)
	return accept;
    for (i = 0; i < f_ac; i++) {
	if (d->shortopt && strlen(d->shortopt) > 0) {
	    if (BU_STR_EQUAL(d->shortopt, f_av[i])) {
		return !accept;
	    }
	}
	if (d->longopt && strlen(d->longopt) > 0) {
	    if (BU_STR_EQUAL(d->longopt, f_av[i])) {
		return !accept;
	    }
	}
    }
    return accept;
}


static char *
opt_describe_internal_ascii(const struct bu_opt_desc *ds, struct bu_opt_desc_opts *settings)
{
    size_t i = 0;
    size_t j = 0;
    size_t opt_cnt = 0;
    struct bu_opt_desc_opts dsettings = BU_OPT_DESC_OPTS_INIT_ZERO;
    size_t offset = dsettings.offset;
    size_t opt_cols = dsettings.option_columns;
    size_t desc_cols = dsettings.description_columns;
    char *input = NULL;
    char **filtered_argv = NULL;
    size_t filtered_argc = 0;
    int accept = 0;

    /*
      bu_opt_desc_t desc_type = BU_OPT_FULL;
      bu_opt_format_t format_type = BU_OPT_ASCII;
    */
    char *finalized;
    struct bu_vls description = BU_VLS_INIT_ZERO;
    int *status;
    if (!ds || opt_desc_is_null(&ds[0]))
	return NULL;

    if (settings) {
	if (settings->offset >= 0)
	    offset = settings->offset;
	if (settings->option_columns >= 0)
	    opt_cols = settings->option_columns;
	if (settings->description_columns >= 0)
	    desc_cols = settings->description_columns;
	if (settings->reject && settings->accept) {
	    bu_log("Error - opt_describe_internal_ascii passed both an accept and a reject list for option filtering\n");
	    return NULL;
	}
	if (settings->reject || settings->accept) {
	    input = (settings->reject) ? bu_strdup(settings->reject) : bu_strdup(settings->accept);
	    filtered_argv = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
	    filtered_argc = bu_argv_from_string(filtered_argv, strlen(input), input);
	    if (settings->accept)
		accept = 1;
	}
    }

    while (!opt_desc_is_null(&ds[i])) i++;
    if (i == 0)
	return NULL;
    opt_cnt = i;
    status = (int *)bu_calloc(opt_cnt, sizeof(int), "opt status");
    i = 0;
    while (i < opt_cnt) {
	const struct bu_opt_desc *curr;
	const struct bu_opt_desc *d;
	curr = &(ds[i]);

	if (!opt_is_filtered(curr, filtered_argc, filtered_argv, accept)) {
	    if (!status[i]) {
		struct bu_vls opts = BU_VLS_INIT_ZERO;
		struct bu_vls help_str = BU_VLS_INIT_ZERO;

		/* We handle all entries with the same set_var in the same
		 * pass, so set the status flags accordingly */
		j = i;
		while (j < opt_cnt) {
		    d = &(ds[j]);
		    if (d == curr || (d->set_var && curr->set_var && d->set_var == curr->set_var)) {
			if ((!d->arg_process && !curr->arg_process) || (d->arg_process && curr->arg_process && d->arg_process == curr->arg_process)) {
			    if (!opt_is_filtered(d, filtered_argc, filtered_argv, accept)) {
				status[j] = 1;
			    }
			}
		    }
		    j++;
		}

		/* Collect the short options first - may be multiple instances with
		 * the same set_var, so accumulate all of them. */
		j = i;
		while (j < opt_cnt) {
		    d = &(ds[j]);
		    if (d == curr || (d->set_var && curr->set_var && d->set_var == curr->set_var)) {
			if ((!d->arg_process && !curr->arg_process) || (d->arg_process && curr->arg_process && d->arg_process == curr->arg_process)) {
			    if (!opt_is_filtered(d, filtered_argc, filtered_argv, accept)) {
				if (d->shortopt && strlen(d->shortopt) > 0) {
				    struct bu_vls tmp_arg = BU_VLS_INIT_ZERO;
				    size_t new_len = strlen(d->arg_helpstr);
				    if (!new_len) {
					bu_vls_sprintf(&tmp_arg, "-%s", d->shortopt);
					new_len = 2;
				    } else {
					bu_vls_sprintf(&tmp_arg, "-%s %s", d->shortopt, d->arg_helpstr);
					new_len = new_len + 4;
				    }
				    if (bu_vls_strlen(&opts) + new_len + offset + 2 > opt_cols + desc_cols) {
					bu_vls_printf(&description, "%*s%s\n", (int)offset, " ", bu_vls_addr(&opts));
					bu_vls_sprintf(&opts, "%s, ", bu_vls_addr(&tmp_arg));
				    } else {
					bu_vls_printf(&opts, "%s, ", bu_vls_addr(&tmp_arg));
				    }
				    bu_vls_free(&tmp_arg);
				}
				/* While we're at it, pick up the string.  The last string with
				 * a matching key wins, as long as its not empty */
				if (strlen(d->help_string) > 0) {
				    bu_vls_sprintf(&help_str, "%s", d->help_string);
				}
			    }
			}
		    }
		    j++;
		}

		/* Now do the long opts */
		j = i;
		while (j < opt_cnt) {
		    d = &(ds[j]);
		    if (d == curr || (d->set_var && curr->set_var && d->set_var == curr->set_var)) {
			if ((!d->arg_process && !curr->arg_process) || (d->arg_process && curr->arg_process && d->arg_process == curr->arg_process)) {
			    if (!opt_is_filtered(d, filtered_argc, filtered_argv, accept)) {
				if (d->longopt && strlen(d->longopt) > 0) {
				    struct bu_vls tmp_arg = BU_VLS_INIT_ZERO;
				    size_t new_len = strlen(d->arg_helpstr);
				    if (!new_len) {
					bu_vls_sprintf(&tmp_arg, "--%s", d->longopt);
					new_len = strlen(d->longopt) + 2;
				    } else {
					bu_vls_sprintf(&tmp_arg, "--%s %s", d->longopt, d->arg_helpstr);
					new_len = strlen(d->longopt) + new_len + 3;
				    }
				    if (bu_vls_strlen(&opts) + new_len + offset + 2 > opt_cols + desc_cols) {
					bu_vls_printf(&description, "%*s%s\n", (int)offset, " ", bu_vls_addr(&opts));
					bu_vls_sprintf(&opts, "%s, ", bu_vls_addr(&tmp_arg));
				    } else {
					bu_vls_printf(&opts, "%s, ", bu_vls_addr(&tmp_arg));
				    }
				    bu_vls_free(&tmp_arg);
				}
			    }
			}
		    }
		    j++;
		}

		bu_vls_trunc(&opts, -2);
		bu_vls_printf(&description, "%*s%s", (int)offset, " ", bu_vls_addr(&opts));
		if (bu_vls_strlen(&opts) > opt_cols) {
		    bu_vls_printf(&description, "\n%*s", (int)(opt_cols + offset), " ");
		} else {
		    bu_vls_printf(&description, "%*s", (int)opt_cols - (int)bu_vls_strlen(&opts), " ");
		}
		if (bu_vls_strlen(&help_str) > desc_cols) {
		    wrap_help(&help_str, offset, opt_cols+offset, desc_cols);
		}
		bu_vls_printf(&description, "%*s%s\n", (int)offset, " ", bu_vls_addr(&help_str));
		bu_vls_free(&help_str);
		bu_vls_free(&opts);
		status[i] = 1;
	    }
	}
	i++;
    }
    finalized = bu_strdup(bu_vls_addr(&description));

    if (filtered_argv)
	bu_free(filtered_argv, "free filter argv");
    bu_free(input, "free filter copy");
    bu_vls_free(&description);
    return finalized;
}


#define OPT_PLAIN    0x1
#define OPT_REQUIRED 0x2
#define OPT_OPTIONAL 0x4
#define OPT_REPEAT   0x10

static int
docbook_get_opt_type(const struct bu_opt_desc *d, struct bu_opt_desc_opts *settings)
{
    const struct bu_opt_desc *curr = NULL;
    int flags = OPT_PLAIN;
    const struct bu_opt_desc *required = NULL;
    const struct bu_opt_desc *repeated = NULL;
    const struct bu_opt_desc *optional = NULL;

    if (settings) {
	required = settings->required;
	repeated = settings->repeated;
	optional = settings->optional;
    }

    if (required) {
	int j = 0;
	curr = &(settings->required[j]);
	while (curr) {
	    j++;
	    if (d == curr) {
		flags = flags & ~(OPT_PLAIN);
		flags |= OPT_REQUIRED;
		break;
	    }
	    curr = &(settings->required[j]);
	}
    }

    if (!(flags & OPT_REQUIRED)) {
	if (optional) {
	    int j = 0;
	    curr = &(optional[j]);
	    while (curr) {
		j++;
		if (d == curr) {
		    flags = flags & ~(OPT_PLAIN);
		    flags |= OPT_OPTIONAL;
		    break;
		}
		curr = &(optional[j]);
	    }
	}
    }

    if (repeated) {
	int j = 0;
	curr = &(repeated[j]);
	while (curr) {
	    j++;
	    if (d == curr) {
		flags |= OPT_REPEAT;
		break;
	    }
	    curr = &(repeated[j]);
	}
    }

    return flags;
}


static void
docbook_print_short_opt(struct bu_vls *desc, const struct bu_opt_desc *d, int opt_type, size_t offset)
{
    if (!desc || !d)
	return;
    bu_vls_printf(desc, "%*s<arg", (int)offset, " ");
    if (opt_type & OPT_PLAIN) {
	bu_vls_printf(desc, " choice='plain'");
    }
    if (opt_type & OPT_REQUIRED) {
	bu_vls_printf(desc, " choice='req'");
    }
    if (opt_type & OPT_OPTIONAL) {
	bu_vls_printf(desc, " choice='opt'");
    }
    if (opt_type & OPT_REPEAT) {
	bu_vls_printf(desc, " rep='repeat'");
    }
    bu_vls_printf(desc, ">-%c", d->shortopt[0]);
    if (d->arg_helpstr && strlen(d->arg_helpstr) > 0) {
	bu_vls_printf(desc, " <replaceable>%s</replaceable>", d->arg_helpstr);
    }
    bu_vls_printf(desc, "</arg>\n");
}


static void
docbook_print_long_opt(struct bu_vls *desc, const struct bu_opt_desc *d, int opt_type, size_t offset)
{
    if (!desc || !d)
	return;
    bu_vls_printf(desc, "%*s<arg", (int)offset, " ");
    if (opt_type & OPT_PLAIN) {
	bu_vls_printf(desc, " choice='plain'");
    }
    if (opt_type & OPT_REQUIRED) {
	bu_vls_printf(desc, " choice='req'");
    }
    if (opt_type & OPT_OPTIONAL) {
	bu_vls_printf(desc, " choice='opt'");
    }
    if (opt_type & OPT_REPEAT) {
	bu_vls_printf(desc, " rep='repeat'");
    }
    bu_vls_printf(desc, ">--%s", d->longopt);
    if (d->arg_helpstr && strlen(d->arg_helpstr) > 0) {
	bu_vls_printf(desc, " <replaceable>%s</replaceable>", d->arg_helpstr);
    }
    bu_vls_printf(desc, "</arg>\n");
}


static char *
opt_describe_internal_docbook(const struct bu_opt_desc *ds, struct bu_opt_desc_opts *settings)
{
    int opt_cnt;
    int i = 0;
    int j;
    int show_all_longopts = 0;
    char *finalized;
    struct bu_vls description = BU_VLS_INIT_ZERO;
    int *status;

    if (!ds || opt_desc_is_null(&ds[0]))
	return NULL;

    if (settings) {
	show_all_longopts = settings->show_all_longopts;
    }

    while (!opt_desc_is_null(&ds[i])) i++;
    if (i == 0)
	return NULL;
    opt_cnt = i;
    status = (int *)bu_calloc(opt_cnt, sizeof(int), "opt status");
    i = 0;
    while (i < opt_cnt) {
	const struct bu_opt_desc *curr = NULL;
	const struct bu_opt_desc *d = NULL;
	curr = &(ds[i]);
	if (!status[i]) {
	    int opt_alias_cnt = 0;
	    int need_group = 0;

	    /* We handle all entries with the same set_var in the same
	     * pass, so set the status flags accordingly */
	    j = i;
	    while (j < opt_cnt) {
		d = &(ds[j]);
		if (d == curr || (d->set_var && curr->set_var && d->set_var == curr->set_var)) {
		    status[j] = 1;
		    opt_alias_cnt++;
		}
		j++;
	    }

	    /* If we've got more than one option, make a group */
	    if (opt_alias_cnt > 1) {
		need_group = 1;
	    }
	    /* If we're showing all the opts and we've got both a short and a long, make
	     * a group */
	    if (show_all_longopts && !need_group) {
		if (curr->shortopt && strlen(d->shortopt) > 0 && curr->longopt && strlen(d->longopt) > 0) {
		    need_group = 1;
		}
	    }

	    if (need_group)
		bu_vls_printf(&description, "<group>\n");

	    /* Go with the short option, unless there isn't one. */
	    j = i;
	    while (j < opt_cnt) {
		d = &(ds[j]);
		if (d == curr || (d->set_var && curr->set_var && d->set_var == curr->set_var)) {
		    int opt_type = docbook_get_opt_type(d, settings);
		    if (d->shortopt && strlen(d->shortopt) > 0) {
			docbook_print_short_opt(&description, d, opt_type, need_group);
			/* If we're supposed to, also do the longopt */
			if (show_all_longopts && !need_group) {
			    if (d->longopt && strlen(d->longopt) > 0) {
				docbook_print_long_opt(&description, d, opt_type, need_group);
			    }
			}
		    } else {
			/* For d == curr we *need* to do a longopt if that's all we've got */
			if ((d == curr || show_all_longopts) && d->longopt && strlen(d->longopt) > 0) {
			    docbook_print_long_opt(&description, d, opt_type, need_group);
			}
		    }
		}
		j++;
	    }

	    if (need_group)
		bu_vls_printf(&description, "</group>\n");
	    status[i] = 1;

	}
	i++;
	/* add an sbr if we've reached a multiple of 5 */
	if (i%5 == 0)
	    bu_vls_printf(&description, "<sbr/>\n");
    }
    finalized = bu_strdup(bu_vls_addr(&description));
    bu_vls_free(&description);
    return finalized;
}


char *
bu_opt_describe(const struct bu_opt_desc *ds, struct bu_opt_desc_opts *settings)
{
    if (!ds)
	return NULL;
    if (!settings)
	return opt_describe_internal_ascii(ds, NULL);
    switch (settings->format) {
	case BU_OPT_ASCII:
	    return opt_describe_internal_ascii(ds, settings);
	    break;
	case BU_OPT_DOCBOOK:
	    return opt_describe_internal_docbook(ds, settings);
	    break;
	default:
	    break;
    }
    return NULL;
}


/*
 * bu_opt is the compact, option-only face of the command-schema parser.
 * Its transient schema deliberately contains no operands or semantic
 * metadata: unrecognized words are pass-through leftovers, and bindings keep
 * main's original direct-pointer/callback behavior.
 */
int
bu_opt_parse_with_policy(struct bu_vls *msgs, size_t argc, const char **argv,
	const struct bu_opt_desc *ds, bu_opt_parse_policy_t policy)
{
    struct bu_cmd_schema schema = {0};
    struct bu_opt_cmd cmd = BU_OPT_CMD_INIT_ZERO;
    struct bu_cmd_parse_binding *bindings;
    unsigned int parse_flags = BU_CMD_PARSE_INTERNAL_LEFTOVERS_FIRST |
	BU_CMD_PARSE_INTERNAL_BU_OPT_SYNTAX;
    int ret;

    if (!argv || !ds || argc > INT_MAX ||
	(policy != BU_OPT_PARSE_INTERSPERSED &&
	 policy != BU_OPT_PARSE_OPTIONS_FIRST))
	return -1;

    if (bu_opt_cmd_create(&cmd, ds, NULL))
	return -1;
    bindings = (struct bu_cmd_parse_binding *)bu_calloc(cmd.option_count + 1,
	sizeof(*bindings), "bu_opt schema adapter bindings");
    for (size_t i = 0; i < cmd.option_count; i++) {
	bindings[i].storage = ds[i].set_var;
	bindings[i].opt_process = (bu_cmd_opt_process_t)ds[i].arg_process;
    }

    schema.name = "bu_opt";
    schema.help = NULL;
    schema.options = cmd.options;
    schema.operands = NULL;
    schema.parse_policy = policy == BU_OPT_PARSE_OPTIONS_FIRST ?
	BU_CMD_PARSE_OPTIONS_FIRST : BU_CMD_PARSE_INTERSPERSED;
    schema.validation.custom_validate = NULL;
    schema.validation.constraints = NULL;
    schema.validation.context_validate = NULL;
    schema.operand_groups = NULL;

    if (policy == BU_OPT_PARSE_INTERSPERSED)
	parse_flags |= BU_CMD_PARSE_INTERNAL_PASS_UNKNOWN;
    else
	parse_flags |= BU_CMD_PARSE_INTERNAL_END_MARKER;

    ret = _bu_cmd_schema_parse_bound(&schema, NULL, msgs, (int)argc, argv,
	bindings, parse_flags);

    if (ret >= 0 && policy == BU_OPT_PARSE_OPTIONS_FIRST) {
	size_t first = (size_t)ret;
	if (first < argc && BU_STR_EQUAL(argv[first], "--"))
	    first++;
	ret = (int)(argc - first);
	if (ret > 0)
	    memmove(argv, argv + first, (size_t)ret * sizeof(*argv));
    }

    bu_free(bindings, "bu_opt schema adapter bindings");
    bu_opt_cmd_clear(&cmd);
    return ret;
}


int
bu_opt_parse(struct bu_vls *msgs, size_t argc, const char **argv,
	const struct bu_opt_desc *ds)
{
    return bu_opt_parse_with_policy(msgs, argc, argv, ds,
	BU_OPT_PARSE_INTERSPERSED);
}


void
bu_opt_validate_result_init(struct bu_opt_validate_result *result)
{
    if (!result)
	return;
    *result = (struct bu_opt_validate_result)BU_OPT_VALIDATE_RESULT_NULL;
}


void
bu_opt_validate_result_clear(struct bu_opt_validate_result *result)
{
    if (!result)
	return;
    if (result->completion_candidates)
	bu_argv_free(result->completion_count,
	    (char **)result->completion_candidates);
    bu_opt_validate_result_init(result);
}


static const struct bu_opt_desc *
opt_desc_for_token(const struct bu_opt_desc *descs, const char *token)
{
    const char *name;
    size_t name_len;
    int is_long;

    if (!descs || !token || token[0] != '-' || !token[1])
	return NULL;
    is_long = token[1] == '-';
    name = token + (is_long ? 2 : 1);
    name_len = strcspn(name, "=");
    if (!is_long && name_len > 1)
	name_len = 1;
    for (size_t i = 0; !opt_desc_is_null(&descs[i]); i++) {
	const char *spelling = is_long ? descs[i].longopt : descs[i].shortopt;
	if (!BU_STR_EMPTY(spelling) && strlen(spelling) == name_len &&
	    !bu_strncmp(spelling, name, name_len))
	    return &descs[i];
    }
    return NULL;
}


static const char *
opt_desc_attached_value(const struct bu_opt_desc *descs,
	const struct bu_opt_value_spec *specs, const char *token,
	const struct bu_opt_desc **option)
{
    if (option)
	*option = NULL;
    if (!descs || !token || token[0] != '-' || !token[1])
	return NULL;
    if (token[1] == '-') {
	const char *name = token + 2;
	const char *equal = strchr(name, '=');
	if (!equal)
	    return NULL;
	for (size_t i = 0; !opt_desc_is_null(&descs[i]); i++) {
	    if (!BU_STR_EMPTY(descs[i].longopt) &&
		    strlen(descs[i].longopt) == (size_t)(equal - name) &&
		    !bu_strncmp(descs[i].longopt, name, (size_t)(equal - name))) {
		if (option)
		    *option = &descs[i];
		return equal + 1;
	    }
	}
	return NULL;
    }

    for (size_t ti = 1; token[ti]; ti++) {
	for (size_t i = 0; !opt_desc_is_null(&descs[i]); i++) {
	    const struct bu_opt_value_spec *spec;
	    int takes_argument;
	    if (BU_STR_EMPTY(descs[i].shortopt) || descs[i].shortopt[1] ||
		    descs[i].shortopt[0] != token[ti])
		continue;
	    spec = opt_effective_value_spec(&descs[i], descs, specs);
	    takes_argument = descs[i].arg_process &&
		descs[i].arg_process != bu_opt_incr_long;
	    if (spec && (spec->value_type == BU_OPT_VALUE_FLAG ||
		    spec->value_type == BU_OPT_VALUE_INCREMENT))
		takes_argument = 0;
	    if (spec && (spec->min_args || spec->max_args))
		takes_argument = spec->max_args != 0;
	    if (takes_argument) {
		if (!token[ti + 1])
		    return NULL;
		if (option)
		    *option = &descs[i];
		return token + ti + 1;
	    }
	    break;
	}
    }
    return NULL;
}


static bu_opt_validate_state_t
opt_validate_state(bu_cmd_validate_state_t state)
{
    switch (state) {
	case BU_CMD_VALIDATE_VALID: return BU_OPT_VALIDATE_VALID;
	case BU_CMD_VALIDATE_INVALID: return BU_OPT_VALIDATE_INVALID;
	case BU_CMD_VALIDATE_INCOMPLETE: return BU_OPT_VALIDATE_INCOMPLETE;
	case BU_CMD_VALIDATE_UNKNOWN:
	default: return BU_OPT_VALIDATE_UNKNOWN;
    }
}


int
bu_opt_desc_validate(const struct bu_opt_desc *descs,
	const struct bu_opt_value_spec *specs, size_t argc, const char **argv,
	size_t cursor_arg, void *context, struct bu_opt_validate_result *result)
{
    static const struct bu_cmd_operand passthrough_operands[] = {
	BU_CMD_OPERAND("arguments", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	    "command arguments", NULL),
	BU_CMD_OPERAND_NULL
    };
    struct bu_cmd_validate_result native = BU_CMD_VALIDATE_RESULT_NULL;
    struct bu_cmd_schema schema = {0};
    struct bu_opt_cmd cmd = BU_OPT_CMD_INIT_ZERO;
    const struct bu_cmd_option *active_option = NULL;
    const struct bu_opt_value_spec *spec = NULL;
    const struct bu_opt_desc *attached_option = NULL;
    const char *attached_value = NULL;
    int ret;

    if (!result)
	return -1;
    bu_opt_validate_result_clear(result);
    if (!descs || (argc && !argv))
	return -1;
    if (bu_opt_cmd_create(&cmd, descs, specs))
	return -1;
    if (cursor_arg > argc)
	cursor_arg = argc;

    schema.name = "bu_opt";
    schema.help = NULL;
    schema.options = cmd.options;
    schema.operands = passthrough_operands;
    schema.parse_policy = BU_CMD_PARSE_INTERSPERSED;
    schema.operand_groups = NULL;
    schema.validation.custom_validate = NULL;
    schema.validation.constraints = NULL;
    schema.validation.context_validate = NULL;

    ret = _bu_cmd_schema_validate_structure(&schema, argc, argv, cursor_arg,
	&native, &active_option);
    result->state = opt_validate_state(native.state);
    result->token_start = native.token_start;
    result->token_end = native.token_end;
    result->expected = ((native.expected & BU_CMD_EXPECT_OPTION) ?
	BU_OPT_EXPECT_OPTION : BU_OPT_EXPECT_NONE) |
	((native.expected & BU_CMD_EXPECT_OPTION_ARG) ?
	 BU_OPT_EXPECT_OPTION_ARG : BU_OPT_EXPECT_NONE);
    result->hint = native.hint;
    result->option = active_option ?
	&descs[(size_t)(active_option - cmd.options)] : NULL;
    if (!(result->expected & BU_OPT_EXPECT_OPTION_ARG) &&
	!(cursor_arg < argc &&
	  opt_desc_for_token(descs, argv[cursor_arg]) == result->option))
	result->option = NULL;
    result->option_name = result->option ?
	(!BU_STR_EMPTY(result->option->longopt) ? result->option->longopt :
	 result->option->shortopt) : NULL;
    result->value_type = result->option ?
	bu_opt_desc_value_type(result->option) : BU_OPT_VALUE_UNKNOWN;
    if (result->option)
	spec = opt_effective_value_spec(result->option, descs, specs);
    if (spec) {
	if (spec->value_type != BU_OPT_VALUE_UNKNOWN)
	    result->value_type = spec->value_type;
	if (spec->hint)
	    result->hint = spec->hint;
    }

    result->completion_count = native.completion_count;
    result->completion_candidates = native.completion_candidates;
    native.completion_count = 0;
    native.completion_candidates = NULL;
    bu_cmd_validate_result_clear(&native);
    bu_opt_cmd_clear(&cmd);

    /* A complete option spelling which requires a following value is
     * incomplete, but the option word itself is not an option argument.  Do
     * not ask a value validator to validate (for example) "--format" as if
     * it were the requested format. */
    if (!ret && cursor_arg < argc)
	attached_value = opt_desc_attached_value(descs, specs, argv[cursor_arg],
	    &attached_option);
    if (!ret && attached_value && attached_option) {
	const struct bu_opt_value_spec *attached_spec =
	    opt_effective_value_spec(attached_option, descs, specs);
	if (attached_spec && attached_spec->validate) {
	    const char **validation_argv = (const char **)bu_malloc(
		argc * sizeof(const char *), "attached option validation argv");
	    memcpy(validation_argv, argv, argc * sizeof(const char *));
	    validation_argv[cursor_arg] = attached_value;
	    result->option = attached_option;
	    result->option_name = !BU_STR_EMPTY(attached_option->longopt) ?
		attached_option->longopt : attached_option->shortopt;
	    ret = attached_spec->validate(attached_option, argc, validation_argv,
		cursor_arg, context, attached_spec->data, result);
	    bu_free(validation_argv, "attached option validation argv");
	}
    } else if (!ret && spec && spec->validate && result->option &&
	    (result->expected & BU_OPT_EXPECT_OPTION_ARG) &&
	    !(result->expected & BU_OPT_EXPECT_OPTION) && cursor_arg < argc &&
	    opt_desc_for_token(descs, argv[cursor_arg]) != result->option) {
		ret = spec->validate(result->option, argc, argv, cursor_arg, context,
		    spec->data, result);
    }
    if (ret)
	bu_opt_validate_result_clear(result);
    return ret;
}


size_t
bu_opt_desc_empty_builder(struct bu_opt_desc *descs, size_t capacity,
	void *UNUSED(storage))
{
    const struct bu_opt_desc empty[] = {BU_OPT_DESC_NULL};
    return bu_opt_desc_copy(descs, capacity, empty);
}


size_t
bu_opt_desc_copy(struct bu_opt_desc *output, size_t capacity,
	const struct bu_opt_desc *local)
{
    size_t count = opt_desc_count(local);

    if (!output)
	return count;
    if (!local || capacity < count + 1)
	return 0;
    memcpy(output, local, (count + 1) * sizeof(*output));
    return count;
}


struct bu_opt_desc *
bu_opt_desc_build(bu_opt_desc_builder_t builder, void *storage, size_t *count)
{
    struct bu_opt_desc *descs;
    size_t expected;
    size_t written;

    if (count)
	*count = 0;
    if (!builder)
	return NULL;
    expected = builder(NULL, 0, storage);
    if (expected == (size_t)-1)
	return NULL;
    descs = (struct bu_opt_desc *)bu_calloc(expected + 1, sizeof(*descs),
	"built bu_opt descriptors");
    written = builder(descs, expected + 1, storage);
    if (written != expected) {
	bu_free(descs, "built bu_opt descriptors");
	return NULL;
    }
    if (count)
	*count = expected;
    return descs;
}


int
bu_opt_parse_build(struct bu_vls *msgs, size_t argc, const char **argv,
	bu_opt_desc_builder_t builder, void *storage)
{
    struct bu_opt_desc *descs = bu_opt_desc_build(builder, storage, NULL);
    int ret;

    if (!descs)
	return -1;
    ret = bu_opt_parse(msgs, argc, argv, descs);
    bu_free(descs, "built bu_opt descriptors");
    return ret;
}


int
bu_opt_parse_build_with_policy(struct bu_vls *msgs, size_t argc,
	const char **argv, bu_opt_desc_builder_t builder, void *storage,
	bu_opt_parse_policy_t policy)
{
    struct bu_opt_desc *descs = bu_opt_desc_build(builder, storage, NULL);
    int ret;

    if (!descs)
	return -1;
    ret = bu_opt_parse_with_policy(msgs, argc, argv, descs, policy);
    bu_free(descs, "built bu_opt descriptors");
    return ret;
}


char *
bu_opt_describe_build(bu_opt_desc_builder_t builder,
	struct bu_opt_desc_opts *settings)
{
    struct bu_opt_desc *descs = bu_opt_desc_build(builder, NULL, NULL);
    char *description;

    if (!descs)
	return NULL;
    description = bu_opt_describe(descs, settings);
    bu_free(descs, "built bu_opt descriptors");
    return description;
}


char *
bu_opt_usage(const struct bu_opt_desc *descs, const char *invocation,
	const char *operands)
{
    struct bu_vls usage = BU_VLS_INIT_ZERO;

    if (!descs || BU_STR_EMPTY(invocation))
	return NULL;
    bu_vls_printf(&usage, "Usage: %s", invocation);
    if (opt_desc_count(descs))
	bu_vls_strcat(&usage, " [options]");
    if (!BU_STR_EMPTY(operands))
	bu_vls_printf(&usage, " %s", operands);
    bu_vls_putc(&usage, '\n');
    char *result = bu_vls_strdup(&usage);
    bu_vls_free(&usage);
    return result;
}


char *
bu_opt_help(const struct bu_opt_desc *descs, const char *invocation,
	const char *operands, const char *summary)
{
    struct bu_vls help = BU_VLS_INIT_ZERO;
    char *usage;
    char *options;

    if (!descs || BU_STR_EMPTY(invocation))
	return NULL;
    usage = bu_opt_usage(descs, invocation, operands);
    if (usage) {
	bu_vls_strcat(&help, usage);
	bu_free(usage, "bu_opt usage");
    }
    if (!BU_STR_EMPTY(summary))
	bu_vls_printf(&help, "\n%s\n", summary);
    options = bu_opt_describe(descs, NULL);
    if (options && options[0])
	bu_vls_printf(&help, "\nOptions:\n%s", options);
    if (options)
	bu_free(options, "bu_opt option help");
    char *result = bu_vls_strdup(&help);
    bu_vls_free(&help);
    return result;
}


char *
bu_opt_help_build(bu_opt_desc_builder_t builder, const char *invocation,
	const char *operands, const char *summary)
{
    struct bu_opt_desc *descs = bu_opt_desc_build(builder, NULL, NULL);
    char *help;

    if (!descs)
	return NULL;
    help = bu_opt_help(descs, invocation, operands, summary);
    bu_free(descs, "built bu_opt descriptors");
    return help;
}


int
bu_opt_validate_build(bu_opt_desc_builder_t builder,
	const struct bu_opt_value_spec *specs, size_t argc, const char **argv,
	size_t cursor_arg, void *context, struct bu_opt_validate_result *result)
{
    struct bu_opt_desc *descs;
    int ret;

    if (!result)
	return -1;
    bu_opt_validate_result_clear(result);
    descs = bu_opt_desc_build(builder, NULL, NULL);
    if (!descs)
	return -1;
    ret = bu_opt_desc_validate(descs, specs, argc, argv, cursor_arg, context,
	result);
    result->option = NULL;
    bu_free(descs, "built bu_opt descriptors");
    return ret;
}


int
bu_opt_int(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    long int l;
    int i;
    char *endptr = NULL;
    int *int_set = (int *)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_int");

    errno = 0;
    l = strtol(argv[0], &endptr, 0);

    if (endptr != NULL && strlen(endptr) > 0) {
	/* Had some invalid character in the input, fail */
	if (msg)
	    bu_vls_printf(msg, "Invalid string specifier for int: %s\n", argv[0]);
	return -1;
    }

    if (errno == ERANGE) {
	if (msg)
	    bu_vls_printf(msg, "Invalid input for int (range error): %s\n", argv[0]);
	return -1;
    }

    /* If the long fits inside an int, we're OK */
    if (l <= INT_MAX && l >= INT_MIN) {
	i = (int)l;
    } else {
	/* Too big or too small, fail */
	if (msg)
	    bu_vls_printf(msg, "String specifies number too large for int data type: %ld\n", l);
	return -1;
    }

    if (int_set)
	*int_set = i;
    return 1;
}


int
bu_opt_long(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    long int l;
    char *endptr = NULL;
    long *long_set = (long *)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_long");

    errno = 0;
    l = strtol(argv[0], &endptr, 0);

    if (endptr != NULL && strlen(endptr) > 0) {
	/* Had some invalid character in the input, fail */
	if (msg)
	    bu_vls_printf(msg, "Invalid string specifier for long: %s\n", argv[0]);
	return -1;
    }

    if (errno == ERANGE) {
	if (msg)
	    bu_vls_printf(msg, "Invalid input for long (range error): %s\n", argv[0]);
	return -1;
    }

    if (long_set)
	*long_set = l;
    return 1;
}


int
bu_opt_long_hex(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    long int l;
    char *endptr = NULL;
    long *long_set = (long *)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_long");

    errno = 0;
    l = strtol(argv[0], &endptr, 16);

    if (endptr != NULL && strlen(endptr) > 0) {
	/* Had some invalid character in the input, fail */
	if (msg) {
	    bu_vls_printf(msg, "Invalid string specifier for long: %s\n", argv[0]);
	}
	return -1;
    }

    if (errno == ERANGE) {
	if (msg) {
	    bu_vls_printf(msg, "Invalid input for long (range error): %s\n", argv[0]);
	}
	return -1;
    }

    if (long_set) (*long_set) = l;
    return 1;
}


int
bu_opt_fastf_t(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    double d;
    fastf_t f;
    fastf_t *f_set = (fastf_t *)set_var;
    char *endptr = NULL;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_fastf_t");

    errno = 0;
    d = strtod(argv[0], &endptr);

    if (endptr != NULL && strlen(endptr) > 0) {
	/* Had some invalid character in the input, fail */
	if (msg) {
	    bu_vls_printf(msg, "Invalid string specifier for fastf_t: %s\n", argv[0]);
	}
	return -1;
    }

    if (errno == ERANGE) {
	if (msg) {
	    bu_vls_printf(msg, "Invalid input for fastf_t (range error): %s\n", argv[0]);
	}
	return -1;
    }

    if (!isfinite(d) ||
	(sizeof(fastf_t) == sizeof(float) && (d > FLT_MAX || d < -FLT_MAX))) {
	if (msg) {
	    bu_vls_printf(msg, "Invalid input for fastf_t (range error): %s\n", argv[0]);
	}
	return -1;
    }

    f = (fastf_t)d;

    if (f_set)
	*f_set = f;
    return 1;
}

int
bu_opt_char(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    char *c_set = (char *)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_str");

    if (c_set)
	*c_set = argv[0][0];

    return 1;
}

int
bu_opt_str(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    const char **s_set = (const char **)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_str");

    if (s_set)
	*s_set = argv[0];
    return 1;
}


int
bu_opt_vls(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    struct bu_vls *s_set = (struct bu_vls *)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_vls");

    if (s_set) {
	if (bu_vls_strlen(s_set) > 0) {
	    bu_vls_printf(s_set, " %s", argv[0]);
	} else {
	    bu_vls_printf(s_set, "%s", argv[0]);
	}
    }
    return 1;
}


int
bu_opt_bool(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    int *b_set = (int *)set_var;
    int bool_val;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_bool");

    bool_val = bu_str_true(argv[0]);

    if (bool_val != 0 && bool_val != 1) {
	if (msg) {
	    bu_vls_printf(msg, "Invalid input for boolean type: %s\n", argv[0]);
	}
	return -1;
    }

    if (b_set) (*b_set) = bool_val;
    return 1;
}


int
bu_opt_color(struct bu_vls *msg, size_t argc, const char **argv, void *set_c)
{
    struct bu_color *set_color = (struct bu_color *)set_c;
    unsigned char rgb[3] = {0, 0, 0};
    int consumed = 0;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_color");

    /* Integer RGB is common enough to have one strict parser.  Preserve the
     * older packed float and hexadecimal forms as compatibility fallbacks. */
    consumed = bu_rgb_from_argv(rgb, argc, argv);
    if (!consumed && bu_str_to_rgb(argv[0], rgb))
	consumed = 1;
    if (!consumed && argc >= 3) {
	struct bu_vls tmp_color = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&tmp_color, "%s/%s/%s", argv[0], argv[1], argv[2]);
	if (bu_str_to_rgb(bu_vls_addr(&tmp_color), rgb))
	    consumed = 3;
	bu_vls_free(&tmp_color);
    }

    if (!consumed) {
	if (msg)
	    bu_vls_sprintf(msg, "No valid color found: %s\n", argv[0]);
	return -1;
    }
    if (set_color)
	(void)bu_color_from_rgb_chars(set_color, rgb);
    return consumed;
}


int
bu_opt_vect_t(struct bu_vls *msg, size_t argc, const char **argv, void *vec)
{
    vect_t parsed = VINIT_ZERO;
    int consumed;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_vect_t");
    consumed = bu_cmd_vector3_from_argv(parsed, argc,
	(const char * const *)argv);
    if (!consumed) {
	if (msg)
	    bu_vls_sprintf(msg, "No valid vector found: %s\n", argv[0]);
	return -1;
    }
    if (vec)
	VMOVE(*(vect_t *)vec, parsed);
    return consumed;
}

int
bu_opt_incr_long(struct bu_vls *msg, size_t UNUSED(argc), const char **UNUSED(argv), void *set_var)
{
    long *long_incr = (long *)set_var;
    if (long_incr) {
	(*long_incr) = (*long_incr) + 1;
    } else {
	if (msg) {
	    bu_vls_sprintf(msg, "No valid supplied to bu_opt_incr_long\n");
	}
    }
    return 0;
}

int
bu_opt_lang(struct bu_vls *msg, size_t argc, const char **argv, void *l)
{
    /*
     * Checks that a string matches the two lower case letter form of ISO 639-1
     * language codes.  List pulled from:
     *
     * http://www.loc.gov/standards/iso639-2/php/English_list.php
     */
    const char *iso639_1[] = {"ab", "aa", "af", "ak", "sq", "am", "ar", "an",
	"hy", "as", "av", "ae", "ay", "az", "bm", "ba", "eu", "be", "bn", "bh",
	"bi", "nb", "bs", "br", "bg", "my", "es", "ca", "km", "ch", "ce", "ny",
	"ny", "zh", "za", "cu", "cu", "cv", "kw", "co", "cr", "hr", "cs", "da",
	"dv", "dv", "nl", "dz", "en", "eo", "et", "ee", "fo", "fj", "fi", "nl",
	"fr", "ff", "gd", "gl", "lg", "ka", "de", "ki", "el", "kl", "gn", "gu",
	"ht", "ht", "ha", "he", "hz", "hi", "ho", "hu", "is", "io", "ig", "id",
	"ia", "ie", "iu", "ik", "ga", "it", "ja", "jv", "kl", "kn", "kr", "ks",
	"kk", "ki", "rw", "ky", "kv", "kg", "ko", "kj", "ku", "kj", "ky", "lo",
	"la", "lv", "lb", "li", "li", "li", "ln", "lt", "lu", "lb", "mk", "mg",
	"ms", "ml", "dv", "mt", "gv", "mi", "mr", "mh", "ro", "ro", "mn", "na",
	"nv", "nv", "nd", "nr", "ng", "ne", "nd", "se", "no", "nb", "nn", "ii",
	"ny", "nn", "ie", "oc", "oj", "cu", "cu", "cu", "or", "om", "os", "os",
	"pi", "pa", "ps", "fa", "pl", "pt", "pa", "ps", "qu", "ro", "rm", "rn",
	"ru", "sm", "sg", "sa", "sc", "gd", "sr", "sn", "ii", "sd", "si", "si",
	"sk", "sl", "so", "st", "nr", "es", "su", "sw", "ss", "sv", "tl", "ty",
	"tg", "ta", "tt", "te", "th", "bo", "ti", "to", "ts", "tn", "tr", "tk",
	"tw", "ug", "uk", "ur", "ug", "uz", "ca", "ve", "vi", "vo", "wa", "cy",
	"fy", "wo", "xh", "yi", "yo", "za", "zu", NULL};


    size_t i = 0;
    struct bu_vls *lang = (struct bu_vls *)l;
    if (lang) {
	int ret = bu_opt_vls(msg, argc, argv, (void *)l);
	if (ret == -1)
	    return -1;
	if (bu_vls_strlen(lang) != 2)
	    return -1;
	/* Only return valid if we've got one of the ISO639-1 lang codes */
	while (iso639_1[i]) {
	    if (BU_STR_EQUAL(bu_vls_addr(lang), iso639_1[i]))
		return ret;
	    i++;
	}
	return -1;
    } else {
	return -1;
    }
}

int
bu_opt_man_section(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    size_t i = 0;
    char *s_set = (char *)set_var;
    const char sections[] = BRLCAD_MAN_SECTIONS;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_str");

    /* One char only */
    if (strlen(argv[0]) != 1)
	return -1;

    while(sections[i]) {
	if (sections[i] == argv[0][0]) {
	    if (s_set)
		(*s_set) = argv[0][0];
	    return 1;
	}
	i++;
    }

    return -1;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
