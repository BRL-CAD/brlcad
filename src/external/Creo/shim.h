/**
 *                          S H I M . H
 * BRL-CAD
 *
 * Published in 2023 by the United States Government.
 * This work is in the public domain.
 *
 */
/**
 * @file shim.h
 *
 * This file is provided as a compilation stub to ensure ongoing build testing
 * of CREO to BRL-CAD conversion source code.  This file does not necessarily
 * reflect the CREO api, its values, or type constructs and any similarity is
 * either coincidental or necessary for compilation.
 */

#include "common.h"

#include <stdio.h> /* for size_t */
#include "vmath.h"

#define ACCESS_AVAILABLE                         1
#define PRODIMTYPE_DIAMETER                      2
#define PRODIMTYPE_LINEAR                        3
#define PRODIMTYPE_RADIUS                        4
#define PROUIMESSAGE_INFO                        5
#define PRO_AXIS                                 6
#define PRO_B_FALSE                              7
#define PRO_B_TRUE                               8
#define PRO_CONTOUR_TRAV_INTERNAL                9
#define PRO_E_DIAMETER                          10
#define PRO_E_EXT_DEPTH_FROM_VALUE              11
#define PRO_E_EXT_DEPTH_TO_VALUE                12
#define PRO_E_HLE_ADD_CBORE                     13
#define PRO_E_HLE_ADD_CSINK                     14
#define PRO_E_HLE_CBOREDEPTH                    15
#define PRO_E_HLE_CBOREDIAM                     16
#define PRO_E_HLE_CSINKANGLE                    17
#define PRO_E_HLE_CSINKDIAM                     18
#define PRO_E_HLE_DEPTH                         19
#define PRO_E_HLE_DRILLANGLE                    20
#define PRO_E_HLE_DRILLDEPTH                    21
#define PRO_E_HLE_HOLEDIAM                      22
#define PRO_E_HLE_STAN_TYPE                     23
#define PRO_E_HLE_TYPE_NEW                      24
#define PRO_E_STD_EDGE_CHAMF_ANGLE              25
#define PRO_E_STD_EDGE_CHAMF_DIM                26
#define PRO_E_STD_EDGE_CHAMF_DIM1               27
#define PRO_E_STD_EDGE_CHAMF_DIM2               28
#define PRO_E_STD_EDGE_CHAMF_SCHEME             29
#define PRO_FEAT_ACTIVE                         30
#define PRO_FEAT_CHAMFER                        31
#define PRO_FEAT_COMPONENT                      32
#define PRO_FEAT_DOME                           33
#define PRO_FEAT_DOME2                          34
#define PRO_FEAT_DRAFT                          35
#define PRO_FEAT_EAR                            36
#define PRO_FEAT_FIRST_FEAT                     37
#define PRO_FEAT_FLANGE                         38
#define PRO_FEAT_HOLE                           39
#define PRO_FEAT_IMPORT                         40
#define PRO_FEAT_LOC_PUSH                       41
#define PRO_FEAT_MERGE                          42
#define PRO_FEAT_MOLD                           43
#define PRO_FEAT_NECK                           44
#define PRO_FEAT_OFFSET                         45
#define PRO_FEAT_PIPE                           46
#define PRO_FEAT_PROTRUSION                     47
#define PRO_FEAT_REPLACE_SURF                   48
#define PRO_FEAT_RIB                            49
#define PRO_FEAT_ROUND                          50
#define PRO_FEAT_SHAFT                          51
#define PRO_FEAT_SHELL                          52
#define PRO_FEAT_SUPPRESSED                     53
#define PRO_FEAT_UDF                            54
#define PRO_FEAT_UDF_THREAD                     55
#define PRO_HLE_ADD_CBORE                       56
#define PRO_HLE_ADD_CSINK                       57
#define PRO_HLE_NEW_TYPE_STANDARD               58
#define PRO_HLE_NEW_TYPE_STRAIGHT               59
#define PRO_HLE_STD_VAR_DEPTH                   60
#define PRO_MDLFILE_ASSEMBLY                    61
#define PRO_MDLFILE_PART                        62
#define PRO_MDL_ASSEMBLY                        63
#define PRO_MDL_PART                            64
#define PRO_NAME_SIZE                           65
#define PRO_PARAM_STRING                        66
#define PRO_SRF_B_SPL                           67
#define PRO_SURF_ORIENT_IN                      68
#define PRO_TK_BAD_CONTEXT                      69
#define PRO_TK_BAD_INPUTS                       70
#define PRO_TK_CONTINUE                         71
#define PRO_TK_GENERAL_ERROR                    72
#define PRO_TK_NO_ERROR                         73
#define PRO_TK_NOT_EXIST                        74
#define PRO_TK_NOT_VALID                        75
#define PRO_TK_SUPP_PARENTS                     76
#define PRO_UI_MESSAGE_OK                       77
#define PRO_UNITSYSTEM_MLT                      78
#define PRO_UNITTYPE_ANGLE                      79
#define PRO_UNITTYPE_FORCE                      80
#define PRO_UNITTYPE_MASS                       81
#define PRO_UNITTYPE_LENGTH                     82
#define PRO_UNITTYPE_TIME                       83
#define PRO_VALUE_UNUSED                        84
#define uiProe2ndImmediate                      85
#define PRO_PARAM_INTEGER                       86
#define PRO_PARAM_BOOLEAN                       87
#define PRO_PARAM_DOUBLE                        88
#define PRO_VALUE_TYPE_INT                      89
#define PRO_VALUE_TYPE_DOUBLE                   90
#define PRO_FEAT_EXTRACT_NO_OPTS                91

