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

/* Pick an arbitrary maximum pending a good reason to pick some specific value */
#define BU_OPT_MAX_ARGS 2000

/* Make a human-readable define for using bu_opt_find to retrieve argv entries
 * that were not associated with options */
#define BU_NON_OPTS -1

/** Output format options for bu_opt documentation generation */
typedef enum {
    BU_OPT_ASCII,
    BU_OPT_DOCBOOK, /* TODO */
    BU_OPT_HTML,    /* TODO */
    BU_OPT_LATEX,   /* TODO */
    BU_OPT_MARKDOWN /* TODO */
} bu_opt_format_t;

typedef enum {
    BU_OPT_SHORT,  /* TODO */
    BU_OPT_FULL    /* TODO */
} bu_opt_desc_t;


/* Forward declaration for bu_opt_desc and bu_opt_arg_process_t */
struct bu_opt_data;

/**
 * Convenience typedef for function callback to validate bu_opt
 * arguments
 */
typedef int (*bu_opt_arg_process_t)(struct bu_vls *, struct bu_opt_data *);

/* typedefs to avoid confusion when working with bu_opt_data
 * and bu_opt_desc tables */
typedef struct bu_ptbl bu_opt_dtbl_t;

/** TODO - should this be a full struct to allow for easier iteration? */
typedef struct bu_ptbl bu_opt_data_t;

/**
 * "Option description" structure
 */
struct bu_opt_desc {
    int index;
    size_t arg_cnt_min;
    size_t arg_cnt_max;
    const char *shortopt;
    const char *longopt;
    bu_opt_arg_process_t arg_process;
    const char *shortopt_doc;
    const char *longopt_doc;
    const char *help_string;
};
#define BU_OPT_DESC_NULL {-1, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL}

/**
 * Initialize a bu_opt_dtbl_t.  If ds is not NULL, populate the table
 * with the bu_opt_desc structs in the ds array. */
BU_EXPORT extern void bu_opt_desc_init(bu_opt_dtbl_t **dtbl, struct bu_opt_desc *ds);

/**
 * Add an option description to a bu_opt_desc ptbl */
BU_EXPORT extern void bu_opt_desc_add(bu_opt_dtbl_t *dtbl, int ind,
	size_t min, size_t max, const char *shortopt,
	const char *longopt, bu_opt_arg_process_t arg_process,
	const char *shortopt_doc, const char *longopt_doc, const char *help_str);
/**
 * Remove option descriptions with index matching key from a bu_opt_desc ptbl */
BU_EXPORT extern void bu_opt_desc_del(bu_opt_dtbl_t *dtbl, int key);

/**
 * Remove option descriptions with either a shortopt or a longopt matching name
 * from a bu_opt_desc ptbl */
BU_EXPORT extern void bu_opt_desc_del_name(bu_opt_dtbl_t *dtbl, const char *name);

/**
 *  Free a bu_opt_dtbl_t.
 */
BU_EXPORT extern void bu_opt_desc_free(bu_opt_dtbl_t *tbl);


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
    bu_opt_desc_t desc_type;
    bu_opt_format_t format_type;
    int offset;
    int option_columns;
    int description_columns;
};

BU_EXPORT extern const char *bu_opt_describe(struct bu_opt_desc *ds, struct bu_opt_desc_opts *settings);
BU_EXPORT extern const char *bu_opt_describe_dtbl(bu_opt_dtbl_t *dtbl, struct bu_opt_desc_opts *settings);



/**
 * Parsed option data container
 */
struct bu_opt_data {
    struct bu_opt_desc *desc;
    unsigned int valid;
    const char *name;
    struct bu_ptbl *args;
    void *user_data;  /* place for arg_process to stash data */
};
#define BU_OPT_DATA_NULL {NULL, 0, NULL, NULL, NULL}
/**
 * Free a table of bu_opt_data results */
BU_EXPORT extern void bu_opt_data_free(bu_opt_data_t *data);

/**
 * Print a table of bu_opt_data structures.  Caller
 * is responsible for freeing return string. */
BU_EXPORT extern void bu_opt_data_print(bu_opt_data_t *data, const char *title);

/**
 * Convenience function for extracting args from a bu_opt_data container.
 * Provided as an easy way to get either the first arg:
 *
 * bu_opt_data_arg(d, 0)
 *
 * when it is known that there is only one argument while also allowing
 * for extraction of other args when multiples are present
 *
 * Returns NULL if the specified arg is not present. index starts from 0. */
BU_EXPORT extern const char *bu_opt_data_arg(struct bu_opt_data *d, size_t ind);

/**
 * Find and return a specific option from a bu_opt_data_t of options using an enum
 * integer as the lookup key.  Will only return an option if its valid entry
 * is set to 1.  A key value of -1 retrieves the bu_opt_data struct with the
 * unknown entries stored in its args table.
 */
BU_EXPORT extern struct bu_opt_data *bu_opt_find(int key, bu_opt_data_t *results);

