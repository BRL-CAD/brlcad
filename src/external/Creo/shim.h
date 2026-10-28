/**
 *                          S H I M . H
 * BRL-CAD
 *
 * Published in 2024 by the United States Government.
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
#define PRO_AXIS                                 5
#define PRO_B_FALSE                              6
#define PRO_B_TRUE                               7
#define PRO_E_DIAMETER                           8
#define PRO_E_HLE_ADD_CBORE                      9
#define PRO_E_HLE_ADD_CSINK                     10
#define PRO_E_HLE_CBOREDEPTH                    11
#define PRO_E_HLE_CBOREDIAM                     12
#define PRO_E_HLE_CSINKANGLE                    13
#define PRO_E_HLE_CSINKDIAM                     14
#define PRO_E_HLE_DEPTH                         15
#define PRO_E_HLE_DRILLANGLE                    16
#define PRO_E_HLE_DRILLDEPTH                    17
#define PRO_E_HLE_HOLEDIAM                      18
#define PRO_E_HLE_TYPE_NEW                      19
#define PRO_FEAT_ACTIVE                         20
#define PRO_FEAT_CHAMFER                        21
#define PRO_FEAT_COMPONENT                      22
#define PRO_FEAT_DOME                           23
#define PRO_FEAT_DOME2                          24
#define PRO_FEAT_DRAFT                          25
#define PRO_FEAT_EAR                            26
#define PRO_FEAT_FIRST_FEAT                     27
#define PRO_FEAT_FLANGE                         28
#define PRO_FEAT_HOLE                           29
#define PRO_FEAT_IMPORT                         30
#define PRO_FEAT_LOC_PUSH                       31
#define PRO_FEAT_MERGE                          32
#define PRO_FEAT_MOLD                           33
#define PRO_FEAT_NECK                           34
#define PRO_FEAT_OFFSET                         35
#define PRO_FEAT_PIPE                           36
#define PRO_FEAT_PROTRUSION                     37
#define PRO_FEAT_REPLACE_SURF                   38
#define PRO_FEAT_RIB                            39
#define PRO_FEAT_ROUND                          40
#define PRO_FEAT_SHAFT                          41
#define PRO_FEAT_SHELL                          42
#define PRO_FEAT_SUPPRESSED                     43
#define PRO_FEAT_UDF                            44
#define PRO_FEAT_UDF_THREAD                     45
#define PRO_HLE_ADD_CBORE                       46
#define PRO_HLE_ADD_CSINK                       47
#define PRO_HLE_NEW_TYPE_STANDARD               48
#define PRO_HLE_NEW_TYPE_STRAIGHT               49
#define PRO_HLE_STD_VAR_DEPTH                   50
#define PRO_MDL_ASSEMBLY                        51
#define PRO_MDL_PART                            52
#define PRO_NAME_SIZE                           53
#define PRO_PARAM_STRING                        54
#define PRO_TK_BAD_CONTEXT                      55
#define PRO_TK_BAD_INPUTS                       56
#define PRO_TK_CONTINUE                         57
#define PRO_TK_GENERAL_ERROR                    58
#define PRO_TK_NO_ERROR                         59
#define PRO_UNITTYPE_LENGTH                     60
#define PRO_VALUE_UNUSED                        61
#define uiProe2ndImmediate                      62
#define PRO_PARAM_INTEGER                       63
#define PRO_PARAM_BOOLEAN                       64
#define PRO_PARAM_DOUBLE                        65
#define PRO_VALUE_TYPE_INT                      66
#define PRO_VALUE_TYPE_DOUBLE                   67
#define PRO_FEAT_EXTRACT_NO_OPTS                68
#define PRO_FEAT_RESUME_INCLUDE_PARENTS         69
#define PRO_REGEN_NO_FLAGS                      70
#define PRO_FEAT_DELETE_NO_OPTS                 71

/* Conversions used by the migrated frontend. */
#define ProMdlToSolid(mdl) ((ProSolid) mdl)
#define ProMdlToPart(mdl)  ((ProPart) mdl)

typedef int   ProBoolean;
typedef int   ProDimensiontype;
typedef int   ProElemId;
typedef int   ProError;
typedef int   ProFeatStatus;
typedef int   ProFeattype;
typedef int   ProFeatureDeleteOptions;
typedef int   ProFeatureResumeOptions;
typedef int   ProMdlType;
typedef int   ProParamvalueType;
typedef int   ProValueDataType;
typedef int   uiCmdAccessMode;
typedef int   uiCmdAccessState;
typedef int   uiCmdCmdId;
typedef int   uiCmdValue;
typedef void* ProAppData;
typedef void* ProArray;
typedef void* ProAsmcomppath;
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
typedef void* ProSurface;
typedef void* ProUnitsystem;
typedef void* ProWVerstamp;
typedef wchar_t ProFileName[260];
typedef wchar_t ProName[PRO_NAME_SIZE + 1];
typedef wchar_t ProPath[260];

/*
 * Definitions where we need to provide internal structure
 * (because our code is accessing it)
 */
typedef double Pro3dPnt[3];
typedef double ProVector[3];
typedef double ProMatrix[4][4];
typedef int    ProIdTable[2];
typedef int    facet_set[3];
typedef struct pf {int id;} ProFeature;
typedef struct pmatl {wchar_t *matl_name;} ProMaterial;
typedef struct pmassp {double volume; double surface_area; double density; double mass;} ProMassProperty;
typedef struct proparam {wchar_t *id;} ProParameter;
typedef struct prouc {double scale;} ProUnitConversion;
typedef struct psap {double color_rgb[3];double transparency;double shininess;double diffuse;double highlite;} ProSurfaceAppearanceProps;
typedef struct pstd {ProSurface surface;int n_vertices;point_t *vertices;point_t *normals;int n_facets;facet_set *facets;} ProSurfaceTessellationData;
typedef struct puitm {ProMdl owner; ProName name;} ProUnititem;

