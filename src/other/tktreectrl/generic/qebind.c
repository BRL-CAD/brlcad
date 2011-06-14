/* 
 * qebind.c --
 *
 *	This module implements quasi-events.
 *
 * Copyright (c) 2002-2010 Tim Baker
 *
 * RCS: @(#) $Id$
 */

/*
 * A general purpose module that allows a program to send event-like
 * messages to scripts, and to bind Tcl commands to those quasi-events.
 * Each event has it's own detail field and other fields, and this
 * module performs %-substitution on bound scripts just like regular
 * Tk binding model.
 *
 * To use it first call QE_BindInit() to initialize the package.
 * Then call QE_InstallEvent() for each new event you wish to define.
 * For events with details, call QE_InstallDetail() to register each
 * detail associated with a specific event type. Then create a
 * binding table, which records all binding commands defined by your
 * scripts, with QE_CreateBindingTable(). QE_BindCmd() is
 * called to associate a Tcl script with a given event for a particular
 * object. The objects that commands are bound to can be a Tk widget or any
 * string, just like the usual "bind" command. Bindings on Tk widgets are
 * automatically deleted when the widget is destroyed.
 */

#include <ctype.h>
#include <string.h>
#ifdef  HAVE_INTPTR_T
#include <stdint.h>
#endif
#include <tcl.h>
#include <tk.h>
#include "qebind.h"

#define dbwin TreeCtrl_dbwin
MODULE_SCOPE void dbwin(char *fmt, ...);

/*
 * The macro below is used to modify a "char" value (e.g. by casting
 * it to an unsigned character) so that it can be used safely with
 * macros such as isspace.
 */

#define UCHAR(c) ((unsigned char) (c))

/*
 * Macros used to cast between pointers and integers (e.g. when storing an int
 * in ClientData), on 64-bit architectures they avoid gcc warning about "cast
 * to/from pointer from/to integer of different size".
 */

#if !defined(INT2PTR) && !defined(PTR2INT)
#   if defined(HAVE_INTPTR_T) || defined(intptr_t)
#	define INT2PTR(p) ((void *)(intptr_t)(p))
#	define PTR2INT(p) ((int)(intptr_t)(p))
#   else
#	define INT2PTR(p) ((void *)(p))
#	define PTR2INT(p) ((int)(p))
#   endif
#endif

int debug_bindings = 0;

/*
 * Allow bindings to be deactivated.
 */
#define BIND_ACTIVE 1

/*
 * Allow new events to be added/removed by Tcl commands.
 */
#define ALLOW_INSTALL 1

/*
 * Delete scripts bound to a window when that window is destroyed.
 */
#define DELETE_WIN_BINDINGS 1

typedef struct BindValue {
	int type; /* Type of event, etc) */
	int detail; /* Misc. other information, or 0 for none */
	ClientData object;
	char *command;
	int specific; /* For less-specific events (detail=0), this is 1
				   * if a more-specific event (detail>0) exists. */
	struct BindValue *nextValue; /* list of BindValues matching event */
#if BIND_ACTIVE
	int active; /* 1 if binding is "active", 0 otherwise */
#endif /* BIND_ACTIVE */
} BindValue;

typedef struct Pattern {
	int type; /* Type of event */
	int detail; /* Misc. other information, or 0 for none */
} Pattern;

typedef struct PatternTableKey {
	int type; /* Type of event */
	int detail; /* Misc. other information, or 0 for none */
} PatternTableKey;

typedef struct ObjectTableKey {
	int type; /* Type of event */
	int detail; /* Misc. other information, or 0 for none */
	ClientData object; /* Object info */
} ObjectTableKey;
 
typedef struct Detail {
	Tk_Uid name; /* Name of detail */
	int code; /* Detail code */
	struct EventInfo *event; /* Associated event */
	QE_ExpandProc expandProc; /* Callback to expand % in scripts */
#if ALLOW_INSTALL
	int dynamic; /* Created by QE_InstallCmd() */
	char *command; /* Tcl command to expand percents, or NULL */
#endif
	struct Detail *next; /* List of Details for event */
} Detail;

typedef struct EventInfo {
	char *name; /* Name of event */
	int type; /* Type of event */
	QE_ExpandProc expandProc; /* Callback to expand % in scripts */
	Detail *detailList; /* List of Details */
	int nextDetailId; /* Next unique Detail.code */
#if ALLOW_INSTALL
	int dynamic; /* Created by QE_InstallCmd() */
	char *command; /* Tcl command to expand percents, or NULL */
#endif
	struct EventInfo *next; /* List of all EventInfos */
} EventInfo;

typedef struct GenerateField {
	char which; /* The %-char */
	char *string; /* Replace %-char with it */
} GenerateField;

typedef struct GenerateData {
	GenerateField staticField[20];
	GenerateField *field;
	int count;
	char *command; /* Tcl command to expand percents, or NULL */
} GenerateData;

typedef struct BindingTable {
	Tcl_Interp *interp;
	Tcl_HashTable patternTable; /* Key: PatternTableKey, Value: (BindValue *) */
	Tcl_HashTable objectTable; /* Key: ObjectTableKey, Value: (BindValue *) */
	Tcl_HashTable eventTableByName; /* Key: string, Value: EventInfo */
	Tcl_HashTable eventTableByType; /* Key: int, Value: EventInfo */
	Tcl_HashTable detailTableByType; /* Key: PatternTableKey, Value: Detail */
#if DELETE_WIN_BINDINGS
	Tcl_HashTable winTable; /* Key: Tk_Uid of window name, Value: WinTableValue */
#endif
	EventInfo *eventList; /* List of all EventInfos */
	int nextEventId; /* Next unique EventInfo.type */
} BindingTable;

static void ExpandPercents(BindingTable *bindPtr, ClientData object, char *command,
	QE_Event *eventPtr, QE_ExpandProc expandProc, Tcl_DString *result);
static int ParseEventDescription(BindingTable *bindPtr, char *eventPattern,
	Pattern *patPtr, EventInfo **eventInfoPtr, Detail **detailPtr);
static int FindSequence(BindingTable *bindPtr, ClientData object,
	char *eventString, int create, int *created, BindValue **result);
static void Percents_CharMap(QE_ExpandArgs *args);
static void Percents_Command(QE_ExpandArgs *args);
#if ALLOW_INSTALL
typedef struct PercentsData {
	GenerateData *gdPtr;
	char *command;
	EventInfo *eventPtr;
	Detail *detailPtr;
} PercentsData;
#endif
static int DeleteBinding(BindingTable *bindPtr, BindValue *valuePtr);
static EventInfo *FindEvent(BindingTable *bindPtr, int eventType);

static int initialized = 0;

int QE_BindInit(Tcl_Interp *interp)
{
	if (initialized)
		return TCL_OK;

	initialized = 1;

	return TCL_OK;
}

static int CheckName(char *name)
{
	char *p = name;

	if (*p == '\0')
		return TCL_ERROR;
	while ((*p != '\0') && (*p != '-') && !isspace(UCHAR(*p)))
		p++;
	if (*p == '\0')
		return TCL_OK;
	return TCL_ERROR;
}

int QE_InstallEvent(QE_BindingTable bindingTable, char *name, QE_ExpandProc expandProc)
{
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	Tcl_HashEntry *hPtr;
	EventInfo *eiPtr;
	int isNew;
	int type;

	if (CheckName(name) != TCL_OK)
	{
		Tcl_AppendResult(bindPtr->interp, "bad event name \"", name, "\"",
			(char *) NULL);
		return 0;
	}

	hPtr = Tcl_CreateHashEntry(&bindPtr->eventTableByName, name, &isNew);
	if (!isNew)
	{
		Tcl_AppendResult(bindPtr->interp, "event \"",
			name, "\" already exists", NULL);
		return 0;
	}

	type = bindPtr->nextEventId++;

	eiPtr = (EventInfo *) Tcl_Alloc(sizeof(EventInfo));
	eiPtr->name = Tcl_Alloc((int) strlen(name) + 1);
	strcpy(eiPtr->name, name);
	eiPtr->type = type;
	eiPtr->expandProc = expandProc;
	eiPtr->detailList = NULL;
	eiPtr->nextDetailId = 1;
#ifdef ALLOW_INSTALL
	eiPtr->dynamic = 0;
	eiPtr->command = NULL;
#endif

	Tcl_SetHashValue(hPtr, (ClientData) eiPtr);

	hPtr = Tcl_CreateHashEntry(&bindPtr->eventTableByType, (char *) INT2PTR(type), &isNew);
	Tcl_SetHashValue(hPtr, (ClientData) eiPtr);

	/* List of EventInfos */
	eiPtr->next = bindPtr->eventList;
	bindPtr->eventList = eiPtr;

	return type;
}

int QE_InstallDetail(QE_BindingTable bindingTable, char *name, int eventType, QE_ExpandProc expandProc)
{
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	Tcl_HashEntry *hPtr;
	Detail *dPtr;
	EventInfo *eiPtr;
	PatternTableKey key;
	int isNew;
	int code;

	if (CheckName(name) != TCL_OK)
	{
		Tcl_AppendResult(bindPtr->interp, "bad detail name \"", name, "\"",
			(char *) NULL);
		return 0;
	}

	/* Find the event this detail goes with */
	eiPtr = FindEvent(bindPtr, eventType);
	if (eiPtr == NULL)
		return 0;

	/* Verify the detail is not already defined for this event */
	for (dPtr = eiPtr->detailList;
		dPtr != NULL;
		dPtr = dPtr->next)
	{
		if (strcmp(dPtr->name, name) == 0)
		{
			Tcl_AppendResult(bindPtr->interp,
				"detail \"", name, "\" already exists for event \"",
				eiPtr->name, "\"", NULL);
			return 0;
		}
	}

	code = eiPtr->nextDetailId++;

	/* New Detail for detailTable */
	dPtr = (Detail *) Tcl_Alloc(sizeof(Detail));
	dPtr->name = Tk_GetUid(name);
	dPtr->code = code;
	dPtr->event = eiPtr;
	dPtr->expandProc = expandProc;
#if ALLOW_INSTALL
	dPtr->dynamic = 0;
	dPtr->command = NULL;
#endif

	/* Entry to find detail by event type and detail code */
	key.type = eventType;
	key.detail = code;
	hPtr = Tcl_CreateHashEntry(&bindPtr->detailTableByType, (char *) &key, &isNew);
	Tcl_SetHashValue(hPtr, (ClientData) dPtr);

	/* List of Details */
	dPtr->next = eiPtr->detailList;
	eiPtr->detailList = dPtr;

	return code;
}

