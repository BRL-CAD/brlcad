#ifndef CREO_BRL_FRONTEND_API_H
#define CREO_BRL_FRONTEND_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (__cdecl *creo_brl_status_fn_t)(const char *message);
typedef void (__cdecl *creo_brl_popup_fn_t)(const char *title, const char *message);
typedef void (__cdecl *creo_brl_apply_control_fn_t)(
    const char *resource,
    const char *type,
    const char *value,
    const char *default_value);

struct creo_brl_frontend_api {
    creo_brl_status_fn_t show_status;
    creo_brl_popup_fn_t show_popup;
    creo_brl_apply_control_fn_t apply_control;
};

#ifdef __cplusplus
}
#endif

#endif
