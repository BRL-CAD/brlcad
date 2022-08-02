/*                         O P T . H
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
 * This module implements a callback and assignment based mechanism
 * for generalized handling of option handling.  Functionally it is
 * intended to provide capabilities similar to getopt_long, Qt's
 * QCommandLineParser, and the tclap library.  Results are returned by
 * way of variable assignment, and function callbacks are used to
 * convert and validate argument strings.
 *
 * The bu_opt option parsing system does not make any use of global
 * values, unless a user defines an option definition array that
 * passes in pointers to global variables for setting.
 *
 * To set up a bu_opt parsing system, an array of bu_opt_desc (option
 * description) structures is defined and terminated with a
 * BU_OPT_DESC_NULL entry.  This array is then used by @link
 * bu_opt_parse @endlink to process an argv array.
 *
 * When defining a bu_opt_desc entry, the type of the set_var
 * assignment variable needed is determined by the arg_process
 * callback.  If no callback is present, set_var is expected to be an
 * integer that will be set to 1 if the option is present in the argv
 * string.
 *
 * There are two styles in which a bu_opt_desc array may be
 * initialized.  The first is very compact but in C89 based code
 * requires static variables as set_var entries, as seen in the
 * following example:
 *
 * @code
 * #define help_str "Print help and exit"
 * static int ph = 0;
 * static int i = 0;
 * static fastf_t f = 0.0;
 * struct bu_opt_desc opt_defs[] = {
 *     {"h", "help",    "",  NULL,            &ph, help_str},
 *     {"n", "num",     "#", &bu_opt_int,     &i,  "Read int"},
 *     {"f", "fastf_t", "#", &bu_opt_fastf_t, &f,  "Read float"},
 *     BU_OPT_DESC_NULL
 * };
 * @endcode
 *
 * This style of initialization is suitable for application programs,
 * but in libraries such static variables will preclude thread and
 * reentrant safety.  For libraries, the BU_OPT and BU_OPT_NULL macros
 * are used to construct a bu_opt_desc array that does not require
 * static variables:
 *
 * @code
 * #define help_str "Print help and exit"
 * int ph = 0;
 * int i = 0;
 * fastf_t f = 0.0;
 * struct bu_opt_desc opt_defs[4];
 * BU_OPT(opt_defs[0], "h", "help",    "",  NULL,            &ph, help_str);
 * BU_OPT(opt_defs[1], "n", "num",     "#", &bu_opt_int,     &i,  "Read int");
 * BU_OPT(opt_defs[2], "f", "fastf_t", "#", &bu_opt_fastf_t, &f,  "Read float");
 * BU_OPT_NULL(opt_defs[3]);
 * @endcode
 *
 * Given the option description array and argc/argv data, @link
 * bu_opt_parse @endlink will do the rest.  The design of @link
 * bu_opt_parse @endlink is to fail early when an invalid option
 * situation is encountered, so code using this system needs to be
 * ready to handle such cases.
 *
 * For generating descriptive help strings from a bu_opt_desc array
 * use the @link bu_opt_describe @endlink function, which supports
 * multiple output styles and formats.
 */
/** @{ */
/** @file bu/opt.h */

/**
 * Callback function signature for bu_opt_desc argument processing
 * functions. Any user defined argument processing function should
 * match this signature and return values as documented below.
 *
 * @param[out]    msg     If not NULL, callback messages (usually
 *                        error descriptions) may be appended here.
 * @param[in]     argc    Number of arguments in argv.
 * @param[in]     argv    @em All arguments that follow the option flag.
 * @param[in, out] set_var The value specified in the associated bu_opt_desc.
 *
 * @returns
 * Val | Interpretation
 * --- | --------------
 * -1  | Invalid argument encountered, or argument expected but not found.
 *  0  | No argument processed (not an error.)
 * >0  | Number of argv elements used in valid argument processing.
 *
 * An example user-defined argument processing function:
 * @code
 * static int
 * parse_opt_mode(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
 * {
 *     int ret, mode;
 *
 *     BU_OPT_CHECK_ARGV0(msg, argc, argv, "mode");
 *
 *     ret = bu_opt_int(msg, argc, argv, set_var);
 *     mode = *(int *)set_var;
 *
 *     if (mode < 0 || mode > 2) {
 *         ret = -1;
 *         if (msg) {
 *             bu_vls_printf(msg, "Error: mode must be 0, 1, or 2.");
 *         }
 *     }
 *     return ret;
 * }
 * @endcode
 */
typedef int (*bu_opt_arg_process_t)(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);


/**
 * A common task when writing bu_opt_arg_process_t validators is to
 * check the first argument of the argv array.  This macro
 * encapsulates that into a standard check.
 */