/* Eight definitions sourced from ProObjects.h */
#define ProMdlToSolid(mdl)    ((ProSolid) mdl)
#define ProMdlToAssembly(mdl) ((ProAssembly) mdl)
#define ProMdlToPart(mdl)     ((ProPart) mdl)
#define ProMdlTohandle(mdl)   ((Prohandle) mdl)
#define ProSolidToMdl(sld)    ((ProMdl) sld)
#define ProAssemblyToMdl(asm) ((ProMdl) asm)
#define ProPartToMdl(prt)     ((ProMdl) prt)
#define ProhandleToMdl(hdl)   ((ProMdl) hdl)

typedef int   ProBool;
typedef int   ProBoolean;
typedef char* ProCharLine;
typedef int   ProContourTraversal;
typedef int   ProDimensiontype;
typedef int   ProElemId;
typedef int   ProError;
typedef int   ProFeatStatus;
typedef int   ProFeattype;
typedef int   ProFeatureDeleteOptions;
typedef int   ProFeatureResumeOptions;
typedef int   ProMdlfileType;
typedef int   ProMdlType;
typedef int   ProParamvalueType;
typedef int   ProParamvalueValue;
typedef int   ProSrftype;
typedef int   ProSurfaceOrient;
typedef int   ProType;
typedef int   ProUIMessageButton;
typedef int   ProUnitsystemType;
typedef int   ProValueDataType;
typedef int   uiCmdAccessMode;
typedef int   uiCmdAccessState;
typedef int   uiCmdCmdId;
typedef int   uiCmdValue;
typedef void* ProAppData;
typedef void* ProArray;
typedef void* ProAsmcomppath;
typedef void* ProAssembly;
typedef void* ProContour;
typedef void* ProCurvedata;
typedef void* ProDimension;
typedef void* ProEdge;
typedef void* ProElement;
typedef void* ProElempath;
typedef void* ProGeomitem;
typedef void* ProGeomitemdata;
typedef void* ProMdl;
typedef void* ProModelitem;
typedef void* ProParamvalue;
typedef void* ProPart;
typedef void* ProSolid;
typedef void* ProSolidBody;
typedef void* ProSurface;
typedef void* ProSurfacedata;
typedef void* ProSurfaceshapedata;
typedef void* ProUnitsystem;
typedef void* ProValue;
typedef void* ProWVerstamp;
typedef wchar_t ProFileName[1];
typedef wchar_t ProLine[1];
typedef wchar_t ProName[1];

/* 
 * Definitions where we need to provide internal structure
 * (because our code is accessing it)
 */
typedef double Pro3dPnt[3];
typedef double ProVector[3];
typedef double ProUvParam[2];
typedef double ProMatrix[4][4];
typedef int    ProIdTable[2];
typedef int  __internal_facetset[3];
typedef struct pf {int id;} ProFeature;
typedef struct pmatl {wchar_t *matl_name;} ProMaterial;
typedef struct pmassp {double volume; double surface_area; double density; double mass;} ProMassProperty;
typedef struct proparam {wchar_t *id;} ProParameter;
typedef struct prouc {double scale;} ProUnitConversion;
typedef struct psap {double color_rgb[3];double transparency;double shininess;double diffuse;double highlite;} ProSurfaceAppearanceProps;
typedef struct pst {int n_facets;__internal_facetset *facets;point_t *vertices;point_t *normals;} ProSurfaceTessellationData;
typedef struct puitm {ProMdl owner; ProName name;} ProUnititem;
typedef struct pvdi {int i; int d;} __internal_ProValueData;
typedef struct pvdm {__internal_ProValueData v;} ProValueData;

