/*             A P 2 1 4 L A Z Y S E S S I O N . C P P
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

#include "AP214LazySession.h"

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

AP214LazyDiagnostic
copy_diagnostic(const LazyDiagnostic &source)
{
    AP214LazyDiagnostic result;
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
diagnostic_key(const AP214LazyDiagnostic &diagnostic)
{
    std::ostringstream key;
    key << diagnostic.entity_type << '#' << diagnostic.entity_id << ':'
	<< diagnostic.attribute << ':' << diagnostic.message;
    return key.str();
}

} // namespace

class AP214LazyBatch::Impl {
public:
    explicit Impl(LazyInstanceBatch &&source)
	: batch(std::move(source)), roots(copy_ids(batch.roots())), instances(copy_ids(batch.instances()))
    {
    }

    LazyInstanceBatch batch;
    std::vector<uint64_t> roots;
    std::vector<uint64_t> instances;
};

AP214LazyBatch::AP214LazyBatch() = default;
AP214LazyBatch::~AP214LazyBatch() = default;
AP214LazyBatch::AP214LazyBatch(std::unique_ptr<Impl> implementation) : impl(std::move(implementation)) {}
AP214LazyBatch::AP214LazyBatch(AP214LazyBatch &&other) noexcept = default;
AP214LazyBatch &AP214LazyBatch::operator=(AP214LazyBatch &&other) noexcept = default;

bool
AP214LazyBatch::Valid() const
{
    return impl && impl->batch.valid();
}

void
AP214LazyBatch::Release()
{
    if (impl) impl->batch.release();
}

const std::vector<uint64_t> &
AP214LazyBatch::Roots() const
{
    static const std::vector<uint64_t> empty;
    return impl ? impl->roots : empty;
}

const std::vector<uint64_t> &
AP214LazyBatch::Instances() const
{
    static const std::vector<uint64_t> empty;
    return impl ? impl->instances : empty;
}

SDAI_Application_instance *
AP214LazyBatch::Get(uint64_t id) const
{
    return impl ? impl->batch.get(static_cast<instanceID>(id)) : NULL;
}

class AP214LazySession::Impl {
public:
    Impl()
	: registry(new Registry(SchemaInit)), manager(new lazyInstMgr)
    {
	manager->setRegistry(registry.get());
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
	    AP214LazyProgress progress;
	    progress.offset = static_cast<uint64_t>(source.offset);
	    progress.file_size = static_cast<uint64_t>(source.fileSize);
	    progress.instances_scanned = source.instancesScanned;
	    progress_callback(progress);
	});
	manager->setCancellationCallback([this]() {
	    return cancellation_callback ? cancellation_callback() : false;
	});
	manager->setDiagnosticCallback([this](const LazyDiagnostic &source) {
	    const AP214LazyDiagnostic diagnostic = copy_diagnostic(source);
	    diagnostics.push_back(diagnostic);
	    if (diagnostic_callback) diagnostic_callback(diagnostic);
	});
    }

    std::unique_ptr<Registry> registry;
    std::unique_ptr<lazyInstMgr> manager;
    mutable std::vector<AP214LazyDiagnostic> diagnostics;
    ProgressCallback progress_callback;
    CancellationCallback cancellation_callback;
    DiagnosticCallback diagnostic_callback;
};

AP214LazySession::AP214LazySession() : impl(new Impl) {}
AP214LazySession::~AP214LazySession() = default;

bool
AP214LazySession::Open(const std::string &path)
{
    return impl && impl->manager->openFile(path);
}

std::vector<uint64_t>
AP214LazySession::AllInstances() const
{
    return impl ? copy_ids(impl->manager->allInstances()) : std::vector<uint64_t>();
}

std::vector<uint64_t>
AP214LazySession::InstancesByType(const std::string &type) const
{
    return impl ? copy_ids(impl->manager->instancesByType(type)) : std::vector<uint64_t>();
}

std::string
AP214LazySession::TypeName(uint64_t id) const
{
    if (!impl) return std::string();
    const char *name = impl->manager->typeFromFile(static_cast<instanceID>(id));
    return name ? name : std::string();
}

std::vector<uint64_t>
AP214LazySession::ForwardReferences(uint64_t id) const
{
    return impl ? copy_ids(impl->manager->forwardReferences(static_cast<instanceID>(id))) :
	std::vector<uint64_t>();
}

std::vector<uint64_t>
AP214LazySession::ReverseReferences(uint64_t id) const
{
    return impl ? copy_ids(impl->manager->reverseReferences(static_cast<instanceID>(id))) :
	std::vector<uint64_t>();
}

AP214LazyBatch
AP214LazySession::LoadBatch(uint64_t root)
{
    return AP214LazyBatch(std::unique_ptr<AP214LazyBatch::Impl>(
	new AP214LazyBatch::Impl(impl->manager->loadBatch(static_cast<instanceID>(root)))));
}

AP214LazyBatch
AP214LazySession::LoadBatch(const std::vector<uint64_t> &roots)
{
    std::vector<instanceID> converted;
    converted.reserve(roots.size());
    for (std::vector<uint64_t>::const_iterator root = roots.begin(); root != roots.end(); ++root)
	converted.push_back(static_cast<instanceID>(*root));
    return AP214LazyBatch(std::unique_ptr<AP214LazyBatch::Impl>(
	new AP214LazyBatch::Impl(impl->manager->loadBatch(converted))));
}

AP214LazyStatistics
AP214LazySession::Statistics() const
{
    AP214LazyStatistics result;
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

const std::vector<AP214LazyDiagnostic> &
AP214LazySession::Diagnostics() const
{
    if (impl) {
	for (std::vector<AP214LazyDiagnostic>::iterator diagnostic = impl->diagnostics.begin();
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
AP214LazySession::ReferenceManager() const
{
    return impl ? impl->manager->getAdapter() : NULL;
}

void
AP214LazySession::SetProgressCallback(const ProgressCallback &callback)
{
    impl->progress_callback = callback;
}

void
AP214LazySession::SetCancellationCallback(const CancellationCallback &callback)
{
    impl->cancellation_callback = callback;
}

void
AP214LazySession::SetDiagnosticCallback(const DiagnosticCallback &callback)
{
    impl->diagnostic_callback = callback;
}

} // namespace step
} // namespace brlcad
