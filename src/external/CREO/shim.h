/* This file is provided as a compilation stub to ensure ongoing build testing
 * of CREO to BRL-CAD conversion source code.  This file does not necessarily
 * reflect the CREO api, its values, or type constructs and any similarity is
 * either coincidental or necessary for compilation.
 */

#include "vmath.h"

#define ACCESS_AVAILABLE			1
#define PRODIMTYPE_DIAMETER			2
#define PRODIMTYPE_LINEAR			3
#define PRODIMTYPE_RADIUS			4
#define PROUIMESSAGE_INFO			5
#define PRO_AXIS				6
#define PRO_B_FALSE				7
#define PRO_B_TRUE				8
#define PRO_E_DIAMETER				9
#define PRO_E_EXT_DEPTH_FROM_VALUE		10
#define PRO_E_EXT_DEPTH_TO_VALUE		11
#define PRO_E_HLE_ADD_CBORE			12
#define PRO_E_HLE_ADD_CSINK			13
#define PRO_E_HLE_CBOREDEPTH			14
#define PRO_E_HLE_CBOREDIAM			15
#define PRO_E_HLE_CSINKANGLE			16
#define PRO_E_HLE_CSINKDIAM			17
#define PRO_E_HLE_DEPTH				18
#define PRO_E_HLE_DRILLANGLE			19
#define PRO_E_HLE_DRILLDEPTH			20
#define PRO_E_HLE_HOLEDIAM			21
#define PRO_E_HLE_STAN_TYPE			22
#define PRO_E_HLE_TYPE_NEW			23
#define PRO_E_STD_EDGE_CHAMF_ANGLE		24
#define PRO_E_STD_EDGE_CHAMF_DIM		25
#define PRO_E_STD_EDGE_CHAMF_DIM1		26
#define PRO_E_STD_EDGE_CHAMF_DIM2		27
#define PRO_E_STD_EDGE_CHAMF_SCHEME		28
#define PRO_FEAT_ACTIVE				29
#define PRO_FEAT_CHAMFER			30
#define PRO_FEAT_COMPONENT			31
#define PRO_FEAT_DOME				32
#define PRO_FEAT_DOME2				33
#define PRO_FEAT_DRAFT				34
#define PRO_FEAT_EAR				35
#define PRO_FEAT_FLANGE				36
#define PRO_FEAT_HOLE				37
#define PRO_FEAT_IMPORT				38
#define PRO_FEAT_LOC_PUSH			39
#define PRO_FEAT_MERGE				40
#define PRO_FEAT_MOLD				41
#define PRO_FEAT_NECK				42
#define PRO_FEAT_OFFSET				43
#define PRO_FEAT_PIPE				44
#define PRO_FEAT_PROTRUSION			45
#define PRO_FEAT_REPLACE_SURF			46
#define PRO_FEAT_RESUME_INCLUDE_PARENTS		47
#define PRO_FEAT_RIB				48
#define PRO_FEAT_ROUND				49
#define PRO_FEAT_SHAFT				50
#define PRO_FEAT_SHELL				51
#define PRO_FEAT_SUPPRESSED			52
#define PRO_FEAT_UDF				53
#define PRO_FEAT_UDF_THREAD			54
#define PRO_HLE_ADD_CBORE			55
#define PRO_HLE_ADD_CSINK			56
#define PRO_HLE_NEW_TYPE_STANDARD		57
#define PRO_HLE_NEW_TYPE_STRAIGHT		58
#define PRO_HLE_STD_VAR_DEPTH			59
#define PRO_MDLFILE_ASSEMBLY			60
#define PRO_MDLFILE_PART			61
#define PRO_MDL_ASSEMBLY			62
#define PRO_MDL_PART				63
#define PRO_PARAM_STRING			64
#define PRO_TK_BAD_CONTEXT			65
#define PRO_TK_BAD_INPUTS			66
#define PRO_TK_CONTINUE				67
#define PRO_TK_GENERAL_ERROR			68
#define PRO_TK_NO_ERROR				69
#define PRO_TK_NOT_EXIST			70
#define PRO_TK_SUPP_PARENTS			71
#define PRO_UI_MESSAGE_OK			72
#define PRO_UNITTYPE_LENGTH			73
#define PRO_VALUE_UNUSED			74
#define uiProe2ndImmediate                      75

