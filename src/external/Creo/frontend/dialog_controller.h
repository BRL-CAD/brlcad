#ifndef CREO_FRONTEND_DIALOG_CONTROLLER_H
#define CREO_FRONTEND_DIALOG_CONTROLLER_H

#include "creo-brl.h"
#include "shared/conversion_options.h"

#if !defined(CREO_EXEC_PLUGIN)
void creo_brl_frontend_dialog_apply_options(
    const struct creo_brl_conversion_options *options);
#endif
int creo_brl_frontend_dialog_command(
    uiCmdCmdId command,
    uiCmdValue *value,
    void *push_command_data);

#endif /* CREO_FRONTEND_DIALOG_CONTROLLER_H */
