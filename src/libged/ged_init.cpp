/*                     G E D _ I N I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2026 United States Government as represented by
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
/** @file init.c
 *
 * NOTE: as this init is global to ALL applications before main(), care must be
 * taken to not write to STDOUT or STDERR or app output may be corrupted,
 * signals can be raised, or worse.
 *
 * Static constructors (REGISTER_GED_COMMAND) call ged_register_command before libged_init()
 * ordering isn't guaranteed across TUs, so ged_register_command is fully lazy-initializing.
 *
 * libged_init() performs a one-time plugin scan unless GED_NO_PLUGIN_SCAN=1.
 */

#include "common.h"

#include <algorithm>
#include <cctype>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "bu/app.h"
#include "bu/dylib.h"
#include "bu/file.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "ged.h"

#define BU_PLUGIN_IMPLEMENTATION
#include "./include/plugin.h"

static struct bu_vls init_msgs = BU_VLS_INIT_ZERO;
static bool ged_initialized = false;
static bool ged_shutting_down = false;
static size_t ged_active_references = 0;
static thread_local size_t ged_thread_active_references = 0;

/* Command implementations and their metadata are one logical registry.  The
 * generalized command table has its own lock, but a libged transaction must
 * span that table, the metadata maps, compatibility aliases, diagnostics, and
 * retained plugin handles. */
static std::recursive_mutex &
ged_registry_mutex()
{
    static std::recursive_mutex mutex;
    return mutex;
}


static std::condition_variable_any &
ged_registry_condition()
{
    static std::condition_variable_any condition;
    return condition;
}


static std::vector<void *> &
ged_plugin_handles()
{
    static std::vector<void *> handles;
    return handles;
}

static std::vector<std::string>
ged_opt_rule_words(const char *words)
{
    std::vector<std::string> result;
    std::string text = words ? words : "";
    size_t pos = 0;

    while (pos < text.size()) {
	while (pos < text.size() && std::isspace((unsigned char)text[pos]))
	    pos++;
	size_t start = pos;
	while (pos < text.size() && !std::isspace((unsigned char)text[pos]))
	    pos++;
	if (start != pos)
	    result.push_back(text.substr(start, pos - start));
    }
    return result;
}


/** Owned normalization of one flat ged_opt_rule declaration. */
struct ged_compiled_opt_rules {
    struct owned_constraint {
	struct bu_cmd_constraint value = {};
	std::vector<std::string> names;
	std::vector<const char *> options;
    };
    struct owned_db_completion {
	struct ged_opt_db_completion value = {};
	std::deque<std::string> type_names;
	std::vector<struct ged_opt_db_type_case> types;
    };

    ged_opt_spec spec = {};
    ged_opt_meta metadata = {};
    std::deque<std::string> value_names;
    std::vector<struct bu_opt_value_spec> values;
    std::deque<std::string> semantic_names;
    std::vector<ged_opt_semantic> semantics;
    std::vector<ged_opt_form> forms;
    std::vector<std::unique_ptr<owned_constraint>> owned_constraints;
    std::vector<struct bu_cmd_constraint> constraints;
    std::vector<std::unique_ptr<owned_db_completion>> owned_db_completions;
    std::vector<struct ged_opt_db_completion> db_completions;

    int compile(const ged_opt_spec *decl, struct bu_vls *msgs)
    {
	struct pending_db_type {
	    ged_opt_db_type_rule rule;
	    size_t index;
	};
	std::vector<pending_db_type> pending_db_types;
	bu_cmd_schema_validate_t schema_validate = NULL;
	bu_cmd_schema_context_validate_t context_validate = NULL;

	if (!decl || !decl->rules || !decl->rule_count)
	    return -1;
	if (decl->forms || decl->metadata) {
	    if (msgs)
		bu_vls_printf(msgs,
		    "%s:%d: command '%s': flat rules cannot be combined with legacy forms or metadata\n",
		    decl->source_file ? decl->source_file : "(unknown)", decl->source_line,
		    decl->name ? decl->name : "(unknown)");
	    return -1;
	}
	auto fail = [decl, msgs](size_t index, const char *reason) {
	    if (msgs)
		bu_vls_printf(msgs,
		    "%s:%d: command '%s': flat option rule %zu: %s\n",
		    decl->source_file ? decl->source_file : "(unknown)", decl->source_line,
		    decl->name ? decl->name : "(unknown)", index, reason);
	    return -1;
	};

	spec = *decl;
	spec.rules = NULL;
	spec.rule_count = 0;
	for (size_t ri = 0; ri < decl->rule_count; ri++) {
	    const ged_opt_rule &rule = decl->rules[ri];
	    if (rule.kind == GED_OPT_RULE_END) {
		if (ri + 1 != decl->rule_count)
		    return fail(ri, "visual terminator must be the final counted rule");
		continue;
	    }
	    switch (rule.kind) {
		case GED_OPT_RULE_VALUE: {
		    std::vector<std::string> names = ged_opt_rule_words(rule.value.option);
		    if (names.empty())
			return fail(ri, "value metadata has no option names");
		    for (const std::string &name : names) {
			value_names.push_back(name);
			struct bu_opt_value_spec value = rule.value;
			value.option = value_names.back().c_str();
			values.push_back(value);
		    }
		    break;
		}
		case GED_OPT_RULE_SEMANTIC: {
		    std::vector<std::string> names = ged_opt_rule_words(rule.semantic.option);
		    if (names.empty())
			return fail(ri, "semantic metadata has no option names");
		    for (const std::string &name : names) {
			semantic_names.push_back(name);
			ged_opt_semantic semantic = rule.semantic;
			semantic.option = semantic_names.back().c_str();
			semantics.push_back(semantic);
		    }
		    break;
		}
		case GED_OPT_RULE_FORM:
		    forms.push_back(rule.form);
		    break;
		case GED_OPT_RULE_CONSTRAINT: {
		    std::unique_ptr<owned_constraint> constraint(new owned_constraint());
		    constraint->names = ged_opt_rule_words(rule.constraint.option_names);
		    if (constraint->names.empty())
			return fail(ri, "constraint has no option names");
		    constraint->options.reserve(constraint->names.size() + 1);
		    for (const std::string &option : constraint->names)
			constraint->options.push_back(option.c_str());
		    constraint->options.push_back(NULL);
		    constraint->value.kind = rule.constraint.kind;
		    constraint->value.condition = rule.constraint.condition;
		    constraint->value.options = constraint->options.data();
		    constraint->value.min_count = rule.constraint.min_count;
		    constraint->value.max_count = rule.constraint.max_count;
		    constraint->value.hint = rule.constraint.hint;
		    owned_constraints.push_back(std::move(constraint));
		    break;
		}
		case GED_OPT_RULE_DB_COMPLETION: {
		    if (BU_STR_EMPTY(rule.db_completion.operand))
			return fail(ri, "database completion has no operand name");
		    for (const std::unique_ptr<owned_db_completion> &prior : owned_db_completions)
			if (BU_STR_EQUAL(prior->value.operand, rule.db_completion.operand))
			    return fail(ri, "duplicate database completion operand");
		    std::unique_ptr<owned_db_completion> policy(new owned_db_completion());
		    policy->value = rule.db_completion;
		    policy->value.type_cases = NULL;
		    owned_db_completions.push_back(std::move(policy));
		    break;
		}
		case GED_OPT_RULE_DB_TYPE:
		    if (BU_STR_EMPTY(rule.db_type.operand) ||
			ged_opt_rule_words(rule.db_type.option_names).empty())
			return fail(ri, "database type refinement is missing an operand or option name");
		    pending_db_types.push_back({rule.db_type, ri});
		    break;
		case GED_OPT_RULE_SCHEMA_VALIDATE:
		    if (!rule.validate || schema_validate)
			return fail(ri, "schema validator is NULL or already defined");
		    schema_validate = rule.validate;
		    break;
		case GED_OPT_RULE_CONTEXT_VALIDATE:
		    if (!rule.context_validate || context_validate)
			return fail(ri, "context validator is NULL or already defined");
		    context_validate = rule.context_validate;
		    break;
		case GED_OPT_RULE_END:
		    break;
		default:
		    return fail(ri, "unknown rule kind");
	    }
	}
	for (const pending_db_type &pending : pending_db_types) {
	    const ged_opt_db_type_rule &type_rule = pending.rule;
	    owned_db_completion *policy = NULL;
	    for (const std::unique_ptr<owned_db_completion> &candidate : owned_db_completions)
		if (BU_STR_EQUAL(candidate->value.operand, type_rule.operand)) {
		    policy = candidate.get();
		    break;
		}
	    if (!policy)
		return fail(pending.index,
		    "database type refinement has no matching completion policy");
	    for (const std::string &name : ged_opt_rule_words(type_rule.option_names)) {
		policy->type_names.push_back(name);
		struct ged_opt_db_type_case type_case = {
		    policy->type_names.back().c_str(), type_rule.scope
		};
		policy->types.push_back(type_case);
	    }
	}

	if (!values.empty()) {
	    const struct bu_opt_value_spec terminal = BU_OPT_VALUE_SPEC_NULL;
	    values.push_back(terminal);
	    metadata.option_values = values.data();
	}
	if (!semantics.empty()) {
	    const ged_opt_semantic terminal = GED_OPT_SEMANTIC_NULL;
	    semantics.push_back(terminal);
	    metadata.option_semantics = semantics.data();
	}
	if (!forms.empty()) {
	    const ged_opt_form terminal = GED_OPT_FORM_NULL;
	    forms.push_back(terminal);
	    spec.forms = forms.data();
	}
	if (!owned_constraints.empty()) {
	    constraints.reserve(owned_constraints.size() + 1);
	    for (const std::unique_ptr<owned_constraint> &constraint : owned_constraints)
		constraints.push_back(constraint->value);
	    const struct bu_cmd_constraint terminal = BU_CMD_CONSTRAINT_NULL;
	    constraints.push_back(terminal);
	    metadata.constraints = constraints.data();
	}
	if (!owned_db_completions.empty()) {
	    db_completions.reserve(owned_db_completions.size() + 1);
	    for (const std::unique_ptr<owned_db_completion> &policy : owned_db_completions) {
		if (!policy->types.empty()) {
		    const struct ged_opt_db_type_case terminal = GED_OPT_DB_TYPE_NULL;
		    policy->types.push_back(terminal);
		    policy->value.type_cases = policy->types.data();
		}
		db_completions.push_back(policy->value);
	    }
	    const struct ged_opt_db_completion terminal = GED_OPT_DB_COMPLETION_NULL;
	    db_completions.push_back(terminal);
	    metadata.db_completions = db_completions.data();
	}
	metadata.validate = schema_validate;
	metadata.context_validate = context_validate;
	if (metadata.option_values || metadata.option_semantics || metadata.constraints ||
	    metadata.validate || metadata.context_validate || metadata.db_completions)
	    spec.metadata = &metadata;
	return 0;
    }
};

struct ged_metadata_registry {
    struct owned_operand_group {
	std::string name;
	std::string help;
	std::vector<struct bu_cmd_operand> roles;
	std::vector<std::string> role_names;
	std::vector<std::string> role_help;
	std::vector<std::string> role_providers;
	std::vector<std::vector<std::string>> role_keyword_strings;
	std::vector<std::vector<const char *>> role_keywords;
    };
    struct owned_operand_layout {
	std::vector<struct bu_cmd_operand> operands;
	std::vector<std::string> operand_names;
	std::vector<std::string> operand_help;
	std::vector<std::string> operand_providers;
	std::vector<std::vector<std::string>> operand_keyword_strings;
	std::vector<std::vector<const char *>> operand_keywords;
	std::vector<std::unique_ptr<owned_operand_group>> owned_groups;
	std::vector<struct bu_cmd_operand_group> groups;
    };
    struct owned_opt_form {
	std::string name;
	std::string help;
	std::vector<std::string> option_names;
	std::vector<const char *> options;
	owned_operand_layout layout;
    };
    struct owned_opt_schema {
	struct bu_cmd_schema schema = {};
	owned_operand_layout layout;
	struct bu_opt_cmd options = BU_OPT_CMD_INIT_ZERO;
	std::vector<std::unique_ptr<owned_opt_form>> owned_forms;
	std::vector<struct bu_cmd_schema_case> cases;
	std::string schema_name;
	ged_opt_spec spec = {};
	ged_opt_meta metadata = {};
	std::unique_ptr<ged_compiled_opt_rules> compiled_rules;
	struct bu_opt_desc *descs = NULL;

	~owned_opt_schema()
	{
	    bu_opt_cmd_clear(&options);
	    if (descs)
		bu_free(descs, "built bu_opt descriptors");
	}
    };
    std::mutex mutex;
    std::map<std::string, const struct bu_cmd_schema *> native_schemas;
    std::map<std::string, const struct ged_cmd_grammar *> grammars;
    std::map<std::string, std::unique_ptr<owned_opt_schema>> opt_schemas;
    std::unordered_map<const struct bu_cmd_schema *, const ged_opt_spec *> opt_schema_specs;
    std::unordered_map<const struct bu_cmd_schema *, const struct bu_opt_desc *> opt_schema_descs;
};


static struct ged_metadata_registry &
ged_metadata()
{
    /* REGISTER_GED_COMMAND constructors in other translation units may arrive
     * before any other libged initialization.  A function-local registry is
     * initialized on first use and therefore has no cross-TU ordering hazard. */
    static struct ged_metadata_registry registry;
    return registry;
}


