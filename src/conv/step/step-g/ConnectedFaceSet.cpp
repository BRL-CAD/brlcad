/*                 ConnectedFaceSet.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step/ConnectedFaceSet.cpp
 *
 * Routines to convert STEP "ConnectedFaceSet" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"
#include "brep/pullback.h"

#include "ConnectedFaceSet.h"
#include "AdvancedFace.h"
#include "FaceSurface.h"
#include "LocalUnits.h"
#include "OpenNurbsInterfaces.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <vector>

#define CLASSNAME "ConnectedFaceSet"
#define ENTITYNAME "Connected_Face_Set"
string ConnectedFaceSet::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)ConnectedFaceSet::Create);

namespace {

/* Measured 152- and 271-face analytic solids spend more time duplicating and
 * stitching standalone BREPs than they recover from helper work.  Reserve
 * face-level scheduling for genuinely large (512+) single solids; smaller
 * items retain edge-level helpers and top-level solid parallelism. */
constexpr size_t kMinimumParallelFaceCount = 512;
/* Serial preparation touches small mutable caches on shared STEP curve and
 * surface adapters.  Retain four faces per requested worker (at least eight,
 * capped at 32) before parallel trim construction and deterministic append
 * release the temporary face BREPs. */
constexpr size_t kMinimumPreparedFacesPerBatch = 8;
constexpr size_t kPreparedFacesPerWorker = 4;
constexpr size_t kMaximumPreparedFacesPerBatch = 32;

struct PreparedFaceTask {
    FaceSurface *face = NULL;
    std::unique_ptr<ON_Brep> brep;
    STEPEntity::ONStateMap on_state;
    std::vector<Face::PreparedBound> bounds;
    uint64_t preparation_us = 0;
    uint64_t trim_construction_us = 0;
    bool finished = false;
};

} // namespace

ConnectedFaceSet::ConnectedFaceSet()
{
    step = NULL;
    id = 0;
}

ConnectedFaceSet::ConnectedFaceSet(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

ConnectedFaceSet::~ConnectedFaceSet()
{
    // elements created through factory will be deleted there.
    cfs_faces.clear();
}

bool
ConnectedFaceSet::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!TopologicalRepresentationItem::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::TopologicalRepresentationItem." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (cfs_faces.empty()) {
	LIST_OF_ENTITIES *l = step->getListOfEntities(sse, "cfs_faces");
	LIST_OF_ENTITIES::iterator i;
	for (i = l->begin(); i != l->end(); i++) {
	    SDAI_Application_instance *entity = (*i);
	    if (entity) {
		Face *aAF = dynamic_cast<Face *>(Factory::CreateObject(sw, entity)); //CreateSurfaceObject(sw,entity));
		if (aAF) {
		    cfs_faces.push_back(aAF);
		} else {
		    l->clear();
		    delete l;
		    sw->entity_status[id] = STEP_LOAD_ERROR;
		    return false;
		}
	    } else {
		std::cerr << CLASSNAME  << ": Unhandled entity in attribute 'cfs_faces'." << std::endl;
		l->clear();
		delete l;
		sw->entity_status[id] = STEP_LOAD_ERROR;
		return false;
	    }
	}
	l->clear();
	delete l;
    }

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
ConnectedFaceSet::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "cfs_faces:" << std::endl;
    LIST_OF_FACES::iterator i;
    for (i = cfs_faces.begin(); i != cfs_faces.end(); ++i) {
	(*i)->Print(level + 1);
    }

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    TopologicalRepresentationItem::Print(level + 1);
}

void
ConnectedFaceSet::ReverseFaceSet()
{
    LIST_OF_FACES::iterator i;
    for (i = cfs_faces.begin(); i != cfs_faces.end(); ++i) {
	(*i)->ReverseFace();
    }
}

size_t
ConnectedFaceSet::MaximumPullbackSpanEstimate() const
{
    size_t maximum = 1;
    for (LIST_OF_FACES::const_iterator face = cfs_faces.begin();
	    face != cfs_faces.end(); ++face) {
	const FaceSurface *surface_face = dynamic_cast<const FaceSurface *>(*face);
	if (surface_face)
	    maximum = std::max(maximum, surface_face->PullbackSpanEstimate());
    }
    return maximum;
}

STEPEntity *
ConnectedFaceSet::GetInstance(STEPWrapper *sw, int id)
{
    return new ConnectedFaceSet(sw, id);
}

STEPEntity *
ConnectedFaceSet::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

