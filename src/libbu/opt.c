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
#include <ctype.h> /* for isspace */

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"

HIDDEN void
bu_opt_data_init_entry(struct bu_opt_data **d, const char *name)
{
    if (!d) return;
    BU_GET(*d, struct bu_opt_data);
    (*d)->desc = NULL;
    (*d)->valid = 1;
    (*d)->name = name;
    (*d)->argc = 0;
    (*d)->argv = NULL;
    (*d)->user_data = NULL;
}

HIDDEN void
bu_opt_free_argv(int ac, char **av)
{
    if (av) {
	int i = 0;
	for (i = 0; i < ac; i++) {
	    bu_free((char *)av[i], "free argv[i]");
	}
	bu_free((char *)av, "free argv");
    }
}

HIDDEN void
bu_opt_data_free_entry(struct bu_opt_data *d)
{
    if (!d) return;
    if (d->name) bu_free((char *)d->name, "free data name");
    if (d->user_data) bu_free(d->user_data, "free user data");
    bu_opt_free_argv(d->argc, (char **)d->argv);
}


void
bu_opt_data_free(struct bu_ptbl *tbl)
{
    size_t i;
    if (!tbl) return;
    for (i = 0; i < BU_PTBL_LEN(tbl); i++) {
	struct bu_opt_data *opt = (struct bu_opt_data *)BU_PTBL_GET(tbl, i);
	bu_opt_data_free_entry(opt);
    }
    bu_ptbl_free(tbl);
    BU_PUT(tbl, struct bu_ptbl);
}

void
bu_opt_data_print(struct bu_ptbl *data, const char *title)
{
    size_t i = 0;
    int j = 0;
    int offset_1 = 3;
    struct bu_vls log = BU_VLS_INIT_ZERO;
    if (!data || BU_PTBL_LEN(data) == 0) return;
    if (title) {
	bu_vls_sprintf(&log, "%s\n", title);
    } else {
	bu_vls_sprintf(&log, "Options:\n");
    }
    for (i = 0; i < BU_PTBL_LEN(data); i++) {
	struct bu_opt_data *d = (struct bu_opt_data *)BU_PTBL_GET(data, i);
	if (d->name) {
	    bu_vls_printf(&log, "%*s%s", offset_1, " ", d->name);
	    if (d->valid) {
		bu_vls_printf(&log, "\t(valid)");
	    } else {
		bu_vls_printf(&log, "\t(invalid)");
	    }
	    if (d->desc && d->desc->arg_cnt_max > 0) {
		if (d->argc > 0 && d->argv) {
		    bu_vls_printf(&log, ": ");
		    for (j = 0; j < d->argc - 1; j++) {
			bu_vls_printf(&log, "%s, ", d->argv[j]);
		    }
		    bu_vls_printf(&log, "%s\n", d->argv[d->argc - 1]);
		} else {
		    bu_vls_printf(&log, "\n");
		}
	    } else {
		bu_vls_printf(&log, "\n");
	    }
	} else {
	    bu_vls_printf(&log, "%*s(unknown): ", offset_1, " ", d->name);
	    if (d->argc > 0 && d->argv) {
		for (j = 0; j < d->argc - 1; j++) {
		    bu_vls_printf(&log, "%s ", d->argv[j]);
		}
		bu_vls_printf(&log, "%s\n", d->argv[d->argc-1]);
	    }
	}
    }
    bu_log("%s", bu_vls_addr(&log));
    bu_vls_free(&log);
}

