
/*
 * bltWinPrnt.c --
 *
 *	This module implements Win32 printer access.
 *
 * Copyright 1998 by Bell Labs Innovations for Lucent Technologies.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the names
 * of Lucent Technologies any of their entities not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * Lucent Technologies disclaims all warranties with regard to this
 * software, including all implied warranties of merchantability and
 * fitness.  In no event shall Lucent Technologies be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether in
 * an action of contract, negligence or other tortuous action, arising
 * out of or in connection with the use or performance of this
 * software.
 *
 */

#include <bltInt.h>
#include <bltHash.h>
#ifndef NO_PRINTER
#include <X11/Xutil.h>
#undef Status
#if defined(_MSC_VER) || defined(__BORLANDC__)
#include <winspool.h>
#endif /* _MSC_VER || __BORLANDC__ */

/*
  set pid [printer open name]
  printer close $pid
  printer write $pid $data
  printer snap $pid .window
  printer names
  printer enum things
  printer getattr $pid
  printer setattr $pid

  set pid [open ]
  blt::printer open {\\alprint\2a211} p1
  p1 getattr varName
  p1 setattr varName
  p1 write $data
  .graph print p1
  p1 snap .window
  p1 close
  blt::printer names
  blt::printer emum things
*/

#define PRINTER_THREAD_KEY	"BLT Printer Data"

typedef struct {
    Blt_HashTable printerTable;	/* Hash table of printer structures keyed by 
				 * the name of the printer. */
    int nextId;
} PrinterInterpData;

typedef struct {
    int type;
    HDC hDC;
} PrintDrawable;

typedef struct {
    Tcl_Interp *interp;
    Tcl_Command cmdToken;	/* Token for vector's Tcl command. */
    char *name;
    char *fileName;
    PrintDrawable drawable;
    HANDLE hPrinter;
    Blt_HashEntry *hashPtr;
    Blt_HashTable *tablePtr;
    char *driverName;
    char *deviceName;
    char *printerName;
    char *docName;
    char *portName;
    DEVMODE *dmPtr;
    int dmSize;
} PrintQueue;

typedef struct {
    DWORD token;
    char *string;
} TokenString;

static TokenString sizeTable[] =
{
 /* Letter 8 1/2 x 11 in */
    { DMPAPER_LETTER, "Letter" },
 /* Letter Small 8 1/2 x 11 in */
    { DMPAPER_LETTERSMALL, "Letter Small" },
 /* Tabloid 11 x 17 in */
    { DMPAPER_TABLOID, "Tabloid" },
 /* Ledger 17 x 11 in */
    { DMPAPER_LEDGER, "Ledger" },
 /* Legal 8 1/2 x 14 in */
    { DMPAPER_LEGAL, "Legal" },
 /* Statement 5 1/2 x 8 1/2 in */
    { DMPAPER_STATEMENT, "Statement" },
 /* Executive 7 1/4 x 10 1/2 in */
    { DMPAPER_EXECUTIVE, "Executive" },
 /* A3 297 x 420 mm */
    { DMPAPER_A3, "A3" },
 /* A4 210 x 297 mm */
    { DMPAPER_A4, "A4" },
 /* A4 Small 210 x 297 mm */
    { DMPAPER_A4SMALL, "A4 Small" },
 /* A5 148 x 210 mm */
    { DMPAPER_A5, "A5" },
 /* B4 (JIS) 250 x 354 */
    { DMPAPER_B4, "B4 (JIS)" },
 /* B5 (JIS) 182 x 257 mm */
    { DMPAPER_B5, "B5 (JIS)" },
 /* Folio 8 1/2 x 13 in */
    { DMPAPER_FOLIO, "Folio" },
 /* Quarto 215 x 275 mm */
    { DMPAPER_QUARTO, "Quarto" },
 /* 10x14 in */
    { DMPAPER_10X14, "10x14" },
 /* 11x17 in */
    { DMPAPER_11X17, "11x17" },
 /* Note 8 1/2 x 11 in */
    { DMPAPER_NOTE, "Note" },
 /* Envelope #9 3 7/8 x 8 7/8 */
    { DMPAPER_ENV_9, "Envelope #9" },
 /* Envelope #10 4 1/8 x 9 1/2 */
    { DMPAPER_ENV_10, "Envelope #10" },
 /* Envelope #11 4 1/2 x 10 3/8 */
    { DMPAPER_ENV_11, "Envelope #11" },
 /* Envelope #12 4 \276 x 11 */
    { DMPAPER_ENV_12, "Envelope #12" },
 /* Envelope #14 5 x 11 1/2 */
    { DMPAPER_ENV_14, "Envelope #14" },
 /* C size sheet */
    { DMPAPER_CSHEET, "C size sheet" },
 /* D size sheet */
    { DMPAPER_DSHEET, "D size sheet" },
 /* E size sheet */
    { DMPAPER_ESHEET, "E size sheet" },
 /* Envelope DL 110 x 220mm */
    { DMPAPER_ENV_DL, "Envelope DL" },
 /* Envelope C5 162 x 229 mm */
    { DMPAPER_ENV_C5, "Envelope C5" },
 /* Envelope C3  324 x 458 mm */
    { DMPAPER_ENV_C3, "Envelope C3" },
 /* Envelope C4  229 x 324 mm */
    { DMPAPER_ENV_C4, "Envelope C4" },
 /* Envelope C6  114 x 162 mm */
    { DMPAPER_ENV_C6, "Envelope C6" },
 /* Envelope C65 114 x 229 mm */
    { DMPAPER_ENV_C65, "Envelope C65" },
 /* Envelope B4  250 x 353 mm */
    { DMPAPER_ENV_B4, "Envelope B4" },
 /* Envelope B5  176 x 250 mm */
    { DMPAPER_ENV_B5, "Envelope B5" },
 /* Envelope B6  176 x 125 mm */
    { DMPAPER_ENV_B6, "Envelope B6" },
 /* Envelope 110 x 230 mm */
    { DMPAPER_ENV_ITALY, "Envelope Italy" },
 /* Env Monarch 3 7/8 x 7 1/2 in */
    { DMPAPER_ENV_MONARCH, "Envelope Monarch" },
 /* 6 3/4 Envelope 3 5/8 x 6 1/2 in */
    { DMPAPER_ENV_PERSONAL, "6 3/4 Envelope" },
 /* US Std Fanfold 14 7/8 x 11 in */
    { DMPAPER_FANFOLD_US, "US Std Fanfold" },
 /* German Std Fanfold 8 1/2 x 12 in */
    { DMPAPER_FANFOLD_STD_GERMAN, "German Std Fanfold" },
 /* German Legal Fanfold 8 1/2 x 13 in */
    { DMPAPER_FANFOLD_LGL_GERMAN, "German Legal Fanfold" },
 /* B4 (ISO) 250 x 353 mm */
    { DMPAPER_ISO_B4, "ISOB4" },
 /* Japanese Postcard 100 x 148 mm */
    { DMPAPER_JAPANESE_POSTCARD, "Postcard (JIS)" },
 /* 9 x 11 in */
    { DMPAPER_9X11, "9x11" },
 /* 10 x 11 in */
    { DMPAPER_10X11, "10x11" },
 /* 15 x 11 in */
    { DMPAPER_15X11, "15x11" },
 /* Envelope Invite 220 x 220 mm */
    { DMPAPER_ENV_INVITE, "Envelope Invite" },
 /* Letter Extra 9 \275 x 12 in */
    { DMPAPER_LETTER_EXTRA, "Letter Extra" },
 /* Legal Extra 9 \275 x 15 in */
    { DMPAPER_LEGAL_EXTRA, "Legal Extra" },
 /* Tabloid Extra 11.69 x 18 in */
    { DMPAPER_TABLOID_EXTRA, "Tabloid Extra" },
 /* A4 Extra 9.27 x 12.69 in */
    { DMPAPER_A4_EXTRA, "A4 Extra" },
 /* Letter Transverse 8 \275 x 11 in */
    { DMPAPER_LETTER_TRANSVERSE, "Letter Transverse" },
 /* A4 Transverse 210 x 297 mm */
    { DMPAPER_A4_TRANSVERSE, "A4 Transverse" },
 /* Letter Extra Transverse 9\275 x 12 in */
    { DMPAPER_LETTER_EXTRA_TRANSVERSE, "Letter Extra Transverse" },
 /* SuperA/SuperA/A4 227 x 356 mm */
    { DMPAPER_A_PLUS, "Super A Plus" },
 /* SuperB/SuperB/A3 305 x 487 mm */
    { DMPAPER_B_PLUS, "Super B Plus" },
 /* Letter Plus 8.5 x 12.69 in */
    { DMPAPER_LETTER_PLUS, "Letter Plus" },
 /* A4 Plus 210 x 330 mm */
    { DMPAPER_A4_PLUS, "A4 Plus" },
 /* A5 Transverse 148 x 210 mm */
    { DMPAPER_A5_TRANSVERSE, "A5 Transverse" },
 /* B5 (JIS) Transverse 182 x 257 mm */
    { DMPAPER_B5_TRANSVERSE, "B5 Transverse" },
 /* A3 Extra 322 x 445 mm */
    { DMPAPER_A3_EXTRA, "A3 Extra" },
 /* A5 Extra 174 x 235 mm */
    { DMPAPER_A5_EXTRA, "A5 Extra" },
 /* B5 (ISO) Extra 201 x 276 mm */
    { DMPAPER_B5_EXTRA, "B5 Extra" },
 /* A2 420 x 594 mm */
    { DMPAPER_A2, "A2" },
 /* A3 Transverse 297 x 420 mm */
    { DMPAPER_A3_TRANSVERSE, "A3 Transverse" },
 /* A3 Extra Transverse 322 x 445 mm   */
    { DMPAPER_A3_EXTRA_TRANSVERSE, "A3 Extra Transverse" },
    { 0, NULL }
};

