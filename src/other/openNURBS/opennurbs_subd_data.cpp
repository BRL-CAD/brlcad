#include "opennurbs.h"

#if !defined(ON_COMPILING_OPENNURBS)
// This check is included in all opennurbs source .c and .cpp files to insure
// ON_COMPILING_OPENNURBS is defined when opennurbs source is compiled.
// When opennurbs source is being compiled, ON_COMPILING_OPENNURBS is defined 
// and the opennurbs .h files alter what is declared and how it is declared.
#error ON_COMPILING_OPENNURBS must be defined when compiling opennurbs
#endif

#include "opennurbs_subd_data.h"

/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2014 Robert McNeel & Associates. All rights reserved.
// OpenNURBS, Rhinoceros, and Rhino3D are registered trademarks of Robert
// McNeel & Associates.
//
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
// ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR PURPOSE AND OF
// MERCHANTABILITY ARE HEREBY DISCLAIMED.
//
// For complete openNURBS copyright information see <http://www.opennurbs.org>.
//
////////////////////////////////////////////////////////////////
*/

const ON_SubDLevel ON_SubDLevel::Empty;

//////////////////////////////////////////////////////////////////////////
//
// ON_SubDimple
//


ON_SubDimple::ON_SubDimple()
  : RuntimeSerialNumber(++ON_SubDimple::Internal_RuntimeSerialNumberGenerator)
{}

ON_SubDimple::~ON_SubDimple()
{
  Destroy();
}

ON_SubDHeap& ON_SubDimple::Heap()
{
  return m_heap;
}

void ON_SubDimple::Clear()
{
  m_subd_appearance = ON_SubD::DefaultSubDAppearance;
  m_texture_coordinate_type = ON_SubDTextureCoordinateType::Unset;
  m_texture_mapping_tag = ON_MappingTag::Unset;
  m_fragment_colors_mapping_tag = ON_MappingTag::Unset;
  m_fragment_texture_settings_hash = ON_SHA1_Hash::EmptyContentHash;
  m_fragment_colors_settings_hash = ON_SHA1_Hash::EmptyContentHash;
  for (unsigned i = 0; i < m_levels.UnsignedCount(); ++i)
  {
    ON_SubDLevel* level = m_levels[i];
    if (nullptr != level)
    {
      m_levels[i] = nullptr;
      delete level;
    }
  }
  m_levels.SetCount(0);
  m_active_level = nullptr;
  m_heap.Clear();
  m_face_packing_id = ON_nil_uuid;
  m_face_packing_topology_hash = ON_SubDHash::Empty;
  m_symmetry = ON_Symmetry::Unset;
}

void  ON_SubDimple::ClearLevelContents(
  ON_SubDLevel* level
)
{
  if (nullptr == level)
    return;

  if (level == m_active_level)
    ChangeGeometryContentSerialNumber(false);

  level->ResetFaceArray();
  level->ResetEdgeArray();
  level->ResetVertexArray();

  ON_SubDVertex* next_vertex = level->m_vertex[0];
  level->m_vertex[0] = nullptr;
  level->m_vertex[1] = nullptr;

  ON_SubDEdge* next_edge = level->m_edge[0];
  level->m_edge[0] = nullptr;
  level->m_edge[1] = nullptr;

  ON_SubDFace* next_face = level->m_face[0];
  level->m_face[0] = nullptr;
  level->m_face[1] = nullptr;

  for (ON_SubDVertex* vertex = next_vertex; nullptr != vertex; vertex = next_vertex)
  {
    next_vertex = const_cast<ON_SubDVertex*>(vertex->m_next_vertex);
    ReturnVertex(vertex);
  }

  for (ON_SubDEdge* edge = next_edge; nullptr != edge; edge = next_edge)
  {
    next_edge = const_cast<ON_SubDEdge*>(edge->m_next_edge);
    ReturnEdge(edge);
  }

  for (ON_SubDFace* face = next_face; nullptr != face; face = next_face)
  {
    next_face = const_cast<ON_SubDFace*>(face->m_next_face);
    ReturnFace(face);
  }

}

unsigned int ON_SubDimple::ClearHigherSubdivisionLevels(
  unsigned int max_level_index
  )
{
  const unsigned int original_level_count = m_levels.UnsignedCount();

  if (max_level_index+1 < m_levels.UnsignedCount())
  {
    unsigned int level_count = m_levels.UnsignedCount();
    if (nullptr != m_active_level && m_active_level->m_level_index > max_level_index)
    {
      if ( level_count > max_level_index )
      {
        m_active_level = m_levels[max_level_index];
        ChangeGeometryContentSerialNumber(false);
      }
    }

    while (level_count > max_level_index+1)
    {
      const unsigned int level_index = (level_count-1);
      ON_SubDLevel* level = m_levels[level_index];
      m_levels[level_index] = nullptr;
      m_levels.Remove();
      level_count--;
      if (level_count != m_levels.UnsignedCount())
      {
        Clear();
        break;
      }

      if ( nullptr == level )
        continue;

      ClearLevelContents(level);

      delete level;
    }
  }

  return original_level_count - m_levels.UnsignedCount();
}


