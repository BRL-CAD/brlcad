/* 
 * tclWinReg.c --
 *
 *	This file contains the implementation of the "registry" Tcl
 *	built-in command.  This command is built as a dynamically
 *	loadable extension in a separate DLL.
 *
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclWinReg.c 1.8 97/08/01 11:17:49
 */

#include <tcl.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

/*
 * VC++ has an alternate entry point called DllMain, so we need to rename
 * our entry point.
 */

#ifndef STATIC_BUILD
#if defined(_MSC_VER)
#   define EXPORT(a,b) __declspec(dllexport) a b
#   define DllEntryPoint DllMain
#else
#   if defined(__BORLANDC__)
#	define EXPORT(a,b) a _export b
#   else
#	define EXPORT(a,b) a b
#   endif
#endif
#endif

/*
 * The following macros convert between different endian ints.
 */

#define SWAPWORD(x) MAKEWORD(HIBYTE(x), LOBYTE(x))
#define SWAPLONG(x) MAKELONG(SWAPWORD(HIWORD(x)), SWAPWORD(LOWORD(x)))
 
/*
 * The following flag is used in OpenKeys to indicate that the specified
 * key should be created if it doesn't currently exist.
 */

#define REG_CREATE 1

/*
 * The following tables contain the mapping from registry root names
 * to the system predefined keys.
 */

static char *rootKeyNames[] = {
    "HKEY_LOCAL_MACHINE", "HKEY_USERS", "HKEY_CLASSES_ROOT",
    "HKEY_CURRENT_USER", "HKEY_CURRENT_CONFIG", NULL
};

static HKEY rootKeys[] = {
    HKEY_LOCAL_MACHINE, HKEY_USERS, HKEY_CLASSES_ROOT, HKEY_CURRENT_USER,
    HKEY_CURRENT_CONFIG, HKEY_PERFORMANCE_DATA, HKEY_DYN_DATA
};

/*
 * The following table maps from registry types to strings.  Note that
 * the indices for this array are the same as the constants for the
 * known registry types so we don't need a separate table to hold the
 * mapping.
 */

static char *typeNames[] = {
    "none", "sz", "expand_sz", "binary", "dword", 
    "dword_big_endian", "link", "multi_sz", "resource_list", NULL
};

static DWORD lastType = REG_RESOURCE_REQUIREMENTS_LIST;


/*
 * Declarations for functions defined in this file.
 */

static void		AppendSystemError(Tcl_Interp *interp, DWORD error);
static DWORD		ConvertDWORD(DWORD type, DWORD value);
static int		DeleteKey(Tcl_Interp *interp, Tcl_Obj *keyNameObj);
static int		DeleteValue(Tcl_Interp *interp, Tcl_Obj *keyNameObj,
			    Tcl_Obj *valueNameObj);
static int		GetKeyNames(Tcl_Interp *interp, Tcl_Obj *keyNameObj,
			    Tcl_Obj *patternObj);
static int		GetType(Tcl_Interp *interp, Tcl_Obj *keyNameObj,
			    Tcl_Obj *valueNameObj);
static int		GetValue(Tcl_Interp *interp, Tcl_Obj *keyNameObj,
			    Tcl_Obj *valueNameObj);
static int		GetValueNames(Tcl_Interp *interp, Tcl_Obj *keyNameObj,
			    Tcl_Obj *patternObj);
static int		OpenKey(Tcl_Interp *interp, Tcl_Obj *keyNameObj,
			    REGSAM mode, int flags, HKEY *keyPtr);
static DWORD		OpenSubKey(char *hostName, HKEY rootKey,
			    char *keyName, REGSAM mode, int flags,
			    HKEY *keyPtr);
static int		ParseKeyName(Tcl_Interp *interp, char *name,
			    char **hostNamePtr, HKEY *rootKeyPtr,
			    char **keyNamePtr);
static DWORD		RecursiveDeleteKey(HKEY hStartKey, LPTSTR pKeyName);
static int		RegistryObjCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc, Tcl_Obj * CONST objv[]);
static int		SetValue(Tcl_Interp *interp, Tcl_Obj *keyNameObj,
			    Tcl_Obj *valueNameObj, Tcl_Obj *dataObj,
			    Tcl_Obj *typeObj);