static size_t
ged_metadata_count_locked(const std::string &key)
{
    struct ged_metadata_registry &registry = ged_metadata();
    std::lock_guard<std::mutex> guard(registry.mutex);
    return (registry.native_schemas.find(key) != registry.native_schemas.end() ? 1 : 0) +
	(registry.grammars.find(key) != registry.grammars.end() ? 1 : 0);
}


static void
ged_metadata_erase_locked(const std::string &key)
{
    struct ged_metadata_registry &registry = ged_metadata();
    std::lock_guard<std::mutex> guard(registry.mutex);
    auto native = registry.native_schemas.find(key);
    if (native != registry.native_schemas.end()) {
	registry.opt_schema_specs.erase(native->second);
	registry.opt_schema_descs.erase(native->second);
	registry.native_schemas.erase(native);
    }
    registry.grammars.erase(key);
    registry.opt_schemas.erase(key);
}


static bool
ged_command_key_valid(const std::string &key)
{
    if (key.empty())
	return false;
    for (unsigned char c : key)
	if (std::isspace(c))
	    return false;
    return true;
}

/* -------------------------------------------------------------------------- */
/* Public Registry API                                                        */
/* -------------------------------------------------------------------------- */

extern "C" GED_EXPORT int
ged_register_command(const struct ged_cmd *cmd)
{
    int ret;

    if (!cmd || !cmd->i || !cmd->i->cname || !cmd->i->cmd) return -1;

    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());

    std::string key(cmd->i->cname);

    if (ged_shutting_down || !ged_command_key_valid(key) ||
	key.compare(0, 6, "_mged_") == 0)
	return -1;

    size_t metadata_kinds = (cmd->i->native_schema ? 1 : 0) +
	(cmd->i->grammar ? 1 : 0) + (cmd->i->opt_spec ? 1 : 0);
    if (key.empty() || metadata_kinds > 1) {
	bu_vls_printf(&init_msgs,
	    "command '%s': at most one metadata representation may be declared\n",
	    key.empty() ? "(unnamed)" : key.c_str());
	return -1;
    }

    std::string mged_key = std::string("_mged_") + key;

    /* The implementation, compatibility alias, and metadata participate in
     * the same first-wins decision. */
    if (bu_plugin_cmd_exists(key.c_str()) ||
	bu_plugin_cmd_exists(mged_key.c_str()) || ged_metadata_count_locked(key)) {
	return 1;
    }

    /* Validate and publish metadata before making the command executable.
     * This prevents a malformed declaration from silently producing a command
     * with no completion, validation, or schema description. */
    if (cmd->i->native_schema) {
	ret = ged_register_command_native_schema(key.c_str(), cmd->i->native_schema);
	if (ret != 0) {
	    bu_vls_printf(&init_msgs, "command '%s': native schema registration failed\n",
		key.c_str());
	    return -1;
	}
    }
    if (cmd->i->grammar) {
	ret = ged_register_command_grammar(key.c_str(), cmd->i->grammar);
	if (ret != 0) {
	    bu_vls_printf(&init_msgs, "command '%s': grammar registration failed\n",
		key.c_str());
	    return -1;
	}
    }
    if (cmd->i->opt_spec) {
	ret = ged_opt_register(key.c_str(), cmd->i->opt_spec);
	if (ret != 0) {
	    bu_vls_printf(&init_msgs, "command '%s': compact schema registration failed\n",
		key.c_str());
	    return -1;
	}
    }

    /* Register the public spelling and compatibility alias as one batch.  If
     * the generalized registry rejects the batch, remove the metadata which
     * was staged while this transaction lock excluded libged readers. */
    const bu_plugin_cmd commands[] = {
	{key.c_str(), cmd->i->cmd},
	{mged_key.c_str(), cmd->i->cmd}
    };
    ret = bu_plugin_cmd_register_batch(commands, 2);
    if (ret != 0) {
	ged_metadata_erase_locked(key);
	bu_vls_printf(&init_msgs, "command '%s': command registration failed\n",
	    key.c_str());
	return ret;
    }

    return 0;
}


static const struct bu_opt_value_spec *
ged_opt_values(const ged_opt_spec *spec)
{
    if (!spec)
	return NULL;
    return spec->metadata ? spec->metadata->option_values : NULL;
}


static const ged_opt_semantic *
ged_opt_semantics(const ged_opt_spec *spec)
{
    if (!spec)
	return NULL;
    return spec->metadata ? spec->metadata->option_semantics : NULL;
}


static const struct ged_opt_db_completion *
ged_opt_db_completions(const ged_opt_spec *spec)
{
    if (!spec)
	return NULL;
    return spec->metadata ? spec->metadata->db_completions : NULL;
}


static const struct bu_cmd_constraint *
ged_opt_constraints(const ged_opt_spec *spec)
{
    if (!spec)
	return NULL;
    return spec->metadata ? spec->metadata->constraints : NULL;
}


static bu_cmd_schema_validate_t
ged_opt_validate(const ged_opt_spec *spec)
{
    if (!spec)
	return NULL;
    return spec->metadata ? spec->metadata->validate : NULL;
}


static bu_cmd_schema_context_validate_t
ged_opt_context_validate(const ged_opt_spec *spec)
{
    if (!spec)
	return NULL;
    return spec->metadata ? spec->metadata->context_validate : NULL;
}


static size_t
ged_opt_desc_match_count(const struct bu_opt_desc *descs, const char *name)
{
    size_t count = 0;
    if (!descs || BU_STR_EMPTY(name))
	return 0;
    for (size_t i = 0; descs[i].shortopt || descs[i].longopt ||
	descs[i].arg_helpstr || descs[i].arg_process || descs[i].set_var ||
	descs[i].help_string; i++) {
	if ((!BU_STR_EMPTY(descs[i].shortopt) &&
		BU_STR_EQUAL(descs[i].shortopt, name)) ||
	    (!BU_STR_EMPTY(descs[i].longopt) &&
		BU_STR_EQUAL(descs[i].longopt, name)))
	    count++;
    }
    return count;
}


static bu_cmd_validate_state_t
ged_opt_native_state(bu_opt_validate_state_t state)
{
    switch (state) {
	case BU_OPT_VALIDATE_VALID: return BU_CMD_VALIDATE_VALID;
	case BU_OPT_VALIDATE_INVALID: return BU_CMD_VALIDATE_INVALID;
	case BU_OPT_VALIDATE_INCOMPLETE: return BU_CMD_VALIDATE_INCOMPLETE;
	case BU_OPT_VALIDATE_UNKNOWN:
	default: return BU_CMD_VALIDATE_UNKNOWN;
    }
}


static const struct bu_cmd_arg_shape ged_text_pattern_shape = {
    BU_CMD_ARG_SHAPE_RANGE_PATTERN, 1, 1, "pattern", NULL, NULL
};


struct ged_text_operand {
    std::string name;
    std::string provider;
    std::vector<std::string> keywords;
    bu_cmd_value_t type = BU_CMD_VALUE_UNKNOWN;
    bu_cmd_value_validate_t validate = NULL;
    const struct bu_cmd_arg_shape *shape = NULL;
    size_t min_count = 1;
    size_t max_count = 1;
    struct bu_cmd_value_range range = BU_CMD_VALUE_RANGE_NONE;
};


struct ged_text_group {
    std::string name;
    std::string help;
    std::vector<ged_text_operand> roles;
    size_t min_count = 1;
    size_t max_count = 1;
};


static int
ged_opt_syntax_error(struct bu_vls *msgs, const ged_opt_spec *spec, const char *command,
	const char *syntax, size_t offset,
	const char *message)
{
	if (msgs)
	    bu_vls_printf(msgs, "%s:%d: command '%s' syntax error at byte %zu: %s\n  %s\n",
	spec && spec->source_file ? spec->source_file : "(unknown)",
	spec ? spec->source_line : 0,
	command ? command : "(unknown)", offset, message,
	syntax ? syntax : "(null)");
    return -1;
}


static int
ged_opt_syntax_uint(const std::string &text, size_t *value)
{
    size_t parsed = 0;

    if (!value || text.empty())
	return -1;
    for (char c : text) {
	if (c < '0' || c > '9')
	    return -1;
	size_t digit = (size_t)(c - '0');
	if (parsed > (((size_t)-1) - digit) / 10)
	    return -1;
	parsed = parsed * 10 + digit;
    }
    *value = parsed;
    return 0;
}


static bu_cmd_value_t
ged_opt_syntax_type(const std::string &name, const char **default_provider)
{
    if (default_provider)
	*default_provider = NULL;
    if (name == "bool") return BU_CMD_VALUE_BOOL;
    if (name == "int" || name == "integer" || name == "positive-int" ||
	name == "nonnegative-int") return BU_CMD_VALUE_INTEGER;
    if (name == "hex-int") return BU_CMD_VALUE_HEX_INTEGER;
    if (name == "long") return BU_CMD_VALUE_LONG;
    if (name == "hex-long") return BU_CMD_VALUE_HEX_LONG;
    if (name == "number" || name == "positive-number" ||
	name == "nonnegative-number") return BU_CMD_VALUE_NUMBER;
    if (name == "char") return BU_CMD_VALUE_CHAR;
    if (name == "vector" || name == "vector3") return BU_CMD_VALUE_VECTOR;
    if (name == "matrix") return BU_CMD_VALUE_MATRIX;
    if (name == "color" || name == "rgb") return BU_CMD_VALUE_COLOR;
    if (name == "keyword") return BU_CMD_VALUE_KEYWORD;
    if (name == "string" || name == "pattern") return BU_CMD_VALUE_STRING;
    if (name == "vls") return BU_CMD_VALUE_VLS;
    if (name == "raw") return BU_CMD_VALUE_RAW;
    if (name == "custom") return BU_CMD_VALUE_CUSTOM;
    if (name == "object") {
	if (default_provider) *default_provider = "ged.db_object";
	return BU_CMD_VALUE_DB_OBJECT;
    }
    if (name == "path") {
	if (default_provider) *default_provider = "ged.db_path";
	return BU_CMD_VALUE_DB_PATH;
    }
    if (name == "file") {
	if (default_provider) *default_provider = "ged.file_path";
	return BU_CMD_VALUE_FILE;
    }
    return BU_CMD_VALUE_UNKNOWN;
}


