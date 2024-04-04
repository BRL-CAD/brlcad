#include "opennurbs.h"

#if !defined(ON_COMPILING_OPENNURBS)
// This check is included in all opennurbs source .c and .cpp files to insure
// ON_COMPILING_OPENNURBS is defined when opennurbs source is compiled.
// When opennurbs source is being compiled, ON_COMPILING_OPENNURBS is defined 
// and the opennurbs .h files alter what is declared and how it is declared.
#error ON_COMPILING_OPENNURBS must be defined when compiling opennurbs
#endif

#include "opennurbs_subd_data.h"

ON_SubD* ON_SubDSectorType::SectorRingSubD(
  double radius,
  double sector_angle_radians,
  ON_SubD* subd
  ) const
{
  if (subd)
    *subd = ON_SubD::Empty;

  if (!IsValid())
    return ON_SUBD_RETURN_ERROR(nullptr);

  const unsigned int R = PointRingCount();
  if (R < 3)
    return ON_SUBD_RETURN_ERROR(nullptr);

  const unsigned int F = FaceCount();
  if ( F < 1)
    return ON_SUBD_RETURN_ERROR(nullptr);

  const unsigned int N = EdgeCount();
  if (N < 2)
    return ON_SUBD_RETURN_ERROR(nullptr);

  if (F != N && F + 1 != N)
    return ON_SUBD_RETURN_ERROR(nullptr);

  const ON_SubDVertexTag vertex_tag = VertexTag();
  const unsigned int ring_ei_delta = 2;

  if (nullptr == subd)
    subd = new ON_SubD;

  ON_SubDVertexTag vertex_tag0;
  ON_SubDVertexTag vertex_tag1;
  ON_SubDEdgeTag edge_tag0;
  ON_SubDEdgeTag edge_tag1;

  switch (vertex_tag)
  {
  case ON_SubDVertexTag::Smooth:
    sector_angle_radians = 2.0*ON_PI;
    vertex_tag0 = ON_SubDVertexTag::Smooth;
    vertex_tag1 = ON_SubDVertexTag::Smooth;
    edge_tag0 = ON_SubDEdgeTag::Smooth;
    edge_tag1 = ON_SubDEdgeTag::Smooth;
    break;

  case ON_SubDVertexTag::Crease:
    if ( !(sector_angle_radians > 0.0 && sector_angle_radians < 2.0*ON_PI) )
      sector_angle_radians = 0.5*ON_PI;
    vertex_tag0 = ON_SubDVertexTag::Crease;
    vertex_tag1 = ON_SubDVertexTag::Crease;
    edge_tag0 = ON_SubDEdgeTag::Crease;
    edge_tag1 = ON_SubDEdgeTag::Crease;
    break;

  case ON_SubDVertexTag::Corner:
    sector_angle_radians = CornerSectorAngleRadians();
    vertex_tag0 = ON_SubDVertexTag::Crease;
    vertex_tag1 = ON_SubDVertexTag::Crease;
    edge_tag0 = ON_SubDEdgeTag::Crease;
    edge_tag1 = ON_SubDEdgeTag::Crease;
    break;

  case ON_SubDVertexTag::Dart:
    sector_angle_radians = 2.0*ON_PI;
    vertex_tag0 = ON_SubDVertexTag::Crease;
    vertex_tag1 = ON_SubDVertexTag::Smooth;
    edge_tag0 = ON_SubDEdgeTag::Crease;
    edge_tag1 = ON_SubDEdgeTag::Smooth;
    break;

  default:
    return ON_SUBD_RETURN_ERROR(nullptr);
    break;
  }

  unsigned int sector_angle_index = ON_SubDSectorType::CornerAngleIndexFromCornerAngleRadians(sector_angle_radians);
  if (sector_angle_index <= ON_SubDSectorType::MaximumCornerAngleIndex
    && fabs(ON_SubDSectorType::AngleRadiansFromCornerAngleIndex(sector_angle_index) - sector_angle_radians) <= 1.0e-6
    )
  {
    sector_angle_radians = ON_SubDSectorType::AngleRadiansFromCornerAngleIndex(sector_angle_index);
  }
  else
  {
    sector_angle_radians = ON_UNSET_UINT_INDEX;
  }
  
  const double smooth_edge_w0 = this->SectorCoefficient();

  ON_SimpleArray< ON_SubDVertex* > V(R);
  ON_SimpleArray< ON_SubDEdge* > E(N);

  ON_3dPoint vertexP = ON_3dPoint::Origin;

  for (unsigned int vi = 0; vi < R; vi++)
  {
    ON_SubDVertexTag vertex_tag_vi;
    if ( 0 == vi )
      vertex_tag_vi = vertex_tag; // center vertex
    else if ( 1 == vi )
      vertex_tag_vi = vertex_tag0; // first edge
    else if ( R == vi+1 && N > F )
      vertex_tag_vi = vertex_tag1; // last edge
    else
      vertex_tag_vi = ON_SubDVertexTag::Smooth; // interior edge or an outer face vertex

    if (radius > 0.0)
    {
      double cos_a, sin_a;
      if (sector_angle_index == ON_UNSET_UINT_INDEX)
      {
        double a = (vi / ((double)(R-1)))*sector_angle_radians;
        cos_a = cos(a);
        sin_a = sin(a);
      }
      else
      {
        ON_SubDMatrix::EvaluateCosAndSin(2*sector_angle_index*vi, (R-1)*ON_SubDSectorType::MaximumCornerAngleIndex,&cos_a,&sin_a);
      }
      const double r = (1 == (vi%2)) ? radius : (2.0*radius);
      vertexP.x = r*cos_a;
      vertexP.y = r*sin_a;
    }
    ON_SubDVertex* vertex = subd->AddVertex( vertex_tag_vi, vertexP);
    V.Append(vertex);
  }
  //V[0]->m_vertex_edge_order = ON_SubD::VertexEdgeOrder::radial;
  
  for (unsigned int vei = 0; vei < N; vei++)
  {
    ON_SubDEdgeTag edge_tag_vei;
    if ( 0 == vei )
      edge_tag_vei = edge_tag0; // first edge
    else if ( vei+1 == N )
      edge_tag_vei = edge_tag1; // last edge
    else
      edge_tag_vei = ON_SubDEdgeTag::Smooth; // interior edge

    double w0 = (ON_SubDEdgeTag::Smooth == edge_tag_vei) ? smooth_edge_w0 : ON_SubDSectorType::IgnoredSectorCoefficient;
    unsigned int ev1i = 1 + vei*ring_ei_delta;
    E.Append(
      subd->AddEdgeWithSectorCoefficients(
        edge_tag_vei,
        V[0], w0,
        V[ev1i], ON_SubDSectorType::IgnoredSectorCoefficient)
        );
  }

  ON_SubDVertex* f_vertex[4] = {};
  ON_SubDEdge* f_edge[4] = {};
  ON_SubDEdgePtr f_edgeptr[4] = {};

  f_vertex[0] = V[0];
  f_vertex[3] = const_cast<ON_SubDVertex*>(E[0]->m_vertex[1]);
  f_edge[3] = E[0];
  for (unsigned int vfi = 0; vfi < F; vfi++)
  {
    f_edge[0] = f_edge[3];
    f_edge[3] = E[(vfi + 1) % N];
    f_vertex[1] = const_cast<ON_SubDVertex*>(f_edge[0]->m_vertex[1]);
    f_vertex[3] = const_cast<ON_SubDVertex*>(f_edge[3]->m_vertex[1]);

    f_edgeptr[0] = ON_SubDEdgePtr::Create(f_edge[0], 0);
    f_edgeptr[3] = ON_SubDEdgePtr::Create(f_edge[3], 1);
    f_vertex[2] = V[2 + 2 * vfi];
    f_edge[1] = subd->AddEdgeWithSectorCoefficients(ON_SubDEdgeTag::Smooth, f_vertex[1], ON_SubDSectorType::IgnoredSectorCoefficient, f_vertex[2], ON_SubDSectorType::IgnoredSectorCoefficient);
    f_edge[2] = subd->AddEdgeWithSectorCoefficients(ON_SubDEdgeTag::Smooth, f_vertex[2], ON_SubDSectorType::IgnoredSectorCoefficient, f_vertex[3], ON_SubDSectorType::IgnoredSectorCoefficient);
    f_edgeptr[1] = ON_SubDEdgePtr::Create(f_edge[1], 0);
    f_edgeptr[2] = ON_SubDEdgePtr::Create(f_edge[2], 0);
    subd->AddFace(f_edgeptr,4);
  }

  return subd;
}

