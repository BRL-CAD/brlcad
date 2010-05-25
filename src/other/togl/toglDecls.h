#ifndef ToglDecls_H
#  define ToglDecls_H

/* 
 * Togl - a Tk OpenGL widget
 *
 * Copyright (C) 1996-2002  Brian Paul and Ben Bederson
 * Copyright (C) 2005-2009  Greg Couch
 * See the LICENSE file for copyright details.
 */

/* !BEGIN!: Do not edit below this line. */

/*
 * Exported function declarations:
 */

/* 0 */
EXTERN int		Togl_Init _ANSI_ARGS_((Tcl_Interp * interp));
/* 1 */
EXTERN void		Togl_MakeCurrent _ANSI_ARGS_((const Togl * togl));
/* 2 */
EXTERN void		Togl_PostRedisplay _ANSI_ARGS_((Togl * togl));
/* 3 */
EXTERN void		Togl_SwapBuffers _ANSI_ARGS_((const Togl * togl));
/* 4 */
EXTERN const char *	Togl_Ident _ANSI_ARGS_((const Togl * togl));
/* 5 */
EXTERN int		Togl_Width _ANSI_ARGS_((const Togl * togl));
/* 6 */
EXTERN int		Togl_Height _ANSI_ARGS_((const Togl * togl));
/* 7 */
EXTERN Tcl_Interp *	Togl_Interp _ANSI_ARGS_((const Togl * togl));
/* 8 */
EXTERN Tk_Window	Togl_TkWin _ANSI_ARGS_((const Togl * togl));
/* 9 */
EXTERN const char *	Togl_CommandName _ANSI_ARGS_((const Togl * togl));
/* 10 */
EXTERN unsigned long	Togl_AllocColor _ANSI_ARGS_((const Togl * togl, 
				float red, float green, float blue));
/* 11 */
EXTERN void		Togl_FreeColor _ANSI_ARGS_((const Togl * togl, 
				unsigned long index));
/* 12 */
EXTERN void		Togl_SetColor _ANSI_ARGS_((const Togl * togl, 
				unsigned long index, float red, float green, 
				float blue));
/* 13 */
EXTERN Tcl_Obj *	Togl_LoadBitmapFont _ANSI_ARGS_((const Togl * togl, 
				const char * fontname));
/* 14 */
EXTERN int		Togl_UnloadBitmapFont _ANSI_ARGS_((const Togl * togl, 
				Tcl_Obj * toglfont));
/* 15 */
EXTERN void		Togl_UseLayer _ANSI_ARGS_((Togl * togl, int layer));
/* 16 */
EXTERN void		Togl_ShowOverlay _ANSI_ARGS_((Togl * togl));
/* 17 */
EXTERN void		Togl_HideOverlay _ANSI_ARGS_((Togl * togl));
/* 18 */
EXTERN void		Togl_PostOverlayRedisplay _ANSI_ARGS_((Togl * togl));
/* 19 */
EXTERN int		Togl_ExistsOverlay _ANSI_ARGS_((const Togl * togl));
/* 20 */
EXTERN int		Togl_GetOverlayTransparentValue _ANSI_ARGS_((
				const Togl * togl));
/* 21 */
EXTERN int		Togl_IsMappedOverlay _ANSI_ARGS_((const Togl * togl));
/* 22 */
EXTERN unsigned long	Togl_AllocColorOverlay _ANSI_ARGS_((
				const Togl * togl, float red, float green, 
				float blue));
/* 23 */
EXTERN void		Togl_FreeColorOverlay _ANSI_ARGS_((const Togl * togl, 
				unsigned long index));
/* 24 */
EXTERN ClientData	Togl_GetClientData _ANSI_ARGS_((const Togl * togl));
/* 25 */
EXTERN void		Togl_SetClientData _ANSI_ARGS_((Togl * togl, 
				ClientData clientData));
/* 26 */
EXTERN void		Togl_DrawBuffer _ANSI_ARGS_((Togl * togl, 
				GLenum mode));
/* 27 */
EXTERN void		Togl_Clear _ANSI_ARGS_((const Togl * togl, 
				GLbitfield mask));
/* 28 */
EXTERN void		Togl_Frustum _ANSI_ARGS_((const Togl * togl, 
				GLdouble left, GLdouble right, 
				GLdouble bottom, GLdouble top, GLdouble near, 
				GLdouble far));