/**
 * Find and return a specific option from a bu_opt_data_t of options using an option
 * string as the lookup key.  Will only return an option if its valid entry
 * is set to 1. A NULL value passed in for name retrieves the bu_opt_data struct with the
 * unknown entries stored in its args table.
 */
BU_EXPORT extern struct bu_opt_data *bu_opt_find_name(const char *name, bu_opt_data_t *results);

/**
 * TODO
 *
 * If an option has a message string associated with it, this function will
 * get it.  This works for both valid and invalid opts, to allow for error
 * message retrieval.  If multiple instances of a key are present, the msg
 * from the last instance is returned. */
BU_EXPORT extern const char *bu_opt_msg(int key, bu_opt_data_t *results);

/**
 * TODO
 *
 * If an option has a message string associated with it, this function will
 * get it.  This works for both valid and invalid opts, to allow for error
 * message retrieval.  If multiple instances of a name are present, the msg
 * from the last instance is returned. */
BU_EXPORT extern const char *bu_opt_msg_name(const char *name, bu_opt_data_t *results);




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
 */
BU_EXPORT extern int bu_opt_parse(bu_opt_data_t **results, struct bu_vls *msgs, int ac, const char **argv, struct bu_opt_desc *ds);
/**
 * Option parse an argv array defined as a space separated string.  This
 * is a convenience function that calls bu_opt_parse and also handles
 * breaking str down into a proper argv array. */
BU_EXPORT extern int bu_opt_parse_str(bu_opt_data_t **results, struct bu_vls *msgs, const char *str, struct bu_opt_desc *ds);


/**
 * For situations requiring a dynamic bu_opt_desc generation mechanism,
 * the procedure is slightly different
 *
 *  If we need dynamic definitions, need to take a slightly different approach
 *
 *  enum d1_opt_ind {D1_HELP, D1_VERBOSITY, D_MAX};
 *  struct bu_opt_desc d1[4] = {
 *      {D1_HELP, 0, 0, "h", "help", NULL, help_str},
 *      {D1_VERBOSITY, 0, 1, "v", "verbosity", &(d1_verbosity), "Set verbosity"},
 *      BU_OPT_DESC_NULL
 *  };

 *  bu_opt_dtbl_t *dtbl;
 *  bu_opt_data_t *results;

 *  bu_opt_desc_init(&dtbl, (struct bu_opt_desc *)&d1);
 *  bu_opt_desc_add(D1_HELP, 0, 0, "?", "",        NULL,      "-?", "",        "");
 *  bu_opt_desc_add(D_MAX + 1,   0, 0, "d", "dummy",   &(dtbl_d), "-d", "--dummy", "Dummy opt");
 *  bu_opt_parse_dtbl(&results, NULL, argc, argv, dtbl);
 */
BU_EXPORT extern int bu_opt_parse_dtbl(bu_opt_data_t **results, struct bu_vls *msgs, int ac, const char **argv, bu_opt_dtbl_t *dtbl);

/**
 * Option parse an argv array defined as a space separated string.  This
 * is a convenience function that calls bu_opt_parse_dtbl and also handles
 * breaking str down into a proper argv array. */
BU_EXPORT extern int bu_opt_parse_dtbl_str(bu_opt_data_t **results, struct bu_vls *msgs, const char *str, bu_opt_dtbl_t *dtbl);

/**
 * In situations where multiple options are present, the general rule is that
 * the last one in the list wins but there are situations where a program may
 * want to (say) accumulate multiple values past with multiple instances of the
 * same opt.  bu_opt_compact implements the "last opt is winner"
 * rule, guaranteeing that the last instance of any given option is the only one
 * in the results.  This is a destructive operation, so a copy should be made
 * of the input table before compacting if the user desires to examine the original
 * parsing data.
 */
BU_EXPORT extern void bu_opt_compact(bu_opt_data_t *results);

/**
 * Audit a set of results and remove any bu_opt_data entries that are not marked
 * as valid.
 */
BU_EXPORT extern void bu_opt_validate(bu_opt_data_t *results);


/* Standard option validators - if a custom option argument
 * validation isn't needed, the functions below can be
 * used for most valid data types. When data conversion is successful,
 * the user_data pointer in bu_opt_data will point to the results
 * of the string->[type] translation in order to allow a calling
 * program to use the int/long/etc. without having to repeat the
 * conversion.
 *
 * TODO - unimplemented */
BU_EXPORT extern int bu_opt_arg_int(struct bu_vls *msg, struct bu_opt_data *data);
BU_EXPORT extern int bu_opt_arg_long(struct bu_vls *msg, struct bu_opt_data *data);
BU_EXPORT extern int bu_opt_arg_bool(struct bu_vls *msg, struct bu_opt_data *data);
BU_EXPORT extern int bu_opt_arg_double(struct bu_vls *msg, struct bu_opt_data *data);


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