static void DeleteEvent(BindingTable *bindPtr, EventInfo *eiPtr)
{
	EventInfo *eiPrev;
	Detail *dPtr, *dNext;

	/* Free Details */
	for (dPtr = eiPtr->detailList;
		dPtr != NULL;
		dPtr = dNext)
	{
		dNext = dPtr->next;
#ifdef ALLOW_INSTALL
		if (dPtr->command != NULL)
			Tcl_Free(dPtr->command);
#endif
		memset((char *) dPtr, 0xAA, sizeof(Detail));
		Tcl_Free((char *) dPtr);
	}

	if (bindPtr->eventList == eiPtr)
		bindPtr->eventList = eiPtr->next;
	else
	{
		for (eiPrev = bindPtr->eventList;
			eiPrev->next != eiPtr;
			eiPrev = eiPrev->next)
		{
		}
		eiPrev->next = eiPtr->next;
	}

	/* Free EventInfo */
	Tcl_Free(eiPtr->name);
#ifdef ALLOW_INSTALL
	if (eiPtr->command != NULL)
		Tcl_Free(eiPtr->command);
#endif
	memset((char *) eiPtr, 0xAA, sizeof(EventInfo));
	Tcl_Free((char *) eiPtr);
}

int QE_UninstallEvent(QE_BindingTable bindingTable, int eventType)
{
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	EventInfo *eiPtr;
	BindValue *valuePtr, **valueList;
	Tcl_DString dString;
	int i, count = 0;

	/* Find the event */
	hPtr = Tcl_FindHashEntry(&bindPtr->eventTableByType, (char *) INT2PTR(eventType));
	if (hPtr == NULL)
		return TCL_ERROR;
	eiPtr = (EventInfo *) Tcl_GetHashValue(hPtr);
	Tcl_DeleteHashEntry(hPtr);

	hPtr = Tcl_FindHashEntry(&bindPtr->eventTableByName, eiPtr->name);
	Tcl_DeleteHashEntry(hPtr);

	Tcl_DStringInit(&dString);

	/* Find all bindings to this event for any object */
	hPtr = Tcl_FirstHashEntry(&bindPtr->patternTable, &search);
	while (hPtr != NULL)
	{
		valuePtr = (BindValue *) Tcl_GetHashValue(hPtr);
		while (valuePtr != NULL)
		{
			if (valuePtr->type == eiPtr->type)
			{
				Tcl_DStringAppend(&dString, (char *) &valuePtr, sizeof(valuePtr));
				count++;
			}
			valuePtr = valuePtr->nextValue;
		}
		hPtr = Tcl_NextHashEntry(&search);
	}

	valueList = (BindValue **) Tcl_DStringValue(&dString);
	for (i = 0; i < count; i++)
		DeleteBinding(bindPtr, valueList[i]);

	Tcl_DStringFree(&dString);

	DeleteEvent(bindPtr, eiPtr);

	return TCL_OK;
}

int QE_UninstallDetail(QE_BindingTable bindingTable, int eventType, int detail)
{
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	PatternTableKey key;
	Tcl_HashEntry *hPtr;
	Detail *dPtr = NULL, *dPrev;
	EventInfo *eiPtr;

	/* Find the event */
	eiPtr = FindEvent(bindPtr, eventType);
	if (eiPtr == NULL)
		return TCL_ERROR;

	if (eiPtr->detailList == NULL)
		return TCL_ERROR;

	/* Delete all bindings on this event/detail for all objects */
	while (1)
	{
		key.type = eventType;
		key.detail = detail;
		hPtr = Tcl_FindHashEntry(&bindPtr->patternTable, (char *) &key);
		if (hPtr == NULL)
			break;
		DeleteBinding(bindPtr, (BindValue *) Tcl_GetHashValue(hPtr));
	}

	if (eiPtr->detailList->code == detail)
	{
		dPtr = eiPtr->detailList;
		eiPtr->detailList = eiPtr->detailList->next;
	}
	else
	{
		for (dPrev = eiPtr->detailList;
			dPrev != NULL;
			dPrev = dPrev->next)
		{
			if ((dPrev->next != NULL) && (dPrev->next->code == detail))
			{
				dPtr = dPrev->next;
				dPrev->next = dPtr->next;
				break;
			}
		}
		if (dPtr == NULL)
			return TCL_ERROR;
	}

#ifdef ALLOW_INSTALL
	if (dPtr->command != NULL)
		Tcl_Free(dPtr->command);
#endif
	memset((char *) dPtr, 0xAA, sizeof(Detail));
	Tcl_Free((char *) dPtr);

	key.type = eventType;
	key.detail = detail;
	hPtr = Tcl_FindHashEntry(&bindPtr->detailTableByType, (char *) &key);
	Tcl_DeleteHashEntry(hPtr);

	return TCL_OK;
}

static EventInfo *FindEvent(BindingTable *bindPtr, int eventType)
{
	Tcl_HashEntry *hPtr;

	hPtr = Tcl_FindHashEntry(&bindPtr->eventTableByType, (char *) INT2PTR(eventType));
	if (hPtr == NULL) return NULL;
	return (EventInfo *) Tcl_GetHashValue(hPtr);
}

static Detail *FindDetail(BindingTable *bindPtr, int eventType, int code)
{
	PatternTableKey key;
	Tcl_HashEntry *hPtr;

	key.type = eventType;
	key.detail = code;
	hPtr = Tcl_FindHashEntry(&bindPtr->detailTableByType, (char *) &key);
	if (hPtr == NULL) return NULL;
	return (Detail *) Tcl_GetHashValue(hPtr);
}

#if DELETE_WIN_BINDINGS
typedef struct WinTableValue
{
	BindingTable *bindPtr;
	ClientData object;
	Tk_Window tkwin;
	int count; /* Number of BindValues on object */
} WinTableValue;
static void TkWinEventProc(ClientData clientData, XEvent *eventPtr)
{
	WinTableValue *cd = (WinTableValue *) clientData;
	BindingTable *bindPtr = cd->bindPtr;
	ClientData object = cd->object;

	if (eventPtr->type != DestroyNotify)
		return;

	QE_DeleteBinding((QE_BindingTable) bindPtr, object, NULL);
}
#endif

QE_BindingTable QE_CreateBindingTable(Tcl_Interp *interp)
{
	BindingTable *bindPtr;

	bindPtr = (BindingTable *) Tcl_Alloc(sizeof(BindingTable));
	bindPtr->interp = interp;
	Tcl_InitHashTable(&bindPtr->patternTable,
		sizeof(PatternTableKey) / sizeof(int));
	Tcl_InitHashTable(&bindPtr->objectTable,
		sizeof(ObjectTableKey) / sizeof(int));
	Tcl_InitHashTable(&bindPtr->eventTableByName, TCL_STRING_KEYS);
	Tcl_InitHashTable(&bindPtr->eventTableByType, TCL_ONE_WORD_KEYS);
	Tcl_InitHashTable(&bindPtr->detailTableByType,
		sizeof(PatternTableKey) / sizeof(int));
#if DELETE_WIN_BINDINGS
	Tcl_InitHashTable(&bindPtr->winTable, TCL_ONE_WORD_KEYS);
#endif
	bindPtr->nextEventId = 1;
	bindPtr->eventList = NULL;

	return (QE_BindingTable) bindPtr;
}

void QE_DeleteBindingTable(QE_BindingTable bindingTable)
{
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	EventInfo *eiPtr, *eiNext;
	Detail *dPtr, *dNext;

	hPtr = Tcl_FirstHashEntry(&bindPtr->patternTable, &search);
	while (hPtr != NULL)
	{
		BindValue *valuePtr = (BindValue *) Tcl_GetHashValue(hPtr);
		while (valuePtr != NULL)
		{
			BindValue *nextValue = valuePtr->nextValue;
			Tcl_Free((char *) valuePtr->command);
			memset((char *) valuePtr, 0xAA, sizeof(BindValue));
			Tcl_Free((char *) valuePtr);
			valuePtr = nextValue;
		}
		hPtr = Tcl_NextHashEntry(&search);
	}
	Tcl_DeleteHashTable(&bindPtr->patternTable);
	Tcl_DeleteHashTable(&bindPtr->objectTable);

	for (eiPtr = bindPtr->eventList;
		eiPtr != NULL;
		eiPtr = eiNext)
	{
		eiNext = eiPtr->next;

		/* Free Detail */
		for (dPtr = eiPtr->detailList;
			dPtr != NULL;
			dPtr = dNext)
		{
			dNext = dPtr->next;
#ifdef ALLOW_INSTALL
			if (dPtr->command != NULL)
				Tcl_Free(dPtr->command);
#endif
			memset((char *) dPtr, 0xAA, sizeof(Detail));
			Tcl_Free((char *) dPtr);
		}

		/* Free EventInfo */
		Tcl_Free(eiPtr->name);
#ifdef ALLOW_INSTALL
		if (eiPtr->command != NULL)
			Tcl_Free(eiPtr->command);
#endif
		memset((char *) eiPtr, 0xAA, sizeof(EventInfo));
		Tcl_Free((char *) eiPtr);
	}

	Tcl_DeleteHashTable(&bindPtr->eventTableByName);
	Tcl_DeleteHashTable(&bindPtr->eventTableByType);
	Tcl_DeleteHashTable(&bindPtr->detailTableByType);

#if DELETE_WIN_BINDINGS
	hPtr = Tcl_FirstHashEntry(&bindPtr->winTable, &search);
	while (hPtr != NULL)
	{
		WinTableValue *cd = (WinTableValue *) Tcl_GetHashValue(hPtr);

		Tk_DeleteEventHandler(cd->tkwin, StructureNotifyMask,
			TkWinEventProc, (ClientData) cd);
		Tcl_Free((char *) cd);
		hPtr = Tcl_NextHashEntry(&search);
	}
	Tcl_DeleteHashTable(&bindPtr->winTable);
#endif

	memset((char *) bindPtr, 0xAA, sizeof(BindingTable));
	Tcl_Free((char *) bindPtr);
}

