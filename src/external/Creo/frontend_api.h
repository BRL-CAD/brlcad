#ifndef CREO_BRL_FRONTEND_API_H
#define CREO_BRL_FRONTEND_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (__cdecl *creo_brl_status_fn_t)(const char *message);
typedef void (__cdecl *creo_brl_popup_fn_t)(const char *title, const char *message);

struct creo_brl_frontend_api {
    creo_brl_status_fn_t show_status;
    creo_brl_popup_fn_t show_popup;
};

#ifdef __cplusplus
}
#endif

#endif
