/*               A P 2 1 4 L A Z Y S E S S I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_AP214_G_AP214LAZYSESSION_H
#define CONV_STEP_AP214_G_AP214LAZYSESSION_H

#include "common.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class SDAI_Application_instance;
class InstMgrBase;

namespace brlcad {
namespace step {

struct AP214LazyProgress {
    uint64_t offset = 0;
    uint64_t file_size = 0;
    uint64_t instances_scanned = 0;
};

struct AP214LazyDiagnostic {
    int severity = 0;
    uint64_t entity_id = 0;
    std::string entity_type;
    uint64_t file_offset = 0;
    uint64_t line = 0;
    std::string attribute;
    std::string message;
    uint64_t occurrences = 0;
};

struct AP214LazyStatistics {
    uint64_t instances_scanned = 0;
    uint64_t instances_loaded = 0;
    uint64_t instances_pinned = 0;
    uint64_t cache_high_water = 0;
    uint64_t cache_hits = 0;
    uint64_t cache_misses = 0;
    uint64_t materializations = 0;
    uint64_t evictions = 0;
    uint64_t active_batches = 0;
    uint64_t data_sections = 0;
    /** Indexed source-record bytes represented by materialized instances. */
    uint64_t resident_source_bytes = 0;
    /** Maximum resident_source_bytes observed during this session. */
    uint64_t source_bytes_high_water = 0;
    bool cancelled = false;
};

class AP214LazyBatch {
public:
    AP214LazyBatch();
    ~AP214LazyBatch();
    AP214LazyBatch(AP214LazyBatch &&other) noexcept;
    AP214LazyBatch &operator=(AP214LazyBatch &&other) noexcept;

    AP214LazyBatch(const AP214LazyBatch &) = delete;
    AP214LazyBatch &operator=(const AP214LazyBatch &) = delete;

    bool Valid() const;
    void Release();
    const std::vector<uint64_t> &Roots() const;
    const std::vector<uint64_t> &Instances() const;
    SDAI_Application_instance *Get(uint64_t id) const;

private:
    class Impl;
    explicit AP214LazyBatch(std::unique_ptr<Impl> implementation);
    std::unique_ptr<Impl> impl;

    friend class AP214LazySession;
};

/** AP214-specific owner for STEPcode's lazy index and generated registry.
 *
 * A batch and every SDAI pointer obtained from it are valid only while the
 * session and batch are alive.  Callers must detach ordinary immutable data
 * before releasing a batch or handing work to another thread.
 */
class AP214LazySession {
public:
    using ProgressCallback = std::function<void(const AP214LazyProgress &)>;
    using CancellationCallback = std::function<bool()>;
    using DiagnosticCallback = std::function<void(const AP214LazyDiagnostic &)>;

    AP214LazySession();
    ~AP214LazySession();

    AP214LazySession(const AP214LazySession &) = delete;
    AP214LazySession &operator=(const AP214LazySession &) = delete;

    bool Open(const std::string &path);
    std::vector<uint64_t> AllInstances() const;
    std::vector<uint64_t> InstancesByType(const std::string &type) const;
    std::string TypeName(uint64_t id) const;
    std::vector<uint64_t> ForwardReferences(uint64_t id) const;
    std::vector<uint64_t> ReverseReferences(uint64_t id) const;
    AP214LazyBatch LoadBatch(uint64_t root);
    AP214LazyBatch LoadBatch(const std::vector<uint64_t> &roots);
    AP214LazyStatistics Statistics() const;
    const std::vector<AP214LazyDiagnostic> &Diagnostics() const;
    InstMgrBase *ReferenceManager() const;

    void SetProgressCallback(const ProgressCallback &callback);
    void SetCancellationCallback(const CancellationCallback &callback);
    void SetDiagnosticCallback(const DiagnosticCallback &callback);

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_AP214_G_AP214LAZYSESSION_H */