int QE_CreateBinding(QE_BindingTable bindingTable, ClientData object,
	char *eventString, char *command, int append)
{
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	BindValue *valuePtr;
	int isNew, length;
	char *cmdOld, *cmdNew;

	if (FindSequence(bindPtr, object, eventString, 1, &isNew, &valuePtr) != TCL_OK)
		return TCL_ERROR;

	/* created a new objectTable entry */
	if (isNew)
	{
		Tcl_HashEntry *hPtr;
		PatternTableKey key;
#if DELETE_WIN_BINDINGS
		char *winName = (char *) object;

		if (winName[0] == '.')
		{
			Tk_Window tkwin = Tk_MainWindow(bindPtr->interp);
			Tk_Window tkwin2;

			tkwin2 = Tk_NameToWindow(bindPtr->interp, winName, tkwin);
			if (tkwin2 != NULL)
			{
				WinTableValue *cd;

				hPtr = Tcl_CreateHashEntry(&bindPtr->winTable, object, &isNew);
				if (isNew)
				{
					cd = (WinTableValue *) Tcl_Alloc(sizeof(WinTableValue));
					cd->bindPtr = bindPtr;
					cd->object = object;
					cd->tkwin = tkwin2;
					cd->count = 0;
					Tk_CreateEventHandler(tkwin2, StructureNotifyMask,
						TkWinEventProc, (ClientData) cd);
					Tcl_SetHashValue(hPtr, (ClientData) cd);
				}
				else
				{
					cd = (WinTableValue *) Tcl_GetHashValue(hPtr);
				}
				/* Number of BindValues for this window */
				cd->count++;
			}
		}
#endif

		key.type = valuePtr->type;
		key.detail = valuePtr->detail;
		hPtr = Tcl_CreateHashEntry(&bindPtr->patternTable, (char *) &key,
			&isNew);

		/*
		 * A patternTable entry exists for each different type/detail.
		 * The entry points to a BindValue which is the head of the list
		 * of BindValue's with this same type/detail, but for different
		 * objects.
		 */
		if (!isNew)
		{
			valuePtr->nextValue = (BindValue *) Tcl_GetHashValue(hPtr);
		}
		Tcl_SetHashValue(hPtr, (ClientData) valuePtr);
	}

	cmdOld = valuePtr->command;

	/* Append given command to any existing command */
	if (append && cmdOld)
	{
		length = (int) (strlen(cmdOld) + strlen(command) + 2);
		cmdNew = Tcl_Alloc((unsigned) length);
		(void) sprintf(cmdNew, "%s\n%s", cmdOld, command);
	}
	/* Copy the given command */
	else
	{
		cmdNew = (char *) Tcl_Alloc((unsigned) strlen(command) + 1);
		(void) strcpy(cmdNew, command);
	}

	/* Free the old command, if any */
	if (cmdOld) Tcl_Free(cmdOld);

	/* Save command associated with this binding */
	valuePtr->command = cmdNew;

	return TCL_OK;
}

int QE_DeleteBinding(QE_BindingTable bindingTable, ClientData object,
	char *eventString)
{
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	BindValue *valuePtr, **valueList;

	/* Delete all bindings on this object */
	if (eventString == NULL)
	{
		Tcl_HashEntry *hPtr;
		Tcl_HashSearch search;
		Tcl_DString dString;
		int i, count = 0;

		Tcl_DStringInit(&dString);

		hPtr = Tcl_FirstHashEntry(&bindPtr->patternTable, &search);
		while (hPtr != NULL)
		{
			valuePtr = (BindValue *) Tcl_GetHashValue(hPtr);
			while (valuePtr != NULL)
			{
				if (valuePtr->object == object)
				{
					Tcl_DStringAppend(&dString, (char *) &valuePtr,
						sizeof(valuePtr));
					count++;
					break;
				}
				valuePtr = valuePtr->nextValue;
			}
			hPtr = Tcl_NextHashEntry(&search);
		}

		valueList = (BindValue **) Tcl_DStringValue(&dString);
		for (i = 0; i < count; i++)
			DeleteBinding(bindPtr, valueList[i]);

		Tcl_DStringFree(&dString);

		return TCL_OK;
	}

	if (FindSequence(bindPtr, object, eventString, 0, NULL, &valuePtr) != TCL_OK)
		return TCL_ERROR;
	if (valuePtr == NULL)
	{
		Tcl_ResetResult(bindPtr->interp);
		return TCL_OK;
	}
	DeleteBinding(bindPtr, valuePtr);
	return TCL_OK;
}

static int DeleteBinding(BindingTable *bindPtr, BindValue *valuePtr)
{
	Tcl_HashEntry *hPtr;
	BindValue *listPtr;
	ObjectTableKey keyObj;
	PatternTableKey keyPat;

	/* Delete the objectTable entry */
	keyObj.type = valuePtr->type;
	keyObj.detail = valuePtr->detail;
	keyObj.object = valuePtr->object;
	hPtr = Tcl_FindHashEntry(&bindPtr->objectTable, (char *) &keyObj);
	if (hPtr == NULL) return TCL_ERROR; /* fatal error */
	Tcl_DeleteHashEntry(hPtr);

	/* Find the patternTable entry for this type/detail */
	keyPat.type = valuePtr->type;
	keyPat.detail = valuePtr->detail;
	hPtr = Tcl_FindHashEntry(&bindPtr->patternTable, (char *) &keyPat);
	if (hPtr == NULL) return TCL_ERROR; /* fatal error */

	/*
	 * Get the patternTable value. This is the head of a list of
	 * BindValue's that match the type/detail, but for different
	 * objects;
	 */
	listPtr = (BindValue *) Tcl_GetHashValue(hPtr);

	/* The deleted BindValue is the first */
	if (listPtr == valuePtr)
	{
		/* The deleted BindValue was the only one in the list */
		if (valuePtr->nextValue == NULL)
		{
			if (debug_bindings)
				dbwin("QE_DeleteBinding: Deleted pattern type=%d detail=%d\n",
					valuePtr->type, valuePtr->detail);

			Tcl_DeleteHashEntry(hPtr);
		}
		/* The next BindValue is the new head of the list */
		else
		{
			Tcl_SetHashValue(hPtr, valuePtr->nextValue);
		}
	}
	/* Look for the deleted BindValue in the list, and remove it */
	else
	{
		while (1)
		{
			if (listPtr->nextValue == NULL) return TCL_ERROR; /* fatal */
			if (listPtr->nextValue == valuePtr)
			{
				if (debug_bindings)
					dbwin("QE_DeleteBinding: Unlinked binding type=%d detail=%d\n",
						valuePtr->type, valuePtr->detail);

				listPtr->nextValue = valuePtr->nextValue;
				break;
			}
			listPtr = listPtr->nextValue;
		}
	}

#if DELETE_WIN_BINDINGS
	{
		char *winName = (char *) valuePtr->object;

		if (winName[0] == '.')
		{
			WinTableValue *cd;

			hPtr = Tcl_FindHashEntry(&bindPtr->winTable, winName);
			if (hPtr == NULL) return TCL_ERROR; /* fatal error */
			cd = (WinTableValue *) Tcl_GetHashValue(hPtr);
			cd->count--;
			if (cd->count == 0)
			{
				Tk_DeleteEventHandler(cd->tkwin, StructureNotifyMask,
					TkWinEventProc, (ClientData) cd);
				Tcl_Free((char *) cd);
				Tcl_DeleteHashEntry(hPtr);
			}
		}
	}
#endif

	Tcl_Free((char *) valuePtr->command);
	memset((char *) valuePtr, 0xAA, sizeof(BindValue));
	Tcl_Free((char *) valuePtr);

	return TCL_OK;
}

int QE_GetAllObjects(QE_BindingTable bindingTable)
{
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	Tcl_DString dString;
	ClientData *objectList;
	int i, count = 0;
	Tcl_Obj *listObj;

	Tcl_DStringInit(&dString);
	hPtr = Tcl_FirstHashEntry(&bindPtr->patternTable, &search);
	while (hPtr != NULL)
	{
		BindValue *valuePtr = (BindValue *) Tcl_GetHashValue(hPtr);
		while (valuePtr != NULL)
		{
			objectList = (ClientData *) Tcl_DStringValue(&dString);
			for (i = 0; i < count; i++)
			{
				if (objectList[i] == valuePtr->object)
					break;
			}
			if (i >= count)
			{
				Tcl_DStringAppend(&dString, (char *) &valuePtr->object,
					sizeof(ClientData));
				count++;
			}
			valuePtr = valuePtr->nextValue;
		}
		hPtr = Tcl_NextHashEntry(&search);
	}
	if (count > 0)
	{
		listObj = Tcl_NewListObj(0, NULL);
		objectList = (ClientData *) Tcl_DStringValue(&dString);
		for (i = 0; i < count; i++)
		{
			Tcl_ListObjAppendElement(bindPtr->interp, listObj,
				Tcl_NewStringObj((char *) objectList[i], -1));
		}
		Tcl_SetObjResult(bindPtr->interp, listObj);
	}
	Tcl_DStringFree(&dString);

	return TCL_OK;
}