static int
ged_opt_syntax_parse_flat(struct bu_vls *msgs, const ged_opt_spec *spec, const char *command,
	const char *syntax,
	std::vector<ged_text_operand> &operands, bu_cmd_parse_policy_t *policy)
{
    size_t pos = 0;
    int saw_operand = 0;

    if (!syntax || !policy)
	return ged_opt_syntax_error(msgs, spec, command, syntax, 0, "missing syntax or result storage");
    *policy = BU_CMD_PARSE_INTERSPERSED;
    while (syntax[pos]) {
	while (syntax[pos] && std::isspace((unsigned char)syntax[pos]))
	    pos++;
	if (!syntax[pos])
	    break;
	size_t start = pos;
	while (syntax[pos] && !std::isspace((unsigned char)syntax[pos]))
	    pos++;
	std::string token(syntax + start, pos - start);

	if (!saw_operand && (token == "interspersed" || token == "options-first" ||
		token == "stop-at-first-operand")) {
	    if (token == "options-first")
		*policy = BU_CMD_PARSE_OPTIONS_FIRST;
	    else if (token == "stop-at-first-operand")
		*policy = BU_CMD_PARSE_STOP_AT_FIRST_OPERAND;
	    continue;
	}
	saw_operand = 1;
	if (!operands.empty() && operands.back().max_count == BU_CMD_COUNT_UNLIMITED)
	    return ged_opt_syntax_error(msgs, spec, command, syntax, start,
		"an unlimited operand must be last");

	ged_text_operand operand;
	if (!token.empty() && (token.back() == '?' || token.back() == '*' ||
		token.back() == '+')) {
	    char cardinality = token.back();
	    token.pop_back();
	    if (cardinality == '?') operand.min_count = 0;
	    if (cardinality == '*') {
		operand.min_count = 0;
		operand.max_count = BU_CMD_COUNT_UNLIMITED;
	    }
	    if (cardinality == '+')
		operand.max_count = BU_CMD_COUNT_UNLIMITED;
	} else if (!token.empty() && token.back() == '}') {
	    size_t open = token.rfind('{');
	    if (open == std::string::npos)
		return ged_opt_syntax_error(msgs, spec, command, syntax, start,
		    "cardinality has '}' without '{'");
	    std::string range = token.substr(open + 1, token.size() - open - 2);
	    token.erase(open);
	    size_t comma = range.find(',');
	    if (comma == std::string::npos) {
		if (ged_opt_syntax_uint(range, &operand.min_count) || !operand.min_count)
		    return ged_opt_syntax_error(msgs, spec, command, syntax, start,
			"exact cardinality must be a positive integer");
		operand.max_count = operand.min_count;
	    } else {
		if (range.find(',', comma + 1) != std::string::npos ||
		    ged_opt_syntax_uint(range.substr(0, comma), &operand.min_count))
		    return ged_opt_syntax_error(msgs, spec, command, syntax, start,
			"invalid minimum cardinality");
		std::string maximum = range.substr(comma + 1);
		if (maximum.empty()) {
		    operand.max_count = BU_CMD_COUNT_UNLIMITED;
		} else if (ged_opt_syntax_uint(maximum, &operand.max_count) ||
			operand.max_count < operand.min_count) {
		    return ged_opt_syntax_error(msgs, spec, command, syntax, start,
			"invalid maximum cardinality");
		}
	    }
	}

	size_t colon = token.find(':');
	if (colon == std::string::npos || !colon || colon + 1 == token.size())
	    return ged_opt_syntax_error(msgs, spec, command, syntax, start,
		"operand must have name:type form");
	operand.name = token.substr(0, colon);
	for (char c : operand.name) {
	    if (!std::isalnum((unsigned char)c) && c != '_' && c != '-')
		return ged_opt_syntax_error(msgs, spec, command, syntax, start,
		    "operand name contains an invalid character");
	}
	for (const ged_text_operand &prior : operands) {
	    if (prior.name == operand.name)
		return ged_opt_syntax_error(msgs, spec, command, syntax, start,
		    "operand name is duplicated");
	}

	std::string type_name = token.substr(colon + 1);
	size_t at = type_name.find('@');
	if (at != std::string::npos) {
	    operand.provider = type_name.substr(at + 1);
	    type_name.erase(at);
	    if (operand.provider.empty() || operand.provider.find('@') != std::string::npos)
		return ged_opt_syntax_error(msgs, spec, command, syntax, start,
		    "semantic provider is empty or malformed");
	    for (char c : operand.provider) {
		if (!std::isalnum((unsigned char)c) && c != '_' && c != '-' && c != '.')
		    return ged_opt_syntax_error(msgs, spec, command, syntax, start,
			"semantic provider contains an invalid character");
	    }
	}
	std::string type_parameters;
	int has_type_parameters = 0;
	size_t parameter_open = type_name.find('(');
	if (parameter_open != std::string::npos) {
	    has_type_parameters = 1;
	    if (type_name.back() != ')' || !parameter_open ||
		type_name.find('(', parameter_open + 1) != std::string::npos) {
		return ged_opt_syntax_error(msgs, spec, command, syntax, start,
		    "malformed type parameters");
	    }
	    type_parameters = type_name.substr(parameter_open + 1,
		type_name.size() - parameter_open - 2);
	    type_name.erase(parameter_open);
	}
	const char *default_provider = NULL;
	operand.type = ged_opt_syntax_type(type_name, &default_provider);
	if (operand.type == BU_CMD_VALUE_UNKNOWN)
	    return ged_opt_syntax_error(msgs, spec, command, syntax, start, "unknown operand type");
	if (type_name == "positive-int")
	    operand.validate = bu_cmd_positive_integer_validate;
	if (type_name == "nonnegative-int")
	    operand.validate = bu_cmd_nonnegative_integer_validate;
	if (type_name == "positive-number")
	    operand.validate = bu_cmd_positive_number_validate;
	if (type_name == "nonnegative-number")
	    operand.validate = bu_cmd_nonnegative_number_validate;
	if (type_name == "rgb")
	    operand.shape = &bu_cmd_rgb_arg_shape;
	if (type_name == "color")
	    operand.shape = &bu_cmd_color_arg_shape;
	if (type_name == "vector3")
	    operand.shape = &bu_cmd_vector3_arg_shape;
	if (type_name == "pattern")
	    operand.shape = &ged_text_pattern_shape;

	if (has_type_parameters && type_name == "keyword") {
	    if (type_parameters.empty())
		return ged_opt_syntax_error(msgs, spec, command, syntax, start,
		    "keyword vocabulary may not be empty");
	    size_t parameter_pos = 0;
	    while (parameter_pos <= type_parameters.size()) {
		size_t separator = type_parameters.find('|', parameter_pos);
		std::string keyword = type_parameters.substr(parameter_pos,
		    separator == std::string::npos ? std::string::npos : separator - parameter_pos);
		if (keyword.empty())
		    return ged_opt_syntax_error(msgs, spec, command, syntax, start,
			"keyword vocabulary contains an empty value");
		if (std::find(operand.keywords.begin(), operand.keywords.end(), keyword) !=
			operand.keywords.end())
		    return ged_opt_syntax_error(msgs, spec, command, syntax, start,
			"keyword vocabulary contains a duplicate value");
		operand.keywords.push_back(keyword);
		if (separator == std::string::npos)
		    break;
		parameter_pos = separator + 1;
	    }
	} else if (has_type_parameters &&
		(type_name == "int" || type_name == "integer" || type_name == "number")) {
	    size_t separator = type_parameters.find(':');
	    if (separator == std::string::npos ||
		type_parameters.find(':', separator + 1) != std::string::npos ||
		(separator == 0 && separator + 1 == type_parameters.size()))
		return ged_opt_syntax_error(msgs, spec, command, syntax, start,
		    "numeric domain must have min:max form with at least one bound");
	    std::string minimum = type_parameters.substr(0, separator);
	    std::string maximum = type_parameters.substr(separator + 1);
	    operand.range.minimum_inclusive = 1;
	    operand.range.maximum_inclusive = 1;
	    if (operand.type == BU_CMD_VALUE_INTEGER) {
		operand.range.kind = BU_CMD_RANGE_INTEGER;
		if (!minimum.empty()) {
		    if (!bu_cmd_long_from_str(&operand.range.integer_minimum, minimum.c_str()))
			return ged_opt_syntax_error(msgs, spec, command, syntax, start,
			    "invalid integer minimum");
		    operand.range.has_minimum = 1;
		}
		if (!maximum.empty()) {
		    if (!bu_cmd_long_from_str(&operand.range.integer_maximum, maximum.c_str()))
			return ged_opt_syntax_error(msgs, spec, command, syntax, start,
			    "invalid integer maximum");
		    operand.range.has_maximum = 1;
		}
		if (operand.range.has_minimum && operand.range.has_maximum &&
			operand.range.integer_maximum < operand.range.integer_minimum)
		    return ged_opt_syntax_error(msgs, spec, command, syntax, start,
			"integer domain maximum is below its minimum");
	    } else {
		operand.range.kind = BU_CMD_RANGE_NUMBER;
		if (!minimum.empty()) {
		    if (!bu_cmd_number_from_str(&operand.range.number_minimum, minimum.c_str()))
			return ged_opt_syntax_error(msgs, spec, command, syntax, start,
			    "invalid number minimum");
		    operand.range.has_minimum = 1;
		}
		if (!maximum.empty()) {
		    if (!bu_cmd_number_from_str(&operand.range.number_maximum, maximum.c_str()))
			return ged_opt_syntax_error(msgs, spec, command, syntax, start,
			    "invalid number maximum");
		    operand.range.has_maximum = 1;
		}
		if (operand.range.has_minimum && operand.range.has_maximum &&
			operand.range.number_maximum < operand.range.number_minimum)
		    return ged_opt_syntax_error(msgs, spec, command, syntax, start,
			"number domain maximum is below its minimum");
	    }
	} else if (has_type_parameters) {
	    return ged_opt_syntax_error(msgs, spec, command, syntax, start,
		"this operand type does not accept parameters");
	}
	if (operand.provider.empty() && default_provider)
	    operand.provider = default_provider;
	operands.push_back(operand);
    }
    return 0;
}


static int
ged_opt_group_cardinality(struct bu_vls *msgs, const ged_opt_spec *spec,
	const char *command, const char *syntax, size_t offset,
	const std::string &suffix, size_t *minimum, size_t *maximum)
{
    if (!minimum || !maximum)
	return -1;
    *minimum = 1;
    *maximum = 1;
    if (suffix.empty())
	return 0;
    if (suffix == "?") {
	*minimum = 0;
	return 0;
    }
    if (suffix == "*") {
	*minimum = 0;
	*maximum = BU_CMD_COUNT_UNLIMITED;
	return 0;
    }
    if (suffix == "+") {
	*maximum = BU_CMD_COUNT_UNLIMITED;
	return 0;
    }
    if (suffix.size() < 3 || suffix.front() != '{' || suffix.back() != '}')
	return ged_opt_syntax_error(msgs, spec, command, syntax, offset,
	    "invalid repeated-group cardinality");
    {
	std::string range = suffix.substr(1, suffix.size() - 2);
	size_t comma = range.find(',');
	if (comma == std::string::npos) {
	    if (ged_opt_syntax_uint(range, minimum) || !*minimum)
		return ged_opt_syntax_error(msgs, spec, command, syntax, offset,
		    "group cardinality must be a positive integer");
	    *maximum = *minimum;
	    return 0;
	}
	if (range.find(',', comma + 1) != std::string::npos ||
	    ged_opt_syntax_uint(range.substr(0, comma), minimum))
	    return ged_opt_syntax_error(msgs, spec, command, syntax, offset,
		"invalid group minimum cardinality");
	std::string maximum_text = range.substr(comma + 1);
	if (maximum_text.empty()) {
	    *maximum = BU_CMD_COUNT_UNLIMITED;
	} else if (ged_opt_syntax_uint(maximum_text, maximum) ||
		*maximum < *minimum) {
	    return ged_opt_syntax_error(msgs, spec, command, syntax, offset,
		"invalid group maximum cardinality");
	}
    }
    return 0;
}


static int
ged_opt_syntax_parse(struct bu_vls *msgs, const ged_opt_spec *spec,
	const char *command, const char *syntax,
	std::vector<ged_text_operand> &operands,
	std::vector<ged_text_group> &groups, bu_cmd_parse_policy_t *policy)
{
    size_t group_start = std::string::npos;

    if (!syntax || !policy)
	return ged_opt_syntax_error(msgs, spec, command, syntax, 0,
	    "missing syntax or result storage");
    for (size_t i = 0; syntax[i]; i++) {
	if (syntax[i] == '(' && (i == 0 || std::isspace((unsigned char)syntax[i - 1]))) {
	    group_start = i;
	    break;
	}
    }
    if (group_start == std::string::npos)
	return ged_opt_syntax_parse_flat(msgs, spec, command, syntax, operands,
	    policy);

    {
	std::string prefix(syntax, group_start);
	if (ged_opt_syntax_parse_flat(msgs, spec, command, prefix.c_str(),
		operands, policy))
	    return -1;
	for (const ged_text_operand &operand : operands) {
	    if (operand.min_count != operand.max_count ||
		operand.max_count == BU_CMD_COUNT_UNLIMITED)
		return ged_opt_syntax_error(msgs, spec, command, syntax, group_start,
		    "operands before repeated groups must have fixed cardinality");
	}
    }

    size_t pos = group_start;
    while (syntax[pos]) {
	while (syntax[pos] && std::isspace((unsigned char)syntax[pos]))
	    pos++;
	if (!syntax[pos])
	    break;
	if (syntax[pos] != '(')
	    return ged_opt_syntax_error(msgs, spec, command, syntax, pos,
		"ordinary operands cannot follow a repeated group");
	if (!groups.empty() &&
	    groups.back().max_count == BU_CMD_COUNT_UNLIMITED)
	    return ged_opt_syntax_error(msgs, spec, command, syntax, pos,
		"an unlimited repeated group must be last");
	size_t open = pos++;
	size_t depth = 1;
	while (syntax[pos] && depth) {
	    if (syntax[pos] == '(')
		depth++;
	    else if (syntax[pos] == ')')
		depth--;
	    pos++;
	}
	if (depth)
	    return ged_opt_syntax_error(msgs, spec, command, syntax, open,
		"repeated group has no closing ')'");
	size_t close = pos - 1;
	std::string body(syntax + open + 1, close - open - 1);
	size_t suffix_start = pos;
	while (syntax[pos] && !std::isspace((unsigned char)syntax[pos]))
	    pos++;
	std::string suffix(syntax + suffix_start, pos - suffix_start);
	ged_text_group group;
	bu_cmd_parse_policy_t role_policy = BU_CMD_PARSE_INTERSPERSED;
	if (ged_opt_group_cardinality(msgs, spec, command, syntax, suffix_start,
		suffix, &group.min_count, &group.max_count) ||
	    ged_opt_syntax_parse_flat(msgs, spec, command, body.c_str(),
		group.roles, &role_policy))
	    return -1;
	if (group.roles.empty())
	    return ged_opt_syntax_error(msgs, spec, command, syntax, open,
		"repeated group is empty");
	for (const ged_text_operand &role : group.roles) {
	    if (role.min_count != 1 || role.max_count != 1 ||
		(role.shape && (role.shape->min_tokens != 1 ||
		 role.shape->max_tokens != 1)))
		return ged_opt_syntax_error(msgs, spec, command, syntax, open,
		    "repeated group roles must be scalar values");
	    if (!group.name.empty())
		group.name += "_";
	    group.name += role.name;
	}
	group.name += "_group";
	group.help = group.name;
	std::replace(group.help.begin(), group.help.end(), '_', ' ');
	groups.push_back(group);
    }
    return 0;
}


