#ifndef CREO_BRL_FRONTEND_API_H
#define CREO_BRL_FRONTEND_API_H

#include "shared/conversion_options.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (__cdecl *creo_brl_status_fn_t)(const char *message);
typedef void (__cdecl *creo_brl_popup_fn_t)(const char *title, const char *message);
typedef void (__cdecl *creo_brl_apply_options_fn_t)(
    const struct creo_brl_conversion_options *options);

struct creo_brl_frontend_api {
    creo_brl_status_fn_t show_status;
    creo_brl_popup_fn_t show_popup;
    creo_brl_apply_options_fn_t apply_options;
};

#ifdef __cplusplus
}
#endif

#endif