int QE_GetBinding(QE_BindingTable bindingTable, ClientData object,
	char *eventString)
{
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	BindValue *valuePtr;

	if (FindSequence(bindPtr, object, eventString, 0, NULL, &valuePtr) != TCL_OK)
		return TCL_ERROR;
	if (valuePtr == NULL)
		return TCL_OK;
	Tcl_SetObjResult(bindPtr->interp, Tcl_NewStringObj(valuePtr->command, -1));
	return TCL_OK;
}

static void GetPatternString(BindingTable *bindPtr, BindValue *bindValue, Tcl_DString *dString)
{
	EventInfo *eiPtr;

	eiPtr = FindEvent(bindPtr, bindValue->type);
	if (eiPtr != NULL)
	{
		Tcl_DStringAppend(dString, "<", 1);
		Tcl_DStringAppend(dString, eiPtr->name, -1);
		if (bindValue->detail)
		{
			Detail *detail = FindDetail(bindPtr, bindValue->type, bindValue->detail);
			if (detail != NULL)
			{
				Tcl_DStringAppend(dString, "-", 1);
				Tcl_DStringAppend(dString, detail->name, -1);
			}
		}
		Tcl_DStringAppend(dString, ">", 1);
	}
}

int QE_GetAllBindings(QE_BindingTable bindingTable, ClientData object)
{
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch search;
	Tcl_DString dString;

	Tcl_DStringInit(&dString);
	hPtr = Tcl_FirstHashEntry(&bindPtr->patternTable, &search);
	while (hPtr != NULL)
	{
		BindValue *valuePtr = (BindValue *) Tcl_GetHashValue(hPtr);
		while (valuePtr != NULL)
		{
			if (valuePtr->object == object)
			{
				Tcl_DStringSetLength(&dString, 0);
				GetPatternString(bindPtr, valuePtr, &dString);
				Tcl_AppendElement(bindPtr->interp, Tcl_DStringValue(&dString));
				break;
			}
			valuePtr = valuePtr->nextValue;
		}
		hPtr = Tcl_NextHashEntry(&search);
	}
	Tcl_DStringFree(&dString);

	return TCL_OK;
}

int QE_GetEventNames(QE_BindingTable bindingTable)
{
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	EventInfo *eiPtr;

	for (eiPtr = bindPtr->eventList;
		eiPtr != NULL;
		eiPtr = eiPtr->next)
	{
		Tcl_AppendElement(bindPtr->interp, eiPtr->name);
	}

	return TCL_OK;
}

int QE_GetDetailNames(QE_BindingTable bindingTable, char *eventName)
{
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	Tcl_HashEntry *hPtr;
	EventInfo *eiPtr;
	Detail *dPtr;

	hPtr = Tcl_FindHashEntry(&bindPtr->eventTableByName, eventName);
	if (hPtr == NULL)
	{
		Tcl_AppendResult(bindPtr->interp, "unknown event \"", eventName,
			"\"", NULL);
		return TCL_ERROR;
	}
	eiPtr = (EventInfo *) Tcl_GetHashValue(hPtr);

	for (dPtr = eiPtr->detailList;
		dPtr != NULL;
		dPtr = dPtr->next)
	{
		Tcl_AppendElement(bindPtr->interp, dPtr->name);
	}

	return TCL_OK;
}

static void ExpandPercents(BindingTable *bindPtr, ClientData object,
	char *command, QE_Event *eventPtr, QE_ExpandProc expandProc,
	Tcl_DString *result)
{
	char *string;
	QE_ExpandArgs expandArgs;

#if 0
	Tcl_DStringSetLength(result, 0);
	if (debug_bindings)
		dbwin("ExpandPercents on '%s' name=%s type=%d detail=%d expand=%lu\n",
			object, eiPtr->name, eiPtr->type, eventPtr->detail, eiPtr->expand);
#endif
	expandArgs.bindingTable = (QE_BindingTable) bindPtr;
	expandArgs.object = object;
	expandArgs.event = eventPtr->type;
	expandArgs.detail = eventPtr->detail;
	expandArgs.result = result;
	expandArgs.clientData = eventPtr->clientData;

	while (1)
	{
		for (string = command; (*string != 0) && (*string != '%'); string++)
		{
			/* Empty loop body. */
		}
		if (string != command)
		{
			Tcl_DStringAppend(result, command, (int) (string - command));
			command = string;
		}
		if (*command == 0)
		{
			break;
		}

		/* Expand % here */
		expandArgs.which = command[1];
		(*expandProc)(&expandArgs);

		command += 2;
	}
}

static void BindEvent(BindingTable *bindPtr, QE_Event *eventPtr, int wantDetail,
	EventInfo *eiPtr, Detail *dPtr, GenerateData *gdPtr)
{
	Tcl_HashEntry *hPtr;
	BindValue *valuePtr;
	ObjectTableKey keyObj;
	PatternTableKey key;
	Tcl_DString scripts, savedResult;
	int code;
	char *p, *end;
	char *command = gdPtr ? gdPtr->command : NULL;

	/* Find the first BindValue for this event */
	key.type = eventPtr->type;
	key.detail = wantDetail ? eventPtr->detail : 0;
	hPtr = Tcl_FindHashEntry(&bindPtr->patternTable, (char *) &key);
	if (hPtr == NULL)
		return;

	/* Collect all scripts, with % expanded, separated by null characters.
	 * Do it this way because anything could happen while evaluating, including
	 * uninstalling events/details, even the interpreter being deleted. */
	Tcl_DStringInit(&scripts);

	for (valuePtr = (BindValue *) Tcl_GetHashValue(hPtr);
		valuePtr; valuePtr = valuePtr->nextValue)
	{
		if (wantDetail && valuePtr->detail)
		{
			keyObj.type = key.type;
			keyObj.detail = 0;
			keyObj.object = valuePtr->object;
			hPtr = Tcl_FindHashEntry(&bindPtr->objectTable, (char *) &keyObj);
			if (hPtr != NULL)
			{
				BindValue *value2Ptr;
				value2Ptr = (BindValue *) Tcl_GetHashValue(hPtr);
				value2Ptr->specific = 1;
			}
		}
		
		/*
		 * If a binding for a more-specific event exists for this object
		 * and event-type, and this is a binding for a less-specific
		 * event, then skip this binding, since the binding for the
		 * more-specific event was already invoked.
		 */
		else if (!wantDetail && valuePtr->specific)
		{
			if (debug_bindings)
				dbwin("QE_BindEvent: Skipping less-specific event type=%d object='%s'\n",
					valuePtr->type, (char *) valuePtr->object);

			valuePtr->specific = 0;
			continue;
		}

#if BIND_ACTIVE
		/* This binding isn't active */
		if (valuePtr->active == 0)
			continue;
#endif /* BIND_ACTIVE */

#if ALLOW_INSTALL
		if (command == NULL)
		{
			if ((dPtr != NULL) && (dPtr->command != NULL))
			{
				command = dPtr->command;
			}
			else if (((dPtr == NULL) ||
				((dPtr != NULL) && (dPtr->expandProc == NULL))) &&
				(eiPtr->command != NULL))
			{
				command = eiPtr->command;
			}
		}
#endif /* ALLOW_INSTALL */

		/* called by QE_GenerateCmd */
		if (command != NULL)
		{
			PercentsData data;

			data.gdPtr = gdPtr;
			data.command = command;
			data.eventPtr = eiPtr;
			data.detailPtr = dPtr;
			eventPtr->clientData = (ClientData) &data;
			ExpandPercents(bindPtr, valuePtr->object, valuePtr->command,
				eventPtr, Percents_Command, &scripts);
		}

		/* called by QE_GenerateCmd */
		else if (gdPtr != NULL)
		{
			/* Called QE_GenerateCmd with:
			 * a) a static event and no percentsCommand argument, or
			 * b) a dynamic event with no percentsCommand installed and
			 *    no percentsCommand argument
			 */
			eventPtr->clientData = (ClientData) gdPtr;
			ExpandPercents(bindPtr, valuePtr->object, valuePtr->command,
				eventPtr, Percents_CharMap, &scripts);
		}
		else
		{
			QE_ExpandProc expandProc =
				((dPtr != NULL) && (dPtr->expandProc != NULL)) ?
				dPtr->expandProc : eiPtr->expandProc;

			ExpandPercents(bindPtr, valuePtr->object, valuePtr->command,
				eventPtr, expandProc, &scripts);
		}

		/* Separate each script by '\0' */
		Tcl_DStringAppend(&scripts, "", 1);

		Tcl_DStringAppend(&scripts, eiPtr->name, -1);
		Tcl_DStringAppend(&scripts, "", 1);

		Tcl_DStringAppend(&scripts, (valuePtr->detail && dPtr) ? dPtr->name : "", -1);
		Tcl_DStringAppend(&scripts, "", 1);

		Tcl_DStringAppend(&scripts, valuePtr->object, -1);
		Tcl_DStringAppend(&scripts, "", 1);
	}

	/* Nothing to do. No need to call Tcl_DStringFree(&scripts) */
	if (Tcl_DStringLength(&scripts) == 0)
		return;
 
	/*
	 * As in Tk bindings, we expect that bindings may be invoked
	 * in the middle of Tcl commands. So we preserve the current
	 * interpreter result and restore it later.
	 */
	Tcl_DStringInit(&savedResult);
	Tcl_DStringGetResult(bindPtr->interp, &savedResult);

	p = Tcl_DStringValue(&scripts);
	end = p + Tcl_DStringLength(&scripts);

	while (p < end)
	{
		code = Tcl_GlobalEval(bindPtr->interp, p);
		p += strlen(p);
		p++;

		if (code != TCL_OK)
		{
			if (code == TCL_CONTINUE)
			{
				/* Nothing */
			}
			else if (code == TCL_BREAK)
			{
				/* Nothing */
			}
			else
			{
				char buf[256];
				char *eventName = p;
				char *detailName = p + strlen(p) + 1;
				char *object = detailName + strlen(detailName) + 1;

				(void) sprintf(buf, "\n    (<%s%s%s> binding on %s)",
					eventName, detailName[0] ? "-" : "", detailName, object);
				Tcl_AddErrorInfo(bindPtr->interp, buf);
				Tcl_BackgroundError(bindPtr->interp);
			}
		}

		/* Skip event\0detail\0object\0 */
		p += strlen(p);
		p++;
		p += strlen(p);
		p++;
		p += strlen(p);
		p++;
	}

	Tcl_DStringFree(&scripts);

	/* Restore the interpreter result */
	Tcl_DStringResult(bindPtr->interp, &savedResult);
}

