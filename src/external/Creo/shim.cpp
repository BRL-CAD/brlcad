/**
 *                        S H I M . C P P
 * BRL-CAD
 *
 * Published in 2024 by the United States Government.
 * This work is in the public domain.
 *
 */
/**
 * @file shim.cpp
 *
 * This file is provided as a compilation stub to ensure ongoing build testing
 * of CREO to BRL-CAD conversion source code.  This file does not necessarily
 * reflect the CREO api, its values, or type constructs and any similarity is
 * either coincidental or necessary for compilation.
 */

#include "common.h"
#include "shim.h"
#include "shared/core_api.h"

static ProName shim_model_name = L"";
static ProName shim_length_unit_name = L"";
static double shim_length_unit_to_mm = 1.0;
static int shim_unit_system = 0;
static int shim_model_name_is_configured = 0;
static int shim_length_unit_is_configured = 0;


static void
copy_wide_text(wchar_t *destination, size_t destination_size, const wchar_t *source)
{
    size_t index = 0;

    if (!destination || destination_size == 0)
        return;
    if (source) {
        while (index + 1u < destination_size && source[index] != L'\0') {
            destination[index] = source[index];
            ++index;
        }
    }
    destination[index] = L'\0';
}


extern "C" void
creo_brl_shim_set_length_unit(const wchar_t *name, double conversion_to_mm)
{
    copy_wide_text(shim_length_unit_name,
                   sizeof(shim_length_unit_name) / sizeof(shim_length_unit_name[0]),
                   name);
    shim_length_unit_to_mm = conversion_to_mm;
    shim_length_unit_is_configured = 1;
}


extern "C" void
creo_brl_shim_set_model_name(const wchar_t *name)
{
    copy_wide_text(shim_model_name,
                   sizeof(shim_model_name) / sizeof(shim_model_name[0]), name);
    shim_model_name_is_configured = 1;
}