#define BU_OPT_CHECK_ARGV0(_msg, _argc, _argv, _opt_name) do { \
	if ((_argc) < 1 || !(_argv) || !(_argv)[0] || (_argv)[0][0] == '\0') { \
	    if ((_msg)) { \
		bu_vls_printf((_msg), "ERROR: missing required argument: %s\n", (_opt_name)); \
	    } \
	    return -1; \
	} \
    } while (0)


/**
 * @brief
 * "Option description" structure.
 *
 * Arrays of this structure are used to define command line options.
 */
struct bu_opt_desc {
    const char *shortopt;             /**< @brief "Short" option (i.e. -h for help option) */
    const char *longopt;              /**< @brief "Long" option (i.e. --help for help option) */
    const char *arg_helpstr;          /**< @brief Documentation describing option argument, if any (i.e. "file" in --input file)*/
    bu_opt_arg_process_t arg_process; /**< @brief Argument processing function pointer */
    void *set_var;                    /**< @brief Pointer to the variable or structure that collects this option's results */
    const char *help_string;          /**< @brief Option description */
};


/** Convenience initializer for NULL bu_opt_desc array terminator */
#define BU_OPT_DESC_NULL {NULL, NULL, NULL, NULL, NULL, NULL}

/** Macro for assigning values to bu_opt_desc array entries. */
#define BU_OPT(_desc, _so, _lo, _ahelp, _aprocess, _var, _help) do { \
	(_desc).shortopt = _so; \
	(_desc).longopt = _lo; \
	(_desc).arg_helpstr = _ahelp; \
	(_desc).arg_process = _aprocess; \
	(_desc).set_var = (void *)_var;  \
	(_desc).help_string = _help; \
    } while (0)

/** Convenience macro for setting a bu_opt_desc struct to BU_OPT_DESC_NULL */
#define BU_OPT_NULL(_desc) do { \
	(_desc).shortopt = NULL; \
	(_desc).longopt = NULL; \
	(_desc).arg_helpstr = NULL; \
	(_desc).arg_process = NULL; \
	(_desc).set_var = NULL; \
	(_desc).help_string = NULL; \
    } while (0)


/**
 * Parse @p argv array using option descriptions.
 *
 * The bu_opt_desc array @p ds must be terminated with
 * BU_OPT_DESC_NULL.
 *
 * @returns
 * Val | Interpretation
 * --- | --------------
 * -1  | Fatal error in parsing.  Program must decide to recover or exit.
 *  0  | All argv options handled.
 *  >0 | Number of unused argv entries returned at the beginning of the argv array.
 *
 * @param[out] msgs    will collect any informational messages
 *                     generated by the parser (typically used for
 *                     error reporting)
 * @param[in] ac       number of input arguments in argv
 * @param[in, out]     argv a return value >0 indicates that argv has been
 *                     reordered to move the indicated number of
 *                     unused args to the beginning of the array
 * @param[in] ds       option structure
 */
BU_EXPORT extern int bu_opt_parse(struct bu_vls *msgs, size_t ac, const char **argv, const struct bu_opt_desc *ds);


/** Output format options for bu_opt documentation generation */
typedef enum {
    BU_OPT_ASCII,
    BU_OPT_DOCBOOK /* TODO */
} bu_opt_format_t;


/**
 * Construct a textual description of the options defined by the
 * array.
 *
 * The structure is as follows:
 *
 * Offset    Options      Descriptions
 * ******--------------*********************
 *       --test-option This is a test option
 *
 * Opt_col specifies how wide the options column is, and desc_cols
 * specifies how wide the description column is.
 *
 * This structure is currently experimental and likely will change as
 * we find out what is needed.
 */

/* TODO - support actually using the struct... */
struct bu_opt_desc_opts {
    bu_opt_format_t format;
    int offset;
    int option_columns;
    int description_columns;
    /* The application needs to inform the printer if certain options
     * have special status */
    struct bu_opt_desc *required;
    struct bu_opt_desc *repeated;
    struct bu_opt_desc *optional;
    /* Report the longopt version(s) of an option even when it has a
     * shortopt */
    int show_all_longopts;
    /* It may not be desirable to print all options.  The caller may
     * supply a space separated list of options to accept or reject.
     * Only one list may be supplied at a time.  Filtering is either
     * accept or reject, not both at once.*/
    const char *accept;
    const char *reject;
};


/**
 * initialize an bu_opt_desc_opts struct.
 *
 * Out of the box, assume an overall column width of 80 characters.
 * Given that width, we do a default partitioning.  The first three
 * numbers tell the option printer what column breakout to use for
 * various components of the lines:
 *
 * offset = 2 is the default column offsetting from the left edge
 * option_columns = The next 28 columns are for printing the option
 * and its aliases description_columns = The remaining 50 columns are
 * for human readable explanations of the option
 *
 * These values were chosen after some casual
 * experimentation/observation to see what "looked right" for Linux
 * command line option printing - if better values (perhaps based on
 * some OS convention or standard) are available, it would be better
 * to use those and document their source.
 */