static void
ged_opt_build_layout(ged_metadata_registry::owned_operand_layout &layout,
	const std::vector<ged_text_operand> &parsed_operands,
	const std::vector<ged_text_group> &parsed_groups)
{
    layout.operand_names.resize(parsed_operands.size());
    layout.operand_help.resize(parsed_operands.size());
    layout.operand_providers.resize(parsed_operands.size());
    layout.operand_keyword_strings.resize(parsed_operands.size());
    layout.operand_keywords.resize(parsed_operands.size());
    for (size_t i = 0; i < parsed_operands.size(); i++) {
	layout.operand_names[i] = parsed_operands[i].name;
	layout.operand_help[i] = parsed_operands[i].name;
	std::replace(layout.operand_help[i].begin(), layout.operand_help[i].end(),
	    '_', ' ');
	layout.operand_providers[i] = parsed_operands[i].provider;
	layout.operand_keyword_strings[i] = parsed_operands[i].keywords;
    }
    for (size_t i = 0; i < parsed_operands.size(); i++) {
	if (layout.operand_keyword_strings[i].empty())
	    continue;
	layout.operand_keywords[i].reserve(
	    layout.operand_keyword_strings[i].size() + 1);
	for (const std::string &keyword : layout.operand_keyword_strings[i])
	    layout.operand_keywords[i].push_back(keyword.c_str());
	layout.operand_keywords[i].push_back(NULL);
    }
    layout.operands.resize(parsed_operands.size() + 1);
    for (size_t i = 0; i < parsed_operands.size(); i++) {
	struct bu_cmd_operand &operand = layout.operands[i];
	operand.name = layout.operand_names[i].c_str();
	operand.min_count = parsed_operands[i].min_count;
	operand.max_count = parsed_operands[i].max_count;
	operand.help = layout.operand_help[i].c_str();
	operand.value_type = parsed_operands[i].type;
	operand.validate = parsed_operands[i].validate;
	operand.semantic_provider = layout.operand_providers[i].empty() ? NULL :
	    layout.operand_providers[i].c_str();
	operand.value_keywords = layout.operand_keywords[i].empty() ? NULL :
	    layout.operand_keywords[i].data();
	operand.shape = parsed_operands[i].shape;
	operand.range = parsed_operands[i].range;
    }

    layout.owned_groups.reserve(parsed_groups.size());
    for (const ged_text_group &parsed_group : parsed_groups) {
	std::unique_ptr<ged_metadata_registry::owned_operand_group> group(
	    new ged_metadata_registry::owned_operand_group());
	group->name = parsed_group.name;
	group->help = parsed_group.help;
	group->role_names.resize(parsed_group.roles.size());
	group->role_help.resize(parsed_group.roles.size());
	group->role_providers.resize(parsed_group.roles.size());
	group->role_keyword_strings.resize(parsed_group.roles.size());
	group->role_keywords.resize(parsed_group.roles.size());
	for (size_t ri = 0; ri < parsed_group.roles.size(); ri++) {
	    group->role_names[ri] = parsed_group.roles[ri].name;
	    group->role_help[ri] = parsed_group.roles[ri].name;
	    std::replace(group->role_help[ri].begin(), group->role_help[ri].end(),
		'_', ' ');
	    group->role_providers[ri] = parsed_group.roles[ri].provider;
	    group->role_keyword_strings[ri] = parsed_group.roles[ri].keywords;
	}
	for (size_t ri = 0; ri < parsed_group.roles.size(); ri++) {
	    if (group->role_keyword_strings[ri].empty())
		continue;
	    group->role_keywords[ri].reserve(
		group->role_keyword_strings[ri].size() + 1);
	    for (const std::string &keyword : group->role_keyword_strings[ri])
		group->role_keywords[ri].push_back(keyword.c_str());
	    group->role_keywords[ri].push_back(NULL);
	}
	group->roles.resize(parsed_group.roles.size() + 1);
	for (size_t ri = 0; ri < parsed_group.roles.size(); ri++) {
	    const ged_text_operand &parsed_role = parsed_group.roles[ri];
	    struct bu_cmd_operand &role = group->roles[ri];
	    role.name = group->role_names[ri].c_str();
	    role.min_count = 1;
	    role.max_count = 1;
	    role.help = group->role_help[ri].c_str();
	    role.value_type = parsed_role.type;
	    role.validate = parsed_role.validate;
	    role.semantic_provider = group->role_providers[ri].empty() ? NULL :
		group->role_providers[ri].c_str();
	    role.value_keywords = group->role_keywords[ri].empty() ? NULL :
		group->role_keywords[ri].data();
	    role.shape = parsed_role.shape;
	    role.range = parsed_role.range;
	}
	layout.owned_groups.push_back(std::move(group));
    }
    layout.groups.resize(parsed_groups.size() + 1);
    for (size_t gi = 0; gi < parsed_groups.size(); gi++) {
	const ged_text_group &parsed_group = parsed_groups[gi];
	const ged_metadata_registry::owned_operand_group *group =
	    layout.owned_groups[gi].get();
	layout.groups[gi].name = group->name.c_str();
	layout.groups[gi].roles = group->roles.data();
	layout.groups[gi].min_count = parsed_group.min_count;
	layout.groups[gi].max_count = parsed_group.max_count;
	layout.groups[gi].help = group->help.c_str();
    }
}


static const ged_opt_spec *
ged_opt_schema_spec(const struct bu_cmd_schema *schema,
	const struct bu_opt_desc **descs)
{
    struct ged_metadata_registry &registry = ged_metadata();
    std::lock_guard<std::mutex> guard(registry.mutex);

    if (descs) {
	auto desc_entry = registry.opt_schema_descs.find(schema);
	*descs = desc_entry == registry.opt_schema_descs.end() ? NULL :
	    desc_entry->second;
    }
    auto entry = registry.opt_schema_specs.find(schema);
    return entry == registry.opt_schema_specs.end() ? NULL : entry->second;
}


static const char *
ged_opt_schema_canonical(const struct bu_cmd_schema *schema, const char *name)
{
    if (!schema || BU_STR_EMPTY(name))
	return NULL;
    for (size_t i = 0; schema->options &&
	    bu_cmd_option_is_valid(&schema->options[i]); i++) {
	const struct bu_cmd_option *option = &schema->options[i];
	if (BU_STR_EQUAL(bu_cmd_option_canonical(option), name) ||
	    (!BU_STR_EMPTY(option->shortopt) && BU_STR_EQUAL(option->shortopt, name)) ||
	    (!BU_STR_EMPTY(option->longopt) && BU_STR_EQUAL(option->longopt, name)))
	    return bu_cmd_option_canonical(option);
    }
    return NULL;
}


static int
ged_opt_schema_option_present(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, const char *name)
{
    const char *canonical = ged_opt_schema_canonical(schema, name);
    return canonical ? bu_cmd_schema_option_present(schema, argc, argv,
	canonical) : 0;
}


static const char *
ged_opt_db_provider(ged_opt_db_kind_t kind, ged_opt_db_scope_t scope,
	int add_hidden)
{
    if (kind == GED_OPT_DB_OBJECT) {
	if (scope == GED_OPT_DB_GEOMETRY && !add_hidden)
	    return "ged.db_object";
	if (scope == GED_OPT_DB_ANY && !add_hidden)
	    return "ged.db_object_any";
	if (scope == GED_OPT_DB_ALL ||
	    (scope == GED_OPT_DB_ANY_HIDDEN && add_hidden))
	    return "ged.db_object_all";
	return NULL;
    }
    if (kind != GED_OPT_DB_PATH_OR_PATTERN)
	return NULL;
    switch (scope) {
	case GED_OPT_DB_GEOMETRY:
	    return add_hidden ? NULL : "ged.db_path_or_pattern";
	case GED_OPT_DB_ALL:
	    return add_hidden ? NULL : "ged.db_path_all_or_pattern";
	case GED_OPT_DB_ANY_HIDDEN:
	    return add_hidden ? NULL : "ged.db_path_any_hidden_or_pattern";
	case GED_OPT_DB_PRIMITIVES:
	    return add_hidden ? "ged.db_path_primitives_hidden_or_pattern" :
		"ged.db_path_primitives_or_pattern";
	case GED_OPT_DB_COMBINATIONS:
	    return add_hidden ? "ged.db_path_combinations_hidden_or_pattern" :
		"ged.db_path_combinations_or_pattern";
	case GED_OPT_DB_REGIONS:
	    return add_hidden ? "ged.db_path_regions_hidden_or_pattern" :
		"ged.db_path_regions_or_pattern";
	case GED_OPT_DB_ANY:
	default:
	    break;
    }
    return NULL;
}


static int
ged_opt_declarative_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    const ged_opt_spec *spec = ged_opt_schema_spec(schema, NULL);
    const struct ged_opt_db_completion *db_completions =
	ged_opt_db_completions(spec);

    if (!spec)
	return -1;
    if (result && result->state != BU_CMD_VALIDATE_INVALID &&
	(result->expected & BU_CMD_EXPECT_OPERAND) && db_completions) {
	size_t prefix_arg = result->token_start < argc ? result->token_start : argc;
	size_t operand_index = bu_cmd_schema_operand_count(schema, prefix_arg, argv);
	const struct bu_cmd_operand *operand = bu_cmd_schema_active_operand(schema,
	    argc, argv, operand_index);
	for (size_t pi = 0; operand && db_completions[pi].operand; pi++) {
	    const struct ged_opt_db_completion *policy = &db_completions[pi];
	    ged_opt_db_scope_t scope = policy->default_scope;
	    int modified = ged_opt_schema_option_present(schema, argc, argv,
		policy->modifier_option);
	    int typed = 0;

	    if (!BU_STR_EQUAL(policy->operand, operand->name))
		continue;
	    for (size_t ti = 0; policy->type_cases &&
		    policy->type_cases[ti].option; ti++) {
		if (ged_opt_schema_option_present(schema, argc, argv,
			policy->type_cases[ti].option)) {
		    scope = policy->type_cases[ti].scope;
		    typed = 1;
		    break;
		}
	    }
	    if (modified && !typed) {
		scope = policy->modified_default_scope;
		modified = 0;
	    }
	    result->semantic_provider = ged_opt_db_provider(policy->kind, scope,
		modified);
	    result->completion_type = policy->kind == GED_OPT_DB_OBJECT ?
		BU_CMD_VALUE_DB_OBJECT : BU_CMD_VALUE_DB_PATH;
	    result->hint = !BU_STR_EMPTY(policy->hint) ? policy->hint :
		(policy->kind == GED_OPT_DB_OBJECT ? "database object" :
		 "database object or path pattern");
	    break;
	}
    }
    {
	bu_cmd_schema_validate_t validate = ged_opt_validate(spec);
	return validate ? validate(schema, argc, argv, cursor_arg, result) : 0;
    }
}


struct ged_schema_analysis_role {
    const char *provider = NULL;
    const char *hint = NULL;
    bu_cmd_value_t type = BU_CMD_VALUE_UNKNOWN;
};


struct ged_schema_analysis_plan {
    std::unordered_map<std::string, struct ged_schema_analysis_role> roles;
};


extern "C" void *
_ged_schema_analysis_plan_create(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv)
{
    const ged_opt_spec *spec = ged_opt_schema_spec(schema, NULL);
    if (!spec || ged_opt_validate(spec))
	return NULL;

    std::unique_ptr<struct ged_schema_analysis_plan> plan(
	new ged_schema_analysis_plan());
    const struct ged_opt_db_completion *db_completions =
	ged_opt_db_completions(spec);
    for (size_t pi = 0; db_completions && db_completions[pi].operand; pi++) {
	const struct ged_opt_db_completion *policy = &db_completions[pi];
	if (plan->roles.find(policy->operand) != plan->roles.end())
	    continue;
	ged_opt_db_scope_t scope = policy->default_scope;
	int modified = ged_opt_schema_option_present(schema, argc, argv,
	    policy->modifier_option);
	int typed = 0;
	for (size_t ti = 0; policy->type_cases &&
		policy->type_cases[ti].option; ti++) {
	    if (ged_opt_schema_option_present(schema, argc, argv,
		    policy->type_cases[ti].option)) {
		scope = policy->type_cases[ti].scope;
		typed = 1;
		break;
	    }
	}
	if (modified && !typed) {
	    scope = policy->modified_default_scope;
	    modified = 0;
	}
	struct ged_schema_analysis_role role;
	role.provider = ged_opt_db_provider(policy->kind, scope, modified);
	role.type = policy->kind == GED_OPT_DB_OBJECT ? BU_CMD_VALUE_DB_OBJECT :
	    BU_CMD_VALUE_DB_PATH;
	role.hint = !BU_STR_EMPTY(policy->hint) ? policy->hint :
	    (policy->kind == GED_OPT_DB_OBJECT ? "database object" :
	     "database object or path pattern");
	plan->roles.emplace(policy->operand, role);
    }
    return plan.release();
}


extern "C" int
_ged_schema_analysis_plan_role(void *data,
	const struct bu_cmd_operand *operand,
	struct bu_cmd_validate_result *result)
{
    struct ged_schema_analysis_plan *plan =
	(struct ged_schema_analysis_plan *)data;
    if (!plan || !operand || BU_STR_EMPTY(operand->name) || !result)
	return 0;
    auto found = plan->roles.find(operand->name);
    if (found == plan->roles.end())
	return 0;
    result->semantic_provider = found->second.provider;
    result->completion_type = found->second.type;
    result->hint = found->second.hint;
    return 1;
}


extern "C" void
_ged_schema_analysis_plan_destroy(void *data)
{
    delete (struct ged_schema_analysis_plan *)data;
}


static const struct bu_opt_value_spec *
ged_opt_value_for_name(const struct bu_cmd_schema *schema,
	const struct bu_opt_value_spec *values, const char *name)
{
    if (!schema || !values || BU_STR_EMPTY(name))
	return NULL;
    for (size_t i = 0; values[i].option; i++) {
	if (BU_STR_EQUAL(values[i].option, name))
	    return &values[i];
	for (size_t oi = 0; schema->options &&
		bu_cmd_option_is_valid(&schema->options[oi]); oi++) {
	    const struct bu_cmd_option *option = &schema->options[oi];
	    if (BU_STR_EQUAL(bu_cmd_option_canonical(option), name) &&
		((!BU_STR_EMPTY(option->shortopt) &&
		  BU_STR_EQUAL(values[i].option, option->shortopt)) ||
		 (!BU_STR_EMPTY(option->longopt) &&
		  BU_STR_EQUAL(values[i].option, option->longopt))))
		return &values[i];
	}
    }
    return NULL;
}


