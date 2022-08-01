/*                        O P T . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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
#include <ctype.h> /* for isspace */
#include <errno.h> /* for errno */

#include "vmath.h"
#include "bu/color.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"


HIDDEN void
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


HIDDEN int
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


HIDDEN int
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


HIDDEN char *
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

HIDDEN int
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


HIDDEN void
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


HIDDEN void
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


HIDDEN char *
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


HIDDEN int
opt_is_flag(const char *opt, const struct bu_opt_desc *ds, const char *arg)
{
    int arg_offset = -1;
    /* Find the corresponding desc, if we have one */
    int desc_ind = 0;
    const struct bu_opt_desc *desc = &(ds[desc_ind]);
    while (desc && !opt_desc_is_null(desc)) {
	if (opt[0] == desc->shortopt[0]) {
	    if (!desc->arg_process) {
		return 1;
	    }
	    break;
	}
	desc_ind++;
	desc = &(ds[desc_ind]);
    }

    /* If there is an arg_process, it's up to the function - if an
     * option without args is valid, and the potential arg after the
     * option isn't a valid arg for this opt, opt can be a flag
     */
    if (desc && desc->arg_process) {
	if (arg) {
	    arg_offset = (*desc->arg_process)(NULL, 1, &arg, NULL);
	    if (!arg_offset) {
		return 1;
	    }
	} else {
	    arg_offset = (*desc->arg_process)(NULL, 0, NULL, NULL);
	    if (!arg_offset) {
		return 1;
	    }
	}
    }

    return 0;
}


HIDDEN int
opt_process(struct bu_ptbl *opts, const char **eq_arg, const char *opt_candidate, const struct bu_opt_desc *ds)
{
    size_t offset = 1;
    const char *opt;
    char *optcpy;
    const char *equal_pos;

    if (!eq_arg && !opt_candidate)
	return 0;
    if (opt_candidate[1] == '-')
	offset++;
    equal_pos = strchr(opt_candidate, '=');

    /* If we've got a single opt, things are handled differently */
    if (offset == 1) {
	if (strlen(opt_candidate+offset) == 1) {
	    optcpy = (char *)bu_calloc(2, sizeof(char), "option");
	    optcpy[0] = (opt_candidate+offset)[0];
	    bu_ptbl_ins(opts, (long *)optcpy);
	    return 1;
	} else {
	    /* single letter opt, but the string is longer. If and
	     * only if this single letter opt is a flag, check for
	     * more flags. Anything that needs an argument in this
	     * context is considered an error, and with a flag option
	     * anything in the same argv with it that is not also a
	     * flag constitutes an error.
	     */
	    opt = opt_candidate+offset;
	    if (opt_is_flag(opt, ds, opt_candidate+offset+1)) {
		optcpy = (char *)bu_calloc(2, sizeof(char), "option");
		optcpy[0] = (opt)[0];
		bu_ptbl_ins(opts, (long *)optcpy);
		opt++;
		while (strlen(opt) > 0) {
		    if (opt_is_flag(opt, ds, NULL)) {
			optcpy = (char *)bu_calloc(2, sizeof(char), "option");
			optcpy[0] = (opt)[0];
			bu_ptbl_ins(opts, (long *)optcpy);
		    } else {
			/* In a flag opt context but hit a non-flag - error. */
			return -1;
		    }
		    opt++;
		}
	    } else {
		/* the interpretation in this context is everything
		 * after the first letter is arg.
		 */
		struct bu_vls vopt = BU_VLS_INIT_ZERO;
		const char *varg = opt_candidate;
		bu_vls_strncat(&vopt, opt_candidate+1, 1);

		varg = opt_candidate + 2;

		/* A exception is an equals sign, e.g. -s=1024 - in
		 * that instance, the expectation might be that =
		 * would be interpreted as an assignment.  This means
		 * that to get the literal =1024 as an option, you
		 * would need a space after the s, e.g.: -s =1024
		 *
		 * For now, commented out to favor consistent behavior
		 * over what "looks right" - may be worth revisiting
		 * or even an option at some point...
		 */

		if (equal_pos)
		    varg++;

		BU_ASSERT(eq_arg != NULL);

		(*eq_arg) = varg;
		opt = bu_strdup(bu_vls_addr(&vopt));
		bu_ptbl_ins(opts, (long *)opt);
		bu_vls_free(&vopt);
	    }
	}
    } else {
	if (equal_pos) {
	    struct bu_vls vopt = BU_VLS_INIT_ZERO;
	    const char *varg = opt_candidate;
	    bu_vls_sprintf(&vopt, "%s", opt_candidate);
	    bu_vls_trunc(&vopt, -1 * (int)strlen(equal_pos));
	    bu_vls_nibble(&vopt, offset);

	    varg = opt_candidate + bu_vls_strlen(&vopt) + 2;
	    if (equal_pos)
		varg++;

	    (*eq_arg) = varg;
	    opt = bu_strdup(bu_vls_addr(&vopt));
	    bu_ptbl_ins(opts, (long *)opt);
	    bu_vls_free(&vopt);
	} else {
	    opt = bu_strdup(opt_candidate+offset);
	    bu_ptbl_ins(opts, (long *)opt);
	}
    }
    return (int)BU_PTBL_LEN(opts);
}