typedef bool ProBool;
typedef bool ProBoolean;
typedef char* ProCharLine;
typedef int ProDimensiontype;
typedef int ProElemId;
typedef int ProError;
typedef int ProFeatStatus;
typedef int ProFeattype;
typedef int ProFeatureDeleteOptions;
typedef int ProFeatureResumeOptions;
typedef int ProMdlType;
typedef int ProParamvalueType;
typedef int ProParamvalueValue;
typedef int ProType;
typedef int ProUIMessageButton;
typedef int uiCmdAccessMode;
typedef int uiCmdAccessState;
typedef int uiCmdCmdId;
typedef int uiCmdValue;
typedef void* ProAppData;
typedef void* ProArray;
typedef void* ProAsmcomppath;
typedef void* ProAssembly;
typedef void* ProCurvedata;
typedef void* ProDimension;
typedef void* ProElement;
typedef void* ProElempath;
typedef void* ProGeomitem;
typedef void* ProGeomitemdata;
typedef void* ProMdl;
typedef void* ProModelitem;
typedef void* ProParamvalue;
typedef void* ProPart;
typedef void* ProSolid;
typedef void* ProUnititem;
typedef void* ProUnitsystem;
typedef void* ProValue;
typedef void* ProWVerstamp;
typedef wchar_t* ProFileName;
typedef wchar_t* ProLine;

/* Definitions where we need to provide internal structure
 * (because our code is accessing it) */
typedef double Pro3dPnt[3];
typedef double ProMatrix[4][4];
typedef int ProIdTable[2];
typedef int __internal_facetset[3];
typedef struct pf {int id;} ProFeature;
typedef struct pmatp {double mass_density;} ProMaterialProps;
typedef struct pmp {int density; int mass; int volume;} ProMassProperty;
typedef struct proparam {wchar_t *id;} ProParameter;
typedef struct prouc {double scale;} ProUnitConversion;
typedef struct psap {double color_rgb[3];double transparency;double shininess;double diffuse;double highlite;} ProSurfaceAppearanceProps;
typedef struct pst {int n_facets;__internal_facetset *facets;point_t *vertices;point_t *normals;} ProSurfaceTessellationData;
typedef struct pvdi {int i; int d;} __internal_ProValueData;
typedef struct pvdm {__internal_ProValueData v;} ProValueData;

/* Function types */
typedef int (*ProFeatureFilterAction)(ProFeature*,void*);
typedef int (*uiCmdCmdActFn)(int,int*,void*);

