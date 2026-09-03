#ifndef CREO_SHARED_CORE_API_H
#define CREO_SHARED_CORE_API_H

#ifdef _WIN32
#  define CREO_BRL_CORE_CALL __cdecl
#  ifdef CREO_BRL_CORE_BUILD
#    define CREO_BRL_CORE_EXPORT __declspec(dllexport)
#  else
#    define CREO_BRL_CORE_EXPORT
#  endif
#else
#  define CREO_BRL_CORE_CALL
#  define CREO_BRL_CORE_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct creo_brl_frontend_api;

#define CREO_BRL_CORE_INITIALIZE_EXPORT "creo_brl_core_initialize"
#define CREO_BRL_CORE_CONVERT_EXPORT "creo_brl_core_convert"
#define CREO_BRL_CORE_TERMINATE_EXPORT "creo_brl_core_terminate"
#define CREO_BRL_CORE_SET_FRONTEND_API_EXPORT "creo_brl_core_set_frontend_api"
#define CREO_BRL_CORE_LOAD_PROFILE_EXPORT "creo_brl_core_load_profile"

enum creo_brl_core_convert_result {
    CREO_BRL_CORE_CONVERT_SUCCESS = 0,
    CREO_BRL_CORE_CONVERT_INVALID_REQUEST = -1,
    CREO_BRL_CORE_CONVERT_OPEN_FAILED = -2,
    CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT = -3,
    CREO_BRL_CORE_CONVERT_NOT_IMPLEMENTED = -4,
    CREO_BRL_CORE_CONVERT_OUTPUT_FAILED = -5
};

CREO_BRL_CORE_EXPORT int CREO_BRL_CORE_CALL creo_brl_core_initialize(void);
CREO_BRL_CORE_EXPORT int CREO_BRL_CORE_CALL creo_brl_core_convert(const char *snapshot_path);
CREO_BRL_CORE_EXPORT void CREO_BRL_CORE_CALL creo_brl_core_terminate(void);
CREO_BRL_CORE_EXPORT void CREO_BRL_CORE_CALL creo_brl_core_load_profile(const char *profile_directory, const char *material_directory);
CREO_BRL_CORE_EXPORT void CREO_BRL_CORE_CALL creo_brl_core_set_frontend_api(const struct creo_brl_frontend_api *api);

typedef int (CREO_BRL_CORE_CALL *creo_brl_core_initialize_fn_t)(void);
typedef int (CREO_BRL_CORE_CALL *creo_brl_core_convert_fn_t)(const char *snapshot_path);
typedef void (CREO_BRL_CORE_CALL *creo_brl_core_terminate_fn_t)(void);
typedef void (CREO_BRL_CORE_CALL *creo_brl_core_load_profile_fn_t)(const char *profile_directory, const char *material_directory);
typedef void (CREO_BRL_CORE_CALL *creo_brl_core_set_frontend_api_fn_t)(const struct creo_brl_frontend_api *api);

#ifdef __cplusplus
}
#endif

#endif