/* Function types */
typedef int (*ProFeatureFilterAction)(ProFeature*,void*);
typedef int (*ProParameterAction)(ProParameter*,int,void*);

/* Functions */
extern "C" int ProArrayAlloc(int,int,int,void**);
extern "C" int ProArrayFree(void**);
extern "C" int ProArrayObjectAdd(ProArray*,int,int,void*);
extern "C" int ProArraySizeGet(ProArray,int*);
extern "C" int ProAsmcompMdlGet(ProFeature*,void**);
extern "C" int ProAsmcomppathInit(void*,ProIdTable,int,void**);
extern "C" int ProAsmcomppathTrfGet(void**,int,ProMatrix);
extern "C" int ProDimensionTypeGet(void**,int*);
extern "C" int ProDimensionValueGet(void**,double *);
extern "C" int ProElementDoubleGet(void*,void*,double*);
extern "C" int ProElementFree(void**);
extern "C" int ProElementIdGet(void*,int*);
extern "C" int ProElementIntegerGet(void*,void*,int*);
extern "C" int ProElementValuetypeGet(void*,int*);
extern "C" int ProElemtreeElementVisit(void*,void*,int (*)(void*,void*,void*,void*),int (*)(void*,void*,void*,void*),void*);
extern "C" int ProFeatureChildrenGet(void*,int**,int*);
extern "C" int ProFeatureDimensionVisit(ProFeature *,int (*)(void* *,int ,void*),int (*)(void**,void*),void*);
extern "C" int ProFeatureElemtreeExtract(ProFeature *,void*,int,void**);
extern "C" int ProFeatureGeomitemVisit(ProFeature *,int,int (*)(void**,int,void*),int (*)(void**,void*),void*);
extern "C" int ProFeatureInit(void*,int,ProFeature*);
extern "C" int ProFeatureStatusGet(ProFeature*,ProFeatStatus *);
extern "C" int ProFeatureTypeGet(ProFeature*,int*);
extern "C" int ProFeatureWithoptionsResume(void*,int*,void*,int);
extern "C" int ProFeatureWithoptionsSuppress(void*,int*,void*,int);
extern "C" int ProGeomitemdataGet(void**,void***);
extern "C" int ProLinedataGet(void**,Pro3dPnt,Pro3dPnt);
extern "C" int ProMdlCurrentGet(void**);
extern "C" int ProMdlMdlnameGet(void*,wchar_t*);
extern "C" int ProMdlPrincipalunitsystemGet(void*,void**);
extern "C" int ProMdlToModelitem(void*,void**);
extern "C" int ProMdlTypeGet(void*,int*);
extern "C" int ProMdlVerstampGet(void*, void**);
extern "C" int ProParameterInit(void *,wchar_t *,void *);
extern "C" int ProParameterValueWithUnitsGet(ProParameter *,void *,void *);
extern "C" int ProParameterVisit(void **, void*, int (*)(ProParameter*, int, void*), void *);
extern "C" int ProParamvalueTypeGet(void **,void *);
extern "C" int ProParamvalueValueGet(void **,int,void *);
extern "C" int ProPartDensityGet(void*,double*);
extern "C" int ProPartMaterialNameGet(void*,wchar_t*);
extern "C" int ProPartTessellate(void*,double,double,int,ProSurfaceTessellationData**);
extern "C" int ProSolidFeatVisit(void*,int (*)(ProFeature*,int,void*),int (*)(ProFeature*,void*),void*);
extern "C" int ProSolidMassPropertyGet(void*,void*,ProMassProperty*);
extern "C" int ProSolidOutlineGet(void*, Pro3dPnt*);
extern "C" int ProStringarrayFree(char**,int);
extern "C" int ProSurfaceSideAppearancepropsGet(void**,int,ProSurfaceAppearanceProps*);
extern "C" int ProUICheckbuttonActivateActionSet(const char*,const char*,void(*)(char*,char*,void*),void*);
extern "C" int ProUICheckbuttonGetState(const char*,const char*,int*);
extern "C" int ProUICheckbuttonSet(const char*,const char*);
extern "C" int ProUICheckbuttonUnset(const char*,const char*);
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
extern "C" int ProUIRadiogroupSelectednamesSet(const char*,const char*,int ,char** );
extern "C" int ProUnitConversionCalculate(ProUnititem*,ProUnititem*,ProUnitConversion*);
extern "C" int ProUnitInit(void*,wchar_t*,ProUnititem*);
extern "C" int ProUnitsystemUnitGet(void**,int,ProUnititem*);
extern "C" int ProVerstampStringFree(char**);
extern "C" int ProVerstampStringGet(void*, char**);
extern "C" int ProWstringFree(wchar_t*);
extern "C" void ProMdlIsSkeleton(void*, int*);
extern "C" void ProPartTessellationFree(ProSurfaceTessellationData**);
extern "C" void ProStringToWstring(wchar_t*, char*);
extern "C" void ProUIInputpanelMaxlenSet(const char*,const char*,int);
extern "C" void ProWstringToString(char*,wchar_t*);
extern "C" void** PRO_CURVE_DATA(void*);

extern "C" void creo_brl_shim_set_length_unit(const wchar_t*,double);
extern "C" void creo_brl_shim_set_model_name(const wchar_t*);

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