static int
ged_opt_copy_validation(const struct bu_cmd_schema *schema,
	const ged_opt_spec *spec,
	struct bu_opt_validate_result *option_result,
	struct bu_cmd_validate_result *result)
{
    const struct bu_opt_value_spec *value;

    if (!option_result ||
	!(option_result->expected & BU_OPT_EXPECT_OPTION_ARG) ||
	(option_result->expected & BU_OPT_EXPECT_OPTION) ||
	!option_result->option_name)
	return 0;
    value = ged_opt_value_for_name(schema, ged_opt_values(spec),
	option_result->option_name);
    if (value && value->validate) {
	bu_cmd_validate_result_clear(result);
	result->state = ged_opt_native_state(option_result->state);
	result->token_start = option_result->token_start;
	result->token_end = option_result->token_end;
	result->expected =
	    ((option_result->expected & BU_OPT_EXPECT_OPTION) ? BU_CMD_EXPECT_OPTION : BU_CMD_EXPECT_NONE) |
	    ((option_result->expected & BU_OPT_EXPECT_OPTION_ARG) ? BU_CMD_EXPECT_OPTION_ARG : BU_CMD_EXPECT_NONE);
	result->hint = option_result->hint;
	result->completion_type = bu_opt_cmd_type(option_result->value_type);
	result->completion_count = option_result->completion_count;
	result->completion_candidates = option_result->completion_candidates;
	option_result->completion_count = 0;
	option_result->completion_candidates = NULL;
	return 1;
    }
    return 0;
}


static int
ged_opt_schema_context_validate(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv, size_t cursor_arg, void *context,
	struct bu_cmd_validate_result *result)
{
    const struct bu_opt_desc *descs = NULL;
    const ged_opt_spec *spec = ged_opt_schema_spec(schema, &descs);
    struct bu_opt_validate_result option_result = BU_OPT_VALIDATE_RESULT_NULL;
    int ret = 0;
    const struct bu_opt_value_spec *values;
    bu_cmd_schema_context_validate_t context_validate;

    if (!spec || !descs)
	return -1;
    values = ged_opt_values(spec);
    context_validate = ged_opt_context_validate(spec);
    ret = bu_opt_desc_validate(descs, values, argc, argv,
	cursor_arg, context, &option_result);
    if (ret)
	goto done;
    (void)ged_opt_copy_validation(schema, spec, &option_result, result);
    bu_opt_validate_result_clear(&option_result);

    /* Scan actual option spans once.  Revalidating the whole argv prefix at
     * every earlier cursor made this O(argc^2) and rebuilt adapter storage on
     * every pass. */
    for (size_t i = 0; values && i < cursor_arg && i < argc; ) {
	const struct bu_opt_desc *desc = NULL;
	const struct bu_opt_value_spec *value = NULL;
	const char *canonical = NULL;
	const char *attached_arg = NULL;
	bool attached_value = false;
	int span = bu_cmd_schema_option_span(schema, argc - i, argv + i);
	if (span <= 0) {
	    i++;
	    continue;
	}
	const char *token = argv[i];
	int is_long = token && token[0] == '-' && token[1] == '-';
	if (is_long) {
	    const char *name = token + 2;
	    size_t name_len = strcspn(name, "=");
	    for (size_t di = 0; bu_cmd_option_is_valid(&schema->options[di]); di++) {
		if (!BU_STR_EMPTY(descs[di].longopt) &&
			strlen(descs[di].longopt) == name_len &&
			!bu_strncmp(descs[di].longopt, name, name_len)) {
		    desc = &descs[di];
		    attached_value = name[name_len] == '=';
		    if (attached_value)
			attached_arg = name + name_len + 1;
		    break;
		}
	    }
	} else if (token && token[0] == '-') {
	    /* bu_opt historically permits a multi-character short spelling.
	     * Resolve an exact spelling before interpreting a word as a cluster. */
	    for (size_t di = 0; bu_cmd_option_is_valid(&schema->options[di]); di++) {
		if (!BU_STR_EMPTY(descs[di].shortopt) &&
			BU_STR_EQUAL(descs[di].shortopt, token + 1)) {
		    if (schema->options[di].arg_requirement != BU_CMD_ARG_NONE)
			desc = &descs[di];
		    break;
		}
	    }
	    /* Otherwise a short cluster belongs to the first member which
	     * consumes a value; remaining bytes are its attached value. */
	    for (size_t ti = 1; token[ti] && !desc; ti++) {
		for (size_t di = 0; bu_cmd_option_is_valid(&schema->options[di]); di++) {
		    if (BU_STR_EMPTY(descs[di].shortopt) ||
			    descs[di].shortopt[1] || descs[di].shortopt[0] != token[ti])
			continue;
		    if (schema->options[di].arg_requirement != BU_CMD_ARG_NONE) {
			desc = &descs[di];
			attached_value = token[ti + 1] != '\0';
			if (attached_value)
			    attached_arg = token + ti + 1;
		    }
		    break;
		}
	    }
	}
	if (desc) {
	    canonical = !BU_STR_EMPTY(desc->longopt) ?
		desc->longopt : desc->shortopt;
	    value = ged_opt_value_for_name(schema, values, canonical);
	}
	size_t first_value = attached_value ? i : i + 1;
	std::vector<const char *> validation_argv;
	if (attached_value) {
	    validation_argv.assign(argv, argv + argc);
	    validation_argv[i] = attached_arg;
	}
	for (size_t vi = first_value; value && value->validate &&
		vi < i + (size_t)span && vi < cursor_arg; vi++) {
	    bu_opt_validate_result_init(&option_result);
	    option_result.state = BU_OPT_VALIDATE_VALID;
	    option_result.token_start = vi;
	    option_result.token_end = vi;
	    option_result.expected = BU_OPT_EXPECT_OPTION_ARG;
	    option_result.value_type = value->value_type;
	    option_result.hint = value->hint;
	    option_result.option = desc;
	    option_result.option_name = canonical;
	    ret = value->validate(desc, argc,
		attached_value ? validation_argv.data() : argv, vi, context, value->data,
		&option_result);
	    if (ret)
		goto done;
	    if (option_result.state == BU_OPT_VALIDATE_INVALID &&
		    ged_opt_copy_validation(schema, spec, &option_result, result))
		goto done;
	    bu_opt_validate_result_clear(&option_result);
	}
	i += (size_t)span;
    }

    if (result->state == BU_CMD_VALIDATE_INVALID)
	goto done;
    if (context_validate)
	ret = context_validate(schema, argc, argv, cursor_arg, context,
	    result);

done:
    bu_opt_validate_result_clear(&option_result);
    return ret;
}


extern "C" int
ged_opt_lint(const ged_opt_spec *spec, struct bu_vls *msgs)
{
    struct bu_opt_desc *descs = NULL;
    size_t option_count = 0;
    std::vector<ged_text_operand> operands;
    std::vector<ged_text_group> groups;
    std::set<std::string> operand_names;
    std::map<std::string, std::set<bu_cmd_value_t>> operand_types;
    bu_cmd_parse_policy_t policy;
    const struct bu_opt_value_spec *values;
    const ged_opt_semantic *semantics;
    int ret = -1;

    if (spec && spec->rules) {
	ged_compiled_opt_rules compiled;
	if (compiled.compile(spec, msgs))
	    return -1;
	return ged_opt_lint(&compiled.spec, msgs);
    }
    if (!spec || !spec->option_builder) {
	if (msgs)
	    bu_vls_printf(msgs, "compact schema: missing option builder\n");
	return -1;
    }
    values = ged_opt_values(spec);
    semantics = ged_opt_semantics(spec);
    if ((spec->syntax && spec->operands) ||
	(spec->forms && (spec->syntax || spec->operands)))
	return ged_opt_syntax_error(msgs, spec, spec->name, spec->syntax, 0,
	    "text syntax, operand tables, and operand forms are mutually exclusive");
    policy = spec->parse_policy;
    if (spec->syntax && ged_opt_syntax_parse(msgs, spec, spec->name, spec->syntax,
	    operands, groups, &policy))
	return -1;
    for (const ged_text_operand &operand : operands) {
	operand_names.insert(operand.name);
	operand_types[operand.name].insert(operand.type);
    }
    for (const ged_text_group &group : groups)
	for (const ged_text_operand &role : group.roles) {
	    operand_names.insert(role.name);
	    operand_types[role.name].insert(role.type);
	}
    for (size_t oi = 0; spec->operands && spec->operands[oi].name; oi++) {
	operand_names.insert(spec->operands[oi].name);
	operand_types[spec->operands[oi].name].insert(spec->operands[oi].value_type);
    }
    for (size_t gi = 0; spec->operand_groups &&
	    spec->operand_groups[gi].name; gi++)
	for (size_t ri = 0; spec->operand_groups[gi].roles &&
		spec->operand_groups[gi].roles[ri].name; ri++) {
	    operand_names.insert(spec->operand_groups[gi].roles[ri].name);
	    operand_types[spec->operand_groups[gi].roles[ri].name].insert(
		spec->operand_groups[gi].roles[ri].value_type);
	}
    descs = bu_opt_desc_build(spec->option_builder, NULL, &option_count);
    if (!descs) {
	if (msgs)
	    bu_vls_printf(msgs, "%s:%d: command '%s': option builder failed\n",
		spec->source_file ? spec->source_file : "(unknown)", spec->source_line,
		spec->name ? spec->name : "(unknown)");
	return -1;
    }
    if (spec->forms) {
	int saw_default = 0;
	std::set<std::string> case_names;
	std::set<std::string> case_predicates;
	for (size_t ci = 0; spec->forms[ci].name; ci++) {
	    const ged_opt_form *cmd_case = &spec->forms[ci];
	    std::vector<ged_text_operand> case_operands;
	    std::vector<ged_text_group> case_groups;
	    bu_cmd_parse_policy_t case_policy = spec->parse_policy;
	    std::set<std::string> layout_names;

	    if (BU_STR_EMPTY(cmd_case->name) ||
		cmd_case->condition < BU_CMD_CONDITION_ALWAYS ||
		cmd_case->condition > BU_CMD_CONDITION_ALL_OPTIONS_PRESENT) {
		if (msgs)
		    bu_vls_printf(msgs, "command '%s': malformed operand case\n",
			spec->name ? spec->name : "(unknown)");
		goto done;
	    }
	    if (!case_names.insert(cmd_case->name).second) {
		if (msgs)
		    bu_vls_printf(msgs,
			"command '%s': duplicate operand case name '%s'\n",
			spec->name ? spec->name : "(unknown)", cmd_case->name);
		goto done;
	    }

	    if (!cmd_case->syntax ||
		ged_opt_syntax_parse(msgs, spec, spec->name, cmd_case->syntax,
		    case_operands, case_groups, &case_policy))
		goto done;
	    for (const ged_text_operand &operand : case_operands) {
		if (!layout_names.insert(operand.name).second) {
		    if (msgs)
			bu_vls_printf(msgs,
			    "command '%s': operand case '%s' repeats role '%s'\n",
			    spec->name ? spec->name : "(unknown)", cmd_case->name,
			    operand.name.c_str());
		    goto done;
		}
		operand_names.insert(operand.name);
		operand_types[operand.name].insert(operand.type);
	    }
	    for (const ged_text_group &group : case_groups)
		for (const ged_text_operand &role : group.roles) {
		    if (!layout_names.insert(role.name).second) {
			if (msgs)
			    bu_vls_printf(msgs,
				"command '%s': operand case '%s' repeats role '%s'\n",
				spec->name ? spec->name : "(unknown)", cmd_case->name,
				role.name.c_str());
			goto done;
		    }
		    operand_names.insert(role.name);
		    operand_types[role.name].insert(role.type);
		}
	    if (ci && case_policy != policy) {
		if (msgs)
		    bu_vls_printf(msgs,
			"command '%s': operand forms must use one parse policy\n",
			spec->name ? spec->name : "(unknown)");
		goto done;
	    }
	    policy = case_policy;
	    if (cmd_case->condition == BU_CMD_CONDITION_ALWAYS) {
		if (saw_default || spec->forms[ci + 1].name) {
		    if (msgs)
			bu_vls_printf(msgs,
			    "command '%s': default operand case must be unique and last\n",
			    spec->name ? spec->name : "(unknown)");
		    goto done;
		}
		saw_default = 1;
		continue;
	    }
	    if (BU_STR_EMPTY(cmd_case->option_names)) {
		if (msgs)
		    bu_vls_printf(msgs,
			"command '%s': operand case '%s' has no condition options\n",
			spec->name ? spec->name : "(unknown)", cmd_case->name);
		goto done;
	    }
	    std::string names(cmd_case->option_names);
	    std::set<std::string> predicate_options;
	    size_t pos = 0;
	    while (pos < names.size()) {
		while (pos < names.size() && std::isspace((unsigned char)names[pos]))
		    pos++;
		size_t start = pos;
		while (pos < names.size() && !std::isspace((unsigned char)names[pos]))
		    pos++;
		if (start == pos)
		    break;
		std::string option = names.substr(start, pos - start);
		if (!predicate_options.insert(option).second) {
		    if (msgs)
			bu_vls_printf(msgs,
			    "command '%s': operand case '%s' repeats option '%s'\n",
			    spec->name ? spec->name : "(unknown)", cmd_case->name,
			    option.c_str());
		    goto done;
		}
		if (ged_opt_desc_match_count(descs, option.c_str()) != 1) {
		    if (msgs)
			bu_vls_printf(msgs,
			    "command '%s': operand case '%s' option '%s' is not unique\n",
			    spec->name ? spec->name : "(unknown)", cmd_case->name,
			    option.c_str());
		    goto done;
		}
	    }
	    std::string predicate = std::to_string((int)cmd_case->condition);
	    for (const std::string &option : predicate_options) {
		predicate += " ";
		predicate += option;
	    }
	    if (!case_predicates.insert(predicate).second) {
		if (msgs)
		    bu_vls_printf(msgs,
			"command '%s': operand case '%s' repeats an earlier condition\n",
			spec->name ? spec->name : "(unknown)", cmd_case->name);
		goto done;
	    }
	}
	if (!saw_default) {
	    if (msgs)
		bu_vls_printf(msgs, "command '%s': operand forms have no default\n",
		    spec->name ? spec->name : "(unknown)");
	    goto done;
	}
    }
    for (size_t i = 0; values && values[i].option; i++) {
	if (ged_opt_desc_match_count(descs, values[i].option) != 1) {
	    if (msgs)
		bu_vls_printf(msgs, "command '%s': value metadata option '%s' is not unique\n",
		    spec->name ? spec->name : "(unknown)", values[i].option);
	    goto done;
	}
	for (size_t j = i + 1; values[j].option; j++) {
	    if (BU_STR_EQUAL(values[i].option, values[j].option)) {
		if (msgs)
		    bu_vls_printf(msgs, "command '%s': duplicate value metadata option '%s'\n",
			spec->name ? spec->name : "(unknown)", values[i].option);
		goto done;
	    }
	}
    }
    for (size_t i = 0; semantics && semantics[i].option; i++) {
	if (ged_opt_desc_match_count(descs, semantics[i].option) != 1) {
	    if (msgs)
		bu_vls_printf(msgs, "command '%s': semantic metadata option '%s' is not unique\n",
		    spec->name ? spec->name : "(unknown)", semantics[i].option);
	    goto done;
	}
	for (size_t j = i + 1; semantics[j].option; j++) {
	    if (BU_STR_EQUAL(semantics[i].option, semantics[j].option)) {
		if (msgs)
		    bu_vls_printf(msgs, "command '%s': duplicate semantic metadata option '%s'\n",
			spec->name ? spec->name : "(unknown)", semantics[i].option);
		goto done;
	    }
	}
    }
    {
	const struct ged_opt_db_completion *db_completions =
	    ged_opt_db_completions(spec);
	for (size_t pi = 0; db_completions && db_completions[pi].operand; pi++) {
	    const struct ged_opt_db_completion *db_policy = &db_completions[pi];
	    int have_modifier = !BU_STR_EMPTY(db_policy->modifier_option);
	    bu_cmd_value_t expected_type = db_policy->kind == GED_OPT_DB_OBJECT ?
		BU_CMD_VALUE_DB_OBJECT : BU_CMD_VALUE_DB_PATH;

	    if (operand_names.find(db_policy->operand) == operand_names.end()) {
		if (msgs)
		    bu_vls_printf(msgs,
			"command '%s': database completion references unknown operand '%s'\n",
			spec->name ? spec->name : "(unknown)", db_policy->operand);
		goto done;
	    }
	    if (operand_types[db_policy->operand].find(expected_type) ==
		    operand_types[db_policy->operand].end()) {
		if (msgs)
		    bu_vls_printf(msgs,
			"command '%s': database completion kind does not match operand '%s'\n",
			spec->name ? spec->name : "(unknown)", db_policy->operand);
		goto done;
	    }
	    if (db_policy->kind < GED_OPT_DB_OBJECT ||
		db_policy->kind > GED_OPT_DB_PATH_OR_PATTERN ||
		db_policy->default_scope < GED_OPT_DB_GEOMETRY ||
		db_policy->default_scope > GED_OPT_DB_REGIONS ||
		db_policy->modified_default_scope < GED_OPT_DB_GEOMETRY ||
		db_policy->modified_default_scope > GED_OPT_DB_REGIONS ||
		!ged_opt_db_provider(db_policy->kind, db_policy->default_scope, 0) ||
		(have_modifier && !ged_opt_db_provider(db_policy->kind,
		    db_policy->modified_default_scope, 0))) {
		if (msgs)
		    bu_vls_printf(msgs,
			"command '%s': database completion for '%s' has an unsupported scope\n",
			spec->name ? spec->name : "(unknown)", db_policy->operand);
		goto done;
	    }
	    if (have_modifier &&
		ged_opt_desc_match_count(descs, db_policy->modifier_option) != 1) {
		if (msgs)
		    bu_vls_printf(msgs,
			"command '%s': database completion modifier option '%s' is not unique\n",
			spec->name ? spec->name : "(unknown)",
			db_policy->modifier_option);
		goto done;
	    }
	    for (size_t ti = 0; db_policy->type_cases &&
		    db_policy->type_cases[ti].option; ti++) {
		const struct ged_opt_db_type_case *type_case =
		    &db_policy->type_cases[ti];
		if (ged_opt_desc_match_count(descs, type_case->option) != 1 ||
		    type_case->scope < GED_OPT_DB_GEOMETRY ||
		    type_case->scope > GED_OPT_DB_REGIONS ||
		    !ged_opt_db_provider(db_policy->kind, type_case->scope,
			have_modifier)) {
		    if (msgs)
			bu_vls_printf(msgs,
			    "command '%s': database type option '%s' has an unsupported scope\n",
			    spec->name ? spec->name : "(unknown)",
			    type_case->option);
		    goto done;
		}
		for (size_t tj = 0; tj < ti; tj++) {
		    if (BU_STR_EQUAL(type_case->option,
			    db_policy->type_cases[tj].option)) {
			if (msgs)
			    bu_vls_printf(msgs,
				"command '%s': duplicate database type option '%s'\n",
				spec->name ? spec->name : "(unknown)",
				type_case->option);
			goto done;
		    }
		}
	    }
	}
    }
    {
	struct bu_opt_validate_result check = BU_OPT_VALIDATE_RESULT_NULL;
	if (bu_opt_desc_validate(descs, values, 0, NULL, 0, NULL, &check) != 0) {
	    if (msgs)
		bu_vls_printf(msgs, "command '%s': invalid option value metadata\n",
		    spec->name ? spec->name : "(unknown)");
	    bu_opt_validate_result_clear(&check);
	    goto done;
	}
	bu_opt_validate_result_clear(&check);
    }
    ret = 0;
done:
    bu_free(descs, "built bu_opt descriptors");
    return ret;
}


