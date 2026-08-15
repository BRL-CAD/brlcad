/*                 C M D S C H E M A _ P R I V A T E . H
 * BRL-CAD
 *
 * Internal adapters for the two public libbu option APIs.  This header is
 * deliberately not installed: bu_opt is a concise facade, while
 * bu_cmd_schema is the full public grammar representation.
 */

#ifndef LIBBU_CMDSCHEMA_PRIVATE_H
#define LIBBU_CMDSCHEMA_PRIVATE_H

#include "bu/cmdschema.h"

__BEGIN_DECLS

typedef int (*bu_cmd_opt_process_t)(struct bu_vls *msg, size_t argc,
	const char **argv, void *storage);

struct bu_cmd_parse_binding {
    void *storage;
    bu_cmd_opt_process_t opt_process;
};

enum bu_cmd_parse_internal_flags {
    BU_CMD_PARSE_INTERNAL_NONE = 0,
    BU_CMD_PARSE_INTERNAL_PASS_UNKNOWN = 1,
    BU_CMD_PARSE_INTERNAL_LEFTOVERS_FIRST = 2,
    BU_CMD_PARSE_INTERNAL_BU_OPT_SYNTAX = 4,
    BU_CMD_PARSE_INTERNAL_END_MARKER = 8
};

int _bu_cmd_schema_parse_bound(const struct bu_cmd_schema *schema, void *data,
	struct bu_vls *msg, int argc, const char *argv[],
	const struct bu_cmd_parse_binding *bindings, unsigned int flags);

int _bu_cmd_schema_validate_structure(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result,
	const struct bu_cmd_option **active_option);

__END_DECLS

#endif /* LIBBU_CMDSCHEMA_PRIVATE_H */
