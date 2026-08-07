/*               S T E P M E C H A N I C A L E X P O R T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_MECHANICAL_EXPORT_H
#define CONV_STEP_MECHANICAL_EXPORT_H

/** Schema-specific values consumed by the shared AP203-family exporter. */
struct STEPMechanicalExportConfig {
    const char *schema_key;
    const char *schema_identifier;
    const char *application;
    const char *preprocessor;
    bool create_design_context;
    bool supports_native_csg;
};

/** Run the common BRL-CAD-to-STEP mechanical export implementation. */
int STEPMechanicalExport(int argc, char *argv[],
    const struct STEPMechanicalExportConfig &config);

#endif /* CONV_STEP_MECHANICAL_EXPORT_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
