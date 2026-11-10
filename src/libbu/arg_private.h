/*                     A R G _ P R I V A T E . H
 * BRL-CAD
 *
 * Internal adapters for the two public libbu option APIs.  This header is
 * deliberately not installed: bu_opt is a concise facade, while
 * bu_arg_schema is the full public grammar representation.
 */

#ifndef LIBBU_ARG_PRIVATE_H
#define LIBBU_ARG_PRIVATE_H

#include "bu/arg.h"

__BEGIN_DECLS

typedef int (*bu_cmd_opt_process_t)(struct bu_vls *msg, size_t argc,
	const char **argv, void *storage);

struct bu_cmd_parse_binding {
    void *storage;
    bu_cmd_opt_process_t opt_process;
};

enum bu_cmd_parse_internal_flags {
    BU_ARG_PARSE_INTERNAL_NONE = 0,
    BU_ARG_PARSE_INTERNAL_PASS_UNKNOWN = 1,
    BU_ARG_PARSE_INTERNAL_LEFTOVERS_FIRST = 2,
    BU_ARG_PARSE_INTERNAL_BU_OPT_SYNTAX = 4,
    BU_ARG_PARSE_INTERNAL_END_MARKER = 8
};

int _bu_arg_schema_parse_bound(const struct bu_arg_schema *schema, void *data,
	struct bu_vls *msg, int argc, const char *argv[],
	const struct bu_cmd_parse_binding *bindings, unsigned int flags);

int _bu_arg_schema_validate_structure(const struct bu_arg_schema *schema,
	size_t argc, const char **argv, size_t cursor_arg,
	struct bu_arg_validate_result *result,
	const struct bu_arg_option **active_option);

__END_DECLS

#endif /* LIBBU_ARG_PRIVATE_H */