static bool TestPoint(
  const ON_3dPoint* SP, 
  unsigned int SPi,
  ON_3dPoint Q,
  unsigned int Pi,
  double* e,
  unsigned int* ei
  )
{
  ON_3dPoint P = SP[SPi];
  double z = fabs(P[(Pi + 1) % 3]) + fabs(P[(Pi + 1) % 3])+ fabs(Q[(Pi + 1) % 3]) + fabs(Q[(Pi + 1) % 3]);
  if (!(0.0 == z))
  {
    // ON_ERROR("point coordinate is not zero.");
    return ON_SUBD_RETURN_ERROR(false);
  }
  if (fabs(P[Pi]) > 1.0)
  {
    // ON_ERROR("point coordinate P[Pi] > 1.");
    return ON_SUBD_RETURN_ERROR(false);
  }
  if (fabs(Q[Pi]) > 1.0)
  {
    // ON_ERROR("point coordinate Q[Pi] > 1.");
    return ON_SUBD_RETURN_ERROR(false);
  }

  double d = fabs(P[Pi] - Q[Pi]);
  if (d > e[Pi])
  {
    e[Pi] = d;
    *ei = SPi;
#if defined(ON_DEBUG)
    if (d > 0.0001)
    {
      // almost certainly a bug
      ON_SubDIncrementErrorCount();
    }
#endif
  }

  return true;
}

static bool ClearCachedPoints(
  unsigned int component_ring_count,
  const ON_SubDComponentPtr* component_ring
  )
{
  if ( component_ring_count < 4 || nullptr == component_ring)
    return ON_SUBD_RETURN_ERROR(false);
  ON_SubDVertex* vertex = component_ring[0].Vertex();
  if ( nullptr == vertex)
    return ON_SUBD_RETURN_ERROR(false);
  vertex->ClearSavedSubdivisionPoints();
  for (unsigned int i = 1; i < component_ring_count; i++)
  {
    ON_SubDEdge* edge = component_ring[i].Edge();
    if ( nullptr == edge)
      return ON_SUBD_RETURN_ERROR(false);
    edge->ClearSavedSubdivisionPoints();
    i++;
    if (i >= component_ring_count)
      break;
    ON_SubDFace* face = component_ring[i].Face();
    if ( nullptr == face)
      return ON_SUBD_RETURN_ERROR(false);
    face->ClearSavedSubdivisionPoints();
  }
  return true;
}