static TokenString statusTable[] =
{
    { PRINTER_STATUS_BUSY, "Busy" },
    { PRINTER_STATUS_DOOR_OPEN, "Door Open" },
    { PRINTER_STATUS_ERROR, "Error" },
    { PRINTER_STATUS_INITIALIZING, "Initializing" },
    { PRINTER_STATUS_IO_ACTIVE, "IO Active" },
    { PRINTER_STATUS_MANUAL_FEED, "Manual Feed" },
    { PRINTER_STATUS_NOT_AVAILABLE, "Not Available" },
    { PRINTER_STATUS_NO_TONER, "No Toner" },
    { PRINTER_STATUS_OFFLINE, "Offline" },
    { PRINTER_STATUS_OUTPUT_BIN_FULL, "Bin Full" },
    { PRINTER_STATUS_OUT_OF_MEMORY, "Out Of Memory" },
    { PRINTER_STATUS_PAGE_PUNT, "Page Punt" },
    { PRINTER_STATUS_PAPER_JAM, "Paper Jam" },
    { PRINTER_STATUS_PAPER_OUT, "Paper Out" },
    { PRINTER_STATUS_PAPER_PROBLEM, "Paper Problem" },
    { PRINTER_STATUS_PAUSED, "Paused" },
    { PRINTER_STATUS_PENDING_DELETION, "Pending Deletion" },
    { PRINTER_STATUS_POWER_SAVE, "Power Save" },
    { PRINTER_STATUS_PRINTING, "Printing" },
    { PRINTER_STATUS_PROCESSING, "Processing" },
    { PRINTER_STATUS_SERVER_UNKNOWN, "Server Unknown" },
    { PRINTER_STATUS_TONER_LOW, "Toner Low" },
    { PRINTER_STATUS_USER_INTERVENTION, "User Intervention" },
    { PRINTER_STATUS_WAITING, "Waiting" },
    { PRINTER_STATUS_WARMING_UP, "Warming Up" },
    { 0, NULL }
};

static TokenString attributeTable[] =
{
    { PRINTER_ATTRIBUTE_DEFAULT, "Default" },
    { PRINTER_ATTRIBUTE_DIRECT, "Direct" },
    { PRINTER_ATTRIBUTE_DO_COMPLETE_FIRST, "Do Complete First" },
    { PRINTER_ATTRIBUTE_ENABLE_BIDI, "Enable BIDI" },
    { PRINTER_ATTRIBUTE_ENABLE_DEVQ, "Enable Devq" },
    { PRINTER_ATTRIBUTE_HIDDEN, "Hidden" },
    { PRINTER_ATTRIBUTE_KEEPPRINTEDJOBS, "Keep Printed Jobs" },
    { PRINTER_ATTRIBUTE_LOCAL, "Local" },
    { PRINTER_ATTRIBUTE_NETWORK, "Network" },
    { PRINTER_ATTRIBUTE_QUEUED, "Queued" },
    { PRINTER_ATTRIBUTE_RAW_ONLY, "Raw Only" },
    { PRINTER_ATTRIBUTE_SHARED, "Shared" },
    { PRINTER_ATTRIBUTE_WORK_OFFLINE, "Offline" },
    { 0, NULL }
};

static TokenString binTable[] =
{
    { DMBIN_UPPER, "Upper" },
    { DMBIN_LOWER, "Lower" },
    { DMBIN_MIDDLE, "Middle" },
    { DMBIN_MANUAL, "Manual" },
    { DMBIN_ENVELOPE, "Envelope" },
    { DMBIN_ENVMANUAL, "Envelope Manual" },
    { DMBIN_AUTO, "Automatic" },
    { DMBIN_TRACTOR, "Tractor" },
    { DMBIN_SMALLFMT, "Small Format" },
    { DMBIN_LARGEFMT, "Large Format" },
    { DMBIN_LARGECAPACITY, "Large Capacity" },
    { DMBIN_CASSETTE, "Cassette" },
    { DMBIN_FORMSOURCE, "Form Source" },
    { 0, NULL }
};

static TokenString orientationTable[] =
{
    { DMORIENT_PORTRAIT, "Portrait" },
    { DMORIENT_LANDSCAPE, "Landscape" },
    { 0, NULL }
};

static TokenString qualityTable[] =
{
    { DMRES_HIGH, "High" },
    { DMRES_MEDIUM, "Medium" },
    { DMRES_LOW, "Low" },
    { DMRES_DRAFT, "Draft" },
    { 0, NULL }
};

static TokenString colorTable[] =
{
    { DMCOLOR_COLOR, "Color" },
    { DMCOLOR_MONOCHROME, "Monochrome" },
    { 0, NULL }
};

static TokenString duplexTable[] =
{
    { DMDUP_SIMPLEX, "Simplex" },
    { DMDUP_HORIZONTAL, "Horizontal" },
    { DMDUP_VERTICAL, "Vertical" },
    { 0, NULL }
};