#ifdef _DEBUG_TESTING_
  static int _face_cnt_ = 0;
#endif

bool
ConnectedFaceSet::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    LIST_OF_FACES::iterator i;
    uint64_t completed_faces = 0;
    const uint64_t total_faces = static_cast<uint64_t>(cfs_faces.size());

    /* Every face owns its destination BREP and OpenNURBS index state during
     * this path.  Preparation stays serial because legacy STEP adapters cache
     * curve endpoints and surface bounds; after that, trim construction reads
     * immutable STEP entities and mutates only its face-local BREP. */
    bool parallel_faces = step &&
	total_faces >= kMinimumParallelFaceCount;
    std::vector<FaceSurface *> ordered_faces;
    if (parallel_faces) {
	ordered_faces.reserve(cfs_faces.size());
	for (i = cfs_faces.begin(); i != cfs_faces.end(); ++i) {
	    FaceSurface *face = dynamic_cast<FaceSurface *>(*i);
	    if (!face) {
		parallel_faces = false;
		break;
	    }
	    ordered_faces.push_back(face);
	}
    }
    if (parallel_faces) {
	ON_Brep stitched;
	/* A face-local bounding box is intentionally too small for the bounded
	 * source-mismatch ceiling used by the serial whole-solid path.  Recover the
	 * authoritative shell scale directly from immutable STEP topology vertices.
	 * Constructing a second OpenNURBS edge graph here retained hundreds of MB
	 * when several large solids entered face scheduling concurrently. */
	ON_BoundingBox shell_bounds;
	const std::chrono::steady_clock::time_point scale_started =
	    std::chrono::steady_clock::now();
	for (std::vector<FaceSurface *>::const_iterator face =
		ordered_faces.begin(); face != ordered_faces.end(); ++face) {
	    if (brlcad::PullbackWorkCancelled()) return false;
	    if (!*face || !(*face)->GrowTopologyVertexBounds(&shell_bounds))
		return false;
	}
	const double shell_scale = shell_bounds.IsValid() ?
	    shell_bounds.Diagonal().Length() : 0.0;
	if (!(shell_scale > 0.0) || !std::isfinite(shell_scale))
	    return false;
	if (step)
	    step->RecordStageTiming("face_topology_scale_prepass", id,
		"CONNECTED_FACE_SET", static_cast<uint64_t>(
		    std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - scale_started).count()),
		total_faces, 0);
	for (size_t batch_begin = 0; batch_begin < ordered_faces.size();
		) {
	    /* Re-evaluate capacity for every small retained batch.  A large solid
	     * may begin while all -j slots are occupied, then enlist helpers as
	     * shorter top-level jobs finish without retaining an entire prepared
	     * shell in memory. */
	    const unsigned int available_face_helpers =
		step->AvailableGeometryHelperCapacity();
	    const size_t requested_batch = std::max<size_t>(
		kMinimumPreparedFacesPerBatch,
		static_cast<size_t>(available_face_helpers + 1) *
		    kPreparedFacesPerWorker);
	    const size_t batch_size = std::min(kMaximumPreparedFacesPerBatch,
		requested_batch);
	    const size_t batch_end = std::min(ordered_faces.size(),
		batch_begin + batch_size);
	    std::vector<PreparedFaceTask> tasks(batch_end - batch_begin);
	    for (size_t local = 0; local < tasks.size(); ++local) {
		PreparedFaceTask &task = tasks[local];
		task.face = ordered_faces[batch_begin + local];
		task.brep.reset(new ON_Brep());
		const std::chrono::steady_clock::time_point preparation_started =
		    std::chrono::steady_clock::now();
		STEPEntity::ONStateScope state(&task.on_state);
		if (step)
		    step->SetProgressDetail("preparing exact BREP face topology",
			task.face ? task.face->STEPid() : id,
			completed_faces, total_faces, "faces",
			"shell=#" + std::to_string(id));
		if (!task.face || !task.face->PrepareONBrep(task.brep.get(),
			&task.bounds)) {
		    if (step)
			step->RecordDiagnostic(
			    brlcad::step::DiagnosticSeverity::Error,
			    task.face ? task.face->STEPid() : id,
			    "ADVANCED_FACE", "topology",
			    "could not prepare an independently owned exact face BREP");
		    return false;
		}
		task.preparation_us = static_cast<uint64_t>(
		    std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() -
			preparation_started).count());
	    }
	    const auto finish_face = [&](size_t local) {
		PreparedFaceTask &task = tasks[local];
		const std::chrono::steady_clock::time_point trim_started =
		    std::chrono::steady_clock::now();
		STEPEntity::ONStateScope state(&task.on_state);
		task.finished = task.face && task.face->FinishONBrep(
		    task.brep.get(), task.bounds, shell_scale);
		task.trim_construction_us = static_cast<uint64_t>(
		    std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - trim_started).count());
	    };
	    step->ParallelForGeometry(tasks.size(), finish_face);
	    for (size_t local = 0; local < tasks.size(); ++local) {
		PreparedFaceTask &task = tasks[local];
		if (step) {
		    step->RecordStageTiming("face_construction_pullback",
			task.face ? task.face->STEPid() : id, "ADVANCED_FACE",
			task.preparation_us + task.trim_construction_us,
			task.brep ? static_cast<uint64_t>(task.brep->m_F.Count()) : 0,
			task.brep ? static_cast<uint64_t>(task.brep->m_E.Count()) : 0,
			task.brep ? static_cast<uint64_t>(task.brep->m_T.Count()) : 0);
		}
		if (!task.finished || !task.brep || task.brep->m_F.Count() < 1)
		    return false;
		for (int face_index = 0;
			face_index < task.brep->m_F.Count(); ++face_index)
		    task.brep->m_F[face_index].m_face_user.i = id;
		stitched.Append(*task.brep);
		++completed_faces;
	    }
	    batch_begin = batch_end;
	}
	if (step)
	    step->SetProgressDetail("stitching exact STEP face topology", id,
		completed_faces, total_faces, "faces",
		"shell=#" + std::to_string(id));
	std::string stitch_failure;
	if (!step_stitch_face_breps(&stitched, LocalUnits::tolerance,
		&stitch_failure)) {
	    if (step)
		step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		    id, "CONNECTED_FACE_SET", "cfs_faces",
		    "face-batch topology stitching failed: " + stitch_failure);
	    return false;
	}
	brep->Append(stitched);
	if (step)
	    step->SetProgressDetail("building exact BREP faces", id,
		completed_faces, total_faces, "faces",
		"shell=#" + std::to_string(id));
	return true;
    }