static int BindEventWrapper(QE_BindingTable bindingTable, QE_Event *eventPtr, GenerateData *gdPtr)
{
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	Detail *dPtr = NULL;
	EventInfo *eiPtr;

	/* Find the event */
	eiPtr = FindEvent(bindPtr, eventPtr->type);
	if (eiPtr == NULL)
		return TCL_OK;

	/* Find the detail */
	if (eventPtr->detail)
	{
		dPtr = FindDetail(bindPtr, eventPtr->type, eventPtr->detail);
		if (dPtr == NULL)
			return TCL_OK;
	}

	BindEvent(bindPtr, eventPtr, 1, eiPtr, dPtr, gdPtr);
	if (eventPtr->detail)
		BindEvent(bindPtr, eventPtr, 0, eiPtr, dPtr, gdPtr);

	return TCL_OK;
}

int QE_BindEvent(QE_BindingTable bindingTable, QE_Event *eventPtr)
{
	return BindEventWrapper(bindingTable, eventPtr, NULL);
}

static char *GetField(char *p, char *copy, int size)
{
	int ch = *p;

	while ((ch != '\0') && !isspace(UCHAR(ch)) &&
		((ch != '>') || (p[1] != '\0'))
		&& (ch != '-') && (size > 1))
	{
		*copy = ch;
		p++;
		copy++;
		size--;
		ch = *p;
	}
	*copy = '\0';

	while ((*p == '-') || isspace(UCHAR(*p)))
	{
		p++;
	}
	return p;
}

#define FIELD_SIZE 48

static int ParseEventDescription1(BindingTable *bindPtr, char *pattern, char eventName[FIELD_SIZE], char detailName[FIELD_SIZE])
{
	Tcl_Interp *interp = bindPtr->interp;
	char *p = pattern;

	eventName[0] = detailName[0] = '\0';

	/* First char must by opening < */
	if (*p != '<')
	{
		Tcl_AppendResult(interp, "missing \"<\" in event pattern \"",
			pattern, "\"", (char *) NULL);
		return TCL_ERROR;
	}
	p++;

	/* Event name (required)*/
	p = GetField(p, eventName, FIELD_SIZE);

	if (debug_bindings)
		dbwin("GetField='%s'\n", eventName);

	/* Terminating > */
	if (*p == '>')
		return TCL_OK;

	/* Detail name (optional) */
	p = GetField(p, detailName, FIELD_SIZE);

	if (debug_bindings)
		dbwin("GetField='%s'\n", detailName);

	/* Terminating > */
	if (*p != '>')
	{
		Tcl_AppendResult(interp, "missing \">\" in event pattern \"",
			pattern, "\"", (char *) NULL);
		return TCL_ERROR;
	}

	return TCL_OK;
}

static int ParseEventDescription(BindingTable *bindPtr, char *eventString,
	Pattern *patPtr, EventInfo **eventInfoPtr, Detail **detailPtr)
{
	Tcl_Interp *interp = bindPtr->interp;
	Tcl_HashEntry *hPtr;
	char eventName[FIELD_SIZE], detailName[FIELD_SIZE];
	EventInfo *eiPtr;
	Detail *dPtr;
	char errorMsg[512];

	if (eventInfoPtr) *eventInfoPtr = NULL;
	if (detailPtr) *detailPtr = NULL;

	patPtr->type = -1;
	patPtr->detail = 0;

	if (ParseEventDescription1(bindPtr, eventString, eventName, detailName) != TCL_OK)
		return TCL_ERROR;

	hPtr = Tcl_FindHashEntry(&bindPtr->eventTableByName, eventName);
	if (hPtr == NULL)
	{
		sprintf(errorMsg, "unknown event \"%.128s\"", eventName);
		Tcl_SetResult(interp, errorMsg, TCL_VOLATILE);
		return TCL_ERROR;
	}
	eiPtr = (EventInfo *) Tcl_GetHashValue(hPtr);
	patPtr->type = eiPtr->type;
	if (eventInfoPtr) *eventInfoPtr = eiPtr;

	if (detailName[0] != '\0')
	{
		/* Find detail for the matching event */
		for (dPtr = eiPtr->detailList;
			dPtr != NULL;
			dPtr = dPtr->next)
		{
			if (strcmp(dPtr->name, detailName) == 0)
				break;
		}
		if (dPtr == NULL)
		{
			sprintf(errorMsg, "unknown detail \"%.128s\" for event \"%.128s\"",
				detailName, eiPtr->name);
			Tcl_SetResult(interp, errorMsg, TCL_VOLATILE);
			return TCL_ERROR;
		}
		patPtr->detail = dPtr->code;
		if (detailPtr) *detailPtr = dPtr;
	}

	return TCL_OK;
}

static int FindSequence(BindingTable *bindPtr, ClientData object,
	char *eventString, int create, int *created, BindValue **result)
{
	Tcl_HashEntry *hPtr;
	Pattern pats;
	ObjectTableKey key;
	BindValue *valuePtr;
	int isNew;

	if (debug_bindings)
		dbwin("FindSequence object='%s' pattern='%s'...\n", (char *) object,
		eventString);

	if (created) (*created) = 0;

	/* Event description -> Pattern */
	if (ParseEventDescription(bindPtr, eventString, &pats, NULL, NULL) != TCL_OK)
		return TCL_ERROR;

	/* type + detail + object -> BindValue */
	key.type = pats.type;
	key.detail = pats.detail;
	key.object = object;
	if (create)
	{
		hPtr = Tcl_CreateHashEntry(&bindPtr->objectTable, (char *) &key, &isNew);

		if (isNew)
		{
			if (debug_bindings)
				dbwin("New BindValue for '%s' type=%d detail=%d\n",
					(char *) object, pats.type, pats.detail);

			valuePtr = (BindValue *) Tcl_Alloc(sizeof(BindValue));
			valuePtr->type = pats.type;
			valuePtr->detail = pats.detail;
			valuePtr->object = object;
			valuePtr->command = NULL;
			valuePtr->specific = 0;
			valuePtr->nextValue = NULL;
#if BIND_ACTIVE
			/* This binding is active */
			valuePtr->active = 1;
#endif /* BIND_ACTIVE */
			Tcl_SetHashValue(hPtr, (ClientData) valuePtr);
		}

		if (created) (*created) = isNew;
		(*result) = (BindValue *) Tcl_GetHashValue(hPtr);
		return TCL_OK;
	}

	/* Look for existing objectTable entry */
	hPtr = Tcl_FindHashEntry(&bindPtr->objectTable, (char *) &key);
	if (hPtr == NULL)
	{
		(*result) = NULL;
		return TCL_OK;
	}
	(*result) = (BindValue *) Tcl_GetHashValue(hPtr);
	return TCL_OK;
}

void QE_ExpandDouble(double number, Tcl_DString *result)
{
	char numStorage[TCL_DOUBLE_SPACE];

	Tcl_PrintDouble((Tcl_Interp *) NULL, number, numStorage);
	Tcl_DStringAppend(result, numStorage, -1);
/*	QE_ExpandString(numStorage, result); */
}

void QE_ExpandNumber(long number, Tcl_DString *result)
{
	char numStorage[TCL_INTEGER_SPACE];

	(void) sprintf(numStorage, "%ld", number);
	Tcl_DStringAppend(result, numStorage, -1);
/*	QE_ExpandString(numStorage, result); */
}

void QE_ExpandString(char *string, Tcl_DString *result)
{
	int length, spaceNeeded, cvtFlags;

	spaceNeeded = Tcl_ScanElement(string, &cvtFlags);
	length = Tcl_DStringLength(result);
	Tcl_DStringSetLength(result, length + spaceNeeded);
	spaceNeeded = Tcl_ConvertElement(string,
		Tcl_DStringValue(result) + length,
		cvtFlags | TCL_DONT_USE_BRACES);
	Tcl_DStringSetLength(result, length + spaceNeeded);
}

void QE_ExpandUnknown(char which, Tcl_DString *result)
{
	char string[2];

	(void) sprintf(string, "%c", which);
	QE_ExpandString(string, result);
}

void QE_ExpandEvent(QE_BindingTable bindingTable, int eventType, Tcl_DString *result)
{
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	EventInfo *eiPtr = FindEvent(bindPtr, eventType);

	if (eiPtr != NULL)
		QE_ExpandString((char *) eiPtr->name, result);
	else
		QE_ExpandString("unknown", result);
}

void QE_ExpandDetail(QE_BindingTable bindingTable, int event, int detail, Tcl_DString *result)
{
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	Detail *dPtr;

	if (detail == 0)
	{
		QE_ExpandString("", result);
		return;
	}

	dPtr = FindDetail(bindPtr, event, detail);
	if (dPtr != NULL)
		QE_ExpandString((char *) dPtr->name, result);
	else
		QE_ExpandString("unknown", result);
}