static TokenString ttOptionTable[] =
{
    { DMTT_BITMAP, "Bitmap" },
    { DMTT_DOWNLOAD, "Download" },
    { DMTT_SUBDEV, "Substitute Device" },
    { DMTT_DOWNLOAD_OUTLINE, "Download Outline" },
    { 0, NULL }
};

static Tcl_ObjCmdProc PrinterCmd;
static Tcl_InterpDeleteProc PrinterInterpDeleteProc;

void
Blt_GetPrinterScale(HDC printerDC, double *xRatioPtr, double *yRatioPtr)
{
    double xScreen, yScreen;
    double xPrinter, yPrinter;
    HDC screenDC;

    xPrinter = (double)GetDeviceCaps(printerDC, LOGPIXELSX);
    yPrinter = (double)GetDeviceCaps(printerDC, LOGPIXELSY);
    screenDC = GetDC(NULL);
    xScreen = (double)GetDeviceCaps(screenDC, LOGPIXELSX);
    yScreen = (double)GetDeviceCaps(screenDC, LOGPIXELSY);
    ReleaseDC(NULL, screenDC);
    *xRatioPtr = (xPrinter / xScreen);
    *yRatioPtr = (yPrinter / yScreen);
}

static PrinterInterpData *
GetPrinterInterpData(Tcl_Interp *interp)
{
    PrinterInterpData *dataPtr;
    Tcl_InterpDeleteProc *proc;

    dataPtr = (PrinterInterpData *)
	Tcl_GetAssocData(interp, PRINTER_THREAD_KEY, &proc);
    if (dataPtr == NULL) {
	dataPtr = Blt_Malloc(sizeof(PrinterInterpData));
	dataPtr->nextId = 0;
	assert(dataPtr);
	Tcl_SetAssocData(interp, PRINTER_THREAD_KEY, PrinterInterpDeleteProc,
		dataPtr);
	Blt_InitHashTable(&dataPtr->printerTable, BLT_STRING_KEYS);
    }
    return dataPtr;
}

static int
GetQueue(
    Tcl_Interp *interp,
    const char *name,
    PrintQueue **queuePtrPtr)
{
    Blt_HashEntry *hPtr;
    PrinterInterpData *dataPtr;

    dataPtr = GetPrinterInterpData(interp);
    hPtr = Blt_FindHashEntry(&dataPtr->printerTable, name);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "can't find printer \"", name, "\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    *queuePtrPtr = (PrintQueue *)Blt_GetHashValue(hPtr);
    return TCL_OK;
}

static int
GetQueueFromObj(
    Tcl_Interp *interp,
    Tcl_Obj *objPtr,
    PrintQueue **queuePtrPtr)
{
    return GetQueue(interp, Tcl_GetString(objPtr), queuePtrPtr);
}

static void
CloseQueue(
    PrintQueue *queuePtr)
{
    ClosePrinter(queuePtr->hPrinter);
    queuePtr->hPrinter = NULL;
}

static int
OpenQueue(
    Tcl_Interp *interp,
    PrintQueue *queuePtr)
{
    PRINTER_DEFAULTS pd;
    HANDLE hPrinter;

    ZeroMemory(&pd, sizeof(pd));
    pd.DesiredAccess = PRINTER_ALL_ACCESS;
    if (!OpenPrinter(queuePtr->printerName, &hPrinter, &pd)) {
        Tcl_AppendResult(interp, "can't open printer \"", 
		queuePtr->printerName, "\": ", Blt_LastError(), (char *)NULL);
	queuePtr->hPrinter = NULL;
	return TCL_ERROR;
    }
    queuePtr->hPrinter = hPrinter;
    return TCL_OK;
}

static HGLOBAL
GetQueueProperties(
    PrintQueue *queuePtr,
    DEVMODE **dmPtrPtr)
{
    HWND hWnd;
    unsigned int dmSize;
    HGLOBAL hMem;
    DEVMODE *dmPtr;

    hWnd = GetDesktopWindow();
    dmSize = DocumentProperties(hWnd, queuePtr->hPrinter, 
	queuePtr->printerName, NULL, NULL, 0);
    if (dmSize == 0) {
	Tcl_AppendResult(queuePtr->interp,
		"can't get document properties for \"", 
		queuePtr->printerName,
		"\": ", Blt_LastError(), (char *)NULL);
	return NULL;
    }
    hMem = GlobalAlloc(GHND, dmSize);
    dmPtr = (DEVMODE *)GlobalLock(hMem);
    if (!DocumentProperties(hWnd, queuePtr->hPrinter, queuePtr->printerName, 
	dmPtr, NULL, DM_OUT_BUFFER)) {
	Tcl_AppendResult(queuePtr->interp,
		"can't allocate document properties for \"",
		queuePtr->printerName, "\": ", Blt_LastError(), 
		(char *)NULL);
	GlobalUnlock(hMem);
	GlobalFree(hMem);
	return NULL;
    }
    *dmPtrPtr = dmPtr;
    queuePtr->dmSize = dmSize;
    return hMem;
}

static int
SetQueueProperties(
    Tcl_Interp *interp, 
    PrintQueue *queuePtr,
    DEVMODE *dmPtr)
{
    HWND hWnd;
    int result;

    hWnd = GetDesktopWindow();
    result = DocumentProperties(hWnd, queuePtr->hPrinter, 
	queuePtr->printerName, dmPtr, dmPtr, DM_IN_BUFFER | DM_OUT_BUFFER);
    if (result == 0) {
	Tcl_AppendResult(interp, "can't set document properties for \"", 
	    queuePtr->printerName, "\": ", Blt_LastError(), (char *)NULL);
	return TCL_ERROR;
    } 
    if (queuePtr->dmPtr != NULL) {
	Blt_Free(queuePtr->dmPtr);
    }
    queuePtr->dmPtr = Blt_Malloc(queuePtr->dmSize);
    *queuePtr->dmPtr = *dmPtr;
    return TCL_OK;
}

static void
DestroyQueue(PrintQueue *queuePtr)
{
    if (queuePtr->drawable.hDC != NULL) {
	DeleteDC(queuePtr->drawable.hDC);
    }
    if (queuePtr->printerName != NULL) {
	Blt_Free(queuePtr->printerName);
    }
    if (queuePtr->deviceName != NULL) {
	Blt_Free(queuePtr->deviceName);
    }
    if (queuePtr->portName != NULL) {
	Blt_Free(queuePtr->portName);
    }
    if (queuePtr->driverName != NULL) {
	Blt_Free(queuePtr->driverName);
    }
    if (queuePtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(queuePtr->tablePtr, queuePtr->hashPtr);
    }
    if (queuePtr->dmPtr != NULL) {
	Blt_Free(queuePtr->dmPtr);
    }
    Blt_Free(queuePtr);
}

static char *
AttributesToString(DWORD attributes, Tcl_DString * resultPtr)
{
    register TokenString *p;

    Tcl_DStringInit(resultPtr);
    for (p = attributeTable; p->string != NULL; p++) {
	if (attributes & p->token) {
	    Tcl_DStringAppendElement(resultPtr, p->string);
	}
    }
    return Tcl_DStringValue(resultPtr);
}