unsigned int ON_SubDimple::ClearLowerSubdivisionLevels(
  unsigned int min_level_index
  )
{
  const unsigned int original_level_count = m_levels.UnsignedCount();

  if (min_level_index > 0 && min_level_index < original_level_count)
  {
    if (nullptr != m_active_level && m_active_level->m_level_index < min_level_index)
    {
      m_active_level = m_levels[min_level_index];
      ChangeGeometryContentSerialNumber(false);
    }

    for ( unsigned int level_index = 0; level_index < min_level_index; level_index++)
    {
      ON_SubDLevel* level = m_levels[level_index];
      m_levels[level_index] = nullptr;
      if ( nullptr == level )
        continue;
      ClearLevelContents(level);
      delete level;
    }
    
    unsigned short new_level_index = 0;
    for (unsigned int level_index = min_level_index; level_index < original_level_count; level_index++, new_level_index++)
    {
      ON_SubDLevel* level = m_levels[level_index];
      m_levels[level_index] = nullptr;
      if ( nullptr == level )
        continue;
      level->m_level_index = new_level_index;

      for (ON_SubDVertex* vertex = level->m_vertex[0]; nullptr != vertex; vertex = const_cast<ON_SubDVertex*>(vertex->m_next_vertex))
      {
        vertex->SetSubdivisionLevel(new_level_index);
      }

      for (ON_SubDEdge* edge = level->m_edge[0]; nullptr != edge; edge = const_cast<ON_SubDEdge*>(edge->m_next_edge))
      {
        edge->SetSubdivisionLevel(new_level_index);
      }

      for (ON_SubDFace* face = level->m_face[0]; nullptr != face; face = const_cast<ON_SubDFace*>(face->m_next_face))
      {
        face->SetSubdivisionLevel(new_level_index);
        face->m_level_zero_face_id = face->m_id;
      }

      m_levels[new_level_index] = level;
    }
    m_levels.SetCount(new_level_index);
  }

  return original_level_count - m_levels.UnsignedCount();
}

unsigned int ON_SubDimple::ClearInactiveLevels()
{
  const unsigned active_level_index = this->ActiveLevelIndex();
  unsigned c1 = ClearHigherSubdivisionLevels(active_level_index);
  unsigned c0 = ClearLowerSubdivisionLevels(active_level_index);
  return c0 + c1;
}

void ON_SubDimple::Destroy()
{
  const unsigned int level_count = m_levels.Count();
  for (unsigned int level_index = 0; level_index < level_count; level_index++)
  {
    ON_SubDLevel* level = m_levels[level_index];
    if ( nullptr == level )
      continue;
    m_levels[level_index] = nullptr;
    delete level;
  }
  m_levels.Destroy();
  m_heap.Destroy();
  m_subd_geometry_content_serial_number = 0;
  m_subd_render_content_serial_number = 0;
}

ON_SubDLevel* ON_SubDimple::ActiveLevel(
  bool bCreateIfNeeded)
{
  if (nullptr == m_active_level)
  {
    unsigned int level_index = (m_levels.UnsignedCount() > 0) ? (m_levels.UnsignedCount()-1) : 0U;
    m_active_level = SubDLevel(level_index,bCreateIfNeeded && 0 == m_levels.UnsignedCount());    
    ChangeGeometryContentSerialNumber(false);
  }
  return m_active_level;
}

class ON_SubDLevel* ON_SubDimple::SubDLevel(
  unsigned int level_index,
  bool bCreateIfNeeded
  )
{
  ON_SubDLevel* level = nullptr;
  if (level_index < m_levels.UnsignedCount())
    level = m_levels[level_index];
  else if (bCreateIfNeeded && level_index == m_levels.UnsignedCount())
  {
    level = new ON_SubDLevel();
    level->m_level_index = level_index;
    m_levels.Append(level);
    if (nullptr == m_active_level)
    {
      m_active_level = level;
      ChangeGeometryContentSerialNumber(false);
    }
  }

  return level;
}

class ON_SubDLevel const * ON_SubDimple::SubDLevel(
  unsigned int level_index
  ) const
{
  if (level_index < m_levels.UnsignedCount())
    return m_levels[level_index];
  return nullptr;
}