/* Functions */
int ProArrayAlloc(int,int,int,void**);
int ProArrayFree(void**);
int ProArraySizeGet(ProArray,int*);
int ProAsmcompMdlGet(ProFeature*,void**);
int ProAsmcompMdlNameGet(ProFeature*,int*,wchar_t*);
int ProAsmcompMdlNameGet(ProFeature*,int*,wchar_t*);
int ProAsmcomppathInit(void*,ProIdTable,int,void**);
int ProAsmcomppathTrfGet(void**,bool,ProMatrix);
int ProAssemblyIsExploded(void*,bool*);
int ProCmdActionAdd(const char*,int(*)(int,int*,void*),int,int(*)(int),bool,bool,int*);
int ProDimensionTypeGet(void**,int*);
int ProDimensionValueGet(void**,double *);
int ProElementFree(void**);
int ProElementIdGet(void*,int*);
int ProElementValueGet(void*,void**);
int ProElemtreeElementVisit(void*,void*,int (*)(void*,void*,void*,void*),int (*)(void*,void*,void*,void*),void*);
int ProFeatureDimensionVisit(ProFeature *,int (*)(void* *,int ,void*),int (*)(void**,void*),void*);
int ProFeatureElemtreeCreate(ProFeature *,void**);
int ProFeatureGeomitemVisit(ProFeature *,int,int (*)(void**,int,void*),int (*)(void**,void*),void*);
int ProFeatureInit(void*,int,ProFeature*);
int ProFeatureResume(void*,int*,int,void*,int);
int ProFeatureStatusGet(ProFeature*,ProFeatStatus *);
int ProFeatureSuppress(void*,int*,int,void*,int);
int ProFeatureTypeGet(ProFeature*,int*);
int ProGeomitemdataGet(void**,void***);
int ProLinedataGet(void**,Pro3dPnt,Pro3dPnt);
int ProMdlCurrentGet(void**);
int ProMdlNameGet(void*,wchar_t*);
int ProMdlToModelitem(void*,void**);
int ProMdlTypeGet(void*,int*);
int ProMdlVerstampGet(void*, void**);
int ProMdlnameInit(void*,int,void*);
int ProMenubarmenuPushbuttonAdd(const char*,const char*,const char*,const char*,const char*,bool,int,wchar_t*);
int ProMessageDisplay(wchar_t *,const char *,const char *);
int ProParameterInit(void *,wchar_t *,void *);
int ProParameterValueGet(ProParameter *,void *);
int ProParameterVisit (void **,	void*,int (*)(ProParameter*,int,void*),void *);
int ProParamvalueTypeGet(void **,void *);
int ProParamvalueValueGet(void **,int,wchar_t *);
int ProPartMaterialNameGet(void*,wchar_t *);
int ProPartMaterialdataGet(void*,wchar_t*,ProMaterialProps*);
int ProPartTessellate(void*,double,double,bool,ProSurfaceTessellationData**);
int ProSolidFeatVisit(void*,int (*)(ProFeature*,int,void*),int (*)(ProFeature*,void*),void*);
int ProSolidMassPropertyGet(void*,void*,ProMassProperty*);
int ProSolidOutlineGet(void*, Pro3dPnt*);
int ProStringarrayFree(char**,int);
int ProStringVerstampGet(void*, void**);
int ProSurfaceAppearancepropsGet(void**,ProSurfaceAppearanceProps*);
int ProUICheckbuttonActivateActionSet(const char*,const char*,void(*)(char*,char*,void*),void*);
int ProUICheckbuttonGetState(const char*,const char*,bool*);
int ProUIDialogActivate(const char*,int*);
int ProUIDialogCreate(const char *,const char *);
int ProUIDialogDestroy(const char *);
int ProUIInputpanelEditable(char*,const char*);
int ProUIInputpanelReadOnly(char*,const char*);
int ProUIInputpanelValueGet(const char *,const char *,wchar_t**);
int ProUILabelTextSet(const char *,const char *,wchar_t *);
int ProUIPushbuttonActivateActionSet(const char *,const char *,void (*)(char*,char *,void*),void*);
int ProUIRadiogroupSelectednamesGet(const char*,const char*,int*,char***);
int ProUITextareaValueSet(const char *,const char *,wchar_t*);
int ProValueDataGet(void*,ProValueData*);
int ProVerstampEqual(void*, void*);
int ProVerstampStringGet(void*, char**);
int ProVerstampStringFree(char**);
int ProWcharSizeVerify(size_t,int*);
int ProWindowRefresh(int);
int ProWstringFree(wchar_t*);
void ProAssemblyUnexplode(void*);
void ProMdlMdlnameGet(void*,wchar_t*);
void ProMdlPrincipalunitsystemGet(void*,void**);
void ProMessageClear();
void ProPartTessellationFree(ProSurfaceTessellationData**);
void ProStringToWstring(wchar_t*,const char*);
void ProUIMessageDialogDisplay(int,const wchar_t *,const wchar_t *,ProUIMessageButton*,int,ProUIMessageButton*);
void ProUnitConversionGet(void**,ProUnitConversion*,void**);
void ProUnitInit(void*,const wchar_t *,void**);
void ProUnitsystemUnitGet(void**,int,void**);
void ProWstringToString(char*,wchar_t*);
void* ProMdlToPart(void*);
void* ProMdlToSolid(void*);
void** PRO_CURVE_DATA(void*);
