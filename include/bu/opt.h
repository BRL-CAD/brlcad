/*                         O P T . H
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

#ifndef BU_OPT_H
#define BU_OPT_H

#include "common.h"
#include "bu/defines.h"
#include "bu/ptbl.h"
#include "bu/vls.h"

__BEGIN_DECLS

/** @addtogroup bu_opt
 * @brief
 * Generalized option handling.
 *
 */
/** @{ */
/** @file bu/opt.h */

/**
 * Convenience typedef for function callback to validate bu_opt
 * arguments
 */
typedef int (*bu_opt_arg_process_t)(struct bu_vls *, int argc, const char **argv, void *);

/**
 * "Option description" structure
 */
struct bu_opt_desc {
    const char *shortopt;
    const char *longopt;
    size_t arg_cnt_min;
    size_t arg_cnt_max;
    bu_opt_arg_process_t arg_process;
    void *set_var;
    const char *arg_helpstr;
    const char *help_string;
};
#define BU_OPT_DESC_NULL {NULL, NULL, 0, 0, NULL, NULL, NULL, NULL}


/**
 * Parse argv array using option descs.
 *
 * returns a bu_opt_data_t of bu_opt_data structure pointers, or NULL if the parsing
 * failed.  If a desc has an arg process, function, the valid field in
 * bu_opt_data for each option will indicate if the value in arg was
 * successfully processed by arg_process.  In situations where multiple options
 * are present, the general rule is that the last one in the list wins.
 *
 * bu_opt_desc array ds must be null terminated with BU_OPT_DESC_NULL.
 *
 * Return 0 if parse successful (all options valid) and 1 otherwise.
 *
 *
 *  Style to use when static definitions are possible
 *
 *  enum d1_opt_ind {D1_HELP, D1_VERBOSITY};
 *  struct bu_opt_desc d1[4] = {
 *      {D1_HELP, 0, 0, "h", "help", NULL, help_str},
 *      {D1_HELP, 0, 0, "?", "", NULL, help_str},
 *      {D1_VERBOSITY, 0, 1, "v", "verbosity", &(d1_verbosity), "Set verbosity"},
 *      BU_OPT_DESC_NULL
 *  };
 *  bu_opt_data_t *results;
 *  bu_opt_parse(&results, NULL, argc, argv, d1);
 *
 * Return vals:
 *
 * -1  - fatal error in parsing.  Program must decide to recover or exit.
 *  0  - all argv options handled.
 *  >0 - some unused argv entries returned in unused array (if provided), return int is unused argc count.
 *
 */
BU_EXPORT extern int bu_opt_parse(const char ***unused, size_t sizeof_unused, struct bu_vls *msgs, int ac, const char **argv, struct bu_opt_desc *ds);


/* Standard option validators - if a custom option argument
 * validation isn't needed, the functions below can be
 * used for most valid data types. When data conversion is successful,
 * the user_data pointer in bu_opt_data will point to the results
 * of the string->[type] translation in order to allow a calling
 * program to use the int/long/etc. without having to repeat the
 * conversion.
 */
BU_EXPORT extern int bu_opt_int(struct bu_vls *msg, int argc, const char **argv, void *set_var);
BU_EXPORT extern int bu_opt_fastf_t(struct bu_vls *msg, int argc, const char **argv, void *set_var);
BU_EXPORT extern int bu_opt_str(struct bu_vls *msg, int argc, const char **argv, void *set_var);
/* TODO - unimplemented */
BU_EXPORT extern int bu_opt_bool(struct bu_vls *msg, int argc, const char **argv, void *set_var);
BU_EXPORT extern int bu_opt_long(struct bu_vls *msg, int argc, const char **argv, void *set_var);
BU_EXPORT extern int bu_opt_float(struct bu_vls *msg, int argc, const char **argv, void *set_var);
BU_EXPORT extern int bu_opt_double(struct bu_vls *msg, int argc, const char **argv, void *set_var);
BU_EXPORT extern int bu_opt_ascii(struct bu_vls *msg, int argc, const char **argv, void *set_var);
BU_EXPORT extern int bu_opt_utf8(struct bu_vls *msg, int argc, const char **argv, void *set_var);


/** Output format options for bu_opt documentation generation */
typedef enum {
    BU_OPT_ASCII,
    BU_OPT_DOCBOOK /* TODO */
} bu_opt_format_t;

/**
 * Construct a textual description of the options defined by
 * the array.
 *
 * The structure is as follows:
 *
 * Offset    Options      Descriptions
 * ******--------------*********************
 *       --test-option This is a test option
 *
 * Opt_col specifies how wide the options column is, and desc_cols
 * specifies how wide the description column is.
 */

/* TODO - support actually using the struct... */
struct bu_opt_desc_opts {
    bu_opt_format_t format;
    int offset;
    int option_columns;
    int description_columns;
};

BU_EXPORT extern const char *bu_opt_describe(struct bu_opt_desc *ds, struct bu_opt_desc_opts *settings);



/** @} */

__END_DECLS

#endif  /* BU_OPT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