static char *
StatusToString(DWORD status, Tcl_DString * resultPtr)
{
    register TokenString *p;

    Tcl_DStringInit(resultPtr);
    for (p = statusTable; p->string != NULL; p++) {
	if (status & p->token) {
	    Tcl_DStringAppendElement(resultPtr, p->string);
	}
    }
    return Tcl_DStringValue(resultPtr);
}

static char *
TokenToString(TokenString *table, DWORD token)
{
    register TokenString *p;

    for (p = table; p->string != NULL; p++) {
	if (token == p->token) {
	    return p->string;
	}
    }
    return "???";
}

static DWORD
StringToToken(TokenString * table, char *string)
{
    register TokenString *p;
    char c;

    c = toupper(string[0]);
    for (p = table; p->string != NULL; p++) {
	if ((c == toupper(p->string[0])) &&
	    (strcasecmp(string, p->string) == 0)) {
	    return p->token;
	}
    }
    return 0;
}

static void
GetFormInfo(
    Tcl_Interp *interp,
    FORM_INFO_1 * infoArr,
    int nForms,
    char *varName)
{
    Tcl_DString dString;
    register int i;

    Tcl_DStringInit(&dString);
    for (i = 0; i < nForms; i++) {
	Tcl_DStringAppendElement(&dString, infoArr[i].pName);
    }
    Tcl_SetVar2(interp, varName, "EnumForms", Tcl_DStringValue(&dString),
	TCL_LEAVE_ERR_MSG);
    Tcl_DStringFree(&dString);
}


