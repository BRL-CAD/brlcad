/*
 * TkDND_OleDND.h -- Tk OleDND Drag'n'Drop Protocol Implementation
 * 
 *    This file implements the unix portion of the drag&drop mechanism
 *    for the Tk toolkit. The protocol in use under windows is the
 *    OleDND protocol.
 *
 * This software is copyrighted by:
 * Georgios Petasis, Athens, Greece.
 * e-mail: petasisg@yahoo.gr, petasis@iit.demokritos.gr
 * Laurent Riesterer, Rennes, France.
 * e-mail: laurent.riesterer@free.fr
 *
 * The following terms apply to all files associated
 * with the software unless explicitly disclaimed in individual files.
 *
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 * 
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
 * DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
 * IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
 * NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 */

#include "OleDND.h"
#if defined(HAVE_STRSAFE_H) || !defined(NO_STRSAFE_H)
#include "Strsafe.h"
#endif

int TkDND_RegisterDragDropObjCmd(ClientData clientData, Tcl_Interp *interp,
                                 int objc, Tcl_Obj *CONST objv[]) {
  TkDND_DropTarget *pDropTarget;
  Tk_Window tkwin;
  HRESULT hret;
  
  if (objc != 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "path");
    return TCL_ERROR;
  }
  Tcl_ResetResult(interp);

  tkwin = TkDND_TkWin(objv[1]);
  if (tkwin == NULL) {
    Tcl_AppendResult(interp, "invalid Tk widget path: \"",
                             Tcl_GetString(objv[1]), (char *) NULL);
    return TCL_ERROR;
  }
  Tk_MakeWindowExist(tkwin);

  pDropTarget = new TkDND_DropTarget(interp, tkwin);
  if (pDropTarget == NULL) {
    Tcl_SetResult(interp, "out of memory", TCL_STATIC);
    return TCL_ERROR;
  }
  hret = RegisterDragDrop(Tk_GetHWND(Tk_WindowId(tkwin)), pDropTarget);
  switch (hret) {
    case E_OUTOFMEMORY: {
      delete pDropTarget;
      Tcl_AppendResult(interp, "unable to register \"", Tcl_GetString(objv[1]),
                "\" as a drop target: out of memory", (char *) NULL);
      break;
    }
    case DRAGDROP_E_INVALIDHWND: {
      delete pDropTarget;
      Tcl_AppendResult(interp, "unable to register \"", Tcl_GetString(objv[1]),
                "\" as a drop target: invalid window handle", (char *) NULL);
      break;
    }
    case DRAGDROP_E_ALREADYREGISTERED: {
      /* Silently ignore this. The window has been registered before. */
    }
    case S_OK: return TCL_OK;
  }
  return TCL_ERROR;
}; /* TkDND_RegisterDragDropObjCmd */

int TkDND_RevokeDragDropObjCmd(ClientData clientData, Tcl_Interp *interp,
                                 int objc, Tcl_Obj *CONST objv[]) {
  Tk_Window tkwin;
  HRESULT hret;
  
  if (objc != 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "path");
    return TCL_ERROR;
  }
  Tcl_ResetResult(interp);

  tkwin = TkDND_TkWin(objv[1]);
  if (tkwin == NULL) {
    Tcl_AppendResult(interp, "invalid Tk widget path: \"",
                             Tcl_GetString(objv[1]), (char *) NULL);
    return TCL_ERROR;
  }

  hret = RevokeDragDrop(Tk_GetHWND(Tk_WindowId(tkwin)));
  if (hret != S_OK) {
    Tcl_AppendResult(interp, "Tk widget \"", Tcl_GetString(objv[1]),
              "\" has never been registered as a drop target", (char *) NULL);
    return TCL_ERROR;
  }
              
  return TCL_OK;
}; /* TkDND_RevokeDragDropObjCmd */