EXTERN EXPORT(int,Registry_Init)(Tcl_Interp *interp);

/*
 *----------------------------------------------------------------------
 *
 * DllEntryPoint --
 *
 *	This wrapper function is used by Windows to invoke the
 *	initialization code for the DLL.  If we are compiling
 *	with Visual C++, this routine will be renamed to DllMain.
 *	routine.
 *
 * Results:
 *	Returns TRUE;
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifdef __WIN32__
#ifndef STATIC_BUILD
BOOL APIENTRY
DllEntryPoint(
    HINSTANCE hInst,		/* Library instance handle. */
    DWORD reason,		/* Reason this function is being called. */
    LPVOID reserved)		/* Not used. */
{
    return TRUE;
}
#endif
#endif

/*
 *----------------------------------------------------------------------
 *
 * Registry_Init --
 *
 *	This procedure initializes the registry command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

EXPORT(int,Registry_Init)(
    Tcl_Interp *interp)
{
    Tcl_CreateObjCommand(interp, "registry", RegistryObjCmd, NULL, NULL);
    return Tcl_PkgProvide(interp, "registry", "1.0");
}

/*
 *----------------------------------------------------------------------
 *
 * RegistryObjCmd --
 *
 *	This function implements the Tcl "registry" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
RegistryObjCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj * CONST objv[])	/* Argument values. */
{
    int index;
    char *errString;

    static char *subcommands[] = { "delete", "get", "keys", "set", "type",
				   "values", (char *) NULL };
    enum SubCmdIdx { DeleteIdx, GetIdx, KeysIdx, SetIdx, TypeIdx, ValuesIdx };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, objc, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], subcommands, "option", 0, &index)
	    != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
	case DeleteIdx:			/* delete */
	    if (objc == 3) {
		return DeleteKey(interp, objv[2]);
	    } else if (objc == 4) {
		return DeleteValue(interp, objv[2], objv[3]);
	    }
	    errString = "keyName ?valueName?";
	    break;
	case GetIdx:			/* get */
	    if (objc == 4) {
		return GetValue(interp, objv[2], objv[3]);
	    }
	    errString = "keyName valueName";
	    break;
	case KeysIdx:			/* keys */
	    if (objc == 3) {
		return GetKeyNames(interp, objv[2], NULL);
	    } else if (objc == 4) {
		return GetKeyNames(interp, objv[2], objv[3]);
	    }
	    errString = "keyName ?pattern?";
	    break;
	case SetIdx:			/* set */
	    if (objc == 3) {
		HKEY key;

		/*
		 * Create the key and then close it immediately.
		 */

		if (OpenKey(interp, objv[2], KEY_ALL_ACCESS, 1, &key)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
		RegCloseKey(key);
		return TCL_OK;
	    } else if (objc == 5 || objc == 6) {
		Tcl_Obj *typeObj = (objc == 5) ? NULL : objv[5];
		return SetValue(interp, objv[2], objv[3], objv[4], typeObj);
	    }
	    errString = "keyName ?valueName data ?type??";
	    break;
	case TypeIdx:			/* type */
	    if (objc == 4) {
		return GetType(interp, objv[2], objv[3]);
	    }
	    errString = "keyName valueName";
	    break;
	case ValuesIdx:			/* values */
	    if (objc == 3) {
 		return GetValueNames(interp, objv[2], NULL);
	    } else if (objc == 4) {
 		return GetValueNames(interp, objv[2], objv[3]);
	    }
	    errString = "keyName ?pattern?";
	    break;
    }
    Tcl_WrongNumArgs(interp, 2, objv, errString);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteKey --
 *
 *	This function deletes a registry key.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