void ON_SubDAggregates::UpdateBoundingBox(
  const ON_SubDLevel* level
  )
{
  ON_BoundingBox bbox = ON_BoundingBox::EmptyBoundingBox;
  if (nullptr != level)
  {
    double x;
    for (const ON_SubDVertex* v = level->m_vertex[0]; nullptr != v; v = v->m_next_vertex)
    {
      if (v->m_P[0] == v->m_P[0] && v->m_P[1] == v->m_P[1] && v->m_P[2] == v->m_P[2])
      {
        bbox.m_min.x = v->m_P[0];
        bbox.m_min.y = v->m_P[1];
        bbox.m_min.z = v->m_P[2];
        bbox.m_max.x = bbox.m_min.x;
        bbox.m_max.y = bbox.m_min.y;
        bbox.m_max.z = bbox.m_min.z;
        for (v = v->m_next_vertex; nullptr != v; v = v->m_next_vertex)
        {
          x = v->m_P[0];
          if (x < bbox.m_min.x) bbox.m_min.x = x; else if (x > bbox.m_max.x) bbox.m_max.x = x;
          x = v->m_P[1];
          if (x < bbox.m_min.y) bbox.m_min.y = x; else if (x > bbox.m_max.y) bbox.m_max.y = x;
          x = v->m_P[2];
          if (x < bbox.m_min.z) bbox.m_min.z = x; else if (x > bbox.m_max.z) bbox.m_max.z = x;
        }
        break;
      }
    }
  }
  m_controlnet_bbox = bbox;
  m_bDirtyBoundingBox = false;
}

ON_BoundingBox ON_SubDLevel::ControlNetBoundingBox() const
{
  if ( m_aggregates.m_bDirtyBoundingBox )
    m_aggregates.UpdateBoundingBox(this);
  return m_aggregates.m_controlnet_bbox;
}

void  ON_SubDAggregates::UpdateTopologicalAttributes(
  const ON_SubDLevel* level
)
{
  m_topological_attributes = 0;
  if (nullptr == level)
    return;

  if (m_bDirtyBoundingBox)
  {
    UpdateBoundingBox(level);
    if (m_bDirtyBoundingBox)
      return;
  }

  bool bIsManifold = level->m_edge_count >= 3 && level->m_face_count >= 1;
  bool bIsOriented = bIsManifold;
  bool bHasBoundary = false;

  for (const ON_SubDEdge* e = level->m_edge[0]; nullptr != e; e = e->m_next_edge)
  {
    if (1 == e->m_face_count)
    {
      bHasBoundary = true;
      if (false == bIsManifold && false == bIsOriented)
        break;
    }
    else if (2 == e->m_face_count)
    {
      if (ON_SUBD_FACE_DIRECTION(e->m_face2[0].m_ptr) == ON_SUBD_FACE_DIRECTION(e->m_face2[1].m_ptr))
      {
        bIsOriented = false;
        if (bHasBoundary && false == bIsManifold)
          break;
      }
    }
    else
    {
      bIsManifold = false;
      bIsOriented = false;
      if (bHasBoundary)
        break;
    }
  }

  double vol = 0.0;
  if (bIsManifold && bIsOriented && false == bHasBoundary)
  {
    const ON_3dVector B(m_controlnet_bbox.IsValid() ? ON_3dVector(m_controlnet_bbox.Center()) : ON_3dVector::ZeroVector);
    ON_3dVector P, Q, R;
    for (const ON_SubDFace* f = level->m_face[0]; nullptr != f && vol == vol; f = f->m_next_face)
    {
      if (false == f->GetSubdivisionPoint( &P.x))
      {
        vol = ON_DBL_QNAN;
        break;
      }
      P -= B;
      const unsigned count = f->EdgeCount();
      if (count < 3)
      {
        vol = ON_DBL_QNAN;
        break;
      }
      const ON_SubDVertex* v = f->Vertex(count - 1);
      if (nullptr == v || false == v->GetSubdivisionPoint( &R.x))
      {
        vol = ON_DBL_QNAN;
        break;
      }
      R -= B;
      for (unsigned fvi = 0; fvi < count; fvi++)
      {
        Q = R;
        v = f->Vertex(fvi);
        if (nullptr == v || false == v->GetSubdivisionPoint( &R.x))
        {
          vol = ON_DBL_QNAN;
          break;
        }
        R -= B;
        // ON_TripleProduct(P, Q, R) = 6x signed volume of tetrahedron with trangle base (P,Q,R) and apex B.
        vol += ON_TripleProduct(P, Q, R);
      }
    }
  }

  // bit 1 indicates m_topological_attributes is set.
  m_topological_attributes = 1;

  if (bIsManifold)
    m_topological_attributes |= 2;
  if (bIsOriented)
    m_topological_attributes |= 4;
  if (bHasBoundary)
    m_topological_attributes |= 8;
  
  if (vol > 0.0)
    m_topological_attributes |= 16;
  else if (vol < 0.0)
    m_topological_attributes |= 32;
}

