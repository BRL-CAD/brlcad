/*                     S T E P S C H E M A R U N T I M E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEPSCHEMARUNTIME_H
#define CONV_STEP_STEPSCHEMARUNTIME_H

#include "common.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <Registry.h>

class EntityDescriptor;
class TypeDescriptor;
class BRLCADWrapper;
class STEPWrapper;

namespace brlcad {
namespace step {

class StepSchemaRuntime {
public:
    using Alias = std::pair<std::string, std::string>;
    using PreprocessHook = void (*)(STEPWrapper &);
    using PostIndexHook = void (*)(STEPWrapper &, BRLCADWrapper &,
	const std::vector<uint64_t> &);

    StepSchemaRuntime(const char *name, CF_init initializer,
                      const std::vector<Alias> &lazy_aliases = {},
                      PreprocessHook preprocess = nullptr,
                      PostIndexHook post_index = nullptr);

    const char *Name() const;
    CF_init Initializer() const;
    const std::vector<Alias> &LazyAliases() const;
    const EntityDescriptor *Entity(const Registry &registry, const char *name) const;
    const TypeDescriptor *Type(const Registry &registry, const char *name) const;
    void Preprocess(STEPWrapper &wrapper) const;
    void PostIndex(STEPWrapper &wrapper, BRLCADWrapper &database,
	const std::vector<uint64_t> &handled_sdrs) const;

private:
    std::string schema_name;
    CF_init initializer;
    std::vector<Alias> aliases;
    PreprocessHook preprocess_hook;
    PostIndexHook post_index_hook;
};

/* Defined exactly once by the active schema plugin. */
const StepSchemaRuntime &CurrentStepSchemaRuntime();

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPSCHEMARUNTIME_H */