/* 29 */
EXTERN int		Togl_GetToglFromObj _ANSI_ARGS_((Tcl_Interp * interp, 
				Tcl_Obj * obj, Togl ** toglPtr));
/* 30 */
EXTERN int		Togl_TakePhoto _ANSI_ARGS_((Togl * togl, 
				Tk_PhotoHandle photo));
/* 31 */
EXTERN Togl_FuncPtr	Togl_GetProcAddr _ANSI_ARGS_((const char * funcname));
/* 32 */
EXTERN int		Togl_GetToglFromName _ANSI_ARGS_((
				Tcl_Interp * interp, const char * cmdName, 
				Togl ** toglPtr));
/* 33 */
EXTERN Bool		Togl_SwapInterval _ANSI_ARGS_((const Togl * togl, 
				int interval));
/* 34 */
EXTERN void		Togl_Ortho _ANSI_ARGS_((const Togl * togl, 
				GLdouble left, GLdouble right, 
				GLdouble bottom, GLdouble top, GLdouble near, 
				GLdouble far));
/* 35 */
EXTERN int		Togl_NumEyes _ANSI_ARGS_((const Togl * togl));
/* 36 */
EXTERN int		Togl_ContextTag _ANSI_ARGS_((const Togl * togl));
/* 37 */
EXTERN Bool		Togl_UpdatePending _ANSI_ARGS_((const Togl * togl));
/* 38 */
EXTERN int		Togl_WriteObj _ANSI_ARGS_((const Togl * togl, 
				const Tcl_Obj * toglfont, Tcl_Obj * obj));
/* 39 */
EXTERN int		Togl_WriteChars _ANSI_ARGS_((const Togl * togl, 
				const Tcl_Obj * toglfont, const char * str, 
				int len));
/* 40 */
EXTERN Bool		Togl_HasRGBA _ANSI_ARGS_((const Togl * togl));
/* 41 */
EXTERN Bool		Togl_IsDoubleBuffered _ANSI_ARGS_((const Togl * togl));
/* 42 */
EXTERN Bool		Togl_HasDepthBuffer _ANSI_ARGS_((const Togl * togl));
/* 43 */
EXTERN Bool		Togl_HasAccumulationBuffer _ANSI_ARGS_((
				const Togl * togl));
/* 44 */
EXTERN Bool		Togl_HasDestinationAlpha _ANSI_ARGS_((
				const Togl * togl));
/* 45 */
EXTERN Bool		Togl_HasStencilBuffer _ANSI_ARGS_((const Togl * togl));
/* 46 */
EXTERN int		Togl_StereoMode _ANSI_ARGS_((const Togl * togl));
/* 47 */
EXTERN Bool		Togl_HasMultisample _ANSI_ARGS_((const Togl * togl));
/* 48 */
EXTERN int		Togl_CopyContext _ANSI_ARGS_((const Togl * from, 
				const Togl * to, unsigned int mask));

