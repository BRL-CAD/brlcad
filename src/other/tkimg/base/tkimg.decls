# tkimg.decls -- -*- tcl -*-
#
# This file contains the declarations for all supported public functions
# that are exported by the TKIMG library via the stubs table. This file
# is used to generate the tkimgDecls.h/tkimgStubsLib.c/tkimgStubsInit.c
# files.
#	

# Declare each of the functions in the public BLT interface.  Note that
# the an index should never be reused for a different function in order
# to preserve backwards compatibility.

library tkimg

# Define the TKIMG interface:

interface tkimg
#hooks {}

#########################################################################
###  Reading and writing image data from channels and/or strings.

declare 0 generic {
    Tcl_Channel tkimg_OpenFileChannel (Tcl_Interp *interp, 
	CONST char *fileName, int permissions)
}
declare 1 generic {
    int tkimg_ReadInit (Tcl_Obj *data, int c, tkimg_MFile *handle)
}
declare 2 generic {
    void tkimg_WriteInit (Tcl_DString *buffer, tkimg_MFile *handle)
}
declare 3 generic {
    int tkimg_Getc (tkimg_MFile *handle)
}
declare 4 generic {
    int tkimg_Read (tkimg_MFile *handle, char *dst, int count)
}
declare 5 generic {
    int tkimg_Putc (int c, tkimg_MFile *handle)
}
declare 6 generic {
    int tkimg_Write (tkimg_MFile *handle, CONST char *src, int count)
}
declare 7 generic {
    void tkimg_ReadBuffer (int onOff)
}

#########################################################################
###  Specialized put block handling transparency

declare 10 generic {
    int tkimg_PhotoPutBlock (Tk_PhotoHandle handle,
	Tk_PhotoImageBlock *blockPtr, int x, int y, int width, int height)
}

#########################################################################
###  Utilities to help handling the differences in 8.3.2 and 8.3 image types.

declare 20 generic {
    void tkimg_FixChanMatchProc (Tcl_Interp **interp, Tcl_Channel *chan,
	CONST char **file, Tcl_Obj **format, int **width, int **height)
}
declare 21 generic {
    void tkimg_FixObjMatchProc (Tcl_Interp **interp, Tcl_Obj **data,
	Tcl_Obj **format, int **width, int **height)
}
declare 22 generic {
    void tkimg_FixStringWriteProc (Tcl_DString *data, Tcl_Interp **interp,
	Tcl_DString **dataPtr, Tcl_Obj **format, Tk_PhotoImageBlock **blockPtr)
}

#########################################################################
###  Like the core functions, except that they accept objPtr == NULL.
###  The byte array function also handles both UTF and non-UTF cores.

declare 30 generic {
    char* tkimg_GetStringFromObj (Tcl_Obj *objPtr, int *lengthPtr)
}
declare 31 generic {
    char* tkimg_GetByteArrayFromObj (Tcl_Obj *objPtr, int *lengthPtr)
}
declare 32 generic {
    int tkimg_ListObjGetElements (Tcl_Interp *interp, Tcl_Obj *objPtr, int *argc, Tcl_Obj ***argv)
}

#########################################################################
