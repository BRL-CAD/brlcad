/* 
 * Togl - a Tk OpenGL widget
 *
 * Copyright (C) 1996-2002  Brian Paul and Ben Bederson
 * Copyright (C) 2005-2009  Greg Couch
 * See the LICENSE file for copyright details.
 */

#include "togl.h"

/* !BEGIN!: Do not edit below this line. */

ToglStubs toglStubs = {
    TCL_STUB_MAGIC,
    NULL,
    Togl_Init, /* 0 */
    Togl_MakeCurrent, /* 1 */
    Togl_PostRedisplay, /* 2 */
    Togl_SwapBuffers, /* 3 */
    Togl_Ident, /* 4 */
    Togl_Width, /* 5 */
    Togl_Height, /* 6 */
    Togl_Interp, /* 7 */
    Togl_TkWin, /* 8 */
    Togl_CommandName, /* 9 */
    Togl_AllocColor, /* 10 */
    Togl_FreeColor, /* 11 */
    Togl_SetColor, /* 12 */
    Togl_LoadBitmapFont, /* 13 */
    Togl_UnloadBitmapFont, /* 14 */
    Togl_UseLayer, /* 15 */
    Togl_ShowOverlay, /* 16 */
    Togl_HideOverlay, /* 17 */
    Togl_PostOverlayRedisplay, /* 18 */
    Togl_ExistsOverlay, /* 19 */
    Togl_GetOverlayTransparentValue, /* 20 */
    Togl_IsMappedOverlay, /* 21 */
    Togl_AllocColorOverlay, /* 22 */
    Togl_FreeColorOverlay, /* 23 */
    Togl_GetClientData, /* 24 */
    Togl_SetClientData, /* 25 */
    Togl_DrawBuffer, /* 26 */
    Togl_Clear, /* 27 */
    Togl_Frustum, /* 28 */
    Togl_GetToglFromObj, /* 29 */
    Togl_TakePhoto, /* 30 */
    Togl_GetProcAddr, /* 31 */
    Togl_GetToglFromName, /* 32 */
    Togl_SwapInterval, /* 33 */
    Togl_Ortho, /* 34 */
    Togl_NumEyes, /* 35 */
    Togl_ContextTag, /* 36 */
    Togl_UpdatePending, /* 37 */
    Togl_WriteObj, /* 38 */
    Togl_WriteChars, /* 39 */
    Togl_HasRGBA, /* 40 */
    Togl_IsDoubleBuffered, /* 41 */
    Togl_HasDepthBuffer, /* 42 */
    Togl_HasAccumulationBuffer, /* 43 */
    Togl_HasDestinationAlpha, /* 44 */
    Togl_HasStencilBuffer, /* 45 */
    Togl_StereoMode, /* 46 */
    Togl_HasMultisample, /* 47 */
    Togl_CopyContext, /* 48 */
};

/* !END!: Do not edit above this line. */