double ON_SubDMatrix::TestEvaluation() const
{
  if ( nullptr == m_S || m_R < 3 )
    return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);

  if (!m_sector_type.IsValid())
    return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);

  const unsigned int F = m_sector_type.FaceCount();
  if (0 == F)
    return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);

  const unsigned int N = m_sector_type.EdgeCount();
  if (0 == N)
    return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);

  const unsigned int R = m_sector_type.PointRingCount();
  if (0 == R)
    return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);
  if (R != m_R)
    return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);
  
  const unsigned f_edge_count = m_sector_type.FacetEdgeCount();
  if (0 == f_edge_count)
    return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);

  double rc = TestMatrix();
  const double*const* S = m_S;
  
  unsigned int SP_low_precision_index = ON_UNSET_UINT_INDEX;
  
  ON_SimpleArray< ON_3dPoint > _P(R);
  ON_3dPoint* SP = _P.Array();

  ON_SimpleArray< double > _Scol(R);
  double* Scol = _Scol.Array();

  ON_SubD subd;
  if (&subd != m_sector_type.SectorRingSubD(0.0,0.0,&subd))
    return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);
  const ON_SubDVertex* vertex0 = subd.FirstVertex();
  if (nullptr == vertex0)
    return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);
  if (N != vertex0->m_edge_count)
    return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);
  if (F != vertex0->m_face_count)
    return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);
  
  ON_SubDSectorIterator sit;
  if ( nullptr == sit.Initialize(vertex0) )
    return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);

  ON_SimpleArray<const ON_SubDVertex*> vertex_ring_array(subd.VertexCount());
  for (const ON_SubDVertex* vertex = vertex0; nullptr != vertex; vertex = vertex->m_next_vertex)
  {
    vertex_ring_array.Append(vertex);
  }
  if ( R != vertex_ring_array.UnsignedCount())
    return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);
  const ON_SubDVertex*const* vertex_ring = vertex_ring_array.Array();

  ON_SimpleArray<ON_SubDComponentPtr> component_ring_array;
  const unsigned int component_ring_count = ON_SubD::GetSectorComponentRing(sit,component_ring_array);
  if ( component_ring_count < 4 || component_ring_count != m_sector_type.ComponentRingCount())
    return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);
  const ON_SubDComponentPtr* component_ring = component_ring_array.Array();

  ON_SimpleArray< ON_3dPoint > _ringP0;

  ON_SimpleArray< ON_3dPoint > _ringP1;

  for (unsigned int vi = 0; vi < R; vi++)
    Scol[vi] = ON_DBL_QNAN;

  for (unsigned int vi = 0; vi < R; vi++)
  {
    double N_vertex_point_precision[3] = { 0 };
    double N_outer_point_precision[3] = { 0 };
    double N_Scol_precision[3] = { 0 };

    for (unsigned int Pi = 0; Pi < 3; Pi++)
    {
      if (false == ClearCachedPoints(component_ring_count,component_ring))
        return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);

      const_cast<ON_SubDVertex*>(vertex_ring[vi])->m_P[Pi] = 1.0;

      if ( R != ON_SubD::GetSectorPointRing(false,component_ring_count,component_ring,_ringP0))
        return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);
      const ON_3dPoint* ringP0 = _ringP0.Array();

      // vertex_ring[]->m_P and ringP0[] should be same point lists
      for (unsigned int i = 0; i < R; i++)
      {
        if (0.0 == ringP0[i][(Pi+1)%3] && 0.0 == ringP0[i][(Pi+2)%3])
        {
          if ( ringP0[i][Pi] == ((i == vi) ? 1.0 : 0.0) )
            continue;
        }
        // vertex_ring[] is not in the expected order or
        // there is a bug in ON_SubD::GetSectorPointRing
        return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);
      }

      if ( R != ON_SubD::GetSectorSubdivisionPointRing(component_ring, component_ring_count,_ringP1))
        return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);
      const ON_3dPoint* ringP1 = _ringP1.Array();

      for (unsigned int i = 0; i < R; i++)
      {
        SP[i] = ON_3dPoint::Origin;
        for (unsigned int j = 0; j < R; j++)
        {
          SP[i] += S[i][j] * ringP0[j];
        }
      }

      if (!(SP[vi][Pi] > 0.0))
      {
        // ON_ERROR("SP[vi][Pi] is not positive.");
        return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);
      }

      if (false == TestPoint(SP, 0, ringP1[0], Pi, N_vertex_point_precision, &SP_low_precision_index))
        return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);

      for (unsigned int j = 1; j < R; j++)
      {
        if (false == TestPoint(SP, j, ringP1[j], Pi, N_outer_point_precision, &SP_low_precision_index))
          return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);
      }

      for (unsigned int i = 0; i < R; i++)
      {
        double d = fabs(S[i][vi] - ringP1[i][Pi]);
        if (d > N_Scol_precision[Pi])
          N_Scol_precision[Pi] = d;
      }

      if (!(N_vertex_point_precision[0] == N_vertex_point_precision[Pi]))
      {
        ON_ERROR("x,y,z vertex point precisions are not identical.");
        return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);
      }
      if (!(N_outer_point_precision[0] == N_outer_point_precision[Pi]))
      {
        ON_ERROR("x,y,z outer point precisions are not identical.");
        return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);
      }
      if (!(N_Scol_precision[0] == N_Scol_precision[Pi]))
      {
        ON_ERROR("x,y,z S column precisions are not identical.");
        return ON_SUBD_RETURN_ERROR(ON_UNSET_VALUE);
      }

      if (rc < N_vertex_point_precision[0])
        rc = N_vertex_point_precision[0];
      if (rc < N_outer_point_precision[0])
        rc = N_outer_point_precision[0];
      if (rc < N_Scol_precision[0])
        rc = N_Scol_precision[0];

      const_cast<ON_SubDVertex*>(vertex_ring[vi])->m_P[Pi] = 0.0;
    }
  }

  return rc; // basic tests passed.
}

static bool GetSectorLimitPointHelper(
  const ON_SubDSectorIterator& sit,
  bool& bUndefinedNormalIsPossible,
  ON_SubDSectorSurfacePoint& limit_point
  )
{
  limit_point.m_limitP[0] = ON_DBL_QNAN;
  limit_point.m_limitP[1] = ON_DBL_QNAN;
  limit_point.m_limitP[2] = ON_DBL_QNAN;

  const ON_SubDSectorType sector_type = ON_SubDSectorType::Create(sit);
  if (false == sector_type.IsValid())
    return  ON_SUBD_RETURN_ERROR(false);

  const unsigned int R = sector_type.PointRingCount();
  if (R < 3)
    return  ON_SUBD_RETURN_ERROR(false);

  double stack_point_ring[41*3];
  double* point_ring = stack_point_ring;
  const unsigned int point_ring_stride = 3;
  unsigned int point_ring_capacity = (unsigned int)(sizeof(stack_point_ring)/(point_ring_stride*sizeof(stack_point_ring[0])));

  if (point_ring_capacity < R )
  {
    point_ring = new(std::nothrow) double[point_ring_stride*R];
    if ( nullptr == point_ring)
      return  ON_SUBD_RETURN_ERROR(false);
    point_ring_capacity = R;
  }

  const unsigned int point_ring_count = ON_SubD::GetSectorPointRing( true, sit, point_ring, point_ring_capacity, point_ring_stride);
  if ( R != point_ring_count )
     return  ON_SUBD_RETURN_ERROR(false);

  bool rc = false;
  for (;;)
  {
    const ON_SubDMatrix& SM = ON_SubDMatrix::FromCache(sector_type);
    if (R != SM.m_R || nullptr == SM.m_LP)
      break;

    if (
      false == bUndefinedNormalIsPossible
      && ON_SubDVertexTag::Crease == SM.m_sector_type.VertexTag()
      && R >= 5
      && *((const ON_3dPoint*)(point_ring+ point_ring_stride)) == *((const ON_3dPoint*)(point_ring + (R-1)* point_ring_stride))
      )
    {
      // Crease where ends of the creased edges are equal.
      // common overlapping creases (happens when a smooth interior edge is separated (unwelded) into two creases)
      bUndefinedNormalIsPossible = true;
    }

    if (false == SM.EvaluateSurfacePoint(point_ring, R, point_ring_stride, bUndefinedNormalIsPossible, limit_point))
      break;

    if (
      false == bUndefinedNormalIsPossible 
      && 0.0 == limit_point.m_limitN[0] && 0.0 == limit_point.m_limitN[1] && 0.0 == limit_point.m_limitN[2]
      && limit_point.IsSet(true)
      )
    {
      // SM.EvaluateSurfacePoint() logged the error - setting bUndefinedNormalIsPossible = true here
      // allows the limit point to be cached so the same error doesn't continue to get logged.
      bUndefinedNormalIsPossible = true;
    }

    rc = true;
    break;
  }

  if ( point_ring != stack_point_ring)
    delete[] point_ring;

  return rc
    ? true
    : ON_SUBD_RETURN_ERROR(false);
}