bool ON_SubDAggregates::GetTopologicalAttributes(bool & bIsManifold, bool & bIsOriented, bool & bHasBoundary, int & solid_orientation) const
{
  // if m_bDirtyBoundingBox is true, then m_topological_attributes is dirty as well.
  const unsigned int topological_attributes = m_bDirtyBoundingBox ? 0U : m_topological_attributes;

  bIsManifold = 0 != (2 & topological_attributes);
  bIsOriented = 0 != (4 & topological_attributes);
  bHasBoundary = 0 != (8 & topological_attributes);
  if (bIsManifold && bIsOriented && false == bHasBoundary)
  {
    if (0 != (16 & topological_attributes))
      solid_orientation = 1;
    else if (0 != (32 & topological_attributes))
      solid_orientation = -1;
    else
      solid_orientation = 2;
  }
  else
    solid_orientation = 0;

  return (0 != topological_attributes);
}

bool ON_SubDAggregates::GetTopologicalAttributes(const ON_SubDLevel * level, bool &bIsManifold, bool & bIsOriented, bool & bHasBoundary, int & solid_orientation)
{
  if ( (m_bDirtyBoundingBox || 0 == m_topological_attributes) && nullptr != level)
    UpdateTopologicalAttributes(level);
  return GetTopologicalAttributes(bIsManifold, bIsOriented, bHasBoundary, solid_orientation);
}

const ON_AggregateComponentStatusEx ON_SubDLevel::AggregateComponentStatus() const
{
  if (false == m_aggregates.m_aggregate_status.IsCurrent())
    m_aggregates.UpdateAggregateComponentStatus(this);
  return m_aggregates.m_aggregate_status;
}

ON__UINT64 ON_SubDLevel::ComponentStatusSerialNumber() const
{
  return m_aggregates.m_aggregate_status.ComponentStatusSerialNumber();
}

void ON_SubDAggregates::UpdateAggregateEdgeAttributes(
  const ON_SubDLevel* level
  )
{
  if (nullptr != level)
  {
    unsigned int bits = 0;
    for (const ON_SubDEdge* e = level->m_edge[0]; nullptr != e; e = e->m_next_edge)
      bits |= e->EdgeAttributes();
    m_aggregate_edge_attributes = bits;
  }
  m_bDirtyEdgeAttributes = false;
}

unsigned int ON_SubDLevel::EdgeFlags() const
{
  if (m_aggregates.m_bDirtyEdgeAttributes)
    m_aggregates.UpdateAggregateEdgeAttributes(this);
  return m_aggregates.m_aggregate_edge_attributes;
}

unsigned int ON_SubD::AggregateEdgeAttributes() const
{
  return ActiveLevel().EdgeFlags();
}

void ON_SubDAggregates::UpdateAggregateComponentStatus(
  const ON_SubDLevel* level
  )
{
  m_aggregate_status = ON_AggregateComponentStatus::Empty;
  if (nullptr != level)
  {
    for (const ON_SubDVertex* v = level->m_vertex[0]; nullptr != v; v = v->m_next_vertex)
      m_aggregate_status.Add(v->m_status);
    for (const ON_SubDEdge* e = level->m_edge[0]; nullptr != e; e = e->m_next_edge)
      m_aggregate_status.Add(e->m_status);
    for (const ON_SubDFace* f = level->m_face[0]; nullptr != f; f = f->m_next_face)
      m_aggregate_status.Add(f->m_status);
  }
}


//////////////////////////////////////////////////////////////////////////
//
// ON_SubDLevel
//

std::shared_ptr<const ON_SubDVertex*> ON_SubDLevel::VertexArray() const
{
  if (m_vertex_count != m_vertex_array_count || nullptr == m_vertex_array.get())
  {
    ON_SubDVertex const** a = new ON_SubDVertex const*[m_vertex_count];
    ON_SubDVertex const** a1 = a + m_vertex_count;
    const_cast<ON_SubDLevel*>(this)->m_vertex_array = std::shared_ptr<const ON_SubDVertex*>(a);
    for (const ON_SubDVertex* v = m_vertex[0]; nullptr != v && a < a1; v = v->m_next_vertex)
      *a++ = v;
    while (a < a1)
      *a++ = nullptr;
    const_cast<ON_SubDLevel*>(this)->m_vertex_array_count = m_vertex_count;
  }
  return m_vertex_array;
}

std::shared_ptr<const ON_SubDEdge*> ON_SubDLevel::EdgeArray() const
{
  if (m_edge_count != m_edge_array_count || nullptr == m_edge_array.get())
  {
    ON_SubDEdge const** a = new ON_SubDEdge const*[m_edge_count];
    ON_SubDEdge const** a1 = a + m_edge_count;
    const_cast<ON_SubDLevel*>(this)->m_edge_array = std::shared_ptr<const ON_SubDEdge*>(a);
    for (const ON_SubDEdge* v = m_edge[0]; nullptr != v && a < a1; v = v->m_next_edge)
      *a++ = v;
    while (a < a1)
      *a++ = nullptr;
    const_cast<ON_SubDLevel*>(this)->m_edge_array_count = m_edge_count;
  }
  return m_edge_array;
}