static int ged_native_schema_lint_node(const char *, const struct bu_cmd_schema *,
	struct bu_vls *);
static int ged_grammar_lint_declaration(const char *, const struct ged_cmd_grammar *,
	struct bu_vls *);

extern "C" int
ged_opt_register(const char *name, const ged_opt_spec *spec)
{
    struct bu_opt_desc *descs;
    size_t option_count = 0;
    std::vector<ged_text_operand> parsed_operands;
    std::vector<ged_text_group> parsed_groups;
    bu_cmd_parse_policy_t parsed_policy;
    const struct bu_opt_value_spec *values;
    const ged_opt_semantic *semantics;
    std::unique_ptr<ged_compiled_opt_rules> compiled_rules;

    if (!name || !spec || !spec->option_builder)
	return -1;
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_shutting_down)
	return -1;
    std::string key(name);
    if (!ged_command_key_valid(key) || key.compare(0, 6, "_mged_") == 0)
	return -1;
    if (spec->rules) {
	compiled_rules.reset(new ged_compiled_opt_rules());
	if (compiled_rules->compile(spec, &init_msgs))
	    return -1;
	spec = &compiled_rules->spec;
    }
    if (ged_opt_lint(spec, &init_msgs))
	return -1;
    values = ged_opt_values(spec);
    semantics = ged_opt_semantics(spec);
    parsed_policy = spec->parse_policy;
    if (spec->syntax && ged_opt_syntax_parse(&init_msgs, spec, name, spec->syntax,
	    parsed_operands, parsed_groups, &parsed_policy))
	return -1;

    descs = bu_opt_desc_build(spec->option_builder, NULL, &option_count);
    if (!descs)
	return -1;
    for (size_t i = 0; values && values[i].option; i++) {
	if (ged_opt_desc_match_count(descs, values[i].option) != 1) {
	    bu_free(descs, "built bu_opt descriptors");
	    return -1;
	}
	for (size_t j = i + 1; values[j].option; j++) {
	    if (BU_STR_EQUAL(values[i].option, values[j].option)) {
		bu_free(descs, "built bu_opt descriptors");
		return -1;
	    }
	}
    }
    for (size_t i = 0; semantics && semantics[i].option; i++) {
	if (ged_opt_desc_match_count(descs, semantics[i].option) != 1) {
	    bu_free(descs, "built bu_opt descriptors");
	    return -1;
	}
	for (size_t j = i + 1; semantics[j].option; j++) {
	    if (BU_STR_EQUAL(semantics[i].option, semantics[j].option)) {
		bu_free(descs, "built bu_opt descriptors");
		return -1;
	    }
	}
    }
    {
	struct bu_opt_validate_result check = BU_OPT_VALIDATE_RESULT_NULL;
	if (bu_opt_desc_validate(descs, values, 0, NULL, 0, NULL,
		&check) != 0) {
	    bu_opt_validate_result_clear(&check);
	    bu_free(descs, "built bu_opt descriptors");
	    return -1;
	}
	bu_opt_validate_result_clear(&check);
    }
    std::unique_ptr<ged_metadata_registry::owned_opt_schema> owned(
	new ged_metadata_registry::owned_opt_schema());
    owned->compiled_rules = std::move(compiled_rules);
    owned->spec = *spec;
    if (spec->metadata && !owned->compiled_rules) {
	owned->metadata = *spec->metadata;
	owned->spec.metadata = &owned->metadata;
    }
    owned->descs = descs;
    descs = NULL;
    if (bu_opt_cmd_create(&owned->options, owned->descs, values))
	return -1;
    for (size_t i = 0; i < option_count; i++) {
	const ged_opt_semantic *semantic = NULL;
	if (semantics) {
	    const char *oname = !BU_STR_EMPTY(owned->descs[i].longopt) ? owned->descs[i].longopt : owned->descs[i].shortopt;
	    for (size_t si = 0; semantics[si].option; si++) {
		if ((!BU_STR_EMPTY(oname) && BU_STR_EQUAL(semantics[si].option, oname)) ||
		    (!BU_STR_EMPTY(owned->descs[i].shortopt) && BU_STR_EQUAL(semantics[si].option, owned->descs[i].shortopt)) ||
		    (!BU_STR_EMPTY(owned->descs[i].longopt) && BU_STR_EQUAL(semantics[si].option, owned->descs[i].longopt))) {
		    semantic = &semantics[si];
		    break;
		}
	    }
	}
	struct bu_cmd_option &option = owned->options.options[i];
	if (semantic && semantic->hint)
	    option.argument = semantic->hint;
	if (semantic && semantic->value_type != BU_CMD_VALUE_UNKNOWN)
	    option.value_type = semantic->value_type;
	if (semantic)
	    option.semantic_provider = semantic->semantic_provider;
    }
    if (bu_opt_cmd_aliases(&owned->options))
	return -1;
    /* Keep the immutable descriptor table with the derived schema.  Completion
     * validation is a hot path and rebuilding this table for every keystroke
     * adds avoidable allocation and builder work. */

    if (spec->syntax)
	ged_opt_build_layout(owned->layout, parsed_operands, parsed_groups);

    if (spec->forms) {
	for (size_t ci = 0; spec->forms[ci].name; ci++) {
	    const ged_opt_form *decl = &spec->forms[ci];
	    std::vector<ged_text_operand> case_operands;
	    std::vector<ged_text_group> case_groups;
	    bu_cmd_parse_policy_t case_policy = spec->parse_policy;
	    std::unique_ptr<ged_metadata_registry::owned_opt_form> cmd_case(
		new ged_metadata_registry::owned_opt_form());

	    if (ged_opt_syntax_parse(&init_msgs, spec, name, decl->syntax,
		    case_operands, case_groups, &case_policy))
		return -1;
	    if (!owned->owned_forms.empty() && case_policy != parsed_policy)
		return -1;
	    parsed_policy = case_policy;
	    cmd_case->name = decl->name;
	    cmd_case->help = decl->help ? decl->help : "";
	    if (decl->option_names) {
		std::string names(decl->option_names);
		size_t pos = 0;
		while (pos < names.size()) {
		    while (pos < names.size() &&
			std::isspace((unsigned char)names[pos]))
			pos++;
		    size_t start = pos;
		    while (pos < names.size() &&
			!std::isspace((unsigned char)names[pos]))
			pos++;
		    if (start == pos)
			break;
		    std::string requested = names.substr(start, pos - start);
		    const char *canonical = NULL;
		    for (size_t oi = 0; oi < option_count; oi++) {
			const struct bu_cmd_option *option = &owned->options.options[oi];
			if ((!BU_STR_EMPTY(option->shortopt) &&
				BU_STR_EQUAL(option->shortopt, requested.c_str())) ||
			    (!BU_STR_EMPTY(option->longopt) &&
				BU_STR_EQUAL(option->longopt, requested.c_str())) ||
			    BU_STR_EQUAL(bu_cmd_option_canonical(option), requested.c_str())) {
			    canonical = bu_cmd_option_canonical(option);
			    break;
			}
		    }
		    if (!canonical)
			return -1;
		    cmd_case->option_names.push_back(canonical);
		}
		cmd_case->options.reserve(cmd_case->option_names.size() + 1);
		for (const std::string &option : cmd_case->option_names)
		    cmd_case->options.push_back(option.c_str());
		cmd_case->options.push_back(NULL);
	    }
	    ged_opt_build_layout(cmd_case->layout, case_operands, case_groups);
	    owned->owned_forms.push_back(std::move(cmd_case));
	}
	owned->cases.resize(owned->owned_forms.size() + 1);
	for (size_t ci = 0; ci < owned->owned_forms.size(); ci++) {
	    const ged_opt_form *decl = &spec->forms[ci];
	    const ged_metadata_registry::owned_opt_form *cmd_case =
		owned->owned_forms[ci].get();
	    owned->cases[ci].name = cmd_case->name.c_str();
	    owned->cases[ci].help = cmd_case->help.empty() ? NULL :
		cmd_case->help.c_str();
	    owned->cases[ci].condition = decl->condition;
	    owned->cases[ci].options = cmd_case->options.empty() ? NULL :
		cmd_case->options.data();
	    owned->cases[ci].operands = cmd_case->layout.operands.data();
	    owned->cases[ci].operand_groups = cmd_case->layout.groups.data();
	}
    }

    owned->schema_name = owned->spec.name ? owned->spec.name : name;
    owned->schema.name = owned->schema_name.c_str();
    owned->schema.help = owned->spec.help;
    owned->schema.options = owned->options.options;
    owned->schema.operands = owned->spec.syntax ? owned->layout.operands.data() :
	owned->spec.operands;
    owned->schema.parse_policy = parsed_policy;
    owned->schema.operand_groups = owned->spec.syntax ? owned->layout.groups.data() :
	owned->spec.operand_groups;
    owned->schema.validation.custom_validate = (owned->spec.forms ||
	ged_opt_db_completions(&owned->spec) || ged_opt_validate(&owned->spec)) ?
	ged_opt_declarative_validate : NULL;
    owned->schema.validation.constraints = ged_opt_constraints(&owned->spec);
    owned->schema.validation.context_validate = ged_opt_schema_context_validate;
    owned->schema.validation.cases = owned->cases.empty() ? NULL : owned->cases.data();
    owned->schema.validation.external_execution = 1;

    if (ged_native_schema_lint_node(key.c_str(), &owned->schema, &init_msgs))
	return -1;

    struct ged_metadata_registry &registry = ged_metadata();
    std::lock_guard<std::mutex> guard(registry.mutex);
    if (registry.native_schemas.find(key) != registry.native_schemas.end() ||
	registry.grammars.find(key) != registry.grammars.end())
	return 1;
    registry.native_schemas[key] = &owned->schema;
    registry.opt_schema_specs[&owned->schema] = &owned->spec;
    registry.opt_schema_descs[&owned->schema] = owned->descs;
    registry.opt_schemas[key] = std::move(owned);
    return 0;
}

