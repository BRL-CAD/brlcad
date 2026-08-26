/**
 * Toolkit-free lifecycle exports for the runtime-loaded backend.
 */

#include "common.h"

extern "C" __declspec(dllexport) int
creo_brl_core_initialize()
{
    return 0;
}

extern "C" __declspec(dllexport) void
creo_brl_core_terminate()
{
}
