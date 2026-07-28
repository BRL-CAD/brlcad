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
bu_opt_parse(struct bu_vls *msgs, size_t argc, const char **argv,
	const struct bu_opt_desc *ds)
{
    struct bu_cmd_schema schema;
    struct bu_cmd_option *options;
    struct bu_cmd_parse_binding *bindings;
    size_t count = 0;
    int ret;

    if (!argv || !ds)
	return -1;
    while (!opt_desc_is_null(&ds[count]))
	count++;

    options = (struct bu_cmd_option *)bu_calloc(count + 1,
	sizeof(*options), "bu_opt schema adapter options");
    bindings = (struct bu_cmd_parse_binding *)bu_calloc(count + 1,
	sizeof(*bindings), "bu_opt schema adapter bindings");

    for (size_t i = 0; i < count; i++) {
	options[i].shortopt = ds[i].shortopt;
	options[i].longopt = ds[i].longopt;
	options[i].canonical = !BU_STR_EMPTY(ds[i].longopt) ?
	    ds[i].longopt : ds[i].shortopt;
	options[i].argument = ds[i].arg_helpstr;
	options[i].help = ds[i].help_string;
	options[i].value_type = ds[i].arg_process ?
	    BU_CMD_VALUE_CUSTOM : BU_CMD_VALUE_FLAG;
	options[i].storage_offset = BU_CMD_STORAGE_NONE;
	options[i].arg_requirement = ds[i].arg_process ?
	    BU_CMD_ARG_REQUIRED : BU_CMD_ARG_NONE;
	bindings[i].storage = ds[i].set_var;
	bindings[i].legacy_process =
	    (bu_cmd_legacy_process_t)ds[i].arg_process;
    }

    schema.name = "bu_opt";
    schema.help = NULL;
    schema.options = options;
    schema.operands = NULL;
    schema.parse_policy = BU_CMD_PARSE_INTERSPERSED;
    schema.validation.custom_validate = NULL;
    schema.validation.constraints = NULL;
    schema.validation.context_validate = NULL;
    schema.operand_groups = NULL;

    ret = _bu_cmd_schema_parse_bound(&schema, NULL, msgs, (int)argc, argv,
	bindings, BU_CMD_PARSE_INTERNAL_PASS_UNKNOWN |
	BU_CMD_PARSE_INTERNAL_LEFTOVERS_FIRST |
	BU_CMD_PARSE_INTERNAL_LEGACY_SYNTAX);

    bu_free(bindings, "bu_opt schema adapter bindings");
    bu_free(options, "bu_opt schema adapter options");
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
    if (l <= INT_MAX && l >= -INT_MAX) {
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

    if (sizeof(fastf_t) == sizeof(float) && (d > FLT_MAX)) {
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

    if (c_set && strlen(argv[0]) > 0)
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
    size_t i = 0;
    size_t acnum = 0;
    char *str1 = NULL;
    char *avnum[4] = {NULL, NULL, NULL, NULL};
    vect_t *v= (vect_t *)vec;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_vect_t");

    /* First, see if the first string converts to a vect_t (should
     * this be a func?)
     */
    str1 = bu_strdup(argv[0]);
    while (str1[i]) {
	/* If we have a separator, replace with a space */
	if (str1[i] == ',' || str1[i] == '/') {
	    str1[i] = ' ';
	}
	i++;
    }
    acnum = bu_argv_from_string(avnum, 3, str1);
    if (acnum == 3) {
	/* We might have three numbers - find out */
	fastf_t v1 = 0.0;
	fastf_t v2 = 0.0;
	fastf_t v3 = 0.0;
	int have_three = 1;
	if (bu_opt_fastf_t(msg, 1, (const char **)&avnum[0], &v1) == -1) {
	    if (msg) {
		bu_vls_sprintf(msg, "Not a number: %s.\n", avnum[0]);
	    }
	    have_three = 0;
	}
	if (bu_opt_fastf_t(msg, 1, (const char **)&avnum[1], &v2) == -1) {
	    if (msg) {
		bu_vls_sprintf(msg, "Not a number: %s.\n", avnum[1]);
	    }
	    have_three = 0;
	}
	if (bu_opt_fastf_t(msg, 1, (const char **)&avnum[2], &v3) == -1) {
	    if (msg) {
		bu_vls_sprintf(msg, "Not a number: %s.\n", avnum[2]);
	    }
	    have_three = 0;
	}
	bu_free(str1, "free tmp str");
	/* If we got here, we do have three numbers */
	if (have_three) {
	    if (v) {
		VSET(*v, v1, v2, v3);
	    }
	    return 1;
	}
    } else {
	/* Can't be just the first arg */
	bu_free(str1, "free tmp str");
    }
    /* First string didn't have three numbers - maybe we have 3 args ? */
    if (argc >= 3) {
	fastf_t v1 = 0.0;
	fastf_t v2 = 0.0;
	fastf_t v3 = 0.0;
	if (bu_opt_fastf_t(msg, 1, &argv[0], &v1) == -1) {
	    if (msg) {
		bu_vls_sprintf(msg, "Not a number: %s.\n", argv[0]);
	    }
	    return -1;
	}
	if (bu_opt_fastf_t(msg, 1, &argv[1], &v2) == -1) {
	    if (msg) {
		bu_vls_sprintf(msg, "Not a number: %s.\n", argv[1]);
	    }
	    return -1;
	}
	if (bu_opt_fastf_t(msg, 1, &argv[2], &v3) == -1) {
	    if (msg) {
		bu_vls_sprintf(msg, "Not a number: %s.\n", argv[2]);
	    }
	    return -1;
	}
	/* If we got here, 3 did the job */
	if (v) {
	    VSET(*v, v1, v2, v3);
	}
	return 3;
    } else {
	/* Not valid with 1 and don't have 3 - we require at least
	 * one, so claim one argv as belonging to this option
	 * regardless.
	 */
	if (msg) {
	    bu_vls_sprintf(msg, "No valid vector found: %s\n", argv[0]);
	}
	return -1;
    }

    return -1;
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