int TkDND_DoDragDropObjCmd(ClientData clientData, Tcl_Interp *interp,
                           int objc, Tcl_Obj *CONST objv[]) {
  TkDND_DataObject *pDataObject = NULL;
  TkDND_DropSource *pDropSource = NULL;
  Tcl_Obj         **elem;
  DWORD             actions = 0;
  DWORD             dwEffect;
  DWORD             dwResult;
  int               status, elem_nu, i, index, nDataLength, button = 1;
  char             *ptr;
  Tcl_UniChar      *unicode, *ptr_u;
  FORMATETC        *m_pfmtetc;
  STGMEDIUM        *m_pstgmed;
  static const char *DropTypes[] = {
    "CF_UNICODETEXT", "CF_TEXT", "CF_HDROP",
    (char *) NULL
  };
  enum droptypes {
    TYPE_CF_UNICODETEXT, TYPE_CF_TEXT, TYPE_CF_HDROP
  };
  static const char *DropActions[] = {
    "copy", "move", "link", "ask",  "private", "refuse_drop",
    "default",
    (char *) NULL
  };
  enum dropactions {
    ActionCopy, ActionMove, ActionLink, ActionAsk, ActionPrivate,
    refuse_drop, ActionDefault
  };
  size_t buffer_size;

  if (objc != 5 && objc != 6) {
    Tcl_WrongNumArgs(interp, 1, objv, "path actions types data ?mouse-button?");
    return TCL_ERROR;
  }
  Tcl_ResetResult(interp);

  /* Get the mouse button. It must be one of 1, 2, or 3. */
  if (objc > 5) {
    status = Tcl_GetIntFromObj(interp, objv[5], &button);
    if (status != TCL_OK) return status;
    if (button < 1 || button > 3) {
      Tcl_SetResult(interp, "button must be either 1, 2, or 3", TCL_STATIC);
      return TCL_ERROR;
    }
  }

  /* Process drag actions. */
  status = Tcl_ListObjGetElements(interp, objv[2], &elem_nu, &elem);
  if (status != TCL_OK) return status;
  for (i = 0; i < elem_nu; i++) {
    status = Tcl_GetIndexFromObj(interp, elem[i], (const char **)DropActions,
                                 "dropactions", 0, &index);
    if (status != TCL_OK) return status;
    switch ((enum dropactions) index) {
      case ActionCopy:    actions |= DROPEFFECT_COPY; break;
      case ActionMove:    actions |= DROPEFFECT_MOVE; break;
      case ActionLink:    actions |= DROPEFFECT_LINK; break;
      case ActionAsk:     /* not supported */;        break;
      case ActionPrivate: actions |= DROPEFFECT_NONE; break;
      case ActionDefault: /* not supported */;        break;
      case refuse_drop:   /* not supported */;        break;
    }
  }

  /* Process drag types. */
  status = Tcl_ListObjGetElements(interp, objv[3], &elem_nu, &elem);
  if (status != TCL_OK) return status;
  m_pfmtetc  = new FORMATETC[elem_nu];
  if (m_pfmtetc == NULL) return TCL_ERROR;
  m_pstgmed  = new STGMEDIUM[elem_nu];
  if (m_pstgmed == NULL) {
    delete[] m_pfmtetc; return TCL_ERROR;
  }
  for (i = 0; i < elem_nu; i++) {
    m_pfmtetc[i].ptd            = 0;
    m_pfmtetc[i].dwAspect       = DVASPECT_CONTENT;
    m_pfmtetc[i].lindex         = -1;
    m_pfmtetc[i].tymed          = TYMED_HGLOBAL;
    m_pstgmed[i].tymed          = TYMED_HGLOBAL;
    m_pstgmed[i].pUnkForRelease = 0;
    status = Tcl_GetIndexFromObj(interp, elem[i], (const char **) DropTypes,
                                 "dropactions", 0, &index);
    if (status == TCL_OK) {
      switch ((enum droptypes) index) {
        case TYPE_CF_UNICODETEXT: {
          m_pfmtetc[i].cfFormat = CF_UNICODETEXT;
          unicode = Tcl_GetUnicodeFromObj(objv[4], &nDataLength);
          buffer_size = (nDataLength+1) * sizeof(Tcl_UniChar);
          m_pstgmed[i].hGlobal = GlobalAlloc(GHND, buffer_size);
          if (m_pstgmed[i].hGlobal) {
            ptr_u = (Tcl_UniChar *) GlobalLock(m_pstgmed[i].hGlobal);
#ifdef HAVE_STRSAFE_H
            StringCchCopyW((LPWSTR) ptr_u, buffer_size, (LPWSTR) unicode);
#else
            lstrcpyW((LPWSTR) ptr_u, (LPWSTR) unicode);
#endif
            GlobalUnlock(m_pstgmed[i].hGlobal);
          }
          break;
        }
        case TYPE_CF_TEXT: {
          m_pfmtetc[i].cfFormat = CF_TEXT;
          nDataLength = Tcl_GetCharLength(objv[4]);
          m_pstgmed[i].hGlobal = GlobalAlloc(GHND, nDataLength+1);
          if (m_pstgmed[i].hGlobal) {
            ptr = (char *) GlobalLock(m_pstgmed[i].hGlobal);
            memcpy(ptr, Tcl_GetString(objv[4]), nDataLength);
            ptr[nDataLength] = '\0';
            GlobalUnlock(m_pstgmed[i].hGlobal);
          }
          break;
        }
        case TYPE_CF_HDROP: {
          LPDROPFILES pDropFiles;
          Tcl_DString ds;
          Tcl_Obj **File, *native_files_obj = NULL, *obj;
          int file_nu, j, size, len;
          char *native_name;

          status = Tcl_ListObjGetElements(interp, objv[4], &file_nu, &File);
          if (status != TCL_OK) {elem_nu = i; goto error;}
          /* What we expect is a list of filenames. Convert the filenames into
           * the native format, and store the translated filenames into a new
           * list... */
          native_files_obj = Tcl_NewListObj(0, NULL);
          if (native_files_obj == NULL) {elem_nu = i; goto error;}
          size = 0;
          for (j = 0; j < file_nu; ++j) {
            Tcl_DStringInit(&ds);
            native_name = Tcl_TranslateFileName(NULL, 
                                                Tcl_GetString(File[j]), &ds);
            if (native_name == NULL) {
              Tcl_DStringFree(&ds);
              continue;
            }
            obj = Tcl_NewStringObj(native_name, -1);
            Tcl_ListObjAppendElement(NULL, native_files_obj, obj);
            /* Get the length in unicode... */
            Tcl_GetUnicodeFromObj(obj, &len);
            size += len + 1; // NULL character...
            Tcl_DStringFree(&ds);
          }

          buffer_size = sizeof(wchar_t) * (size+1);
          m_pfmtetc[i].cfFormat = CF_HDROP;
          m_pstgmed[i].hGlobal = GlobalAlloc(GHND, 
                   (DWORD) (sizeof(DROPFILES) + buffer_size));
          if (m_pstgmed[i].hGlobal) {
            TCHAR *CurPosition;
            pDropFiles = (LPDROPFILES) GlobalLock(m_pstgmed[i].hGlobal);
            // Set the offset where the starting point of the file start.
            pDropFiles->pFiles = sizeof(DROPFILES);
            // File contains wide characters?
            pDropFiles->fWide = TRUE;
            CurPosition = (TCHAR *) (LPBYTE(pDropFiles) + sizeof(DROPFILES)); 
            Tcl_ListObjGetElements(NULL, native_files_obj, &file_nu, &File);
            for (j = 0; j < file_nu; j++) {
              TCHAR *pszFileName = (TCHAR *)
                                   Tcl_GetUnicodeFromObj(File[j], &len);
              // Copy the file name into global memory.
#ifdef HAVE_STRSAFE_H
              StringCchCopyW(CurPosition, buffer_size, pszFileName);
#else
              lstrcpyW(CurPosition, pszFileName);
#endif
              /*
               * Move the current position beyond the file name copied, and
               * don't forget the NULL terminator (+1)
               */
              CurPosition += 1 + _tcschr(pszFileName, '\0') - pszFileName;
            }
            /*
             * Finally, add an additional null terminator, as per CF_HDROP
             * Format specs.
             */
            *CurPosition = '\0';
            GlobalUnlock(m_pstgmed[i].hGlobal);
          }
          if (native_files_obj) Tcl_DecrRefCount(native_files_obj);
          break;
        }
      }
    } else {
      unsigned char *bytes;
      /* A user defined type? */
      m_pfmtetc[i].cfFormat = RegisterClipboardFormat(TCL_GETSTRING(elem[i]));
      bytes = Tcl_GetByteArrayFromObj(objv[4], &nDataLength);
      m_pstgmed[i].hGlobal = GlobalAlloc(GHND, nDataLength);
      if (m_pstgmed[i].hGlobal) {
        ptr = (char *) GlobalLock(m_pstgmed[i].hGlobal);
        memcpy(ptr, bytes, nDataLength);
        GlobalUnlock(m_pstgmed[i].hGlobal);
      }
      break;
    }
  }; /* for (i = 0; i < elem_nu; i++) */
  
  pDataObject = new TkDND_DataObject(m_pfmtetc, m_pstgmed, elem_nu);
  if (pDataObject == NULL) {
    Tcl_SetResult(interp, "unable to create OLE Data object", TCL_STATIC);
    return TCL_ERROR;
  }
  
  pDropSource = new TkDND_DropSource(button);
  if (pDropSource == NULL) {
    pDataObject->Release();
    Tcl_SetResult(interp, "unable to create OLE Drop Source object",TCL_STATIC);
    return TCL_ERROR;
  }

  dwResult = DoDragDrop(pDataObject, pDropSource, actions, &dwEffect);
  // release the COM interfaces
  pDropSource->Release();
  pDataObject->Release();
  for (i = 0; i < elem_nu; i++) {
    ReleaseStgMedium(&m_pstgmed[i]);
  }
  delete[] m_pfmtetc;
  delete[] m_pstgmed;
  if (dwResult == DRAGDROP_S_DROP) {
    switch (dwEffect) {
      case DROPEFFECT_COPY: Tcl_SetResult(interp, "copy", TCL_STATIC); break;
      case DROPEFFECT_MOVE: Tcl_SetResult(interp, "move", TCL_STATIC); break;
      case DROPEFFECT_LINK: Tcl_SetResult(interp, "link", TCL_STATIC); break;
    }
  } else {
    Tcl_SetResult(interp, "refuse_drop", TCL_STATIC);
  }
  return TCL_OK;
error:
  // release the COM interfaces
  if (pDropSource) pDropSource->Release();
  if (pDataObject) pDataObject->Release();
  for (i = 0; i < elem_nu; i++) {
    ReleaseStgMedium(&m_pstgmed[i]);
  }
  delete[] m_pfmtetc;
  delete[] m_pstgmed;
  return TCL_ERROR;
}; /* TkDND_DoDragDropObjCmd */

