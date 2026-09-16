/*                     B U _ P L U G I N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file bu_plugin.h
 *
 * @brief Cross-platform plugin system with command registry and dynamic loading
 *
 * This header provides a complete plugin infrastructure for applications that need
 * to support both built-in commands and dynamically loadable plugins. It handles
 * cross-platform symbol visibility, thread-safe command registration, ABI safety,
 * and lifecycle management.
 *
 * NOTE:  This header is explicitly NOT public libbu API - it is intended to be
 * internal to BRL-CAD, for reuse in various BRL-CAD libraries.  External
 * applications should NOT depend on this file when designing their own code.
 *
 * # Overview
 *
 * The BU plugin system provides:
 *   - Thread-safe command registry with O(1) lookup performance
 *   - Cross-platform dynamic library loading (Windows DLL, POSIX .so/.dylib)
 *   - ABI version checking to prevent incompatible plugin loads
 *   - Customizable command function signatures via preprocessor macros
 *   - Built-in command support for C++ applications
 *   - Security features: path validation callbacks and exception handling
 *   - Centralized logging with buffering support for startup scenarios
 *   - Multi-library support with namespace isolation
 *
 * # Command Signature Flexibility
 *
 * By default, commands use the signature: int (*)(void)
 *
 * However, applications can define custom signatures by setting BU_PLUGIN_CMD_RET
 * and BU_PLUGIN_CMD_ARGS macros before including this header. All core functionality
 * (registration, lookup, loading, iteration) works with any signature. The only
 * limitation is that bu_plugin_cmd_run() is only available for the default signature;
 * applications using custom signatures should provide their own wrapper functions.
 *
 * See Scenario 4 and tests/alt_signature for complete examples.
 *
 * # Basic Usage Scenarios
 *
 * ## Scenario 1: Creating a Host Application
 *
 * A host application uses bu_plugin.h to create a plugin-enabled executable:
 *
 * @code
 * // host_application.cpp
 * #define BU_PLUGIN_IMPLEMENTATION  // Enable implementation in this compilation unit
 * #include "bu_plugin.h"
 *
 * int main(int argc, char** argv) {
 *     // Initialize the plugin system
 *     bu_plugin_init();
 *
 *     // Load a plugin from disk
 *     int loaded = bu_plugin_load("./plugins/myplugin.so");
 *     if (loaded < 0) {
 *         fprintf(stderr, "Failed to load plugin\n");
 *         return 1;
 *     }
 *     printf("Loaded %d command(s) from plugin\n", loaded);
 *
 *     // Execute a command from the plugin
 *     int result = 0;
 *     int status = bu_plugin_cmd_run("mycommand", &result);
 *     if (status == 0) {
 *         printf("Command returned: %d\n", result);
 *     }
 *
 *     return 0;
 * }
 * @endcode
 *
 * ## Scenario 2: Creating a Dynamic Plugin
 *
 * A plugin exports commands that can be loaded by the host:
 *
 * @code
 * // myplugin.cpp
 * #include "bu_plugin.h"
 *
 * // Implement your command functions
 * static int hello_cmd(void) {
 *     printf("Hello from plugin!\n");
 *     return 42;
 * }
 *
 * static int goodbye_cmd(void) {
 *     printf("Goodbye from plugin!\n");
 *     return 0;
 * }
 *
 * // Define the commands array
 * static bu_plugin_cmd s_commands[] = {
 *     {"hello", hello_cmd},
 *     {"goodbye", goodbye_cmd}
 * };
 *
 * // Create the manifest
 * static bu_plugin_manifest s_manifest = {
 *     "myplugin",                           // plugin_name
 *     1,                                    // version
 *     2,                                    // cmd_count
 *     s_commands,                           // commands array
 *     BU_PLUGIN_ABI_VERSION,                // abi_version
 *     sizeof(bu_plugin_manifest)            // struct_size
 * };
 *
 * // Export the manifest (creates the bu_plugin_info symbol)
 * BU_PLUGIN_DECLARE_MANIFEST(s_manifest)
 * @endcode
 *
 * ## Scenario 3: Registering Built-in Commands (C++)
 *
 * C++ applications can register commands at static initialization time:
 *
 * @code
 * // builtin_commands.cpp
 * #include "bu_plugin.h"
 *
 * static int version_cmd(void) {
 *     printf("Version 1.0.0\n");
 *     return 0;
 * }
 *
 * static int help_cmd(void) {
 *     printf("Available commands: version, help\n");
 *     return 0;
 * }
 *
 * // Register commands automatically at startup
 * REGISTER_BU_PLUGIN_COMMAND("version", version_cmd)
 * REGISTER_BU_PLUGIN_COMMAND("help", help_cmd)
 * @endcode
 *
 * ## Scenario 4: Custom Command Signatures
 *
 * Applications can customize the command function signature by defining macros
 * before including bu_plugin.h. Note that bu_plugin_cmd_run() is only available
 * for the default signature; custom signatures require application-specific wrappers.
 *
 * @code
 * // host_with_custom_sig.cpp
 * // Define custom signature BEFORE including bu_plugin.h
 * #define BU_PLUGIN_CMD_RET int
 * #define BU_PLUGIN_CMD_ARGS int argc, const char** argv
 * #define BU_PLUGIN_IMPLEMENTATION
 * #include "bu_plugin.h"
 *
 * // Provide a custom wrapper function for the custom signature
 * extern "C" int my_cmd_run(const char *name, int argc, const char** argv, int *result) {
 *     bu_plugin_cmd_impl fn = bu_plugin_cmd_get(name);
 *     if (!fn) return -1;
 *     try {
 *         int ret = fn(argc, argv);  // Call with custom args
 *         if (result) *result = ret;
 *         return 0;
 *     } catch (...) {
 *         return -2;
 *     }
 * }
 *
 * // Now commands use: int (*)(int argc, const char** argv)
 * static int my_command(int argc, const char** argv) {
 *     printf("Received %d arguments\n", argc);
 *     for (int i = 0; i < argc; i++) {
 *         printf("  arg[%d]: %s\n", i, argv[i]);
 *     }
 *     return 0;
 * }
 * REGISTER_BU_PLUGIN_COMMAND("mycommand", my_command);
 *
 * // In plugin code (with same custom signature):
 * #define BU_PLUGIN_CMD_RET int
 * #define BU_PLUGIN_CMD_ARGS int argc, const char** argv
 * #define BU_PLUGIN_BUILDING_DLL
 * #include "bu_plugin.h"
 *
 * static int plugin_cmd(int argc, const char** argv) {
 *     // Implementation
 *     return 0;
 * }
 * static bu_plugin_cmd s_commands[] = {{"plugin_cmd", plugin_cmd}};
 * static bu_plugin_manifest s_manifest = {
 *     "my-plugin", 1, 1, s_commands,
 *     BU_PLUGIN_ABI_VERSION, sizeof(bu_plugin_manifest)
 * };
 * BU_PLUGIN_DECLARE_MANIFEST(s_manifest)
 *
 * // See tests/alt_signature for a complete working example
 * @endcode
 *
 * ## Scenario 5: Multi-Library Plugin Systems
 *
 * Multiple libraries can maintain separate plugin ecosystems using namespacing:
 *
 * @code
 * // Library 1: mylib1
 * // mylib1_plugin.cpp
 * #define BU_PLUGIN_NAME mylib1  // Namespace plugins as "mylib1"
 * #include "bu_plugin.h"
 *
 * static int lib1_cmd(void) { return 1; }
 * static bu_plugin_cmd s_lib1_cmds[] = {{"lib1_cmd", lib1_cmd}};
 * static bu_plugin_manifest s_lib1_manifest = {
 *     "mylib1_plugin", 1, 1, s_lib1_cmds,
 *     BU_PLUGIN_ABI_VERSION, sizeof(bu_plugin_manifest)
 * };
 * BU_PLUGIN_DECLARE_MANIFEST(s_lib1_manifest)
 * // Exports: mylib1_plugin_info()
 *
 * // Library 2: mylib2
 * // mylib2_plugin.cpp
 * #define BU_PLUGIN_NAME mylib2  // Namespace plugins as "mylib2"
 * #include "bu_plugin.h"
 *
 * static int lib2_cmd(void) { return 2; }
 * static bu_plugin_cmd s_lib2_cmds[] = {{"lib2_cmd", lib2_cmd}};
 * static bu_plugin_manifest s_lib2_manifest = {
 *     "mylib2_plugin", 1, 1, s_lib2_cmds,
 *     BU_PLUGIN_ABI_VERSION, sizeof(bu_plugin_manifest)
 * };
 * BU_PLUGIN_DECLARE_MANIFEST(s_lib2_manifest)
 * // Exports: mylib2_plugin_info()
 * @endcode
 *
 * ## Scenario 6: Iterating Over Registered Commands
 *
 * List all available commands (useful for help systems):
 *
 * @code
 * // Callback to print each command
 * static int print_cmd(const char *name, bu_plugin_cmd_impl impl, void *data) {
 *     (void)impl;  // unused
 *     (void)data;  // unused
 *     printf("  - %s\n", name);
 *     return 0;  // continue iteration
 * }
 *
 * int main(void) {
 *     bu_plugin_init();
 *     bu_plugin_load("./plugins/example.so");
 *
 *     printf("Available commands:\n");
 *     bu_plugin_cmd_foreach(print_cmd, NULL);
 *     return 0;
 * }
 * @endcode
 *
 * ## Scenario 7: Security - Path Validation
 *
 * Control which plugin paths can be loaded:
 *
 * @code
 * // Path validation callback
 * static int path_validator(const char *path) {
 *     // Only allow plugins from trusted directory
 *     const char *trusted_dir = "/opt/myapp/plugins/";
 *     return (strncmp(path, trusted_dir, strlen(trusted_dir)) == 0) ? 1 : 0;
 * }
 *
 * int main(void) {
 *     bu_plugin_init();
 *     bu_plugin_set_path_allow(path_validator);
 *
 *     // This will succeed
 *     bu_plugin_load("/opt/myapp/plugins/safe.so");
 *
 *     // This will be rejected
 *     bu_plugin_load("/tmp/untrusted.so");  // Denied by policy
 *     return 0;
 * }
 * @endcode
 *
 * ## Scenario 8: Logging Integration
 *
 * Integrate plugin system logs with your application's logging:
 *
 * @code
 * static void my_logger(int level, const char *msg) {
 *     const char *level_str = "INFO";
 *     if (level == BU_LOG_WARN) level_str = "WARN";
 *     if (level == BU_LOG_ERR) level_str = "ERROR";
 *     fprintf(stderr, "[%s] %s\n", level_str, msg);
 * }
 *
 * int main(void) {
 *     // Set logger before initialization to capture startup logs
 *     bu_plugin_set_logger(my_logger);
 *     bu_plugin_init();
 *
 *     // Or flush buffered startup logs later
 *     bu_plugin_flush_logs(my_logger);
 *
 *     bu_plugin_load("./plugin.so");  // Logs will use my_logger
 *     return 0;
 * }
 * @endcode
 *
 * # Build Configuration
 *
 * ## Default: Static or Header-Only Implementation
 *
 * Host library (compiles the implementation, no DLL export/import):
 * @code
 * // CMakeLists.txt
 * add_library(myhost host.cpp)
 * target_compile_definitions(myhost PRIVATE BU_PLUGIN_IMPLEMENTATION)
 * @endcode
 *
 * This is the recommended approach for most use cases. Each library that needs
 * the plugin implementation defines BU_PLUGIN_IMPLEMENTATION in one translation
 * unit without symbol conflicts.
 *
 * ## Optional: Exporting from a Shared Library (Windows DLL)
 *
 * If you want to export bu_plugin symbols from a shared library on Windows:
 * @code
 * // CMakeLists.txt
 * add_library(myhost SHARED host.cpp)
 * target_compile_definitions(myhost PRIVATE
 *     BU_PLUGIN_IMPLEMENTATION
 *     BU_PLUGIN_DLL_EXPORTS)
 * @endcode
 *
 * Client code linking to this DLL would define BU_PLUGIN_DLL_IMPORTS:
 * @code
 * add_library(myclient client.cpp)
 * target_compile_definitions(myclient PRIVATE BU_PLUGIN_DLL_IMPORTS)
 * target_link_libraries(myclient PRIVATE myhost)
 * @endcode
 *
 * Plugin library (uses the API):
 * @code
 * // CMakeLists.txt
 * add_library(myplugin MODULE plugin.cpp)
 * # No special definitions needed - plugins export their manifest with BU_PLUGIN_DECLARE_MANIFEST
 * @endcode
 *
 * # Advanced Features
 *
 * - **Thread Safety**: All registry operations are protected by mutex
 * - **ABI Safety**: Version and struct size validation prevent incompatible loads
 * - **Exception Handling**: C++ exceptions in commands are caught and logged
 * - **Duplicate Detection**: First-wins policy with warnings for duplicates
 * - **Name Normalization**: Leading/trailing whitespace automatically trimmed
 * - **Lifecycle Management**: Plugins kept loaded for process lifetime
 */

