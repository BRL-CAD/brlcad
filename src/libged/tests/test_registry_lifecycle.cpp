/*             T E S T _ R E G I S T R Y _ L I F E C Y C L E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#include <bu.h>
#include <ged.h>

#include "../include/plugin.h"

static int
transaction_probe_a(struct ged *, int, const char *[])
{
    return 11;
}

static int
transaction_probe_b(struct ged *, int, const char *[])
{
    return 22;
}

static const struct bu_cmd_schema transaction_schema_a =
    BU_CMD_SCHEMA("registry_transaction_probe", "transaction declaration A",
	NULL, NULL, BU_CMD_PARSE_INTERSPERSED,
	BU_CMD_SCHEMA_META_EXTERNAL(NULL, NULL, NULL));
static const struct bu_cmd_schema transaction_schema_b =
    BU_CMD_SCHEMA("registry_transaction_probe", "transaction declaration B",
	NULL, NULL, BU_CMD_PARSE_INTERSPERSED,
	BU_CMD_SCHEMA_META_EXTERNAL(NULL, NULL, NULL));
static const struct bu_cmd_schema metadata_collision_schema =
    BU_CMD_SCHEMA("registry_metadata_collision_probe", "metadata collision declaration",
	NULL, NULL, BU_CMD_PARSE_INTERSPERSED,
	BU_CMD_SCHEMA_META_EXTERNAL(NULL, NULL, NULL));

static struct ged_cmd_impl transaction_impl_a = {
    "registry_transaction_probe", transaction_probe_a, GED_CMD_DEFAULT,
    &transaction_schema_a, NULL, NULL
};
static struct ged_cmd_impl transaction_impl_b = {
    "registry_transaction_probe", transaction_probe_b, GED_CMD_DEFAULT,
    &transaction_schema_b, NULL, NULL
};
static struct ged_cmd transaction_command_a = {&transaction_impl_a};
static struct ged_cmd transaction_command_b = {&transaction_impl_b};

static std::mutex help_mutex;
static std::condition_variable help_condition;
static bool help_entered = false;
static bool help_continue = false;
static std::atomic<bool> nested_access_succeeded(false);

static int
blocking_validate(struct ged *, const char *, size_t,
	struct ged_cmd_validate_result *)
{
    return 0;
}

static int
blocking_analyze(struct ged *, const char *, struct ged_cmd_analysis *)
{
    return 0;
}

static char *
blocking_json(void)
{
    return bu_strdup("{\"name\":\"registry_shutdown_probe\"}");
}

static char *
blocking_help(const char *)
{
    {
	std::unique_lock<std::mutex> lock(help_mutex);
	help_entered = true;
	help_condition.notify_all();
	help_condition.wait(lock, []() { return help_continue; });
    }
    /* Shutdown has already begun.  An active metadata callback must still be
     * able to enter another metadata API before it relinquishes its pin. */
    char *nested = ged_cmd_schema_json("draw");
    nested_access_succeeded.store(nested != NULL, std::memory_order_release);
    if (nested)
	bu_free(nested, "nested registry JSON");
    return bu_strdup("registry shutdown probe help");
}

static const struct ged_cmd_grammar blocking_grammar = {
    "registry_shutdown_probe", "registry shutdown lifetime probe",
    blocking_validate, blocking_analyze, blocking_json, NULL, NULL,
    blocking_help
};

static const struct ged_cmd_grammar malformed_grammar = {
    "registry_malformed_grammar_probe", "malformed grammar probe",
    NULL, NULL, NULL, NULL, NULL, NULL
};
static const struct ged_cmd_grammar missing_help_grammar = {
    "registry_missing_help_probe", "missing generated help probe",
    blocking_validate, blocking_analyze, blocking_json, NULL, NULL, NULL
};

