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

#define BU_OPT_DATA_GET(_od, _name) {\
    BU_GET(_od, struct bu_opt_data); \
    if (_name) { \
	_od->name = _name; \
    } else { \
	_od->name = NULL; \
    } \
    _od->args = NULL; \
    _od->valid = 1; \
    _od->desc = NULL; \
    _od->user_data = NULL; \
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
		bu_free(dc, "free duplicate");
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

/* This implements criteria for deciding when an argv string is
 * an option.  Right now the criteria are:
 *
 * 1.  Must have a '-' char as first character
 * 2.  Must not have white space characters present in the string.
 */
HIDDEN int
is_opt(const char *opt) {
    size_t i = 0;
    if (!opt) return 0;
    if (!strlen(opt)) return 0;
    if (opt[0] != '-') return 0;
    for (i = 1; i < strlen(opt); i++) {
	if (isspace(opt[i])) return 0;
    }
    return 1;
}

HIDDEN struct bu_ptbl *
bu_opt_parse_internal(int argc, const char **argv, struct bu_opt_desc *ds, struct bu_ptbl *dptbl, struct bu_vls *UNUSED(msgs))
{
    int i = 0;
    int offset = 0;
    const char *ns = NULL;
    struct bu_ptbl *opt_data;
    struct bu_opt_data *unknowns = NULL;
    if (!argv || (!ds && !dptbl) || (ds && dptbl)) return NULL;

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
	/* If 'opt' isn't an option, make a container for non-option values and build it up until
	 * we reach an option */
	if (!is_opt(argv[i])) {
	    if (!unknowns) {
		BU_OPT_DATA_GET(unknowns, NULL);
		BU_GET(unknowns->args, struct bu_ptbl);
		bu_ptbl_init(unknowns->args, 8, "args init");
	    }
	    ns = bu_strdup(argv[i]);
	    bu_ptbl_ins(unknowns->args, (long *)ns);
	    i++;
	    while (i < argc && !is_opt(argv[i])) {
		ns = bu_strdup(argv[i]);
		bu_ptbl_ins(unknowns->args, (long *)ns);
		i++;
	    }
	    continue;
	}

	/* It may be that an = has been used instead of a space.  Handle that, and
	 * strip leading '-' characters.  Also, short-opt options may not have a
	 * space between their option and the argument. That is also handled here */
	opt = opt_process(&eq_arg, argv[i]);

	/* Find the corresponding desc, if we have one */
	if (ds) {
	    desc = &(ds[0]);
	} else {
	    desc = (struct bu_opt_desc *)BU_PTBL_GET(dptbl, 0);
	}
	while (!desc_found && (desc && desc->index != -1)) {
	    if (BU_STR_EQUAL(opt+offset, desc->shortopt) || BU_STR_EQUAL(opt+offset, desc->longopt)) {
		desc_found = 1;
		continue;
	    }
	    desc_ind++;
	    if (ds) {
		desc = &(ds[desc_ind]);
	    } else {
		desc = (struct bu_opt_desc *)BU_PTBL_GET(dptbl, desc_ind);
	    }
	}

	/* If we don't know what we're dealing with, keep going */
	if (!desc_found) {
	    struct bu_vls rebuilt_opt = BU_VLS_INIT_ZERO;
	    if (!unknowns) {
		BU_OPT_DATA_GET(unknowns, NULL);
		BU_GET(unknowns->args, struct bu_ptbl);
		bu_ptbl_init(unknowns->args, 8, "args init");
	    }
	    if (strlen(opt) == 1) {
		bu_vls_sprintf(&rebuilt_opt, "-%s", opt);
	    } else {
		bu_vls_sprintf(&rebuilt_opt, "--%s", opt);
	    }
	    bu_ptbl_ins(unknowns->args, (long *)bu_strdup(bu_vls_addr(&rebuilt_opt)));
	    bu_free(opt, "free opt");
	    bu_vls_free(&rebuilt_opt);
	    if (eq_arg)
		bu_ptbl_ins(unknowns->args, (long *)eq_arg);
	    i++;
	    continue;
	}

	/* Initialize with opt */
	BU_OPT_DATA_GET(data, opt);
	data->desc = desc;
	if (eq_arg) {
	    /* Okay, we actually need it - initialize the arg table */
	    BU_GET(data->args, struct bu_ptbl);
	    bu_ptbl_init(data->args, 8, "args init");
	    bu_ptbl_ins(data->args, (long *)eq_arg);
	    arg_cnt = 1;
	}

	/* handled the option - any remaining processing is on args, if any*/
	i = i + 1;

	/* If we already got an arg from the equals mechanism and we aren't
	 * supposed to have one, we're invalid */
	if (arg_cnt > 0 && desc->arg_cnt_max == 0) data->valid = 0;

	/* If we're looking for args, do so */
	if (desc->arg_cnt_max > 0) {
	    while (arg_cnt < desc->arg_cnt_max && i < argc && !is_opt(argv[i])) {
		ns = bu_strdup(argv[i]);
		if (!data->args) {
		    /* Okay, we actually need it - initialize the arg table */
		    BU_GET(data->args, struct bu_ptbl);
		    bu_ptbl_init(data->args, 8, "args init");
		}
		bu_ptbl_ins(data->args, (long *)ns);
		i++;
		arg_cnt++;
	    }
	    if (arg_cnt < desc->arg_cnt_min) {
		data->valid = 0;
	    }
	}

	/* Now see if we need to validate the arg(s) */
	if (desc->arg_process) {
	    int arg_offset = (*desc->arg_process)(NULL, data);
	    if (arg_offset < 0) {
		/* arg(s) present but not associated with opt, back them out of data
		 *
		 * test example for this - color option
		 *
		 * --color 200/30/10 input output  (1 arg to color, 3 args total)
		 * --color 200 30 10 input output  (3 args to color, 5 total)
		 *
		 *  to handle the color case, need to process all three in first case,
		 *  recognize that first one is sufficient, and release the latter two.
		 *  for the second case all three are necessary
		 *
		 * */
		int len = BU_PTBL_LEN(data->args);
		int j = 0;
		i = i + arg_offset;
		while (j > arg_offset) {
		    bu_free((void *)BU_PTBL_GET(data->args, len + j - 1), "free str");
		    j--;
		}
		bu_ptbl_trunc(data->args, len + arg_offset);
	    }
	}
	bu_ptbl_ins(opt_data, (long *)data);
    }
    if (unknowns) bu_ptbl_ins(opt_data, (long *)unknowns);

    return opt_data;
}

