/* 
 * getToglFromWidget:
 *
 * Given a Python widget, get the corresponding Togl pointer.  <Python.h>
 * and <togl.h> should be included before this.  If included into a C file,
 * there should be a static keyword just before the include.
 * 
 * There should be one copy of getToglFromWidget per-shared libary so that
 * the library's Tcl/Tk/Togl stub pointers are properly initialized.
 *
 * Copyright (C) 2006  Greg Couch
 * See the LICENSE file for copyright details.
 */

Togl   *
getToglFromWidget(PyObject *widget)
{
    PyObject *cmdNameObj, *tk, *interpAddr;
    const char *cmdName;
    Tcl_Interp *interp;
    Togl   *curTogl;

#ifdef USE_TOGL_STUBS
    static int didOnce = 0;
#endif

    /* Python: cmdName = widget._w */
    /* Python: interpAddr = widget.tk.interpaddr() */
    cmdNameObj = PyObject_GetAttrString(widget, "_w");
    tk = PyObject_GetAttrString(widget, "tk");
    if (cmdNameObj == NULL || !PyString_Check(cmdNameObj) || tk == NULL) {
        Py_XDECREF(cmdNameObj);
        Py_XDECREF(tk);
#ifdef __cplusplus
        throw   std::invalid_argument("not a Tk widget");
#else
        return NULL;
#endif
    }

    interpAddr = PyEval_CallMethod(tk, "interpaddr", "()");
    if (interpAddr == NULL || !PyInt_Check(interpAddr)) {
        Py_DECREF(cmdNameObj);
        Py_DECREF(tk);
        Py_XDECREF(interpAddr);
#ifdef __cplusplus
        throw   std::invalid_argument("not a Tk widget");
#else
        return NULL;
#endif
    }

    cmdName = PyString_AsString(cmdNameObj);
    interp = (Tcl_Interp *) PyInt_AsLong(interpAddr);

#ifdef USE_TOGL_STUBS
    if (!didOnce) {
        /* make sure stubs are initialized before calling a Togl function. */
        if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL
                || Tk_InitStubs(interp, TK_VERSION, 0) == NULL
                || Togl_InitStubs(interp, TOGL_VERSION, 0) == NULL)
#  ifdef __cplusplus
            throw   std::runtime_error("unable to initialize Togl");
#  else
            return NULL;
#  endif
        didOnce = 1;
    }
#endif

    if (Togl_GetToglFromName(interp, cmdName, &curTogl) != TCL_OK)
        curTogl = NULL;
    Py_DECREF(cmdNameObj);
    Py_DECREF(tk);
    Py_DECREF(interpAddr);
#ifdef __cplusplus
    if (curTogl == NULL)
        throw   std::invalid_argument("not a Togl widget");
#endif
    return curTogl;
}
