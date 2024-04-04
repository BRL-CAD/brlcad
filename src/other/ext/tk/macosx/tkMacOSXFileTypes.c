/*
There are situations where a graphical user interface needs to know the file
type (i.e. data format) of a file.  The two main ones are when generating an
icon to represent a file, and when filtering the choice of files in a file
open or save dialog.

Early Macintosh systems used OSTypes as identifiers for file types.  An OSType
is a FourCC datatype - four bytes which can be packed into a 32 bit integer.  In
the HFS filesystem they were included in the file metadata.  The metadata also
included another OSType (the Creator Code) which identified the application
which created the file.

In OSX 10.4 the Uniform Type Identifier was introduced as an alternative way to
describe file types.  These are strings (NSStrings, actually) in a reverse DNS
format, such as "com.apple.application-bundle".  Apple provided a tool for
converting OSType codes to Uniform Type Identifiers, which they deprecated in
macOS 12.0 after introducing the UTType class in macOS 11.0.  An instance of the
UTType class has properties which give the Uniform Type Identifier as well as
the preferred file name extension for a given file type.

This module provides tools for working with file types which are meant to abstract
the many variants that Apple has used over the years, and which can be used
without generating deprecation warnings.
*/

#include "tkMacOSXPrivate.h"
#include "tkMacOSXFileTypes.h"
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 110000
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#endif

#define CHARS_TO_OSTYPE(string) (OSType) string[0] << 24 | \
                                (OSType) string[1] << 16 | \
                                (OSType) string[2] <<  8 | \
                                (OSType) string[3]

static BOOL initialized = false;
static Tcl_HashTable ostype2identifier;
static void initOSTypeTable(void) {
    int newPtr;
    Tcl_HashEntry *hPtr;
    const IdentifierForOSType *entry;
    Tcl_InitHashTable(&ostype2identifier, TCL_ONE_WORD_KEYS);
    for (entry = OSTypeDB; entry->ostype != NULL; entry++) {
	const char *key = INT2PTR(CHARS_TO_OSTYPE(entry->ostype));
        hPtr = Tcl_CreateHashEntry(&ostype2identifier, key, &newPtr);
	if (newPtr) {
	    Tcl_SetHashValue(hPtr, entry->identifier);
	}
    }
    initialized = true;
}

MODULE_SCOPE NSString *TkMacOSXOSTypeToUTI(OSType ostype) {
    if (!initialized) {
	initOSTypeTable();
    }
    Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&ostype2identifier, INT2PTR(ostype));
    if (hPtr) {
	char *UTI = Tcl_GetHashValue(hPtr);
	return [[NSString alloc] initWithCString:UTI
					encoding:NSASCIIStringEncoding];
    }
    return nil;
}

/*
 * The NSWorkspace method iconForFileType, which was deprecated in macOS 12.0, would
 * accept an NSString which could be an encoding of an OSType, or a file extension,
 * or a Uniform Type Idenfier.  This function can serve as a replacement.
 */
MODULE_SCOPE NSImage *TkMacOSXIconForFileType(NSString *filetype) {
    if (!initialized) {
	initOSTypeTable();
    }
#if MAC_OS_X_VERSION_MAX_ALLOWED < 110000
// We don't have UTType but iconForFileType is not deprecated, so use it.
    return [[NSWorkspace sharedWorkspace] iconForFileType:filetype];
#else
// We might have UTType but iconForFileType might be deprecated.
    if (@available(macOS 11.0, *)) {
	/* Yes, we do have UTType */
	UTType *uttype = [UTType typeWithIdentifier: filetype];
	if (uttype == nil || !uttype.isDeclared) {
	    uttype = [UTType typeWithFilenameExtension: filetype];
	}
	if (uttype == nil || (!uttype.isDeclared && filetype.length == 4)) {
	    OSType ostype = CHARS_TO_OSTYPE(filetype.UTF8String);
	    NSString *UTI = TkMacOSXOSTypeToUTI(ostype);
	    if (UTI) {
		uttype = [UTType typeWithIdentifier:UTI];
	    }
	}
	if (uttype == nil || !uttype.isDeclared) {
	    return nil;
	}
	return [[NSWorkspace sharedWorkspace] iconForContentType:uttype];
    } else {
	/* No, we don't have UTType. */
 #if MAC_OS_X_VERSION_MIN_REQUIRED < 110000
	/* but iconForFileType is not deprecated, so we can use it. */
    return [[NSWorkspace sharedWorkspace] iconForFileType:filetype];
 #else
    /*
     * Cannot be reached: MIN_REQUIRED >= 110000 yet 11.0 is not available.
     * But the compiler can't figure that out, so it will warn about an
     * execution path with no return value unless we put a return here.
     */
    return nil;
 #endif
    }
#endif
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
