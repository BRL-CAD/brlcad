#ifdef offsetof
#define Blt_Offset(type, field) ((int) offsetof(type, field))
#else
#define Blt_Offset(type, field) ((int) ((char *) &((type *) 0)->field))
#endif

typedef int (Blt_SwitchParseProc) _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, char *switchName, char *value, char *record, 
	int offset));

typedef void (Blt_SwitchFreeProc) _ANSI_ARGS_((char *ptr));

typedef struct {
    Blt_SwitchParseProc *parseProc;	/* Procedure to parse a switch value
					 * and store it in its converted 
					 * form in the data record. */
    Blt_SwitchFreeProc *freeProc;	/* Procedure to free a switch. */
    ClientData clientData;		/* Arbitrary one-word value
					 * used by switch parser,
					 * passed to parseProc. */
} Blt_SwitchCustom;

/*
 * Type values for Blt_SwitchSpec structures.  See the user
 * documentation for details.
 */
typedef enum {
    BLT_SWITCH_BOOLEAN, BLT_SWITCH_INT, BLT_SWITCH_INT_POSITIVE,
    BLT_SWITCH_INT_NONNEGATIVE, BLT_SWITCH_DOUBLE, BLT_SWITCH_STRING, 
    BLT_SWITCH_LIST, BLT_SWITCH_FLAG, BLT_SWITCH_VALUE, BLT_SWITCH_CUSTOM, 
    BLT_SWITCH_END
} Blt_SwitchTypes;

typedef struct {
    Blt_SwitchTypes type;	/* Type of option, such as BLT_SWITCH_COLOR;
				 * see definitions below.  Last option in
				 * table must have type BLT_SWITCH_END. */
    char *switchName;		/* Switch used to specify option in argv.
				 * NULL means this spec is part of a group. */
    int offset;			/* Where in widget record to store value;
				 * use Blt_Offset macro to generate values
				 * for this. */
    int flags;			/* Any combination of the values defined
				 * below. */
    Blt_SwitchCustom *customPtr; /* If type is BLT_SWITCH_CUSTOM then this is
				 * a pointer to info about how to parse and
				 * print the option.  Otherwise it is
				 * irrelevant. */
    int value;
} Blt_SwitchSpec;

#define BLT_SWITCH_ARGV_ONLY		(1<<0)
#define BLT_SWITCH_OBJV_ONLY		(1<<0)
#define BLT_SWITCH_ARGV_PARTIAL		(1<<1)
#define BLT_SWITCH_OBJV_PARTIAL		(1<<1)
/*
 * Possible flag values for Blt_SwitchSpec structures.  Any bits at
 * or above BLT_SWITCH_USER_BIT may be used by clients for selecting
 * certain entries.  
 */
#define BLT_SWITCH_NULL_OK		(1<<0)
#define BLT_SWITCH_DONT_SET_DEFAULT	(1<<3)
#define BLT_SWITCH_SPECIFIED		(1<<4)
#define BLT_SWITCH_USER_BIT		(1<<8)

extern int Blt_ProcessSwitches _ANSI_ARGS_((Tcl_Interp *interp, 
	Blt_SwitchSpec *specs, int argc, char **argv, char *record, int flags));

extern void Blt_FreeSwitches _ANSI_ARGS_((Blt_SwitchSpec *specs, char *record, 
	int flags));

extern int Blt_SwitchChanged _ANSI_ARGS_(TCL_VARARGS(Blt_SwitchSpec *, specs));

#if (TCL_VERSION_NUMBER >= _VERSION(8,0,0)) 
extern int Blt_ProcessObjSwitches _ANSI_ARGS_((Tcl_Interp *interp, 
	Blt_SwitchSpec *specPtr, int objc, Tcl_Obj *CONST *objv, char *record, 
	int flags));
#endif