/* Function types */
typedef int (*ProFeatureFilterAction)(ProFeature*,void*);
typedef int (*ProParameterAction)(ProParameter*,int,void*);
typedef int (*uiCmdCmdActFn)(int,int*,void*);

/* Functions */
extern "C" int ProArrayAlloc(int,int,int,void**);
extern "C" int ProArrayFree(void**);
extern "C" int ProArrayObjectAdd(ProArray*,int,int,void*);
extern "C" int ProArraySizeGet(ProArray,int*);
extern "C" int ProAsmcompMdlGet(ProFeature*,void**);
extern "C" int ProAsmcompMdlMdlnameGet(ProFeature*,int*,wchar_t*);
extern "C" int ProAsmcomppathInit(void*,ProIdTable,int,void**);
extern "C" int ProAsmcomppathTrfGet(void**,int,ProMatrix);
extern "C" int ProAssemblyIsExploded(void*,int*);
extern "C" int ProBsplinesrfdataGet(void**,int[2],double**,double**,double**,ProVector**,int*,int*,int*);
extern "C" int ProCmdActionAdd(const char*,int(*)(int,int*,void*),int,int(*)(int),int,int,int*);
extern "C" int ProDimensionTypeGet(void**,int*);
extern "C" int ProDimensionValueGet(void**,double *);
extern "C" int ProElementDoubleGet(void*,void*,double*);
extern "C" int ProElementFree(void**);
extern "C" int ProElementIdGet(void*,int*);
extern "C" int ProElementIntegerGet(void*,void*,int*);
extern "C" int ProElementValueGet(void*,void**);
extern "C" int ProElementValuetypeGet(void*,int*);
extern "C" int ProElemtreeElementVisit(void*,void*,int (*)(void*,void*,void*,void*),int (*)(void*,void*,void*,void*),void*);
extern "C" int ProFeatureChildrenGet(void*,int**,int*);
extern "C" int ProFeatureDimensionVisit(ProFeature *,int (*)(void* *,int ,void*),int (*)(void**,void*),void*);
extern "C" int ProFeatureElemtreeCreate(ProFeature *,void**);
extern "C" int ProFeatureElemtreeExtract(ProFeature *,void*,int,void**);
extern "C" int ProFeatureGeomitemVisit(ProFeature *,int,int (*)(void**,int,void*),int (*)(void**,void*),void*);
extern "C" int ProFeatureInit(void*,int,ProFeature*);
extern "C" int ProFeatureStatusGet(ProFeature*,ProFeatStatus *);
extern "C" int ProFeatureTypeGet(ProFeature*,int*);
extern "C" int ProFeatureWithoptionsResume(void*,int*,void*,int);
extern "C" int ProFeatureWithoptionsSuppress(void*,int*,void*,int);
extern "C" int ProGeomitemdataGet(void**,void***);
extern "C" int ProLinedataGet(void**,Pro3dPnt,Pro3dPnt);
extern "C" int ProMaterialCurrentGet(void*,ProMaterial*);
extern "C" int ProMdlCurrentGet(void**);
extern "C" int ProMdlMdlnameGet(void*,wchar_t*);
extern "C" int ProMdlPrincipalunitsystemGet(void*,void**);
extern "C" int ProMdlToModelitem(void*,void**);
extern "C" int ProMdlTypeGet(void*,int*);
extern "C" int ProMdlVerstampGet(void*, void**);
extern "C" int ProMdlnameInit(void*,int,void*);
extern "C" int ProMenubarmenuPushbuttonAdd(const char*,const char*,const char*,const char*,const char*,int,int,wchar_t*);
extern "C" int ProMessageDisplay(wchar_t *,const char *,const char *);
extern "C" int ProParameterInit(void *,wchar_t *,void *);
extern "C" int ProParameterValueGet(ProParameter *,void *);
extern "C" int ProParameterValueWithUnitsGet(ProParameter *,void *,void *);
extern "C" int ProParameterVisit(void **, void*, int (*)(ProParameter*, int, void*), void *);
extern "C" int ProParamvalueTypeGet(void **,void *);
extern "C" int ProParamvalueValueGet(void **,int,void *);
extern "C" int ProPartDensityGet(void*,double*);
extern "C" int ProPartTessellate(void*,double,double,int,ProSurfaceTessellationData**);
extern "C" int ProSolidBodiesCollect(void*, ProSolidBody**);
extern "C" int ProSolidBodySurfaceVisit(void*,int(*)(void*,int,void*),void*);
extern "C" int ProSolidFeatVisit(void*,int (*)(ProFeature*,int,void*),int (*)(ProFeature*,void*),void*);
extern "C" int ProSolidMassPropertyGet(void*,void*,ProMassProperty*);
extern "C" int ProSolidOutlineGet(void*, Pro3dPnt*);
extern "C" int ProStringVerstampGet(void*, void**);
extern "C" int ProStringarrayFree(char**,int);
extern "C" int ProSurfaceSideAppearancepropsGet(void**,int,ProSurfaceAppearanceProps*);
extern "C" int ProUICheckbuttonActivateActionSet(const char*,const char*,void(*)(char*,char*,void*),void*);
extern "C" int ProUICheckbuttonGetState(const char*,const char*,int*);
extern "C" int ProUIDialogActivate(const char*, int*);
extern "C" int ProUIDialogCreate(const char *,const char *);
extern "C" int ProUIDialogDestroy(const char *);
extern "C" int ProUIInputpanelEditable(char*,const char*);
extern "C" int ProUIInputpanelReadOnly(char*,const char*);
extern "C" int ProUIInputpanelValueGet(const char *,const char *,wchar_t**);
extern "C" int ProUIInputpanelValueSet(const char *,const char *,wchar_t*);
extern "C" int ProUILabelTextSet(const char *,const char *,wchar_t *);
extern "C" int ProUIPushbuttonActivateActionSet(const char *,const char *,void (*)(char*,char *,void*),void*);
extern "C" int ProUIRadiogroupSelectednamesGet(const char*,const char*,int*,char***);
extern "C" int ProUITextareaValueSet(const char *,const char *,wchar_t*);
extern "C" int ProUnitConversionCalculate(ProUnititem*,ProUnititem*,ProUnitConversion*);
extern "C" int ProUnitsystemTypeGet(void**,ProUnitsystemType*);
extern "C" int ProUnitsystemUnitGet(void**,int,ProUnititem*);
extern "C" int ProUtilCollectParameters(void**,void**);
extern "C" int ProValueDataGet(void*,ProValueData*);
extern "C" int ProVerstampEqual(void*, void*);
extern "C" int ProVerstampStringFree(char**);
extern "C" int ProVerstampStringGet(void*, char**);
extern "C" int ProWcharSizeVerify(size_t,int*);
extern "C" int ProWindowRefresh(int);
extern "C" int ProWstringFree(wchar_t*);
extern "C" void ProAssemblyUnexplode(void*);
extern "C" void ProContourEdgeVisit(void*,void*,int(*)(void*,int,void*),int(*)(void*,void*),void*);
extern "C" void ProContourTraversalGet(void*,int*);
extern "C" void ProMdlIsSkeleton(void*, int*);
extern "C" void ProMessageClear();
extern "C" void ProPartTessellationFree(ProSurfaceTessellationData**);
extern "C" void ProStringToWstring(wchar_t*,const char*);
extern "C" void ProSurfaceContourVisit(void*,int(*)(void*,int,void*),int(*)(void*,void*), void*);
extern "C" void ProSurfaceIdGet(void*,int*);
extern "C" void ProSurfaceToNURBS(void*,void***);
extern "C" void ProSurfacedataGet(void*,int*,double*,double*,int*,void**,int*);
extern "C" void ProUIInputpanelMaxlenSet(const char*,const char*,int);
extern "C" void ProUIMessageDialogDisplay(int,const wchar_t *,const wchar_t *,ProUIMessageButton*,int,ProUIMessageButton*);
extern "C" void ProUnitInit(void*,const wchar_t *,ProUnititem*);
extern "C" void ProWstringToString(char*,wchar_t*);
extern "C" void** PRO_CURVE_DATA(void*);

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
