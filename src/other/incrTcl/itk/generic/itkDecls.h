
/* !BEGIN!: Do not edit below this line. */

#define ITK_STUBS_EPOCH 0
#define ITK_STUBS_REVISION 17

#if !defined(USE_ITK_STUBS)

/*
 * Exported function declarations:
 */

/* 0 */
ITKAPI int		Itk_Init (Tcl_Interp * interp);
/* 1 */
ITKAPI int		Itk_SafeInit (Tcl_Interp * interp);
/* Slot 2 is reserved */
/* Slot 3 is reserved */
/* Slot 4 is reserved */
/* Slot 5 is reserved */
/* Slot 6 is reserved */
/* Slot 7 is reserved */
/* Slot 8 is reserved */
/* Slot 9 is reserved */
/* Slot 10 is reserved */
/* Slot 11 is reserved */
/* Slot 12 is reserved */
/* 13 */
ITKAPI int		Itk_ArchetypeInit (Tcl_Interp* interp);

#endif /* !defined(USE_ITK_STUBS) */

typedef struct ItkStubHooks {
    struct ItkIntStubs *itkIntStubs;
} ItkStubHooks;

typedef struct ItkStubs {
    int magic;
    int epoch;
    int revision;
    struct ItkStubHooks *hooks;

    int (*itk_Init) (Tcl_Interp * interp); /* 0 */
    int (*itk_SafeInit) (Tcl_Interp * interp); /* 1 */
    void (*reserved2)(void);
    void (*reserved3)(void);
    void (*reserved4)(void);
    void (*reserved5)(void);
    void (*reserved6)(void);
    void (*reserved7)(void);
    void (*reserved8)(void);
    void (*reserved9)(void);
    void (*reserved10)(void);
    void (*reserved11)(void);
    void (*reserved12)(void);
    int (*itk_ArchetypeInit) (Tcl_Interp* interp); /* 13 */
} ItkStubs;

#ifdef __cplusplus
extern "C" {
#endif
extern const ItkStubs *itkStubsPtr;
#ifdef __cplusplus
}
#endif

#if defined(USE_ITK_STUBS)

/*
 * Inline function declarations:
 */

#ifndef Itk_Init
#define Itk_Init \
	(itkStubsPtr->itk_Init) /* 0 */
#endif
#ifndef Itk_SafeInit
#define Itk_SafeInit \
	(itkStubsPtr->itk_SafeInit) /* 1 */
#endif
/* Slot 2 is reserved */
/* Slot 3 is reserved */
/* Slot 4 is reserved */
/* Slot 5 is reserved */
/* Slot 6 is reserved */
/* Slot 7 is reserved */
/* Slot 8 is reserved */
/* Slot 9 is reserved */
/* Slot 10 is reserved */
/* Slot 11 is reserved */
/* Slot 12 is reserved */
#ifndef Itk_ArchetypeInit
#define Itk_ArchetypeInit \
	(itkStubsPtr->itk_ArchetypeInit) /* 13 */
#endif

#endif /* defined(USE_ITK_STUBS) */

/* !END!: Do not edit above this line. */