static ON_SubDSectorSurfacePoint* LimitPointPool(
  const ON_SubDSectorSurfacePoint* pReturnToPool // If nullptr, then one is allocated
  )
{
  static ON_FixedSizePool limit_point_fsp;
  if (0 == limit_point_fsp.SizeofElement())
  {
    if (nullptr != pReturnToPool)
      return ON_SUBD_RETURN_ERROR(nullptr);
    static ON_SleepLock initialize_lock;
    initialize_lock.GetLock();
    if (0 == limit_point_fsp.SizeofElement())
      limit_point_fsp.Create(sizeof(ON_SubDSectorSurfacePoint), 0, 0);
    initialize_lock.ReturnLock();
  }

  if (nullptr != pReturnToPool)
  {
    limit_point_fsp.ThreadSafeReturnElement((void*)pReturnToPool);
    return nullptr;
  }

  ON_SubDSectorSurfacePoint* lp = (ON_SubDSectorSurfacePoint*)limit_point_fsp.ThreadSafeAllocateDirtyElement();  
  if (nullptr == lp)
    return ON_SUBD_RETURN_ERROR(nullptr);
  return lp;
}

bool ON_SubDVertex::GetSurfacePoint(
  const ON_SubDFace* sector_face,
  ON_SubDSectorSurfacePoint& limit_point
) const
{
  return GetSurfacePoint( sector_face, false, limit_point);
}

bool ON_SubDVertex::GetSurfacePoint(
  const ON_SubDFace* sector_face,
  bool bUndefinedNormalIsPossible,
  ON_SubDSectorSurfacePoint& limit_point
  ) const
{
  bool rc = false;
  ON_SubDSectorIterator sit;
  const ON_SubDFace* limit_point_sector_face = nullptr;
  
  if (nullptr != sector_face)
  {
    for (unsigned int vfi = 0; vfi < m_face_count; vfi++)
    {
      if (sector_face == m_faces[vfi])
      {
        rc = true;
        break;
      }
    }
    if (false == rc)
    {
      // sector_face is not referenced by this vertex
      limit_point = ON_SubDSectorSurfacePoint::Unset;
      return ON_SUBD_RETURN_ERROR(false);
    }
  }
  
  if (this->SurfacePointIsSet() )
  {
    if (nullptr == m_limit_point.m_sector_face && nullptr == m_limit_point.m_next_sector_limit_point)
    {
      // single sector
      limit_point = m_limit_point;
      limit_point.m_next_sector_limit_point = nullptr;
      return true;
    }

    if (nullptr == sector_face)
    {
      // this vertex has multiple sectors
      limit_point = ON_SubDSectorSurfacePoint::Unset;
      return ON_SUBD_RETURN_ERROR(false);
    }

    if (nullptr == sit.Initialize(sector_face, 0, this))
    {
      limit_point = ON_SubDSectorSurfacePoint::Unset;
      return ON_SUBD_RETURN_ERROR(false);
    }

    limit_point_sector_face = sit.IncrementToCrease(-1);

    if (nullptr == limit_point_sector_face)
    {
      // no creases 
      limit_point = ON_SubDSectorSurfacePoint::Unset;
      return ON_SUBD_RETURN_ERROR(false);
    }
    
    for (const ON_SubDSectorSurfacePoint* lp = &m_limit_point; nullptr != lp; lp = lp->m_next_sector_limit_point)
    {
      if (limit_point_sector_face == lp->m_sector_face)
      {
        limit_point = *lp;
        limit_point.m_next_sector_limit_point = nullptr;
        return true;
      }
    }

    // cache does not contain this limit point.
  }

  if (nullptr == (nullptr == sector_face ? sit.Initialize(this) : sit.Initialize(sector_face, 0, this)))
  {
    limit_point = ON_SubDSectorSurfacePoint::Unset;
    return ON_SUBD_RETURN_ERROR(false);
  }

  limit_point_sector_face = sit.IncrementToCrease(-1);

  rc = GetSectorLimitPointHelper( sit, bUndefinedNormalIsPossible, limit_point);

  if (false == rc)
  {
    limit_point = ON_SubDSectorSurfacePoint::Unset;
    return ON_SUBD_RETURN_ERROR(false);
  }
    
  limit_point.m_sector_face = this->IsSingleSectorVertex() ? nullptr : limit_point_sector_face;
  
  ON_SubDSectorSurfacePoint saved_limit_point = limit_point;
  saved_limit_point.m_next_sector_limit_point = (ON_SubDSectorSurfacePoint*)1; // causes unnecessary test to be skipped
  SetSavedSurfacePoint( bUndefinedNormalIsPossible, saved_limit_point);

  return rc;
}

bool ON_SubDVertex::GetSavedSurfacePoint(
  double limit_point[3]
) const
{
  const bool rc = SurfacePointIsSet();
  if (rc)
  {
    limit_point[0] = m_limit_point.m_limitP[0];
    limit_point[1] = m_limit_point.m_limitP[1];
    limit_point[2] = m_limit_point.m_limitP[2];
  }
  return rc;
}

const ON_3dPoint ON_SubDVertex::SurfacePoint() const
{
  ON_3dPoint limit_point(ON_3dPoint::NanPoint);
  return GetSurfacePoint(&limit_point.x) ? limit_point : ON_3dPoint::NanPoint;
}

