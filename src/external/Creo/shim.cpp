/**
 * Build-only Creo Toolkit stubs for the migrated frontend.
 */

#include "common.h"

#include <cstring>

#include "shim.h"
#include "frontend/dialog_controller.h"
#include "shared/core_api.h"


static ProName shim_model_name = L"";
static ProName shim_length_unit_name = L"";
static double shim_length_unit_to_mm = 1.0;
static int shim_unit_system = 0;
static int shim_model_name_is_configured = 0;
static int shim_length_unit_is_configured = 0;
static const struct creo_brl_shim_model *shim_active_assembly = NULL;
static const ProSurfaceTessellationData *shim_active_tessellation = NULL;
static int shim_active_tessellation_count = 0;


static const struct creo_brl_shim_model *
as_shim_model(const void *handle)
{
    const struct creo_brl_shim_model *candidate =
        static_cast<const struct creo_brl_shim_model *>(handle);

    return (candidate && candidate->magic == CREO_BRL_SHIM_MODEL_MAGIC) ?
               candidate :
               NULL;
}


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


extern "C" void creo_brl_show_status(const char *) {}
extern "C" void creo_brl_show_popup(const char *, const char *) {}
extern "C" void creo_brl_core_load_profile_shim(void) {}
extern "C" int creo_brl_core_convert_shim(const char *) { return CREO_BRL_CORE_CONVERT_SUCCESS; }
extern "C" int ProArrayAlloc(int, int, int, void **) { return 0; }
extern "C" int ProArrayFree(void **) { return 0; }
extern "C" int ProArrayObjectAdd(ProArray *, int, int, void *) { return 0; }
extern "C" int ProArraySizeGet(ProArray array, int *size)
{
    if (array && size && array == shim_active_tessellation) {
        *size = shim_active_tessellation_count;
        return PRO_TK_NO_ERROR;
    }
    return 0;
}
extern "C" int ProAsmcompMdlGet(ProFeature *feature, void **model)
{
    if (!feature || !model || !shim_active_assembly || feature->id < 0 ||
        static_cast<size_t>(feature->id) >= shim_active_assembly->component_count)
        return PRO_TK_BAD_INPUTS;
    *model = shim_active_assembly->components[feature->id].model;
    return *model ? PRO_TK_NO_ERROR : PRO_TK_BAD_INPUTS;
}
extern "C" int ProAsmcomppathInit(void *owner, ProIdTable ids, int count, void **path)
{
    const struct creo_brl_shim_model *model = as_shim_model(owner);

    if (!model || !ids || count != 1 || !path || ids[0] < 0 ||
        static_cast<size_t>(ids[0]) >= model->component_count)
        return PRO_TK_BAD_INPUTS;
    *path = const_cast<struct creo_brl_shim_component *>(&model->components[ids[0]]);
    return PRO_TK_NO_ERROR;
}
extern "C" int ProAsmcomppathTrfGet(void **path, int, ProMatrix matrix)
{
    const struct creo_brl_shim_component *component =
        path ? static_cast<const struct creo_brl_shim_component *>(*path) : NULL;

    if (!component || !matrix)
        return PRO_TK_BAD_INPUTS;
    std::memcpy(matrix, component->matrix, sizeof(ProMatrix));
    return PRO_TK_NO_ERROR;
}
extern "C" int ProDimensionTypeGet(void **, int *) { return 0; }
extern "C" int ProDimensionValueGet(void **, double *) { return 0; }
extern "C" int ProElementDoubleGet(void *, void *, double *) { return 0; }
extern "C" int ProElementFree(void **) { return 0; }
extern "C" int ProElementIdGet(void *, int *) { return 0; }
extern "C" int ProElementIntegerGet(void *, void *, int *) { return 0; }
extern "C" int ProElementValuetypeGet(void *, int *) { return 0; }
extern "C" int ProElemtreeElementVisit(void *, void *, int (*)(void *, void *, void *, void *), int (*)(void *, void *, void *, void *), void *) { return 0; }
extern "C" int ProFeatureChildrenGet(void *, int **, int *) { return 0; }
extern "C" int ProFeatureDimensionVisit(ProFeature *, int (*)(void **, int, void *), int (*)(void **, void *), void *) { return 0; }
extern "C" int ProFeatureElemtreeExtract(ProFeature *, void *, int, void **) { return 0; }
extern "C" int ProFeatureGeomitemVisit(ProFeature *, int, int (*)(void **, int, void *), int (*)(void **, void *), void *) { return 0; }
extern "C" int ProFeatureInit(void *, int, ProFeature *) { return 0; }
extern "C" int ProFeatureStatusGet(ProFeature *feature, ProFeatStatus *status)
{
    if (!feature || !status || !shim_active_assembly)
        return 0;
    *status = PRO_FEAT_ACTIVE;
    return PRO_TK_NO_ERROR;
}
extern "C" int ProFeatureTypeGet(ProFeature *feature, int *type)
{
    if (!feature || !type || !shim_active_assembly)
        return 0;
    *type = PRO_FEAT_COMPONENT;
    return PRO_TK_NO_ERROR;
}
extern "C" int ProFeatureWithoptionsResume(void *, int *, void *, int) { return 0; }
extern "C" int ProFeatureWithoptionsSuppress(void *, int *, void *, int) { return 0; }
extern "C" int ProGeomitemdataGet(void **, void ***) { return 0; }
extern "C" int ProLinedataGet(void **, Pro3dPnt, Pro3dPnt) { return 0; }
extern "C" int ProMdlCurrentGet(void **) { return 0; }
extern "C" int ProMdlMdlnameGet(void *handle, wchar_t *name)
{
    const struct creo_brl_shim_model *model = as_shim_model(handle);

    if (!name)
        return PRO_TK_BAD_INPUTS;
    if (model) {
        if (!model->name || !model->name[0])
            return PRO_TK_BAD_INPUTS;
        copy_wide_text(name, PRO_NAME_SIZE + 1u, model->name);
        return PRO_TK_NO_ERROR;
    }
    if (!shim_model_name_is_configured)
        return PRO_TK_BAD_INPUTS;
    copy_wide_text(name, PRO_NAME_SIZE + 1u, shim_model_name);
    return PRO_TK_NO_ERROR;
}
extern "C" int ProMdlPrincipalunitsystemGet(void *handle, void **system)
{
    const struct creo_brl_shim_model *model = as_shim_model(handle);

    if (!system)
        return PRO_TK_BAD_INPUTS;
    if (model) {
        if (model->length_unit_to_mm <= 0.0)
            return PRO_TK_BAD_INPUTS;
        *system = handle;
        return PRO_TK_NO_ERROR;
    }
    if (!shim_length_unit_is_configured)
        return PRO_TK_BAD_INPUTS;
    *system = &shim_unit_system;
    return PRO_TK_NO_ERROR;
}
extern "C" int ProMdlToModelitem(void *, void **) { return 0; }
extern "C" int ProMdlTypeGet(void *handle, int *type)
{
    const struct creo_brl_shim_model *model = as_shim_model(handle);

    if (!model || !type || model->type == 0)
        return 0;
    *type = model->type;
    return PRO_TK_NO_ERROR;
}
extern "C" int ProMdlVerstampGet(void *, void **) { return 0; }
extern "C" int ProParameterInit(void *, wchar_t *, void *) { return 0; }
extern "C" int ProParameterValueWithUnitsGet(ProParameter *, void *, void *) { return 0; }
extern "C" int ProParameterVisit(void **, void *, int (*)(ProParameter *, int, void *), void *) { return 0; }
extern "C" int ProParamvalueTypeGet(void **, void *) { return 0; }
extern "C" int ProParamvalueValueGet(void **, int, void *) { return 0; }
extern "C" int ProPartDensityGet(void *, double *) { return 0; }
extern "C" int ProPartMaterialNameGet(void *, wchar_t *) { return 0; }
extern "C" int ProPartTessellate(void *handle, double, double, int include_quilts, ProSurfaceTessellationData **tessellation)
{
    const struct creo_brl_shim_model *model = as_shim_model(handle);

    shim_active_tessellation = NULL;
    shim_active_tessellation_count = 0;
    if (!model || !tessellation)
        return 0;

    if (include_quilts != PRO_B_FALSE) {
        if (!model->tessellation)
            return model->tessellation_error ? model->tessellation_error : 0;
        *tessellation = const_cast<ProSurfaceTessellationData *>(model->tessellation);
        shim_active_tessellation = model->tessellation;
        shim_active_tessellation_count = model->tessellation_surface_count;
        return model->tessellation_error ? model->tessellation_error : PRO_TK_NO_ERROR;
    }

    if (!model->solid_only_tessellation)
        return model->solid_only_tessellation_error ? model->solid_only_tessellation_error : 0;
    *tessellation = const_cast<ProSurfaceTessellationData *>(model->solid_only_tessellation);
    shim_active_tessellation = model->solid_only_tessellation;
    shim_active_tessellation_count = model->solid_only_surface_count;
    return model->solid_only_tessellation_error ? model->solid_only_tessellation_error : PRO_TK_NO_ERROR;
}
extern "C" int ProSolidFeatVisit(void *handle, int (*visit)(ProFeature *, int, void *), int (*filter)(ProFeature *, void *), void *app_data)
{
    const struct creo_brl_shim_model *model = as_shim_model(handle);

    if (!model)
        return 0;
    if (model->feat_visit_error != 0)
        return model->feat_visit_error;
    if (model->type != PRO_MDL_ASSEMBLY)
        return PRO_TK_NO_ERROR;

    const struct creo_brl_shim_model *previous_assembly = shim_active_assembly;
    for (size_t index = 0; index < model->component_count; ++index) {
        ProFeature feature = { static_cast<int>(index) };

        shim_active_assembly = model;
        if (filter) {
            const int filter_status = filter(&feature, app_data);
            if (filter_status == PRO_TK_CONTINUE)
                continue;
            if (filter_status != PRO_TK_NO_ERROR) {
                shim_active_assembly = previous_assembly;
                return filter_status;
            }
        }
        if (visit && visit(&feature, PRO_TK_NO_ERROR, app_data) != PRO_TK_NO_ERROR) {
            shim_active_assembly = previous_assembly;
            return PRO_TK_GENERAL_ERROR;
        }
    }
    shim_active_assembly = previous_assembly;
    return PRO_TK_NO_ERROR;
}
extern "C" int ProSolidMassPropertyGet(void *handle, void *, ProMassProperty *mass_properties)
{
    const struct creo_brl_shim_model *model = as_shim_model(handle);

    if (!model || !model->has_mass_properties || !mass_properties)
        return 0;
    *mass_properties = model->mass_properties;
    return PRO_TK_NO_ERROR;
}
extern "C" int ProSolidOutlineGet(void *handle, Pro3dPnt *bbox)
{
    const struct creo_brl_shim_model *model = as_shim_model(handle);

    if (!model || !model->has_bbox || !bbox)
        return 0;
    std::memcpy(bbox, model->bbox, sizeof(model->bbox));
    return PRO_TK_NO_ERROR;
}
extern "C" int ProStringarrayFree(char **, int) { return 0; }
extern "C" int ProSurfaceSideAppearancepropsGet(void **, int, ProSurfaceAppearanceProps *) { return 0; }
extern "C" int ProUICheckbuttonActivateActionSet(const char *, const char *, void (*)(char *, char *, void *), void *) { return 0; }
extern "C" int ProUICheckbuttonGetState(const char *, const char *, int *) { return 0; }
extern "C" int ProUICheckbuttonSet(const char *, const char *) { return 0; }
extern "C" int ProUICheckbuttonUnset(const char *, const char *) { return 0; }
extern "C" int ProUIDialogActivate(const char *, int *) { return 0; }
extern "C" int ProUIDialogCreate(const char *, const char *) { return 0; }
extern "C" int ProUIDialogDestroy(const char *) { return 0; }
extern "C" int ProUIInputpanelEditable(char *, const char *) { return 0; }
extern "C" int ProUIInputpanelReadOnly(char *, const char *) { return 0; }
extern "C" int ProUIInputpanelValueGet(const char *, const char *, wchar_t **) { return 0; }
extern "C" int ProUIInputpanelValueSet(const char *, const char *, wchar_t *) { return 0; }
extern "C" int ProUILabelTextSet(const char *, const char *, wchar_t *) { return 0; }
extern "C" int ProUIPushbuttonActivateActionSet(const char *, const char *, void (*)(char *, char *, void *), void *) { return 0; }
extern "C" int ProUIRadiogroupSelectednamesGet(const char *, const char *, int *, char ***) { return 0; }
extern "C" int ProUIRadiogroupSelectednamesSet(const char *, const char *, int, char **) { return 0; }
extern "C" int ProUnitConversionCalculate(ProUnititem *from, ProUnititem *, ProUnitConversion *conversion)
{
    const struct creo_brl_shim_model *model = from ? as_shim_model(from->owner) : NULL;

    if (!conversion)
        return PRO_TK_BAD_INPUTS;
    if (model) {
        conversion->scale = model->length_unit_to_mm;
        return PRO_TK_NO_ERROR;
    }
    if (!shim_length_unit_is_configured)
        return PRO_TK_BAD_INPUTS;
    conversion->scale = shim_length_unit_to_mm;
    return PRO_TK_NO_ERROR;
}
extern "C" int ProUnitInit(void *owner, wchar_t *name, ProUnititem *unit)
{
    if (!unit || !name)
        return PRO_TK_BAD_INPUTS;
    unit->owner = owner;
    copy_wide_text(unit->name, PRO_NAME_SIZE + 1u, name);
    return PRO_TK_NO_ERROR;
}
extern "C" int ProUnitsystemUnitGet(void **system, int unit_type, ProUnititem *unit)
{
    const struct creo_brl_shim_model *model = system ? as_shim_model(*system) : NULL;

    if (!system || !*system || unit_type != PRO_UNITTYPE_LENGTH || !unit)
        return PRO_TK_BAD_INPUTS;
    unit->owner = *system;
    copy_wide_text(unit->name, PRO_NAME_SIZE + 1u,
                   (model && model->length_unit_name) ? model->length_unit_name : shim_length_unit_name);
    return PRO_TK_NO_ERROR;
}
extern "C" int ProVerstampStringFree(char **) { return 0; }
extern "C" int ProVerstampStringGet(void *, char **) { return 0; }
extern "C" int ProWstringFree(wchar_t *) { return 0; }
extern "C" void ProMdlIsSkeleton(void *, int *is_skeleton)
{
    if (is_skeleton)
        *is_skeleton = PRO_B_FALSE;
}
extern "C" void ProPartTessellationFree(ProSurfaceTessellationData **tessellation)
{
    if (tessellation)
        *tessellation = NULL;
    shim_active_tessellation = NULL;
    shim_active_tessellation_count = 0;
}
extern "C" void ProStringToWstring(wchar_t *, char *) {}
extern "C" void ProUIInputpanelMaxlenSet(const char *, const char *, int) {}
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
extern "C" void **PRO_CURVE_DATA(void *) { return NULL; }