typedef struct ToglStubs {
    int magic;
    struct ToglStubHooks *hooks;

    int (*togl_Init) _ANSI_ARGS_((Tcl_Interp * interp)); /* 0 */
    void (*togl_MakeCurrent) _ANSI_ARGS_((const Togl * togl)); /* 1 */
    void (*togl_PostRedisplay) _ANSI_ARGS_((Togl * togl)); /* 2 */
    void (*togl_SwapBuffers) _ANSI_ARGS_((const Togl * togl)); /* 3 */
    const char * (*togl_Ident) _ANSI_ARGS_((const Togl * togl)); /* 4 */
    int (*togl_Width) _ANSI_ARGS_((const Togl * togl)); /* 5 */
    int (*togl_Height) _ANSI_ARGS_((const Togl * togl)); /* 6 */
    Tcl_Interp * (*togl_Interp) _ANSI_ARGS_((const Togl * togl)); /* 7 */
    Tk_Window (*togl_TkWin) _ANSI_ARGS_((const Togl * togl)); /* 8 */
    const char * (*togl_CommandName) _ANSI_ARGS_((const Togl * togl)); /* 9 */
    unsigned long (*togl_AllocColor) _ANSI_ARGS_((const Togl * togl, float red, float green, float blue)); /* 10 */
    void (*togl_FreeColor) _ANSI_ARGS_((const Togl * togl, unsigned long index)); /* 11 */
    void (*togl_SetColor) _ANSI_ARGS_((const Togl * togl, unsigned long index, float red, float green, float blue)); /* 12 */
    Tcl_Obj * (*togl_LoadBitmapFont) _ANSI_ARGS_((const Togl * togl, const char * fontname)); /* 13 */
    int (*togl_UnloadBitmapFont) _ANSI_ARGS_((const Togl * togl, Tcl_Obj * toglfont)); /* 14 */
    void (*togl_UseLayer) _ANSI_ARGS_((Togl * togl, int layer)); /* 15 */
    void (*togl_ShowOverlay) _ANSI_ARGS_((Togl * togl)); /* 16 */
    void (*togl_HideOverlay) _ANSI_ARGS_((Togl * togl)); /* 17 */
    void (*togl_PostOverlayRedisplay) _ANSI_ARGS_((Togl * togl)); /* 18 */
    int (*togl_ExistsOverlay) _ANSI_ARGS_((const Togl * togl)); /* 19 */
    int (*togl_GetOverlayTransparentValue) _ANSI_ARGS_((const Togl * togl)); /* 20 */
    int (*togl_IsMappedOverlay) _ANSI_ARGS_((const Togl * togl)); /* 21 */
    unsigned long (*togl_AllocColorOverlay) _ANSI_ARGS_((const Togl * togl, float red, float green, float blue)); /* 22 */
    void (*togl_FreeColorOverlay) _ANSI_ARGS_((const Togl * togl, unsigned long index)); /* 23 */
    ClientData (*togl_GetClientData) _ANSI_ARGS_((const Togl * togl)); /* 24 */
    void (*togl_SetClientData) _ANSI_ARGS_((Togl * togl, ClientData clientData)); /* 25 */
    void (*togl_DrawBuffer) _ANSI_ARGS_((Togl * togl, GLenum mode)); /* 26 */
    void (*togl_Clear) _ANSI_ARGS_((const Togl * togl, GLbitfield mask)); /* 27 */
    void (*togl_Frustum) _ANSI_ARGS_((const Togl * togl, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near, GLdouble far)); /* 28 */
    int (*togl_GetToglFromObj) _ANSI_ARGS_((Tcl_Interp * interp, Tcl_Obj * obj, Togl ** toglPtr)); /* 29 */
    int (*togl_TakePhoto) _ANSI_ARGS_((Togl * togl, Tk_PhotoHandle photo)); /* 30 */
    Togl_FuncPtr (*togl_GetProcAddr) _ANSI_ARGS_((const char * funcname)); /* 31 */
    int (*togl_GetToglFromName) _ANSI_ARGS_((Tcl_Interp * interp, const char * cmdName, Togl ** toglPtr)); /* 32 */
    Bool (*togl_SwapInterval) _ANSI_ARGS_((const Togl * togl, int interval)); /* 33 */
    void (*togl_Ortho) _ANSI_ARGS_((const Togl * togl, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near, GLdouble far)); /* 34 */
    int (*togl_NumEyes) _ANSI_ARGS_((const Togl * togl)); /* 35 */
    int (*togl_ContextTag) _ANSI_ARGS_((const Togl * togl)); /* 36 */
    Bool (*togl_UpdatePending) _ANSI_ARGS_((const Togl * togl)); /* 37 */
    int (*togl_WriteObj) _ANSI_ARGS_((const Togl * togl, const Tcl_Obj * toglfont, Tcl_Obj * obj)); /* 38 */
    int (*togl_WriteChars) _ANSI_ARGS_((const Togl * togl, const Tcl_Obj * toglfont, const char * str, int len)); /* 39 */
    Bool (*togl_HasRGBA) _ANSI_ARGS_((const Togl * togl)); /* 40 */
    Bool (*togl_IsDoubleBuffered) _ANSI_ARGS_((const Togl * togl)); /* 41 */
    Bool (*togl_HasDepthBuffer) _ANSI_ARGS_((const Togl * togl)); /* 42 */
    Bool (*togl_HasAccumulationBuffer) _ANSI_ARGS_((const Togl * togl)); /* 43 */
    Bool (*togl_HasDestinationAlpha) _ANSI_ARGS_((const Togl * togl)); /* 44 */
    Bool (*togl_HasStencilBuffer) _ANSI_ARGS_((const Togl * togl)); /* 45 */
    int (*togl_StereoMode) _ANSI_ARGS_((const Togl * togl)); /* 46 */
    Bool (*togl_HasMultisample) _ANSI_ARGS_((const Togl * togl)); /* 47 */
    int (*togl_CopyContext) _ANSI_ARGS_((const Togl * from, const Togl * to, unsigned int mask)); /* 48 */
} ToglStubs;