const ON_3dVector ON_SubDVertex::SurfaceNormal(
  const ON_SubDFace* sector_face,
  bool bUndefinedNormalPossible
) const
{
  for (;;)
  {
    if (m_face_count < 1 || nullptr == m_faces)
    {
      ON_ERROR("No faces on this vertex.");
      break;
    }

    if (nullptr == sector_face && IsCreaseOrCorner())
    {
      const ON_SubDComponentPtrPair crease_pair = this->CreasedEdgePair(false);
      const ON_SubDEdge* e[2] = { crease_pair.First().Edge(), crease_pair.Second().Edge() };
      if (nullptr == e[0] || 1 != e[0]->m_face_count || nullptr == e[1] || 1 != e[1]->m_face_count)
      {
        ON_ERROR("sector_face must be specified in this case.");
        break;
      }
    }

    if (nullptr == sector_face)
      sector_face = m_faces[0];
    ON_SubDSectorSurfacePoint limit_point;
    bool rc = ON_SubDVertex::GetSurfacePoint(
      sector_face,
      bUndefinedNormalPossible,
      limit_point
    );
    if (false == rc)
      break;
    const ON_3dVector N(limit_point.m_limitN);
    if (false == bUndefinedNormalPossible && N.IsZero())
      break;
    return N;
  }

  return ON_3dVector::NanVector;
}

const ON_SubDSectorSurfacePoint& ON_SubDVertex::SectorSurfacePointForExperts() const
{
  return this->m_limit_point;
}

const ON_Plane ON_SubDVertex::VertexFrame(
  ON_SubDComponentLocation subd_appearance
) const
{
  if (0 == FaceCount())
    return ON_Plane::NanPlane;

  const ON_SubDFace* sector_face = Face(0);
  if (nullptr == sector_face)
    return ON_Plane::NanPlane;

  ON_Plane vertex_frame(ON_Plane::NanPlane);
  if (ON_SubDComponentLocation::ControlNet == subd_appearance)
  {
    ON_3dVector V = ON_3dVector::ZeroVector;
    for (int vei = 0; vei < m_edge_count; ++vei)
    {
      const ON_SubDEdge* e = Edge(vei);
      if (nullptr == e)
        continue;
      const ON_SubDVertex* v1 = e->OtherEndVertex(this);
      if (nullptr == v1)
        continue;
      const ON_SubDFace* f = (1 == e->m_face_count) ? e->Face(0) : nullptr;
      if (nullptr == f)
        continue;
      sector_face = f;
      V = (v1->ControlNetPoint() - ControlNetPoint()).UnitVector();
      break;
    }
    vertex_frame.CreateFromNormal(ControlNetPoint(), sector_face->ControlNetCenterNormal());
    const ON_3dVector X = (V - (V * vertex_frame.zaxis) * vertex_frame.zaxis).UnitVector();
    if (X.IsUnitVector())
    {
      vertex_frame.xaxis = X;
      vertex_frame.yaxis = ON_CrossProduct(vertex_frame.zaxis, vertex_frame.xaxis).UnitVector();
    }
  }
  else
  {
    // If this is a smooth vertex or a crease vertex on the boundary,
    // then the sector_face does not matter. Otherwise it picks the
    // "side of the crease" for the normal.
    ON_SubDSectorSurfacePoint limit_point;
    if (FaceCount())
      if (false == GetSurfacePoint(sector_face, limit_point))
        return ON_Plane::NanPlane;

    ON_3dVector Y(ON_CrossProduct(limit_point.m_limitN, limit_point.m_limitT1));
    Y.Unitize();

    // The normal is more important than the tangent direction. 
    vertex_frame.CreateFromNormal(ON_3dPoint(limit_point.m_limitP), ON_3dVector(limit_point.m_limitN));
    vertex_frame.yaxis = Y;
    vertex_frame.xaxis = ON_CrossProduct(vertex_frame.yaxis, vertex_frame.zaxis);
    vertex_frame.xaxis.Unitize();
  }

  return vertex_frame.IsValid() ? vertex_frame : ON_Plane::NanPlane;
}


const ON_Plane ON_SubDEdge::CenterFrame(
  ON_SubDComponentLocation subd_appearance
) const
{
  // to fix RH-41763, get the limit mesh fragment for an attached face
  // and use subd.LimitSurfaceMesh().GetEdgeCenterPointAndNormal()
  // to get P and N.
  ON_Plane edge_frame(ON_Plane::NanPlane);
  ON_3dPoint center_point(ON_3dPoint::NanPoint);
  ON_3dVector center_normal(ON_3dVector::NanVector);
  bool rc = false;
  // surface center and normal are not available in public opennurbs
  center_point = ControlNetCenterPoint();
  center_normal = ControlNetCenterNormal(0);
  rc = center_point.IsValid() && center_normal.IsUnitVector();
  if (rc)
  {
    if (false == edge_frame.CreateFromNormal(center_point, center_normal))
      return ON_Plane::NanPlane;
    const ON_3dVector U = ControlNetDirection();
    ON_2dVector v(U * edge_frame.xaxis, U * edge_frame.yaxis);
    if ( v.Unitize() )
    {
      if (fabs(v.y) > ON_SQRT_EPSILON&& fabs(v.x) < (1.0 - ON_SQRT_EPSILON))
      {
        const ON_3dVector X = (v.x * edge_frame.xaxis + v.y * edge_frame.yaxis).UnitVector();
        if (X.IsUnitVector())
        {
          const ON_3dVector Y = ON_CrossProduct(edge_frame.zaxis, X).UnitVector();
          if (Y.UnitVector())
          {
            edge_frame.xaxis = X;
            edge_frame.yaxis = Y;
          }
        }
      }
      else if (v.x < 0.0)
      {
        edge_frame.xaxis = -edge_frame.xaxis;
        edge_frame.yaxis = -edge_frame.yaxis;
      }
    }
  }
  return edge_frame.IsValid() ? edge_frame : ON_Plane::NanPlane;
}



const ON_3dPoint ON_SubDVertex::Point(ON_SubDComponentLocation point_location) const
{
  switch (point_location)
  {
  case ON_SubDComponentLocation::ControlNet:
    return this->ControlNetPoint();
    break;
  case ON_SubDComponentLocation::Surface:
    return this->SurfacePoint();
    break;
  }
  return ON_3dPoint::NanPoint;
}