int
bu_opt_parse(struct bu_ptbl **tbl, struct bu_vls *msgs, int ac, const char **argv, struct bu_opt_desc *ds)
{
    struct bu_ptbl *results = NULL;
    if (!tbl || !argv || !ds) return 1;
    results = bu_opt_parse_internal(ac, argv, ds, NULL, msgs);
    if (results) {
	(*tbl) = results;
	return 0;
    } else {
	return 1;
    }
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

int
bu_opt_parse_dtbl(struct bu_ptbl **tbl, struct bu_vls *msgs, int ac, const char **argv, struct bu_ptbl *dtbl)
{
    struct bu_ptbl *results = NULL;
    if (!tbl || !argv || !dtbl) return 1;
    results = bu_opt_parse_internal(ac, argv, NULL, dtbl, msgs);
    if (results) {
	(*tbl) = results;
	return 0;
    } else {
	return 1;
    }
}

int
bu_opt_parse_str_dtbl(struct bu_ptbl **tbl, struct bu_vls *msgs, const char *str, struct bu_ptbl *dtbl)
{
    int ret = 0;
    char *input = NULL;
    char **argv = NULL;
    int argc = 0;
    if (!tbl || !str || !dtbl) return 1;
    input = bu_strdup(str);
    argv = (char **)bu_calloc(strlen(input) + 1, sizeof(char *), "argv array");
    argc = bu_argv_from_string(argv, strlen(input), input);

    ret = bu_opt_parse_dtbl(tbl, msgs, argc, (const char **)argv, dtbl);

    bu_free(input, "free str copy");
    bu_free(argv, "free argv memory");
    return ret;
}


struct bu_opt_data *
bu_opt_find_name(const char *name, struct bu_ptbl *opts)
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
	if (BU_STR_EQUAL(opt->desc->shortopt, name) || BU_STR_EQUAL(opt->desc->longopt, name)) {
	    /* option culling guarantees us one "winner" if multiple instances
	     * of an option were originally supplied, so if we find a match we
	     * have found what we wanted.  Now, just need to check validity */
	    return (opt->valid) ? opt : NULL;
	}
    }
    return NULL;
}