static struct ged_cmd_impl blocking_impl = {
    "registry_shutdown_probe", transaction_probe_a, GED_CMD_DEFAULT,
    NULL, &blocking_grammar, NULL
};
static struct ged_cmd blocking_command = {&blocking_impl};
static struct ged_cmd_impl malformed_grammar_impl = {
    "registry_malformed_grammar_probe", transaction_probe_a, GED_CMD_DEFAULT,
    NULL, &malformed_grammar, NULL
};
static struct ged_cmd malformed_grammar_command = {&malformed_grammar_impl};
static struct ged_cmd_impl missing_help_impl = {
    "registry_missing_help_probe", transaction_probe_a, GED_CMD_DEFAULT,
    NULL, &missing_help_grammar, NULL
};
static struct ged_cmd missing_help_command = {&missing_help_impl};
static struct ged_cmd_impl metadata_collision_impl = {
    "registry_metadata_collision_probe", transaction_probe_a, GED_CMD_DEFAULT,
    NULL, &blocking_grammar, NULL
};
static struct ged_cmd metadata_collision_command = {&metadata_collision_impl};
static struct ged_cmd_impl whitespace_impl = {
    " registry_bad_name", transaction_probe_a, GED_CMD_DEFAULT,
    NULL, NULL, NULL
};
static struct ged_cmd whitespace_command = {&whitespace_impl};
static struct ged_cmd_impl reserved_alias_impl = {
    "_mged_registry_bad_name", transaction_probe_a, GED_CMD_DEFAULT,
    NULL, NULL, NULL
};
static struct ged_cmd reserved_alias_command = {&reserved_alias_impl};

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;
    const struct ged_cmd_semantic_provider reserved_provider = {
	GED_CMD_PROVIDER_DB_PATH, NULL, NULL, NULL, NULL,
	BU_CMD_VALUE_DB_PATH, 0
    };
    if (ged_cmd_semantic_provider_register(&reserved_provider) != -1) {
	std::fprintf(stderr, "reserved provider was shadowed before first lookup\n");
	return 1;
    }
    const char * const *commands = NULL;
    size_t initial_count = ged_cmd_list(&commands);
    bool initial_draw = false;
    for (size_t i = 0; i < initial_count; i++)
	if (commands[i] && BU_STR_EQUAL(commands[i], "draw")) initial_draw = true;
    bool command_snapshot_terminated = commands && commands[initial_count] == NULL;
    bu_argv_free(initial_count, (char **)commands);
    if (!initial_count || !initial_draw || !command_snapshot_terminated ||
	    !ged_cmd_schema_exists("draw") ||
	    !ged_cmd_semantic_provider_exists(GED_CMD_PROVIDER_DB_PATH)) {
	std::fprintf(stderr, "initial GED registry is incomplete\n");
	return 1;
    }
    const char *suggestion = NULL;
    if (ged_cmd_lookup(&suggestion, "drwa") < 0 || !suggestion ||
	    !BU_STR_EQUAL(suggestion, "draw")) {
	std::fprintf(stderr, "closest-command lookup did not return stable public storage\n");
	return 1;
    }
    commands = NULL;
    size_t lookup_churn_count = ged_cmd_list(&commands);
    bu_argv_free(lookup_churn_count, (char **)commands);
    if (!BU_STR_EQUAL(suggestion, "draw")) {
	std::fprintf(stderr, "closest-command storage was invalidated by registry enumeration\n");
	return 1;
    }
    if (ged_cmd_lookup(&suggestion, "_mged_draw") < 0 || !suggestion ||
	    !bu_strncmp(suggestion, "_mged_", 6)) {
	std::fprintf(stderr, "closest-command lookup exposed a synthetic compatibility alias\n");
	return 1;
    }
    const char *startup_messages = ged_init_msgs();
    if (!startup_messages ||
	    !std::strstr(startup_messages, "invalid generalized manifest") ||
	    ged_cmd_exists("registry_invalid_manifest_probe") ||
	    ged_cmd_schema_exists("registry_invalid_manifest_probe")) {
	std::fprintf(stderr, "rejected plugin fixture published a command or schema\n");
	return 1;
    }

    int register_a = -2;
    int register_b = -2;
    std::thread first([&register_a]() {
	register_a = ged_register_command(&transaction_command_a);
    });
    std::thread second([&register_b]() {
	register_b = ged_register_command(&transaction_command_b);
    });
    first.join();
    second.join();
    if (!((register_a == 0 && register_b == 1) ||
	    (register_a == 1 && register_b == 0)) ||
	    !ged_cmd_same("registry_transaction_probe",
		"_mged_registry_transaction_probe")) {
	std::fprintf(stderr, "command/alias transaction was partially published (%d, %d)\n",
	    register_a, register_b);
	return 1;
    }
    char *transaction_json = ged_cmd_schema_json("registry_transaction_probe");
    const char *transaction_argv[] = {"registry_transaction_probe"};
    struct ged *transaction_gedp = ged_create();
    int transaction_result = transaction_gedp ?
	ged_exec(transaction_gedp, 1, transaction_argv) : -1;
    bool first_won = register_a == 0;
    bool declaration_matches = transaction_json &&
	std::strstr(transaction_json, first_won ? "declaration A" : "declaration B");
    if (transaction_json)
	bu_free(transaction_json, "transaction schema json");
    if (transaction_gedp)
	ged_close(transaction_gedp);
    if (!declaration_matches || transaction_result != (first_won ? 11 : 22)) {
	std::fprintf(stderr, "command implementation and schema came from different registrations\n");
	return 1;
    }
    int native_collision_seed = ged_register_command_native_schema(
	"registry_metadata_collision_probe", &metadata_collision_schema);
    int native_collision = ged_register_command(&metadata_collision_command);
    int malformed_registration = ged_register_command(&malformed_grammar_command);
    int missing_help_registration = ged_register_command(&missing_help_command);
    int whitespace_registration = ged_register_command(&whitespace_command);
    int reserved_alias_registration = ged_register_command(&reserved_alias_command);
    int reserved_schema_registration = ged_register_command_native_schema(
	"_mged_registry_bad_schema", &metadata_collision_schema);
    int whitespace_schema_registration = ged_register_command_native_schema(
	" registry_bad_schema", &metadata_collision_schema);
    if (native_collision_seed != 0 || native_collision != 1 ||
	ged_cmd_exists("registry_metadata_collision_probe") ||
	!ged_cmd_schema_exists("registry_metadata_collision_probe") ||
	malformed_registration != -1 ||
	ged_cmd_exists("registry_malformed_grammar_probe") ||
	ged_cmd_schema_exists("registry_malformed_grammar_probe") ||
	missing_help_registration != -1 ||
	ged_cmd_exists("registry_missing_help_probe") ||
	whitespace_registration != -1 || ged_cmd_exists("registry_bad_name") ||
	reserved_alias_registration != -1 || reserved_schema_registration != -1 ||
	whitespace_schema_registration != -1) {
	std::fprintf(stderr,
	    "invalid/conflicting metadata result: seed=%d collision=%d malformed=%d missing_help=%d whitespace=%d alias=%d reserved_schema=%d whitespace_schema=%d\n",
	    native_collision_seed, native_collision, malformed_registration,
	    missing_help_registration, whitespace_registration, reserved_alias_registration,
	    reserved_schema_registration, whitespace_schema_registration);
	return 1;
    }
    if (ged_register_command(&blocking_command) != 0 ||
	ged_register_command_native_schema("registry_shutdown_probe",
	    &transaction_schema_a) != 1) {
	std::fprintf(stderr, "blocking grammar fixture registration was not atomic\n");
	return 1;
    }
    const struct ged_cmd_semantic_provider transient_provider = {
	"registry.lifecycle_provider", NULL, NULL, NULL, NULL,
	BU_CMD_VALUE_UNKNOWN, 0
    };
    if (ged_cmd_semantic_provider_register(&transient_provider) != 0 ||
	    !ged_cmd_semantic_provider_exists(transient_provider.name)) {
	std::fprintf(stderr, "custom semantic provider registration failed\n");
	return 1;
    }
    struct ged_cmd_validate_result retained_validation =
	GED_CMD_VALIDATE_RESULT_NULL;
    struct ged_cmd_analysis retained_analysis = GED_CMD_ANALYSIS_NULL;
    ged_cmd_analysis_init(&retained_analysis);
    struct ged_cmd_completion_result retained_completion =
	GED_CMD_COMPLETION_RESULT_NULL;
    if (ged_cmd_validate(NULL, "draw sample", std::strlen("draw sample"),
	    &retained_validation) != 0 || !retained_validation.owned_strings ||
	    !retained_validation.hint || !retained_validation.semantic_provider ||
	ged_cmd_analyze(NULL, "draw sample", &retained_analysis) != 0 ||
	    !retained_analysis.owned_storage || retained_analysis.token_count < 2 ||
	    !retained_analysis.tokens[1].hint ||
	ged_cmd_complete_result(NULL, "draw --m", std::strlen("draw --m"),
	    &retained_completion) < 0 || !retained_completion.hint) {
	std::fprintf(stderr, "public command metadata results did not take string ownership\n");
	ged_cmd_validate_result_clear(&retained_validation);
	ged_cmd_analysis_clear(&retained_analysis);
	ged_cmd_completion_result_clear(&retained_completion);
	return 1;
    }
    const std::string retained_validation_hint(retained_validation.hint);
    const std::string retained_validation_provider(
	retained_validation.semantic_provider);
    const std::string retained_analysis_hint(retained_analysis.tokens[1].hint);
    const std::string retained_completion_hint(retained_completion.hint);
    char *help_result = NULL;
    std::atomic<bool> shutdown_finished(false);
    std::thread help_thread([&help_result]() {
	help_result = ged_cmd_help("registry_shutdown_probe", NULL);
    });
    {
	std::unique_lock<std::mutex> lock(help_mutex);
	if (!help_condition.wait_for(lock, std::chrono::seconds(10),
		[]() { return help_entered; })) {
	    help_continue = true;
	    help_condition.notify_all();
	    help_thread.join();
	    ged_cmd_validate_result_clear(&retained_validation);
	    ged_cmd_analysis_clear(&retained_analysis);
	    ged_cmd_completion_result_clear(&retained_completion);
	    std::fprintf(stderr, "blocking help callback was not entered\n");
	    return 1;
	}
    }
    std::thread shutdown_thread([&shutdown_finished]() {
	libged_shutdown();
	shutdown_finished.store(true, std::memory_order_release);
    });

    bool shutdown_rejected_mutation = false;
    const auto mutation_deadline = std::chrono::steady_clock::now() +
	std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < mutation_deadline) {
	if (ged_register_command(&transaction_command_a) == -1) {
	    shutdown_rejected_mutation = true;
	    break;
	}
	std::this_thread::yield();
    }
    const struct ged_cmd_semantic_provider shutdown_provider = {
	"registry.shutdown_provider", NULL, NULL, NULL, NULL,
	BU_CMD_VALUE_UNKNOWN, 0
    };
    bool provider_rejected = shutdown_rejected_mutation &&
	ged_cmd_semantic_provider_register(&shutdown_provider) == -1;
    bool shutdown_waited = !shutdown_finished.load(std::memory_order_acquire);
    {
	std::lock_guard<std::mutex> lock(help_mutex);
	help_continue = true;
    }
    help_condition.notify_all();
    help_thread.join();
    shutdown_thread.join();
    bool callback_result_ok = help_result &&
	BU_STR_EQUAL(help_result, "registry shutdown probe help");
    if (help_result)
	bu_free(help_result, "blocking registry help");
    if (!shutdown_rejected_mutation || !provider_rejected || !shutdown_waited ||
	    !callback_result_ok ||
	    !nested_access_succeeded.load(std::memory_order_acquire)) {
	std::fprintf(stderr, "shutdown did not pin callbacks or reject concurrent mutation\n");
	ged_cmd_validate_result_clear(&retained_validation);
	ged_cmd_analysis_clear(&retained_analysis);
	ged_cmd_completion_result_clear(&retained_completion);
	return 1;
    }
    if (retained_validation_hint != retained_validation.hint ||
	retained_validation_provider != retained_validation.semantic_provider ||
	retained_analysis_hint != retained_analysis.tokens[1].hint ||
	retained_completion_hint != retained_completion.hint) {
	std::fprintf(stderr, "published metadata strings did not survive registry shutdown\n");
	ged_cmd_validate_result_clear(&retained_validation);
	ged_cmd_analysis_clear(&retained_analysis);
	ged_cmd_completion_result_clear(&retained_completion);
	return 1;
    }
    ged_cmd_validate_result_clear(&retained_validation);
    ged_cmd_analysis_clear(&retained_analysis);
    ged_cmd_completion_result_clear(&retained_completion);

    commands = NULL;
    size_t restarted_count = ged_cmd_list(&commands);
    bool restarted_draw = false;
    for (size_t i = 0; i < restarted_count; i++)
	if (commands[i] && BU_STR_EQUAL(commands[i], "draw")) restarted_draw = true;
    bu_argv_free(restarted_count, (char **)commands);
    if (restarted_count != initial_count || !restarted_draw || !ged_cmd_schema_exists("draw") ||
	    !ged_cmd_semantic_provider_exists(GED_CMD_PROVIDER_DB_PATH) ||
	    ged_cmd_semantic_provider_exists(transient_provider.name)) {
	std::fprintf(stderr, "GED registry did not survive shutdown/reinitialization (%zu != %zu)\n",
	    restarted_count, initial_count);
	return 1;
    }

    libged_shutdown();
    libged_shutdown();
    return 0;
}
