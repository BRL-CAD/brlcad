/*                        P L U G I N . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2025 United States Government as represented by
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
/** @file plugin.h
 *
 * Mechanisms for providing modular commands to the GED framework.
 */

#ifndef LIBGED_PLUGIN_H
#define LIBGED_PLUGIN_H

#include "../ged_private.h"
#include "brlcad_version.h"

#define GED_API (2*1000000 + (BRLCAD_VERSION_MAJOR*10000) + (BRLCAD_VERSION_MINOR*100) + BRLCAD_VERSION_PATCH)

#define GED_CMD_DEFAULT       0
#define GED_CMD_INTERACTIVE   (1ULL << 0)
#define GED_CMD_UPDATE_SCENE  (1ULL << 1)
#define GED_CMD_UPDATE_VIEW   (1ULL << 2)
#define GED_CMD_AUTOVIEW      (1ULL << 3)
#define GED_CMD_ALL_VIEWS     (1ULL << 4)
#define GED_CMD_VIEW_CALLBACK (1ULL << 5)

struct ged_cmd_impl {
    const char *cname;
    ged_func_ptr cmd;
    unsigned long long opts;
};

struct ged_cmd_process_impl {
    ged_process_ptr func;
};

#ifdef __cplusplus
extern "C" {
#endif

    int ged_register_command(const struct ged_cmd *cmd);
    int ged_command_exists(const char *name);
    const struct ged_cmd *ged_get_command(const char *name);
    size_t ged_registered_count(void);
    void ged_list_command_names(struct bu_vls *out_csv);
    void ged_list_command_array(const char * const **cl, size_t *cnt);
    int ged_load_plugin_file(const char *fullpath);
    void ged_scan_plugins(void);
    void libged_init(void);
    void libged_shutdown(void);
    const char *ged_init_msgs();

#ifdef __cplusplus
}
#endif

/* Static registration macro: active only when building static core AND not flagged GED_PLUGIN_ONLY */
#if defined(LIBGED_STATIC_CORE) && !defined(GED_PLUGIN_ONLY)

// GCC/Clang: use section attribute. MSVC: use allocate pragma.
#if defined(_MSC_VER)
#pragma section(".gedcmd", read)
#pragma section(".ged_cmd_set$a", read)
#define GED_CMD_LINKER_SET __declspec(allocate(".ged_cmd_set$a"))
#define REGISTER_GED_COMMAND(cmd_symbol)                                                   \
    GED_CMD_LINKER_SET const struct ged_cmd * __ged_cmd_ptr_##cmd_symbol = &cmd_symbol;    \
    __declspec(allocate(".gedcmd")) const struct ged_cmd * cmd_symbol##_keep = &cmd_symbol;\
    static void __cdecl register_##cmd_symbol(void) {                                      \
        (void)ged_register_command(&cmd_symbol);                                           \
    }                                                                                      \
    __declspec(allocate(".CRT$XCU")) void (__cdecl *cmd_symbol##_ctor)(void) = register_##cmd_symbol;
#else
#define GED_CMD_LINKER_SET __attribute__((used, section("ged_cmd_set")))
#define REGISTER_GED_COMMAND(cmd_symbol)                                                   \
    GED_CMD_LINKER_SET const struct ged_cmd * __ged_cmd_ptr_##cmd_symbol = &cmd_symbol;    \
    static const struct ged_cmd * cmd_symbol##_keep __attribute__((used)) = &cmd_symbol;   \
    static void register_##cmd_symbol(void) __attribute__((constructor));                  \
    static void register_##cmd_symbol(void) {                                              \
        (void)ged_register_command(&cmd_symbol);                                           \
    }
#endif

#else
#define REGISTER_GED_COMMAND(cmd_symbol) /* no-op */
#endif

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