struct bu_opt_data *
bu_opt_find(int key, struct bu_ptbl *opts)
{
    size_t i;
    if (!opts) return NULL;

    for (i = 0; i < BU_PTBL_LEN(opts); i++) {
	struct bu_opt_data *opt = (struct bu_opt_data *)BU_PTBL_GET(opts, i);
	/* Don't check the unknown opts - they were already marked as not
	 * valid opts per the current descriptions in the parsing pass */
	if (key == -1 && !opt->name) return opt;
	if (!opt->name) continue;
	if (!opt->desc) continue;
	if (key == opt->desc->index) {
	    /* option culling guarantees us one "winner" if multiple instances
	     * of an option were originally supplied, so if we find a match we
	     * have found what we wanted.  Now, just need to check validity */
	    return (opt->valid) ? opt : NULL;
	}
    }
    return NULL;
}

void
bu_opt_data_init(struct bu_opt_data *d)
{
    if (!d) return;
    d->desc = NULL;
    d->valid = 0;
    d->name = NULL;
    d->args = NULL;
    d->user_data = NULL;
}

void
bu_opt_data_free(struct bu_opt_data *d)
{
    if (!d) return;
    if (d->name) bu_free((char *)d->name, "free data name");
    if (d->user_data) bu_free(d->user_data, "free user data");
    if (d->args) {
	size_t i;
	for (i = 0; i < BU_PTBL_LEN(d->args); i++) {
	    char *arg = (char *)BU_PTBL_GET(d->args, i);
	    bu_free(arg, "free arg");
	}
	bu_ptbl_free(d->args);
	BU_PUT(d->args, struct bu_ptbl);
    }
}


void
bu_opt_data_free_tbl(struct bu_ptbl *tbl)
{
    size_t i;
    if (!tbl) return;
    for (i = 0; i < BU_PTBL_LEN(tbl); i++) {
	struct bu_opt_data *opt = (struct bu_opt_data *)BU_PTBL_GET(tbl, i);
	bu_opt_data_free(opt);
    }
    bu_ptbl_free(tbl);
    BU_PUT(tbl, struct bu_ptbl);
}

const char *
bu_opt_data_arg(struct bu_opt_data *d, int ind)
{
    if (!d) return NULL;
    if (d->args) {
	if (ind > (int)(BU_PTBL_LEN(d->args) - 1)) return NULL;
	return (const char *)BU_PTBL_GET(d->args, ind);
    }
    return NULL;
}

void
bu_opt_desc_init(struct bu_opt_desc *d)
{
    if (!d) return;
    d->index = -1;
    d->arg_cnt_min = 0;
    d->arg_cnt_max = 0;
    d->shortopt = NULL;
    d->longopt = NULL;
    d->arg_process = NULL;
    d->shortopt_doc = NULL;
    d->longopt_doc = NULL;
    d->help_string = NULL;
}

void
bu_opt_desc_set(struct bu_opt_desc *d, int ind,
	size_t min, size_t max, const char *shortopt,
	const char *longopt, bu_opt_arg_process_t arg_process,
	const char *shortopt_doc, const char *longopt_doc, const char *help_str)
{
    if (!d) return;
    d->index = ind;
    d->arg_cnt_min = min;
    d->arg_cnt_max = max;
    d->shortopt = shortopt;
    d->longopt = longopt;
    d->arg_process = arg_process;
    d->shortopt_doc= shortopt_doc;
    d->longopt_doc = longopt_doc;
    d->help_string = help_str;
}

void
bu_opt_desc_free(struct bu_opt_desc *d)
{
    if (!d) return;
    if (d->shortopt) bu_free((char *)d->shortopt, "shortopt");
    if (d->longopt) bu_free((char *)d->longopt, "shortopt");
    if (d->shortopt_doc) bu_free((char *)d->shortopt_doc, "shortopt");
    if (d->longopt_doc) bu_free((char *)d->longopt_doc, "shortopt");
    if (d->help_string) bu_free((char *)d->help_string, "shortopt");
}