std::shared_ptr<const ON_SubDFace*> ON_SubDLevel::FaceArray() const
{
  if (m_face_count != m_face_array_count || nullptr == m_face_array.get())
  {
    ON_SubDFace const** a = new ON_SubDFace const*[m_face_count];
    ON_SubDFace const** a1 = a + m_face_count;
    const_cast<ON_SubDLevel*>(this)->m_face_array = std::shared_ptr<const ON_SubDFace*>(a);
    for (const ON_SubDFace* v = m_face[0]; nullptr != v && a < a1; v = v->m_next_face)
      *a++ = v;
    while (a < a1)
      *a++ = nullptr;
    const_cast<ON_SubDLevel*>(this)->m_face_array_count = m_face_count;
  }
  return m_face_array;
}

//////////////////////////////////////////////////////////////////////////
//
// ON_SubD::Tranxform
// ON_SubDimple::Transform
// ON_SubDLevel::Transform
// ON_SubDVertex::Transform
// ON_SubDEdge::Transform
// ON_SubDFace::Transform
//

static void TransformPoint(
  const double* xform,
  double P[3]
  )
{
  const double x = xform[0] * P[0] + xform[1] * P[1] + xform[2] * P[2] + xform[3];
  const double y = xform[4] * P[0] + xform[5] * P[1] + xform[6] * P[2] + xform[7];
  const double z = xform[8] * P[0] + xform[9] * P[1] + xform[10] * P[2] + xform[11];
  const double w = xform[12] * P[0] + xform[13] * P[1] + xform[14] * P[2] + xform[15];
  if (1.0 == w)
  {
    P[0] = x;
    P[1] = y;
    P[2] = z;
  }
  else
  {
    P[0] = x / w;
    P[1] = y / w;
    P[2] = z / w;
  }
}

static void TransformVector(
  const double* xform,
  double V[3]
  )
{
  const double x = xform[0] * V[0] + xform[1] * V[1] + xform[2] * V[2];
  const double y = xform[4] * V[0] + xform[5] * V[1] + xform[6] * V[2];
  const double z = xform[8] * V[0] + xform[9] * V[1] + xform[10] * V[2];
  V[0] = x;
  V[1] = y;
  V[2] = z;
}

bool ON_SubDSectorSurfacePoint::Transform(
  const ON_Xform& xform
  )
{
  TransformPoint(&xform.m_xform[0][0],m_limitP);
  TransformVector(&xform.m_xform[0][0],m_limitT1);
  TransformVector(&xform.m_xform[0][0],m_limitT2);
  ON_3dVector N = ON_CrossProduct(m_limitT1,m_limitT2);
  bool rc = N.Unitize();
  m_limitN[0] = N.x;
  m_limitN[1] = N.y;
  m_limitN[2] = N.z;
  return rc;
}


bool ON_SubDVertex::Transform(
  bool bTransformationSavedSubdivisionPoint,
  const class ON_Xform& xform
  )
{
  TransformPoint(&xform.m_xform[0][0],m_P);

  Internal_TransformComponentBase(bTransformationSavedSubdivisionPoint, xform);

  // TODO:
  //   If the vertex 
  //     is tagged as ON_SubDVertexTag::Corner
  //     and bTransformationSavedSubdivisionPoint is true, 
  //     and the corner sector(s) contains interior smooth edges,
  //     and the transformation changes the angle between a corner sector's crease boundary, 
  //   then the sector's interior smooth edge's m_sector_coefficient[] could change
  //   and invalidate the subdivison points and limit points.
  //   This is only possible for uncommon (in practice) transformations
  //   and corner sectors and will require a fair bit of testing for 
  //   now it's easier to simply set bTransformationSavedSubdivisionPoint to false
  //   at a higher level when these types of transformations are encountered.
  if ( bTransformationSavedSubdivisionPoint && Internal_SurfacePointFlag() )
  {
    for (const ON_SubDSectorSurfacePoint* lp = &m_limit_point; nullptr != lp; lp = lp->m_next_sector_limit_point)
      const_cast<ON_SubDSectorSurfacePoint*>(lp)->Transform(xform);
  }
  else
    Internal_ClearSurfacePointFlag();

  return true;
}