extern "C" GED_EXPORT int
ged_register_command_native_schema(const char *name, const struct bu_cmd_schema *schema)
{
    struct bu_vls lint_msgs = BU_VLS_INIT_ZERO;
    int lint_failures;

    if (!name || !schema)
	return -1;
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_shutting_down)
	return -1;
    std::string key(name);
    if (!ged_command_key_valid(key) || key.compare(0, 6, "_mged_") == 0)
	return -1;

    lint_failures = ged_native_schema_lint_node(name, schema, &lint_msgs);
    if (lint_failures) {
	bu_vls_printf(&init_msgs, "native command schema '%s' rejected:\n%s", name,
	    bu_vls_addr(&lint_msgs));
	bu_vls_free(&lint_msgs);
	return -1;
    }
	bu_vls_free(&lint_msgs);
    struct ged_metadata_registry &registry = ged_metadata();
    std::lock_guard<std::mutex> guard(registry.mutex);
    if (registry.native_schemas.find(key) != registry.native_schemas.end() ||
	registry.grammars.find(key) != registry.grammars.end())
	return 1;

    registry.native_schemas[key] = schema;
    return 0;
}

extern "C" int
ged_register_command_grammar(const char *name, const struct ged_cmd_grammar *grammar)
{
    struct bu_vls lint_msgs = BU_VLS_INIT_ZERO;
    int lint_failures;

    if (!name || !grammar)
	return -1;
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_shutting_down)
	return -1;
    std::string key(name);
    if (!ged_command_key_valid(key) || key.compare(0, 6, "_mged_") == 0)
	return -1;

    /* Registration can occur while the static registry is still being
     * assembled.  Check the declaration itself here, but defer the optional
     * command-owned lint callback until the completed registry is queried: a
     * callback may legitimately inspect sibling commands. */
    lint_failures = ged_grammar_lint_declaration(name, grammar, &lint_msgs);
    if (lint_failures) {
	bu_vls_printf(&init_msgs, "command grammar '%s' rejected:\n%s", name,
	    bu_vls_addr(&lint_msgs));
	bu_vls_free(&lint_msgs);
	return -1;
    }
    bu_vls_free(&lint_msgs);

    struct ged_metadata_registry &registry = ged_metadata();
    std::lock_guard<std::mutex> guard(registry.mutex);
    if (registry.grammars.find(key) != registry.grammars.end() ||
	registry.native_schemas.find(key) != registry.native_schemas.end())
	return 1;

    registry.grammars[key] = grammar;
    return 0;
}

extern "C" int
ged_command_exists(const char *name)
{
    if (!name)
	return 0;

    ged_ensure_initialized();
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    return bu_plugin_cmd_exists(name);
}

extern "C" size_t
ged_registered_count(void)
{
    ged_ensure_initialized();
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    return bu_plugin_cmd_count();
}

extern "C" void
ged_list_command_names(struct bu_vls *out_csv)
{
    ged_ensure_initialized();
    if (!out_csv) return;
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    bu_vls_trunc(out_csv, 0);
    auto cb = [](const char *name, bu_plugin_cmd_impl impl, void *ud) -> int {
	(void)impl;
	struct bu_vls *v = (struct bu_vls *)ud;
	if (!name) return 0;
	if (bu_strncmp(name, "_mged_", 6) == 0) return 0; /* skip synthetic aliases */
	if (bu_vls_strlen(v) > 0) bu_vls_printf(v, ",");
	bu_vls_printf(v, "%s", name);
	return 0;
    };
    /* Iterate sorted names via generalized registry */
    bu_plugin_cmd_foreach(cb, (void *)out_csv);
}

extern "C" void
ged_list_command_array(const char * const **cl, size_t *cnt)
{
    ged_ensure_initialized();
    if (!cl || !cnt) return;
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    std::vector<std::string> names;
    auto cb = [](const char *name, bu_plugin_cmd_impl impl, void *ud) -> int {
	(void)impl;
	if (!name) return 0;
	if (bu_strncmp(name, "_mged_", 6) == 0) return 0;
	std::vector<std::string> *v = (std::vector<std::string> *)ud;
	v->push_back(std::string(name));
	return 0;
    };
    bu_plugin_cmd_foreach(cb, (void *)&names);
    char **alist = (char **)bu_calloc(names.size() + 1, sizeof(char *),
	"ged cmd argv");
    size_t len = 0;
    for (auto &n : names) {
	alist[len++] = bu_strdup(n.c_str());
    }
    *cl = (const char * const *)alist;
    *cnt = len;
}

/* -------------------------------------------------------------------------- */
/* Plugin Loading                                                             */
/* -------------------------------------------------------------------------- */

static void
scan_plugins(void)
{
    const char *env_block = getenv("GED_NO_PLUGIN_SCAN");
    if (env_block && BU_STR_EQUAL(env_block, "1")) {
	return;
    }

    const char *ppath = bu_dir(NULL, 0, BU_DIR_LIBEXEC, "ged", NULL);
    if (!ppath) {
	return;
    }

    struct bu_vls pattern = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&pattern, "*%s", GED_PLUGIN_SUFFIX);
    char **ged_filenames = NULL;
    size_t ged_nfiles = bu_file_list(ppath, bu_vls_cstr(&pattern), &ged_filenames);

    for (size_t i = 0; i < ged_nfiles; i++) {
	char pfile[MAXPATHLEN] = {0};
	bu_dir(pfile, MAXPATHLEN, BU_DIR_LIBEXEC, "ged", ged_filenames[i], NULL);

	if (!bu_plugin_path_allowed(pfile)) {
	    bu_vls_printf(&init_msgs, "plugin '%s': path rejected by host policy\n", pfile);
	    continue;
	}

	/* Open once and retain this exact handle only after both manifests and all
	 * command/schema pairs have registered successfully. */
	void *handle = bu_dlopen(pfile, BU_RTLD_NOW | BU_RTLD_LOCAL);
	if (!handle) {
	    const char *loader_error = bu_dlerror();
	    bu_vls_printf(&init_msgs, "plugin '%s': unable to load: %s\n", pfile,
		loader_error ? loader_error : "unknown dynamic loader error");
	    continue;
	}

	typedef const bu_plugin_manifest *(*plugin_info_fn)(void);
	void *plugin_info_val = bu_dlsym(handle, BU_PLUGIN_MANIFEST_SYM);
	plugin_info_fn plugin_info = (plugin_info_fn)(intptr_t)plugin_info_val;
	const bu_plugin_manifest *plugin_manifest = plugin_info ? plugin_info() : NULL;
	if (!plugin_manifest || plugin_manifest->abi_version != BU_PLUGIN_ABI_VERSION ||
		plugin_manifest->struct_size < sizeof(bu_plugin_manifest) ||
		plugin_manifest->cmd_count > 8192U ||
		(plugin_manifest->cmd_count && !plugin_manifest->commands)) {
	    bu_vls_printf(&init_msgs, "plugin '%s': invalid generalized manifest\n", pfile);
	    (void)bu_dlclose(handle);
	    continue;
	}

	std::unordered_map<std::string, const struct ged_cmd_schema *> schemas;
	bool manifest_valid = true;
	void *schema_info_val = bu_dlsym(handle, "ged_plugin_schema_info");
	if (schema_info_val) {
	    typedef const struct ged_plugin_schema_manifest *(*schema_info_fn)(void);
	    schema_info_fn schema_info = (schema_info_fn)(intptr_t)schema_info_val;
	    const struct ged_plugin_schema_manifest *schema_manifest = schema_info();
	    manifest_valid = schema_manifest &&
		schema_manifest->struct_size >= sizeof(struct ged_plugin_schema_manifest) &&
		schema_manifest->abi_version == GED_PLUGIN_SCHEMA_ABI_VERSION &&
		schema_manifest->schema_count < 8192U &&
		(!schema_manifest->schema_count || schema_manifest->schemas);
	    for (unsigned int si = 0; manifest_valid &&
		    si < schema_manifest->schema_count; si++) {
		const struct ged_cmd_schema *schema = &schema_manifest->schemas[si];
		size_t kinds = (schema->native_schema ? 1 : 0) +
		    (schema->grammar ? 1 : 0) + (schema->opt_spec ? 1 : 0);
		if (BU_STR_EMPTY(schema->cname) || kinds > 1 ||
			!schemas.emplace(schema->cname, schema).second)
		    manifest_valid = false;
	    }
	}
	if (!manifest_valid) {
	    bu_vls_printf(&init_msgs, "plugin '%s': invalid schema manifest\n", pfile);
	    (void)bu_dlclose(handle);
	    continue;
	}

	std::vector<std::pair<std::string, ged_func_ptr>> registered;
	std::unordered_set<std::string> command_names;
	for (unsigned int ci = 0; manifest_valid && ci < plugin_manifest->cmd_count; ci++) {
	    const bu_plugin_cmd *command = &plugin_manifest->commands[ci];
	    if (BU_STR_EMPTY(command->name) || !command->impl ||
		    !command_names.insert(command->name).second) {
		manifest_valid = false;
		break;
	    }
	    auto schema_it = schemas.find(command->name);
	    const struct ged_cmd_schema *schema = schema_it == schemas.end() ?
		NULL : schema_it->second;
	    struct ged_cmd_impl impl = {
		command->name, command->impl, GED_CMD_DEFAULT,
		schema ? schema->native_schema : NULL,
		schema ? schema->grammar : NULL,
		schema ? schema->opt_spec : NULL
	    };
	    const struct ged_cmd command_record = {&impl};
	    int command_ret = ged_register_command(&command_record);
	    if (command_ret < 0) {
		manifest_valid = false;
		break;
	    }
	    if (!command_ret)
		registered.emplace_back(command->name, command->impl);
	    if (schema_it != schemas.end())
		schemas.erase(schema_it);
	}
	if (!schemas.empty())
	    manifest_valid = false;

	if (!manifest_valid) {
	    for (auto it = registered.rbegin(); it != registered.rend(); ++it) {
		std::string alias = std::string("_mged_") + it->first;
		const bu_plugin_cmd rollback[] = {
		    {it->first.c_str(), it->second}, {alias.c_str(), it->second}
		};
		(void)bu_plugin_cmd_unregister_batch(rollback, 2);
		ged_metadata_erase_locked(it->first);
	    }
	    bu_vls_printf(&init_msgs, "plugin '%s': registration transaction rejected\n", pfile);
	    (void)bu_dlclose(handle);
	    continue;
	}

	if (!registered.empty())
	    ged_plugin_handles().push_back(handle);
	else
	    (void)bu_dlclose(handle);
    }

    bu_vls_free(&pattern);
    bu_argv_free(ged_nfiles, ged_filenames);
}