bool ON_SubDVertex::GetSurfacePoint(
  double limit_point[3]
) const
{
  if (nullptr == limit_point)
    return false;

  bool rc = SurfacePointIsSet();
  if ( rc )
  {
    limit_point[0] = m_limit_point.m_limitP[0];
    limit_point[1] = m_limit_point.m_limitP[1];
    limit_point[2] = m_limit_point.m_limitP[2];
  }
  else
  {
    ON_SubDSectorSurfacePoint lp;
    rc = GetSurfacePoint(Face(0), true, lp);
    if (rc)
    {
      limit_point[0] = lp.m_limitP[0];
      limit_point[1] = lp.m_limitP[1];
      limit_point[2] = lp.m_limitP[2];
    }
    else
    {
      limit_point[0] = ON_DBL_QNAN;
      limit_point[1] = ON_DBL_QNAN;
      limit_point[2] = ON_DBL_QNAN;
    }
  }
  return rc;
}

static bool SetLimitPointSectorCheck(
  const ON_SubDVertex* vertex,
  ON_SubDSectorSurfacePoint& limit_point
  )
{
  const unsigned int vertex_face_count = vertex->m_face_count;
  if ( vertex_face_count < 1 || nullptr == vertex->m_faces)
      return ON_SUBD_RETURN_ERROR(false);

  ON_SubDSectorIterator sit;

  const ON_SubDFace* limit_point_sector_face = limit_point.m_sector_face;

  if (nullptr != limit_point_sector_face)
  {
    bool rc = false;
    for (unsigned int vfi = 0; vfi < vertex_face_count; vfi++)
    {
      if (limit_point_sector_face == vertex->m_faces[vfi])
      {
        rc = true;
        break;
      }
    }
    if (false == rc)      
      return ON_SUBD_RETURN_ERROR(false); // sector_face is not referenced by this vertex
    if (nullptr == sit.Initialize(limit_point_sector_face, 0, vertex))
      return ON_SUBD_RETURN_ERROR(false);
  }
  else if (nullptr == sit.Initialize(vertex))
    return ON_SUBD_RETURN_ERROR(false);

  limit_point_sector_face = sit.IncrementToCrease(-1);
  unsigned int sector_face_count = 0;
  const ON_SubDFace* sector_face0 = sit.CurrentFace();
  for (const ON_SubDFace* sector_face = sector_face0; nullptr != sector_face && sector_face_count <= vertex_face_count; sector_face = sit.NextFace(ON_SubDSectorIterator::StopAt::AnyCrease))
  {
    if (sector_face == sector_face0 && sector_face_count == vertex_face_count && vertex->IsSmoothOrDart())
    {
      // interior vertex
      limit_point_sector_face = nullptr;
      break;
    }
    sector_face_count++;
  }

  if (sector_face_count > vertex_face_count)
  {
    // error in topology information
    return ON_SUBD_RETURN_ERROR(false);
  }

  if (sector_face_count == vertex_face_count)
  {
    // vertex has 1 sector (bounded or interior)
    limit_point_sector_face = nullptr;
  }
  else if (nullptr == limit_point_sector_face )
  {
    // vertex has multiple sectors and
    // limit_point.m_sector_face must be the "first" face in the sector
    return ON_SUBD_RETURN_ERROR(false);
  }

  limit_point.m_sector_face = limit_point_sector_face;
  return true;
}

bool ON_SubDVertex::SetSavedSurfacePoint(
  bool bUndefinedNormalIsPossible,
  ON_SubDSectorSurfacePoint limit_point
  ) const
{
  const bool bSkipSectorCheck = (1U == (ON__UINT_PTR)limit_point.m_next_sector_limit_point);
  limit_point.m_next_sector_limit_point = nullptr;

  if ( limit_point.IsSet(bUndefinedNormalIsPossible)
    && (bSkipSectorCheck || SetLimitPointSectorCheck(this,limit_point))
    )
  {
    if (nullptr == limit_point.m_sector_face 
      || ON_UNSET_VALUE == m_limit_point.m_limitP[0]
      || false == Internal_SurfacePointFlag()
      )
    {
      // vertex has 1 sector or this is the first cached limit point
      this->ClearSavedSurfacePoints();
      m_limit_point = limit_point;
      m_limit_point.m_next_sector_limit_point = nullptr;
    }
    else
    {
      // get a point from the pool
      ON_SubDSectorSurfacePoint* lp = LimitPointPool(nullptr);
      if ( nullptr == lp)
        return ON_SUBD_RETURN_ERROR(false);

      // set the point
      *lp = limit_point;

      // first limit point location wins
      ON_SubDMeshFragment::SealPoints(true, m_limit_point.m_limitP, lp->m_limitP);

      // it is expected that the limit normal and tangents will be substantually different.
      lp->m_next_sector_limit_point = nullptr;

      // append the point to the vertex's linked list.
      const ON_SubDSectorSurfacePoint* p = &m_limit_point;
      while(nullptr != p->m_next_sector_limit_point)
      {
        p = p->m_next_sector_limit_point;
      }

      const_cast<ON_SubDSectorSurfacePoint*>(p)->m_next_sector_limit_point = lp;
    }

    Internal_SetSavedSurfacePointFlag(true);

    return true;
  }

  return ON_SUBD_RETURN_ERROR(false);
}


void ON_SubDVertex::ClearSavedSurfacePoints() const
{
  // clear vertex specific cache
  Internal_ClearSurfacePointFlag();
  if (nullptr != m_limit_point.m_next_sector_limit_point)
  {
    // return multiple sector limit points to pool
    const ON_SubDSectorSurfacePoint* next_p = m_limit_point.m_next_sector_limit_point;
    m_limit_point.m_next_sector_limit_point = nullptr;
    for (const ON_SubDSectorSurfacePoint* p = next_p; nullptr != p; p = next_p)
    {
      next_p = p->m_next_sector_limit_point;
      LimitPointPool(p);
    }
  }
  m_limit_point = ON_SubDSectorSurfacePoint::Unset;
}

void ON_SubDVertex::ClearSavedSubdivisionPoints() const
{
  // clear vertex specific limit point cache
  ClearSavedSurfacePoints();
  // clear general subdivision point cache
  ON_SubDComponentBase::Internal_ClearSubdivisionPointAndSurfacePointFlags();
}