bool ON_SubDVertex::SetControlNetPoint(
  ON_3dPoint control_net_point,
  bool bClearNeighborhoodCache
)
{
  if (false == control_net_point.IsValid())
    return false;

  if (!(m_P[0] == control_net_point.x && m_P[1] == control_net_point.y && m_P[2] == control_net_point.z))
  {
    m_P[0] = control_net_point.x;
    m_P[1] = control_net_point.y;
    m_P[2] = control_net_point.z;
    ClearSavedSubdivisionPoints();

    if (bClearNeighborhoodCache)
    {
      for (unsigned short vei = 0; vei < m_edge_count; vei++)
      {
        ON_SubDEdge* edge = ON_SUBD_EDGE_POINTER(m_edges[vei].m_ptr);
        if (nullptr == edge)
          continue;
        edge->ClearSavedSubdivisionPoints();
        ON_SubDFacePtr* fptr = edge->m_face2;
        for (unsigned short efi = 0; efi < edge->m_face_count; efi++, fptr++)
        {
          if (2 == efi)
          {
            fptr = edge->m_facex;
            if (nullptr == fptr)
              break;
          }
          ON_SubDFace* face = ON_SUBD_FACE_POINTER(fptr->m_ptr);
          if (nullptr == face)
            continue;
          face->ClearSavedSubdivisionPoints();

          ON_SubDEdgePtr* eptr = face->m_edge4;
          for (unsigned short fei = 0; fei < face->m_edge_count; fei++, eptr++)
          {
            if (4 == fei)
            {
              eptr = face->m_edgex;
              if (nullptr == eptr)
                break;
            }
            ON_SubDEdge* fedge = ON_SUBD_EDGE_POINTER(eptr->m_ptr);
            if (nullptr == fedge)
              continue;
            ON_SubDVertex* fvertex = const_cast<ON_SubDVertex*>(fedge->m_vertex[ON_SUBD_EDGE_DIRECTION(eptr->m_ptr)]);
            if (nullptr == fvertex)
              continue;
            fvertex->ClearSavedSubdivisionPoints();
          }
        }
      }
    }
  }

  return true;
}

void ON_SubDComponentBase::Internal_TransformComponentBase(
  bool bTransformationSavedSubdivisionPoint,
  const class ON_Xform& xform
  )
{
  if ( SavedSubdivisionPointIsSet() )
  {
    if (bTransformationSavedSubdivisionPoint)
      TransformPoint(&xform.m_xform[0][0], m_saved_subd_point1);
    else
      ON_SUBD_CACHE_CLEAR_POINT_FLAG(m_saved_points_flags);
  }
}

bool ON_SubDEdge::Transform(
  bool bTransformationSavedSubdivisionPoint,
  const class ON_Xform& xform
  )
{
  Internal_TransformComponentBase(bTransformationSavedSubdivisionPoint, xform);

    Internal_ClearSurfacePointFlag();

  return true;
}

bool ON_SubDFace::Transform(
  bool bTransformationSavedSubdivisionPoint,
  const class ON_Xform& xform
  )
{
  Internal_TransformComponentBase(bTransformationSavedSubdivisionPoint, xform);

  if (bTransformationSavedSubdivisionPoint && Internal_SurfacePointFlag() )
  {
    for (ON_SubDMeshFragment* f = m_mesh_fragments; nullptr != f; f = f->m_next_fragment)
      f->Transform(xform);
  }
  else
    Internal_ClearSurfacePointFlag();

  return true;
}

bool ON_SubDLevel::Transform(
  bool bTransformationSavedSubdivisionPoint,
  const class ON_Xform& xform
  )    
{
  bool rc = true;

  m_aggregates.m_bDirtyBoundingBox = true;

  for (const ON_SubDVertex* vertex = m_vertex[0]; nullptr != vertex; vertex = vertex->m_next_vertex)
  {
    if (false == const_cast<ON_SubDVertex*>(vertex)->Transform(bTransformationSavedSubdivisionPoint, xform))
      rc = false;
  }
  
  for (const ON_SubDEdge* edge = m_edge[0]; nullptr != edge; edge = edge->m_next_edge)
  {
    if (false == const_cast<ON_SubDEdge*>(edge)->Transform(bTransformationSavedSubdivisionPoint, xform))
      rc = false;
  }
  
  for (const ON_SubDFace* face = m_face[0]; nullptr != face; face = face->m_next_face)
  {
    if (false == const_cast<ON_SubDFace*>(face)->Transform(bTransformationSavedSubdivisionPoint, xform))
      rc = false;
  }

  if (false == m_surface_mesh.Transform(xform))
    rc = false;
  
  if (false == m_control_net_mesh.Transform(xform))
    rc = false;

  if (rc)
    return true;

  return ON_SUBD_RETURN_ERROR(false);
}


bool ON_SubDMesh::Transform(
  const ON_Xform& xform
  )
{
  if (false == xform.IsValid())
    return false;
  if (xform.IsIdentity())
    return true;
  if (xform.IsZero())
    return false;
  ON_SubDMeshImpl* impl = m_impl_sp.get();
  if ( nullptr == impl )
    return true; // transform applied to empty mesh is true on purpose.  Changing to false will break other code.
  return impl->Transform(xform);
}

