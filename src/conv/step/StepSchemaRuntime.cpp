/*                   S T E P S C H E M A R U N T I M E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "StepSchemaRuntime.h"

brlcad::step::StepSchemaRuntime::StepSchemaRuntime(
    const char *name, CF_init init, const std::vector<Alias> &lazy_aliases,
    PreprocessHook preprocess, PostIndexHook post_index)
    : schema_name(name ? name : "unknown"), initializer(init), aliases(lazy_aliases),
      preprocess_hook(preprocess), post_index_hook(post_index)
{
}

const char *
brlcad::step::StepSchemaRuntime::Name() const
{
    return schema_name.c_str();
}

CF_init
brlcad::step::StepSchemaRuntime::Initializer() const
{
    return initializer;
}

const std::vector<brlcad::step::StepSchemaRuntime::Alias> &
brlcad::step::StepSchemaRuntime::LazyAliases() const
{
    return aliases;
}

const EntityDescriptor *
brlcad::step::StepSchemaRuntime::Entity(const Registry &registry, const char *name) const
{
    return registry.FindEntity(name);
}

const TypeDescriptor *
brlcad::step::StepSchemaRuntime::Type(const Registry &registry, const char *name) const
{
    return registry.FindType(name);
}

void
brlcad::step::StepSchemaRuntime::Preprocess(STEPWrapper &wrapper) const
{
    if (preprocess_hook) preprocess_hook(wrapper);
}

void
brlcad::step::StepSchemaRuntime::PostIndex(STEPWrapper &wrapper,
    BRLCADWrapper &database, const std::vector<uint64_t> &handled_sdrs) const
{
    if (post_index_hook) post_index_hook(wrapper, database, handled_sdrs);
}
