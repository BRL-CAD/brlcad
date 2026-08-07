/*               S T E P N A T I V E C S G E X P O R T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_NATIVE_CSG_EXPORT_H
#define CONV_STEP_NATIVE_CSG_EXPORT_H

#include <string>

struct AP203_Contents;
struct directory;
struct rt_wdb;

enum StepNativeCsgStatus {
    STEP_NATIVE_CSG_SUCCESS = 0,
    STEP_NATIVE_CSG_NOT_APPLICABLE,
    STEP_NATIVE_CSG_ERROR
};

/** Export one BRL-CAD solid or region as a schema-native CSG representation.
 *
 * AP203e2, AP214, and AP242 implicit primitives are used when exact.  Other
 * finite solids may be emitted as manifold-BRep boolean operands.  NOT_APPLICABLE
 * means the object has no CSG root (for example, one unsupported primitive)
 * and may safely be passed to the ordinary BRep exporter.  ERROR means a
 * boolean tree could not be represented without changing its semantics.
 */
StepNativeCsgStatus ExportSTEPNativeCSG(struct directory *dp,
    struct rt_wdb *wdbp, struct AP203_Contents *sc, std::string &diagnostic);

#endif /* CONV_STEP_NATIVE_CSG_EXPORT_H */

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