#ifdef _DEBUG_TESTING_
    int facecnt = 0;
#endif
    for (i = cfs_faces.begin(); i != cfs_faces.end(); ++i) {
	if (brlcad::PullbackWorkCancelled()) return false;
	if (step) {
	    step->SetProgressDetail("building exact BREP faces",
		(*i) ? (*i)->STEPid() : id, completed_faces, total_faces,
		"faces", "shell=#" + std::to_string(id));
	}
#ifdef _DEBUG_TESTING_
	if (facecnt != _face_cnt_) {
	    facecnt++;
	    continue;
	    std::cerr << "We're here." << std::endl;
	}
#endif
	const int first_face_index = brep->m_F.Count();
	const int initial_edges = brep->m_E.Count();
	const int initial_trims = brep->m_T.Count();
	const std::chrono::steady_clock::time_point face_started =
	    std::chrono::steady_clock::now();
	const bool face_loaded = (*i)->LoadONBrep(brep);
	if (step) {
	    step->RecordStageTiming("face_construction_pullback",
		(*i) ? (*i)->STEPid() : id, "ADVANCED_FACE",
		static_cast<uint64_t>(
		    std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - face_started).count()),
		1, static_cast<uint64_t>(std::max(0,
		    brep->m_E.Count() - initial_edges)),
		static_cast<uint64_t>(std::max(0,
		    brep->m_T.Count() - initial_trims)));
	}
	if (!face_loaded) {
	    if (step && step->Verbose())
		std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    return false;
	}
	/* Retain authoritative connected-shell membership on every emitted face.
	 * STEP exporters sometimes reuse one EDGE_CURVE across an outer and a void
	 * shell; geometric adjacency alone is then cyclic and cannot recover the
	 * intended two-use topological edge pairing.  ON_Brep documents
	 * m_face_user.i for component labels and does not serialize it. */
	for (int face_index = first_face_index;
		face_index < brep->m_F.Count(); ++face_index)
	    brep->m_F[face_index].m_face_user.i = id;
	++completed_faces;
#ifdef _DEBUG_TESTING_
	facecnt++;
#endif
	}
	if (step)
	    step->SetProgressDetail("building exact BREP faces", id,
		completed_faces, total_faces, "faces",
		"shell=#" + std::to_string(id));
    return true;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