DeleteKey(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *keyNameObj)	/* Name of key to delete. */
{
    char *tail, *buffer, *hostName, *keyName;
    HKEY rootKey, subkey;
    DWORD result;
    int length;
    Tcl_Obj *resultPtr;

    /*
     * Find the parent of the key being deleted and open it.
     */

    keyName = Tcl_GetStringFromObj(keyNameObj, &length);
    buffer = ckalloc(length + 1);
    strcpy(buffer, keyName);

    if (ParseKeyName(interp, buffer, &hostName, &rootKey, &keyName)
	    != TCL_OK) {
	ckfree(buffer);
	return TCL_ERROR;
    }

    resultPtr = Tcl_GetObjResult(interp);
    if (*keyName == '\0') {
	Tcl_AppendToObj(resultPtr, "bad key: cannot delete root keys", -1);
	ckfree(buffer);
	return TCL_ERROR;
    }

    tail = strrchr(keyName, '\\');
    if (tail) {
	*tail++ = '\0';
    } else {
	tail = keyName;
	keyName = NULL;
    }

    result = OpenSubKey(hostName, rootKey, keyName,
	    KEY_ENUMERATE_SUB_KEYS | DELETE, 0, &subkey);
    if (result != ERROR_SUCCESS) {
	ckfree(buffer);
	if (result == ERROR_FILE_NOT_FOUND) {
	    return TCL_OK;
	} else {
	    Tcl_AppendToObj(resultPtr, "unable to delete key: ", -1);
	    AppendSystemError(interp, result);
	    return TCL_ERROR;
	}
    }

    /*
     * Now we recursively delete the key and everything below it.
     */

    result = RecursiveDeleteKey(subkey, tail);

    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
	Tcl_AppendToObj(resultPtr, "unable to delete key: ", -1);
	AppendSystemError(interp, result);
	result = TCL_ERROR;
    } else {
	result = TCL_OK;
    }

    RegCloseKey(subkey);
    ckfree(buffer);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteValue --
 *
 *	This function deletes a value from a registry key.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
