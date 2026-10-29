#ifndef CREO_FRONTEND_DIALOG_CONTROLLER_H
#define CREO_FRONTEND_DIALOG_CONTROLLER_H

#if defined(__cplusplus)
#  include "frontend/creo_toolkit.h"
#else
#  include <ProUICmd.h>
#endif
#include "shared/conversion_options.h"

#ifdef __cplusplus
extern "C" {
#endif

void creo_brl_show_status(const char *message);
void creo_brl_show_popup(const char *title, const char *message);
void creo_brl_core_load_profile_shim(void);
int creo_brl_core_convert_shim(const char *snapshot_path);

void __cdecl creo_brl_frontend_apply_options(
    const struct creo_brl_conversion_options *options);
int creo_brl_frontend_command(
    uiCmdCmdId command,
    uiCmdValue *value,
    void *push_command_data);
uiCmdAccessState creo_brl_frontend_access(
    uiCmdAccessMode access_mode);

#ifdef __cplusplus
}
#endif

#endif /* CREO_FRONTEND_DIALOG_CONTROLLER_H */