void ON_SubDVertex::ClearSavedSubdivisionPoints(bool bClearNeighborhood) const
{
  ClearSavedSubdivisionPoints();
  if (false == bClearNeighborhood)
    return;

  for (unsigned short vei = 0; vei < m_edge_count; ++vei)
  {
    const ON_SubDEdge* e = ON_SUBD_EDGE_POINTER(m_edges[vei].m_ptr);
    if (nullptr == e)
      continue;
    e->ClearSavedSubdivisionPoints();
    const ON_SubDVertex* other_v = e->OtherEndVertex(this);
    if (nullptr != other_v)
      other_v->ClearSavedSubdivisionPoints();
  }
  for (unsigned short vfi = 0; vfi < m_face_count; ++vfi)
  {
    const ON_SubDFace* f = m_faces[vfi];
    if (nullptr == f)
      continue;
    f->ClearSavedSubdivisionPoints();
    const ON_SubDEdgePtr* eptr = f->m_edge4;
    for (unsigned short fei = 0; fei < f->m_edge_count; ++fei, ++eptr)
    {
      if (4 == fei)
      {
        eptr = f->m_edgex;
        if (nullptr == eptr)
          break;
      }
      const ON_SubDEdge* e = ON_SUBD_EDGE_POINTER(eptr->m_ptr);
      if (nullptr == e)
        continue;
      e->ClearSavedSubdivisionPoints();
      if (nullptr != e->m_vertex[0])
        e->m_vertex[0]->ClearSavedSubdivisionPoints();
      if (nullptr != e->m_vertex[1])
        e->m_vertex[1]->ClearSavedSubdivisionPoints();
    }
  }
}

bool ON_SubDVertex::SurfacePointIsSet() const
{
  const bool rc = Internal_SurfacePointFlag();
  if (false == rc)
  {
    this->ClearSavedSurfacePoints();
  }
  return rc;
}


void ON_SubDEdge::ClearSavedSubdivisionPoints() const
{
  // considering using a global pool for the limit curve cache - not yet.
  ON_SubDComponentBase::Internal_ClearSubdivisionPointAndSurfacePointFlags();
}

void ON_SubDEdge::ClearSavedSubdivisionPoints(bool bClearNeighborhood) const
{
  ClearSavedSubdivisionPoints();
  if (false == bClearNeighborhood)
    return;

  for (unsigned evi = 0; evi < 2; ++evi)
  {
    const ON_SubDVertex* v = m_vertex[evi];
    if (nullptr == v)
      continue;
    v->ClearSavedSubdivisionPoints();
  }

  const ON_SubDFacePtr* fptr = this->m_face2;
  for (unsigned short efi = 0; efi < m_face_count; ++efi, ++fptr)
  {
    if (2 == efi)
    {
      fptr = this->m_facex;
      if (nullptr == fptr)
        break;
    }
    const ON_SubDFace* f = ON_SUBD_FACE_POINTER(fptr->m_ptr);
    if (nullptr != f)
      f->ClearSavedSubdivisionPoints();
  }
}

const ON_SubDMeshFragment * ON_SubDFace::MeshFragments() const
{
  // NOTE:
  // Clearing the ON_SubDComponentBase::SavedPointsFlags::SurfacePointBit bit
  // on m_saved_points_flags is used to mark mesh fragmants as dirty. 
  // They need to be regerated before being used.
  return (0 != ON_SUBD_CACHE_LIMITLOC_FLAG(m_saved_points_flags)) ? m_mesh_fragments : nullptr;
}

void ON_SubDFace::ClearSavedSubdivisionPoints() const
{
  // considering using a global pool for the limit fragment cache - not yet.
  ON_SubDComponentBase::Internal_ClearSubdivisionPointAndSurfacePointFlags();
}

void ON_SubDFace::ClearSavedSubdivisionPoints(bool bClearNeighborhood) const
{
  ClearSavedSubdivisionPoints();
  if (false == bClearNeighborhood)
    return;

  const ON_SubDEdgePtr* eptr = this->m_edge4;
  for (unsigned short fei = 0; fei < this->m_edge_count; ++fei, ++eptr)
  {
    if (4 == fei)
    {
      eptr = this->m_edgex;
      if (nullptr == eptr)
        break;
    }
    const ON_SubDEdge* e = ON_SUBD_EDGE_POINTER(eptr->m_ptr);
    if (nullptr == e)
      continue;
    e->ClearSavedSubdivisionPoints();
    for (unsigned evi = 0; evi < 2; ++evi)
    {
      const ON_SubDVertex* v = e->m_vertex[evi];
      if (nullptr == v)
        continue;
      v->ClearSavedSubdivisionPoints();
    }    
  }
}


unsigned int ON_SubDVertex::VertexId() const
{
  return m_id;
}

unsigned int ON_SubDEdge::EdgeId() const
{
  return m_id;
}

unsigned int ON_SubDFace::FaceId() const
{
  return m_id;
}

const ON_3dPoint ON_SubDFace::ControlNetCenterPoint() const
{
  if (m_edge_count < 3)
    return ON_3dPoint::NanPoint;
  ON_3dPoint center(0.0, 0.0, 0.0);
  const ON_SubDEdgePtr* eptr = m_edge4;
  for (unsigned short fvi = 0; fvi < m_edge_count; ++fvi, ++eptr)
  {
    if (4 == fvi)
    {
      eptr = m_edgex;
      if ( nullptr == eptr)
        return ON_3dPoint::NanPoint;
    }
    const ON_SubDVertex* v = eptr->RelativeVertex(0);
    if ( nullptr == v)
      return ON_3dPoint::NanPoint;
    center += v->ControlNetPoint();
  }
  const double c = (double)m_edge_count;
  center.x /= c;
  center.y /= c;
  center.z /= c;
  return center;
}