#ifndef BU_PLUGIN_H
#define BU_PLUGIN_H

#include "common.h"

#include <stddef.h>
#include "bu/defines.h"
#include "bu/dylib.h"

#ifdef __cplusplus
extern "C" {
#endif

    /*
     * ============================================================================
     * Cross-platform symbol visibility macros - EXPLICIT OPT-IN POLICY
     * ============================================================================
     *
     * This header provides a complete plugin implementation that is intended to be
     * reused across multiple BRL-CAD libraries. By default, bu_plugin.h does NOT
     * apply Windows DLL import/export decorations. This allows libraries to include
     * the implementation without symbol conflicts.
     *
     * # Default Behavior (no explicit opt-in)
     *
     * When neither BU_PLUGIN_DLL_EXPORTS nor BU_PLUGIN_DLL_IMPORTS is defined,
     * BU_PLUGIN_API expands to:
     *   - Empty on Windows (no __declspec decoration)
     *   - __attribute__((visibility("default"))) on GCC/Clang (if supported)
     *   - Empty otherwise
     *
     * This default is suitable for:
     *   - Static library builds
     *   - Header-only or inline implementation usage
     *   - Multiple libraries embedding the implementation separately
     *
     * # Explicit Opt-In for DLL Export/Import
     *
     * To enable Windows DLL symbol export/import decorations, define one of:
     *
     *   BU_PLUGIN_DLL_EXPORTS - When building a DLL that exports bu_plugin symbols
     *   BU_PLUGIN_DLL_IMPORTS - When using a DLL that exports bu_plugin symbols
     *
     * Example (exporting from a DLL):
     *   target_compile_definitions(mylib PRIVATE BU_PLUGIN_DLL_EXPORTS)
     *
     * Example (importing from a DLL):
     *   target_compile_definitions(client PRIVATE BU_PLUGIN_DLL_IMPORTS)
     *
     * NOTE: Defining both BU_PLUGIN_DLL_EXPORTS and BU_PLUGIN_DLL_IMPORTS
     * simultaneously is an error and will trigger a compile-time check.
     *
     * # BU_PLUGIN_IMPLEMENTATION
     *
     * BU_PLUGIN_IMPLEMENTATION controls whether the inline C++ implementation
     * section is compiled into the current translation unit. It is independent
     * of symbol export/import decorations. Define it in exactly one .cpp file
     * per library that needs the implementation.
     *
     * ============================================================================
     */

    /* Ensure mutual exclusivity of EXPORTS and IMPORTS */
#if defined(BU_PLUGIN_DLL_EXPORTS) && defined(BU_PLUGIN_DLL_IMPORTS)
#  error "BU_PLUGIN_DLL_EXPORTS and BU_PLUGIN_DLL_IMPORTS cannot both be defined"
#endif

    /* Internal helpers for actual symbol decoration */
#if defined(HAVE_WINDOWS_H)
#  define BU_PLUGIN_EXPORT_IMPL __declspec(dllexport)
#  define BU_PLUGIN_IMPORT_IMPL __declspec(dllimport)
#  define BU_PLUGIN_LOCAL_IMPL
#else
#  if (defined(__GNUC__) && __GNUC__ >= 4) || defined(__clang__)
#    define BU_PLUGIN_EXPORT_IMPL __attribute__ ((visibility ("default")))
#    define BU_PLUGIN_IMPORT_IMPL __attribute__ ((visibility ("default")))
#    define BU_PLUGIN_LOCAL_IMPL  __attribute__ ((visibility ("hidden")))
#  else
#    define BU_PLUGIN_EXPORT_IMPL
#    define BU_PLUGIN_IMPORT_IMPL
#    define BU_PLUGIN_LOCAL_IMPL
#  endif
#endif

    /*
     * BU_PLUGIN_API: Primary visibility macro for bu_plugin declarations/definitions.
     *
     * Applied to all public bu_plugin functions. The decoration depends on opt-in:
     *   - BU_PLUGIN_DLL_EXPORTS defined: dllexport (Windows) or visibility default (GCC/Clang)
     *   - BU_PLUGIN_DLL_IMPORTS defined: dllimport (Windows) or visibility default (GCC/Clang)
     *   - Neither defined (default): visibility default (GCC/Clang) or empty (Windows)
     */
#if defined(BU_PLUGIN_DLL_EXPORTS)
#  define BU_PLUGIN_API BU_PLUGIN_EXPORT_IMPL
#elif defined(BU_PLUGIN_DLL_IMPORTS)
#  define BU_PLUGIN_API BU_PLUGIN_IMPORT_IMPL
#else
    /* Default: no dllimport/dllexport on Windows, visibility default on GCC/Clang */
#  if defined(HAVE_WINDOWS_H)
#    define BU_PLUGIN_API
#  else
#    if (defined(__GNUC__) && __GNUC__ >= 4) || defined(__clang__)
#      define BU_PLUGIN_API __attribute__ ((visibility ("default")))
#    else
#      define BU_PLUGIN_API
#    endif
#  endif
#endif

    /*
     * BU_PLUGIN_EXPORT: Legacy macro for plugin manifest export.
     * Plugins use BU_PLUGIN_DECLARE_MANIFEST which needs dllexport on Windows.
     * This always exports regardless of BU_PLUGIN_API policy.
     */
#define BU_PLUGIN_EXPORT BU_PLUGIN_EXPORT_IMPL

    /*
     * BU_PLUGIN_LOCAL: For internal symbols (hidden visibility when supported).
     */
#define BU_PLUGIN_LOCAL BU_PLUGIN_LOCAL_IMPL

    /*
     * Log levels for the logger callback.
     */
#define BU_LOG_INFO  0
#define BU_LOG_WARN  1
#define BU_LOG_ERR   2

    /**
     * bu_plugin_logger_cb - Logger callback function type.
     * @param level  Log level (BU_LOG_INFO, BU_LOG_WARN, BU_LOG_ERR).
     * @param msg    The log message (null-terminated).
     */
    typedef void (*bu_plugin_logger_cb)(int level, const char *msg);

    /**
     * bu_plugin_path_allow_cb - Path allow policy callback function type.
     * @param path  The plugin path being checked.
     * @return 1 if the path is allowed, 0 if denied.
     */
    typedef int (*bu_plugin_path_allow_cb)(const char *path);

    /**
     * bu_plugin_set_logger - Set the logger callback.
     * @param cb  The callback function for logging. Pass NULL to buffer messages internally.
     *
     * When no logger is set (cb is NULL), log messages are buffered internally to avoid
     * writing to STDOUT/STDERR during early program startup. Use bu_plugin_flush_logs()
     * to retrieve buffered messages after the application has finished initializing.
     */
    BU_PLUGIN_API void bu_plugin_set_logger(bu_plugin_logger_cb cb);

    /**
     * bu_plugin_logf - Log a formatted message.
     * @param level  Log level (BU_LOG_INFO, BU_LOG_WARN, BU_LOG_ERR).
     * @param fmt    printf-style format string.
     * @param ...    Format arguments.
     *
     * If a logger callback is set, the message is passed to it immediately.
     * Otherwise, the message is buffered internally for later retrieval via
     * bu_plugin_flush_logs().
     */
    BU_PLUGIN_API void bu_plugin_logf(int level, const char *fmt, ...);

    /**
     * bu_plugin_flush_logs - Flush buffered startup logs to a callback.
     * @param cb  Callback function to receive each buffered log message.
     *            Called once for each buffered message with (level, msg).
     *
     * During early startup, when no logger callback is set, log messages are
     * buffered internally to avoid writing to STDOUT/STDERR. Call this function
     * after initialization is complete to process any startup messages.
     *
     * After flushing, the internal buffer is cleared.
     * If cb is NULL, the buffer is simply cleared without processing messages.
     */
    BU_PLUGIN_API void bu_plugin_flush_logs(bu_plugin_logger_cb cb);

    /**
     * bu_plugin_set_path_allow - Set the path allow policy callback.
     * @param cb  The callback to check if a path is allowed for loading.
     *            Pass NULL to allow all paths (default behavior).
     *
     * When set, bu_plugin_load will call this callback before attempting to load
     * a plugin. If the callback returns 0, the load is rejected.
     */
    BU_PLUGIN_API void bu_plugin_set_path_allow(bu_plugin_path_allow_cb cb);

    /*
     * Type definitions for plugin commands.
     * Applications can define BU_PLUGIN_CMD_RET and BU_PLUGIN_CMD_ARGS before
     * including this header to customize the command function signature.
     */

    /* Default return type is int */
#ifndef BU_PLUGIN_CMD_RET
#define BU_PLUGIN_CMD_RET int
#define BU_PLUGIN_CMD_RET_IS_DEFAULT
#endif

    /* Default arguments is void */
#ifndef BU_PLUGIN_CMD_ARGS
#define BU_PLUGIN_CMD_ARGS void
#define BU_PLUGIN_CMD_ARGS_IS_DEFAULT
#endif

    /* Detect if we're using the default signature */
#if defined(BU_PLUGIN_CMD_RET_IS_DEFAULT) && defined(BU_PLUGIN_CMD_ARGS_IS_DEFAULT)
#define BU_PLUGIN_DEFAULT_SIGNATURE
#endif

    /* Clean up helper macros immediately to avoid namespace pollution.
       BU_PLUGIN_DEFAULT_SIGNATURE is all we need going forward. */
#undef BU_PLUGIN_CMD_RET_IS_DEFAULT
#undef BU_PLUGIN_CMD_ARGS_IS_DEFAULT

    /**
     * bu_plugin_cmd_impl - Function pointer type for a plugin command implementation.
     * This is a default implementation; the host can define its own typedef
     * before including this header by defining BU_PLUGIN_CMD_IMPL_DEFINED.
     * For this test, it's simply: int (*)(void)
     */
#ifndef BU_PLUGIN_CMD_IMPL_DEFINED
    typedef BU_PLUGIN_CMD_RET (*bu_plugin_cmd_impl)(BU_PLUGIN_CMD_ARGS);
#define BU_PLUGIN_CMD_IMPL_DEFINED
#endif

    /**
     * bu_plugin_cmd - Descriptor for a single plugin command.
     */
    typedef struct bu_plugin_cmd {
	const char *name;           /* Command name */
	bu_plugin_cmd_impl impl;    /* Function pointer to implementation */
    } bu_plugin_cmd;

    /**
     * ABI version for bu_plugin_manifest. Increment when making breaking changes.
     */
#define BU_PLUGIN_ABI_VERSION 1

    /**
     * bu_plugin_manifest - Descriptor for a plugin's exported commands.
     *
     * For ABI safety, manifests include:
     *   - abi_version: Must match BU_PLUGIN_ABI_VERSION (currently 1)
     *   - struct_size: Must be >= sizeof(bu_plugin_manifest)
     *
     * This allows the loader to detect incompatible plugins.
     */
    typedef struct bu_plugin_manifest {
	const char *plugin_name;    /* Name of the plugin */
	unsigned int version;       /* Plugin manifest version (for compatibility checks) */
	unsigned int cmd_count;     /* Number of commands in the commands array */
	const bu_plugin_cmd *commands;  /* Array of command descriptors */
	unsigned int abi_version;   /* ABI version, must be BU_PLUGIN_ABI_VERSION */
	size_t struct_size;         /* Size of this struct, for forward compatibility */
    } bu_plugin_manifest;

    /*
     * Registry APIs - Declared here, implemented in the host library.
     */

    /**
     * bu_plugin_cmd_register - Register a command with the global registry.
     * @param name  The command name. Leading/trailing whitespace is trimmed.
     *              Internal whitespace triggers a warning but is allowed.
     * @param impl  The function pointer for the command implementation.
     * @return 0 on success, 1 if duplicate (first wins policy), -1 on error.
     *
     * Behavior:
     *   - Trims leading/trailing whitespace from name
     *   - Warns if internal whitespace is detected
     *   - Logs and returns 1 if a command with the same name already exists
     *   - Returns -1 for null/empty name or null impl
     */
    BU_PLUGIN_API int bu_plugin_cmd_register(const char *name, bu_plugin_cmd_impl impl);

    /**
     * bu_plugin_cmd_exists - Check if a command is registered.
     * @param name  The command name to check.
     * @return 1 if the command exists, 0 otherwise.
     */
    BU_PLUGIN_API int bu_plugin_cmd_exists(const char *name);

    /**
     * bu_plugin_cmd_get - Retrieve a command's implementation.
     * @param name  The command name to look up.
     * @return The function pointer, or NULL if not found.
     */
    BU_PLUGIN_API bu_plugin_cmd_impl bu_plugin_cmd_get(const char *name);

    /**
     * bu_plugin_cmd_count - Get the number of registered commands.
     * @return The count of registered commands.
     */
    BU_PLUGIN_API size_t bu_plugin_cmd_count(void);

    /**
     * bu_plugin_cmd_foreach - Iterate over all registered commands in sorted order.
     *
     * callback:  Function called for each command with (name, impl, user_data).
     * user_data: Opaque pointer passed to callback.
     *
     * Useful for listing all available commands (e.g., for help systems).
     * Commands are iterated in alphabetical order by name for stable output.
     * The callback should return 0 to continue, non-zero to stop iteration.
     *
     * Implementation note: Uses a snapshot pattern to minimize lock duration.
     * The registry is locked only while copying command names, then unlocked
     * before sorting and calling the callback.
     *
     * @code
     * // Callback to print each command name
     * static int print_command(const char *name, bu_plugin_cmd_impl impl, void *user_data) {
     *     (void)impl;       // unused
     *     (void)user_data;  // unused
     *     printf("  %s\n", name);
     *     return 0;  // continue iteration
     * }
     *
     * // Print sorted list of all available commands
     * void list_commands(void) {
     *     printf("Available commands:\n");
     *     bu_plugin_cmd_foreach(print_command, NULL);
     * }
     * @endcode
     */
    typedef int (*bu_plugin_cmd_callback)(const char *name, bu_plugin_cmd_impl impl, void *user_data);
    BU_PLUGIN_API void bu_plugin_cmd_foreach(bu_plugin_cmd_callback callback, void *user_data);

    /**
     * bu_plugin_init - Initialize the plugin registry (call once at startup).
     * @return 0 on success.
     *
     * Note: All registry operations are thread-safe, protected by a mutex.
     */
    BU_PLUGIN_API int bu_plugin_init(void);

#ifdef BU_PLUGIN_DEFAULT_SIGNATURE
    /**
     * bu_plugin_cmd_run - Safely run a registered command by name.
     * @param name  The command name to run.
     * @param result  Output parameter for the command's return value (can be NULL).
     * @return 0 on success, -1 if command not found, -2 if command threw an exception.
     *
     * On C++ builds, this function wraps the command execution in try/catch
     * to safely handle exceptions. The exception is logged but not re-thrown.
     *
     * Note: This function is only available when using the default command signature
     * int (*)(void). For custom signatures, applications should provide their own
     * wrapper functions tailored to the specific signature (see alt_sig_cmd_run in
     * the alternative signature test for an example).
     */
    BU_PLUGIN_API int bu_plugin_cmd_run(const char *name, BU_PLUGIN_CMD_RET *result);
#endif /* BU_PLUGIN_DEFAULT_SIGNATURE */

    /**
     * bu_plugin_load - Load a dynamic plugin from a shared library path.
     * @param path  Path to the shared library (.so, .dylib, .dll).
     * @return Number of commands registered from the plugin, or -1 on error.
     *
     * This function:
     *   - Enforces the path-allow policy (if set via bu_plugin_set_path_allow)
     *   - Windows: Converts UTF-8 path to UTF-16; uses LoadLibraryExW with safer flags
     *   - POSIX: Clears dlerror before dlsym for accurate error reporting
     *   - Validates manifest abi_version and struct_size
     *   - Detects and logs duplicate command names within a manifest
     *
     * Note: Plugins are kept loaded for the lifetime of the process. There is
     * currently no bu_plugin_unload() function. This is intentional for simplicity
     * and safety - unloading code that may have function pointers still in use
     * is error-prone.
     */
    BU_PLUGIN_API int bu_plugin_load(const char *path);

    /* Additional optional APIs (handles retained for lifetime, optional unload) */
    BU_PLUGIN_API size_t bu_plugin_loaded_modules_count(void);
    BU_PLUGIN_API void   bu_plugin_shutdown(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*
 * C++ helper macro for registering built-in commands at static initialization time.
 * Usage: REGISTER_BU_PLUGIN_COMMAND("cmdname", my_cmd_func);
 * This creates a static object whose constructor registers the command.
 * Uses __COUNTER__ for unique variable names to avoid collisions.
 */
#ifdef __cplusplus

/* Helper macros for unique identifier generation */
#define BU_PLUGIN_CONCAT_IMPL(a, b) a##b
#define BU_PLUGIN_CONCAT(a, b) BU_PLUGIN_CONCAT_IMPL(a, b)

#ifdef __COUNTER__
#define BU_PLUGIN_UNIQUE_ID BU_PLUGIN_CONCAT(_bu_plugin_cmd_registrar_, __COUNTER__)
#else
#define BU_PLUGIN_UNIQUE_ID BU_PLUGIN_CONCAT(_bu_plugin_cmd_registrar_, __LINE__)
#endif

namespace bu_plugin_detail {
struct CommandRegistrar {
    CommandRegistrar(const char *name, bu_plugin_cmd_impl impl) {
	bu_plugin_cmd_register(name, impl);
    }
};
}

#define REGISTER_BU_PLUGIN_COMMAND(name, impl) \
    static ::bu_plugin_detail::CommandRegistrar \
    BU_PLUGIN_UNIQUE_ID(name, impl) (name, impl)
#endif



/*
 * Macro for declaring the plugin manifest symbol in a dynamic plugin.
 * This exports a function that returns a pointer to the plugin's manifest.
 * Usage in plugin source:
 *   static bu_plugin_cmd s_commands[] = { {"example", example_impl}, ... };
 *   static bu_plugin_manifest s_manifest = { "myplugin", 1, 1, s_commands };
 *   BU_PLUGIN_DECLARE_MANIFEST(s_manifest)
 *
 * The host will dlsym() for a namespaced symbol "<host>_plugin_info".
 * To set the host namespace, define BU_PLUGIN_NAME (e.g., 'ged') before including.
 */
#ifndef BU_PLUGIN_NAME
#define BU_PLUGIN_NAME bu
#endif
#define BU_PLUGIN_CAT2_IMPL(a,b) a##b
#define BU_PLUGIN_CAT2(a,b) BU_PLUGIN_CAT2_IMPL(a,b)
#define BU_PLUGIN_STR1(x) #x
#define BU_PLUGIN_STR(x) BU_PLUGIN_STR1(x)
#define BU_PLUGIN_MANIFEST_FN  BU_PLUGIN_CAT2(BU_PLUGIN_NAME, _plugin_info)
#define BU_PLUGIN_MANIFEST_SYM BU_PLUGIN_STR(BU_PLUGIN_MANIFEST_FN)

#ifdef __cplusplus
#define BU_PLUGIN_DECLARE_MANIFEST(manifest_var) \
    extern "C" BU_PLUGIN_EXPORT const bu_plugin_manifest* BU_PLUGIN_MANIFEST_FN(void) { \
	return &(manifest_var); \
    }
#else
#define BU_PLUGIN_DECLARE_MANIFEST(manifest_var) \
    BU_PLUGIN_EXPORT const bu_plugin_manifest* BU_PLUGIN_MANIFEST_FN(void) { \
	return &(manifest_var); \
    }
#endif

/*
 * Built-in registry implementation (C++ only).
 * This is included in the host library when BU_PLUGIN_IMPLEMENTATION is defined.
 */
#if defined(BU_PLUGIN_IMPLEMENTATION) && defined(__cplusplus)

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cctype>
#include <mutex>
#include <vector>
#include <algorithm>
#include <exception>

#include "bio.h"

namespace bu_plugin_impl {

/**
 * Thread-safe registry using unordered_map for O(1) lookups.
 * Uses a static local to ensure initialization before first use.
 * Protected by a mutex for thread-safe access.
 */
static std::unordered_map<std::string, bu_plugin_cmd_impl>& get_registry() {
    static std::unordered_map<std::string, bu_plugin_cmd_impl> registry;
    return registry;
}

static std::mutex& get_mutex() {
    static std::mutex mtx;
    return mtx;
}

/* Logger callback storage */
static bu_plugin_logger_cb& get_logger() {
    static bu_plugin_logger_cb logger = nullptr;
    return logger;
}

/* Path allow callback storage */
static bu_plugin_path_allow_cb& get_path_allow() {
    static bu_plugin_path_allow_cb cb = nullptr;
    return cb;
}

/* Internal implementation detail - buffered startup log storage.
   Avoids writing to STDOUT/STDERR during early startup. */
struct BufferedLogEntry {
    int level;
    std::string msg;
};

static std::vector<BufferedLogEntry>& get_log_buffer() {
    static std::vector<BufferedLogEntry> buffer;
    return buffer;
}

static std::mutex& get_log_buffer_mutex() {
    static std::mutex mtx;
    return mtx;
}

/* Retained module handles (kept loaded for lifetime unless bu_plugin_shutdown is called) */
#if defined(HAVE_WINDOWS_H)
typedef HMODULE bu_plugin_module_handle_t;
#else
typedef void*   bu_plugin_module_handle_t;
#endif
static std::vector<bu_plugin_module_handle_t>& get_modules() {
    static std::vector<bu_plugin_module_handle_t> mods;
    return mods;
}

/* Trim leading/trailing whitespace from a string, returns trimmed copy */
static std::string trim_whitespace(const char *str) {
    if (!str) return "";
    const char *start = str;
    while (*start && std::isspace(static_cast<unsigned char>(*start))) {
	++start;
    }
    if (*start == '\0') return "";
    const char *end = str + std::strlen(str) - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(*end))) {
	--end;
    }
    return std::string(start, static_cast<size_t>(end - start + 1));
}

/* Check if string contains internal whitespace */
static bool has_internal_whitespace(const std::string& s) {
    for (size_t i = 0; i < s.size(); ++i) {
	if (std::isspace(static_cast<unsigned char>(s[i]))) {
	    return true;
	}
    }
    return false;
}

/* Conservative sanity bounds to avoid misinterpreting legacy or invalid manifests */
static inline bool manifest_is_plausible(const bu_plugin_manifest *m) {
    if (!m) return false;
    if (m->struct_size < sizeof(bu_plugin_manifest) || m->struct_size > 65536) return false;
    if (m->abi_version != BU_PLUGIN_ABI_VERSION) return false;
    if (m->cmd_count > 8192U) return false;
    if (m->cmd_count > 0 && !m->commands) return false;
    if (!m->plugin_name) return false;
    size_t nlen = std::strlen(m->plugin_name);
    if (nlen == 0 || nlen > 1024) return false;
    return true;
}

} /* namespace bu_plugin_impl */

