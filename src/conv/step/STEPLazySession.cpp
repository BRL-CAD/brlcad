/*                     S T E P L A Z Y S E S S I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include "STEPLazySession.h"

#include <schema.h>

#include <cllazyfile/lazyInstMgr.h>
#include <cllazyfile/lazySupport.h>
#include <clstepcore/Registry.h>

#include <sstream>
#include <utility>

namespace brlcad {
namespace step {

namespace {

template <typename View>
std::vector<uint64_t>
copy_ids(const View &view)
{
    std::vector<uint64_t> result;
    result.reserve(view.size());
    for (typename View::const_iterator id = view.begin(); id != view.end(); ++id)
	result.push_back(static_cast<uint64_t>(*id));
    return result;
}

STEPLazyDiagnostic
copy_diagnostic(const LazyDiagnostic &source)
{
    STEPLazyDiagnostic result;
    result.severity = static_cast<int>(source.severity);
    result.entity_id = static_cast<uint64_t>(source.entity);
    result.entity_type = source.type;
    result.file_offset = static_cast<uint64_t>(source.offset);
    result.line = source.line;
    result.attribute = source.attribute;
    result.message = source.message;
    result.occurrences = source.occurrences;
    return result;
}

std::string
diagnostic_key(const STEPLazyDiagnostic &diagnostic)
{
    std::ostringstream key;
    key << diagnostic.entity_type << '#' << diagnostic.entity_id << ':'
	<< diagnostic.attribute << ':' << diagnostic.message;
    return key.str();
}

} // namespace

class STEPLazyBatch::Impl {
public:
    explicit Impl(LazyInstanceBatch &&source)
	: batch(std::move(source)), roots(copy_ids(batch.roots())), instances(copy_ids(batch.instances()))
    {
    }

    LazyInstanceBatch batch;
    std::vector<uint64_t> roots;
    std::vector<uint64_t> instances;
};

STEPLazyBatch::STEPLazyBatch() = default;
STEPLazyBatch::~STEPLazyBatch() = default;
STEPLazyBatch::STEPLazyBatch(std::unique_ptr<Impl> implementation) : impl(std::move(implementation)) {}
STEPLazyBatch::STEPLazyBatch(STEPLazyBatch &&other) noexcept = default;
STEPLazyBatch &STEPLazyBatch::operator=(STEPLazyBatch &&other) noexcept = default;

bool
STEPLazyBatch::Valid() const
{
    return impl && impl->batch.valid();
}

void
STEPLazyBatch::Release()
{
    if (impl) impl->batch.release();
}

const std::vector<uint64_t> &
STEPLazyBatch::Roots() const
{
    static const std::vector<uint64_t> empty;
    return impl ? impl->roots : empty;
}

const std::vector<uint64_t> &
STEPLazyBatch::Instances() const
{
    static const std::vector<uint64_t> empty;
    return impl ? impl->instances : empty;
}

SDAI_Application_instance *
STEPLazyBatch::Get(uint64_t id) const
{
    return impl ? impl->batch.get(static_cast<instanceID>(id)) : NULL;
}

class STEPLazySession::Impl {
public:
    Impl()
	: registry(new Registry(SchemaInit)), manager(new lazyInstMgr)
    {
	manager->setRegistry(registry.get());
	/* A few mixed AP203/AP214 exporters emit these no-attribute AP203
	 * convenience subtypes while declaring AUTOMOTIVE_DESIGN.  Preserve the
	 * raw indexed keyword, but construct its exact AP214 parent object. */
#ifdef AP214e3
	manager->setMaterializationTypeAlias("DESIGN_CONTEXT", "PRODUCT_DEFINITION_CONTEXT");
	manager->setMaterializationTypeAlias("MECHANICAL_CONTEXT", "PRODUCT_CONTEXT");
#endif
	install_callbacks();
    }

    ~Impl()
    {
	/* SDAI instances use descriptors owned by the generated registry. */
	manager.reset();
	registry.reset();
    }

    void install_callbacks()
    {
	manager->setProgressCallback([this](const LazyScanProgress &source) {
	    if (!progress_callback) return;
	    STEPLazyProgress progress;
	    progress.offset = static_cast<uint64_t>(source.offset);
	    progress.file_size = static_cast<uint64_t>(source.fileSize);
	    progress.instances_scanned = source.instancesScanned;
	    progress_callback(progress);
	});
	manager->setCancellationCallback([this]() {
	    return cancellation_callback ? cancellation_callback() : false;
	});
	manager->setDiagnosticCallback([this](const LazyDiagnostic &source) {
	    const STEPLazyDiagnostic diagnostic = copy_diagnostic(source);
	    diagnostics.push_back(diagnostic);
	    if (diagnostic_callback) diagnostic_callback(diagnostic);
	});
    }

    std::unique_ptr<Registry> registry;
    std::unique_ptr<lazyInstMgr> manager;
    mutable std::vector<STEPLazyDiagnostic> diagnostics;
    ProgressCallback progress_callback;
    CancellationCallback cancellation_callback;
    DiagnosticCallback diagnostic_callback;
};

