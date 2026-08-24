/**
 *                    L O A D E R . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/**
 * @file loader.c
 *
 * Minimal Creo-facing facade that runtime-loads the real converter core.
 */

#include <windows.h>
#include <stdio.h>
#include <wchar.h>

#include <ProUtil.h>

typedef int (__cdecl *creo_brl_core_initialize_fn_t)(void);
typedef void (__cdecl *creo_brl_core_terminate_fn_t)(void);

static HMODULE core_module = NULL;
static creo_brl_core_initialize_fn_t core_initialize_fn = NULL;
static creo_brl_core_terminate_fn_t core_terminate_fn = NULL;

/*
 * GetModuleHandleExW(...FROM_ADDRESS...) needs any address that belongs
 * to this module. A data symbol avoids a function-pointer cast.
 */
static const char module_anchor = 0;

static void
show_bootstrap_error(const wchar_t *context)
{
    DWORD error_code = GetLastError();
    wchar_t system_message[1024] = {0};
    wchar_t message[2048] = {0};

    FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        0,
        system_message,
        (DWORD)_countof(system_message),
        NULL);

    swprintf_s(
        message,
        _countof(message),
        L"%ls\n\nWin32 error %lu:\n%ls",
        context,
        error_code,
        system_message[0] ? system_message : L"(no system error text)");

    MessageBoxW(
        NULL,
        message,
        L"creo-brl bootstrap failure",
        MB_OK | MB_ICONERROR | MB_TOPMOST);
}

static int
self_bin_dir(wchar_t *dir, size_t dir_len)
{
    HMODULE self = NULL;
    DWORD length = 0;
    wchar_t *last_slash = NULL;

    if (!dir || dir_len == 0)
        return 0;

    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)&module_anchor,
            &self)) {
        return 0;
    }

    length = GetModuleFileNameW(self, dir, (DWORD)dir_len);
    if (length == 0 || length >= dir_len)
        return 0;

    last_slash = wcsrchr(dir, L'\\');
    if (!last_slash)
        return 0;

    *last_slash = L'\0';
    return 1;
}

static void
clear_core_state(void)
{
    core_initialize_fn = NULL;
    core_terminate_fn = NULL;
    core_module = NULL;
}

__declspec(dllexport) int
user_initialize(void)
{
    int expected_wchar_size = 0;
    int tk_err = 0;
    wchar_t bin_dir[32768] = {0};
    wchar_t core_path[32768] = {0};
    FARPROC proc = NULL;
    int core_result = -1;

    if (core_module) {
        MessageBoxW(
            NULL,
            L"user_initialize was called while creo-brl-core.dll was already loaded.",
            L"creo-brl bootstrap failure",
            MB_OK | MB_ICONERROR | MB_TOPMOST);
        return -1;
    }

    /* Keep one direct Toolkit call in the Creo-facing entry point. */
    tk_err = ProWcharSizeVerify((int)sizeof(wchar_t), &expected_wchar_size);
    if (tk_err != 0 || expected_wchar_size != (int)sizeof(wchar_t)) {
        wchar_t message[512] = {0};

        swprintf_s(
            message,
            _countof(message),
            L"Facade ProWcharSizeVerify failed.\nerr=%d expected=%d actual=%zu",
            tk_err,
            expected_wchar_size,
            sizeof(wchar_t));

        MessageBoxW(
            NULL,
            message,
            L"creo-brl bootstrap failure",
            MB_OK | MB_ICONERROR | MB_TOPMOST);
        return -1;
    }

    if (!self_bin_dir(bin_dir, _countof(bin_dir))) {
        show_bootstrap_error(L"Unable to determine the creo-brl.dll directory.");
        return -1;
    }

    if (swprintf_s(
            core_path,
            _countof(core_path),
            L"%ls\\creo-brl-core.dll",
            bin_dir) < 0) {
        MessageBoxW(
            NULL,
            L"Unable to construct the creo-brl-core.dll path.",
            L"creo-brl bootstrap failure",
            MB_OK | MB_ICONERROR | MB_TOPMOST);
        return -1;
    }

    /*
     * Keep DLL search changes scoped to this load operation so we do not
     * alter Creo's process-wide loader policy.
     */
    core_module = LoadLibraryExW(
        core_path,
        NULL,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
        LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!core_module) {
        show_bootstrap_error(L"LoadLibraryExW(creo-brl-core.dll) failed.");
        clear_core_state();
        return -1;
    }

    proc = GetProcAddress(core_module, "creo_brl_core_initialize");
    if (!proc) {
        show_bootstrap_error(L"GetProcAddress(creo_brl_core_initialize) failed.");
        FreeLibrary(core_module);
        clear_core_state();
        return -1;
    }
    core_initialize_fn = (creo_brl_core_initialize_fn_t)proc;

    proc = GetProcAddress(core_module, "creo_brl_core_terminate");
    if (!proc) {
        show_bootstrap_error(L"GetProcAddress(creo_brl_core_terminate) failed.");
        FreeLibrary(core_module);
        clear_core_state();
        return -1;
    }
    core_terminate_fn = (creo_brl_core_terminate_fn_t)proc;

    core_result = core_initialize_fn();
    if (core_result != 0) {
        FreeLibrary(core_module);
        clear_core_state();
        return core_result;
    }

    return 0;
}

__declspec(dllexport) void
user_terminate(void)
{
    HMODULE module_to_free = core_module;
    creo_brl_core_terminate_fn_t terminate_to_call = core_terminate_fn;

    /*
     * Clear callable state before unloading so no stale function pointer
     * survives a successful FreeLibrary.
     */
    clear_core_state();

    if (terminate_to_call)
        terminate_to_call();

    if (module_to_free)
        FreeLibrary(module_to_free);
}