/* -------------------------------------------------------------------------- */
/* Initialization & Shutdown                                                  */
/* -------------------------------------------------------------------------- */
extern "C" void ged_force_static_registration(void);

/* Phase 0: logger to funnel bu_plugin messages into init_msgs (avoid stdout/stderr) */
static void _ged_plugin_logger(int level, const char *msg)
{
    (void)level;
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (msg) bu_vls_printf(&init_msgs, "%s\n", msg);
}

static void
libged_init(void)
{
    /* Bootstrap generalized registry and set logger */
    bu_plugin_set_logger(_ged_plugin_logger);
    (void)bu_plugin_init();

#if defined(LIBGED_STATIC_CORE)
    ged_force_static_registration();
#endif

    /* At this point, static constructors may or may not have run for all TUs.
     * Any that have will have already populated the registry through ged_register_command.
     * We proceed to scan plugins once to add plugin-only and user-provided commands.
     */
    scan_plugins();
}

extern "C" void ged_ensure_initialized()
{
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_initialized)
	return;
    ged_shutting_down = false;
    libged_init();
    ged_initialized = true;
}


static void
ged_registry_reference_add_locked(void)
{
    ged_active_references++;
    ged_thread_active_references++;
}


extern "C" int
_ged_registry_access_acquire(void)
{
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_shutting_down && !ged_thread_active_references)
	return -1;
    ged_registry_reference_add_locked();
    return 0;
}


extern "C" int
_ged_registry_mutation_acquire(void)
{
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_shutting_down)
	return -1;
    ged_registry_reference_add_locked();
    return 0;
}


extern "C" void
_ged_registry_access_release(void)
{
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (!ged_active_references || !ged_thread_active_references)
	return;
    ged_active_references--;
    ged_thread_active_references--;
    if (!ged_active_references)
	ged_registry_condition().notify_all();
}


extern "C" size_t
_ged_registry_access_depth(void)
{
    return ged_thread_active_references;
}


namespace {

class ged_registry_reference {
    public:
	explicit ged_registry_reference(bool mutation = false) : held(
	    (mutation ? _ged_registry_mutation_acquire() :
	     _ged_registry_access_acquire()) == 0) {}
	~ged_registry_reference()
	{
	    if (held)
		_ged_registry_access_release();
	}
	bool acquired() const { return held; }

    private:
	bool held;
};

}

extern "C" ged_func_ptr
_ged_cmd_func(const char *name)
{
    if (BU_STR_EMPTY(name))
	return GED_FUNC_PTR_NULL;
    ged_ensure_initialized();
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_shutting_down && !ged_thread_active_references)
	return GED_FUNC_PTR_NULL;
    return (ged_func_ptr)bu_plugin_cmd_get(name);
}

extern "C" ged_func_ptr
_ged_cmd_acquire(const char *name)
{
    if (BU_STR_EMPTY(name))
	return GED_FUNC_PTR_NULL;
    ged_ensure_initialized();
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_shutting_down && !ged_thread_active_references)
	return GED_FUNC_PTR_NULL;
    ged_func_ptr func = (ged_func_ptr)bu_plugin_cmd_get(name);
    if (func)
	ged_registry_reference_add_locked();
    return func;
}

extern "C" void
_ged_cmd_release(void)
{
    _ged_registry_access_release();
}

extern "C" void
libged_shutdown(void)
{
    std::unique_lock<std::recursive_mutex> transaction(ged_registry_mutex());
    if (ged_thread_active_references) {
	bu_log("libged_shutdown: refusing shutdown from an active libged operation\n");
	return;
    }
    ged_shutting_down = true;
    ged_registry_condition().wait(transaction, [] { return !ged_active_references; });

    /* Metadata and provider records contain pointers into plugin modules, so
     * tear them down before unloading retained handles. */
    _ged_cmd_semantic_provider_registry_reset();

    {
	struct ged_metadata_registry &registry = ged_metadata();
	std::lock_guard<std::mutex> guard(registry.mutex);
	registry.native_schemas.clear();
	registry.grammars.clear();
	registry.opt_schema_specs.clear();
	registry.opt_schema_descs.clear();
	registry.opt_schemas.clear();
    }

    bu_plugin_shutdown();
    std::vector<void *> &handles = ged_plugin_handles();
    for (auto it = handles.rbegin(); it != handles.rend(); ++it)
	if (*it) (void)bu_dlclose(*it);
    handles.clear();
    bu_vls_free(&init_msgs);
    ged_initialized = false;
    ged_shutting_down = false;
}

/* Provide existing init message accessor */
extern "C" const char *
ged_init_msgs()
{
    ged_ensure_initialized();
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    static thread_local std::string snapshot;
    snapshot = bu_vls_cstr(&init_msgs);
    return snapshot.c_str();
}

/* Backwards compatibility wrappers for old APIs (if needed) */

extern "C" int
ged_cmd_exists(const char *cmd)
{
    ged_ensure_initialized();
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    return bu_plugin_cmd_exists(cmd);
}

extern "C" int
ged_cmd_schema_exists(const char *cmd)
{
    if (!cmd)
	return 0;

    ged_ensure_initialized();
    ged_registry_reference reference;
    if (!reference.acquired())
	return 0;
    return (_ged_cmd_native_schema(cmd) != NULL || _ged_cmd_grammar(cmd) != NULL) ? 1 : 0;
}

extern "C" char *
ged_cmd_schema_json(const char *cmd)
{
    if (!cmd)
	return NULL;

    ged_ensure_initialized();
    ged_registry_reference reference;
    if (!reference.acquired())
	return NULL;
    const struct ged_cmd_grammar *grammar = _ged_cmd_grammar(cmd);
    if (grammar && grammar->describe_json)
	return grammar->describe_json();
    const struct bu_cmd_schema *native_schema = _ged_cmd_native_schema(cmd);
    if (native_schema)
	return bu_cmd_schema_describe_json(native_schema);
    return NULL;
}

extern "C" char *
ged_cmd_help(const char *cmd, const char *invocation)
{
    if (!cmd)
	return NULL;

    ged_ensure_initialized();
    ged_registry_reference reference;
    if (!reference.acquired())
	return NULL;
    const struct ged_cmd_grammar *grammar = _ged_cmd_grammar(cmd);
    if (grammar && grammar->describe_help)
	return grammar->describe_help(invocation);
    const struct bu_cmd_schema *native_schema = _ged_cmd_native_schema(cmd);
    return native_schema ? bu_cmd_schema_help(native_schema, invocation) : NULL;
}

extern "C" int
ged_cmd_help_append(struct bu_vls *output, const char *cmd, const char *invocation)
{
    if (!output)
	return -1;

    char *help = ged_cmd_help(cmd, invocation);
    if (!help)
	return -1;

    bu_vls_strcat(output, help);
    bu_free(help, "GED command help");
    return 0;
}

extern "C" const struct bu_cmd_schema *
_ged_cmd_native_schema(const char *cmd)
{
    if (!cmd)
	return NULL;
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());

    const char *lookup = cmd;
    if (bu_strncmp(lookup, "_mged_", 6) == 0)
	lookup += 6;

    struct ged_metadata_registry &registry = ged_metadata();
    std::lock_guard<std::mutex> guard(registry.mutex);
    auto it = registry.native_schemas.find(std::string(lookup));
    if (it == registry.native_schemas.end())
	return NULL;

    return it->second;
}

extern "C" const struct ged_cmd_grammar *
_ged_cmd_grammar(const char *cmd)
{
    if (!cmd)
	return NULL;
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());

    const char *lookup = cmd;
    if (bu_strncmp(lookup, "_mged_", 6) == 0)
	lookup += 6;

    struct ged_metadata_registry &registry = ged_metadata();
    std::lock_guard<std::mutex> guard(registry.mutex);
    auto it = registry.grammars.find(std::string(lookup));
    if (it == registry.grammars.end())
	return NULL;

    return it->second;
}

static int
ged_schema_lint_provider(const char *path, const char *role, const char *provider, struct bu_vls *msgs)
{
    if (BU_STR_EMPTY(provider))
	return 0;
    if (ged_cmd_semantic_provider_exists(provider))
	return 0;
    if (msgs)
	bu_vls_printf(msgs, "%s: unresolved %s semantic provider \"%s\"\n", path, role, provider);
    return 1;
}


static int
ged_native_schema_lint_node(const char *path, const struct bu_cmd_schema *schema, struct bu_vls *msgs)
{
    int failures;

    if (!schema) {
	return bu_cmd_schema_lint(schema, msgs);
    }
    failures = bu_cmd_schema_lint(schema, msgs);
    if (schema->options) {
	for (size_t i = 0; bu_cmd_option_is_valid(&schema->options[i]); i++) {
	    const struct bu_cmd_option *option = &schema->options[i];
	    failures += ged_schema_lint_provider(path, "native option", option->semantic_provider, msgs);
	}
    }
    if (schema->operands) {
	for (size_t i = 0; schema->operands[i].name; i++) {
	    const struct bu_cmd_operand *operand = &schema->operands[i];
	    failures += ged_schema_lint_provider(path, "native operand", operand->semantic_provider, msgs);
	}
    }

    return failures;
}

static int
ged_grammar_lint_declaration(const char *path, const struct ged_cmd_grammar *grammar,
	struct bu_vls *msgs)
{
    int failures = 0;

    if (!grammar) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: null grammar adapter\n", path ? path : "(null)");
	return 1;
    }
    if (BU_STR_EMPTY(grammar->name)) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: grammar adapter has no name\n", path);
	failures++;
    }
    if (BU_STR_EMPTY(grammar->help)) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: grammar adapter has no help text\n", path);
	failures++;
    }
    if (!grammar->validate || !grammar->analyze || !grammar->describe_json ||
	    !grammar->describe_help) {
	if (msgs)
	    bu_vls_printf(msgs,
		"%s: grammar adapter is missing required validation, analysis, JSON, or help hooks\n",
		path);
	failures++;
    }
    return failures;
}


static int
ged_grammar_lint_node(const char *path, const struct ged_cmd_grammar *grammar,
	struct bu_vls *msgs)
{
    int failures = ged_grammar_lint_declaration(path, grammar, msgs);
    if (grammar && grammar->lint)
	failures += grammar->lint(msgs);
    return failures;
}

extern "C" int
ged_cmd_schema_lint(const char *cmd, struct bu_vls *msgs)
{
    int failures = 0;

    ged_ensure_initialized();
    ged_registry_reference reference;
    if (!reference.acquired())
	return -1;

    if (cmd) {
	const struct bu_cmd_schema *native_schema = _ged_cmd_native_schema(cmd);
	const struct ged_cmd_grammar *grammar = _ged_cmd_grammar(cmd);
	if (!native_schema && !grammar) {
	    if (msgs)
		bu_vls_printf(msgs, "%s: schema metadata unavailable\n", cmd);
	    return 1;
	}
	if (native_schema)
	    failures += ged_native_schema_lint_node(cmd, native_schema, msgs);
	if (grammar)
	    failures += ged_grammar_lint_node(cmd, grammar, msgs);
	return failures;
    }

    std::map<std::string, const struct bu_cmd_schema *> native_schemas;
    std::map<std::string, const struct ged_cmd_grammar *> grammars;
    {
	struct ged_metadata_registry &registry = ged_metadata();
	std::lock_guard<std::mutex> guard(registry.mutex);
	native_schemas = registry.native_schemas;
	grammars = registry.grammars;
    }
    for (const auto &entry : native_schemas) {
	failures += ged_native_schema_lint_node(entry.first.c_str(), entry.second, msgs);
    }
    for (const auto &entry : grammars) {
	failures += ged_grammar_lint_node(entry.first.c_str(), entry.second, msgs);
    }

    return failures;
}

extern "C" int
ged_cmd_same(const char *cmd1, const char *cmd2)
{
    ged_ensure_initialized();
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    bu_plugin_cmd_impl c1 = bu_plugin_cmd_get(cmd1);
    bu_plugin_cmd_impl c2 = bu_plugin_cmd_get(cmd2);
    if (!c1 || !c2) return 0;
    return (c1 == c2) ? 1 : 0;
}

/* Edit distance lookup retained for help suggestions */
extern "C" int
ged_cmd_lookup(const char **ncmd, const char *cmd)
{
    if (!ncmd) return -1;
    *ncmd = NULL;
    if (!cmd) return -1;
    ged_ensure_initialized();
    std::lock_guard<std::recursive_mutex> transaction(ged_registry_mutex());
    size_t min_dist = (size_t)LONG_MAX;
    static thread_local std::string closest;
    closest.clear();
    auto cb = [](const char *name, bu_plugin_cmd_impl impl, void *ud) -> int {
	(void)impl;
	struct {
	    const char *target;
	    size_t *min_dist;
	    std::string *closest;
	} *ctx = (decltype(ctx))ud;
	if (!name || bu_strncmp(name, "_mged_", 6) == 0) return 0;
	size_t edist = bu_editdist(ctx->target, name);
	if (edist < *(ctx->min_dist)) {
	    *(ctx->min_dist) = edist;
	    *(ctx->closest) = name;
	}
	return 0;
    };
    struct { const char *target; size_t *min_dist; std::string *closest; } ctx = {
	cmd, &min_dist, &closest
    };
    bu_plugin_cmd_foreach(cb, (void *)&ctx);
    if (closest.empty())
	return -1;
    *ncmd = closest.c_str();
    return (int)min_dist;
}

extern "C" size_t
ged_cmd_list(const char * const **cl)
{
    ged_ensure_initialized();
    size_t cnt = 0;
    ged_list_command_array(cl, &cnt);
    return cnt;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