#define BU_OPT_DESC_OPTS_INIT_ZERO { BU_OPT_ASCII, 2, 28, 50, NULL, NULL, NULL, 1, NULL, NULL }

/**
 *
 * Using the example definition:
 *
 * @code
 * struct bu_opt_desc opt_defs[] = {
 *     {"h", "help",    "",  NULL,            &ph, "Print help string and exit."},
 *     {"n", "num",     "#", &bu_opt_int,     &i,  "Read int"},
 *     {"f", "fastf_t", "#", &bu_opt_fastf_t, &f,  "Read float"},
 *     BU_OPT_DESC_NULL
 * };
 * @endcode
 *
 * bu_opt_describe would generate the following help string by
 * default:
 *
 @verbatim
 -h, --help                    Print help string and exit.
 -n #, --num #                 Read int
 -f #, --fastf_t #             Read float
 @endverbatim
 *
 * When multiple options use the same set_var to capture their effect,
 * they are considered aliases for documentation purposes.  For
 * example, if we add multiple aliases to the help option and make it
 * more elaborate:
 *
 * @code
 * #define help_str "Print help and exit. If a type is specified to --help, print help specific to that type"
 * struct help_struct hs;
 * struct bu_opt_desc opt_defs[] = {
 *     {"h", "help",    "[type]",  &hfun, &hs, help_str},
 *     {"H", "HELP",    "[type]",  &hfun, &hs, help_str},
 *     {"?", "",        "[type]",  &hfun, &hs, help_str},
 *     {"n", "num",     "#", &bu_opt_int,     &i,  "Read int"},
 *     {"f", "fastf_t", "#", &bu_opt_fastf_t, &f,  "Read float"},
 *     BU_OPT_DESC_NULL
 * };
 * @endcode
 *
 * the generated help string reflects this:
 *
 @verbatim
 -h [type], -H [type], -? [type], --help [type], --HELP [type]
 Print help and exit. If a type is specified to
 --help, print help specific to that type
 -n #, --num #                 Read int
 -f #, --fastf_t #             Read float
 @endverbatim
 *
 * @returns
 * The generated help string. Note that the string uses allocated
 * memory and it is the responsibility of the caller to free it with
 * @link bu_free @endlink.
 */
BU_EXPORT extern char *bu_opt_describe(const struct bu_opt_desc *ds, struct bu_opt_desc_opts *settings);


/** @} */

/** @addtogroup bu_opt_arg_process
 *
 * Standard option validators.  If a custom option argument validation isn't
 * needed, the functions below can be used for most valid data types.  When
 * data conversion is successful, the user_data pointer in bu_opt_data will
 * point to the results of the string->[type] translation in order to allow a
 * calling program to use the int/long/etc. without having to repeat the
 * conversion.
 *
 * These functions should return -1 if there was a problem processing the
 * value, and the number of argv entries processed otherwise.  (Some validators
 * such as bu_opt_color may read different numbers of args depending on what is
 * found so calling code can't assume a successful validation will always
 * return 1.  Hence -1 is the error return - option validation will never
 * "revert" previously processed argv entries.)
 */
/** @{ */

/**
 * Process 1 argument to set a boolean type
 */
BU_EXPORT extern int bu_opt_bool(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Process 1 argument to set an integer
 */
BU_EXPORT extern int bu_opt_int(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Process 1 argument to set a long
 */
BU_EXPORT extern int bu_opt_long(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);
/**
 * Process 1 argument (hex style) to set a long
 */
BU_EXPORT extern int bu_opt_long_hex(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Process 1 argument to set a @link fastf_t @endlink (either a float
 * or a double, depending on how BRL-CAD was compiled)
 */
BU_EXPORT extern int bu_opt_fastf_t(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Process 1 argument to set a char pointer (uses the original argv
 * string, does not make a copy)
 */
BU_EXPORT extern int bu_opt_str(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Process 1 argument to append to a vls (places a space before the
 * new entry if the target vls is not empty)
 */
BU_EXPORT extern int bu_opt_vls(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Process 1 or 3 arguments to set a bu_color
 */
BU_EXPORT extern int bu_opt_color(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/**
 * Process 1 or 3 arguments to set a vect_t
 */
BU_EXPORT extern int bu_opt_vect_t(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

/** @} */

/**
 * Process 0 arguments, incrementing the value held by a long.  This is
 * useful for situations where multiple specifications of identical options are
 * intended to change output, such as multiple -v options to increase
 * verbosity.
 */
BU_EXPORT extern int bu_opt_incr_long(struct bu_vls *msg, size_t argc, const char **argv, void *set_var);

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