/* This implements naive criteria for deciding when an argv string is
 * an option.  Right now the criteria are:
 *
 * 1.  Must have a '-' char as first character
 * 2.  Must not be ONLY the '-' char
 * 3.  Must not have white space characters present in the string.
 */
HIDDEN int
can_be_opt(const char *opt)
{
    size_t i = 0;
    if (!opt)
	return 0;
    if (!strlen(opt))
	return 0;
    if (opt[0] != '-')
	return 0;
    if (BU_STR_EQUAL(opt, "-"))
	return 0;
    for (i = 1; i < strlen(opt); i++) {
	if (isspace(opt[i]))
	    return 0;
    }
    return 1;
}


int
bu_opt_parse(struct bu_vls *msgs, size_t argc, const char **argv, const struct bu_opt_desc *ds)
{
    size_t i = 0;
    size_t j = 0;
    int ret_argc = 0;
    struct bu_ptbl known_args = BU_PTBL_INIT_ZERO;
    struct bu_ptbl unknown_args = BU_PTBL_INIT_ZERO;

    if (!argv || !ds)
	return -1;

    /* Now identify opt/arg pairs.*/
    while (i < (size_t)argc) {
	int desc_found = 0;
	int desc_ind = 0;
	int opt_cnt = 0;
	const char *eq_arg = NULL;
	const struct bu_opt_desc *desc = NULL;
	struct bu_ptbl opts = BU_PTBL_INIT_ZERO;

	/* If argv[i] isn't an option, stick the argv entry (and any
	 * immediately following non-option entries) into the unknown
	 * args table
	 */
	if (!can_be_opt(argv[i])) {
	    bu_ptbl_ins(&unknown_args, (long *)argv[i]);
	    i++;
	    while (i < argc && !can_be_opt(argv[i])) {
		bu_ptbl_ins(&unknown_args, (long *)argv[i]);
		i++;
	    }
	    continue;
	}

	/* Now we're past the easy case, and whether something is an
	 * option or an argument depends on context. argv[i] is at
	 * least a possibility for a valid option, so the first order
	 * of business is to determine if it is one.
	 */

	/* It may be that an = has been used instead of a space.
	 * Handle that, and strip leading '-' characters.  Short-opt
	 * options may be grouped onto one shared dash (e.g. rm -rf),
	 * so opt_process can return more than one option. Also
	 * handled here, short-opt options may not have a space
	 * between their option and the argument.
	 */
	opt_cnt = opt_process(&opts, &eq_arg, argv[i], ds);
	if (opt_cnt == -1) {
	    for(j = 0; j < BU_PTBL_LEN(&opts); j++) {
		char *o = (char *)BU_PTBL_GET(&opts, j);
		bu_free(o, "free arg cpy");
	    }
	    bu_ptbl_free(&unknown_args);
	    bu_ptbl_free(&known_args);
	    bu_ptbl_free(&opts);
	    return -1;

	} else if (opt_cnt == 0) {
	    /* skip, fall through */
	    i++;

	} else if (opt_cnt > 1) {

	    for (j = 0; j < (size_t)opt_cnt; j++) {
		int* flag_var;
		char* opt = (char*)BU_PTBL_GET(&opts, j);
		/* Find the corresponding desc - if we're in a
		 * multiple flag processing situation, we've already
		 * verified that each entry has a desc.
		 */
		desc_ind = 0;
		desc = &(ds[0]);
		while (desc && !opt_desc_is_null(desc)) {
		    if (opt[0] == desc->shortopt[0]) {
			break;
		    }
		    desc_ind++;
		    desc = &(ds[desc_ind]);
		}
		/* this is a flag - if we don't have an arg processing
		 * function, try to set an int
		 */
		if (desc->arg_process) {
		    (void)(*desc->arg_process)(msgs, 0, NULL, desc->set_var);
		}
		else {
		    flag_var = (int*)desc->set_var;
		    if (flag_var) {
			*flag_var = 1;
		    }
		}
	    }
	    /* record the option in known args */
	    bu_ptbl_ins(&known_args, (long*)argv[i]);
	    i++;

	} else {
	    /* should be just one option */
	    char* opt = NULL;
	    if (BU_PTBL_LEN(&opts)) {
		opt = (char*)BU_PTBL_GET(&opts, 0);
	    }

	    /* Find the corresponding desc, if we have one */
	    desc = &(ds[0]);
	    desc_ind = 0;
	    desc_found = 0;
	    while (!desc_found && (desc && !opt_desc_is_null(desc))) {
		if (BU_STR_EQUAL(opt, desc->shortopt) || BU_STR_EQUAL(opt, desc->longopt)) {
		    desc_found = 1;
		    continue;
		}
		desc_ind++;
		desc = &(ds[desc_ind]);
	    }

	    /* If we don't know what we're dealing with, keep going */
	    if (!desc_found) {
		/* Since the equals sign is regarded as forcing an
		 * argument to map to a particular option (and is an
		 * error if that option isn't supposed to have
		 * arguments) we pass along the original option
		 * intact. */
		bu_ptbl_ins(&unknown_args, (long *)argv[i]);
		i++;

		/* Do the opts cleanup that would otherwise be done at the end */
		for(j = 0; j < BU_PTBL_LEN(&opts); j++) {
		    char *o = (char *)BU_PTBL_GET(&opts, j);
		    bu_free(o, "free arg cpy");
		}
		bu_ptbl_free(&opts);

		continue;
	    }

	    /* record the option in known args - any remaining
	     * processing is on args, if any
	     */
	    bu_ptbl_ins(&known_args, (long *)argv[i]);

	    /* any remaining processing is on trailing args, if any */
	    i = i + 1;

	    /* If we might have args and we have a validator function,
	     * construct the greediest possible interpretation of the
	     * option description and run the validator to determine
	     * the number of argv entries associated with this option
	     * (can_be_opt is not enough if the option is number
	     * based, since -9 may be both a valid option and a valid
	     * argument - the validator must make the decision.  If we
	     * do not have a validator, the best we can do is the
	     * can_be_opt test as a terminating trigger.
	     */
	    if (desc->arg_process) {
		/* Construct the greedy interpretation of the option argv */
		int k = 0;
		int arg_offset = 0;
		size_t g_argc = argc - i;
		const char *prev_opt = argv[i-1];
		const char **g_argv = argv + i;
		/* If we have an arg hiding in the previous option,
		 * temporarily rework the argv array for this purpose
		 */
		if (eq_arg) {
		    g_argv--;
		    g_argv[0] = eq_arg;
		    g_argc++;
		}
		arg_offset = (*desc->arg_process)(msgs, g_argc, g_argv, desc->set_var);
		if (arg_offset == -1) {
		    /* This isn't just an unknown option to be passed
		     * through for possible later processing.  If the
		     * arg_process callback returns -1, something has
		     * gone seriously awry and a known-to-be-invalid
		     * arg was seen.  Fail early and hard.
		     */
		    if (msgs) {
			bu_vls_printf(msgs, "Invalid argument supplied to %s: %s - halting.\n", argv[i-1], argv[i]);
		    }
		    bu_ptbl_free(&unknown_args);
		    bu_ptbl_free(&known_args);

		    for(j = 0; j < BU_PTBL_LEN(&opts); j++) {
			char *o = (char *)BU_PTBL_GET(&opts, j);
			bu_free(o, "free arg cpy");
		    }
		    bu_ptbl_free(&opts);
		    return -1;
		}
		/* Put the original opt back and adjust the
		 * arg_offset, if we substituted the eq_arg pointer
		 * into the argv array
		 */
		if (eq_arg) {
		    /* If the arg_process callback did nothing with
		     * the arg, but the arg was sent to this option
		     * with an = assignment, something is wrong - the
		     * most likely scenario is an = assignment forced
		     * an argument to be sent to an option that
		     * doesn't take arguments.
		     */
		    if (!arg_offset) {
			if (msgs) {
			    bu_vls_printf(msgs, "Option %s did not successfully use the supplied argument %s - halting.\n", argv[i-1], eq_arg);
			}
			bu_ptbl_free(&unknown_args);
			bu_ptbl_free(&known_args);
			for(j = 0; j < BU_PTBL_LEN(&opts); j++) {
			    char *o = (char *)BU_PTBL_GET(&opts, j);
			    bu_free(o, "free arg cpy");
			}
			bu_ptbl_free(&opts);
			return -1;
		    }

		    g_argv[0] = prev_opt;
		    if (arg_offset > 0) {
			arg_offset--;
		    }
		}
		/* If we used any of the argv entries, accumulate them
		 * for later reordering and increment i */
		for (k = (int)i; k < (int)(i + arg_offset); k++) {
		    bu_ptbl_ins(&known_args, (long *)argv[k]);
		}
		i = i + arg_offset;
	    } else {
		/* no arg_process means this is a flag - try to set an int */
		int *flag_var = (int *)desc->set_var;
		if (flag_var)
		    *flag_var = 1;

		/* If we already got an arg from the equals mechanism
		 * and we aren't supposed to have one, we're invalid -
		 * halt.
		 */
		if (eq_arg) {
		    if (msgs) {
			bu_vls_printf(msgs, "Option %s does not take an argument, but %s was supplied - halting.\n", argv[i-1], eq_arg);
		    }
		    bu_ptbl_free(&unknown_args);
		    bu_ptbl_free(&known_args);
		    for(j = 0; j < BU_PTBL_LEN(&opts); j++) {
			char *o = (char *)BU_PTBL_GET(&opts, j);
			bu_free(o, "free arg cpy");
		    }
		    bu_ptbl_free(&opts);
		    return -1;
		}
	    }
	}

	for(j = 0; j < BU_PTBL_LEN(&opts); j++) {
	    char *o = (char *)BU_PTBL_GET(&opts, j);
	    bu_free(o, "free arg cpy");
	}
	bu_ptbl_free(&opts);
    }

    /* Rearrange argv so the unused options are ordered at the front
     * of the array.
     */
    ret_argc = (int)BU_PTBL_LEN(&unknown_args);
    if (ret_argc > 0) {
	size_t avc = 0;
	size_t akc = BU_PTBL_LEN(&known_args);
	for (avc = 0; avc < (size_t)ret_argc; avc++) {
	    argv[avc] = (const char *)BU_PTBL_GET(&unknown_args, avc);
	}
	/* Put the known option argv pointers at the end of the array,
	 * in case they are still needed for memory freeing by the
	 * caller
	 */
	for (avc = 0; avc < akc; avc++) {
	    argv[avc+ret_argc] = (const char *)BU_PTBL_GET(&known_args, avc);
	}
    }
    bu_ptbl_free(&unknown_args);
    bu_ptbl_free(&known_args);

    return ret_argc;
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
    unsigned int rgb[3];

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_color");

    /* First, see if the first string converts to rgb */
    if (!bu_str_to_rgb((char *)argv[0], (unsigned char *)&rgb)) {
	/* nope - maybe we have 3 args? */
	if (argc >= 3) {
	    struct bu_vls tmp_color = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&tmp_color, "%s/%s/%s", argv[0], argv[1], argv[2]);
	    if (!bu_str_to_rgb(bu_vls_addr(&tmp_color), (unsigned char *)&rgb)) {
		/* Not valid with 3 */
		bu_vls_free(&tmp_color);
		if (msg)
		    bu_vls_sprintf(msg, "No valid color found.\n");
		return -1;
	    } else {
		/* 3 did the job */
		bu_vls_free(&tmp_color);
		if (set_color)
		    (void)bu_color_from_rgb_chars(set_color, (unsigned char *)&rgb);
		return 3;
	    }
	} else {
	    /* Not valid with 1 and don't have 3 - we require at least
	     * one, so claim one argv as belonging to this option
	     * regardless.
	     */
	    if (msg)
		bu_vls_sprintf(msg, "No valid color found: %s\n", argv[0]);
	    return -1;
	}
    } else {
	/* yep, 1 did the job */
	if (set_color)
	    (void)bu_color_from_rgb_chars(set_color, (unsigned char *)&rgb);
	return 1;
    }

    return -1;
}


int
bu_opt_vect_t(struct bu_vls *msg, size_t argc, const char **argv, void *vec)
{
    size_t i = 0;
    size_t acnum = 0;
    char *str1 = NULL;
    char *avnum[5] = {NULL, NULL, NULL, NULL, NULL};
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
    acnum = bu_argv_from_string(avnum, 4, str1);
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