void QE_ExpandPattern(QE_BindingTable bindingTable, int eventType, int detail, Tcl_DString *result)
{
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	EventInfo *eiPtr = FindEvent(bindPtr, eventType);

	Tcl_DStringAppend(result, "<", 1);
	Tcl_DStringAppend(result, eiPtr ? eiPtr->name : "unknown", -1);
	if (detail)
	{
		Detail *dPtr = FindDetail(bindPtr, eventType, detail);
		Tcl_DStringAppend(result, "-", 1);
		Tcl_DStringAppend(result, dPtr ? dPtr->name : "unknown", -1);
	}
	Tcl_DStringAppend(result, ">", 1);
}

int QE_BindCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[])
{
	int objC = objc - objOffset;
	Tcl_Obj *CONST *objV = objv + objOffset;
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	Tk_Window tkwin = Tk_MainWindow(bindPtr->interp);
	ClientData object;
	char *string;

	if ((objC < 1) || (objC > 4))
	{
		Tcl_WrongNumArgs(bindPtr->interp, objOffset + 1, objv,
			"?object? ?pattern? ?script?");
		return TCL_ERROR;
	}

	if (objC == 1)
	{
		QE_GetAllObjects(bindingTable);
		return TCL_OK;
	}

	string = Tcl_GetString(objV[1]);

	if (string[0] == '.')
	{
		Tk_Window tkwin2;
		tkwin2 = Tk_NameToWindow(bindPtr->interp, string, tkwin);
		if (tkwin2 == NULL)
		{
			return TCL_ERROR;
		}
		object = (ClientData) Tk_GetUid(Tk_PathName(tkwin2));
	}
	else
	{
		object = (ClientData) Tk_GetUid(string);
	}

	if (objC == 4)
	{
		int append = 0;
		char *sequence = Tcl_GetString(objV[2]);
		char *script = Tcl_GetString(objV[3]);

		if (script[0] == 0)
		{
			return QE_DeleteBinding(bindingTable, object, sequence);
		}
		if (script[0] == '+')
		{
			script++;
			append = 1;
		}
		return QE_CreateBinding(bindingTable, object, sequence, script,
			append);
	}
	else if (objC == 3)
	{
		char *sequence = Tcl_GetString(objV[2]);

		return QE_GetBinding(bindingTable, object, sequence);
	}
	else
	{
		QE_GetAllBindings(bindingTable, object);
	}

	return TCL_OK;
}

int QE_UnbindCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[])
{
	int objC = objc - objOffset;
	Tcl_Obj *CONST *objV = objv + objOffset;
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	Tk_Window tkwin = Tk_MainWindow(bindPtr->interp);
	ClientData object;
	char *string, *sequence;

	if ((objC < 2) || (objC > 3))
	{
		Tcl_WrongNumArgs(bindPtr->interp, objOffset + 1, objv,
			"object ?pattern?");
		return TCL_ERROR;
	}

	string = Tcl_GetString(objV[1]);

	if (string[0] == '.')
	{
		Tk_Window tkwin2;
		tkwin2 = Tk_NameToWindow(bindPtr->interp, string, tkwin);
		if (tkwin2 == NULL)
		{
			return TCL_ERROR;
		}
		object = (ClientData) Tk_GetUid(Tk_PathName(tkwin2));
	}
	else
	{
		object = (ClientData) Tk_GetUid(string);
	}

	if (objC == 2)
	{
		return QE_DeleteBinding(bindingTable, object, NULL);
	}

	sequence = Tcl_GetString(objV[2]);
	return QE_DeleteBinding(bindingTable, object, sequence);
}

/*
 * qegenerate -- Generate events from scripts.
 * Usage: qegenerate $pattern ?$charMap? ?$percentsCommand?
 * Desciption: Scripts can generate "fake" quasi-events by providing
 * a quasi-event pattern and option field/value pairs.
 */

int
QE_GenerateCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[])
{
	int objC = objc - objOffset;
	Tcl_Obj *CONST *objV = objv + objOffset;
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	QE_Event fakeEvent;
	EventInfo *eiPtr;
	Detail *dPtr;
	GenerateData genData;
	GenerateField *fieldPtr;
	char *p, *t;
	int listObjc;
	int i;
	Tcl_Obj **listObjv;
	Pattern pats;
	int result;

	if (objC < 2 || objC > 4)
	{
		Tcl_WrongNumArgs(bindPtr->interp, objOffset + 1, objv,
			"pattern ?charMap? ?percentsCommand?");
		return TCL_ERROR;
	}

	p = Tcl_GetStringFromObj(objV[1], NULL);
	if (ParseEventDescription(bindPtr, p, &pats, &eiPtr, &dPtr) != TCL_OK)
		return TCL_ERROR;

	/* Can't generate an event without a detail */
	if ((dPtr == NULL) && (eiPtr->detailList != NULL))
	{
		Tcl_AppendResult(bindPtr->interp, "cannot generate \"", p,
			"\": missing detail", (char *) NULL);
		return TCL_ERROR;
	}

	if (objC >= 3)
	{
		if (Tcl_ListObjGetElements(bindPtr->interp, objV[2],
			&listObjc, &listObjv) != TCL_OK)
			return TCL_ERROR;

		if (listObjc & 1)
		{
			Tcl_AppendResult(bindPtr->interp,
				"char map must have even number of elements", (char *) NULL);
			return TCL_ERROR;
		}

		genData.count = listObjc / 2;
		genData.field = genData.staticField;
		if (genData.count > sizeof(genData.staticField) /
			sizeof(genData.staticField[0]))
		{
			genData.field = (GenerateField *) Tcl_Alloc(sizeof(GenerateField) *
				genData.count);
		}
		genData.count = 0;

		while (listObjc > 1)
		{
			int length;

			t = Tcl_GetStringFromObj(listObjv[0], &length);
			if (length != 1)
			{
				Tcl_AppendResult(bindPtr->interp, "invalid percent char \"", t,
					"\"", NULL);
				result = TCL_ERROR;
				goto done;
			}
			/* Duplicate %-chars result in last duplicate being used */
			fieldPtr = NULL;
			for (i = 0; i < genData.count; i++)
			{
				if (genData.field[i].which == t[0])
				{
					fieldPtr = &genData.field[i];
					break;
				}
			}
			if (fieldPtr == NULL)
				fieldPtr = &genData.field[genData.count++];
			fieldPtr->which = t[0];
			fieldPtr->string = Tcl_GetStringFromObj(listObjv[1], NULL);
			listObjv += 2;
			listObjc -= 2;
		}
	}
	else
	{
		genData.count = 0;
		genData.field = genData.staticField;
	}

	if (objC == 4)
	{
		genData.command = Tcl_GetString(objV[3]);
	}
	else
	{
		genData.command = NULL;
	}

	fakeEvent.type = pats.type;
	fakeEvent.detail = pats.detail;
	fakeEvent.clientData = NULL;

	result = BindEventWrapper(bindingTable, &fakeEvent, &genData);

done:
	if (genData.field != genData.staticField)
		Tcl_Free((char *) genData.field);
	return result;
}

#if BIND_ACTIVE

/* qeconfigure $win <Term-fresh> -active no */

int
QE_ConfigureCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[])
{
	int objC = objc - objOffset;
	Tcl_Obj *CONST *objV = objv + objOffset;
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	Tcl_Interp *interp = bindPtr->interp;
	Tk_Window tkwin = Tk_MainWindow(interp);
	static CONST char *configSwitch[] = {"-active", NULL};
	Tcl_Obj *CONST *objPtr;
	BindValue *valuePtr;
	char *t, *eventString;
	int index;
	ClientData object;

	if (objC < 3)
	{
		Tcl_WrongNumArgs(interp, objOffset + 1, objv,
			"object pattern ?option? ?value? ?option value ...?");
		return TCL_ERROR;
	}

	t = Tcl_GetStringFromObj(objV[1], NULL);
	eventString = Tcl_GetStringFromObj(objV[2], NULL);

	if (t[0] == '.')
	{
		Tk_Window tkwin2;
		tkwin2 = Tk_NameToWindow(interp, t, tkwin);
		if (tkwin2 == NULL)
		{
			return TCL_ERROR;
		}
		object = (ClientData) Tk_GetUid(Tk_PathName(tkwin2));
	}
	else
	{
		object = (ClientData) Tk_GetUid(t);
	}

	if (FindSequence(bindPtr, object, eventString, 0, NULL, &valuePtr) != TCL_OK)
		return TCL_ERROR;
	if (valuePtr == NULL)
		return TCL_OK;

	objPtr = objv + objOffset + 3;
	objc -= objOffset + 3;

	if (objc == 0)
	{
		Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj("-active", -1));
		Tcl_ListObjAppendElement(interp, listObj, Tcl_NewBooleanObj(valuePtr->active));
		Tcl_SetObjResult(interp, listObj);
		return TCL_OK;
	}

	if (objc == 1)
	{
		if (Tcl_GetIndexFromObj(interp, objPtr[0], configSwitch,
			"option", 0, &index) != TCL_OK)
		{
			return TCL_ERROR;
		}
		switch (index)
		{
			case 0: /* -active */
				Tcl_SetObjResult(interp, Tcl_NewBooleanObj(valuePtr->active));
				break;
		}
		return TCL_OK;
	}

	while (objc > 1)
	{
		if (Tcl_GetIndexFromObj(interp, objPtr[0], configSwitch,
			"option", 0, &index) != TCL_OK)
		{
			return TCL_ERROR;
		}
		switch (index)
		{
			case 0: /* -active */
				if (Tcl_GetBooleanFromObj(interp, objPtr[1], &valuePtr->active)
					!= TCL_OK)
				{
					return TCL_ERROR;
				}
				break;
		}
		objPtr += 2;
		objc -= 2;
	}

	return TCL_OK;
}