#ifdef __cplusplus
extern "C" {
#endif
extern ToglStubs *toglStubsPtr;
#ifdef __cplusplus
}
#endif

#if defined(USE_TOGL_STUBS) && !defined(USE_TOGL_STUB_PROCS)

/*
 * Inline function declarations:
 */

#ifndef Togl_Init
#define Togl_Init \
	(toglStubsPtr->togl_Init) /* 0 */
#endif
#ifndef Togl_MakeCurrent
#define Togl_MakeCurrent \
	(toglStubsPtr->togl_MakeCurrent) /* 1 */
#endif
#ifndef Togl_PostRedisplay
#define Togl_PostRedisplay \
	(toglStubsPtr->togl_PostRedisplay) /* 2 */
#endif
#ifndef Togl_SwapBuffers
#define Togl_SwapBuffers \
	(toglStubsPtr->togl_SwapBuffers) /* 3 */
#endif
#ifndef Togl_Ident
#define Togl_Ident \
	(toglStubsPtr->togl_Ident) /* 4 */
#endif
#ifndef Togl_Width
#define Togl_Width \
	(toglStubsPtr->togl_Width) /* 5 */
#endif
#ifndef Togl_Height
#define Togl_Height \
	(toglStubsPtr->togl_Height) /* 6 */
#endif
#ifndef Togl_Interp
#define Togl_Interp \
	(toglStubsPtr->togl_Interp) /* 7 */
#endif
#ifndef Togl_TkWin
#define Togl_TkWin \
	(toglStubsPtr->togl_TkWin) /* 8 */
#endif
#ifndef Togl_CommandName
#define Togl_CommandName \
	(toglStubsPtr->togl_CommandName) /* 9 */
#endif
#ifndef Togl_AllocColor
#define Togl_AllocColor \
	(toglStubsPtr->togl_AllocColor) /* 10 */
#endif
#ifndef Togl_FreeColor
#define Togl_FreeColor \
	(toglStubsPtr->togl_FreeColor) /* 11 */
#endif
#ifndef Togl_SetColor
#define Togl_SetColor \
	(toglStubsPtr->togl_SetColor) /* 12 */
#endif
#ifndef Togl_LoadBitmapFont
#define Togl_LoadBitmapFont \
	(toglStubsPtr->togl_LoadBitmapFont) /* 13 */
#endif
#ifndef Togl_UnloadBitmapFont
#define Togl_UnloadBitmapFont \
	(toglStubsPtr->togl_UnloadBitmapFont) /* 14 */
#endif
#ifndef Togl_UseLayer
#define Togl_UseLayer \
	(toglStubsPtr->togl_UseLayer) /* 15 */
#endif
#ifndef Togl_ShowOverlay
#define Togl_ShowOverlay \
	(toglStubsPtr->togl_ShowOverlay) /* 16 */
#endif
#ifndef Togl_HideOverlay
#define Togl_HideOverlay \
	(toglStubsPtr->togl_HideOverlay) /* 17 */
#endif
#ifndef Togl_PostOverlayRedisplay
#define Togl_PostOverlayRedisplay \
	(toglStubsPtr->togl_PostOverlayRedisplay) /* 18 */
#endif
#ifndef Togl_ExistsOverlay
#define Togl_ExistsOverlay \
	(toglStubsPtr->togl_ExistsOverlay) /* 19 */
#endif
#ifndef Togl_GetOverlayTransparentValue
#define Togl_GetOverlayTransparentValue \
	(toglStubsPtr->togl_GetOverlayTransparentValue) /* 20 */
#endif
#ifndef Togl_IsMappedOverlay
#define Togl_IsMappedOverlay \
	(toglStubsPtr->togl_IsMappedOverlay) /* 21 */
#endif
#ifndef Togl_AllocColorOverlay
#define Togl_AllocColorOverlay \
	(toglStubsPtr->togl_AllocColorOverlay) /* 22 */
