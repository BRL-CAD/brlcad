#include <cstdio>
#include <cstdlib>

#include <windows.h>

#include "shared/core_api.h"


static const int EXPECTED_ARGUMENT_COUNT = 2;


struct core_exports {
    creo_brl_core_initialize_fn_t initialize;
    creo_brl_core_convert_fn_t convert;
    creo_brl_core_terminate_fn_t terminate;
    creo_brl_core_set_frontend_api_fn_t set_frontend_api;
    creo_brl_core_load_profile_fn_t load_profile;
};


template <typename Function>
static bool
resolve_required_export(HMODULE core_module, const char *export_name, Function *function)
{
    FARPROC export_function = GetProcAddress(core_module, export_name);

    if (!export_function) {
        std::fprintf(stderr, "Unable to resolve required core export: %s\n", export_name);
        return false;
    }

    *function = reinterpret_cast<Function>(export_function);
    return true;
}


static bool
resolve_core_exports(HMODULE core_module, struct core_exports *exports)
{
    return resolve_required_export(core_module, CREO_BRL_CORE_INITIALIZE_EXPORT, &exports->initialize) &&
           resolve_required_export(core_module, CREO_BRL_CORE_CONVERT_EXPORT, &exports->convert) &&
           resolve_required_export(core_module, CREO_BRL_CORE_TERMINATE_EXPORT, &exports->terminate) &&
           resolve_required_export(core_module, CREO_BRL_CORE_SET_FRONTEND_API_EXPORT, &exports->set_frontend_api) &&
           resolve_required_export(core_module, CREO_BRL_CORE_LOAD_PROFILE_EXPORT, &exports->load_profile);
}


static int
run_core_abi_test(const char *core_library_path)
{
    HMODULE core_module = LoadLibraryA(core_library_path);
    struct core_exports exports = {};
    int result = EXIT_FAILURE;

    if (!core_module) {
        std::fprintf(stderr, "Unable to load core library: %s (error %lu)\n",
                     core_library_path, static_cast<unsigned long>(GetLastError()));
        return result;
    }

    if (!resolve_core_exports(core_module, &exports))
        goto cleanup;

    exports.set_frontend_api(NULL);
    if (exports.initialize() != CREO_BRL_CORE_CONVERT_SUCCESS) {
        std::fprintf(stderr, "Core initialization failed.\n");
        goto cleanup;
    }

    exports.terminate();
    result = EXIT_SUCCESS;

cleanup:
    FreeLibrary(core_module);
    return result;
}


int
main(int argc, char *argv[])
{
    if (argc != EXPECTED_ARGUMENT_COUNT) {
        std::fprintf(stderr, "Usage: %s <core-library>\n", argv[0]);
        return EXIT_FAILURE;
    }

    return run_core_abi_test(argv[1]);
}