DeleteValue(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *keyNameObj,	/* Name of key. */
    Tcl_Obj *valueNameObj)	/* Name of value to delete. */
{
    HKEY key;
    char *valueName;
    int length;
    DWORD result;
    Tcl_Obj *resultPtr;
    
    /*
     * Attempt to open the key for deletion.
     */

    if (OpenKey(interp, keyNameObj, KEY_SET_VALUE, 0, &key)
	    != TCL_OK) {
	return TCL_ERROR;
    }

    resultPtr = Tcl_GetObjResult(interp);
    valueName = Tcl_GetStringFromObj(valueNameObj, &length);
    result = RegDeleteValue(key, valueName);
    if (result != ERROR_SUCCESS) {
	Tcl_AppendStringsToObj(resultPtr, "unable to delete value \"",
		Tcl_GetStringFromObj(valueNameObj, NULL), "\" from key \"",
		Tcl_GetStringFromObj(keyNameObj, NULL), "\": ", NULL);
	AppendSystemError(interp, result);
	result = TCL_ERROR;
    } else {
	result = TCL_OK;
    }
    RegCloseKey(key);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * GetKeyNames --
 *
 *	This function enumerates the subkeys of a given key.  If the
 *	optional pattern is supplied, then only keys that match the
 *	pattern will be returned.
 *
 * Results:
 *	Returns the list of subkeys in the result object of the
 *	interpreter, or an error message on failure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetKeyNames(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *keyNameObj,	/* Key to enumerate. */
    Tcl_Obj *patternObj)	/* Optional match pattern. */
{
    HKEY key;
    DWORD index;
    char buffer[MAX_PATH+1], *pattern;
    Tcl_Obj *resultPtr;
    int result = TCL_OK;

    /*
     * Attempt to open the key for enumeration.
     */

    if (OpenKey(interp, keyNameObj, KEY_ENUMERATE_SUB_KEYS, 0, &key)
	    != TCL_OK) {
	return TCL_ERROR;
    }

    if (patternObj) {
	pattern = Tcl_GetStringFromObj(patternObj, NULL);
    } else {
	pattern = NULL;
    }

    /*
     * Enumerate over the subkeys until we get an error, indicating the
     * end of the list.
     */

    resultPtr = Tcl_GetObjResult(interp);
    for (index = 0; RegEnumKey(key, index, buffer, MAX_PATH+1)
	     == ERROR_SUCCESS; index++) {
	if (pattern && !Tcl_StringMatch(buffer, pattern)) {
	    continue;
	}
	result = Tcl_ListObjAppendElement(interp, resultPtr,
		Tcl_NewStringObj(buffer, -1));
	if (result != TCL_OK) {
	    break;
	}
    }

    RegCloseKey(key);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * GetType --
 *
 *	This function gets the type of a given registry value and
 *	places it in the interpreter result.
 *
 * Results:
 *	Returns a normal Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetType(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *keyNameObj,	/* Name of key. */
    Tcl_Obj *valueNameObj)	/* Name of value to get. */
{
    HKEY key;
    Tcl_Obj *resultPtr;
    DWORD result;
    DWORD type;
    
    /*
     * Attempt to open the key for reading.
     */

    if (OpenKey(interp, keyNameObj, KEY_QUERY_VALUE, 0, &key)
	    != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Get the type of the value.
     */

    resultPtr = Tcl_GetObjResult(interp);

    result = RegQueryValueEx(key, Tcl_GetStringFromObj(valueNameObj, NULL),
	    NULL, &type, NULL, NULL);
    RegCloseKey(key);

    if (result != ERROR_SUCCESS) {
	Tcl_AppendStringsToObj(resultPtr, "unable to get type of value \"",
		Tcl_GetStringFromObj(valueNameObj, NULL), "\" from key \"",
		Tcl_GetStringFromObj(keyNameObj, NULL), "\": ", NULL);
	AppendSystemError(interp, result);
	return TCL_ERROR;
    }

    /*
     * Set the type into the result.  Watch out for unknown types.
     * If we don't know about the type, just use the numeric value.
     */

    if (type > lastType) {
	Tcl_SetIntObj(resultPtr, type);
    } else {
	Tcl_SetStringObj(resultPtr, typeNames[type], -1);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetValue --
 *
 *	This function gets the contents of a registry value and places
 *	a list containing the data and the type in the interpreter
 *	result.
 *
 * Results:
 *	Returns a normal Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetValue(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *keyNameObj,	/* Name of key. */
    Tcl_Obj *valueNameObj)	/* Name of value to get. */
{
    HKEY key;
    char *valueName;
    DWORD result, length, type;
    Tcl_Obj *resultPtr;
    Tcl_DString data;

    /*
     * Attempt to open the key for reading.
     */

    if (OpenKey(interp, keyNameObj, KEY_QUERY_VALUE, 0, &key)
	    != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Get the value once to determine the length then again to store
     * the data in the buffer.
     */

    Tcl_DStringInit(&data);
    resultPtr = Tcl_GetObjResult(interp);

    valueName = Tcl_GetStringFromObj(valueNameObj, (int*) &length);
    result = RegQueryValueEx(key, valueName, NULL, &type, NULL, &length);
    if (result == ERROR_SUCCESS) {
	Tcl_DStringSetLength(&data, length);
	result = RegQueryValueEx(key, valueName, NULL, &type,
		(LPBYTE) Tcl_DStringValue(&data), &length);
    }
    RegCloseKey(key);
    if (result != ERROR_SUCCESS) {
	Tcl_AppendStringsToObj(resultPtr, "unable to get value \"",
		Tcl_GetStringFromObj(valueNameObj, NULL), "\" from key \"",
		Tcl_GetStringFromObj(keyNameObj, NULL), "\": ", NULL);
	AppendSystemError(interp, result);
	Tcl_DStringFree(&data);
	return TCL_ERROR;
    }

    /*
     * If the data is a 32-bit quantity, store it as an integer object.  If it
     * is a multi-string, store it as a list of strings.  For null-terminated
     * strings, append up the to first null.  Otherwise, store it as a binary
     * string.
     */

    if (type == REG_DWORD || type == REG_DWORD_BIG_ENDIAN) {
	Tcl_SetIntObj(resultPtr, ConvertDWORD(type,
		*((DWORD*) Tcl_DStringValue(&data))));
    } else if (type == REG_MULTI_SZ) {
	char *p = Tcl_DStringValue(&data);
	char *lastChar = Tcl_DStringValue(&data) + Tcl_DStringLength(&data);

	/*
	 * Multistrings are stored as an array of null-terminated strings,
	 * terminated by two null characters.  Also do a bounds check in
	 * case we get bogus data.
	 */

	while (p < lastChar && *p != '\0') {
	    Tcl_ListObjAppendElement(interp, resultPtr,
		    Tcl_NewStringObj(p, -1));
	    while (*p++ != '\0') {}
	}
    } else if ((type == REG_SZ) || (type == REG_EXPAND_SZ)) {
	Tcl_SetStringObj(resultPtr, Tcl_DStringValue(&data), -1);
    } else {
	Tcl_SetStringObj(resultPtr, Tcl_DStringValue(&data), length);
    }
    Tcl_DStringFree(&data);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * GetValueNames --
 *
 *	This function enumerates the values of the a given key.  If
 *	the optional pattern is supplied, then only value names that
 *	match the pattern will be returned.
 *
 * Results:
 *	Returns the list of value names in the result object of the
 *	interpreter, or an error message on failure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetValueNames(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *keyNameObj,	/* Key to enumerate. */
    Tcl_Obj *patternObj)	/* Optional match pattern. */
{
    HKEY key;
    Tcl_Obj *resultPtr;
    DWORD index, size, result;
    Tcl_DString buffer;
    char *pattern;

    /*
     * Attempt to open the key for enumeration.
     */

    if (OpenKey(interp, keyNameObj, KEY_QUERY_VALUE, 0, &key)
	    != TCL_OK) {
	return TCL_ERROR;
    }

    resultPtr = Tcl_GetObjResult(interp);

    /*
     * Query the key to determine the appropriate buffer size to hold the
     * largest value name plus the terminating null.
     */

    result = RegQueryInfoKey(key, NULL, NULL, NULL, NULL, NULL, NULL, &index,
	&size, NULL, NULL, NULL);
    if (result != ERROR_SUCCESS) {
	Tcl_AppendStringsToObj(resultPtr, "unable to query key \"",
		Tcl_GetStringFromObj(keyNameObj, NULL), "\": ", NULL);
	AppendSystemError(interp, result);
	RegCloseKey(key);
	result = TCL_ERROR;
	goto done;
    }
    size++;


    Tcl_DStringInit(&buffer);
    Tcl_DStringSetLength(&buffer, size);
    index = 0;
    result = TCL_OK;

    if (patternObj) {
	pattern = Tcl_GetStringFromObj(patternObj, NULL);
    } else {
	pattern = NULL;
    }

    /*
     * Enumerate the values under the given subkey until we get an error,
     * indicating the end of the list.  Note that we need to reset size
     * after each iteration because RegEnumValue smashes the old value.
     */

    while (RegEnumValue(key, index, Tcl_DStringValue(&buffer), &size, NULL,
	    NULL, NULL, NULL) == ERROR_SUCCESS) {
	if (!pattern || Tcl_StringMatch(Tcl_DStringValue(&buffer), pattern)) {
	    result = Tcl_ListObjAppendElement(interp, resultPtr,
		    Tcl_NewStringObj(Tcl_DStringValue(&buffer), size));
	    if (result != TCL_OK) {
		break;
	    }
	}
	index++;
	size = Tcl_DStringLength(&buffer);
    }
    Tcl_DStringFree(&buffer);

    done:
    RegCloseKey(key);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * OpenKey --
 *
 *	This function opens the specified key.  This function is a
 *	simple wrapper around ParseKeyName and OpenSubKey.
 *
 * Results:
 *	Returns the opened key in the keyPtr argument and a Tcl
 *	result code.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
OpenKey(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *keyNameObj,	/* Key to open. */
    REGSAM mode,		/* Access mode. */
    int flags,			/* 0 or REG_CREATE. */
    HKEY *keyPtr)		/* Returned HKEY. */
{
    char *keyName, *buffer, *hostName;
    int length;
    HKEY rootKey;
    DWORD result;

    keyName = Tcl_GetStringFromObj(keyNameObj, &length);
    buffer = ckalloc(length + 1);
    strcpy(buffer, keyName);

    result = ParseKeyName(interp, buffer, &hostName, &rootKey, &keyName);
    if (result == TCL_OK) {
	result = OpenSubKey(hostName, rootKey, keyName, mode, flags, keyPtr);
	if (result != ERROR_SUCCESS) {
	    Tcl_Obj *resultPtr = Tcl_GetObjResult(interp);
	    Tcl_AppendToObj(resultPtr, "unable to open key: ", -1);
	    AppendSystemError(interp, result);
	    result = TCL_ERROR;
	} else {
	    result = TCL_OK;
	}
    }

    ckfree(buffer);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * OpenSubKey --
 *
 *	This function opens a given subkey of a root key on the
 *	specified host.
 *
 * Results:
 *	Returns the opened key in the keyPtr and a Windows error code
 *	as the return value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static DWORD
OpenSubKey(
    char *hostName,		/* Host to access, or NULL for local. */
    HKEY rootKey,		/* Root registry key. */
    char *keyName,		/* Subkey name. */
    REGSAM mode,		/* Access mode. */
    int flags,			/* 0 or REG_CREATE. */
    HKEY *keyPtr)		/* Returned HKEY. */
{
    DWORD result;

    /*
     * Attempt to open the root key on a remote host if necessary.
     */

    if (hostName) {
	result = RegConnectRegistry(hostName, rootKey, &rootKey);
	if (result != ERROR_SUCCESS) {
	    return result;
	}
    }

    /*
     * Now open the specified key with the requested permissions.  Note
     * that this key must be closed by the caller.
     */

    if (flags & REG_CREATE) {
	DWORD create;
	result = RegCreateKeyEx(rootKey, keyName, 0, "",
		REG_OPTION_NON_VOLATILE, mode, NULL, keyPtr, &create);
    } else {
	result = RegOpenKeyEx(rootKey, keyName, 0, mode, keyPtr);
    }

    /*
     * Be sure to close the root key since we are done with it now.
     */

    if (hostName) {
	RegCloseKey(rootKey);
    }
    return result; 
}

/*
 *----------------------------------------------------------------------
 *
 * ParseKeyName --
 *
 *	This function parses a key name into the host, root, and subkey
 *	parts. 
 *
 * Results:
 *	The pointers to the start of the host and subkey names are
 *	returned in the hostNamePtr and keyNamePtr variables.  The
 *	specified root HKEY is returned in rootKeyPtr.  Returns
 *	a standard Tcl result.
 *
 *
 * Side effects:
 *	Modifies the name string by inserting nulls.
 *
 *----------------------------------------------------------------------
 */

static int
ParseKeyName(
    Tcl_Interp *interp,		/* Current interpreter. */
    char *name,
    char **hostNamePtr,
    HKEY *rootKeyPtr,
    char **keyNamePtr)
{
    char *rootName;
    int result, index;
    Tcl_Obj *rootObj, *resultPtr = Tcl_GetObjResult(interp);

    /*
     * Split the key into host and root portions.
     */

    *hostNamePtr = *keyNamePtr = rootName = NULL;
    if (name[0] == '\\') {
	if (name[1] == '\\') {
	    *hostNamePtr = name;
	    for (rootName = name+2; *rootName != '\0'; rootName++) {
		if (*rootName == '\\') {
		    *rootName++ = '\0';
		    break;
		}
	    }
	}
    } else {
	rootName = name;
    }
    if (!rootName) {
	Tcl_AppendStringsToObj(resultPtr, "bad key \"", name,
		"\": must start with a valid root", NULL);
	return TCL_ERROR;
    }

    /*
     * Split the root into root and subkey portions.
     */

    for (*keyNamePtr = rootName; **keyNamePtr != '\0'; (*keyNamePtr)++) {
	if (**keyNamePtr == '\\') {
	    **keyNamePtr = '\0';
	    (*keyNamePtr)++;
	    break;
	}
    }

    /*
     * Look for a matching root name.
     */

    rootObj = Tcl_NewStringObj(rootName, -1);
    result = Tcl_GetIndexFromObj(interp, rootObj, rootKeyNames, "root name",
	    TCL_EXACT, &index);
    Tcl_DecrRefCount(rootObj);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    *rootKeyPtr = rootKeys[index];
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RecursiveDeleteKey --
 *
 *	This function recursively deletes all the keys below a starting
 *	key.  Although Windows 95 does this automatically, we still need
 *	to do this for Windows NT.
 *
 * Results:
 *	Returns a Windows error code.
 *
 * Side effects:
 *	Deletes all of the keys and values below the given key.
 *
 *----------------------------------------------------------------------
 */

static DWORD
RecursiveDeleteKey(
    HKEY startKey,		/* Parent of key to be deleted. */
    char *keyName)		/* Name of key to be deleted. */
{
    DWORD result, subKeyLength;
    Tcl_DString subkey;
    HKEY hKey;

    /*
     * Do not allow NULL or empty key name.
     */

    if (!keyName || lstrlen(keyName) == '\0') {
	return ERROR_BADKEY;
    }

    result = RegOpenKeyEx(startKey, keyName, 0,
	    KEY_ENUMERATE_SUB_KEYS | DELETE | KEY_QUERY_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
	return result;
    }
    result = RegQueryInfoKey(hKey, NULL, NULL, NULL, NULL, &subKeyLength,
	    NULL, NULL, NULL, NULL, NULL, NULL);
    subKeyLength++;
    if (result != ERROR_SUCCESS) {
	return result;
    }

    Tcl_DStringInit(&subkey);
    Tcl_DStringSetLength(&subkey, subKeyLength);

    while (result == ERROR_SUCCESS) {
	/*
	 * Always get index 0 because key deletion changes ordering.
	 */

	subKeyLength = Tcl_DStringLength(&subkey);
	result=RegEnumKeyEx(hKey, 0, Tcl_DStringValue(&subkey), &subKeyLength,
		NULL, NULL, NULL, NULL);
	if (result == ERROR_NO_MORE_ITEMS) {
	    result = RegDeleteKey(startKey, keyName);
	    break;
	} else if (result == ERROR_SUCCESS) {
	    result = RecursiveDeleteKey(hKey, Tcl_DStringValue(&subkey));
	}
    }
    Tcl_DStringFree(&subkey);
    RegCloseKey(hKey);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * SetValue --
 *
 *	This function sets the contents of a registry value.  If
 *	the key or value does not exist, it will be created.  If it
 *	does exist, then the data and type will be replaced.
 *
 * Results:
 *	Returns a normal Tcl result.
 *
 * Side effects:
 *	May create new keys or values.
 *
 *----------------------------------------------------------------------
 */

static int
SetValue(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *keyNameObj,	/* Name of key. */
    Tcl_Obj *valueNameObj,	/* Name of value to set. */
    Tcl_Obj *dataObj,		/* Data to be written. */
    Tcl_Obj *typeObj)		/* Type of data to be written. */
{
    DWORD type, result;
    HKEY key;
    int length;
    char *valueName;
    Tcl_Obj *resultPtr;

    if (typeObj == NULL) {
	type = REG_SZ;
    } else if (Tcl_GetIndexFromObj(interp, typeObj, typeNames, "type",
	    0, (int *) &type) != TCL_OK) {
	if (Tcl_GetIntFromObj(NULL, typeObj, (int*) &type) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_ResetResult(interp);
    }
    if (OpenKey(interp, keyNameObj, KEY_ALL_ACCESS, 1, &key) != TCL_OK) {
	return TCL_ERROR;
    }

    valueName = Tcl_GetStringFromObj(valueNameObj, &length);
    resultPtr = Tcl_GetObjResult(interp);

    if (type == REG_DWORD || type == REG_DWORD_BIG_ENDIAN) {
	DWORD value;
	if (Tcl_GetIntFromObj(interp, dataObj, (int*) &value) != TCL_OK) {
	    RegCloseKey(key);
	    return TCL_ERROR;
	}

	value = ConvertDWORD(type, value);
	result = RegSetValueEx(key, valueName, 0, type, (BYTE*) &value,
		sizeof(DWORD));
    } else if (type == REG_MULTI_SZ) {
	Tcl_DString data;
	int objc, i;
	Tcl_Obj **objv;
	char *element;

	if (Tcl_ListObjGetElements(interp, dataObj, &objc, &objv) != TCL_OK) {
	    RegCloseKey(key);
	    return TCL_ERROR;
	}

	/*
	 * Append the elements as null terminated strings.  Note that
	 * we must not assume the length of the string in case there are
	 * embedded nulls, which aren't allowed in REG_MULTI_SZ values.
	 */

	Tcl_DStringInit(&data);
	for (i = 0; i < objc; i++) {
	    element = Tcl_GetStringFromObj(objv[i], NULL);
	    Tcl_DStringAppend(&data, element, -1);
	    Tcl_DStringSetLength(&data, Tcl_DStringLength(&data)+1);
	}
	result = RegSetValueEx(key, valueName, 0, type,
		(LPBYTE) Tcl_DStringValue(&data),
		(DWORD) (Tcl_DStringLength(&data)+1));
	Tcl_DStringFree(&data);
    } else {
	char *data = Tcl_GetStringFromObj(dataObj, &length);

	/*
	 * Include the null in the length if we are storing a null terminated
	 * string.  Note that we also need to call strlen to find the first
	 * null so we don't pass bad data to the registry.
	 */

	if (type == REG_SZ || type == REG_EXPAND_SZ) {
	    length = strlen(data) + 1;
	}

	result = RegSetValueEx(key, valueName, 0, type, (LPBYTE)data, length);
    }
    RegCloseKey(key);
    if (result != ERROR_SUCCESS) {
	Tcl_AppendToObj(resultPtr, "unable to set value: ", -1);
	AppendSystemError(interp, result);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AppendSystemError --
 *
 *	This routine formats a Windows system error message and places
 *	it into the interpreter result.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
AppendSystemError(
    Tcl_Interp *interp,		/* Current interpreter. */
    DWORD error)		/* Result code from error. */
{
    int length;
    char *msgbuf, id[10];
    Tcl_Obj *resultPtr = Tcl_GetObjResult(interp);

    sprintf(id, "%d", error);
    length = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM
	    | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, error,
	    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&msgbuf,
	    0, NULL);
    if (length == 0) {
	if (error == ERROR_CALL_NOT_IMPLEMENTED) {
	    msgbuf = "function not supported under Win32s";
	} else {
	    msgbuf = id;
	}
    } else {
	/*
	 * Trim the trailing CR/LF from the system message.
	 */
	if (msgbuf[length-1] == '\n') {
	    msgbuf[--length] = 0;
	}
	if (msgbuf[length-1] == '\r') {
	    msgbuf[--length] = 0;
	}
    }
    Tcl_SetErrorCode(interp, "WINDOWS", id, msgbuf, (char *) NULL);
    Tcl_AppendToObj(resultPtr, msgbuf, -1);

    if (length != 0) {
	LocalFree(msgbuf);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConvertDWORD --
 *
 *	This function determines whether a DWORD needs to be byte
 *	swapped, and returns the appropriately swapped value.
 *
 * Results:
 *	Returns a converted DWORD.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static DWORD
ConvertDWORD(
    DWORD type,			/* Either REG_DWORD or REG_DWORD_BIG_ENDIAN */
    DWORD value)		/* The value to be converted. */
{
    DWORD order = 1;
    DWORD localType;

    /*
     * Check to see if the low bit is in the first byte.
     */

    localType = (*((char*)(&order)) == 1) ? REG_DWORD : REG_DWORD_BIG_ENDIAN;
    return (type != localType) ? SWAPLONG(value) : value;
}