static int
GetPrinterAttributes(
    Tcl_Interp *interp,		/* Interpreter context. */
    PrintQueue *queuePtr,
    Tcl_Obj *objPtr)		/* Name of array variable to contain
				 * printer device information. */
{	
    char *string;
    Tcl_DString dString;
    DEVMODE *dmPtr;
    DWORD bytesNeeded;
    HGLOBAL hMem1, hMem2;
    PRINTER_INFO_2* pi2Ptr;
    LPVOID buffer;
    int result = TCL_ERROR;
    char *varName;

    if (OpenQueue(interp, queuePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_DStringInit(&dString);
    hMem2 = NULL;

    GetPrinter(queuePtr->hPrinter, 2, NULL, 0, &bytesNeeded);

    /* Windows 95/98 seems to only want locked memory. Allocating
     * unlocked memory will sometimes crash the printer driver and
     * therefore Windows itself.  */

    hMem1 = GlobalAlloc(GHND, bytesNeeded);
    if (hMem1 == NULL) {
        Tcl_AppendResult(interp, "can't allocate memory for printer \"", 
		queuePtr->name, "\": ", Blt_LastError(), (char *)NULL);
	goto error;
    }
    buffer = (LPVOID)GlobalLock(hMem1);
    if (!GetPrinter(queuePtr->hPrinter, 2, buffer, bytesNeeded, 
	&bytesNeeded)) {
        Tcl_AppendResult(interp, "can't get printer \"", queuePtr->name, "\": ",
	    Blt_LastError(), (char *)NULL);
	goto error;
    }
    hMem2 = GetQueueProperties(queuePtr, &dmPtr);
    if (hMem2 == NULL) {
        Tcl_AppendResult(interp, "can't allocate memory for printer \"", 
		queuePtr->name, "\" properties: ", Blt_LastError(), (char *)NULL);
	goto error;
    }
    pi2Ptr = (PRINTER_INFO_2 *)buffer;
    varName = Tcl_GetString(objPtr);
    Tcl_SetVar2(interp, varName, "ServerName", pi2Ptr->pServerName, 0);
    Tcl_SetVar2(interp, varName, "PrinterName", pi2Ptr->pPrinterName, 0);
    Tcl_SetVar2(interp, varName, "PortName", pi2Ptr->pPortName, 0);
    Tcl_SetVar2(interp, varName, "DriverName", pi2Ptr->pDriverName, 0);
    Tcl_SetVar2(interp, varName, "Comment", pi2Ptr->pComment, 0);
    Tcl_SetVar2(interp, varName, "Location", pi2Ptr->pLocation, 0);
    Tcl_SetVar2(interp, varName, "SepFile", pi2Ptr->pSepFile, 0);
    Tcl_SetVar2(interp, varName, "PrintProcessor", pi2Ptr->pPrintProcessor, 0);
    Tcl_SetVar2(interp, varName, "Datatype", pi2Ptr->pDatatype, 0);
    Tcl_SetVar2(interp, varName, "Parameters", pi2Ptr->pParameters, 0);
    Tcl_SetVar2(interp, varName, "Attributes",
		AttributesToString(pi2Ptr->Attributes, &dString), 0);
    Tcl_SetVar2(interp, varName, "Priority", Blt_Itoa(pi2Ptr->Priority), 0);
    Tcl_SetVar2(interp, varName, "DefaultPriority",
		Blt_Itoa(pi2Ptr->DefaultPriority), 0);
    Tcl_SetVar2(interp, varName, "StartTime", Blt_Itoa(pi2Ptr->StartTime), 0);
    Tcl_SetVar2(interp, varName, "UntilTime", Blt_Itoa(pi2Ptr->UntilTime), 0);
    Tcl_SetVar2(interp, varName, "Status",
		StatusToString(pi2Ptr->Status, &dString), 0);
    Tcl_SetVar2(interp, varName, "Jobs", Blt_Itoa(pi2Ptr->cJobs), 0);
    Tcl_SetVar2(interp, varName, "AveragePPM", Blt_Itoa(pi2Ptr->AveragePPM), 0);

    if (dmPtr->dmFields & DM_ORIENTATION) {
	Tcl_SetVar2(interp, varName, "Orientation",
	    TokenToString(orientationTable, dmPtr->dmOrientation), 0);
    }
    if (dmPtr->dmFields & DM_PAPERSIZE) {
	Tcl_SetVar2(interp, varName, "PaperSize",
	    TokenToString(sizeTable, dmPtr->dmPaperSize), 0);
    }
    if (dmPtr->dmFields & DM_PAPERWIDTH) {
	Tcl_SetVar2(interp, varName, "PaperWidth",
	    Blt_Itoa(dmPtr->dmPaperWidth), 0);
    }
    if (dmPtr->dmFields & DM_PAPERLENGTH) {
	Tcl_SetVar2(interp, varName, "PaperLength",
	    Blt_Itoa(dmPtr->dmPaperLength), 0);
    }
    if (dmPtr->dmFields & DM_SCALE) {
	Tcl_SetVar2(interp, varName, "Scale", Blt_Itoa(dmPtr->dmScale), 0);
    }
    if (dmPtr->dmFields & DM_COPIES) {
	Tcl_SetVar2(interp, varName, "Copies", Blt_Itoa(dmPtr->dmCopies), 0);
    }
    if (dmPtr->dmFields & DM_DEFAULTSOURCE) {
	Tcl_SetVar2(interp, varName, "DefaultSource",
	    TokenToString(binTable, dmPtr->dmDefaultSource), 0);
    }
    if (dmPtr->dmFields & DM_PRINTQUALITY) {
	if (dmPtr->dmPrintQuality < 0) {
	    string = TokenToString(qualityTable, dmPtr->dmPrintQuality);
	} else {
	    string = Blt_Itoa(dmPtr->dmPrintQuality);
	}
	Tcl_SetVar2(interp, varName, "PrintQuality", string, 0);
    }
    if (dmPtr->dmFields & DM_COLOR) {
	Tcl_SetVar2(interp, varName, "Color",
	    TokenToString(colorTable, dmPtr->dmColor), 0);
    }
    if (dmPtr->dmFields & DM_DUPLEX) {
	Tcl_SetVar2(interp, varName, "Duplex",
	    TokenToString(duplexTable, dmPtr->dmDuplex), 0);
    }
    if (dmPtr->dmFields & DM_YRESOLUTION) {
	Tcl_SetVar2(interp, varName, "YResolution",
	    Blt_Itoa(dmPtr->dmYResolution), 0);
    }
    if (dmPtr->dmFields & DM_TTOPTION) {
	Tcl_SetVar2(interp, varName, "TTOption",
	    TokenToString(ttOptionTable, dmPtr->dmTTOption), 0);
    }
    if (dmPtr->dmFields & DM_COLLATE) {
	if (dmPtr->dmCollate == DMCOLLATE_TRUE) {
	    string = "true";
	} else if (dmPtr->dmCollate == DMCOLLATE_FALSE) {
	    string = "false";
	} else {
	    string = "???";
	}
	Tcl_SetVar2(interp, varName, "Collate", string, 0);
    }
    if (dmPtr->dmFields & DM_FORMNAME) {
	Tcl_SetVar2(interp, varName, "FormName", dmPtr->dmFormName, 0);
    }
    Tcl_SetVar2(interp, varName, "OutputFile", dmPtr->dmDeviceName, 0);
    result = TCL_OK;

 error:
    Tcl_DStringFree(&dString);
    CloseQueue(queuePtr);
    if (hMem1 != NULL) {
	GlobalUnlock(hMem1);
	GlobalFree(hMem1);
    }
    if (hMem2 != NULL) {
	GlobalUnlock(hMem2);
	GlobalFree(hMem2);
    }
    return result;
}

static int
SetQueueAttributes(
    Tcl_Interp *interp,
    PrintQueue *queuePtr,
    Tcl_Obj *objPtr)
{
    char *string;
    DEVMODE *dmPtr;
    int value;
    HGLOBAL hMem;
    int result;
    char *varName;

    if (OpenQueue(interp, queuePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    hMem = GetQueueProperties(queuePtr, &dmPtr);
    CloseQueue(queuePtr);
    if (hMem == NULL) {
       return TCL_ERROR;
    }
    dmPtr->dmFields = 0;
    varName = Tcl_GetString(objPtr);
    string = (char *)Tcl_GetVar2(interp, varName, "Orientation", 0);
    if (string != NULL) {
	value = StringToToken(orientationTable, string);
	if (value > 0) {
	    dmPtr->dmFields |= DM_ORIENTATION;
	    dmPtr->dmOrientation = value;
	}
    }
    string = (char *)Tcl_GetVar2(interp, varName, "PaperSize", 0);
    if (string != NULL) {
	value = StringToToken(sizeTable, string);
	if (value > 0) {
	    dmPtr->dmFields |= DM_PAPERSIZE;
	    dmPtr->dmPaperSize = value;
	}
    }
    string = (char *)Tcl_GetVar2(interp, varName, "PaperWidth", 0);
    if (string != NULL) {
	if (Tcl_GetInt(interp, string, &value) == TCL_OK) {
	    dmPtr->dmFields |= DM_PAPERWIDTH;
	    dmPtr->dmPaperWidth = value;
	}
    }
    string = (char *)Tcl_GetVar2(interp, varName, "PaperLength", 0);
    if (string != NULL) {
	if (Tcl_GetInt(interp, string, &value) == TCL_OK) {
	    dmPtr->dmFields |= DM_PAPERLENGTH;
	    dmPtr->dmPaperLength = value;
	}
    }
    string = (char *)Tcl_GetVar2(interp, varName, "Scale", 0);
    if (string != NULL) {
	if (Tcl_GetInt(interp, string, &value) == TCL_OK) {
	    dmPtr->dmFields |= DM_SCALE;
	    dmPtr->dmScale = value;
	}
    }
    string = (char *)Tcl_GetVar2(interp, varName, "Copies", 0);
    if (string != NULL) {
	if (Tcl_GetInt(interp, string, &value) == TCL_OK) {
	    dmPtr->dmFields |= DM_COPIES;
	    dmPtr->dmCopies = value;
	}
    }
    string = (char *)Tcl_GetVar2(interp, varName, "DefaultSource", 0);
    if (string != NULL) {
	value = StringToToken(binTable, string);
	if (value > 0) {
	    dmPtr->dmFields |= DM_DEFAULTSOURCE;
	    dmPtr->dmDefaultSource = value;
	}
    }
    string = (char *)Tcl_GetVar2(interp, varName, "PrintQuality", 0);
    if (string != NULL) {
	value = StringToToken(qualityTable, string);
	if (value > 0) {
	    dmPtr->dmFields |= DM_PRINTQUALITY;
	    dmPtr->dmPrintQuality = value;
	}
    }
    string = (char *)Tcl_GetVar2(interp, varName, "Color", 0);
    if (string != NULL) {
	value = StringToToken(colorTable, string);
	if (value > 0) {
	    dmPtr->dmFields |= DM_COLOR;
	    dmPtr->dmColor = value;
	}
    }
    string = (char *)Tcl_GetVar2(interp, varName, "Duplex", 0);
    if (string != NULL) {
	value = StringToToken(duplexTable, string);
	if (value > 0) {
	    dmPtr->dmFields |= DM_DUPLEX;
	    dmPtr->dmDuplex = value;
	}
    }
    string = (char *)Tcl_GetVar2(interp, varName, "YResolution", 0);
    if (string != NULL) {
	if (Tcl_GetInt(interp, string, &value) == TCL_OK) {
	    dmPtr->dmFields |= DM_YRESOLUTION;
	    dmPtr->dmYResolution = value;
	}
    }
    string = (char *)Tcl_GetVar2(interp, varName, "TTOption", 0);
    if (string != NULL) {
	value = StringToToken(ttOptionTable, string);
	if (value > 0) {
	    dmPtr->dmFields |= DM_TTOPTION;
	    dmPtr->dmTTOption = value;
	}
    }
    string = (char *)Tcl_GetVar2(interp, varName, "Collate", 0);
    if (string != NULL) {
	if (Tcl_GetBoolean(interp, string, &value) == TCL_OK) {
	    dmPtr->dmFields |= DM_COLLATE;
	    dmPtr->dmCollate = value;
	}
    }
    string = (char *)Tcl_GetVar2(interp, varName, "OutputFile", 0);
    if (string != NULL) {
	if (queuePtr->fileName != NULL) {
	    Blt_Free(queuePtr->fileName);
	}
	queuePtr->fileName = Blt_Strdup(string);
    }
    if (queuePtr->dmPtr != NULL) {
	Blt_Free(queuePtr->dmPtr);
    }
    string = (char *)Tcl_GetVar2(interp, varName, "DocumentName", 0);
    if (string != NULL) {
	if (queuePtr->docName != NULL) {
	    Blt_Free(queuePtr->docName);
	}
	queuePtr->docName = Blt_Strdup(string);
    }
    result = SetQueueProperties(interp, queuePtr, dmPtr);
    GlobalUnlock(hMem);
    GlobalFree(hMem);
    CloseQueue(queuePtr);
    return result;
}

/*ARGSUSED*/
static int
EnumOp(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    TokenString *p;
    char c;
    unsigned int length;
    char *attr;

    attr = Tcl_GetStringFromObj(objv[2], &length);
    c = attr[0];
    if ((c == 'p') && (strncmp(attr, "paper", length) == 0)) {
	p = sizeTable;
    } else if ((c == 'q') && (strncmp(attr, "quality", length) == 0)) {
	p = qualityTable;
    } else if ((c == 'b') && (strncmp(attr, "bin", length) == 0)) {
	p = binTable;
    } else if ((c == 'o') && (strncmp(attr, "orientation", length) == 0)) {
	p = orientationTable;
    } else if ((c == 'c') && (strncmp(attr, "color", length) == 0)) {
	p = colorTable;
    } else if ((c == 'd') && (strncmp(attr, "duplex", length) == 0)) {
	p = duplexTable;
    } else if ((c == 't') && (strncmp(attr, "ttoption", length) == 0)) {
	p = ttOptionTable;
    } else {
	Tcl_AppendResult(interp, "bad enumeration field \"", attr, 
"\": should be \"paper\", \"quality\", \"bin\", \"orientation\", \"color\", \"duplex\", or \"ttoption\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    for ( /*empty*/ ; p->string != NULL; p++) {
	Tcl_AppendElement(interp, p->string);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
OpenOp(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    PrinterInterpData *dataPtr = clientData;
    PrintQueue *queuePtr;
    LPVOID buffer;
    PRINTER_INFO_2* pi2Ptr;
    DWORD bytesNeeded;
    int isNew;
    Blt_HashEntry *hPtr;
    HANDLE hMem;
    char *name;

    name = Tcl_GetString(objv[2]);
    hPtr = Blt_CreateHashEntry(&dataPtr->printerTable, name, &isNew);
    if (isNew) {
	queuePtr = Blt_Calloc(1, sizeof(PrintQueue));
	queuePtr->name = Blt_GetHashKey(&dataPtr->printerTable, hPtr);
	queuePtr->interp = interp;
	Tcl_SetResult(interp, name, TCL_VOLATILE);
	Blt_SetHashValue(hPtr, queuePtr);
	queuePtr->hashPtr = hPtr;
	queuePtr->tablePtr = &dataPtr->printerTable;
	queuePtr->printerName = Blt_Strdup(name);
    } else {
	Tcl_AppendResult(interp, "printer \"", name, "\" is already open",
			 (char *)NULL);
	return TCL_ERROR;
    }
    if (OpenQueue(interp, queuePtr) != TCL_OK) {
	DestroyQueue(queuePtr);
	return TCL_ERROR;
    }
    /* Call the first time to determine the amount of memory needed. */
    GetPrinter(queuePtr->hPrinter, 2, NULL, 0, &bytesNeeded);
    if ((bytesNeeded == 0) || (GetLastError() != ERROR_INSUFFICIENT_BUFFER)) {
	Tcl_AppendResult(interp, "can't get size of attribute buffer for \"",
			 name, "\": ", Blt_LastError(), (char *)NULL);
	return TCL_ERROR;
    }
    /* Allocate a buffer to contain all printer information. */
    hMem = GlobalAlloc(GHND, bytesNeeded);
    if (hMem == NULL) {
	return TCL_ERROR;
    }
    buffer = (LPVOID)GlobalLock(hMem);

    /* And call the again to actually get the printer. */
    if (!GetPrinter(queuePtr->hPrinter, 2, buffer, bytesNeeded, 
		    &bytesNeeded)) {
	Tcl_AppendResult(interp, "can't get printer attributes for \"",
	    name, "\": ", Blt_LastError(), (char *)NULL);
        GlobalUnlock(hMem);
        GlobalFree(hMem);
	return TCL_ERROR;
    }
    pi2Ptr = (PRINTER_INFO_2 *)buffer;
    if (pi2Ptr->pDevMode != NULL) {
	queuePtr->deviceName = Blt_Strdup(pi2Ptr->pDevMode->dmDeviceName);
    }
    queuePtr->driverName = Blt_Strdup(pi2Ptr->pDriverName);
    /*
    queuePtr->printerName = Blt_Strdup(pi2Ptr->pPrinterName);
    */
    queuePtr->portName = Blt_Strdup(pi2Ptr->pPortName);
    GlobalUnlock(hMem);
    GlobalFree(hMem);
    return TCL_OK;
}

/*ARGSUSED*/
static int
NamesOp(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)	/* Not used. */
{
    DWORD nPrinters, bytesNeeded;
    int elemSize, level;
    unsigned char *buffer;
    int result, flags;
    HANDLE hMem;

    if (Blt_GetPlatformId() == VER_PLATFORM_WIN32_NT) {
	level = 4;
	elemSize = sizeof(PRINTER_INFO_4);
	flags = PRINTER_ENUM_NAME;
    } else {
	level = 5;
	elemSize = sizeof(PRINTER_INFO_5);
	flags = PRINTER_ENUM_LOCAL;
    }
    result = EnumPrinters(
	flags,			/* Flags */
	NULL,			/* Printer name */
	level,			/* Information level: 1, 2, 4, or 5 */
	NULL,			/* Array of returned information */
	0,			/* Size of array */
	&bytesNeeded,		/* Size needed for array */
	&nPrinters);		/* Number of structures returned */

    if ((!result) && (GetLastError() != ERROR_INSUFFICIENT_BUFFER)) {
	Tcl_AppendResult(interp, "can't enumerate printers (memory alloc): ",
	    Blt_LastError(), (char *)NULL);
	return TCL_ERROR;
    }
    hMem = GlobalAlloc(GHND, bytesNeeded);
    buffer = (unsigned char *)GlobalLock(hMem);

    result = EnumPrinters(
	flags,			/* Flags */
	NULL,			/* Printer name */
	level,			/* Information level: 1, 2, 4, or 5 */
	buffer,			/* Array of returned information */
	bytesNeeded,		/* Size of array */
	&bytesNeeded,		/* Size needed for array */
	&nPrinters);		/* Number of structures returned */

    if (!result) {
	Tcl_AppendResult(interp, "can't enumerate printers: ",
	    Blt_LastError(), (char *)NULL);
	return TCL_ERROR;
    }
    if (objc > 2) {	
	register unsigned int i;
	char *pattern;
	char *p;

	p = buffer;
        pattern = Tcl_GetString(objv[2]);
	for (i = 0; i < nPrinters; i++) {
	    if (Tcl_StringMatch(p, pattern)) {
		Tcl_AppendElement(interp, *(char **)p);
	    }
	    p += elemSize;
	}
    } else {
	register unsigned int i;
	char *p;

	p = buffer;
	for (i = 0; i < nPrinters; i++) {
	    Tcl_AppendElement(interp, *(char **)p);
	    p += elemSize;
	}
    }
    GlobalUnlock(hMem);
    GlobalFree(hMem);
    return TCL_OK;
}

/*ARGSUSED*/
static int
CloseOp(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    PrintQueue *queuePtr;

    if (GetQueueFromObj(interp, objv[2], &queuePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    DestroyQueue(queuePtr);
    return TCL_OK;
}

/*ARGSUSED*/
static int
GetAttrOp(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    PrintQueue *queuePtr;

    if (GetQueueFromObj(interp, objv[2], &queuePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return GetPrinterAttributes(interp, queuePtr, objv[3]);
}

/*ARGSUSED*/
static int
SetAttrOp(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    PrintQueue *queuePtr;

    if (GetQueueFromObj(interp, objv[2], &queuePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    return SetQueueAttributes(interp, queuePtr, objv[3]);
}

/*
 * --------------------------------------------------------------------------
 *
 * SnapOp --
 *
 *	Prints a snapshot of a Tk_Window to the designated printer.
 *
 * Results:
 *	Returns a standard Tcl result.  If an error occurred
 *	TCL_ERROR is returned and interp->result will contain an
 *	error message.
 *
 * -------------------------------------------------------------------------
 */
static int
SnapOp(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    BITMAPINFO bi;
    DIBSECTION ds;
    HBITMAP hBitmap;
    HPALETTE hPalette;
    HDC hDC, printDC, memDC;
    void *data;
    Tk_Window tkwin;
    TkWinDCState state;
    int result;
    PrintQueue *queuePtr;
    DOCINFO di;
    double pageWidth, pageHeight;
    int jobId;
    char *driverName;
    DEVMODE *dmPtr;
    HGLOBAL hMem;
    Tcl_DString dString;
    char *path;

    Tcl_DStringInit(&dString);
    if (GetQueueFromObj(interp, objv[2], &queuePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    path = Tcl_GetString(objv[3]);
    tkwin = Tk_NameToWindow(interp, path, Tk_MainWindow(interp));
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    if (Tk_WindowId(tkwin) == None) {
	Tk_MakeWindowExist(tkwin);
    }
    
    result = TCL_ERROR;
    hDC = TkWinGetDrawableDC(Tk_Display(tkwin), Tk_WindowId(tkwin), &state);

    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = Tk_Width(tkwin);
    bi.bmiHeader.biHeight = Tk_Height(tkwin);
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    hBitmap = CreateDIBSection(hDC, &bi, DIB_RGB_COLORS, &data, NULL, 0);
    memDC = CreateCompatibleDC(hDC);
    SelectBitmap(memDC, hBitmap);
    hPalette = Blt_GetSystemPalette();
    if (hPalette != NULL) {
	SelectPalette(hDC, hPalette, FALSE);
	RealizePalette(hDC);
	SelectPalette(memDC, hPalette, FALSE);
	RealizePalette(memDC);
    }
    /* Copy the window contents to the memory surface. */
    if (!BitBlt(memDC, 0, 0, Tk_Width(tkwin), Tk_Height(tkwin), hDC, 0, 0,
		SRCCOPY)) {
	Tcl_AppendResult(interp, "can't blit \"", Tk_PathName(tkwin), "\": ",
			 Blt_LastError(), (char *)NULL);
	goto done;
    }
    /* Now that the DIB contains the image of the window, get the
     * databits and write them to the printer device, stretching the
     * image to the fit the printer's resolution.  */
    if (GetObject(hBitmap, sizeof(DIBSECTION), &ds) == 0) {
	Tcl_AppendResult(interp, "can't get DIB object: ", Blt_LastError(), 
			 (char *)NULL);
	goto done;
    } 
    driverName = NULL;
    if (Blt_GetPlatformId() == VER_PLATFORM_WIN32_NT) {
	driverName = queuePtr->driverName;
    }
    if (OpenQueue(interp, queuePtr) != TCL_OK) {
	goto done;
    }
    hMem = GetQueueProperties(queuePtr, &dmPtr);
    if (hMem == NULL) {
	goto done;
    }
    printDC = CreateDC(driverName, queuePtr->deviceName, NULL, dmPtr);
    GlobalUnlock(hMem);
    GlobalFree(hMem);
    if (printDC == NULL) {
	Tcl_AppendResult(interp, "can't allocate printer DC for \"",
		queuePtr->name, "\": ", Blt_LastError(), (char *)NULL);
	goto done;
    }
    {
	double scale, sx, sy;

	/* Get the resolution of the printer device. */
	sx = (double)GetDeviceCaps(printDC, HORZRES)/(double)Tk_Width(tkwin);
	sy = (double)GetDeviceCaps(printDC, VERTRES)/(double)Tk_Height(tkwin);
	scale = MIN(sx, sy);
	pageWidth = scale * Tk_Width(tkwin);
	pageHeight = scale * Tk_Height(tkwin);
    }
    ZeroMemory(&di, sizeof(di));
    di.cbSize = sizeof(di);
    Tcl_DStringAppend(&dString, "Snapshot of \"", -1);
    Tcl_DStringAppend(&dString, Tk_PathName(tkwin), -1);
    Tcl_DStringAppend(&dString, "\"", -1);
    di.lpszDocName = Tcl_DStringValue(&dString);
    jobId = StartDoc(printDC, &di);
    if (jobId <= 0) {
	Tcl_AppendResult(interp, "can't start document: ", Blt_LastError(), 
		(char *)NULL);
	goto done;
    }
    if (StartPage(printDC) <= 0) {
	Tcl_AppendResult(interp, "error starting page: ", Blt_LastError(), 
		(char *)NULL);
	goto done;
    }
    StretchDIBits(printDC, 0, 0, ROUND(pageWidth), ROUND(pageHeight), 0, 0, 
	Tk_Width(tkwin), Tk_Height(tkwin), ds.dsBm.bmBits, 
	(LPBITMAPINFO)&ds.dsBmih, DIB_RGB_COLORS, SRCCOPY);
    EndPage(printDC);
    EndDoc(printDC);
    DeleteDC(printDC);
    Tcl_SetResult(interp, Blt_Itoa(jobId), TCL_VOLATILE);
    result = TCL_OK;

  done:
    Tcl_DStringFree(&dString);
    if (queuePtr->hPrinter != NULL) {
	CloseQueue(queuePtr);
    }    
    DeleteBitmap(hBitmap);
    DeleteDC(memDC);
    TkWinReleaseDrawableDC(Tk_WindowId(tkwin), hDC, &state);
    if (hPalette != NULL) {
	DeletePalette(hPalette);
    }
    return result;
}

/*ARGSUSED*/
static int
WriteOp(
    ClientData clientData,	/* Interpreter-specific data. */
    Tcl_Interp *interp,
    int objc,			/* Not used. */
    Tcl_Obj *CONST *objv)
{
    DWORD bytesLeft, nBytes;
    DOC_INFO_1 di1;
    DWORD jobId;
    char *title;
    register char *data;
    static int nextJob = 0;
    char string[200];
    PrintQueue *queuePtr;
    int result;

    if (GetQueueFromObj(interp, objv[2], &queuePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (OpenQueue(interp, queuePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (objc == 5) {
	title = Tcl_GetString(objv[3]);
	data = Tcl_GetStringFromObj(objv[4], &bytesLeft);
    } else {
	sprintf(string, "Print Job #%d", nextJob++);
	title = string;
	data = Tcl_GetStringFromObj(objv[3], &bytesLeft);
    }
    ZeroMemory(&di1, sizeof(DOC_INFO_1));
    di1.pDocName = title;
    if (queuePtr->fileName != NULL) {
	di1.pOutputFile = queuePtr->fileName;
    } else {
	di1.pOutputFile = NULL;
    }
    di1.pDatatype = "RAW";

    result = TCL_ERROR;
    /* Start new document */
    jobId = StartDocPrinter(queuePtr->hPrinter, 1, (unsigned char *)&di1);
    if (jobId == 0) {
	Tcl_AppendResult(interp, "error starting document on \"", 
	 queuePtr->printerName, "\": ", Blt_LastError(), (char *)NULL);
	goto error;
    }
    /* Start new page */
    if (!StartPagePrinter(queuePtr->hPrinter)) {
	Tcl_AppendResult(interp, "error starting page on \"", 
	 queuePtr->printerName, "\": ", Blt_LastError(), (char *)NULL);
	goto error;
    }
    do {
	if (!WritePrinter(queuePtr->hPrinter, data, bytesLeft, &nBytes)) {
	    Tcl_AppendResult(interp, "can't write data to \"", 
		queuePtr->printerName, "\": ", Blt_LastError(), (char *)NULL);
	    EndDocPrinter(queuePtr->hPrinter);
	    goto error;
	}
	data += nBytes;
	bytesLeft -= nBytes;
    } while (bytesLeft > 0);
    /* End last page */
    if (!EndPagePrinter(queuePtr->hPrinter)) {
	Tcl_AppendResult(interp, "error ending page on \"", 
		queuePtr->printerName, "\": ", Blt_LastError(), (char *)NULL);
	goto error;
    }
    /* End document */
    if (!EndDocPrinter(queuePtr->hPrinter)) {
	Tcl_AppendResult(interp, "error ending document on \"", 
		queuePtr->printerName, "\": ", Blt_LastError(), (char *)NULL);
	goto error;
    }
    result = TCL_OK;
 error:
    CloseQueue(queuePtr);
    return result;
}

static Blt_OpSpec printerOps[] =
{
    {"close", 1, (Blt_Op)CloseOp, 3, 3, "pid",},
    {"enum", 1, (Blt_Op)EnumOp, 3, 3, "attribute",},
    {"getattrs", 1, (Blt_Op)GetAttrOp, 4, 4, "pid varName",},
    {"names", 1, (Blt_Op)NamesOp, 2, 3, "?pattern?",},
    {"open", 1, (Blt_Op)OpenOp, 3, 3, "printerName",},
    {"setattrs", 1, (Blt_Op)SetAttrOp, 4, 4, "pid varName",},
    {"snap", 1, (Blt_Op)SnapOp, 4, 4, "pid window",},
    {"write", 1, (Blt_Op)WriteOp, 4, 5, "pid ?title? string",},
};
static int nPrinterOps = sizeof(printerOps) / sizeof(Blt_OpSpec);

/* ARGSUSED */
static int
PrinterCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,
    int objc,
    Tcl_Obj *CONST *objv)
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOpFromObj(interp, nPrinterOps, printerOps, BLT_OP_ARG1, 
		    objc, objv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (clientData, interp, objc, objv);
    return result;
}

/*
 * -----------------------------------------------------------------------
 *
 * PrinterInterpDeleteProc --
 *
 *	This is called when the interpreter hosting one or more printer 
 *	commands is destroyed.  
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Closes and removes all open printers.
 *
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
PrinterInterpDeleteProc(clientData, interp)
    ClientData clientData;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
{
    PrinterInterpData *dataPtr = clientData;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    PrintQueue *queuePtr;

    for (hPtr = Blt_FirstHashEntry(&dataPtr->printerTable, &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	queuePtr = (PrintQueue *)Blt_GetHashValue(hPtr);
	queuePtr->hashPtr = NULL;
	DestroyQueue(queuePtr);
    }
    Blt_DeleteHashTable(&dataPtr->printerTable);
    Tcl_DeleteAssocData(interp, PRINTER_THREAD_KEY);
    Blt_Free(dataPtr);
}


int
Blt_PrinterInit(Tcl_Interp *interp)
{
    static Blt_ObjCmdSpec cmdSpec = {
	"printer", PrinterCmd
    };
    PrinterInterpData *dataPtr;

    dataPtr = GetPrinterInterpData(interp);
    cmdSpec.clientData = dataPtr;
    if (Blt_InitObjCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}


/* Public routines */
int
Blt_GetOpenPrinter(
    Tcl_Interp *interp,
    const char *name,
    Drawable *drawablePtr)
{
    PrintQueue *queuePtr;
    
    if (GetQueue(interp, name, &queuePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (queuePtr->drawable.hDC == NULL) {
	char *driverName;
	HGLOBAL hMem;
	DEVMODE *dmPtr;
	HDC hDC;

	driverName = NULL;
	if (Blt_GetPlatformId() == VER_PLATFORM_WIN32_NT) {
	    driverName = queuePtr->driverName;
	}
	if (OpenQueue(interp, queuePtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	hMem = GetQueueProperties(queuePtr, &dmPtr);
	if (hMem == NULL) {
	    CloseQueue(queuePtr);
	    return TCL_ERROR;
	}
	if (queuePtr->dmPtr != NULL) {
	    *dmPtr = *queuePtr->dmPtr;
	}
	hDC = CreateDC(driverName, queuePtr->deviceName, NULL, dmPtr);
	GlobalUnlock(hMem);
	GlobalFree(hMem);
	CloseQueue(queuePtr);
	if (hDC == NULL) {
	    Tcl_AppendResult(interp, "can't allocate printer DC for \"",
		queuePtr->name, "\": ", Blt_LastError(), (char *)NULL);
	    return TCL_ERROR;
	}
	queuePtr->drawable.hDC = hDC;
	queuePtr->drawable.type = TWD_WINDC;
    }
    *drawablePtr = (Drawable)(&queuePtr->drawable);
    return TCL_OK;
}

#include <commdlg.h>

int
Blt_PrintDialog(
    Tcl_Interp *interp,
    Drawable *drawablePtr)
{
    PRINTDLG dlg;
    static PrintDrawable drawable;
    int mode, result;
    
    ZeroMemory(&dlg, sizeof(PRINTDLG));
    dlg.lStructSize = sizeof(PRINTDLG);
    dlg.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION;
    mode = Tcl_SetServiceMode(TCL_SERVICE_NONE);
    result = PrintDlg(&dlg);
    Tcl_SetServiceMode(mode);
    if (!result) {
	if (!CommDlgExtendedError()) {
	    return TCL_RETURN;	/* User canceled. */
	}
	Tcl_AppendResult(interp, "can't access printer:", Blt_LastError(), 
			 (char *)NULL);
	return TCL_ERROR;
    } 
    *drawablePtr = (Drawable)&drawable;
    drawable.type = TWD_WINDC;
    drawable.hDC = dlg.hDC;
    return TCL_OK;
}

int
Blt_StartPrintJob(
    Tcl_Interp *interp,
    Drawable drawable)
{
    DOCINFO di;
    PrintDrawable *drawPtr = (PrintDrawable *)drawable;
    int jobId;

    ZeroMemory((char *)&di, sizeof(DOCINFO));
    di.cbSize = sizeof(DOCINFO);
    di.lpszDocName = "Unknown";
    jobId = StartDoc(drawPtr->hDC, &di);
    if (jobId == 0) {
	Tcl_AppendResult(interp, "error starting document: ",
 			 Blt_LastError(), (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

int
Blt_EndPrintJob(
    Tcl_Interp *interp,
    Drawable drawable)
{
    PrintDrawable *drawPtr = (PrintDrawable *)drawable;

    EndPage(drawPtr->hDC);
    EndDoc(drawPtr->hDC);
    return TCL_OK;
}

#endif /*NO_PRINTER*/