const ON_3dVector ON_SubDFace::ControlNetCenterNormal() const
{
  if (4 == m_edge_count)
  {
    // faster code for most common case
    const ON_SubDVertex* v[4] = { m_edge4[0].RelativeVertex(0),m_edge4[1].RelativeVertex(0),m_edge4[2].RelativeVertex(0),m_edge4[3].RelativeVertex(0)};
    if (nullptr == v[0] || nullptr == v[1] || nullptr == v[2] || nullptr == v[3])
      return ON_3dVector::NanVector;
    const ON_3dVector A = ON_3dPoint(v[2]->m_P) - ON_3dPoint(v[0]->m_P);
    const ON_3dVector B = ON_3dPoint(v[3]->m_P) - ON_3dPoint(v[1]->m_P);
    const ON_3dVector N = ON_CrossProduct(A, B);
    return N.UnitVector();
  }

  if (3 == m_edge_count)
  {
    // faster code for 2nd most common case
    const ON_SubDVertex* v[3] = { m_edge4[0].RelativeVertex(0),m_edge4[1].RelativeVertex(0),m_edge4[2].RelativeVertex(0)};
    if (nullptr == v[0] || nullptr == v[1] || nullptr == v[2])
      return ON_3dVector::NanVector;
    const ON_3dPoint C0(v[0]->m_P);
    const ON_3dVector A = ON_3dPoint(v[1]->m_P) - C0;
    const ON_3dVector B = ON_3dPoint(v[2]->m_P) - C0;
    const ON_3dVector N = ON_CrossProduct(A, B);
    return N.UnitVector();
  }

  // less common cases
  const ON_3dPoint C = ControlNetCenterPoint();
  if (false == (C.x == C.x))
    return ON_3dVector::NanVector;

  // If the face is planar (convex or non-convex), this will return
  // the plane's normal with the correct orientation.
  // Otherwise this will return a good choice of normal for
  // a non-planar face.
  ON_3dVector A;
  ON_3dVector B = ControlNetPoint(m_edge_count - 1) - C;
  ON_3dVector N = ON_3dVector::ZeroVector;
  for (unsigned short fvi = 0; fvi < m_edge_count; ++fvi)
  {
    A = B;
    B = ControlNetPoint(fvi) - C;
    N += ON_CrossProduct(A, B); // do not untize this and then the code works for non-convex faces too.
  }

  return N.UnitVector();
}

bool ON_SubDFace::IsConvex() const
{
  const ON_3dVector N = ControlNetCenterNormal();
  if (N.IsNotZero())
  {
    if (3 == m_edge_count)
      return true;
    ON_3dPoint P = ControlNetPoint(m_edge_count - 2);
    ON_3dPoint Q = ControlNetPoint(m_edge_count - 1);
    ON_3dVector A;
    ON_3dVector B = Q - P;
    for (unsigned short fvi = 0; fvi < m_edge_count; ++fvi)
    {
      P = Q;
      Q = ControlNetPoint(fvi);
      A = B;
      B = Q - P;
      const ON_3dVector AxB = ON_CrossProduct(A, B);
      const double d = AxB*N;
      if ( false == (d > 0.0) && false == AxB.IsZero() )
        return false;
    }
    return true;
  }
  return false;
}

bool ON_SubDFace::IsNotConvex() const
{
  const ON_3dVector N = ControlNetCenterNormal();
  if (N.IsNotZero())
  {
    if (3 == m_edge_count)
      return false;
    bool bIsNotConvex = false;
    ON_3dPoint P = ControlNetPoint(m_edge_count - 2);
    ON_3dPoint Q = ControlNetPoint(m_edge_count - 1);
    ON_3dVector A;
    ON_3dVector B = Q - P;
    for (unsigned short fvi = 0; fvi < m_edge_count; ++fvi)
    {
      P = Q;
      Q = ControlNetPoint(fvi);
      A = B;
      B = Q - P;
      const ON_3dVector AxB = ON_CrossProduct(A, B);
      const double d = AxB * N;
      if (false == (d == d))
        return false; // nan
      if (false == (d > 0.0) && false == AxB.IsZero())
        bIsNotConvex = true;
    }
    return bIsNotConvex;
  }
  return false;
}

bool ON_SubDFace::IsPlanar(double planar_tolerance) const
{
  const ON_3dPoint C = ControlNetCenterPoint();
  const ON_3dVector N = ControlNetCenterNormal();
  if (C.IsValid() && N.IsNotZero())
  {
    if (3 == m_edge_count)
      return true;
    if (false == (planar_tolerance >= 0.0))
      planar_tolerance = ON_ZERO_TOLERANCE;
    const ON_SubDEdgePtr* eptr = m_edge4;
    double d0 = 0.0;
    double d1 = 0.0;
    for (unsigned short fvi = 0; fvi < m_edge_count; ++fvi, ++eptr)
    {
      if (4 == fvi)
      {
        eptr = m_edgex;
        if (nullptr == eptr)
          return false;
      }
      const ON_SubDVertex* v = eptr->RelativeVertex(0);
      if (nullptr == v)
        return false;
      const double d = (ON_3dPoint(v->m_P) - C)*N;
      if (false == (d == d))
        return false; // nan
      if (d < d0)
        d0 = d;
      else if (d > d1)
        d1 = d;
      else
        continue;
      if ((d1 - d0) > planar_tolerance)
        return false;
    }
    return true;
  }
  return false;
}


bool ON_SubDFace::IsNotPlanar(double planar_tolerance) const
{
  const ON_3dPoint C = ControlNetCenterPoint();
  const ON_3dVector N = ControlNetCenterNormal();
  if (C.IsValid() && N.IsNotZero())
  {
    if (3 == m_edge_count)
      return false;
    if (false == (planar_tolerance >= 0.0))
      planar_tolerance = ON_ZERO_TOLERANCE;
    const ON_SubDEdgePtr* eptr = m_edge4;
    double d0 = 0.0;
    double d1 = 0.0;
    bool bNotPlanar = false;
    for (unsigned short fvi = 0; fvi < m_edge_count; ++fvi, ++eptr)
    {
      if (4 == fvi)
      {
        eptr = m_edgex;
        if (nullptr == eptr)
          return false;
      }
      const ON_SubDVertex* v = eptr->RelativeVertex(0);
      if (nullptr == v)
        return false;
      const double d = (ON_3dPoint(v->m_P) - C)*N;
      if (false == (d == d))
        return false; // nan
      if (d < d0)
        d0 = d;
      else if (d > d1)
        d1 = d;
      else
        continue;
      if ((d1 - d0) > planar_tolerance)
        bNotPlanar = true;
    }
    return bNotPlanar;
  }
  return false;
}


const ON_Plane ON_SubDFace::ControlNetCenterFrame() const
{
  const ON_3dPoint C = ControlNetCenterPoint();
  const ON_3dVector N = ControlNetCenterNormal();
  if (C.IsValid() && N.IsNotZero())
  {
    // TODO - better choices for x and y axes
    ON_Plane center_frame;
    if (center_frame.CreateFromNormal(C, N))
      return center_frame;
  }
  return ON_Plane::NanPlane;
}