extern "C" {

    BU_PLUGIN_API void bu_plugin_set_logger(bu_plugin_logger_cb cb) {
	bu_plugin_impl::get_logger() = cb;
    }

    BU_PLUGIN_API void bu_plugin_logf(int level, const char *fmt, ...) {
	/* Buffer size of 2048 should be sufficient for most log messages.
	   Messages longer than this will be truncated. */
	char buf[2048];
	va_list args;
	va_start(args, fmt);
#if defined(HAVE_WINDOWS_H)
	_vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, args);
#else
	vsnprintf(buf, sizeof(buf), fmt, args);
#endif
	va_end(args);
	buf[sizeof(buf) - 1] = '\0';

	bu_plugin_logger_cb logger = bu_plugin_impl::get_logger();
	if (logger) {
	    /* Logger callback is set - dispatch immediately */
	    logger(level, buf);
	} else {
	    /* No logger set - buffer internally to avoid STDOUT/STDERR during startup */
	    std::lock_guard<std::mutex> lock(bu_plugin_impl::get_log_buffer_mutex());
	    bu_plugin_impl::get_log_buffer().push_back({level, std::string(buf)});
	}
    }

    BU_PLUGIN_API void bu_plugin_flush_logs(bu_plugin_logger_cb cb) {
	std::vector<bu_plugin_impl::BufferedLogEntry> logs;
	{
	    std::lock_guard<std::mutex> lock(bu_plugin_impl::get_log_buffer_mutex());
	    logs = std::move(bu_plugin_impl::get_log_buffer());
	    /* Note: move leaves source in valid but unspecified state; clear ensures it's empty */
	    bu_plugin_impl::get_log_buffer().clear();
	}

	if (cb) {
	    for (const auto& entry : logs) {
		cb(entry.level, entry.msg.c_str());
	    }
	}
	/* If cb is NULL, logs are simply discarded */
    }

    BU_PLUGIN_API void bu_plugin_set_path_allow(bu_plugin_path_allow_cb cb) {
	bu_plugin_impl::get_path_allow() = cb;
    }

    BU_PLUGIN_API int bu_plugin_cmd_register(const char *name, bu_plugin_cmd_impl impl) {
	if (!name || !impl) return -1;

	/* Trim whitespace from name */
	std::string trimmed = bu_plugin_impl::trim_whitespace(name);
	if (trimmed.empty()) return -1;  /* Reject empty string names */

	/* Warn about internal whitespace */
	if (bu_plugin_impl::has_internal_whitespace(trimmed)) {
	    bu_plugin_logf(BU_LOG_WARN, "Command name '%s' contains internal whitespace", trimmed.c_str());
	}

	std::lock_guard<std::mutex> lock(bu_plugin_impl::get_mutex());
	auto& reg = bu_plugin_impl::get_registry();
	if (reg.find(trimmed) != reg.end()) {
	    bu_plugin_logf(BU_LOG_WARN, "Duplicate command '%s' ignored (first wins)", trimmed.c_str());
	    return 1; /* Duplicate - first wins */
	}
	reg[trimmed] = impl;
	return 0;
    }

    BU_PLUGIN_API int bu_plugin_cmd_exists(const char *name) {
	if (!name) return 0;
	std::string trimmed = bu_plugin_impl::trim_whitespace(name);
	if (trimmed.empty()) return 0;
	std::lock_guard<std::mutex> lock(bu_plugin_impl::get_mutex());
	auto& reg = bu_plugin_impl::get_registry();
	return reg.find(trimmed) != reg.end() ? 1 : 0;
    }

    BU_PLUGIN_API bu_plugin_cmd_impl bu_plugin_cmd_get(const char *name) {
	if (!name) return nullptr;
	std::string trimmed = bu_plugin_impl::trim_whitespace(name);
	if (trimmed.empty()) return nullptr;
	std::lock_guard<std::mutex> lock(bu_plugin_impl::get_mutex());
	auto& reg = bu_plugin_impl::get_registry();
	auto it = reg.find(trimmed);
	return (it != reg.end()) ? it->second : nullptr;
    }

    BU_PLUGIN_API size_t bu_plugin_cmd_count(void) {
	std::lock_guard<std::mutex> lock(bu_plugin_impl::get_mutex());
	return bu_plugin_impl::get_registry().size();
    }

    BU_PLUGIN_API void bu_plugin_cmd_foreach(bu_plugin_cmd_callback callback, void *user_data) {
	if (!callback) return;

	/* Snapshot the registry under lock, then release lock for iteration */
	std::vector<std::pair<std::string, bu_plugin_cmd_impl>> snapshot;
	{
	    std::lock_guard<std::mutex> lock(bu_plugin_impl::get_mutex());
	    auto& reg = bu_plugin_impl::get_registry();
	    snapshot.reserve(reg.size());
	    for (const auto& pair : reg) {
		snapshot.push_back(pair);
	    }
	}

	/* Sort out of lock */
	std::sort(snapshot.begin(), snapshot.end(),
		[](const std::pair<std::string, bu_plugin_cmd_impl>& a,
		    const std::pair<std::string, bu_plugin_cmd_impl>& b) {
		return a.first < b.first;
		});

	/* Iterate out of lock */
	for (const auto& pair : snapshot) {
	    if (callback(pair.first.c_str(), pair.second, user_data) != 0) {
		break;  /* Callback requested stop */
	    }
	}
    }

    BU_PLUGIN_API int bu_plugin_init(void) {
	/* No-op for now; registry is initialized on first access */
	return 0;
    }