#endif /* BIND_ACTIVE */

/* Perform %-substitution with $charMap only */
static void Percents_CharMap(QE_ExpandArgs *args)
{
	GenerateData *gdPtr = (GenerateData *) args->clientData;
	int i;

	for (i = 0; i < gdPtr->count; i++)
	{
		GenerateField *gfPtr = &gdPtr->field[i];
		if (gfPtr->which == args->which)
		{
			QE_ExpandString(gfPtr->string, args->result);
			return;
		}
	}
	QE_ExpandUnknown(args->which, args->result);
}

/* Perform %-substitution by calling a Tcl command */
static void Percents_Command(QE_ExpandArgs *args)
{
	BindingTable *bindPtr = (BindingTable *) args->bindingTable;
	Tcl_Interp *interp = bindPtr->interp;
	PercentsData *data = (PercentsData *) args->clientData;
	GenerateData *gdPtr = data->gdPtr;
	EventInfo *eiPtr = data->eventPtr;
	Detail *dPtr = data->detailPtr;
	Tcl_DString command;
	Tcl_SavedResult state;
	int i;

	Tcl_DStringInit(&command);
	Tcl_DStringAppend(&command, data->command, -1);
	Tcl_DStringAppend(&command, " ", 1);
	Tcl_DStringAppend(&command, &args->which, 1);
	Tcl_DStringAppend(&command, " ", 1);
	Tcl_DStringAppend(&command, (char *) args->object, -1);
	Tcl_DStringAppend(&command, " ", 1);
	Tcl_DStringAppend(&command, eiPtr->name, -1);
	Tcl_DStringAppend(&command, " ", 1);
	if (dPtr != NULL)
		Tcl_DStringAppend(&command, dPtr->name, -1);
	else
		Tcl_DStringAppend(&command, "{}", -1);
	Tcl_DStringStartSublist(&command);

	for (i = 0; i < gdPtr->count; i++)
	{
		GenerateField *genField = &gdPtr->field[i];
		char string[2];
		string[0] = genField->which;
		string[1] = '\0';
		Tcl_DStringAppendElement(&command, string);
		Tcl_DStringAppendElement(&command, genField->string);
	}

	Tcl_DStringEndSublist(&command);
	Tcl_SaveResult(interp, &state);
	if (Tcl_EvalEx(interp, Tcl_DStringValue(&command),
		Tcl_DStringLength(&command), TCL_EVAL_GLOBAL) == TCL_OK)
	{
		QE_ExpandString(Tcl_GetStringFromObj(Tcl_GetObjResult(interp),
			NULL), args->result);
	}
	else
	{
		QE_ExpandUnknown(args->which, args->result);
		Tcl_AddErrorInfo(interp, "\n    (expanding percents)");
		Tcl_BackgroundError(interp);
	}
	Tcl_RestoreResult(interp, &state);
	Tcl_DStringFree(&command);
}

#if ALLOW_INSTALL

static int
QE_InstallCmd_New(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[])
{
	int objC = objc - objOffset;
	Tcl_Obj *CONST *objV = objv + objOffset;
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	char *pattern, *command = NULL;
	char eventName[FIELD_SIZE], detailName[FIELD_SIZE];
	int id, length;
	EventInfo *eiPtr;
	Detail *dPtr = NULL;
	Tcl_HashEntry *hPtr;

	if (objC < 2 || objC > 3)
	{
		Tcl_WrongNumArgs(bindPtr->interp, objOffset + 1, objv, "pattern ?percentsCommand?");
		return TCL_ERROR;
	}

	pattern = Tcl_GetString(objV[1]);
	if (ParseEventDescription1(bindPtr, pattern, eventName, detailName) != TCL_OK)
		return TCL_ERROR;
	hPtr = Tcl_FindHashEntry(&bindPtr->eventTableByName, eventName);

	/* The event is not defined */
	if (hPtr == NULL)
	{
		id = QE_InstallEvent(bindingTable, eventName, NULL);
		if (id == 0)
			return TCL_ERROR;

		/* Find the event we just installed */
		hPtr = Tcl_FindHashEntry(&bindPtr->eventTableByName, eventName);
		if (hPtr == NULL)
			return TCL_ERROR;
		eiPtr = (EventInfo *) Tcl_GetHashValue(hPtr);

		/* Mark as installed-by-script */
		eiPtr->dynamic = 1;
	}

	/* The event is already defined */
	else
	{
		eiPtr = (EventInfo *) Tcl_GetHashValue(hPtr);
	}

	if (detailName[0])
	{
		for (dPtr = eiPtr->detailList;
			dPtr != NULL;
			dPtr = dPtr->next)
		{
			if (strcmp(dPtr->name, detailName) == 0)
				break;
		}

		/* The detail is not defined */
		if (dPtr == NULL)
		{
			/* Define the new detail */
			id = QE_InstallDetail(bindingTable, detailName, eiPtr->type, NULL);
			if (id == 0)
				return TCL_ERROR;

			/* Get the detail we just defined */
			dPtr = FindDetail(bindPtr, eiPtr->type, id);
			if (dPtr == NULL)
				return TCL_ERROR;

			/* Mark as installed-by-script */
			dPtr->dynamic = 1;
		}
	}

	if (objC == 3)
		command = Tcl_GetStringFromObj(objV[2], &length);

	if (dPtr != NULL)
	{
		if (!dPtr->dynamic)
		{
			Tcl_AppendResult(bindPtr->interp, pattern, " is not dynamic",
				NULL);
			return TCL_ERROR;
		}
		if (command != NULL)
		{
			if (dPtr->command)
			{
				Tcl_Free(dPtr->command);
				dPtr->command = NULL;
			}
			if (length)
			{
				dPtr->command = Tcl_Alloc(length + 1);
				(void) strcpy(dPtr->command, command);
			}
		}
		if (dPtr->command)
			Tcl_SetResult(bindPtr->interp, dPtr->command, TCL_VOLATILE);
	}
	else
	{
		if (!eiPtr->dynamic)
		{
			Tcl_AppendResult(bindPtr->interp, pattern, " is not dynamic",
				NULL);
			return TCL_ERROR;
		}
		if (command != NULL)
		{
			if (eiPtr->command)
			{
				Tcl_Free(eiPtr->command);
				eiPtr->command = NULL;
			}
			if (length)
			{
				eiPtr->command = Tcl_Alloc(length + 1);
				(void) strcpy(eiPtr->command, command);
			}
		}
		if (eiPtr->command)
			Tcl_SetResult(bindPtr->interp, eiPtr->command, TCL_VOLATILE);
	}

	return TCL_OK;
}

static int
QE_InstallCmd_Old(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[])
{
	int objC = objc - objOffset;
	Tcl_Obj *CONST *objV = objv + objOffset;
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	static CONST char *commandOption[] = {"detail", "event", NULL};
	int index;

	if (objC < 2)
	{
		Tcl_WrongNumArgs(bindPtr->interp, objOffset + 1, objv, "option arg ...");
		return TCL_ERROR;
	}

	if (Tcl_GetIndexFromObj(bindPtr->interp, objV[1],
		commandOption, "option", 0, &index) != TCL_OK)
	{
		return TCL_ERROR;
	}
	switch (index)
	{
		case 0: /* detail */
		{
			char *eventName, *detailName, *command;
			int id, length;
			Detail *dPtr;
			EventInfo *eiPtr;
			Tcl_HashEntry *hPtr;

			if ((objC < 4) || (objC > 5))
			{
				Tcl_WrongNumArgs(bindPtr->interp, objOffset + 2, objv,
					"event detail ?percentsCommand?");
				return TCL_ERROR;
			}

			/* Find the event type */
			eventName = Tcl_GetStringFromObj(objV[2], NULL);
			hPtr = Tcl_FindHashEntry(&bindPtr->eventTableByName, eventName);
			if (hPtr == NULL)
			{
				Tcl_AppendResult(bindPtr->interp, "unknown event \"",
					eventName, "\"", NULL);
				return TCL_ERROR;
			}
			eiPtr = (EventInfo *) Tcl_GetHashValue(hPtr);

			/* Get the detail name */
			detailName = Tcl_GetStringFromObj(objV[3], NULL);

			/* Define the new detail */
			id = QE_InstallDetail(bindingTable, detailName, eiPtr->type, NULL);
			if (id == 0)
				return TCL_ERROR;

			/* Get the detail we just defined */
			dPtr = FindDetail(bindPtr, eiPtr->type, id);
			if (dPtr == NULL)
				return TCL_ERROR;
			dPtr->dynamic = 1;

			if (objC == 4)
				break;

			/* Set the Tcl command for this detail */
			command = Tcl_GetStringFromObj(objV[4], &length);
			if (length)
			{
				dPtr->command = Tcl_Alloc(length + 1);
				(void) strcpy(dPtr->command, command);
			}
			break;
		}

		case 1: /* event */
		{
			char *eventName, *command;
			int id, length;
			EventInfo *eiPtr;
			Tcl_HashEntry *hPtr;

			if (objC < 3 || objC > 4)
			{
				Tcl_WrongNumArgs(bindPtr->interp, objOffset + 2, objv,
					"name ?percentsCommand?");
				return TCL_ERROR;
			}

			eventName = Tcl_GetStringFromObj(objV[2], NULL);

			id = QE_InstallEvent(bindingTable, eventName, NULL);
			if (id == 0)
				return TCL_ERROR;

			/* Find the event we just installed */
			hPtr = Tcl_FindHashEntry(&bindPtr->eventTableByName, eventName);
			if (hPtr == NULL)
				return TCL_ERROR;
			eiPtr = (EventInfo *) Tcl_GetHashValue(hPtr);

			/* Mark as installed-by-script */
			eiPtr->dynamic = 1;

			if (objC == 3)
				break;

			/* Set the Tcl command for this event */
			command = Tcl_GetStringFromObj(objV[3], &length);
			if (length)
			{
				eiPtr->command = Tcl_Alloc(length + 1);
				(void) strcpy(eiPtr->command, command);
			}
			break;
		}
	}

	return TCL_OK;
}

