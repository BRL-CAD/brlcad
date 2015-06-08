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

    if (ds->arg_cnt_min != 0) non_null++;
    if (ds->arg_cnt_max != 0) non_null++;
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
			if (d->arg_cnt_min == 0 && d->arg_cnt_max != 0) {
			    if (new_len > 0) {
				bu_vls_sprintf(&tmp_arg, "-%s [%s]", d->shortopt, d->arg_helpstr);
				new_len = new_len + 5;
			    } else {
				bu_vls_sprintf(&tmp_arg, "-%s [opts]", d->shortopt);
				new_len = 9;
			    }
			} else {
			    if (d->arg_cnt_min == 0 && d->arg_cnt_max == 0) {
				bu_vls_sprintf(&tmp_arg, "-%s", d->shortopt);
				new_len = 2;
			    }
			    if (d->arg_cnt_min > 0) {
				if (new_len > 0) {
				    bu_vls_sprintf(&tmp_arg, "-%s %s", d->shortopt, d->arg_helpstr);
				    new_len = new_len + 3;
				} else {
				    bu_vls_sprintf(&tmp_arg, "-%s opts", d->shortopt);
				    new_len = 7;
				}
			    }
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
			if (d->arg_cnt_min == 0 && d->arg_cnt_max != 0) {
			    if (new_len > 0) {
				bu_vls_sprintf(&tmp_arg, "--%s [%s]", d->longopt, d->arg_helpstr);
				new_len = new_len + strlen(d->longopt) + 5;
			    } else {
				bu_vls_sprintf(&tmp_arg, "--%s [opts]", d->longopt);
				new_len = strlen(d->longopt) + 9;
			    }
			} else {
			    if (d->arg_cnt_min == 0 && d->arg_cnt_max == 0) {
				bu_vls_sprintf(&tmp_arg, "--%s", d->longopt);
				new_len = strlen(d->longopt) + 2;
			    }
			    if (d->arg_cnt_min > 0) {
				if (new_len > 0) {
				    bu_vls_sprintf(&tmp_arg, "--%s %s", d->longopt, d->arg_helpstr);
				    new_len = strlen(d->longopt) + new_len + 3;
				} else {
				    bu_vls_sprintf(&tmp_arg, "--%s opts", d->longopt);
				    new_len = strlen(d->longopt) + 7;
				}
			    }
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
opt_process(char **eq_arg, const char *opt_candidate)
{
    int offset = 1;
    char *inputcpy;
    char *final_opt;
    char *equal_pos;
    if (!eq_arg && !opt_candidate) return NULL;
    inputcpy = bu_strdup(opt_candidate);
    if (inputcpy[1] == '-') offset++;
    equal_pos = strchr(inputcpy, '=');

    /* If we've got a single opt, things are handled differently */
    if (offset == 1) {
	if (strlen(opt_candidate+offset) == 1) {
	    final_opt = bu_strdup(opt_candidate+offset);
	} else {
	    /* single letter opt, but the string is longer - the
	     * interpretation in this context is everything after
	     * the first letter is arg.*/
	    struct bu_vls vopt = BU_VLS_INIT_ZERO;
	    struct bu_vls varg = BU_VLS_INIT_ZERO;
	    bu_vls_strncat(&vopt, inputcpy+1, 1);
	    bu_vls_sprintf(&varg, "%s", inputcpy);
	    bu_vls_nibble(&varg, 2);

	    /* A exception is an equals sign, e.g. -s=1024 - in that
	     * instance, the expectation might be that = would be interpreted
	     * as an assignment.  This means that to get the literal =1024 as
	     * an option, you would need a space after the s, e.g.:  -s =1024
	     *
	     * For now, commented out to favor consistent behavior over what
	     * "looks right" - may be worth revisiting or even an option at
	     * some point...*/

	    if (equal_pos && equal_pos == inputcpy+2) {
		bu_vls_nibble(&varg, 1);
	    }

	    (*eq_arg) = bu_strdup(bu_vls_addr(&varg));
	    final_opt = bu_strdup(bu_vls_addr(&vopt));
	    bu_vls_free(&vopt);
	    bu_vls_free(&varg);
	}
    } else {
	if (equal_pos) {
	    struct bu_vls vopt = BU_VLS_INIT_ZERO;
	    struct bu_vls varg = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&vopt, "%s", inputcpy);
	    bu_vls_trunc(&vopt, -1 * strlen(equal_pos));
	    bu_vls_nibble(&vopt, offset);
	    bu_vls_sprintf(&varg, "%s", inputcpy);
	    bu_vls_nibble(&varg, strlen(inputcpy) - strlen(equal_pos) + 1);
	    (*eq_arg) = bu_strdup(bu_vls_addr(&varg));
	    final_opt = bu_strdup(bu_vls_addr(&vopt));
	    bu_vls_free(&vopt);
	    bu_vls_free(&varg);
	} else {
	    final_opt = bu_strdup(opt_candidate+offset);
	}
    }
    bu_free(inputcpy, "cleanup working copy");
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
bu_opt_parse(const char ***unused, size_t sizeof_unused, struct bu_vls *msgs, int argc, const char **argv, struct bu_opt_desc *ds)
{
    int i = 0;
    int offset = 0;
    int ret_argc = 0;
    struct bu_ptbl unknown_args = BU_PTBL_INIT_ZERO;
    if (!argv || !ds) return -1;

    /* Now identify opt/arg pairs.*/
    while (i < argc) {
	int desc_found = 0;
	int desc_ind = 0;
	size_t arg_cnt = 0;
	char *opt = NULL;
	char *eq_arg = NULL;
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

	/* We've got a description of the option.  Now the real work begins. */
	if (eq_arg) arg_cnt = 1;

	/* handled the option - any remaining processing is on args, if any*/
	i = i + 1;

	/* If we already got an arg from the equals mechanism and we aren't
	 * supposed to have one, we're invalid - halt. */
	if (eq_arg && desc->arg_cnt_max == 0) {
	    if (msgs) bu_vls_printf(msgs, "Option %s takes no arguments, but argument %s is present - halting.\n", argv[i-1], eq_arg);
	    bu_free(eq_arg, "free arg after equals sign copy");
	    return -1;
	}

	/* If we're looking for args, do so */
	if (desc->arg_cnt_max > 0) {
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
		int g_argc = desc->arg_cnt_max;
		const char **g_argv = (const char **)bu_calloc(g_argc + arg_cnt + 1, sizeof(char *), "greedy argv");
		if (!g_argc && arg_cnt) g_argc = arg_cnt;
		if (i != argc || arg_cnt) {
		    if (arg_cnt)
			g_argv[0] = eq_arg;
		    for (k = 0; k < g_argc; k++) {
			g_argv[k+arg_cnt] = argv[i + k];
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
		    if (arg_offset == 0) {
			if (desc->arg_cnt_min > 0) {
			    if (msgs) bu_vls_printf(msgs, "Option %s requires an argument but none was found - halting.\n", argv[i-1]);
			    return -1;
			} else {
			    continue;
			}
		    }
		    i = i + arg_offset - arg_cnt;
		} else {
		    if (desc->arg_cnt_min == 0) {
			/* If this is allowed to function just as a flag, an int may
			 * be supplied to record the status - try to set it */
			int *flag_var = (int *)desc->set_var;
			if (flag_var) (*flag_var) = 1;
		    }
		}
		bu_free(g_argv, "free greedy argv");
	    } else {
		while (arg_cnt < desc->arg_cnt_max && i < argc && !can_be_opt(argv[i])) {
		    i++;
		    arg_cnt++;
		}
		if (arg_cnt < desc->arg_cnt_min) {
		    if (msgs) bu_vls_printf(msgs, "Option %s requires at least %d arguments but only %d were found - halting.\n", argv[i-1], desc->arg_cnt_min, arg_cnt);
		    return -1;
		}
	    }
	} else {
	    /* only a flag - see if we're supposed to set an int */
	    int *flag_var = (int *)desc->set_var;
	    if (flag_var) (*flag_var) = 1;
	}
    }

    /* Copy as many of the unknown args as we can fit into the provided argv array.
     * Program must check return value to see if any were lost due to the provided
     * array being too small. */
    ret_argc = BU_PTBL_LEN(&unknown_args);
    if (ret_argc > 0 && sizeof_unused > 0 && unused) {
	int avc = 0;
	int max_avc_cnt = (BU_PTBL_LEN(&unknown_args) < sizeof_unused) ? BU_PTBL_LEN(&unknown_args) : sizeof_unused;
	for (avc = 0; avc < max_avc_cnt; avc++) {
	    (*unused)[avc] = (const char *)BU_PTBL_GET(&unknown_args, avc);
	}
    }
    bu_ptbl_free(&unknown_args);

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
	return 0;
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
bu_opt_fastf_t(struct bu_vls *msg, int argc, const char **argv, void *set_var)
{
    fastf_t f;
    fastf_t *f_set = (fastf_t *)set_var;
    char *endptr = NULL;

    if (!argv || !argv[0] || strlen(argv[0]) == 0 || argc != 1 ) {
	return 0;
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

/* TODO - the bu_strdup is required because of eq_arg (i.e., breaking
 * out an argument from an atomic argv entry) - can that be reworked
 * to avoid needing the strdup by incrementing the pointer and pointing
 * to the arg portion of the original string? */
int
bu_opt_str(struct bu_vls *UNUSED(msg), int argc, const char **argv, void *set_var)
{
    char **s_set = (char **)set_var;

    if (!argv || !argv[0] || strlen(argv[0]) == 0 || argc != 1 ) {
	return 0;
    }

    if (s_set) (*s_set) = bu_strdup(argv[0]);
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