/*
 * For C++ compilers, use extern "C"
 */
#ifdef __cplusplus
extern "C" {
#endif
DLLEXPORT int Tkdnd_Init(Tcl_Interp *interp);
DLLEXPORT int Tkdnd_SafeInit(Tcl_Interp *interp);
#ifdef __cplusplus
}
#endif

int DLLEXPORT Tkdnd_Init(Tcl_Interp *interp) {
  int major, minor, patchlevel;
  HRESULT hret;

  if (
#ifdef USE_TCL_STUBS 
      Tcl_InitStubs(interp, "8.3", 0)
#else
      Tcl_PkgRequire(interp, "Tcl", "8.3", 0)
#endif /* USE_TCL_STUBS */
            == NULL) {
            return TCL_ERROR;
  }
  if (
#ifdef USE_TK_STUBS
       Tk_InitStubs(interp, "8.3", 0)
#else
       Tcl_PkgRequire(interp, "Tk", "8.3", 0)
#endif /* USE_TK_STUBS */
            == NULL) {
            return TCL_ERROR;
  }

  /*
   * Get the version, because we really need 8.3.3+.
   */
  Tcl_GetVersion(&major, &minor, &patchlevel, NULL);
  if ((major == 8) && (minor == 3) && (patchlevel < 3)) {
    Tcl_SetResult(interp, "tkdnd requires Tk 8.3.3 or greater", TCL_STATIC);
    return TCL_ERROR;
  }

  /*
   * Initialise OLE.
   */
  hret = OleInitialize(NULL);
  
  /*
   * If OleInitialize returns S_FALSE, OLE has already been initialized
   */
  if (hret != S_OK && hret != S_FALSE) {
    Tcl_AppendResult(interp, "unable to initialize OLE2",
      (char *) NULL);
    return TCL_ERROR;
  }

  /* Register the various commands */
  if (Tcl_CreateObjCommand(interp, "_RegisterDragDrop",
           (Tcl_ObjCmdProc*) TkDND_RegisterDragDropObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
      return TCL_ERROR;
  }
  if (Tcl_CreateObjCommand(interp, "_RevokeDragDrop",
           (Tcl_ObjCmdProc*) TkDND_RevokeDragDropObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
      return TCL_ERROR;
  }

  if (Tcl_CreateObjCommand(interp, "_DoDragDrop",
           (Tcl_ObjCmdProc*) TkDND_DoDragDropObjCmd,
           (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL) == NULL) {
      return TCL_ERROR;
  }

  Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION);
  return TCL_OK;
} /* Tkdnd_Init */

int DLLEXPORT Tkdnd_SafeInit(Tcl_Interp *interp) {
  return Tkdnd_Init(interp);
} /* Tkdnd_SafeInit */
