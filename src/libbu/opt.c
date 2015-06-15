/*                        O P T . C
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
#include <ctype.h> /* for isspace */
#include <errno.h> /* for errno */

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"

HIDDEN void
wrap_help(struct bu_vls *help, int indent, int offset, int len)
{
    int i = 0;
    char *input = NULL;
    char **argv = NULL;
    int argc = 0;
    struct bu_vls new_help = BU_VLS_INIT_ZERO;
    struct bu_vls working = BU_VLS_INIT_ZERO;
    bu_vls_trunc(&working, 0);
    bu_vls_trunc(&new_help, 0);

    input = bu_strdup(bu_vls_addr(help));
    argv = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
    argc = bu_argv_from_string(argv, strlen(input), input);

    for (i = 0; i < argc; i++) {
	int avl = strlen(argv[i]);
	if ((int)bu_vls_strlen(&working) + avl + 1 > len) {
	    bu_vls_printf(&new_help, "%s\n%*s", bu_vls_addr(&working), offset+indent, " ");
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
opt_desc_is_null(struct bu_opt_desc *ds)
{
    int non_null = 0;
    if (!ds) return 1;

    if (ds->shortopt) non_null++;
    if (ds->longopt) non_null++;
    if (ds->arg_process) non_null++;
    if (ds->arg_helpstr) non_null++;
    if (ds->help_string) non_null++;
    if (ds->set_var) non_null++;

    return (non_null > 0) ? 0 : 1;
}

HIDDEN const char *
bu_opt_describe_internal_ascii(struct bu_opt_desc *ds, struct bu_opt_desc_opts *settings)
{
    size_t i = 0;
    size_t j = 0;
    size_t opt_cnt = 0;
    int offset = 2;
    int opt_cols = 28;
    int desc_cols = 50;
    /*
    bu_opt_desc_t desc_type = BU_OPT_FULL;
    bu_opt_format_t format_type = BU_OPT_ASCII;
    */
    const char *finalized;
    struct bu_vls description = BU_VLS_INIT_ZERO;
    int *status;
    if (!ds || opt_desc_is_null(&ds[0])) return NULL;

    if (settings) {
	offset = settings->offset;
	opt_cols = settings->option_columns;
	desc_cols = settings->description_columns;
    }

    while (!opt_desc_is_null(&ds[i])) i++;
    if (i == 0) return NULL;
    opt_cnt = i;
    status = (int *)bu_calloc(opt_cnt, sizeof(int), "opt status");
    i = 0;
    while (i < opt_cnt) {
	struct bu_opt_desc *curr;
	struct bu_opt_desc *d;
	curr = &(ds[i]);
	if (!status[i]) {
	    struct bu_vls opts = BU_VLS_INIT_ZERO;
	    struct bu_vls help_str = BU_VLS_INIT_ZERO;

	    /* We handle all entries with the same set_var in the same
	     * pass, so set the status flags accordingly */
	    j = i;
	    while (j < opt_cnt) {
		d = &(ds[j]);
		if (d == curr || (d->set_var && curr->set_var && d->set_var == curr->set_var)) {
		    status[j] = 1;
		}
		j++;
	    }

	    /* Collect the short options first - may be multiple instances with
	     * the same set_var, so accumulate all of them. */
	    j = i;
	    while (j < opt_cnt) {
		d = &(ds[j]);
		if (d == curr || (d->set_var && curr->set_var && d->set_var == curr->set_var)) {
		    if (d->shortopt && strlen(d->shortopt) > 0) {
			struct bu_vls tmp_arg = BU_VLS_INIT_ZERO;
			int new_len = strlen(d->arg_helpstr);
			if (!new_len) {
			    bu_vls_sprintf(&tmp_arg, "-%s", d->shortopt);
			    new_len = 2;
			} else {
			    bu_vls_sprintf(&tmp_arg, "-%s %s", d->shortopt, d->arg_helpstr);
			    new_len = new_len + 4;
			}
			if ((int)bu_vls_strlen(&opts) + new_len + offset + 2 > opt_cols + desc_cols) {
			    bu_vls_printf(&description, "%*s%s\n", offset, " ", bu_vls_addr(&opts));
			    bu_vls_sprintf(&opts, "%s, ", bu_vls_addr(&tmp_arg));
			} else {
			    bu_vls_printf(&opts, "%s, ", bu_vls_addr(&tmp_arg));
			}
			bu_vls_free(&tmp_arg);
		    }
		    /* While we're at it, pick up the string.  The last string with
		     * a matching key wins, as long as its not empty */
		    if (strlen(d->help_string) > 0) bu_vls_sprintf(&help_str, "%s", d->help_string);
		}
		j++;
	    }

	    /* Now do the long opts */
	    j = i;
	    while (j < opt_cnt) {
		d = &(ds[j]);
		if (d == curr || (d->set_var && curr->set_var && d->set_var == curr->set_var)) {
		    if (d->longopt && strlen(d->longopt) > 0) {
			struct bu_vls tmp_arg = BU_VLS_INIT_ZERO;
			int new_len = strlen(d->arg_helpstr);
			if (!new_len) {
			    bu_vls_sprintf(&tmp_arg, "--%s", d->longopt);
			    new_len = strlen(d->longopt) + 2;
			} else {
			    bu_vls_sprintf(&tmp_arg, "--%s %s", d->longopt, d->arg_helpstr);
			    new_len = strlen(d->longopt) + new_len + 3;
			}
			if ((int)bu_vls_strlen(&opts) + new_len + offset + 2 > opt_cols + desc_cols) {
			    bu_vls_printf(&description, "%*s%s\n", offset, " ", bu_vls_addr(&opts));
			    bu_vls_sprintf(&opts, "%s, ", bu_vls_addr(&tmp_arg));
			} else {
			    bu_vls_printf(&opts, "%s, ", bu_vls_addr(&tmp_arg));
			}
			bu_vls_free(&tmp_arg);
		    }
		}
		j++;
	    }

	    bu_vls_trunc(&opts, -2);
	    bu_vls_printf(&description, "%*s%s", offset, " ", bu_vls_addr(&opts));
	    if ((int)bu_vls_strlen(&opts) > opt_cols) {
		bu_vls_printf(&description, "\n%*s", opt_cols + offset, " ");
	    } else {
		bu_vls_printf(&description, "%*s", opt_cols - (int)bu_vls_strlen(&opts), " ");
	    }
	    if ((int)bu_vls_strlen(&help_str) > desc_cols) {
		wrap_help(&help_str, offset, opt_cols+offset, desc_cols);
	    }
	    bu_vls_printf(&description, "%*s%s\n", offset, " ", bu_vls_addr(&help_str));
	    bu_vls_free(&help_str);
	    bu_vls_free(&opts);
	    status[i] = 1;
	}
	i++;
    }
    finalized = bu_strdup(bu_vls_addr(&description));
    bu_vls_free(&description);
    return finalized;
}

const char *
bu_opt_describe(struct bu_opt_desc *ds, struct bu_opt_desc_opts *settings)
{
    if (!ds) return NULL;
    if (!settings) return bu_opt_describe_internal_ascii(ds, NULL);
    return NULL;
}

HIDDEN char *
opt_process(const char **eq_arg, const char *opt_candidate)
{
    int offset = 1;
    char *final_opt;
    char *equal_pos;
    if (!eq_arg && !opt_candidate) return NULL;
    if (opt_candidate[1] == '-') offset++;
    equal_pos = strchr(opt_candidate, '=');

    /* If we've got a single opt, things are handled differently */
    if (offset == 1) {
	if (strlen(opt_candidate+offset) == 1) {
	    final_opt = bu_strdup(opt_candidate+offset);
	} else {
	    /* single letter opt, but the string is longer - the
	     * interpretation in this context is everything after
	     * the first letter is arg.*/
	    struct bu_vls vopt = BU_VLS_INIT_ZERO;
	    const char *varg = opt_candidate;
	    bu_vls_strncat(&vopt, opt_candidate+1, 1);

	    varg = opt_candidate + 2;

	    /* A exception is an equals sign, e.g. -s=1024 - in that
	     * instance, the expectation might be that = would be interpreted
	     * as an assignment.  This means that to get the literal =1024 as
	     * an option, you would need a space after the s, e.g.:  -s =1024
	     *
	     * For now, commented out to favor consistent behavior over what
	     * "looks right" - may be worth revisiting or even an option at
	     * some point...*/

	    if (equal_pos) varg++;

	    (*eq_arg) = varg;
	    final_opt = bu_strdup(bu_vls_addr(&vopt));
	    bu_vls_free(&vopt);
	}
    } else {
	if (equal_pos) {
	    struct bu_vls vopt = BU_VLS_INIT_ZERO;
	    const char *varg = opt_candidate;
	    bu_vls_sprintf(&vopt, "%s", opt_candidate);
	    bu_vls_trunc(&vopt, -1 * strlen(equal_pos));
	    bu_vls_nibble(&vopt, offset);

	    varg = opt_candidate + bu_vls_strlen(&vopt) + 2;
	    if (equal_pos) varg++;

	    (*eq_arg) = varg;
	    final_opt = bu_strdup(bu_vls_addr(&vopt));
	    bu_vls_free(&vopt);
	} else {
	    final_opt = bu_strdup(opt_candidate+offset);
	}
    }
    return final_opt;
}

/* This implements naive criteria for deciding when an argv string is
 * an option.  Right now the criteria are:
 *
 * 1.  Must have a '-' char as first character
 * 2.  Must not have white space characters present in the string.
 */
HIDDEN int
can_be_opt(const char *opt) {
    size_t i = 0;
    if (!opt) return 0;
    if (!strlen(opt)) return 0;
    if (opt[0] != '-') return 0;
    for (i = 1; i < strlen(opt); i++) {
	if (isspace(opt[i])) return 0;
    }
    return 1;
}


int
bu_opt_parse(struct bu_vls *msgs, int argc, const char **argv, struct bu_opt_desc *ds)
{
    int i = 0;
    int offset = 0;
    int ret_argc = 0;
    struct bu_ptbl known_args = BU_PTBL_INIT_ZERO;
    struct bu_ptbl unknown_args = BU_PTBL_INIT_ZERO;
    if (!argv || !ds) return -1;

    /* Now identify opt/arg pairs.*/
    while (i < argc) {
	int desc_found = 0;
	int desc_ind = 0;
	char *opt = NULL;
	const char *eq_arg = NULL;
	struct bu_opt_desc *desc = NULL;
	/* If argv[i] isn't an option, stick the argv entry (and any
	 * immediately following non-option entries) into the
	 * unknown args table */
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
	 * option or an argument depends on context. argv[i] is at least
	 * a possibility for a valid option, so the first order of business
	 * is to determine if it is one. */

	/* It may be that an = has been used instead of a space.  Handle that, and
	 * strip leading '-' characters.  Also, short-opt options may not have a
	 * space between their option and the argument. That is also handled here */
	opt = opt_process(&eq_arg, argv[i]);

	/* Find the corresponding desc, if we have one */
	desc = &(ds[0]);
	while (!desc_found && (desc && !opt_desc_is_null(desc))) {
	    if (BU_STR_EQUAL(opt+offset, desc->shortopt) || BU_STR_EQUAL(opt+offset, desc->longopt)) {
		desc_found = 1;
		continue;
	    }
	    desc_ind++;
	    desc = &(ds[desc_ind]);
	}

	/* If we don't know what we're dealing with, keep going */
	if (!desc_found) {
	    /* Since the equals sign is regarded as forcing an argument
	     * to map to a particular option (and is an error if that
	     * option isn't supposed to have arguments) we pass along
	     * the original option intact. */
	    bu_ptbl_ins(&unknown_args, (long *)argv[i]);
	    i++;
	    continue;
	}

	/* record the option in known args - any remaining processing is on args, if any*/
	bu_ptbl_ins(&known_args, (long *)argv[i]);

	/* any remaining processing is on trailing args, if any */
	i = i + 1;

	/* If we might have args and we have a validator function,
	 * construct the greediest possible interpretation of the option
	 * description and run the validator to determine the number of
	 * argv entries associated with this option (can_be_opt is not
	 * enough if the option is number based, since -9 may be both a
	 * valid option and a valid argument - the validator must make the
	 * decision.  If we do not have a validator, the best we can do
	 * is the can_be_opt test as a terminating trigger. */
	if (desc->arg_process) {
	    /* Construct the greedy interpretation of the option argv */
	    int k = 0;
	    int arg_offset = 0;
	    int g_argc = argc - i;
	    const char *prev_opt = argv[i-1];
	    const char **g_argv = argv + i;
	    /* If we have an arg hiding in the previous option, temporarily
	     * rework the argv array for this purpose */
	    if (eq_arg) {
		g_argv--;
		g_argv[0] = eq_arg;
		g_argc++;
	    }
	    arg_offset = (*desc->arg_process)(msgs, g_argc, g_argv, desc->set_var);
	    if (arg_offset == -1) {
		/* This isn't just an unknown option to be passed
		 * through for possible later processing.  If the
		 * arg_process callback returns -1, something has gone
		 * seriously awry and a known-to-be-invalid arg was
		 * seen.  Fail early and hard. */
		if (msgs) bu_vls_printf(msgs, "Invalid argument supplied to %s: %s - halting.\n", argv[i-1], argv[i]);
		return -1;
	    }
	    /* Put the original opt back and adjust the arg_offset, if we substituted
	     * the eq_arg pointer into the argv array */
	    if (eq_arg) {
		/* If the arg_process callback did nothing with the arg, but the arg was
		 * sent to this option with an = assignment, something is wrong - the
		 * most likely scenario is an = assignment forced an argument to be
		 * sent to an option that doesn't take arguments */
		if (!arg_offset) {
		    if (msgs) bu_vls_printf(msgs, "Option %s did not successfully use the supplied argument %s - haulting.\n", argv[i-1], eq_arg);
		    return -1;
		}

		g_argv[0] = prev_opt;
		if (arg_offset > 0) {
		    arg_offset--;
		}
	    }
	    /* If we used any of the argv entries, accumulate them for later reordering
	     * and increment i */
	    for (k = (int)i; k < (int)(i + arg_offset); k++) {
		bu_ptbl_ins(&known_args, (long *)argv[k]);
	    }
	    i = i + arg_offset;
	} else {
	    /* no arg_process means this is a flag - try to set an int */
	    int *flag_var = (int *)desc->set_var;
	    if (flag_var) (*flag_var) = 1;

	    /* If we already got an arg from the equals mechanism and we aren't
	     * supposed to have one, we're invalid - halt. */
	    if (eq_arg) {
		if (msgs) bu_vls_printf(msgs, "Option %s does not take an argument, but %s was supplied - haulting.\n", argv[i-1], eq_arg);
		return -1;
	    }
	}
    }

    /* Rearrange argv so the unused options are ordered at the front of the array. */
    ret_argc = BU_PTBL_LEN(&unknown_args);
    if (ret_argc > 0) {
	int avc = 0;
	int akc = BU_PTBL_LEN(&known_args);
	for (avc = 0; avc < ret_argc; avc++) {
	    argv[avc] = (const char *)BU_PTBL_GET(&unknown_args, avc);
	}
	/* Put the option argv pointers at the end of the array, in case they
	 * are still needed for memory freeing by the caller */
	for (avc = ret_argc; avc < akc + ret_argc; avc++) {
	    argv[avc] = (const char *)BU_PTBL_GET(&unknown_args, avc);
	}
    }
    bu_ptbl_free(&unknown_args);
    bu_ptbl_free(&known_args);

    return ret_argc;
}

int
bu_opt_int(struct bu_vls *msg, int argc, const char **argv, void *set_var)
{
    long int l;
    int i;
    char *endptr = NULL;
    int *int_set = (int *)set_var;

    if (!argv || !argv[0] || strlen(argv[0]) == 0 || argc != 1 ) {
	if (msg) bu_vls_printf(msg, "bu_opt_int requires arg, but arg not found - aborting\n");
	return -1;
    }

    l = strtol(argv[0], &endptr, 0);

    if (endptr != NULL && strlen(endptr) > 0) {
	/* Had some invalid character in the input, fail */
	if (msg) bu_vls_printf(msg, "Invalid string specifier for int: %s\n", argv[0]);
	return -1;
    }

    if (errno == ERANGE) {
	if (msg) bu_vls_printf(msg, "Invalid input for int (range error): %s\n", argv[0]);
	return -1;
    }

    /* If the long fits inside an int, we're OK */
    if (l <= INT_MAX || l >= -INT_MAX) {
	i = (int)l;
    } else {
	/* Too big or too small, fail */
	if (msg) bu_vls_printf(msg, "String specifies number too large for int data type: %l\n", l);
	return -1;
    }

    if (int_set) (*int_set) = i;
    return 1;
}

int
bu_opt_long(struct bu_vls *msg, int argc, const char **argv, void *set_var)
{
    long int l;
    char *endptr = NULL;
    long *long_set = (long *)set_var;

    if (!argv || !argv[0] || strlen(argv[0]) == 0 || argc != 1 ) {
	if (msg) bu_vls_printf(msg, "bu_opt_long requires arg, but arg not found - aborting\n");
	return -1;
    }

    l = strtol(argv[0], &endptr, 0);

    if (endptr != NULL && strlen(endptr) > 0) {
	/* Had some invalid character in the input, fail */
	if (msg) bu_vls_printf(msg, "Invalid string specifier for int: %s\n", argv[0]);
	return -1;
    }

    if (errno == ERANGE) {
	if (msg) bu_vls_printf(msg, "Invalid input for int (range error): %s\n", argv[0]);
	return -1;
    }

    if (long_set) (*long_set) = l;
    return 1;
}

int
bu_opt_fastf_t(struct bu_vls *msg, int argc, const char **argv, void *set_var)
{
    fastf_t f;
    fastf_t *f_set = (fastf_t *)set_var;
    char *endptr = NULL;

    if (!argv || !argv[0] || strlen(argv[0]) == 0 || argc != 1 ) {
	if (msg) bu_vls_printf(msg, "bu_opt_fastf_t requires arg, but arg not found - aborting\n");
	return -1;
    }

    if (sizeof(fastf_t) == sizeof(float)) {
	f = strtof(argv[0], &endptr);
    }

    if (sizeof(fastf_t) == sizeof(double)) {
	f = strtod(argv[0], &endptr);
    }

    if (endptr != NULL && strlen(endptr) > 0) {
	/* Had some invalid character in the input, fail */
	if (msg) bu_vls_printf(msg, "Invalid string specifier for fastf_t: %s\n", argv[0]);
	return -1;
    }

    if (errno == ERANGE) {
	if (msg) bu_vls_printf(msg, "Invalid input for fastf_t (range error): %s\n", argv[0]);
	return -1;
    }

    if (f_set) (*f_set) = f;
    return 1;
}

int
bu_opt_str(struct bu_vls *msg, int argc, const char **argv, void *set_var)
{
    const char **s_set = (const char **)set_var;

    if (!argv || !argv[0] || strlen(argv[0]) == 0 || argc != 1 ) {
	if (msg) bu_vls_printf(msg, "bu_opt_str requires arg, but arg not found - aborting\n");
	return -1;
    }

    if (s_set) (*s_set) = argv[0];
    return 1;
}

int
bu_opt_vls(struct bu_vls *msg, int argc, const char **argv, void *set_var)
{
    struct bu_vls *s_set = (struct bu_vls *)set_var;

    if (!argv || !argv[0] || strlen(argv[0]) == 0 || argc != 1 ) {
	if (msg) bu_vls_printf(msg, "bu_opt_vls requires arg, but arg not found - aborting\n");
	return -1;
    }

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
bu_opt_bool(struct bu_vls *msg, int argc, const char **argv, void *set_var)
{
    int *b_set = (int *)set_var;
    int bool_val;

    if (!argv || !argv[0] || strlen(argv[0]) == 0 || argc != 1 ) {
	if (msg) bu_vls_printf(msg, "bu_opt_bool requires arg, but arg not found - aborting\n");
	return -1;
    }

    bool_val = bu_str_true(argv[0]);

    if (bool_val != 0 && bool_val != 1) {
	if (msg) bu_vls_printf(msg, "Invalid input for boolean type: %s\n", argv[0]);
	return -1;
    }

    if (b_set) (*b_set) = bool_val;
    return 1;
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