bool ON_SubDimple::Transform(
  const ON_Xform& xform
  )
{
  const bool bSymmetricInput = m_symmetry.SameSymmetricObjectGeometry(this);
  const ON_Symmetry symmetry0 = m_symmetry;
  const ON__UINT64 gsn0 = this->GeometryContentSerialNumber();

  const bool bUpdateFacePackingHash
    = m_face_packing_topology_hash.m_subd_geometry_content_serial_number == gsn0
    && m_face_packing_topology_hash.IsNotEmpty()
    && m_face_packing_topology_hash.SubDHash() == this->SubDHash(ON_SubDHashType::TopologyAndEdgeCreases, false).SubDHash()
    ;


  if (false == xform.IsValid())
    return false;
  if (xform.IsZero())
    return true;
  if (xform.IsIdentity())
    return true;

  const unsigned int level_count = m_levels.UnsignedCount();
  if ( level_count <= 0 )
    return true; // transform applied to empty subd is true on purpose.

  bool rc = true;

  // If 
  // 1) The transformation is being applied to every vertex, edge and 
  //    face in every level of a subdivision object, and
  // 2) the transformation is an isometry (rotation, translation, ...),
  //   a uniform scale, or a composition of these types, 
  // then set bTransformationSavedSubdivisionPoint = true to apply the
  // transformation to saved subdivision and saved limit point information.
  // In all other cases, set bTransformationSavedSubdivisionPoint = false
  // and any saved subdivision points or saved limit points will be
  // deleted.
  const bool bTransformationSavedSubdivisionPoint = false; // todo - set this correctly

  for (unsigned int level_index = 0; level_index < level_count; level_index++)
  {
    ON_SubDLevel* level = m_levels[level_index];
    if (nullptr == level)
    {
      ON_SubDIncrementErrorCount();
      continue;
    }

    if (false == level->Transform(bTransformationSavedSubdivisionPoint, xform))
    {
      rc = false;
      break;
    }
  }

  // SubD has been moved - geometry changed and we need to bump the geometry content serial number.
  this->ChangeGeometryContentSerialNumber(false);

  // GeometryContentSerial number trackers need to be updated
  // so the SubD knows its status with respect to the 
  // newly transformed geometry.

  if (bUpdateFacePackingHash)
    m_face_packing_topology_hash = this->SubDHash(ON_SubDHashType::TopologyAndEdgeCreases, false);

  if (m_symmetry.IsSet())
  {
    m_symmetry = m_symmetry.TransformConditionally(xform);
    bool bSymmetricOutput = false;
    if (bSymmetricInput)
    {
      // see if the transformed object will still be symmetric.
      if (ON_Symmetry::Coordinates::Object == m_symmetry.SymmetryCoordinates())
      {
        // object is still symmetric.
        bSymmetricOutput = true;
      }
      else if (ON_Symmetry::Coordinates::World == m_symmetry.SymmetryCoordinates())
      {
        // if transform didn't move the symmetry
        if ( 0 == ON_Symmetry::CompareSymmetryTransformation(&symmetry0, &m_symmetry, ON_UNSET_VALUE) )
          bSymmetricOutput = true;
      }
    }
    if (bSymmetricOutput)
    {
      if (ON_Symmetry::Coordinates::Object == m_symmetry.SymmetryCoordinates())
      {
        // symmetry constraints transformed with object
        m_symmetry.SetSymmetricObject(this);
      }
      else
      {
        // object moved with respect to symmetry contstraints
        // DO NOTHING HERE - the serial number and hashes on m_symmetry will inform downstream processes
        // that the object no longer has the symmetry property specified by m_symmetry.
        // It will get updated when appropriate - typically in replace object.
        // EXAMPLE: Make a SubD plane - reflect it across the world Y axis. 
        // Then rotate the plane a bit. The rotated plane gets fixed in replace object.
      }
    }
    else
    {
      // input was already dirty - remove all object settings from m_symmetry.
      m_symmetry.ClearSymmetricObject();
    }
  }
  else
  {
    m_symmetry = ON_Symmetry::Unset;
  }

  return rc;
}

bool ON_SubDMeshFragment::Transform(
  const ON_Xform& xform
  )
{
  const unsigned count = PointCount();
  if (0 == count)
  {
    m_surface_bbox = ON_BoundingBox::EmptyBoundingBox;
    return true;
  }
  if ( false == ON_TransformPointList(3,false, count,(int)m_P_stride,m_P,xform) )
    return ON_SUBD_RETURN_ERROR(false);
  if (count == NormalCount())
  {
    if (false == ON_TransformVectorList(3, count, (int)m_N_stride, m_N, xform))
      return ON_SUBD_RETURN_ERROR(false);
  }
  if (0 != (ON_SubDMeshFragment::EtcControlNetQuadBit & m_vertex_count_etc))
  {
    for (int i = 0; i < 4; i++)
    {
      const ON_3dPoint A(m_ctrlnetP[i]);
      if (A.IsValid())
      {
        const ON_3dPoint B = xform * A;
        m_ctrlnetP[i][0] = B.x;
        m_ctrlnetP[i][1] = B.y;
        m_ctrlnetP[i][2] = B.z;
      }
    }
  }
  if (0 != (ON_SubDMeshFragment::EtcControlNetQuadBit & m_vertex_capacity_etc))
  {
    const ON_3dVector A(m_ctrlnetN);
    if (A.IsNotZero())
    {
      ON_3dVector B = xform * A;
      if ( A.IsUnitVector() && false == B.IsUnitVector() )
        B = B.UnitVector();
      m_ctrlnetN[0] = B.x;
      m_ctrlnetN[1] = B.y;
      m_ctrlnetN[2] = B.z;
    }
  }
  ON_GetPointListBoundingBox(3,0,count,(int)m_P_stride,m_P,&m_surface_bbox.m_min.x,&m_surface_bbox.m_max.x,false);
  return true;
}