#ifdef BU_PLUGIN_DEFAULT_SIGNATURE
    BU_PLUGIN_API int bu_plugin_cmd_run(const char *name, BU_PLUGIN_CMD_RET *result) {
	bu_plugin_cmd_impl fn = bu_plugin_cmd_get(name);
	if (!fn) {
	    bu_plugin_logf(BU_LOG_ERR, "Command '%s' not found", name ? name : "(null)");
	    return -1;
	}

#ifdef __cplusplus
	try {
#endif
	    BU_PLUGIN_CMD_RET ret = fn();
	    if (result) {
		*result = ret;
	    }
	    return 0;
#ifdef __cplusplus
	} catch (const std::exception& e) {
	    bu_plugin_logf(BU_LOG_ERR, "Command '%s' threw exception: %s", name, e.what());
	    return -2;
	} catch (...) {
	    bu_plugin_logf(BU_LOG_ERR, "Command '%s' threw unknown exception", name);
	    return -2;
	}
#endif
    }
#endif /* BU_PLUGIN_DEFAULT_SIGNATURE */

    /**
     * bu_plugin_load - Load a dynamic plugin and register its commands.
     */
    BU_PLUGIN_API int bu_plugin_load(const char *path) {
	if (!path || path[0] == '\0') {
	    bu_plugin_logf(BU_LOG_ERR, "Invalid plugin path (null or empty)");
	    return -1;
	}

	/* Enforce path allow policy */
	bu_plugin_path_allow_cb path_allow = bu_plugin_impl::get_path_allow();
	if (path_allow && !path_allow(path)) {
	    bu_plugin_logf(BU_LOG_ERR, "Plugin path '%s' not allowed by policy", path);
	    return -1;
	}

#if defined(HAVE_WINDOWS_H)
	/* Convert UTF-8 path to UTF-16 for Windows */
	int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
	if (wlen <= 0) {
	    bu_plugin_logf(BU_LOG_ERR, "Failed to convert plugin path to UTF-16: %s (error %lu)", path, GetLastError());
	    return -1;
	}
	std::vector<wchar_t> wpath(static_cast<size_t>(wlen));
	MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath.data(), wlen);

	/* Use LoadLibraryExW with safer flags (no DLL search path manipulation) */
	HMODULE handle = LoadLibraryExW(wpath.data(), NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	if (!handle) {
	    /* Fallback to LoadLibraryW if the flags are not supported */
	    handle = LoadLibraryW(wpath.data());
	}
	if (!handle) {
	    DWORD err = GetLastError();
	    bu_plugin_logf(BU_LOG_ERR, "Failed to load plugin: %s (Windows error %lu)", path, err);
	    return -1;
	}
	typedef const bu_plugin_manifest* (*info_fn)(void);
	info_fn get_info = reinterpret_cast<info_fn>(reinterpret_cast<void*>(GetProcAddress(handle, BU_PLUGIN_MANIFEST_SYM)));
	if (!get_info) {
	    DWORD err = GetLastError();
	    bu_plugin_logf(BU_LOG_ERR, "Plugin %s does not export %s (Windows error %lu)", path, BU_PLUGIN_MANIFEST_SYM, err);
	    FreeLibrary(handle);
	    return -1;
	}
#else
	void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (!handle) {
	    const char *err = dlerror();
	    bu_plugin_logf(BU_LOG_ERR, "Failed to load plugin: %s (%s)", path, err ? err : "unknown error");
	    return -1;
	}

	/* Clear dlerror before dlsym for accurate error reporting */
	dlerror();

	typedef const bu_plugin_manifest* (*info_fn)(void);
	info_fn get_info = reinterpret_cast<info_fn>(dlsym(handle, BU_PLUGIN_MANIFEST_SYM));
	const char *sym_err = dlerror();
	if (sym_err || !get_info) {
	    bu_plugin_logf(BU_LOG_ERR, "Plugin %s does not export %s (%s)",
		    path, BU_PLUGIN_MANIFEST_SYM, sym_err ? sym_err : "symbol not found");
	    dlclose(handle);
	    return -1;
	}
#endif

	const bu_plugin_manifest *manifest = get_info();
	if (!manifest) {
	    bu_plugin_logf(BU_LOG_ERR, "Plugin %s returned NULL manifest", path);
#if defined(HAVE_WINDOWS_H)
	    FreeLibrary(handle);
#else
	    dlclose(handle);
#endif
	    return -1;
	}

	/* Defensive plausibility check BEFORE touching fields further */
	if (!bu_plugin_impl::manifest_is_plausible(manifest)) {
	    bu_plugin_logf(BU_LOG_ERR, "Plugin %s has incompatible or invalid manifest layout", path);
#if defined(HAVE_WINDOWS_H)
	    FreeLibrary(handle);
#else
	    dlclose(handle);
#endif
	    return -1;
	}

	/* Validate manifest ABI version and struct_size */
	if (manifest->abi_version != BU_PLUGIN_ABI_VERSION) {
	    bu_plugin_logf(BU_LOG_ERR, "Plugin %s has incompatible ABI version %u (expected %u)",
		    path, manifest->abi_version, BU_PLUGIN_ABI_VERSION);
#if defined(HAVE_WINDOWS_H)
	    FreeLibrary(handle);
#else
	    dlclose(handle);
#endif
	    return -1;
	}

	if (manifest->struct_size < sizeof(bu_plugin_manifest)) {
	    bu_plugin_logf(BU_LOG_ERR, "Plugin %s has incompatible manifest struct_size %zu (expected >= %zu)",
		    path, manifest->struct_size, sizeof(bu_plugin_manifest));
#if defined(HAVE_WINDOWS_H)
	    FreeLibrary(handle);
#else
	    dlclose(handle);
#endif
	    return -1;
	}

	/* Validate manifest has commands */
	if (!manifest->commands || manifest->cmd_count == 0) {
	    bu_plugin_logf(BU_LOG_INFO, "Plugin %s has no commands", path);
	    /* Not an error, just nothing to register */
#if defined(HAVE_WINDOWS_H)
	    bu_plugin_impl::get_modules().push_back(handle);
#else
	    bu_plugin_impl::get_modules().push_back(handle);
#endif
	    return 0;
	}

	/* Additional guard: cap absurd counts to avoid bad memory walks */
	if (manifest->cmd_count > 8192U) {
	    bu_plugin_logf(BU_LOG_ERR, "Plugin %s reports unreasonable cmd_count %u", path, manifest->cmd_count);
#if defined(HAVE_WINDOWS_H)
	    FreeLibrary(handle);
#else
	    dlclose(handle);
#endif
	    return -1;
	}

	/* Detect duplicate command names within the manifest */
	std::unordered_set<std::string> manifest_names;
	for (unsigned int i = 0; i < manifest->cmd_count; i++) {
	    const bu_plugin_cmd *cmd = &manifest->commands[i];
	    if (cmd->name) {
		std::string trimmed = bu_plugin_impl::trim_whitespace(cmd->name);
		if (!trimmed.empty() && manifest_names.find(trimmed) != manifest_names.end()) {
		    bu_plugin_logf(BU_LOG_WARN, "Plugin %s has duplicate command name '%s' in manifest",
			    path, trimmed.c_str());
		}
		manifest_names.insert(trimmed);
	    }
	}

	int registered = 0;
	for (unsigned int i = 0; i < manifest->cmd_count; i++) {
	    const bu_plugin_cmd *cmd = &manifest->commands[i];
	    if (cmd->name && cmd->impl) {
		int result = bu_plugin_cmd_register(cmd->name, cmd->impl);
		if (result == 0) {
		    registered++;
		}
		/* result == 1 means duplicate (logged by register function) */
	    }
	}

	/* Retain module handle for lifetime */
#if defined(HAVE_WINDOWS_H)
	bu_plugin_impl::get_modules().push_back(handle);
#else
	bu_plugin_impl::get_modules().push_back(handle);
#endif

	return registered;
    }

    /* Count retained modules */
    BU_PLUGIN_API size_t bu_plugin_loaded_modules_count(void) {
	return bu_plugin_impl::get_modules().size();
    }

    /* Optional shutdown: unload modules in reverse order and clear registry */
    BU_PLUGIN_API void bu_plugin_shutdown(void) {
	auto &mods = bu_plugin_impl::get_modules();
	for (auto it = mods.rbegin(); it != mods.rend(); ++it) {
#if defined(HAVE_WINDOWS_H)
	    if (*it) FreeLibrary(*it);
#else
	    if (*it) dlclose(*it);
#endif
	}
	mods.clear();
	std::lock_guard<std::mutex> lock(bu_plugin_impl::get_mutex());
	bu_plugin_impl::get_registry().clear();
    }

} /* extern "C" */

#endif /* BU_PLUGIN_IMPLEMENTATION && __cplusplus */

#endif /* BU_PLUGIN_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