struct bu_opt_data *
bu_opt_find(const char *name, struct bu_ptbl *opts)
{
    size_t i;
    if (!opts) return NULL;

    for (i = 0; i < BU_PTBL_LEN(opts); i++) {
	struct bu_opt_data *opt = (struct bu_opt_data *)BU_PTBL_GET(opts, i);
	/* Don't check the unknown opts - they were already marked as not
	 * valid opts per the current descriptions in the parsing pass */
	if (!name && !opt->name) return opt;
	if (!opt->name) continue;
	if (!opt->desc) continue;
	if ((name && BU_STR_EQUAL(opt->desc->shortopt, name)) || (name && BU_STR_EQUAL(opt->desc->longopt, name))) {
	    /* option culling guarantees us one "winner" if multiple instances
	     * of an option were originally supplied, so if we find a match we
	     * have found what we wanted.  Now, just need to check validity */
	    return (opt->valid) ? opt : NULL;
	}
    }
    return NULL;
}

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
    if (!ds || ds[0].index == -1) return NULL;

    if (settings) {
	offset = settings->offset;
	opt_cols = settings->option_columns;
	desc_cols = settings->description_columns;
    }

    while (ds[i].index != -1) i++;
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

	    /* We handle all entries with the same key in the same
	     * pass, so set the status flags accordingly */
	    j = i;
	    while (j < opt_cnt) {
		d = &(ds[j]);
		if (d->index == curr->index) {
		    status[j] = 1;
		}
		j++;
	    }

	    /* Collect the short options first - may be multiple instances with
	     * the same index defining aliases, so accumulate all of them. */
	    j = i;
	    while (j < opt_cnt) {
		d = &(ds[j]);
		if (d->index == curr->index) {
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
		if (d->index == curr->index) {
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

#if 0
	    /* A possible exception is an equals sign, e.g. -s=1024 - in that
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
#endif

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

void
bu_opt_compact(struct bu_ptbl *opts)
{
    int i;
    int ptblpos = BU_PTBL_LEN(opts) - 1;
    struct bu_ptbl tbl;
    bu_ptbl_init(&tbl, 8, "local table");
    while (ptblpos >= 0) {
	struct bu_opt_data *data = (struct bu_opt_data *)BU_PTBL_GET(opts, ptblpos);
	if (!data) {
	    ptblpos--;
	    continue;
	}
	bu_ptbl_ins(&tbl, (long *)data);
	BU_PTBL_CLEAR_I(opts, ptblpos);
	for (i = ptblpos - 1; i >= 0; i--) {
	    struct bu_opt_data *dc = (struct bu_opt_data *)BU_PTBL_GET(opts, i);
	    if ((dc && dc->desc && data->desc) && dc->desc->index == data->desc->index) {
		bu_opt_data_free_entry(dc);
		BU_PTBL_CLEAR_I(opts, i);
	    }
	}
	ptblpos--;
    }
    bu_ptbl_reset(opts);
    for (i = BU_PTBL_LEN(&tbl) - 1; i >= 0; i--) {
	bu_ptbl_ins(opts, BU_PTBL_GET(&tbl, i));
    }
    bu_ptbl_free(&tbl);
}


void
bu_opt_validate(struct bu_ptbl *opts)
{
    size_t i;
    struct bu_ptbl tbl;
    bu_ptbl_init(&tbl, 8, "local table");
    for (i = 0; i < BU_PTBL_LEN(opts); i++) {
	struct bu_opt_data *dc = (struct bu_opt_data *)BU_PTBL_GET(opts, i);
	if (dc && (dc->valid || (dc->desc && dc->desc->index == -1))) {
	    bu_ptbl_ins(&tbl, (long *)dc);
	} else {
	    bu_opt_data_free_entry(dc);
	    BU_PTBL_CLEAR_I(opts, i);
	}
    }
    bu_ptbl_reset(opts);
    for (i = 0; i < BU_PTBL_LEN(&tbl); i++) {
	bu_ptbl_ins(opts, BU_PTBL_GET(&tbl, i));
    }
    bu_ptbl_free(&tbl);
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

HIDDEN int
ptbl_to_argv(const char ***argv, struct bu_ptbl *tbl) {
    int i;
    const char **av;
    int ac;
    if (!argv || !tbl || BU_PTBL_LEN(tbl) == 0) return 0;
    ac = BU_PTBL_LEN(tbl);
    av = (const char **)bu_calloc(ac + 1, sizeof(char *), "argv");
    for (i = 0; i < ac; i++) {
	av[i] = (const char *)BU_PTBL_GET(tbl, i);
    }
    /* Make it explicitly clear that the array is NULL terminated */
    av[ac] = NULL;
    (*argv) = av;
    return ac;
}

int
bu_opt_parse(struct bu_ptbl **tbl, struct bu_vls *UNUSED(msgs), int argc, const char **argv, struct bu_opt_desc *ds)
{
    int i = 0;
    int offset = 0;
    const char *ns = NULL;
    struct bu_ptbl *opt_data;
    struct bu_ptbl unknown_args = BU_PTBL_INIT_ZERO;
    struct bu_opt_data *unknown = NULL;
    if (!argv || !ds) return 1;

    BU_GET(opt_data, struct bu_ptbl);
    bu_ptbl_init(opt_data, 8, "opt_data");

    /* Now identify opt/arg pairs.*/
    while (i < argc) {
	int desc_found = 0;
	int desc_ind = 0;
	size_t arg_cnt = 0;
	char *opt = NULL;
	char *eq_arg = NULL;
	struct bu_opt_data *data = NULL;
	struct bu_opt_desc *desc = NULL;
	struct bu_ptbl data_args = BU_PTBL_INIT_ZERO;
	/* If argv[i] isn't an option, stick the argv entry (and any
	 * immediately following non-option entries) into the
	 * unknown args table */
	if (!can_be_opt(argv[i])) {
	    ns = bu_strdup(argv[i]);
	    bu_ptbl_ins(&unknown_args, (long *)ns);
	    i++;
	    while (i < argc && !can_be_opt(argv[i])) {
		ns = bu_strdup(argv[i]);
		bu_ptbl_ins(&unknown_args, (long *)ns);
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
	while (!desc_found && (desc && desc->index != -1)) {
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
	    ns = bu_strdup(argv[i]);
	    bu_ptbl_ins(&unknown_args, (long *)ns);
	    i++;
	    continue;
	}

	/* We've got a description of the option.  Now the real work begins. */

	/* Initialize with opt */
	bu_opt_data_init_entry(&data, opt);
	data->desc = desc;
	if (eq_arg) {
	    bu_ptbl_ins(&data_args, (long *)eq_arg);
	    arg_cnt = 1;
	}

	/* handled the option - any remaining processing is on args, if any*/
	i = i + 1;

	/* If we already got an arg from the equals mechanism and we aren't
	 * supposed to have one, we're invalid */
	if (arg_cnt > 0 && desc->arg_cnt_max == 0) data->valid = 0;

	/* If we're looking for args, do so */
	if (desc->arg_cnt_max > 0) {
	    /* If we might have args and we have a validator function,
	     * construct the greediest possible iterpretation of the option
	     * description and run the validator to determine the number of
	     * argv entries associated with this option (can_be_opt is not
	     * enough if the option is number based, since -9 may be both a
	     * valid option and a valid argument - the validator must make the
	     * decision.  If we do not have a validator, the best we can do
	     * is the can_be_opt test as a terminating trigger. */
	    if (desc->arg_process) {
		/* Construct the greedy interpretation of the bu_opt_data argv */
		int k = 0;
		int arg_offset = 0;
		int g_argc = desc->arg_cnt_max;
		const char **g_argv = (const char **)bu_calloc(g_argc + arg_cnt + 1, sizeof(char *), "greedy argv");
		if (!g_argc && arg_cnt) g_argc = arg_cnt;
		if (i != argc) {
		    if (arg_cnt)
			g_argv[0] = eq_arg;
		    for (k = arg_cnt; k < g_argc; k++) {
			g_argv[k] = argv[i + k];
		    }
		    data->argc = g_argc;
		    data->argv = g_argv;
		    arg_offset = (*desc->arg_process)(NULL, data);
		    i = i + arg_offset - arg_cnt;
		    if (!arg_offset && arg_cnt) arg_offset = arg_cnt;
		    /* If we used 1 or more args, construct the final argv array that will
		     * be assigned to the bu_opt_data container */
		    if (arg_offset > 0) {
			data->argc = arg_offset;
			data->argv = (const char **)bu_calloc(arg_offset + 1, sizeof(char *), "final array");
			if (arg_cnt) data->argv[0] = eq_arg;
			for (k = arg_cnt; k < arg_offset; k++) {
			    data->argv[k] = bu_strdup(g_argv[k - arg_cnt]);
			}
		    }
		} else {
		    /* Need args, but have none - invalid */
		    data->valid = 0;
		    data->argc = 0;
		    data->argv = NULL;
		}
		bu_free(g_argv, "free greedy argv");
	    } else {
		while (arg_cnt < desc->arg_cnt_max && i < argc && !can_be_opt(argv[i])) {
		    ns = bu_strdup(argv[i]);
		    bu_ptbl_ins(&data_args, (long *)ns);
		    i++;
		    arg_cnt++;
		}
		if (arg_cnt < desc->arg_cnt_min) {
		    data->valid = 0;
		}
		data->argc = ptbl_to_argv(&(data->argv), &data_args);
	    }
	}
	bu_ptbl_free(&data_args);
	bu_ptbl_ins(opt_data, (long *)data);
    }

    /* Make an argv array and data entry for the unknown, if any */
    if (BU_PTBL_LEN(&unknown_args) > 0){
	bu_opt_data_init_entry(&unknown, NULL);
	unknown->argc = ptbl_to_argv(&(unknown->argv), &unknown_args);
	bu_ptbl_free(&unknown_args);
	bu_ptbl_ins(opt_data, (long *)unknown);
    }

    (*tbl) = opt_data;
    return 0;
}

int
bu_opt_parse_str(struct bu_ptbl **tbl, struct bu_vls *msgs, const char *str, struct bu_opt_desc *ds)
{
    int ret = 0;
    char *input = NULL;
    char **argv = NULL;
    int argc = 0;
    if (!tbl || !str || !ds) return 1;
    input = bu_strdup(str);
    argv = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
    argc = bu_argv_from_string(argv, strlen(input), input);

    ret = bu_opt_parse(tbl, msgs, argc, (const char **)argv, ds);

    bu_free(input, "free str copy");
    bu_free(argv, "free argv memory");
    return ret;
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