void
bu_opt_desc_free_tbl(struct bu_ptbl *tbl)
{
    size_t i;
    if (!tbl) return;
    for (i = 0; i < BU_PTBL_LEN(tbl); i++) {
	struct bu_opt_desc *opt = (struct bu_opt_desc *)BU_PTBL_GET(tbl, i);
	bu_opt_desc_free(opt);
    }
    bu_ptbl_free(tbl);
    BU_PUT(tbl, struct bu_ptbl);
}

HIDDEN void
wrap_help(struct bu_vls *help, int offset, int len)
{
    int i, sc = 0;
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
	    bu_vls_printf(&new_help, "%s\n", bu_vls_addr(&working));
	    for (sc = 0; sc < offset; sc++) bu_vls_printf(&new_help, " ");
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
bu_opt_describe(struct bu_opt_desc *ds, bu_opt_format_t UNUSED(type), int opt_cols, int desc_cols)
{
    size_t i = 0;
    size_t j = 0;
    size_t opt_cnt = 0;
    int sc = 0;
    const char *finalized;
    struct bu_vls description = BU_VLS_INIT_ZERO;
    int *status;
    if (!ds || ds[0].index == -1) return NULL;
    while (ds[i].index != -1) i++;
    opt_cnt = i;
    status = (int *)bu_calloc(opt_cnt, sizeof(int), "opt status");
    i = 0;
    while (i < opt_cnt) {
	struct bu_opt_desc *curr = &(ds[i]);
	if (!status[i]) {
	    struct bu_vls opts = BU_VLS_INIT_ZERO;
	    struct bu_vls help_str = BU_VLS_INIT_ZERO;
	    /* Collect the short options first - may be multiple instances with
	     * the same index defining aliases, so accumulate all of them. */
	    j = i;
	    while (j < opt_cnt) {
		struct bu_opt_desc *d = &(ds[j]);
		if (d->index == curr->index) {
		    int new_len = strlen(d->shortopt_doc);
		    if (new_len > 0) {
			if ((int)bu_vls_strlen(&opts) + new_len + 2 > opt_cols + desc_cols) {
			    bu_vls_printf(&description, "%s\n", bu_vls_addr(&opts));
			    bu_vls_sprintf(&opts, "%s, ", d->shortopt_doc);
			} else {
			    bu_vls_printf(&opts, "%s, ", d->shortopt_doc);
			}
		    }
		    if (strlen(d->help_string) > 0) bu_vls_sprintf(&help_str, "%s", d->help_string);
		}
		j++;
	    }
	    j = i;
	    while (j < opt_cnt) {
		struct bu_opt_desc *d = &(ds[j]);
		if (d->index == curr->index) {
		    int new_len = strlen(d->longopt_doc);
		    if (new_len > 0) {
			if ((int)bu_vls_strlen(&opts) + new_len + 2 > opt_cols + desc_cols) {
			    bu_vls_printf(&description, "%s\n", bu_vls_addr(&opts));
			    bu_vls_sprintf(&opts, "%s, ", d->longopt_doc);
			} else {
			    bu_vls_printf(&opts, "%s, ", d->longopt_doc);
			}
		    }
		}
		j++;
	    }

	    bu_vls_trunc(&opts, -2);
	    bu_vls_printf(&description, "%s", bu_vls_addr(&opts));
	    if ((int)bu_vls_strlen(&opts) > opt_cols) {
		bu_vls_printf(&description, "\n");
		for (sc = 0; sc < opt_cols; sc++) bu_vls_printf(&description, " ");
	    } else {
		for (sc = 0; sc < opt_cols - (int)bu_vls_strlen(&opts); sc++) bu_vls_printf(&description, " ");
	    }
	    if ((int)bu_vls_strlen(&help_str) > desc_cols) {
		wrap_help(&help_str, opt_cols, desc_cols);
	    }
	    bu_vls_printf(&description, "%s\n", bu_vls_addr(&help_str));
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




/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
