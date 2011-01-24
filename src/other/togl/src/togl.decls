library togl
interface togl

# Declare each of the functions in the public Togl interface.  Note that
# the an index should never be reused for a different function in order
# to preserve backwards compatibility.

# package initialization
declare 0 generic {
	int Togl_Init(Tcl_Interp *interp)
}

# Miscellaneous
declare 1 generic {
	void Togl_MakeCurrent(const Togl *togl)
}
declare 2 generic {
	void Togl_PostRedisplay(Togl *togl)
}
declare 3 generic {
	void Togl_SwapBuffers(const Togl *togl)
}
declare 33 generic {
	Bool Togl_SwapInterval(const Togl *togl, int interval)
}
declare 48 generic {
	int Togl_CopyContext(const Togl *from, const Togl *to, unsigned int mask)
}

# Query functions
declare 4 generic {
	const char *Togl_Ident(const Togl *togl)
}
declare 5 generic {
	int Togl_Width(const Togl *togl)
}
declare 6 generic {
	int Togl_Height(const Togl *togl)
}
declare 7 generic {
	Tcl_Interp *Togl_Interp(const Togl *togl)
}
declare 8 generic {
	Tk_Window Togl_TkWin(const Togl *togl)
}
declare 9 generic {
	const char *Togl_CommandName(const Togl *togl)
}
declare 36 generic {
	int Togl_ContextTag(const Togl *togl)
}
declare 37 generic {
	Bool Togl_UpdatePending(const Togl *togl)
}

declare 40 generic {
	Bool Togl_HasRGBA(const Togl *togl)
}

declare 41 generic {
	Bool Togl_IsDoubleBuffered(const Togl *togl)
}

declare 42 generic {
	Bool Togl_HasDepthBuffer(const Togl *togl)
}

declare 43 generic {
	Bool Togl_HasAccumulationBuffer(const Togl *togl)
}

declare 44 generic {
	Bool Togl_HasDestinationAlpha(const Togl *togl)
}

declare 45 generic {
	Bool Togl_HasStencilBuffer(const Togl *togl)
}

declare 46 generic {
	int Togl_StereoMode(const Togl *togl)
}

declare 47 generic {
	Bool Togl_HasMultisample(const Togl *togl)
}

# Color Index mode
declare 10 generic {
	unsigned long Togl_AllocColor(const Togl *togl, float red,
		float green, float blue)
}
declare 11 generic {
	void Togl_FreeColor(const Togl *togl, unsigned long index)
}
declare 12 generic {
	void Togl_SetColor(const Togl *togl, unsigned long index,
		float red, float green, float blue)
}

# Bitmap fonts
declare 13 generic {
	Tcl_Obj *Togl_LoadBitmapFont(const Togl *togl, const char *fontname)
}
declare 14 generic {
	int Togl_UnloadBitmapFont(const Togl *togl, Tcl_Obj *toglfont)
}

declare 38 generic {
	int Togl_WriteObj(const Togl *togl, const Tcl_Obj *toglfont, Tcl_Obj *obj)
}

declare 39 generic {
	int Togl_WriteChars(const Togl *togl, const Tcl_Obj *toglfont, const char *str, int len)
}

# Overlay functions
declare 15 generic {
	void Togl_UseLayer(Togl *togl, int layer)
}
declare 16 generic {
	void Togl_ShowOverlay(Togl *togl)
}
declare 17 generic {
	void Togl_HideOverlay(Togl *togl)
}
declare 18 generic {
	void Togl_PostOverlayRedisplay(Togl *togl)
}
declare 19 generic {
	int Togl_ExistsOverlay(const Togl *togl)
}
declare 20 generic {
	int Togl_GetOverlayTransparentValue(const Togl *togl)
}
declare 21 generic {
	int Togl_IsMappedOverlay(const Togl *togl)
}
declare 22 generic {
	unsigned long Togl_AllocColorOverlay(const Togl *togl,
		float red, float green, float blue)
}
declare 23 generic {
	void Togl_FreeColorOverlay(const Togl *togl, unsigned long index)
}

# User client data
declare 24 generic {
	ClientData Togl_GetClientData(const Togl *togl)
}
declare 25 generic {
	void Togl_SetClientData(Togl *togl, ClientData clientData)
}

# Stereo support
declare 26 generic {
	void Togl_DrawBuffer(Togl *togl, GLenum mode)
}
declare 27 generic {
	void Togl_Clear(const Togl *togl, GLbitfield mask)
}
declare 28 generic {
	void Togl_Frustum(const Togl *togl, GLdouble left, GLdouble right,
		GLdouble bottom, GLdouble top, GLdouble near, GLdouble far)
}
declare 34 generic {
	void Togl_Ortho(const Togl *togl, GLdouble left, GLdouble right,
		GLdouble bottom, GLdouble top, GLdouble near, GLdouble far)
}
declare 35 generic {
	int Togl_NumEyes(const Togl *togl)
}

# save current contents of OpenGL window into photo image
declare 30 generic {
	int Togl_TakePhoto(Togl *togl, Tk_PhotoHandle photo)
}

# platform-independent lookup of OpenGL functions
declare 31 generic {
	Togl_FuncPtr Togl_GetProcAddr(const char *funcname)
}

# Return the Togl data associated with pathName
declare 29 generic {
	int Togl_GetToglFromObj(Tcl_Interp *interp, Tcl_Obj *obj, Togl **toglPtr)
}
declare 32 generic {
	int Togl_GetToglFromName(Tcl_Interp *interp, const char *cmdName, Togl **toglPtr)
}
