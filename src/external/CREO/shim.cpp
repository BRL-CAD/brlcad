/* This file is provided as a compilation stub to ensure ongoing build testing
 * of CREO to BRL-CAD conversion source code.  This file does not necessarily
 * reflect the CREO api, its values, or type constructs and any similarity is
 * either coincidental or necessary for compilation.
 */

#include "common.h"
#include "shim.h"

/* Functions */
int ProArrayAlloc(int,int,int,void**) {return 0;}
int ProArrayFree(void**) {return 0;}
int ProArraySizeGet(ProArray,int*) {return 0;}
int ProAsmcompMdlGet(ProFeature*,void**) {return 0;}
int ProAsmcompMdlNameGet(ProFeature*,int*,wchar_t*) {return 0;}
int ProAsmcomppathInit(void*,ProIdTable,int,void**) {return 0;}
int ProAsmcomppathTrfGet(void**,bool,ProMatrix) {return 0;}
int ProAssemblyIsExploded(void*,bool*) {return 0;}
int ProCmdActionAdd(const char*,int(*)(int,int*,void*),int,int(*)(int),bool,bool,int*) {return 0;}
int ProDimensionTypeGet(void**,int*) {return 0;}
int ProDimensionValueGet(void**,double *) {return 0;}
int ProElementFree(void**) {return 0;}
int ProElementIdGet(void*,int*) {return 0;}
int ProElementValueGet(void*,void**) {return 0;}
int ProElemtreeElementVisit(void*,void*,int (*)(void*,void*,void*,void*),int (*)(void*,void*,void*,void*),void*) {return 0;}
int ProFeatureDimensionVisit(ProFeature *,int (*)(void* *,int ,void*),int (*)(void**,void*),void*) {return 0;}
int ProFeatureElemtreeCreate(ProFeature *,void**) {return 0;}
int ProFeatureGeomitemVisit(ProFeature *,int,int (*)(void**,int,void*),int (*)(void**,void*),void*) {return 0;}
int ProFeatureInit(void*,int,ProFeature*) {return 0;}
int ProFeatureResume(void*,int*,int,void*,int) {return 0;}
int ProFeatureStatusGet(ProFeature*,ProFeatStatus *) {return 0;}
int ProFeatureSuppress(void*,int*,int,void*,int) {return 0;}
int ProFeatureTypeGet(ProFeature*,int*) {return 0;}
int ProGeomitemdataGet(void**,void***) {return 0;}
int ProLinedataGet(void**,Pro3dPnt,Pro3dPnt) {return 0;}
int ProMdlCurrentGet(void**) {return 0;}
int ProMdlNameGet(void*,wchar_t*) {return 0;}
int ProMdlToModelitem(void*,void**) {return 0;}
int ProMdlTypeGet(void*,int*) {return 0;}
int ProMdlVerstampGet(void*, void**) {return 0;}
int ProMdlnameInit(void*,int,void*) {return 0;}
int ProMenubarmenuPushbuttonAdd(const char*,const char*,const char*,const char*,const char*,bool,int,wchar_t*) {return 0;}
int ProMessageDisplay(wchar_t *,const char *,const char *) {return 0;}
int ProParameterInit(void *,wchar_t *,void *) {return 0;}
int ProParameterValueGet(ProParameter *,void *) {return 0;}
int ProParameterVisit (void **,	void*,int (*)(ProParameter*,int,void*),void *) {return 0;}
int ProParamvalueTypeGet(void **,void *) {return 0;}
int ProParamvalueValueGet(void **,int,wchar_t *) {return 0;}
int ProPartMaterialNameGet(void*,wchar_t *) {return 0;}
int ProPartMaterialdataGet(void*,wchar_t*,ProMaterialProps*) {return 0;}
int ProPartTessellate(void*,double,double,bool,ProSurfaceTessellationData**) {return 0;}
int ProSolidFeatVisit(void*,int (*)(ProFeature*,int,void*),int (*)(ProFeature*,void*),void*) {return 0;}
int ProSolidMassPropertyGet(void*,void*,ProMassProperty*) {return 0;}
int ProSolidOutlineGet(void*, Pro3dPnt*) {return 0;}
int ProStringarrayFree(char**,int) {return 0;}
int ProStringVerstampGet(void*, void**) {return 0;}
int ProSurfaceAppearancepropsGet(void**,ProSurfaceAppearanceProps*) {return 0;}
int ProUICheckbuttonActivateActionSet(const char*,const char*,void(*)(char*,char*,void*),void*) {return 0;}
int ProUICheckbuttonGetState(const char*,const char*,bool*) {return 0;}
int ProUIDialogActivate(const char*,int*) {return 0;}
int ProUIDialogCreate(const char *,const char *) {return 0;}
int ProUIDialogDestroy(const char *) {return 0;}
int ProUIInputpanelEditable(char*,const char*) {return 0;}
int ProUIInputpanelReadOnly(char*,const char*) {return 0;}
int ProUIInputpanelValueGet(const char *,const char *,wchar_t**) {return 0;}
int ProUILabelTextSet(const char *,const char *,wchar_t *) {return 0;}
int ProUIPushbuttonActivateActionSet(const char *,const char *,void (*)(char*,char *,void*),void*) {return 0;}
int ProUIRadiogroupSelectednamesGet(const char*,const char*,int*,char***) {return 0;}
int ProUITextareaValueSet(const char *,const char *,wchar_t*) {return 0;}
int ProValueDataGet(void*,ProValueData*) {return 0;}
int ProVerstampEqual(void*, void*) {return 0;}
int ProVerstampStringGet(void*, char**) {return 0;}
int ProVerstampStringFree(char**) {return 0;}
int ProWcharSizeVerify(size_t,int*) {return 0;}
int ProWindowRefresh(int) {return 0;}
int ProWstringFree(wchar_t*) {return 0;}
void ProAssemblyUnexplode(void*) {}
void ProMdlMdlNameGet(void*,int*,wchar_t*) {}
void ProMdlMdlnameGet(void*,wchar_t*) {}
void ProMdlPrincipalunitsystemGet(void*,void**) {}
void ProMessageClear() {}
void ProPartTessellationFree(ProSurfaceTessellationData**) {}
void ProStringToWstring(wchar_t*,const char*) {}
void ProUIMessageDialogDisplay(int,const wchar_t *,const wchar_t *,ProUIMessageButton*,int,ProUIMessageButton*) {}
void ProUnitConversionGet(void**,ProUnitConversion*,void**) {}
void ProUnitInit(void*,const wchar_t *,void**) {}
void ProUnitsystemUnitGet(void**,int,void**) {}
void ProWstringToString(char*,wchar_t*) {}
void* ProMdlToPart(void*v) {return v;}
void* ProMdlToSolid(void*v) {return v;}
void** PRO_CURVE_DATA(void*) {return NULL;}
