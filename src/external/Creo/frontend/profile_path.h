#ifndef CREO_FRONTEND_PROFILE_PATH_H
#define CREO_FRONTEND_PROFILE_PATH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*creo_brl_profile_path_exists_fn_t)(const char *path, void *context);

int creo_brl_resolve_profile_path(
    const char *environment_profile,
    const char *working_directory,
    const char *bundled_profile_path,
    creo_brl_profile_path_exists_fn_t path_exists,
    void *context,
    char *profile_path,
    size_t profile_path_size);

#ifdef __cplusplus
}
#endif

#endif /* CREO_FRONTEND_PROFILE_PATH_H */