STEPLazySession::STEPLazySession() : impl(new Impl) {}
STEPLazySession::~STEPLazySession() = default;

bool
STEPLazySession::Open(const std::string &path)
{
    return impl && impl->manager->openFile(path);
}

std::vector<uint64_t>
STEPLazySession::AllInstances() const
{
    return impl ? copy_ids(impl->manager->allInstances()) : std::vector<uint64_t>();
}

std::vector<uint64_t>
STEPLazySession::InstancesByType(const std::string &type) const
{
    return impl ? copy_ids(impl->manager->instancesByType(type)) : std::vector<uint64_t>();
}

std::string
STEPLazySession::TypeName(uint64_t id) const
{
    if (!impl) return std::string();
    const char *name = impl->manager->typeFromFile(static_cast<instanceID>(id));
    return name ? name : std::string();
}

std::vector<uint64_t>
STEPLazySession::ForwardReferences(uint64_t id) const
{
    return impl ? copy_ids(impl->manager->forwardReferences(static_cast<instanceID>(id))) :
	std::vector<uint64_t>();
}

std::vector<uint64_t>
STEPLazySession::ReverseReferences(uint64_t id) const
{
    return impl ? copy_ids(impl->manager->reverseReferences(static_cast<instanceID>(id))) :
	std::vector<uint64_t>();
}

STEPLazyBatch
STEPLazySession::LoadBatch(uint64_t root)
{
    return STEPLazyBatch(std::unique_ptr<STEPLazyBatch::Impl>(
	new STEPLazyBatch::Impl(impl->manager->loadBatch(static_cast<instanceID>(root)))));
}

STEPLazyBatch
STEPLazySession::LoadBatch(const std::vector<uint64_t> &roots)
{
    std::vector<instanceID> converted;
    converted.reserve(roots.size());
    for (std::vector<uint64_t>::const_iterator root = roots.begin(); root != roots.end(); ++root)
	converted.push_back(static_cast<instanceID>(*root));
    return STEPLazyBatch(std::unique_ptr<STEPLazyBatch::Impl>(
	new STEPLazyBatch::Impl(impl->manager->loadBatch(converted))));
}

STEPLazyStatistics
STEPLazySession::Statistics() const
{
    STEPLazyStatistics result;
    if (!impl) return result;
    const LazyCacheStatistics source = impl->manager->cacheStatistics();
    result.instances_scanned = source.instancesScanned;
    result.instances_loaded = source.instancesLoaded;
    result.instances_pinned = source.instancesPinned;
    result.cache_high_water = source.cacheHighWater;
    result.cache_hits = source.cacheHits;
    result.cache_misses = source.cacheMisses;
    result.materializations = source.materializations;
    result.evictions = source.evictions;
    result.active_batches = source.activeBatches;
    result.data_sections = source.dataSections;
    result.resident_source_bytes = source.residentSourceBytes;
    result.source_bytes_high_water = source.sourceBytesHighWater;
    result.cancelled = source.cancelled;
    return result;
}

const std::vector<STEPLazyDiagnostic> &
STEPLazySession::Diagnostics() const
{
    if (impl) {
	for (std::vector<STEPLazyDiagnostic>::iterator diagnostic = impl->diagnostics.begin();
	     diagnostic != impl->diagnostics.end(); ++diagnostic) {
	    const uint64_t occurrences = impl->manager->diagnosticCount(
		diagnostic_key(*diagnostic));
	    if (occurrences > diagnostic->occurrences)
		diagnostic->occurrences = occurrences;
	}
    }
    return impl->diagnostics;
}

InstMgrBase *
STEPLazySession::ReferenceManager() const
{
    return impl ? impl->manager->getAdapter() : NULL;
}

void
STEPLazySession::SetProgressCallback(const ProgressCallback &callback)
{
    impl->progress_callback = callback;
}

void
STEPLazySession::SetCancellationCallback(const CancellationCallback &callback)
{
    impl->cancellation_callback = callback;
}

void
STEPLazySession::SetDiagnosticCallback(const DiagnosticCallback &callback)
{
    impl->diagnostic_callback = callback;
}

} // namespace step
} // namespace brlcad