/* Functions */
extern "C" void creo_brl_show_status(const char *) {}
extern "C" void creo_brl_core_load_profile_shim(void) {}
extern "C" int creo_brl_core_convert_shim(const char *) {return CREO_BRL_CORE_CONVERT_SUCCESS;}
extern "C" int ProArrayAlloc(int,int,int,void**) {return 0;}
extern "C" int ProArrayFree(void**) {return 0;}
extern "C" int ProArrayObjectAdd(ProArray*,int,int,void*) {return 0;}
extern "C" int ProArraySizeGet(ProArray,int*) {return 0;}
extern "C" int ProAsmcompMdlGet(ProFeature*,void**) {return 0;}
extern "C" int ProAsmcompMdlMdlnameGet(ProFeature*,int*,wchar_t*) {return 0;}
extern "C" int ProAsmcomppathInit(void*,ProIdTable,int,void**) {return 0;}
extern "C" int ProAsmcomppathTrfGet(void**,int,ProMatrix) {return 0;}
extern "C" int ProAssemblyIsExploded(void*,int*) {return 0;}
extern "C" int ProBsplinesrfdataGet(void**,int[2],double**,double**,double**,ProVector**,int*,int*,int*) {return 0;}
extern "C" int ProCmdActionAdd(const char*,int(*)(int,int*,void*),int,int(*)(int),int,int,int*) {return 0;}
extern "C" int ProDimensionTypeGet(void**,int*) {return 0;}
extern "C" int ProDimensionValueGet(void**,double *) {return 0;}
extern "C" int ProDirectoryCurrentGet(void*) {return 0;}
extern "C" int ProElementDoubleGet(void*,void*,double*) {return 0;}
extern "C" int ProElementFree(void**) {return 0;}
extern "C" int ProElementIdGet(void*,int*) {return 0;}
extern "C" int ProElementIntegerGet(void*,void*,int*) {return 0;}
extern "C" int ProElementValueGet(void*,void**) {return 0;}
extern "C" int ProElementValuetypeGet(void*,int*) {return 0;}
extern "C" int ProElemtreeElementVisit(void*,void*,int (*)(void*,void*,void*,void*),int (*)(void*,void*,void*,void*),void*) {return 0;}
extern "C" int ProFeatureChildrenGet(void*,int**,int*) {return 0;}
extern "C" int ProFeatureDimensionVisit(ProFeature *,int (*)(void* *,int ,void*),int (*)(void**,void*),void*) {return 0;}
extern "C" int ProFeatureElemtreeCreate(ProFeature *,void**) {return 0;}
extern "C" int ProFeatureElemtreeExtract(ProFeature *,void*,int,void**) {return 0;}
extern "C" int ProFeatureGeomitemVisit(ProFeature *,int,int (*)(void**,int,void*),int (*)(void**,void*),void*) {return 0;}
extern "C" int ProFeatureInit(void*,int,ProFeature*) {return 0;}
extern "C" int ProFeatureStatusGet(ProFeature*,ProFeatStatus *) {return 0;}
extern "C" int ProFeatureTypeGet(ProFeature*,int*) {return 0;}
extern "C" int ProFeatureWithoptionsResume(void*,int*,void*,int) {return 0;}
extern "C" int ProFeatureWithoptionsSuppress(void*,int*,void*,int) {return 0;}
extern "C" int ProGeomitemdataGet(void**,void***) {return 0;}
extern "C" int ProLinedataGet(void**,Pro3dPnt,Pro3dPnt) {return 0;}
extern "C" int ProMaterialCurrentGet(void*,ProMaterial*) {return 0;}
extern "C" int ProMdlCurrentGet(void**) {return 0;}
extern "C" int ProMdlMdlnameGet(void*,wchar_t *name)
{
    if (!name || !shim_model_name_is_configured)
        return PRO_TK_BAD_INPUTS;
    copy_wide_text(name, PRO_NAME_SIZE + 1u, shim_model_name);
    return PRO_TK_NO_ERROR;
}
extern "C" int ProMdlPrincipalunitsystemGet(void*,void **system)
{
    if (!system || !shim_length_unit_is_configured)
        return PRO_TK_BAD_INPUTS;
    *system = &shim_unit_system;
    return PRO_TK_NO_ERROR;
}
extern "C" int ProMdlToModelitem(void*,void**) {return 0;}
extern "C" int ProMdlTypeGet(void*,int*) {return 0;}
extern "C" int ProMdlVerstampGet(void*, void**) {return 0;}
extern "C" int ProMdlnameInit(void*,int,void*) {return 0;}
extern "C" int ProMenubarmenuPushbuttonAdd(const char*,const char*,const char*,const char*,const char*,int,int,wchar_t*) {return 0;}
extern "C" int ProMessageDisplay(wchar_t *,const char *,const char *) {return 0;}
extern "C" int ProParameterInit(void *,wchar_t *,void *) {return 0;}
extern "C" int ProParameterValueGet(ProParameter *,void *) {return 0;}
extern "C" int ProParameterValueWithUnitsGet(ProParameter *,void *,void *) {return 0;}
extern "C" int ProParameterVisit(void **, void*, int (*)(ProParameter*, int, void*), void *) {return 0;}
extern "C" int ProParamvalueTypeGet(void **,void *) {return 0;}
extern "C" int ProParamvalueValueGet(void **,int,void *) {return 0;}
extern "C" int ProPartDensityGet(void*,double*) {return 0;}
extern "C" int ProPartMaterialNameGet(void*,wchar_t*) {return 0;}
extern "C" int ProPartTessellate(void*,double,double,int,ProSurfaceTessellationData**) {return 0;}
extern "C" int ProSolidBodiesCollect(void*, ProSolidBody**) {return 0;}
extern "C" int ProSolidBodySurfaceVisit(void*,int(*)(void*,int,void*),void*) {return 0;}
extern "C" int ProSolidFeatVisit(void*,int (*)(ProFeature*,int,void*),int (*)(ProFeature*,void*),void*) {return 0;}
extern "C" int ProSolidMassPropertyGet(void*,void*,ProMassProperty*) {return 0;}
extern "C" int ProSolidOutlineGet(void*, Pro3dPnt*) {return 0;}
extern "C" int ProStringVerstampGet(void*, void**) {return 0;}
extern "C" int ProStringarrayFree(char**,int) {return 0;}
extern "C" int ProSurfaceAreaEval(void*,double*) {return 0;}
extern "C" int ProSurfaceSideAppearancepropsGet(void**,int,ProSurfaceAppearanceProps*) {return 0;}
extern "C" int ProUICheckbuttonActivateActionSet(const char*,const char*,void(*)(char*,char*,void*),void*) {return 0;}
extern "C" int ProUICheckbuttonDisable(const char*,const char*) {return 0;}
extern "C" int ProUICheckbuttonEnable(const char*,const char*) {return 0;}
extern "C" int ProUICheckbuttonGetState(const char*,const char*,int*) {return 0;}
extern "C" int ProUICheckbuttonSet(const char*,const char*) {return 0;}
extern "C" int ProUICheckbuttonUnset(const char*,const char*) {return 0;}
extern "C" int ProUIDialogActivate(const char*, int*) {return 0;}
extern "C" int ProUIDialogCreate(const char *,const char *) {return 0;}
extern "C" int ProUIDialogDestroy(const char *) {return 0;}
extern "C" int ProUIInputpanelEditable(char*,const char*) {return 0;}
extern "C" int ProUIInputpanelReadOnly(char*,const char*) {return 0;}
extern "C" int ProUIInputpanelValueGet(const char *,const char *,wchar_t**) {return 0;}
extern "C" int ProUIInputpanelValueSet(const char *,const char *,wchar_t*) {return 0;}
extern "C" int ProUILabelTextSet(const char *,const char *,wchar_t *) {return 0;}
extern "C" int ProUIPushbuttonActivateActionSet(const char *,const char *,void (*)(char*,char *,void*),void*) {return 0;}
extern "C" int ProUIRadiogroupSelectednamesGet(const char*,const char*,int*,char***) {return 0;}
extern "C" int ProUIRadiogroupSelectednamesSet(const char*,const char*,int ,char** ) {return 0;}
extern "C" int ProUITextareaValueSet(const char *,const char *,wchar_t*) {return 0;}
extern "C" int ProUnitConversionCalculate(ProUnititem*,ProUnititem*,ProUnitConversion *conversion)
{
    if (!conversion || !shim_length_unit_is_configured)
        return PRO_TK_BAD_INPUTS;
    conversion->scale = shim_length_unit_to_mm;
    return PRO_TK_NO_ERROR;
}
extern "C" int ProUnitInit(void *owner,const wchar_t *name,ProUnititem *unit)
{
    if (!unit || !name)
        return PRO_TK_BAD_INPUTS;
    unit->owner = owner;
    copy_wide_text(unit->name, PRO_NAME_SIZE + 1u, name);
    return PRO_TK_NO_ERROR;
}
extern "C" int ProUnitsystemTypeGet(void**,ProUnitsystemType*) {return 0;}
extern "C" int ProUnitsystemUnitGet(void **system,int unit_type,ProUnititem *unit)
{
    if (!system || !*system || unit_type != PRO_UNITTYPE_LENGTH || !unit)
        return PRO_TK_BAD_INPUTS;
    unit->owner = *system;
    copy_wide_text(unit->name, PRO_NAME_SIZE + 1u, shim_length_unit_name);
    return PRO_TK_NO_ERROR;
}
extern "C" int ProUtilCollectParameters(void**,void**) {return 0;}
extern "C" int ProValueDataGet(void*,ProValueData*) {return 0;}
extern "C" int ProVerstampEqual(void*, void*) {return 0;}
extern "C" int ProVerstampStringFree(char**) {return 0;}
extern "C" int ProVerstampStringGet(void*, char**) {return 0;}
extern "C" int ProWcharSizeVerify(size_t,int*) {return 0;}
extern "C" int ProWindowRefresh(int) {return 0;}
extern "C" int ProWstringFree(wchar_t*) {return 0;}
extern "C" void ProAssemblyUnexplode(void*) {}
extern "C" void ProContourEdgeVisit(void*,void*,int(*)(void*,int,void*),int(*)(void*,void*),void*) {}
extern "C" void ProContourTraversalGet(void*,int*) {}
extern "C" void ProMdlIsSkeleton(void*, int*) {}
extern "C" void ProMessageClear() {}
extern "C" void ProPartTessellationFree(ProSurfaceTessellationData**) {}
extern "C" void ProStringToWstring(wchar_t*, char*) {}
extern "C" void ProSurfaceContourVisit(void*,int(*)(void*,int,void*),int(*)(void*,void*), void*) {}
extern "C" void ProSurfaceIdGet(void*,int*) {}
extern "C" void ProSurfaceToNURBS(void*,void***) {}
extern "C" void ProSurfacedataGet(void*,int*,double*,double*,int*,void**,int*) {}
extern "C" void ProUIInputpanelMaxlenSet(const char*,const char*,int) {}
extern "C" void ProUIMessageDialogDisplay(int,const wchar_t *,const wchar_t *,ProUIMessageButton*,int,ProUIMessageButton*) {}
extern "C" void ProWstringToString(char *destination, wchar_t *source)
{
    size_t index = 0;

    if (!destination)
        return;
    if (source) {
        while (source[index] != L'\0') {
            destination[index] = source[index] >= 0 && source[index] <= 0x7f ?
                                     (char)source[index] : '?';
            ++index;
        }
    }
    destination[index] = '\0';
}
extern "C" void** PRO_CURVE_DATA(void*) {return NULL;}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