int
QE_InstallCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[])
{
	int objC = objc - objOffset;
	Tcl_Obj *CONST *objV = objv + objOffset;
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	char *s;
	int length;

	if (objC < 2)
	{
		Tcl_WrongNumArgs(bindPtr->interp, objOffset + 1, objv, "pattern ?percentsCommand?");
		return TCL_ERROR;
	}

	s = Tcl_GetStringFromObj(objV[1], &length);
	if (length && (!strcmp(s, "detail") || !strcmp(s, "event")))
		return QE_InstallCmd_Old(bindingTable, objOffset, objc, objv);

	return QE_InstallCmd_New(bindingTable, objOffset, objc, objv);
}

static int
QE_UninstallCmd_New(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[])
{
	int objC = objc - objOffset;
	Tcl_Obj *CONST *objV = objv + objOffset;
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	char *pattern;
	Pattern pats;
	EventInfo *eiPtr;
	Detail *dPtr;

	if (objC != 2)
	{
		Tcl_WrongNumArgs(bindPtr->interp, objOffset + 1, objv, "pattern");
		return TCL_ERROR;
	}

	pattern = Tcl_GetString(objV[1]);
	if (ParseEventDescription(bindPtr, pattern, &pats, &eiPtr, &dPtr) != TCL_OK)
		return TCL_ERROR;

	if (dPtr != NULL)
	{
		if (!dPtr->dynamic)
		{
			Tcl_AppendResult(bindPtr->interp,
				"can't uninstall static detail \"", dPtr->name, "\"", NULL);
			return TCL_ERROR;
		}
		return QE_UninstallDetail(bindingTable, eiPtr->type, dPtr->code);
	}

	if (!eiPtr->dynamic)
	{
		Tcl_AppendResult(bindPtr->interp,
			"can't uninstall static event \"", eiPtr->name, "\"", NULL);
		return TCL_ERROR;
	}

	return QE_UninstallEvent(bindingTable, eiPtr->type);
}

static int
QE_UninstallCmd_Old(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[])
{
	int objC = objc - objOffset;
	Tcl_Obj *CONST *objV = objv + objOffset;
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	static CONST char *commandOption[] = {"detail", "event", NULL};
	int index;

	if (objC < 2)
	{
		Tcl_WrongNumArgs(bindPtr->interp, objOffset + 1, objv, "option arg ...");
		return TCL_ERROR;
	}

	if (Tcl_GetIndexFromObj(bindPtr->interp, objV[1],
		commandOption, "option", 0, &index) != TCL_OK)
	{
		return TCL_ERROR;
	}

	switch (index)
	{
		case 0: /* detail */
		{
			char *eventName, *detailName;
			Detail *dPtr;
			EventInfo *eiPtr;
			Tcl_HashEntry *hPtr;

			if (objC != 4)
			{
				Tcl_WrongNumArgs(bindPtr->interp, objOffset + 2, objv,
					"event detail");
				return TCL_ERROR;
			}

			/* Find the event type */
			eventName = Tcl_GetStringFromObj(objV[2], NULL);
			hPtr = Tcl_FindHashEntry(&bindPtr->eventTableByName, eventName);
			if (hPtr == NULL)
			{
				Tcl_AppendResult(bindPtr->interp, "unknown event \"",
					eventName, "\"", NULL);
				return TCL_ERROR;
			}
			eiPtr = (EventInfo *) Tcl_GetHashValue(hPtr);

			/* Get the detail name */
			detailName = Tcl_GetStringFromObj(objV[3], NULL);
			for (dPtr = eiPtr->detailList;
				dPtr != NULL;
				dPtr = dPtr->next)
			{
				if (strcmp(dPtr->name, detailName) == 0)
					break;
			}
			if (dPtr == NULL)
			{
				Tcl_AppendResult(bindPtr->interp,
					"unknown detail \"", detailName, "\" for event \"",
					eiPtr->name, "\"", NULL);
				return TCL_ERROR;
			}

			if (!dPtr->dynamic)
			{
				Tcl_AppendResult(bindPtr->interp,
					"can't uninstall static detail \"", detailName, "\"", NULL);
				return TCL_ERROR;
			}

			return QE_UninstallDetail(bindingTable, eiPtr->type, dPtr->code);
		}

		case 1: /* event */
		{
			Tcl_HashEntry *hPtr;
			EventInfo *eiPtr;
			char *eventName;

			if (objC != 3)
			{
				Tcl_WrongNumArgs(bindPtr->interp, objOffset + 2, objv,
					"name");
				return TCL_ERROR;
			}

			/* Find the event type */
			eventName = Tcl_GetStringFromObj(objV[2], NULL);
			hPtr = Tcl_FindHashEntry(&bindPtr->eventTableByName, eventName);
			if (hPtr == NULL)
			{
				Tcl_AppendResult(bindPtr->interp, "unknown event \"",
					eventName, "\"", NULL);
				return TCL_ERROR;
			}
			eiPtr = (EventInfo *) Tcl_GetHashValue(hPtr);

			if (!eiPtr->dynamic)
			{
				Tcl_AppendResult(bindPtr->interp,
					"can't uninstall static event \"", eventName, "\"", NULL);
				return TCL_ERROR;
			}

			return QE_UninstallEvent(bindingTable, eiPtr->type);
		}
	}

	return TCL_OK;
}

int QE_UninstallCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[])
{
	int objC = objc - objOffset;
	Tcl_Obj *CONST *objV = objv + objOffset;
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	char *s;
	int length;

	if (objC < 2)
	{
		Tcl_WrongNumArgs(bindPtr->interp, objOffset + 1, objv, "pattern");
		return TCL_ERROR;
	}

	s = Tcl_GetStringFromObj(objV[1], &length);
	if (length && (!strcmp(s, "detail") || !strcmp(s, "event")))
		return QE_UninstallCmd_Old(bindingTable, objOffset, objc, objv);

	return QE_UninstallCmd_New(bindingTable, objOffset, objc, objv);
}

static int
QE_LinkageCmd_New(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[])
{
	int objC = objc - objOffset;
	Tcl_Obj *CONST *objV = objv + objOffset;
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	char *pattern;
	Pattern pats;
	EventInfo *eiPtr;
	Detail *dPtr;

	if (objC != 2)
	{
		Tcl_WrongNumArgs(bindPtr->interp, objOffset + 1, objv, "pattern");
		return TCL_ERROR;
	}

	pattern = Tcl_GetString(objV[1]);
	if (ParseEventDescription(bindPtr, pattern, &pats, &eiPtr, &dPtr) != TCL_OK)
		return TCL_ERROR;

	if (dPtr != NULL)
	{
		Tcl_SetResult(bindPtr->interp, dPtr->dynamic ? "dynamic" : "static",
			TCL_STATIC);
		return TCL_OK;
	}

	Tcl_SetResult(bindPtr->interp, eiPtr->dynamic ? "dynamic" : "static",
		TCL_STATIC);
	return TCL_OK;
}

static int
QE_LinkageCmd_Old(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[])
{
	int objC = objc - objOffset;
	Tcl_Obj *CONST *objV = objv + objOffset;
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	char *eventName, *detailName;
	Detail *dPtr;
	EventInfo *eiPtr;
	Tcl_HashEntry *hPtr;

	if (objC < 2 || objC > 3)
	{
		Tcl_WrongNumArgs(bindPtr->interp, objOffset + 1, objv, "event ?detail?");
		return TCL_ERROR;
	}

	/* Find the event type */
	eventName = Tcl_GetStringFromObj(objV[1], NULL);
	hPtr = Tcl_FindHashEntry(&bindPtr->eventTableByName, eventName);
	if (hPtr == NULL)
	{
		Tcl_AppendResult(bindPtr->interp, "unknown event \"",
			eventName, "\"", NULL);
		return TCL_ERROR;
	}
	eiPtr = (EventInfo *) Tcl_GetHashValue(hPtr);

	if (objC == 2)
	{
		Tcl_SetResult(bindPtr->interp, eiPtr->dynamic ? "dynamic" : "static",
			TCL_STATIC);
		return TCL_OK;
	}

	/* Get the detail name */
	detailName = Tcl_GetStringFromObj(objV[2], NULL);
	for (dPtr = eiPtr->detailList;
		dPtr != NULL;
		dPtr = dPtr->next)
	{
		if (strcmp(dPtr->name, detailName) == 0)
			break;
	}
	if (dPtr == NULL)
	{
		Tcl_AppendResult(bindPtr->interp,
			"unknown detail \"", detailName, "\" for event \"",
			eiPtr->name, "\"", NULL);
		return TCL_ERROR;
	}

	Tcl_SetResult(bindPtr->interp, dPtr->dynamic ? "dynamic" : "static",
		TCL_STATIC);

	return TCL_OK;
}

int QE_LinkageCmd(QE_BindingTable bindingTable, int objOffset, int objc,
	Tcl_Obj *CONST objv[])
{
	int objC = objc - objOffset;
	Tcl_Obj *CONST *objV = objv + objOffset;
	BindingTable *bindPtr = (BindingTable *) bindingTable;
	char *s;
	int length;

	if (objC < 2)
	{
		Tcl_WrongNumArgs(bindPtr->interp, objOffset + 1, objv, "pattern");
		return TCL_ERROR;
	}

	s = Tcl_GetStringFromObj(objV[1], &length);
	if ((objC == 3) || (length && s[0] != '<'))
		return QE_LinkageCmd_Old(bindingTable, objOffset, objc, objv);

	return QE_LinkageCmd_New(bindingTable, objOffset, objc, objv);
}

#endif /* ALLOW_INSTALL */

