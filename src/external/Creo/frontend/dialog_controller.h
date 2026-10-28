#ifndef CREO_FRONTEND_DIALOG_CONTROLLER_H
#define CREO_FRONTEND_DIALOG_CONTROLLER_H

#include "frontend/creo_toolkit.h"
#include "shared/conversion_options.h"

void creo_brl_frontend_dialog_apply_options(
    const struct creo_brl_conversion_options *options);
int creo_brl_frontend_dialog_command(
    uiCmdCmdId command,
    uiCmdValue *value,
    void *push_command_data);

#endif /* CREO_FRONTEND_DIALOG_CONTROLLER_H */