bool ON_SubDMeshImpl::Transform(
  const ON_Xform& xform
  )
{
  m_bbox = ON_BoundingBox::EmptyBoundingBox;
  ON_BoundingBox bbox = ON_BoundingBox::EmptyBoundingBox;
  for ( const ON_SubDMeshFragment* fragment = m_first_fragment; nullptr != fragment; fragment = fragment->m_next_fragment)
  {
    if ( false == const_cast<ON_SubDMeshFragment*>(fragment)->Transform(xform) )
      return ON_SUBD_RETURN_ERROR(false);
    if ( fragment == m_first_fragment )
      bbox = fragment->m_surface_bbox;
    else
      bbox.Union(fragment->m_surface_bbox);
  }
  m_bbox = bbox;
  ChangeContentSerialNumber();
  return true;
}

//////////////////////////////////////////////////////////////////////////
//
//
//
//

ON_BoundingBox ON_SubDVertex::ControlNetBoundingBox() const
{
  ON_BoundingBox bbox;
  bbox.m_min = m_P;
  bbox.m_min = bbox.m_min;
  return bbox;
}



const ON_BoundingBox ON_SubDEdge::ControlNetBoundingBox() const
{
  ON_BoundingBox bbox;
  if (nullptr != m_vertex[0] && nullptr != m_vertex[1])
  {
    ON_3dPoint P[2];
    P[0] = m_vertex[0]->m_P;
    P[1] = m_vertex[1]->m_P;
    ON_GetPointListBoundingBox(3, 0, 2, 3, &P[0].x, &bbox.m_min.x, &bbox.m_max.x,false);
  }
  return bbox;
}

const ON_BoundingBox ON_SubDFace::ControlNetBoundingBox() const
{
  ON_BoundingBox bbox;
  ON_3dPoint P[16];
  unsigned int P_count = 0;
  const unsigned int P_capacity = (unsigned int)(sizeof(P) / sizeof(P[0]));
  bool bGrowBox = false;

  const unsigned int count = m_edge_count;
  for (unsigned int i = 0; i < count; i++)
  {
    const ON_SubDVertex* vertex = Vertex(i);
    if (nullptr == vertex)
      continue;
    P[P_count++] = vertex->m_P;
    if (P_count == P_capacity)
    {
      ON_GetPointListBoundingBox(3, 0, P_count, 3, &P[0].x, &bbox.m_min.x, &bbox.m_max.x, bGrowBox);
      P_count = 0;
      bGrowBox = true;
    }
  }

  if ( P_count > 0)
    ON_GetPointListBoundingBox(3, 0, P_count, 3, &P[0].x, &bbox.m_min.x, &bbox.m_max.x, bGrowBox);

  return bbox;
}

bool ON_Symmetry::SameSymmetricObjectGeometry(const class ON_SubD* subd) const
{
  const ON_SubDimple* subdimple = (nullptr != subd) ? subd->SubDimple() : nullptr;
  return SameSymmetricObjectGeometry(subdimple);
}

bool ON_Symmetry::SameSymmetricObjectTopology(const class ON_SubD* subd) const
{
  const ON_SubDimple* subdimple = (nullptr != subd) ? subd->SubDimple() : nullptr;
  return SameSymmetricObjectTopology(subdimple);
}

bool ON_Symmetry::SameSymmetricObjectGeometry(const class ON_SubDimple* subdimple) const
{
  if (this->IsSet() && m_symmetric_object_content_serial_number != 0 && nullptr != subdimple)
  {
    const ON__UINT64 subd_gsn = subdimple->GeometryContentSerialNumber();
    if (m_symmetric_object_content_serial_number == subd_gsn)
      return true; // speedy check worked
    if (m_symmetric_object_geometry_hash.IsSet() && m_symmetric_object_geometry_hash == subdimple->SubDHash(ON_SubDHashType::Geometry, false).SubDHash())
      return true;
  }
  return false;
}

bool ON_Symmetry::SameSymmetricObjectTopology(const class ON_SubDimple* subdimple) const
{
  if (this->IsSet() && m_symmetric_object_content_serial_number != 0 && nullptr != subdimple)
  {
    const ON__UINT64 subd_gsn = subdimple->GeometryContentSerialNumber();
    if (m_symmetric_object_content_serial_number == subd_gsn)
      return true; // speedy check worked (same geometry in fact!)
    if (m_symmetric_object_topology_hash.IsSet() && m_symmetric_object_topology_hash == subdimple->SubDHash(ON_SubDHashType::Topology, false).SubDHash())
      return true;
  }
  return false;
}