#endif
#ifndef Togl_FreeColorOverlay
#define Togl_FreeColorOverlay \
	(toglStubsPtr->togl_FreeColorOverlay) /* 23 */
#endif
#ifndef Togl_GetClientData
#define Togl_GetClientData \
	(toglStubsPtr->togl_GetClientData) /* 24 */
#endif
#ifndef Togl_SetClientData
#define Togl_SetClientData \
	(toglStubsPtr->togl_SetClientData) /* 25 */
#endif
#ifndef Togl_DrawBuffer
#define Togl_DrawBuffer \
	(toglStubsPtr->togl_DrawBuffer) /* 26 */
#endif
#ifndef Togl_Clear
#define Togl_Clear \
	(toglStubsPtr->togl_Clear) /* 27 */
#endif
#ifndef Togl_Frustum
#define Togl_Frustum \
	(toglStubsPtr->togl_Frustum) /* 28 */
#endif
#ifndef Togl_GetToglFromObj
#define Togl_GetToglFromObj \
	(toglStubsPtr->togl_GetToglFromObj) /* 29 */
#endif
#ifndef Togl_TakePhoto
#define Togl_TakePhoto \
	(toglStubsPtr->togl_TakePhoto) /* 30 */
#endif
#ifndef Togl_GetProcAddr
#define Togl_GetProcAddr \
	(toglStubsPtr->togl_GetProcAddr) /* 31 */
#endif
#ifndef Togl_GetToglFromName
#define Togl_GetToglFromName \
	(toglStubsPtr->togl_GetToglFromName) /* 32 */
#endif
#ifndef Togl_SwapInterval
#define Togl_SwapInterval \
	(toglStubsPtr->togl_SwapInterval) /* 33 */
#endif
#ifndef Togl_Ortho
#define Togl_Ortho \
	(toglStubsPtr->togl_Ortho) /* 34 */
#endif
#ifndef Togl_NumEyes
#define Togl_NumEyes \
	(toglStubsPtr->togl_NumEyes) /* 35 */
#endif
#ifndef Togl_ContextTag
#define Togl_ContextTag \
	(toglStubsPtr->togl_ContextTag) /* 36 */
#endif
#ifndef Togl_UpdatePending
#define Togl_UpdatePending \
	(toglStubsPtr->togl_UpdatePending) /* 37 */
#endif
#ifndef Togl_WriteObj
#define Togl_WriteObj \
	(toglStubsPtr->togl_WriteObj) /* 38 */
#endif
#ifndef Togl_WriteChars
#define Togl_WriteChars \
	(toglStubsPtr->togl_WriteChars) /* 39 */
#endif
#ifndef Togl_HasRGBA
#define Togl_HasRGBA \
	(toglStubsPtr->togl_HasRGBA) /* 40 */
#endif
#ifndef Togl_IsDoubleBuffered
#define Togl_IsDoubleBuffered \
	(toglStubsPtr->togl_IsDoubleBuffered) /* 41 */
#endif
#ifndef Togl_HasDepthBuffer
#define Togl_HasDepthBuffer \
	(toglStubsPtr->togl_HasDepthBuffer) /* 42 */
#endif
#ifndef Togl_HasAccumulationBuffer
#define Togl_HasAccumulationBuffer \
	(toglStubsPtr->togl_HasAccumulationBuffer) /* 43 */
#endif
#ifndef Togl_HasDestinationAlpha
#define Togl_HasDestinationAlpha \
	(toglStubsPtr->togl_HasDestinationAlpha) /* 44 */
#endif
#ifndef Togl_HasStencilBuffer
#define Togl_HasStencilBuffer \
	(toglStubsPtr->togl_HasStencilBuffer) /* 45 */
#endif
#ifndef Togl_StereoMode
#define Togl_StereoMode \
	(toglStubsPtr->togl_StereoMode) /* 46 */
#endif
#ifndef Togl_HasMultisample
#define Togl_HasMultisample \
	(toglStubsPtr->togl_HasMultisample) /* 47 */
#endif
#ifndef Togl_CopyContext
#define Togl_CopyContext \
	(toglStubsPtr->togl_CopyContext) /* 48 */
#endif

#endif /* defined(USE_TOGL_STUBS) && !defined(USE_TOGL_STUB_PROCS) */

/* !END!: Do not edit above this line. */

#endif
