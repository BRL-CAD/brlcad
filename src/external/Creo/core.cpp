/**
 * Toolkit-free lifecycle exports for the runtime-loaded backend.
 */

#include "common.h"

#include "shared/core_api.h"

extern "C" CREO_BRL_CORE_EXPORT int CREO_BRL_CORE_CALL
creo_brl_core_initialize()
{
    return 0;
}

extern "C" CREO_BRL_CORE_EXPORT void CREO_BRL_CORE_CALL
creo_brl_core_terminate()
{
}
