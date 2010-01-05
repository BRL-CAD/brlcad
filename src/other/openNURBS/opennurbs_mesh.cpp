/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2007 Robert McNeel & Associates. All rights reserved.
// Rhinoceros is a registered trademark of Robert McNeel & Assoicates.
//
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
// ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR PURPOSE AND OF
// MERCHANTABILITY ARE HEREBY DISCLAIMED.
//				
// For complete openNURBS copyright information see <http://www.opennurbs.org>.
//
////////////////////////////////////////////////////////////////
*/

#include "opennurbs.h"

// function pointers to mesh creation tools in Rhino SDK
ON_MeshNurbsSurface   ON_NurbsSurface::_MeshNurbsSurface = 0;
ON_MeshPlaneSurface   ON_PlaneSurface::_MeshPlaneSurface = 0;
ON_MeshRevSurface     ON_RevSurface::_MeshRevSurface = 0;
ON_MeshSumSurface     ON_SumSurface::_MeshSumSurface = 0;
ON_MeshBrepFace       ON_BrepFace::_MeshBrepFace = 0;

ON_MassPropertiesCurve     ON_Curve::_MassPropertiesCurve = 0;
ON_fnMeshCurve             ON_Curve::_MeshCurve = 0;
ON_MassPropertiesSurface   ON_Surface::_MassPropertiesSurface = 0;
ON_MassPropertiesMesh      ON_Mesh::_MassPropertiesMesh = 0;

ON__m__Brep_Mesh            ON_Brep::m__Mesh = 0;
ON__m__Brep_MassProperties  ON_Brep::m__MassProperties = 0;
ON__m__Brep_SplitFaces      ON_Brep::m__SplitFaces = 0;
ON__m__Brep_RegionTopologyHelper ON_Brep::m__RegionTopologyHelper = 0;
ON__m__Brep_MergeBrepsHelper ON_Brep::m__MergeBrepsHelper = 0;
ON__m__Brep_IsPointInsideHelper ON_Brep::m__IsPointInsideHelper = 0;

ON_fnMesh2dPolygon ON_Mesh::_Mesh2dPolygon = 0;

static // LEAVE THIS STATIC
void ON_MeshSurfaceHelper(
            ON_Mesh* mesh,
            int vcnt_s,
            int vcnt_t,
            int srfIsClosed[2], // 0 = no, 1 = clsd, 2 = per
            int srfIsSingular[4]
            )
{
  const int fcnt_s = vcnt_s-1;
  const int fcnt_t = vcnt_t-1;
  int i, j, k, m, n;
  ON_MeshFace mf;

  // airtight seams on closed surfaces
  for ( i = 0; i < 2; i++ )
  {
    bool bIsClosed = (0!=srfIsClosed[i]);
    if ( !bIsClosed )
      continue;
    bool bIsPeriodic = (2==srfIsClosed[i]);
    // j = first face
    // n = j increment
    // k = max value of j
    // m = offset of matching vertex
    if ( i)
    {
      k = vcnt_s*vcnt_t;
      n = vcnt_t;
      m = vcnt_t-1;
    }
    else
    {
      k = vcnt_t;
      n = 1;
      m = vcnt_t*(vcnt_s-1);
    }
    for ( j = 0; j < k; j += n )
    {
      mesh->m_V[j] = mesh->m_V[j+m];
      if ( bIsPeriodic )
        mesh->m_N[j] = mesh->m_N[j+m];
    }
  }

  // airtight corners and triangles at singular points
  int vdelta=0; // keeps lint happy
  int fvi0=0, fvi1=0, vindx=0, sing_count=0;
  for ( i = 0; i < 4; i++ )
  {
    // i: 0=south, 1=east, 2=north, 3=west;
    if ( !srfIsSingular[i] )
    {
      continue;
    }

    sing_count++;
    // j = first face index
    // n = j increment
    // k = max value of j
    // fvi0 = meshface.vi[] index of first corner
    // fvi1 = meshface.vi[] index of second corner
    // vindx = index of first vertex on singular side
    // vdelta = vindx increment
    switch (i)
    {
    case 0: // south
      j = 0;
      n = fcnt_t;
      k = j + fcnt_s*n;
      vindx = 0;
      vdelta =  vcnt_t;
      fvi0 = 2;
      fvi1 = 3;
      break;
    case 1: // east
      j = fcnt_t*(fcnt_s-1);
      n = 1;
      k = j + fcnt_t*n;
      vindx = vcnt_t*(vcnt_s-1);
      vdelta =  1;
      fvi0 = 3;
      fvi1 = 0;
      break;
    case 2: // north
      j = fcnt_t-1;
      n = fcnt_t;
      k = j + fcnt_s*n;
      vindx = vcnt_t-1;
      vdelta =  vcnt_t;
      fvi0 = 0;
      fvi1 = 1;
      break;
    case 3: // west
      j = 0;
      n = 1;
      k = j + fcnt_t*n;
      vindx = 0;
      vdelta =  1;
      fvi0 = 1;
      fvi1 = 2;
      break;
    default:
      j = 0; // keep lint happy - this is never executed
      k = 0;
      n = 0;
      break;
    }

    // 3d location of singular point
    ON_3fPoint p = mesh->m_V[vindx];

    while( j < k)
    {
      mf.vi[0] = mesh->m_F[j].vi[fvi0];
      mf.vi[1] = mesh->m_F[j].vi[fvi1];
      if ( j < k/2 )
        mf.vi[2] = vindx;
      else
        mf.vi[2] = vindx+vdelta;
      mf.vi[3] = mf.vi[2];
      mesh->m_F[j] = mf;
      mesh->m_V[vindx] = p;

      j += n;
      vindx += vdelta;
    }
    mesh->m_V[vindx] = p;
  }

  if ( sing_count > 0 )
  {
    // there will be one unused vertex on each singular side
    mesh->CullUnusedVertices();
  }
  
}

ON_Mesh* ON_MeshSurface( 
            const ON_Surface& surface, 
            int u_count,
            const double* u,
            int v_count,
            const double* v,
            ON_Mesh* mesh
            )
{
  int i, j;

  if (mesh)
  {
    mesh->Destroy();
  }

  // Validate input - doesn't take very long and, if developers
  // actually bother to test, check error messages, and check
  // return codes, will save development time.
  double u0 = ON_UNSET_VALUE;
  double u1 = ON_UNSET_VALUE;
  double v0 = ON_UNSET_VALUE;
  double v1 = ON_UNSET_VALUE;
  {
    if ( u_count < 2 || v_count < 2 || 0 == u || 0 == v )
    {
      ON_ERROR("ON_MeshSurface - illegal input");
      return 0;
    }

    if ( !surface.GetDomain(0,&u0,&u1) || !surface.GetDomain(1,&v0,&v1) )
    {
      ON_ERROR("ON_MeshSurface - input surface is bad");
      return 0;
    }

    if ( u[0] < u0 || u[u_count-1] > u1 )
    {
      ON_ERROR("ON_MeshSurface - u[] parameters not in surface domain");
      return 0;
    }

    if ( v[0] < v0 || v[v_count-1] > v1 )
    {
      ON_ERROR("ON_MeshSurface - v[] parameters not in surface domain");
      return 0;
    }
    
    for (i = 1; i < u_count; i++ )
    {
      if ( u[i-1] >= u[i] )
      {
        ON_ERROR("ON_MeshSurface - illegal u[] array");
        return 0;
      }
    }
    for (j = 1; j < v_count; j++ )
    {
      if ( v[j-1] >= v[j] )
      {
        ON_ERROR("ON_MeshSurface - illegal v[] array");
        return 0;
      }
    }
  }

  int vindex = 0;
  int hint[2] = {0,0};

  ON_3dPoint P;
  ON_3dVector N;
  ON_MeshFace mf;

  if ( 0 == mesh )
  {
    mesh = new ON_Mesh();
  }

  mesh->m_V.Reserve(u_count*v_count);
  mesh->m_N.Reserve(u_count*v_count);
  mesh->m_T.Reserve(u_count*v_count);
  mesh->m_S.Reserve(u_count*v_count);
  mesh->m_F.Reserve((u_count-1)*(v_count-1));
  mesh->m_srf_domain[0].Set(u[0],u[u_count-1]);
  mesh->m_srf_domain[1].Set(v[0],v[v_count-1]);
  mesh->m_packed_tex_domain[0].Set(0.0, 1.0);
  mesh->m_packed_tex_domain[1].Set(0.0, 1.0);
  mesh->m_packed_tex_rotate = false;

  mesh->m_Ttag.SetDefaultSurfaceParameterMappingTag();

  for ( i = 0; i < u_count; i++ )
  {
    for ( j = 0; j < v_count; j++ )
    {
      surface.EvNormal( u[i], v[j], P, N, 0, hint );
      mesh->m_V.Append( ON_3fPoint(&P.x) );
      mesh->m_N.Append( ON_3fVector(&N.x) );
      mesh->m_T.AppendNew().Set( (float)mesh->m_srf_domain[0].NormalizedParameterAt(u[i]),
                                 (float)mesh->m_srf_domain[1].NormalizedParameterAt(v[j])
                               );
      mesh->m_S.AppendNew().Set(u[i],v[j]);
      if ( i && j ) // bug fix here
      {
        vindex = mesh->m_V.Count()-1;
        mf.vi[2] = vindex;         // NE vertex index
        mf.vi[3] = vindex-v_count; // NW vertex index
        mf.vi[1] = mf.vi[2]-1;     // SE vertex index
        mf.vi[0] = mf.vi[3]-1;     // SW vertex index
        mesh->m_F.Append(mf);
      }
    }
  }

  int srfIsClosed[2] = {0,0};
  int srfIsSingular[4] = {0,0,0,0};
  bool bNeedHelper = false;

  if ( u0 == u[0] && u1 == u[u_count-1] )
  {
    if ( surface.IsClosed(0) )
    {
      bNeedHelper = true;
      srfIsClosed[0] = surface.IsPeriodic(0)?2:1;
    }
  }

  if ( v0 == v[0] && v1 == v[v_count-1] )
  {
    if ( surface.IsClosed(1) )
    {
      bNeedHelper = true;
      srfIsClosed[1] = surface.IsPeriodic(1)?2:1;
    }
  }

  if ( v0 == v[0] )
  {
    // south
    srfIsSingular[0] = surface.IsSingular(0);
    if ( srfIsSingular[0] )
      bNeedHelper = true;
  }
  if ( u1 == u[u_count-1] )
  {
    // east
    srfIsSingular[1] = surface.IsSingular(1);
    if ( srfIsSingular[1] )
      bNeedHelper = true;
  }
  if ( v1 == v[v_count-1] )
  {
    // north
    srfIsSingular[2] = surface.IsSingular(2);
    if ( srfIsSingular[2] )
      bNeedHelper = true;
  }
  if ( u0 == u[0] )
  {
    // west
    srfIsSingular[3] = surface.IsSingular(3);
    if ( srfIsSingular[3] )
      bNeedHelper = true;
  }

  if ( bNeedHelper )
  {
    ON_MeshSurfaceHelper(mesh,u_count,v_count,srfIsClosed,srfIsSingular);
  }

  return mesh;
}

ON_Mesh* ON_MeshSurface( const ON_Surface& surface, 
                         int mesh_density,
                         ON_Mesh* mesh )
{
  // 23 December 2003 Dale Lear
  //     Fixed several bugs in this function.
  // quick and dirty mesh
  ON_Workspace ws;
  if (mesh)
    mesh->Destroy();
  int sdegree = surface.Degree(0);
  int tdegree = surface.Degree(1);
  if ( sdegree < 1 || tdegree < 1 )
    return NULL;
  if ( mesh_density < 0 )
  {
    if ( sdegree == 1 && tdegree > 1 )
    {
      tdegree -= mesh_density;
    }
    else if ( tdegree == 1 && sdegree > 1 )
    {
      sdegree -= mesh_density;
    }
    else
    {
      sdegree -= mesh_density;
      tdegree -= mesh_density;
    }
  }
  int scount = surface.SpanCount(0);
  int tcount = surface.SpanCount(1);
  if ( scount < 1 )
    return NULL;
  if ( tcount < 1 )
    return NULL;
  double* s = ws.GetDoubleMemory(scount+1);
  double* t = ws.GetDoubleMemory(tcount+1);
  if ( !surface.GetSpanVector(0,s) )
    return NULL;
  if ( !surface.GetSpanVector(1,t) )
    return NULL;

  if ( !mesh )
    mesh = new ON_Mesh();
  int fcnt = (scount*sdegree)*(tcount*tdegree);
  if ( mesh_density > 0 )
  {
    bool bUpOnce = (fcnt > mesh_density);
    while ( fcnt > mesh_density && (sdegree > 1 || tdegree > 1 ) )
    {
      if ( sdegree > tdegree )
        sdegree--;
      else if ( tdegree > sdegree )
        tdegree--;
      else if ( scount > tcount )
        sdegree--;
      else
        tdegree--;
      fcnt = (scount*sdegree)*(tcount*tdegree);
    }

    while ( fcnt < mesh_density )
    {
      if ( sdegree == 1 && tdegree > 1 )
      {
        tdegree++;
      }
      else if ( tdegree == 1 && sdegree > 1 )
      {
        sdegree++;
      }
      if ( sdegree == 1 && tdegree == 1 )
      {
        sdegree++;
        tdegree++;
      }
      else if ( scount*sdegree < tcount*tdegree )
        sdegree++;
      else
        tdegree++;
      fcnt = (scount*sdegree)*(tcount*tdegree);
      if ( bUpOnce )
        break;
    }
  }
  const int fcnt_s = scount*sdegree;
  const int fcnt_t = tcount*tdegree;
  const int vcnt_s = fcnt_s + 1;
  const int vcnt_t = fcnt_t + 1;
  int vcnt = vcnt_s*vcnt_t;
  mesh->m_V.Reserve(vcnt);
  mesh->m_T.Reserve(vcnt);
  mesh->m_S.Reserve(vcnt);
  mesh->m_N.Reserve(vcnt);
  mesh->m_F.Reserve(fcnt);

  double u, v;
  ON_Interval sdom, tdom;
  int hint[2];
  hint[0] = 0;
  hint[1] = 0;
  int i, j, k, n;

  double* a = ws.GetDoubleMemory(sdegree+1);
  double* b = ws.GetDoubleMemory(tdegree+1);

  double x = 1.0/sdegree;
  for ( i = 0; i < sdegree; i++ )
    a[i] = i*x;
  a[sdegree] = 1.0;
  x = 1.0/tdegree;
  for ( i = 0; i < tdegree; i++ )
    b[i] = i*x;
  b[tdegree] = 1.0;

  int vindex = 0;

  ON_3dPoint P;
  ON_3dVector N;
  ON_MeshFace mf;

  mesh->m_srf_domain[0].Set(s[0],s[scount]);
  mesh->m_srf_domain[1].Set(t[0],t[tcount]);
  mesh->m_packed_tex_domain[0].Set(0.0, 1.0);
  mesh->m_packed_tex_domain[1].Set(0.0, 1.0);
  mesh->m_packed_tex_rotate = false;

  mesh->m_Ttag.SetDefaultSurfaceParameterMappingTag();

  for ( i = 0; i < scount; i++ )
  {
    sdom.Set(s[i],s[i+1]);
    for ( j = i?1:0; j <= sdegree; j++ )
    {
      u = sdom.ParameterAt(a[j]);
      for ( k = 0; k < tcount; k++ )
      {
        tdom.Set(t[k],t[k+1]);
        for ( n = k?1:0; n <= tdegree; n++ )
        {
          v = tdom.ParameterAt(b[n]);
          surface.EvNormal( u, v, P, N, 0, hint );
          mesh->m_V.Append( ON_3fPoint(&P.x) );
          mesh->m_N.Append( ON_3fVector(&N.x) );
          mesh->m_T.AppendNew().Set( 
                    (float)mesh->m_srf_domain[0].NormalizedParameterAt(u),
                    (float)mesh->m_srf_domain[1].NormalizedParameterAt(v)
                    );
          mesh->m_S.AppendNew().Set( u, v );
          if ( j && n )
          {
            vindex = mesh->m_V.Count()-1;
            mf.vi[2] = vindex;        // NE vertex index
            mf.vi[3] = vindex-vcnt_t; // NW vertex index
            mf.vi[1] = mf.vi[2]-1;    // SE vertex index
            mf.vi[0] = mf.vi[3]-1;    // SW vertex index
            mesh->m_F.Append(mf);
          }
        }
      }
    }
  }

  int srfIsClosed[2] = {0,0};
  int srfIsSingular[4] = {0,0,0,0};
  bool bNeedHelper = false;

  for(i = 0; i < 2; i++)
  {
    if ( surface.IsClosed(i) )
    {
      bNeedHelper = true;
      srfIsClosed[i] = surface.IsPeriodic(i)?2:1;
    }
  }

  for ( i = 0; i < 4; i++ )
  {
    srfIsSingular[i] = surface.IsSingular(i);
    if ( srfIsSingular[i] )
      bNeedHelper = true;
  }

  if ( bNeedHelper )
  {
    ON_MeshSurfaceHelper(
              mesh,
              vcnt_s,
              vcnt_t,
              srfIsClosed, // 0 = no, 1 = clsd, 2 = per
              srfIsSingular
              );
  }

  return mesh;
}

ON_Mesh* ON_Surface::CreateMesh( 
           const ON_MeshParameters& mp,
           ON_Mesh* mesh
           ) const
{
  // vitrual ON_Surface function.
  // When openNURBS is used as part of the Rhino SDK, this
  // function works correctly.  The meshing source code is
  // not included with the free openNURBS toolkit.

  // simple and stupid mesh
  return ON_MeshSurface( *this, 0, mesh );
}

ON_Mesh* ON_SurfaceProxy::CreateMesh( 
           const ON_MeshParameters& mp,
           ON_Mesh* mesh
           ) const
{
  if ( m_surface )
  {
    mesh = m_surface->CreateMesh(mp,mesh);
    if ( mesh && m_bTransposed )
    {
      int i, cnt;
      float f;
      double x;
      ON_2fPoint tc;
      ON_2dPoint sp;

      mesh->Flip();

      ON_Interval tmp = mesh->m_packed_tex_domain[0];
      mesh->m_packed_tex_domain[0] = mesh->m_packed_tex_domain[1];
      mesh->m_packed_tex_domain[1] = tmp;
      
      tmp = mesh->m_srf_domain[0];
      mesh->m_srf_domain[0] = mesh->m_srf_domain[1];
      mesh->m_srf_domain[1] = tmp;

      double t = mesh->m_srf_scale[0];
      mesh->m_srf_scale[0] = mesh->m_srf_scale[1];
      mesh->m_srf_scale[1] = t;

      if ( mesh->HasTextureCoordinates() )
      {
        cnt = mesh->m_T.Count();;
        for ( i = 0; i < cnt; i++ )
        {
          tc = mesh->m_T[i];
          f = tc.x; tc.x = tc.y; tc.y = f;
          mesh->m_T[i] = tc;
        }
      }

      if ( mesh->HasSurfaceParameters() )
      {
        cnt = mesh->m_S.Count();;
        for ( i = 0; i < cnt; i++ )
        {
          sp = mesh->m_S[i];
          x = sp.x; sp.x = sp.y; sp.y = x;
          mesh->m_S[i] = sp;
        }
      }
    }
  } 
  else if ( mesh )
  {
    // no "real" surface to mesh
    mesh->Destroy();
    mesh = 0;
  }
  return mesh;
}

ON_Mesh* ON_NurbsSurface::CreateMesh( 
           const ON_MeshParameters& mp,
           ON_Mesh* mesh
           ) const
{
  if ( _MeshNurbsSurface ) // provided by Rhino SDK
    return _MeshNurbsSurface(*this,mp,mesh);
  return ON_Surface::CreateMesh(mp,mesh);
}

ON_Mesh* ON_PlaneSurface::CreateMesh( 
           const ON_MeshParameters& mp,
           ON_Mesh* mesh
           ) const
{
  if ( _MeshPlaneSurface ) // provided by Rhino SDK
    return _MeshPlaneSurface(*this,mp,mesh);
  return ON_Surface::CreateMesh(mp,mesh);

}

ON_Mesh* ON_RevSurface::CreateMesh( 
           const ON_MeshParameters& mp,
           ON_Mesh* mesh
           ) const
{
  if ( _MeshRevSurface ) // provided by Rhino SDK
    return _MeshRevSurface(*this,mp,mesh);
  return ON_Surface::CreateMesh(mp,mesh);
}

ON_Mesh* ON_SumSurface::CreateMesh( 
           const ON_MeshParameters& mp,
           ON_Mesh* mesh
           ) const
{
  if ( _MeshSumSurface ) // provided by Rhino SDK
    return _MeshSumSurface(*this,mp,mesh);
  return ON_Surface::CreateMesh(mp,mesh);
}

ON_Mesh* ON_BrepFace::CreateMesh( 
           const ON_MeshParameters& mp,
           ON_Mesh* mesh
           ) const
{
  if ( _MeshBrepFace ) // provided by Rhino SDK
    return _MeshBrepFace(*this,mp,mesh);
  if ( Brep() && Brep()->FaceIsSurface(m_face_index) )
    return ON_Surface::CreateMesh(mp,mesh);
  if ( mesh )
    mesh->Destroy();
  return 0;
}

int ON_Brep::CreateMesh( 
  const ON_MeshParameters& mp,
  ON_SimpleArray<ON_Mesh*>& mesh_list
  ) const
{
  if ( ON_Brep::m__Mesh ) 
  {
    // provided by Rhino SDK
    return ON_Brep::m__Mesh(*this,mp,mesh_list);
  }

  int i, cnt=0, mesh_list_count0 = mesh_list.Count();
  mesh_list.Reserve(mesh_list_count0 + m_F.Count() );
  for ( i = 0; i < m_F.Count(); i++ )
  {
    ON_Mesh* mesh = m_F[i].CreateMesh( mp );
    if ( mesh )
      cnt++;
    mesh_list.Append(mesh);
  }
  if ( cnt < 1 )
    mesh_list.SetCount(mesh_list_count0);
  return mesh_list.Count() - mesh_list_count0;
}

ON_MeshCurveParameters::ON_MeshCurveParameters()
{
  memset(this,0,sizeof(*this));
}


ON_PolylineCurve* ON_Curve::MeshCurve(
    ON_MeshCurveParameters& mp,
    ON_PolylineCurve* polyline,
    bool bSkipFirstPoint,
    const ON_Interval* domain
    )
{
  return ( 0 != ON_Curve::_MeshCurve ) 
         ? ON_Curve::_MeshCurve(this,&mp,polyline,bSkipFirstPoint,domain) 
         : 0;
}

bool ON_Mesh2dPolygon( 
      int point_count, int point_stride, const double* P,
      int tri_stride, int* triangle 
      )
{
  return (0 != ON_Mesh::_Mesh2dPolygon )
    ? (ON_Mesh::_Mesh2dPolygon(point_count,point_stride,P,tri_stride,triangle) >= 0)
    : false;
}

ON_OBJECT_IMPLEMENT(ON_Mesh,ON_Geometry,"4ED7D4E4-E947-11d3-BFE5-0010830122F0");

/*
ON_MeshEdge& ON_MeshEdge::operator=( const ON_MeshEdge& src )
{
  int fc = src.m_fcount+(src.m_fcount%1);
  if ( fc <= 2 ) {
    if ( m_fi && m_fi != m_private_fi ) onfree((void*)m_fi);
    m_fi = m_private_fi;
    fc = 2;
  }
  else {
    if ( (m_fcount+(m_fcount%1)) != fc ) {
      if ( m_fi == m_private_fi )
        m_fi = 0;
      m_fi = (int*)onrealloc(m_fi,fc*sizeof(*m_fi));
    }
  }
  memcpy(m_fi,src.m_fi,fc*sizeof(*m_fi));
  m_fcount=src.m_fcount;
  return *this;
}

void ON_MeshEdge::AppendFaceIndex(int face_index)
{
  if ( m_fcount>0 && !(m_fcount%1) ) {
    if ( m_fi == m_private_fi ) {
      m_fi = (int*)onmalloc((m_fcount+2)*sizeof(*m_fi));
      m_fi[0] = m_private_fi[0];
      m_fi[1] = m_private_fi[1];
    }
    else {
      m_fi = (int*)onrealloc(m_fi,(m_fcount+2)*sizeof(*m_fi));
    }
  }
  m_fi[m_fcount++] = face_index;
}
*/

bool
ON_MeshFace::IsValid(int vertex_count) const
{
  return (    vi[0] >= 0 && vi[0] < vertex_count
           && vi[1] >= 0 && vi[1] < vertex_count
           && vi[2] >= 0 && vi[2] < vertex_count
           && vi[3] >= 0 && vi[3] < vertex_count
           && vi[0] != vi[1] && vi[1] != vi[2] && vi[2] != vi[0] 
           && (vi[2]==vi[3]||(vi[0] != vi[3] && vi[1] != vi[3])) );
}


ON_Mesh::ON_Mesh() : m_packed_tex_rotate(0), 
                     m_parent(0),
                     m_mesh_parameters(0), 
                     m_invalid_count(0), 
                     m_quad_count(0), 
                     m_triangle_count(0),
                     m_closed(-1),
                     m_mtree(0)

{
  m_top.m_mesh = this;
  m_srf_scale[0] = 0.0;
  m_srf_scale[1] = 0.0;
  m_kstat[0] = 0;
  m_kstat[1] = 0;
  m_kstat[2] = 0;
  m_kstat[3] = 0;
  InvalidateBoundingBoxes();
  m_partition = 0;
  m_hidden_count = 0;
}


ON_Mesh::ON_Mesh(
    int initial_facet_capacity,  // initial facet array capacity
    int initial_vertex_capacity,  // initial vertex array capacity
    bool bHasVertexNormals, // true if mesh has unit vertex normals
    bool bHasTextureCoordinates // true if mesh has texture coordinates
    )
    : m_V(initial_vertex_capacity),
      m_F(initial_facet_capacity),
      m_N(bHasVertexNormals?initial_vertex_capacity:0),
      m_T(bHasTextureCoordinates?initial_vertex_capacity:0),
      m_packed_tex_rotate(0),
      m_parent(0), 
      m_mesh_parameters(0),
      m_invalid_count(0), 
      m_quad_count(0), 
      m_triangle_count(0),
      m_closed(-1),
      m_mtree(0)
{
  m_top.m_mesh = this;
  m_srf_scale[0] = 0.0;
  m_srf_scale[1] = 0.0;
  m_kstat[0] = 0;
  m_kstat[1] = 0;
  m_kstat[2] = 0;
  m_kstat[3] = 0;
  InvalidateBoundingBoxes();
  m_partition = 0;
  m_hidden_count = 0;
}

ON_Mesh::ON_Mesh( const ON_Mesh& src ) 
        : m_packed_tex_rotate(0),
          m_parent(0), 
          m_mesh_parameters(0), 
          m_invalid_count(0), 
          m_quad_count(0), 
          m_triangle_count(0),
          m_closed(-1),
          m_mtree(0)
{
  m_top.m_mesh = this;
  m_srf_scale[0] = 0.0;
  m_srf_scale[1] = 0.0;

  m_kstat[0] = 0;
  m_kstat[1] = 0;
  m_kstat[2] = 0;
  m_kstat[3] = 0;
  InvalidateBoundingBoxes();
  m_partition = 0;
  m_hidden_count = 0;
  ON_Mesh::operator=(src);
}


unsigned int ON_Mesh::SizeOf() const
{
  unsigned int sz = ON_Geometry::SizeOf();
  sz += m_V.SizeOfArray();
  sz += m_F.SizeOfArray();
  sz += m_N.SizeOfArray();
  sz += m_FN.SizeOfArray();
  sz += m_T.SizeOfArray();
  sz += m_S.SizeOfArray();
  sz += m_K.SizeOfArray();
  sz += m_C.SizeOfArray();  
  sz += m_top.m_topv_map.SizeOfArray();
  sz += m_top.m_topv.SizeOfArray();
  sz += m_top.m_tope.SizeOfArray();
  sz += m_top.m_topf.SizeOfArray();
  return sz;
}

ON__UINT32 ON_Mesh::DataCRC(ON__UINT32 current_remainder) const
{
  const ON_3fPoint* p = m_V.Array();
  current_remainder = ON_CRC32(current_remainder,m_V.Count()*sizeof(p[0]),p);
  current_remainder = ON_CRC32(current_remainder,m_F.Count()*sizeof(ON_MeshFace),m_F.Array());
  const ON_3fVector* v = m_N.Array();
  current_remainder = ON_CRC32(current_remainder,m_N.Count()*sizeof(v[0]),v);
  return current_remainder;
}

ON_Mesh& ON_Mesh::operator=( const ON_Mesh& src )
{
  if ( this != &src ) 
  {
    Destroy();
    ON_Geometry::operator=(src);

    m_V  = src.m_V;
    m_F  = src.m_F;
    m_N  = src.m_N;
    m_FN = src.m_FN;
    m_T  = src.m_T;
    m_TC = src.m_TC;
    m_S  = src.m_S;
    m_H  = src.m_H;

    m_Ctag = src.m_Ctag;
    m_Ttag = src.m_Ttag;
    m_packed_tex_domain[0] = src.m_packed_tex_domain[0];
    m_packed_tex_domain[1] = src.m_packed_tex_domain[1];
    m_srf_domain[0] = src.m_srf_domain[0];
    m_srf_domain[1] = src.m_srf_domain[1];
    m_srf_scale[0] = src.m_srf_scale[0];
    m_srf_scale[1] = src.m_srf_scale[1];
    m_packed_tex_rotate = src.m_packed_tex_rotate;

    m_K = src.m_K;
    m_C = src.m_C;

    m_parent         = src.m_parent;
    //m_material_index = src.m_material_index;

    if ( m_mesh_parameters ) {
      delete m_mesh_parameters;
      m_mesh_parameters = 0;
    }
    if ( src.m_mesh_parameters )
      m_mesh_parameters = new ON_MeshParameters(*src.m_mesh_parameters);

    m_invalid_count = src.m_invalid_count;
    m_quad_count = src.m_quad_count;
    m_triangle_count = src.m_triangle_count;

    m_closed = src.m_closed;

    memcpy(m_vbox,src.m_vbox,sizeof(m_vbox));
    memcpy(m_nbox,src.m_nbox,sizeof(m_nbox));
    memcpy(m_tbox,src.m_tbox,sizeof(m_tbox));

    int i;
    for ( i = 0; i < 4; i++ ) {
      if ( m_kstat[i] ) 
      {
        delete m_kstat[i];
        m_kstat[i] = 0;
      }
      if ( src.m_kstat[i] )
      {
        m_kstat[i] = new ON_MeshCurvatureStats(*src.m_kstat[i]);
      }
    }

    // do not copy m_mtree
    // do not copy m_top
  }
  return *this;
}

ON_Mesh::~ON_Mesh()
{
  Destroy();
  m_top.m_mesh = 0;
}

void ON_Mesh::MemoryRelocate()
{
  // the back pointer on m_top needs to be updated.
  m_top.m_mesh = this;
}

void ON_Mesh::Destroy()
{
  PurgeUserData();
  DestroyRuntimeCache( true );
  m_Ttag.Default();
  m_Ctag.Default();
  m_V.Destroy();
  m_F.Destroy();
  m_N.Destroy();
  m_FN.Destroy();
  m_T.Destroy();
  m_TC.Destroy();
  m_S.Destroy();
  m_K.Destroy();
  m_C.Destroy();
}

void ON_Mesh::EmergencyDestroy()
{
  DestroyRuntimeCache( false );
  m_V.EmergencyDestroy();
  m_F.EmergencyDestroy();
  m_N.EmergencyDestroy();
  m_FN.EmergencyDestroy();
  m_T.EmergencyDestroy();
  m_TC.EmergencyDestroy();
  m_S.EmergencyDestroy();
  m_K.EmergencyDestroy();
  m_C.EmergencyDestroy();
}

ON_BOOL32 ON_Mesh::IsValid( ON_TextLog* text_log ) const
{
  const int facet_count = FaceCount();
  const int vertex_count = VertexCount();
  int fi, vi;

  if (facet_count < 1) 
  {
    if ( text_log )
    {
      text_log->Print("ON_Mesh.m_F.Count() < 1 (should be at least 1).\n");
    }
    return false;
  }

  if ( vertex_count < 3 )
  {
    if ( text_log )
    {
      text_log->Print("ON_Mesh.m_V.Count() < 3 (should be at least 3).\n");
    }
    return false;
  }

  if ( m_N.Count() > 0 && m_N.Count() != vertex_count )
  {
    if ( text_log )
    {
      text_log->Print("ON_Mesh.m_N.Count() = %d (should be 0 or %d=vertex_count).\n",
                      m_N.Count(),vertex_count);
    }
    return false;
  }

  if ( m_T.Count() > 0 && m_T.Count() != vertex_count )
  {
    if ( text_log )
    {
      text_log->Print("ON_Mesh.m_T.Count() = %d (should be 0 or %d=vertex_count).\n",
                      m_T.Count(),vertex_count);
    }
    return false;
  }

  if ( m_S.Count() > 0 && m_S.Count() != vertex_count )
  {
    if ( text_log )
    {
      text_log->Print("ON_Mesh.m_S.Count() = %d (should be 0 or %d=vertex_count).\n",
                      m_S.Count(),vertex_count);
    }
    return false;
  }

  for ( fi = 0; fi < facet_count; fi++ ) 
  {
    if ( !m_F[fi].IsValid( vertex_count ) ) 
    {
      if ( text_log )
      {
        text_log->Print("ON_Mesh.m_F[%d] is not a valid mesh face.\n",fi);
      }
      return false;
    }
 }

  if ( HasVertexNormals() ) 
  {
    float x;
    for ( vi = 0; vi < vertex_count; vi++ ) {
      x = m_N[vi][0]*m_N[vi][0] + m_N[vi][1]*m_N[vi][1] + m_N[vi][2]*m_N[vi][2];
      if ( x < 0.985 || x > 1.015 )
      {
        if ( text_log )
        {
          text_log->Print("ON_Mesh.m_N[%d] is not a unit vector (length = %g).\n",vi,sqrt(x));
        }
        return false;
      }
    }
  }

	// Greg Arden 9 May 2003. Fixes TRR#10604.  Attempt to detect meshes with non-finite vertices
	// by testing the bounding box.
  int i;
  for ( i = 0; i < 3; i++ )
  {
    if ( !ON_IsValid( m_vbox[0][i] ) || !ON_IsValid( m_vbox[1][i] ) )
    {
      if ( text_log )
      {
        text_log->Print("ON_Mesh.m_vbox is not finite.  Check for invalid vertices\n");
      }		
	    return false;
    }	
  }

  return true;
}

void ON_Mesh::Dump( ON_TextLog& dump ) const
{
  const int half_max = 8;

  const int fcount = m_F.Count();
  int i;
  const int vcount = m_V.Count();
  ON_3dPoint p;

  dump.Print("ON_Mesh: vertex count = %d  facet count = %d\n", m_V.Count(), m_F.Count() );
  dump.Print("vertex normals: %s\n",HasVertexNormals()?"true":"false");
  dump.Print("face normals:   %s\n",HasFaceNormals()?"true":"false");
  dump.Print("tex coords:     %s\n",HasTextureCoordinates()?"true":"false");
  dump.Print("vertex kappa:   %s\n",HasPrincipalCurvatures()?"true":"false");
  dump.Print("vertex colors:  %s\n",HasVertexColors()?"true":"false");
  dump.Print("m_Ctag:\n"); dump.PushIndent(); m_Ctag.Dump(dump); dump.PopIndent();
  dump.Print("m_packed_tex_rotate: %s\n",m_packed_tex_rotate?"true":"false");
  dump.Print("m_packed_tex_domain: (%g,%g)x(%g,%g)\n",
             m_packed_tex_domain[0][0],m_packed_tex_domain[0][1],
             m_packed_tex_domain[1][0],m_packed_tex_domain[1][1]);
  dump.Print("m_srf_domain: (%g,%g)x(%g,%g)\n",m_srf_domain[0][0],m_srf_domain[0][1],m_srf_domain[1][0],m_srf_domain[1][1]);
  dump.Print("m_srf_scale: %g,%g\n",m_srf_scale[0],m_srf_scale[0]);
  dump.Print("m_Ttag:\n"); dump.PushIndent(); m_Ttag.Dump(dump); dump.PopIndent();

  dump.PushIndent();

  dump.Print("%d mesh vertices:\n",m_V.Count());
  {
    dump.PushIndent();
    for (i = 0; i < vcount; i++)
    {
      if ( i == half_max && 2*half_max < vcount )
      {
        dump.Print("...\n");
        i = vcount-half_max;
      }
      else
      {
        p = m_V[i];
        dump.Print("m_V[%d] = (%g,%g,%g)\n",i,p.x,p.y,p.z);
      }
    }
    dump.PopIndent();
  }

  if ( HasVertexNormals() )
  {
    dump.Print("%d mesh vertex normals:\n",m_N.Count());
    {
      dump.PushIndent();
      for (i = 0; i < vcount; i++)
      {
        if ( i == half_max && 2*half_max < vcount )
        {
          dump.Print("...\n");
          i = vcount-half_max;
        }
        else
        {
          p = m_N[i];
          dump.Print("m_N[%d] = (%g,%g,%g)\n",i,p.x,p.y,p.z);
        }
      }
      dump.PopIndent();
    }
  }

  if ( HasTextureCoordinates() )
  {
    dump.Print("%d mesh vertex texture coordinates:\n",m_T.Count());
    {
      dump.PushIndent();
      for (i = 0; i < vcount; i++)
      {
        if ( i == half_max && 2*half_max < vcount )
        {
          dump.Print("...\n");
          i = vcount-half_max;
        }
        else
        {
          ON_2fPoint tp = m_T[i];
          p.x = tp.x;
          p.y = tp.y;
          dump.Print("m_T[%d] = (%g,%g)\n",i,p.x,p.y);
        }
      }
      dump.PopIndent();
    }
  }


  if ( HasSurfaceParameters() )
  {
    dump.Print("%d mesh vertex surface parameters:\n",m_S.Count());
    {
      dump.PushIndent();
      for (i = 0; i < vcount; i++)
      {
        if ( i == half_max && 2*half_max < vcount )
        {
          dump.Print("...\n");
          i = vcount-half_max;
        }
        else
        {
          ON_2dPoint srfuv = m_S[i];
          dump.Print("m_S[%d] = (%g,%g)\n",i,srfuv.x,srfuv.y);
        }
      }
      dump.PopIndent();
    }
  }

  dump.Print("%d mesh faces:\n",m_F.Count());
  {
    dump.PushIndent();
    for (i = 0; i < fcount; i++)
    {
      if ( i == half_max && 2*half_max < fcount )
      {
        dump.Print("...\n");
        i = fcount-half_max;
      }
      else if ( m_F[i].vi[2] == m_F[i].vi[3] )
        dump.Print("m_F[%d].vi = (%d,%d,%d)\n",i,m_F[i].vi[0],m_F[i].vi[1],m_F[i].vi[2]);
      else
        dump.Print("m_F[%d].vi = (%d,%d,%d,%d)\n",i,m_F[i].vi[0],m_F[i].vi[1],m_F[i].vi[2],m_F[i].vi[3]);
    }
    dump.PopIndent();
  }

  if ( HasFaceNormals() )
  {
    dump.Print("%d mesh face normals:\n",m_FN.Count());
    {
      dump.PushIndent();
      for (i = 0; i < fcount; i++)
      {
        if ( i == half_max && 2*half_max < fcount )
        {
          dump.Print("...\n");
          i = fcount-half_max;
        }
        else
        {
          p = m_FN[i];
          dump.Print("m_FN[%d] = (%g,%g,%g)\n",i,p.x,p.y,p.z);
        }
      }
      dump.PopIndent();
    }
  }


  dump.PopIndent();
}


bool ON_Mesh::WriteFaceArray( int vcount, int fcount, ON_BinaryArchive& file ) const
{
  unsigned char  cvi[4];
  unsigned short svi[4];
  const int* vi;
  int i_size = 0;
  if ( vcount < 256 ) {
    i_size = 1; // unsigned chars
  }
  else if (vcount < 65536 ) {
    i_size = 2; // unsigned shorts
  }
  else {
    i_size = 4; // 4 byte ints
  }

  bool rc = file.WriteInt( i_size );
  int i;
  switch(i_size) {
  case 1:
    for ( i = 0; i < fcount && rc ; i++ ) {
      vi = m_F[i].vi;
      cvi[0] = (unsigned char)vi[0];
      cvi[1] = (unsigned char)vi[1];
      cvi[2] = (unsigned char)vi[2];
      cvi[3] = (unsigned char)vi[3];
      rc = file.WriteChar( 4, cvi );
    }
    break;
  case 2:
    for ( i = 0; i < fcount && rc ; i++ ) {
      vi = m_F[i].vi;
      svi[0] = (unsigned short)vi[0];
      svi[1] = (unsigned short)vi[1];
      svi[2] = (unsigned short)vi[2];
      svi[3] = (unsigned short)vi[3];
      rc = file.WriteShort( 4, svi );
    }
    break;
  case 4:
    for ( i = 0; i < fcount && rc ; i++ ) {
      rc = file.WriteInt( 4, m_F[i].vi );
    }
    break;
  }

  return rc;
}

bool ON_Mesh::ReadFaceArray( int vcount, int fcount, ON_BinaryArchive& file )
{
  unsigned char  cvi[4];
  unsigned short svi[4];
  unsigned int* vi;
  int i_size = 0;

  if ( m_F.Capacity() < fcount )
    m_F.SetCapacity(fcount);
  bool rc = file.ReadInt( &i_size );
  int i = 0;
  switch(i_size) {
  case 1:
    for ( i = 0; i < fcount && rc ; i++ ) {
      rc = file.ReadChar( 4, cvi );
      vi = (unsigned int*)m_F[i].vi;
      vi[0] = cvi[0];
      vi[1] = cvi[1];
      vi[2] = cvi[2];
      vi[3] = cvi[3];
    }
    break;
  case 2:
    for ( i = 0; i < fcount && rc ; i++ ) {
      rc = file.ReadShort( 4, svi );
      vi = (unsigned int*)m_F[i].vi;
      vi[0] = svi[0];
      vi[1] = svi[1];
      vi[2] = svi[2];
      vi[3] = svi[3];
    }
    break;
  case 4:
    for ( i = 0; i < fcount && rc ; i++ ) {
      rc = file.ReadInt( 4, m_F[i].vi );
    }
    break;
  }
  m_F.SetCount(i);

  return rc;
}


bool ON_Mesh::Write_1( ON_BinaryArchive& file ) const
{
  // ver 1.0 uncompressed format

  bool rc = file.WriteArray( m_V );
  if (rc) rc = file.WriteArray( m_N );
  if (rc) rc = file.WriteArray( m_T );
  if (rc) rc = file.WriteArray( m_K );
  if (rc) rc = file.WriteArray( m_C );

  return rc;
}

bool ON_Mesh::Read_1( ON_BinaryArchive& file )
{
  // common to all 1.x formats (uncompressed)

  bool rc = file.ReadArray( m_V );
  if (rc) rc = file.ReadArray( m_N );
  if (rc) rc = file.ReadArray( m_T );
  if (rc) rc = file.ReadArray( m_K );
  if (rc) rc = file.ReadArray( m_C );

  return rc;
}

bool ON_Mesh::Write_2( int Vcount, ON_BinaryArchive& file ) const
{
  // ver 2.0 compressed format
  const ON::endian e = file.Endian();

  bool rc = true;

  if ( Vcount > m_V.Count() )
    return false;

  if ( Vcount > 0 ) 
  {
    const int Ncount = (m_V.Count() == m_N.Count()) ? Vcount : 0;
    const int Tcount = (m_V.Count() == m_T.Count()) ? Vcount : 0;
    const int Kcount = (m_V.Count() == m_K.Count()) ? Vcount : 0;
    const int Ccount = (m_V.Count() == m_C.Count()) ? Vcount : 0;

    if ( e == ON::big_endian ) 
    {
      // These calls temporarily put the m_V[], m_N[], m_T[], m_K[]
      // and m_C[] arrays in little endian byte order because 3dm archives
      // are always in little endian byte order.
      //
      // This code assumes sizeof(ON_Color)=4, sizeof(float)=4 
      // and sizeof(double)=8.
      // If this is not the case, then changing the 4's and 8's below
      // will not work.  You will have to copy the mesh definition
      // into temporary arrays of 4 byte floats/8 byte doubles
      // and them compress the temporary arrays.  If you do this,
      // then remove the "restore" byte order calls below.
      file.ToggleByteOrder( Vcount*3, 4, m_V.Array(), (void*)m_V.Array() );
      file.ToggleByteOrder( Ncount*3, 4, m_N.Array(), (void*)m_N.Array() );
      file.ToggleByteOrder( Tcount*2, 4, m_T.Array(), (void*)m_T.Array() );
      file.ToggleByteOrder( Kcount*2, 8, m_K.Array(), (void*)m_K.Array() );
      file.ToggleByteOrder( Ccount,   4, m_C.Array(), (void*)m_C.Array() );
    }
    if (rc) rc = file.WriteCompressedBuffer( Vcount*sizeof(ON_3fPoint),         m_V.Array() );
    if (rc) rc = file.WriteCompressedBuffer( Ncount*sizeof(ON_3fVector),        m_N.Array() );
    if (rc) rc = file.WriteCompressedBuffer( Tcount*sizeof(ON_2fPoint),         m_T.Array() );
    if (rc) rc = file.WriteCompressedBuffer( Kcount*sizeof(ON_SurfaceCurvature),m_K.Array() );
    if (rc) rc = file.WriteCompressedBuffer( Ccount*sizeof(ON_Color),           m_C.Array() );
    if ( e == ON::big_endian ) 
    {
      // These calls restore the m_V[], m_N[], m_T[], m_K[] and m_C[] arrays
      // to the correct big endian runtime byte order.  This must be done even
      // if rc is false.
      file.ToggleByteOrder( Vcount*3, 4, m_V.Array(), (void*)m_V.Array() );
      file.ToggleByteOrder( Ncount*3, 4, m_N.Array(), (void*)m_N.Array() );
      file.ToggleByteOrder( Tcount*2, 4, m_T.Array(), (void*)m_T.Array() );
      file.ToggleByteOrder( Kcount*2, 8, m_K.Array(), (void*)m_K.Array() );
      file.ToggleByteOrder( Ccount,   4, m_C.Array(), (void*)m_C.Array() );
    }
  }

  return rc;
}

bool ON_Mesh::Read_2( int vcount, ON_BinaryArchive& file )
{
  // common to all 2.x formats (compressed)
  const ON::endian e = file.Endian();

  bool rc = true;


  if ( vcount > 0 ) 
  {
    size_t sz = 0;
    int bFailedCRC;

    sz = 0;
    if (rc) rc = file.ReadCompressedBufferSize( &sz );
    if (rc && sz) 
    {
      if ( sz == vcount*sizeof(m_V[0]) )
      {
        m_V.SetCapacity(vcount);
        if (rc) rc = file.ReadCompressedBuffer( sz,m_V.Array(),&bFailedCRC);
        if (rc) m_V.SetCount(vcount);
      }
      else
      {
        rc = false; // buffer is wrong size
      }
    }

    sz = 0;
    if (rc) rc = file.ReadCompressedBufferSize( &sz );
    if (rc && sz) 
    {
      if ( sz == vcount*sizeof(m_N[0]) )
      {
        m_N.SetCapacity(vcount);
        if (rc) rc = file.ReadCompressedBuffer( sz,m_N.Array(),&bFailedCRC );
        if (rc) m_N.SetCount(vcount);
      }
      else
      {
        rc = false; // buffer is wrong size
      }
    }
    
    sz = 0;
    if (rc) rc = file.ReadCompressedBufferSize( &sz );
    if (rc && sz) 
    {
      if ( sz == vcount*sizeof(m_T[0]) )
      {
        m_T.SetCapacity(vcount);
        if (rc) rc = file.ReadCompressedBuffer( sz,m_T.Array(),&bFailedCRC );
        if (rc) m_T.SetCount(vcount);
      }
      else
      {
        rc = false; // buffer is wrong size
      }
    }
    
    sz = 0;
    if (rc) rc = file.ReadCompressedBufferSize( &sz );
    if (rc && sz) 
    {
      if ( sz == vcount*sizeof(m_K[0]) )
      {
        m_K.SetCapacity(vcount);
        if (rc) rc = file.ReadCompressedBuffer( sz,m_K.Array(),&bFailedCRC );
        if (rc) m_K.SetCount(vcount);
      }
      else
      {
        rc = false; // buffer is wrong size
      }
    }
    
    sz = 0;
    if (rc) rc = file.ReadCompressedBufferSize( &sz );
    if (rc && sz) 
    {
      if ( sz == vcount*sizeof(m_C[0]) )
      {
        m_C.SetCapacity(vcount);
        if (rc) rc = file.ReadCompressedBuffer( sz,m_C.Array(),&bFailedCRC );
        if (rc) m_C.SetCount(vcount);
      }
      else
      {
        rc = false; // buffer is wrong size
      }
    }
    
    if ( e == ON::big_endian ) 
    {
      // This code assumes sizeof(ON_Color)=4, sizeof(float)=4 
      // and sizeof(double)=8.
      // If this is not the case, then changing the 4's and 8's below
      // will not work.  You will have to read the compressed
      // information into temporary arrays of 4 byte floats/8 byte doubles
      // and then convert those numbers to whatever is stored in the
      // m_V[], m_N[], m_T[], m_K[] and m_C[] arrays/
      file.ToggleByteOrder( m_V.Count()*3, 4, m_V.Array(), (void*)m_V.Array() );
      file.ToggleByteOrder( m_N.Count()*3, 4, m_N.Array(), (void*)m_N.Array() );
      file.ToggleByteOrder( m_T.Count()*2, 4, m_T.Array(), (void*)m_T.Array() );
      file.ToggleByteOrder( m_K.Count()*2, 8, m_K.Array(), (void*)m_K.Array() );
      file.ToggleByteOrder( m_C.Count()*3, 4, m_C.Array(), (void*)m_C.Array() );
    }
  }


  return rc;
}


ON_BOOL32 ON_Mesh::Write( ON_BinaryArchive& file ) const
{
  int i;
  //const int major_version = 1; // uncompressed
  //const int major_version = 2; // beta format (never used)
  const int major_version = 3; // compressed
  bool rc = file.Write3dmChunkVersion(major_version,4);

  const int vcount = VertexCount();
  const int fcount = FaceCount();

  if (rc) rc = file.WriteInt( vcount );
  if (rc) rc = file.WriteInt( fcount );
  if (rc) rc = file.WriteInterval( m_packed_tex_domain[0] );
  if (rc) rc = file.WriteInterval( m_packed_tex_domain[1] );
  if (rc) rc = file.WriteInterval( m_srf_domain[0] );
  if (rc) rc = file.WriteInterval( m_srf_domain[1] );
  if (rc) rc = file.WriteDouble( 2, m_srf_scale );
  if (rc) rc = file.WriteFloat( 6, &m_vbox[0][0] );
  if (rc) rc = file.WriteFloat( 6, &m_nbox[0][0] );
  if (rc) rc = file.WriteFloat( 4, &m_tbox[0][0] );

  if (rc) rc = file.WriteInt( m_closed );

  unsigned char b = m_mesh_parameters ? 1 : 0;
  if (rc) rc = file.WriteChar(b);
  if (rc && b) {
    if (rc) rc = file.BeginWrite3dmChunk( TCODE_ANONYMOUS_CHUNK, 0 );
    if (rc) {
      rc = m_mesh_parameters->Write(file);
      if ( !file.EndWrite3dmChunk() )
        rc = false;
    }
  }

  for ( i = 0; rc && i < 4; i++ ) {
    b = m_kstat[i] ? 1 : 0;
    rc = file.WriteChar(b);
    if (b) {
      rc = file.BeginWrite3dmChunk( TCODE_ANONYMOUS_CHUNK, 0 );
      if (rc) {
        rc = m_kstat[i]->Write(file);
        if ( !file.EndWrite3dmChunk() )
          rc = false;
      }
    }
  }

  if (rc) rc = WriteFaceArray( vcount, fcount, file );

  if (rc) {
    // major version is a hard coded 3

    //if ( major_version == 1 )
    //  rc = Write_1(file);
    //else if ( major_version == 3 )
      rc = Write_2(vcount,file);
    //else
    //  rc = false;
  }

  // added for minor version 1.2 and 3.2
  i = m_packed_tex_rotate ? 1 : 0;
  if (rc) rc = file.WriteInt( i );

  // added for minor version 3.3
  if (rc) rc = file.WriteUuid( m_Ttag.m_mapping_id );
 
  // compressed m_S[]
  if ( rc && vcount > 0 ) 
  {
    const ON::endian e = file.Endian();
    if ( e == ON::big_endian ) 
    {
      file.ToggleByteOrder( m_S.Count()*2, 8, m_S.Array(), (void*)m_S.Array() );
    }
    if (rc) rc = file.WriteCompressedBuffer( m_S.Count()*sizeof(ON_2dPoint),m_S.Array() );
    if ( e == ON::big_endian ) 
    {
      file.ToggleByteOrder( m_S.Count()*2, 8, m_S.Array(), (void*)m_S.Array() );
    }
  }

  // added for minor version 3.4
  if (rc) rc = m_Ttag.Write(file);

  return rc;
}

//// This id was used in the ON_Mesh::m_mapping_id
//// field to indicate the texture coordinates are the
//// canonical ON_Mesh uv texture coordinates by the 
//// OpenNURBS parameteric surface meshers like
//// ON_Surface::CreateMesh() and ON_Brep::CreateMesh().
//
//// {B988A6C2-61A6-45a7-AAEE-9AED7EF4E316}
static const ON_UUID obsolete_default_srfp_mapping_id = { 0xb988a6c2, 0x61a6, 0x45a7, { 0xaa, 0xee, 0x9a, 0xed, 0x7e, 0xf4, 0xe3, 0x16 } };

// overrides virtual ON_Object::Write
ON_BOOL32 ON_TextureMapping::Write(
        ON_BinaryArchive& file
      ) const
{
  bool rc = file.BeginWrite3dmChunk(TCODE_ANONYMOUS_CHUNK,1,1);
  if (rc)
  {

    for(;;)
    {
      rc = file.WriteUuid( m_mapping_id);
      if (!rc) break;

      rc = file.WriteInt( m_type );
      if (!rc) break;

      rc = file.WriteInt( m_projection );
      if (!rc) break;

      rc = file.WriteXform( m_Pxyz );
      if (!rc) break;

      // Do not write m_Nxyz - it is calculated from m_Pxyz.
      rc = file.WriteXform( m_uvw );
      if (!rc) break;

      rc = file.WriteString(m_mapping_name);
      if (!rc) break;

      rc = file.WriteObject(m_mapping_primitive);
      if (!rc) break;

      // 13 October 2006 ver 1.1 fields
      rc = file.WriteInt(m_texture_space);
      if (!rc) break;

      rc = file.WriteBool(m_bCapped);
      if (!rc) break;

      break;
    }

    if ( !file.EndWrite3dmChunk() )
      rc = false;
  }

  return rc;
}

// overrides virtual ON_Object::Read
ON_BOOL32 ON_TextureMapping::Read(
        ON_BinaryArchive& file
      )
{
  Default();

  int major_version = 0;
  int minor_version = 0;
  int i;

  bool rc = file.BeginRead3dmChunk(TCODE_ANONYMOUS_CHUNK,&major_version,&minor_version);
  if (rc)
  {
    if ( 1 == major_version )
    {
      // DO NOT SAVE m_mapping_index in archive.
      // 1.0 fields
      for(;;)
      {
        rc = file.ReadUuid( m_mapping_id );
        if (!rc) break;
        if ( 0 == ON_UuidCompare(&obsolete_default_srfp_mapping_id,&m_mapping_id) )
          m_mapping_id = ON_nil_uuid;

        rc = file.ReadInt( &i );
        if (!rc) break;
        m_type = TypeFromInt(i);

        rc = file.ReadInt( &i );
        if (!rc) break;
        m_projection = ProjectionFromInt(i);

        rc = file.ReadXform( m_Pxyz );
        if (!rc) break;

        m_Pxyz.GetSurfaceNormalXform(m_Nxyz);

        rc = file.ReadXform( m_uvw );
        if (!rc) break;

        rc = file.ReadString(m_mapping_name);
        if (!rc) break;

        rc = (file.ReadObject(&m_mapping_primitive) >= 0);
        if (!rc) break;

        if ( minor_version >= 1 )
        {
          rc = file.ReadInt(&i);
          if (!rc) break;
          m_texture_space = TextureSpaceFromInt(i);

          rc = file.ReadBool(&m_bCapped);
          if (!rc) break;
        }

        break;
      }
    }

    if ( !file.EndRead3dmChunk() )
      rc = false;
  }

  return rc;
}

static
void GetSurfaceParametersHelper( const ON_Mesh& mesh,
                                 double tex_x, double tex_y, 
                                 double* srf_s, double* srf_t )
{
  // convert texture coordinates to surface parameters
  // Used to reconstruct m_S[] when old files are read.
  double tex_s, tex_t;

  if ( mesh.m_packed_tex_rotate ) 
  {
    // undo rotation and normalize
    tex_s =  mesh.m_packed_tex_domain[1].NormalizedParameterAt( tex_y );
    tex_t = 1.0 -  mesh.m_packed_tex_domain[0].NormalizedParameterAt( tex_x );
  }
  else 
  {
    // normalize
    tex_s =  mesh.m_packed_tex_domain[0].NormalizedParameterAt( tex_x );
    tex_t =  mesh.m_packed_tex_domain[1].NormalizedParameterAt( tex_y );
  }
  *srf_s =  mesh.m_srf_domain[0].ParameterAt(tex_s);
  *srf_t =  mesh.m_srf_domain[1].ParameterAt(tex_t);
}


ON_BOOL32 ON_Mesh::Read( ON_BinaryArchive& file )
{
  Destroy();

  int major_version = 0;
  int minor_version = 0;
  int i;
  bool rc = file.Read3dmChunkVersion(&major_version,&minor_version);
  
  if (rc && (1 == major_version || 3 == major_version) ) 
  {
    int vcount = 0;
    int fcount = 0;

    if (rc) rc = file.ReadInt( &vcount );
    if (rc) rc = file.ReadInt( &fcount );
    if (rc) rc = file.ReadInterval( m_packed_tex_domain[0] );
    if (rc) rc = file.ReadInterval( m_packed_tex_domain[1] );
    if (rc) rc = file.ReadInterval( m_srf_domain[0] );
    if (rc) rc = file.ReadInterval( m_srf_domain[1] );
    if (rc) rc = file.ReadDouble( 2, m_srf_scale );
    if (rc) rc = file.ReadFloat( 6, &m_vbox[0][0] );
    if (rc) rc = file.ReadFloat( 6, &m_nbox[0][0] );
    if (rc) rc = file.ReadFloat( 4, &m_tbox[0][0] );

    if (rc) rc = file.ReadInt( &m_closed );

    unsigned char b = 0;
    unsigned int tcode;
    int value;
    if (rc) rc = file.ReadChar(&b);
    if (rc && b) {
      rc = file.BeginRead3dmChunk(&tcode,&value);
      if (rc) {
        m_mesh_parameters = new ON_MeshParameters();
        rc = m_mesh_parameters->Read( file );
        if (!file.EndRead3dmChunk())
          rc = false;
      }
    }

    for ( i = 0; rc && i < 4; i++ ) {
      rc = file.ReadChar(&b);
      if (rc && b) {
        rc = file.BeginRead3dmChunk( &tcode, &value );
        if (rc) {
          m_kstat[i] = new ON_MeshCurvatureStats();
          rc = m_kstat[i]->Read(file);
          if ( !file.EndRead3dmChunk() )
            rc = false;
        }
      }
    }

    if (rc) rc = ReadFaceArray( vcount, fcount, file );

    if (rc) {
      if ( major_version==1) {
        rc = Read_1(file);
      }
      else if ( major_version == 3 ) {
        rc = Read_2(vcount,file);
      }
      else
        rc = false;
    }

    if ( minor_version >= 2 ) 
    {
      int b = m_packed_tex_rotate;
      if (rc) rc = file.ReadInt( &b );
      m_packed_tex_rotate = b?true:false;
    }

    if ( 3 == major_version )
    {
      if ( minor_version >= 3 )
      {
        // added for minor version 3.3
        if (rc) rc = file.ReadUuid( m_Ttag.m_mapping_id );

        // compressed m_S[]
        if ( rc && vcount > 0 ) 
        {
          size_t sz = 0;
          ON_BOOL32 bFailedCRC=false;
          if (rc) rc = file.ReadCompressedBufferSize( &sz );
          if (rc && sz) 
          {
            if ( sz == vcount*sizeof(m_S[0]) )
            {
              m_S.SetCapacity(vcount);
              if (rc) rc = file.ReadCompressedBuffer( sz, m_S.Array(), &bFailedCRC );
              if (rc) m_S.SetCount(vcount);
              if ( ON::big_endian == file.Endian() ) 
              {
                file.ToggleByteOrder( m_S.Count()*2, 8, m_S.Array(), (void*)m_S.Array() );
              }
            }
            else
            {
              rc = false; // buffer is wrong size
            }
          }
        }
        if ( minor_version >= 4 && file.ArchiveOpenNURBSVersion() >= 200606010 )
        {
          if (rc) rc = m_Ttag.Read(file);
        }
      }
    }

    if (    0 == m_S.Count()
         && m_V.Count() > 0
         && HasTextureCoordinates()
         && m_srf_domain[0].IsIncreasing() 
         && m_srf_domain[1].IsIncreasing() 
         && m_packed_tex_domain[0].IsInterval()
         && m_packed_tex_domain[1].IsInterval()
         && 0 == m_Ttag.m_mapping_crc
         && ON_UuidIsNil(m_Ttag.m_mapping_id)
          ) 
    {
      // This is a mesh from an old file - but there is enough 
      // information to calculate the m_S[] values from the 
      // m_T[] values.
      m_S.SetCapacity(vcount);
      m_S.SetCount(0);
      ON_2dPoint sp;
      ON_2fPoint tc;
      for ( i = 0; i < vcount; i++)
      {
        tc = m_T[i];
        sp.x = tc.x;
        sp.y = tc.y;
        GetSurfaceParametersHelper(*this,sp.x,sp.y,&sp.x,&sp.y);
        m_S.Append(sp);
      }
      m_Ttag.SetDefaultSurfaceParameterMappingTag();
    }
  }

  return rc;
}

ON::object_type ON_Mesh::ObjectType() const
{
  return ON::mesh_object;
}

int ON_Mesh::Dimension() const
{
  return 3;
}

#if defined(ON_COMPILER_MSC)
// Disable the MSC /W4 warning
//   C4189: 'breakpoint_here_for_bad_vbox' : local variable is initialized but not referenced
// on the line
//   int breakpoint_here_for_bad_vbox = 0.  
// The warning disable is here because MS is ignoring it
// if I put it inside of the function.
#pragma warning( push )
#pragma warning( disable : 4189 ) 
#endif

ON_BOOL32 ON_Mesh::GetBBox( // returns true if successful
       double* boxmin, // minimum
       double* boxmax, // maximum
       ON_BOOL32 bGrowBox
       ) const
{
  ON_BOOL32 rc = false;
  const int facet_count  = FaceCount();
  const int vertex_count = VertexCount();
  if ( facet_count >= 1 && vertex_count >= 3 ) 
  {
    ON_BoundingBox vbox;
    if ( m_vbox[0][0] > m_vbox[1][0] ) 
    {
      // const lie - cache mesh bounding box
      rc = ON_GetPointListBoundingBox( 3, 0, 
              vertex_count, 3, &m_V[0].x,
              const_cast<float*>(&m_vbox[0][0]), 
              const_cast<float*>(&m_vbox[1][0]), 
              false 
              );
    }
    else
    {
      rc = true;
    }

    if ( rc ) 
    {
      vbox.m_min.x = m_vbox[0][0];
      vbox.m_min.y = m_vbox[0][1];
      vbox.m_min.z = m_vbox[0][2];
      vbox.m_max.x = m_vbox[1][0];
      vbox.m_max.y = m_vbox[1][1];
      vbox.m_max.z = m_vbox[1][2];
      rc = vbox.IsValid();
      if (rc)
      {
        if ( bGrowBox )
        {
          vbox.Union( ON_BoundingBox(boxmin,boxmax) );
        }

        boxmin[0] = vbox.m_min.x;
        boxmin[1] = vbox.m_min.y;
        boxmin[2] = vbox.m_min.z;

        boxmax[0] = vbox.m_max.x;
        boxmax[1] = vbox.m_max.y;
        boxmax[2] = vbox.m_max.z;
      }
#if defined(ON_DEBUG) && !defined(ON_COMPILER_GNU)
      // generates gcc unused variable warning
      else
      {
        int breakpoint_here_for_bad_vbox=0;
      }
#endif
    }
  }
  return rc;
}

#if defined(ON_COMPILER_MSC)
#pragma warning( pop )
#endif

bool ON_Mesh::IsDeformable() const
{
  return true;
}

bool ON_Mesh::MakeDeformable()
{
  return true;
}

ON_BOOL32 ON_Mesh::Transform( 
       const ON_Xform& xform
       )
{
  TransformUserData(xform);
	DestroyTree();

  double d = xform.Determinant();
  const int vertex_count = VertexCount();
  ON_BOOL32 rc = ON_TransformPointList( 3, false, vertex_count, 3, &m_V[0][0], xform );

  if ( rc )
  {
    m_Ctag.Transform(xform);
    m_Ttag.Transform(xform);
    int tci, tccnt = m_TC.Count();
    for ( tci = 0; tci < tccnt; tci++ )
    {
      m_TC[tci].m_tag.Transform(xform);
    }
  }

  if ( rc && 0.0 == d )
  {
    // mesh has been squashed to a plane (or worse)
    if ( HasVertexNormals() )
    {
      ComputeFaceNormals();
      ComputeVertexNormals();
    }
    else if ( HasFaceNormals() )
    {
      ComputeFaceNormals();
    }
  }
  else if ( rc )
  {
    if ( HasVertexNormals() ) 
    {
      // See http://www.gignews.com/realtime020100.htm or these
      // references.
      //
      // 1. Hanrahan, Pat, 
      //    "A Survey of Ray-Surface Intersection Algorithms", 
      //     chapter 3 in Andrew Glassner (editor), 
      //     An Introduction to Ray Tracing, 
      //     Academic Press Inc., London, 1989.
      //
      // 2. Turkowski, Ken, 
      //    "Properties of Surface-Normal Transformations", 
      //     in Andrew Glassner (editor), 
      //     Graphics Gems, Academic Press, Inc., 
      //     pp. 539-547, 1990. 
      ON_Xform N_xform;
      double d = xform.GetSurfaceNormalXform(N_xform);
      rc = ON_TransformVectorList( 3, vertex_count, 3, &m_N[0][0], N_xform );
      if ( d < 0.0 )
      {
        FlipVertexNormals();
      }
      UnitizeVertexNormals();
    }

    if ( rc && HasFaceNormals() ) 
    {
      ComputeFaceNormals();
    }
  }

  if ( rc && HasPrincipalCurvatures() ) 
  {
    if ( fabs(fabs(d) - 1.0) > ON_SQRT_EPSILON ) 
    {
      // If it's a uniform scale, handle it, otherwise we can't do it.
      double scale = xform.m_xform[0][0];
      if ( 0.0 != scale && 0.0 != d
           && scale == xform.m_xform[1][1] 
           && scale == xform.m_xform[2][2] 
           && fabs(d - scale*scale*scale) <= d*ON_SQRT_EPSILON )
      {
        // uniform scale
        const double ks = 1.0/scale;
        ON_SurfaceCurvature* sc = m_K.Array();
        int ki = m_K.Count();
        while ( ki-- )
        {
          sc->k1 *= ks;
          sc->k2 *= ks;
          sc++;
        }

        // update curvature stats.
        for ( int j = 0; j < 4; j++ )
        {
          if ( m_kstat[j] )
            m_kstat[j]->Set( m_kstat[j]->m_style,m_K.Count(),m_K.Array(),m_N.Array() );
        }
      }
      else
      {
        ON_ERROR("ON_Mesh::Transform() cannot apply this transform to curvatures.\n");
        rc = false;
      }
    }
  }

  InvalidateVertexBoundingBox();
  InvalidateVertexNormalBoundingBox();
  if ( fabs(d) <= ON_ZERO_TOLERANCE )
    DestroyTopology(); // transform may not be one-to-one on vertices
  return rc;
}

void ON_Mesh::DestroyRuntimeCache( bool bDelete )
{
  int i;

  if (bDelete )
  {
    DestroyTree();
    DestroyPartition();
    m_top.Destroy();
    DeleteMeshParameters();
    InvalidateCurvatureStats();
  }
  else
  {
    // do not free any memory
    m_top.EmergencyDestroy();
  }

  InvalidateBoundingBoxes();
  m_partition = 0;
  m_mesh_parameters = 0;
  m_mtree = 0;
  m_top.m_mesh = this;
  m_parent = 0;
  //m_material_index = -1;
  m_closed = -1;
  for ( i = 0; i < 4; i++ ) 
  {
    m_kstat[i] = 0;
  }
}

ON_BOOL32 ON_Mesh::SwapCoordinates(
      int i, int j        // indices of coords to swap
      )
{
  const int vertex_count = VertexCount();
  ON_BOOL32 rc = ON_SwapPointListCoordinates( vertex_count, 3, &m_V[0][0], i, j );
  if ( rc && HasVertexNormals() ) {
    rc = ON_SwapPointListCoordinates( vertex_count, 3, &m_N[0][0], i, j );
  }
  if ( rc && i != j ) {
    float x;
    if ( m_vbox[0][0] <= m_vbox[1][0] ) {
      x = m_vbox[0][i]; m_vbox[0][i] = m_vbox[0][j]; m_vbox[0][j] = x;
      x = m_vbox[1][i]; m_vbox[1][i] = m_vbox[1][j]; m_vbox[1][j] = x;
    }
    if ( m_nbox[0][0] <= m_nbox[1][0] ) {
      x = m_nbox[0][i]; m_nbox[0][i] = m_nbox[0][j]; m_nbox[0][j] = x;
      x = m_nbox[1][i]; m_nbox[1][i] = m_nbox[1][j]; m_nbox[1][j] = x;
    }
  }
  return rc;
}

void ON_Mesh::SetClosed(int b)
{
  // 6 Novermber 2003 Dale Lear - let expert user set m_closed to all 4 of its
  //                              valid values.
  switch(b)
  {
  case 0:
    m_closed = 0;
    break;
  case 1:
    m_closed = 1;
    break;
  case 2:
    m_closed = 2;
    break;
  default:
    m_closed = -1;
    break;
  }
}

static 
int ON_MeshIsManifold_CompareV( const void* a, const void* b )
{
  return memcmp(a,b,sizeof(ON_3fPoint));
  /*
  float d;
  const float* fa = (const float*)a;
  const float* fb = (const float*)b;
  if ( 0.0f == (d = (*fa++ - *fb++)) )
  {
    if ( 0.0f == (d = (*fa++ - *fb++)) )
    {
      if ( 0.0f == (d = (*fa++ - *fb++)) )
        return 0;
    }
  }
  return ( d < 0.0f ) ? -1 : 1;
  */
}

static 
int ON_MeshGetVertexEdges_Compare2dex( const void* a, const void* b )
{
  return ON_Compare2dex((const ON_2dex*)a,(const ON_2dex*)b);
}

static 
int ON_MeshIsManifold_Compare3dex( const void* a, const void* b )
{
  return ON_Compare3dex((const ON_3dex*)a,(const ON_3dex*)b);
}

//static 
//int ON_MeshGetVertexEdges_CompareInt( const int* a, const int* b )
//{
//  return (*a-*b);
//}


int ON_Mesh::GetVertexEdges( 
  int vertex_index_count,
  const int* vertex_index, 
  bool bNoDuplicates,
  ON_SimpleArray<ON_2dex>& edges
  ) const
{
  // Get edges connected to vertices in vertex_index[] array.
  const int edges_count0 = edges.Count();

  const int mesh_vcount = m_V.Count();

  //03/12/2007 TimH. The line below appears to be a typo.  Using the following line works better.
  //const int mesh_fcount = m_V.Count();
  const int mesh_fcount = m_F.Count();

  if (   vertex_index_count <= 0 || 0 == vertex_index 
      || mesh_fcount <= 0 || mesh_vcount < 3 )
  {
    return 0;
  }

  int vei, efi, fvi, ei, fi, j, n, vi;
  const int* f_vi;
  ON_2dex edge_ends;
  const ON_MeshFace* f = m_F.Array();

  if (    m_top.m_bIsValid 
       && mesh_vcount == m_top.m_topv_map.Count()
       && m_top.m_tope.Count() > 0 )
  {
    // Topology looks good; use it to speed up the search.
    const int* topv_map = m_top.m_topv_map;
    const int top_vcount = m_top.m_topv.Count();
    const int top_ecount = m_top.m_tope.Count();
    int top_vi;
    for ( n = 0; n < vertex_index_count; n++ )
    {
      vi = vertex_index[n];
      if ( vi < 0 || vi >= mesh_vcount )
        continue;
      top_vi = topv_map[vi];
      if ( top_vi < 0 || top_vi > top_vcount )
        continue;
      edge_ends.i = vi;
      const ON_MeshTopologyVertex& v = m_top.m_topv[top_vi];
      for ( vei = 0; vei < v.m_tope_count; vei++ )
      {
        ei = v.m_topei[vei];
        if ( ei < 0 || ei >= top_ecount )
          continue;
        const ON_MeshTopologyEdge& e = m_top.m_tope[ei];
        for ( efi = 0; efi < e.m_topf_count; efi++ )
        {
          fi = e.m_topfi[efi];
          if ( fi < 0 || fi >= mesh_fcount )
            continue;
          f_vi = f[fi].vi;
          for ( fvi = 0; fvi < 4; fvi++ )
          {
            if ( f_vi[fvi] == vi )
            {
              j = f_vi[(fvi+3)%4];
              if ( j >= 0 && j < mesh_vcount && vi != j )
              {
                edge_ends.i = j;
                edge_ends.j = vi;
                edges.Append(edge_ends);
              }
              j = f_vi[ (2==fvi && f_vi[2]==f_vi[3]) ? 0 : ((fvi+1)%4) ];
              if ( j >= 0 && j < mesh_vcount && vi != j )
              {
                edge_ends.i = vi;
                edge_ends.j = j;
                edges.Append(edge_ends);
              }
              break; // done with this face
            }
          }
        }
      }
    }
  }
  else
  {
    // slow-n-stupid search through all the faces

    // Sort vertex_index[] array so we can use a quick
    // binary search to see if a face is using one of 
    // the vertices in the list.
    ON_Workspace ws;
    for ( vi = 1; vi < vertex_index_count; vi++ )
    {
      if ( vertex_index[vi] < vertex_index[vi-1] )
      {
        // need to sort vertex_index[] array
        int* tmp = ws.GetIntMemory(vertex_index_count);
        memcpy(tmp,vertex_index,vertex_index_count*sizeof(tmp[0]));
        ON_SortIntArray(ON::heap_sort,tmp,vertex_index_count);
        vertex_index = tmp;
        break;
      }
    }

    // Find all the faces that use a vertex in the vertex_index[] array.
    for ( fi = 0; fi < mesh_fcount; fi++ )
    {
      f_vi = f[fi].vi;
      for ( fvi = 0; fvi < 4; fvi++ )
      {
        vi = f_vi[fvi];
        if ( ON_BinarySearchIntArray(vi,vertex_index,vertex_index_count) )
        {
          // vi is in the vertex_index[] array.  Add the edges
          // of this face that begin and end at this vertex.
          j = f_vi[(fvi+3)%4];
          if ( j >= 0 && j < mesh_vcount && vi != j )
          {
            edge_ends.i = j;
            edge_ends.j = vi;
            edges.Append(edge_ends);
          }
          j = f_vi[ (2==fvi && f_vi[2]==f_vi[3]) ? 0 : ((fvi+1)%4) ];
          if ( j >= 0 && j < mesh_vcount && vi != j )
          {
            edge_ends.i = vi;
            edge_ends.j = j;
            edges.Append(edge_ends);
          }
        }
      }
    }
  }

  if ( bNoDuplicates && edges.Count() > edges_count0 )
  {
    for ( ei = edges_count0; ei < edges.Count(); ei++ )
    {
      edge_ends = edges[ei];
      if ( edge_ends.i > edge_ends.j )
      {
        j = edge_ends.i; edge_ends.i = edge_ends.j; edge_ends.j = j;
      }
    }
    ON_hsort( edges.Array() + edges_count0,
              edges.Count() - edges_count0,
              sizeof(edge_ends), 
              ON_MeshGetVertexEdges_Compare2dex);
    edge_ends = edges[edges_count0];
    for ( ei = j = edges_count0+1; ei < edges.Count(); ei++ )
    {
      if ( ON_Compare2dex(&edge_ends,&edges[ei]) )
      {
        edge_ends = edges[ei];
        if ( j != ei )
          edges[j] = edge_ends;
        j++;
      }
    }
    edges.SetCount(j);
  }

  return (edges.Count() - edges_count0);
}

int ON_Mesh::GetMeshEdges( 
  ON_SimpleArray<ON_2dex>& edges
  ) const
{
  const int edges_count0 = edges.Count();
  int fi, ei, j, fvi;
  const int* f_vi;
  const ON_MeshFace* f = m_F.Array();
  const int mesh_vcount = m_V.Count();
  const int mesh_fcount = m_F.Count();
  edges.Reserve( edges_count0 + 4*mesh_fcount );
  ON_2dex e;

  // Find all the faces that use a vertex in the vertex_index[] array.
  for ( fi = 0; fi < mesh_fcount; fi++ )
  {
    f_vi = f[fi].vi;
    ei = f_vi[3];
    for ( fvi = 0; fvi < 4; fvi++ )
    {
      e.i = ei;
      ei = *f_vi++;
      e.j = ei;
      if ( e.i > e.j )
      {
        j = e.i; e.i = e.j; e.j = j;
      }
      if ( e.i != e.j && e.i >= 0 && e.j < mesh_vcount )
      {
        edges.Append(e);
      }
    }
  }
  

  if ( edges.Count() > edges_count0 )
  {
    ON_hsort( edges.Array() + edges_count0,
              edges.Count() - edges_count0,
              sizeof(e), 
              ON_MeshGetVertexEdges_Compare2dex);
    e = edges[edges_count0];
    for ( ei = j = edges_count0+1; ei < edges.Count(); ei++ )
    {
      if ( ON_Compare2dex(&e,&edges[ei]) )
      {
        e = edges[ei];
        if ( j != ei )
          edges[j] = e;
        j++;
      }
    }
    edges.SetCount(j);
  }

  return edges.Count() - edges_count0;
}



bool ON_Mesh::IsManifold(
  bool bTopologicalTest,
  bool* pbIsOriented,
  bool* pbHasBoundary
  ) const
{
  bool bIsManifold = false;
  if ( pbIsOriented )
    *pbIsOriented = false;
  if ( pbHasBoundary )
    *pbHasBoundary = false;
  const int vcount = m_V.Count();
  const int fcount = m_F.Count();
  if ( vcount > 0 && fcount > 0 )
  {
    ON_Workspace ws;
    ON_3dex e;
    int i, j, ecount;
    const int* fvi;
    ON_3fPoint v0;
    const ON_3fPoint* v;
    const ON_MeshFace* f;
    int* vid = ws.GetIntMemory(vcount);
    ON_3dex* edge = (ON_3dex*)ws.GetMemory(4*fcount*sizeof(*edge));

    if ( bTopologicalTest )
    {
      // coincident vertices are assigned the same vertex id
      ON_Sort(ON::heap_sort,vid,m_V.Array(),vcount,sizeof(m_V[0]),ON_MeshIsManifold_CompareV);
      ecount = 0;
      v = m_V.Array();
      ecount = 0;
      j = vcount;
      for ( i = 0; i < vcount; i = j)
      {
        v0 = v[vid[i]];
        vid[i] = ecount;
        for ( j = i+1; j < vcount; j++ )
        {
          if ( ON_MeshIsManifold_CompareV(&v,v+vid[j]) )
          {
            ecount++;
            break;
          }
          vid[j] = ecount;
        }
      }
    }
    else
    {
      // each vertex gets a unique id.
      for ( i = 0; i < vcount; i++ )
        vid[i] = i;
    }

    // build a list of edges
    f = m_F.Array();
    ecount = 0;
    for ( i = 0; i < fcount; i++ )
    {
      fvi = (f++)->vi;
      if (   fvi[0] >= 0 && fvi[0] < vcount 
          && fvi[1] >= 0 && fvi[1] < vcount
          && fvi[2] >= 0 && fvi[2] < vcount
          && fvi[3] >= 0 && fvi[3] < vcount )
      {
        // loop unrolled for speed
        j = ecount;
        e.i = vid[fvi[0]];  e.j = vid[fvi[1]];
        if ( 0 != (e.k = e.j - e.i) )
        {
          if ( e.k < 0 ) {e.k = e.i; e.i = e.j; e.j = e.k; e.k = 1;} else e.k = 0;
          edge[ecount++] = e;
        }
        e.i = vid[fvi[1]];  e.j = vid[fvi[2]];
        if ( 0 != (e.k = e.j - e.i) )
        {
          if ( e.k < 0 ) {e.k = e.i; e.i = e.j; e.j = e.k; e.k = 1;} else e.k = 0;
          edge[ecount++] = e;
        }
        e.i = vid[fvi[2]];  e.j = vid[fvi[3]];
        if ( 0 != (e.k = e.j - e.i) )
        {
          if ( e.k < 0 ) {e.k = e.i; e.i = e.j; e.j = e.k; e.k = 1;} else e.k = 0;
          edge[ecount++] = e;
        }
        e.i = vid[fvi[3]];  e.j = vid[fvi[0]];
        if ( 0 != (e.k = e.j - e.i) )
        {
          if ( e.k < 0 ) {e.k = e.i; e.i = e.j; e.j = e.k; e.k = 1;} else e.k = 0;
          edge[ecount++] = e;
        }
        if ( ecount < j+3 )
          ecount = j;
      }
    }

    if ( ecount >= 4 )
    {
      bIsManifold = true;
      bool bIsOriented  = (pbIsOriented)  ? true  : false;
      bool bHasBoundary = (pbHasBoundary) ? false : true;
      ON_hsort(edge,ecount,sizeof(edge[0]),ON_MeshIsManifold_Compare3dex);

      i = 0;
      e = *edge;
      while ( --ecount )
      {
        edge++;
        if ( memcmp(&e,edge,2*sizeof(int)) )
        {
          if (!i)
            bHasBoundary = true;
          e = *edge;
          i = 0;
        }
        else
        {
          if ( i++ )
          {
            bIsManifold = false;
            break;
          }
          if ( e.k == edge->k )
            bIsOriented = false;
        }
      }

      if ( bIsManifold )
      {
        if ( pbIsOriented )
          *pbIsOriented = bIsOriented;
        if ( pbHasBoundary )
          *pbHasBoundary = bHasBoundary;
      }
    }
  }

  return bIsManifold;
}


bool ON_Mesh::IsClosed() const
{
  if ( m_closed < 0 ) {
    // lazy evaluation
    int closed = 0;
    const ON_MeshTopology& top = Topology();
    const int ecnt = top.m_tope.Count();
    int ei, efcnt;
    if ( ecnt >= 6 ) {
      closed = 1;
      for ( ei = 0; ei < ecnt; ei++ ) {
        efcnt = top.m_tope[ei].m_topf_count;
        if ( efcnt == 1 ) {
          closed = 0;
          break;
        }
        if ( efcnt > 2 ) {
          closed = 2;
        }
      }
    }
    ((ON_Mesh*)this)->m_closed = closed;
  }
  return (m_closed>0) ? true : false;
}

bool ON_Mesh::SetVertex(
       int vertex_index,
       const ON_3dPoint& vertex_location
       )
{
  ON_3fPoint p((float)vertex_location.x, (float)vertex_location.y, (float)vertex_location.z);
  return SetVertex( vertex_index, p );
}

bool ON_Mesh::SetVertex(
       int vertex_index,
       const ON_3fPoint& vertex_location
       )
{
  bool rc = false;
  int vertex_count = m_V.Count();
  if ( vertex_index >= 0 ) {
    if ( vertex_index < vertex_count ) {
      m_V[vertex_index] = vertex_location;
      rc = true;
    }
    else if ( vertex_index == vertex_count ) {
      m_V.Append(vertex_location);
      rc = true;
    }
  }
  return rc;
}

bool ON_Mesh::SetVertexNormal(
       int vertex_index,
       const ON_3dVector& normal
       )
{
  bool rc = false;
  // use double precision for unitizing normal
  ON_3dVector unit_vector = normal;
  const bool bUnitVector = unit_vector.Unitize();
  ON_3fVector v((float)unit_vector.x, (float)unit_vector.y, (float)unit_vector.z);
  int normal_count = m_N.Count();
  if ( vertex_index >= 0 ) {
    if ( vertex_index < normal_count ) {
      m_N[vertex_index] = v;
      rc = bUnitVector;
    }
    else if ( vertex_index == normal_count ) {
      m_N.Append(v);
      rc = bUnitVector;
    }
  }
  return rc;
}

bool ON_Mesh::SetVertexNormal(
       int vertex_index,
       const ON_3fVector& normal
       )
{
  ON_3dVector v(normal.x,normal.y,normal.z);
  return SetVertexNormal(vertex_index,v);
}

bool ON_Mesh::SetTextureCoord(
       int vertex_index,
       double s, double t    // texture coordinates
       )
{
  ON_2fPoint tc((float)s,(float)t);
  bool rc = false;
  int vertex_count = m_T.Count();
  if ( vertex_index >= 0 ) {
    if ( vertex_index < vertex_count ) {
      m_T[vertex_index] = tc;
      rc = true;
    }
    else if ( vertex_index == vertex_count ) {
      m_T.Append(tc);
      rc = true;
    }
  }
  return rc;
}


bool ON_Mesh::SetTriangle(
       int face_index,
       int a,int b ,int c // vertex indices
       )
{
  return SetQuad( face_index, a,b,c,c );
}

bool ON_Mesh::SetQuad(
       int face_index,
       int a, int b, int c, int d // vertex indices
       )
{
  bool rc = false;
  int face_count = m_F.Count();
  if ( face_index >= 0 ) {
    ON_MeshFace f;
    f.vi[0] = a;
    f.vi[1] = b;
    f.vi[2] = c;
    f.vi[3] = d;
    if ( face_index < face_count ) {
      m_F[face_index] = f;
      rc = true;
    }
    else if ( face_index == face_count ) {
      m_F.Append(f);
      rc = true;
    }
    if ( rc )
      rc = f.IsValid(m_V.Count());
  }
  return rc;
}



int ON_Mesh::FaceCount() const
{
  return m_F.Count();
}

int ON_Mesh::QuadCount() const
{
  // number of faces that are quads
  if (   m_quad_count     < 0
      || m_triangle_count < 0 
      || m_invalid_count  < 0 
      || m_quad_count + m_triangle_count + m_invalid_count != FaceCount() ) 
  {
    const_cast<ON_Mesh*>(this)->CountQuads();
  }
  return m_quad_count;
}

int ON_Mesh::TriangleCount() const
{
  // number of faces that are triangles
  QuadCount(); // makes sure counts are valid
  return m_triangle_count;
}

int ON_Mesh::VertexCount() const
{
  return m_V.Count();
}

bool ON_Mesh::HasVertexNormals() const
{
  const int vertex_count = VertexCount();
  return ( vertex_count > 0 && m_N.Count() == vertex_count ) ? true : false;
}

bool ON_Mesh::HasFaceNormals() const
{
  const int face_count = FaceCount();
  return ( face_count > 0 && m_FN.Count() == face_count ) ? true : false;
}

bool ON_Mesh::HasTextureCoordinates() const
{
  const int vertex_count = VertexCount();
  return ( vertex_count > 0 && m_T.Count() == vertex_count ) ? true : false;
}

bool ON_Mesh::HasCachedTextureCoordinates() const
{
  const int vertex_count = VertexCount();
  if (vertex_count > 0 )
  {
    int tci, tccount = m_TC.Count();
    for ( tci = 0; tci < tccount; tci++ )
    {
      if ( vertex_count == m_TC[tci].m_T.Count() )
        return true;
    }
  }
  return false;
}

const ON_TextureCoordinates* 
ON_Mesh::CachedTextureCoordinates( const ON_UUID& mapping_id ) const
{
  const int vertex_count = VertexCount();
  if (vertex_count > 0 )
  {
    const ON_TextureCoordinates* TC = m_TC.Array();
    int tci, tccount = m_TC.Count();
    for ( tci = 0; tci < tccount; tci++ )
    {
      if (   vertex_count == TC->m_T.Count() 
          && mapping_id == TC->m_tag.m_mapping_id )
      {
        return TC;
      }
    }
  }
  return 0;
}

bool ON_Mesh::HasSurfaceParameters() const
{
  const int vertex_count = VertexCount();
  return ( vertex_count > 0 && m_S.Count() == vertex_count ) ? true : false;
}

bool ON_Mesh::HasPrincipalCurvatures() const
{
  const int vertex_count = VertexCount();
  return ( vertex_count > 0 && m_K.Count() == vertex_count ) ? true : false;
}

bool ON_Mesh::HasVertexColors() const
{
  const int vertex_count = VertexCount();
  return ( vertex_count > 0 && m_C.Count() == vertex_count ) ? true : false;
}

void ON_Mesh::InvalidateBoundingBoxes()
{
  InvalidateVertexBoundingBox();
  InvalidateVertexNormalBoundingBox();
  InvalidateTextureCoordinateBoundingBox();
  InvalidateCurvatureStats();
}

void ON_Mesh::InvalidateVertexBoundingBox()
{
  m_vbox[0][0] = m_vbox[0][1] = m_vbox[0][2] =  1.0;
  m_vbox[1][0] = m_vbox[1][1] = m_vbox[1][2] = -1.0;
}

void ON_Mesh::InvalidateVertexNormalBoundingBox()
{
  m_nbox[0][0] = m_nbox[0][1] = m_nbox[0][2] =  1.0;
  m_nbox[1][0] = m_nbox[1][1] = m_nbox[1][2] = -1.0;
}

void ON_Mesh::InvalidateTextureCoordinateBoundingBox()
{
  m_tbox[0][0] = m_tbox[0][1] =  1.0;
  m_tbox[1][0] = m_tbox[1][1] = -1.0;
}

void ON_Mesh::InvalidateCurvatureStats()
{
  int i;
  for ( i = 0; i < 4; i++ ) {
    if ( m_kstat[i] ) {
      delete m_kstat[i];
      m_kstat[i] = 0;
    }
  }
}

bool ON_Mesh::UnitizeVertexNormals()
{
  bool rc = HasVertexNormals();
  if ( rc ) {
    const int vertex_count = VertexCount();
    float* n = &m_N[0][0];
    int i;
    ON_3dVector N;
    for ( i = 0; i < vertex_count; i++ ) {
      N.x = n[0];
      N.y = n[1];
      N.z = n[2];
      if ( !N.Unitize() )
        rc = false;
      *n++ = (float)N.x;
      *n++ = (float)N.y;
      *n++ = (float)N.z;
    }
  }
  return rc;
}

bool ON_Mesh::UnitizeFaceNormals()
{
  bool rc = HasFaceNormals();
  if ( rc ) {
    const int face_count = FaceCount();
    float* n = &m_FN[0][0];
    int i;
    ON_3dVector N;
    for ( i = 0; i < face_count; i++ ) {
      N.x = n[0];
      N.y = n[1];
      N.z = n[2];
      if ( !N.Unitize() )
        rc = false;
      *n++ = (float)N.x;
      *n++ = (float)N.y;
      *n++ = (float)N.z;
    }
  }
  return rc;
}


bool ON_Mesh::GetCurvatureStats( // returns true if successful
       ON::curvature_style kappa_style,
       ON_MeshCurvatureStats& stats
       ) const
{
  bool rc = false;
  stats.Destroy();
  int ksi;
  switch ( kappa_style ) {
    case ON::gaussian_curvature:
      ksi = 0;
      break;
    case ON::mean_curvature:
      ksi = 1;
      break;
    case ON::min_curvature: // minimum unsigned radius of curvature
      ksi = 2;
      break;
    case ON::max_curvature: // maximum unsigned radius of curvature
      ksi = 3;
      break;
    //case ON::section_curvature_x:
    //  ksi = 4;
    //  break;
    //case ON::section_curvature_y:
    //  ksi = 5;
    //  break;
    //case ON::section_curvature_z:
    //  ksi = 6;
    //  break;
    default:
      ksi = -1;
      break;
  }
  if ( ksi >= 0 && ksi <= 3 && HasPrincipalCurvatures() ) {
    ON_Mesh* p = (ON_Mesh*)this; // const lie 
    if ( !m_kstat[ksi] ) {
      p->m_kstat[ksi] = new ON_MeshCurvatureStats();
      p->m_kstat[ksi]->Set( kappa_style, m_K.Count(), m_K.Array(), m_N.Array() );
    }
    if ( p->m_kstat[ksi] ) {
      stats = *p->m_kstat[ksi];
      rc = true;
    }
  }
  return rc;
}


const ON_MeshTopology& ON_Mesh::Topology() const
{
  if ( !m_top.m_bIsValid ) {
    // const lie since volitile members are not universally supported
    ON_MeshTopology* top = (ON_MeshTopology*)&m_top;
    top->m_mesh = this;
    top->Create();
  }
  return m_top;
}

void ON_Mesh::DestroyTopology()
{
  m_top.Destroy();
}

bool
ON_MeshFace::IsTriangle() const 
{
  return vi[2]==vi[3];
}

bool
ON_MeshFace::IsQuad() const 
{
  return vi[2]!=vi[3];
}

void
ON_MeshFace::Flip()
{
  int i;
  if ( vi[2] == vi[3] ) {
    i = vi[1];
    vi[1] = vi[2];
    vi[2] = i;
    vi[3] = i;
  }
  else {
    i = vi[1];
    vi[1] = vi[3];
    vi[3] = i;
  }
}

/*
ON_MeshEdge::ON_MeshEdge() : m_fcount(0)
{
  m_fi = m_private_fi;
  m_private_fi[0] = 0;
  m_private_fi[0] = 0;
}

ON_MeshEdge::~ON_MeshEdge()
{
  if ( m_fi && m_fi != m_private_fi ) onfree((void*)m_fi);
}

ON_MeshEdge::ON_MeshEdge( const ON_MeshEdge& src ) : m_fcount(0), m_fi(0)
{
  *this = src;
}

int ON_MeshEdge::FaceIndexCount() const
{
  return m_fcount;
}

const int* ON_MeshEdge::FaceIndexArray() const
{
  return m_fi;
}

int& ON_MeshEdge::operator[](int i)
{
  return (i>=0&&i<m_fcount) ? m_fi[i] : m_private_fi[0];
}

int ON_MeshEdge::operator[](int i) const
{
  return (i>=0&&i<m_fcount) ? m_fi[i] : m_private_fi[0];
}
*/

void 
ON_Mesh::Flip()
{
  FlipFaceOrientation();
  FlipFaceNormals();
  FlipVertexNormals();

  // Do not modify m_S[] or m_T[]
  // values here.
}

void 
ON_Mesh::FlipVertexNormals()
{
  int i;
  const int vcount = VertexCount();
  if ( HasVertexNormals() ) {
    for ( i = 0; i < vcount; i++ ) {
      m_N[i].Reverse();
    }
  }
}

void 
ON_Mesh::FlipFaceNormals()
{
  int i;
  const int fcount = FaceCount();
  if ( HasFaceNormals() ) {
    for( i = 0; i < fcount; i++ ) {
      m_FN[i].Reverse();
    }
  }
}

void 
ON_Mesh::FlipFaceOrientation()
{
  int i;
  const int fcount = FaceCount();
  for( i = 0; i < fcount; i++ ) {
    m_F[i].Flip();
  }
  if ( fcount > 0 )
    DestroyTopology(); // flipping changes order of face corners
}

bool
ON_Mesh::ComputeFaceNormal( int fi )
{
  bool rc = false;
  const int fcount = FaceCount();
  if ( fi >= 0 && fi < fcount && HasFaceNormals() ) 
  {
    ON_3fVector a, b, n;
    const int* vi = m_F[fi].vi;
    a = m_V[vi[2]] - m_V[vi[0]];
    b = m_V[vi[3]] - m_V[vi[1]];
    n = ON_CrossProduct( a, b ); // works for triangles, quads, and nonplanar quads
    rc = n.Unitize();
    m_FN[fi] = n;
  }
  return rc;
}

bool
ON_Mesh::ComputeFaceNormals()
{
  bool rc = false;
  const int fcount = FaceCount();
  if ( fcount > 0 ) {
    ON_3fVector a, b, n;
    int fi;
    const int* vi;
    if ( m_FN.Capacity() < fcount )
      m_FN.SetCapacity(fcount);
    m_FN.SetCount(0);
    rc = true;
    for ( fi = 0; fi < fcount; fi++ ) {
      vi = m_F[fi].vi;
      a = m_V[vi[2]] - m_V[vi[0]];
      b = m_V[vi[3]] - m_V[vi[1]];
      n = ON_CrossProduct( a, b ); // works for triangles, quads, and nonplanar quads
      n.Unitize();
      m_FN.Append(n);
    }
  }
  else {
    m_FN.Destroy();
  }
  return rc;
}


static int compareRadial3fPoint( const ON_3fPoint* a, const ON_3fPoint* b )
{
  double ar = a->x+a->y+a->z;
  double br = b->x+b->y+b->z;
  if ( ar < br )
    return -1;
  if ( ar > br )
    return 1;
  return 0;
}

bool ON_Mesh::CombineCoincidentVertices( 
        const ON_3fVector tolerance,
        double cos_normal_angle // = -1.0  // cosine(break angle) -1.0 will merge all coincident vertices
        )
{

  bool rc = false;
  const int vcount = VertexCount();
  if ( vcount > 0 && rc ) {
    ON_Workspace ws;
    int* index = ws.GetIntMemory(vcount);
    rc = m_V.Sort( ON::quick_sort, index, compareRadial3fPoint );
    int i, j;
    ON_3fPoint p0, p1, pmin, pmax;
    for ( i = 0; i < vcount; i++ ) {
      p0 = m_V[i];
      pmin = p0 - tolerance;
      pmax = p0 + tolerance;
      for ( j = i+1; j < vcount; j++ ) {
        p1 = m_V[j];
        // TODO        
      }
    }
  }
  return false;
}


struct tagMESHPOINTS
{
  // p0 = bogus pointer - never dereferenced - that is used
  //      to calculate vertex index in CompareMeshPoint().
  const char* p0;
  ON_3fPoint*  V;
  ON_2fPoint*  T;
  ON_3fVector* N;
  ON_SurfaceCurvature* K;
  ON_Color* C;
};

static int CompareMeshPoint(const void* a,const void* b,void* ptr)
{
  float d;
  const struct tagMESHPOINTS * mp = (const struct tagMESHPOINTS *)ptr;

  // use bogus pointer to convert a,b into vertex indices
  int i = (int)(((const char*)a) - mp->p0); // the (int) is for 64 bit size_t conversion
  int j = (int)(((const char*)b) - mp->p0);

  d = mp->V[j].x - mp->V[i].x;
  if ( d == 0.0f )
  {
    d = mp->V[j].y - mp->V[i].y;
    if ( d == 0.0f )
    {
      d = mp->V[j].z - mp->V[i].z;

      //if ( d == 0.0f )
      //  return 0;

      if ( d == 0.0f && 0 != mp->N)
      {
        d = mp->N[j].x - mp->N[i].x;
        if ( d == 0.0f )
        {
          d = mp->N[j].y - mp->N[i].y;
          if ( d == 0.0f )
          {
            d = mp->N[j].z - mp->N[i].z;
          }
        }
      }

      if ( d == 0.0f && 0 != mp->T)
      {
        d = mp->T[j].x - mp->T[i].x;
        if ( d == 0.0f )
        {
          d = mp->T[j].y - mp->T[i].y;
        }
      }

      if ( d == 0.0f && 0 != mp->C )
      {
        int u = ((int)mp->C[j])-((int)mp->C[i]);
        if ( u < 0 )
          d = -1.0f;
        else if ( u > 0 )
          d = 1.0f;
      }

      if ( d == 0.0f && 0 != mp->K )
      {
        double dk = mp->K[j].k1 - mp->K[i].k1;
        if ( dk < 0.0 )
          d = -1.0;
        else if ( dk > 0.0 )
          d = 1.0;
        else
        {
          dk = mp->K[j].k2 - mp->K[i].k2;
          if ( dk < 0.0 )
            d = -1.0;
          else if ( dk > 0.0 )
            d = 1.0;
        }
      }
    }
  }
  
  if ( d < 0.0f )
    return -1;
  if ( d > 0.0f )
    return 1;
  return 0;
}

bool ON_Mesh::CombineIdenticalVertices(
                                bool bIgnoreVertexNormals,
                                bool bIgnoreTextureCoordinates
                                )
{
  // 11 June 2003 - added and tested.
  bool rc = false;
  ON_Mesh& mesh = *this;

  int vertex_count = mesh.VertexCount();
  if ( vertex_count > 0 )
  {
    ON_SimpleArray<int> index_array(vertex_count);
    ON_SimpleArray<int> remap_array(vertex_count);

    int remap_vertex_count = 0;
    int merge_count = 0;
    int i0, i1, k;

    struct tagMESHPOINTS mp;
    memset(&mp,0,sizeof(mp));
    mp.p0 = (const char*)&mp; // bogus pointer - never dereferenced
    mp.V = mesh.m_V.Array();
    mp.N = mesh.HasVertexNormals()       ? mesh.m_N.Array() : 0;
    mp.T = mesh.HasTextureCoordinates()  ? mesh.m_T.Array() : 0;
    mp.C = mesh.HasVertexColors()        ? mesh.m_C.Array() : 0;
    mp.K = mesh.HasPrincipalCurvatures() ? mesh.m_K.Array() : 0;

    if ( bIgnoreVertexNormals )
    {
      mp.N = 0;
    }

    if ( bIgnoreTextureCoordinates )
    {
      mp.T = 0;
      mp.C = 0;
      mp.K = 0;
    }

    index_array.SetCount(vertex_count);
    index_array.Zero();
    int* index = index_array.Array();

    remap_array.SetCount(vertex_count);
    int* remap = remap_array.Array();
    for ( k = 0; k < vertex_count; k++ )
      remap[k] = -1;

    ON_Sort( 
          ON::heap_sort,
          index,
          mp.p0,                 // data buffer
          vertex_count,
          sizeof(*mp.p0),
          CompareMeshPoint,
          &mp
         );

    for ( i0 = 0; i0 < vertex_count; i0 = i1 )
    {
      for ( i1 = i0+1; i1 < vertex_count; i1++ )
      {
        if ( CompareMeshPoint( mp.p0+index[i0], mp.p0+index[i1], &mp ) )
          break;
        else
          merge_count++;
      }
      for ( /*empty*/; i0 < i1; i0++ )
      {
        remap[index[i0]] = remap_vertex_count;
      }
      remap_vertex_count++;
    }

    if ( bIgnoreVertexNormals )
    {
      mp.N = mesh.HasVertexNormals() ? mesh.m_N.Array() : 0;
    }

    if ( bIgnoreTextureCoordinates )
    {
      mp.T = mesh.HasTextureCoordinates()  ? mesh.m_T.Array() : 0;
      mp.C = mesh.HasVertexColors()        ? mesh.m_C.Array() : 0;
      mp.K = mesh.HasPrincipalCurvatures() ? mesh.m_K.Array() : 0;
    }

    if ( remap_vertex_count > 0 && remap_vertex_count < vertex_count )
    {
      ON_SimpleArray<ON_3fPoint> p_array(remap_vertex_count);
      p_array.SetCount(remap_vertex_count);
      ON_3fPoint* p = p_array.Array();
      ON_3fVector* v = (ON_3fVector*)p;

      for ( k = 0; k < vertex_count; k++ )
      {
        p[remap[k]] = mp.V[k];
      }
      for ( k = 0; k < remap_vertex_count; k++ )
        mp.V[k] = p[k];
      mesh.m_V.SetCount(remap_vertex_count);

      if ( 0 != mp.N )
      {
        if ( bIgnoreVertexNormals )
        {
          // average vertex normals of combined vertices
          p_array.Zero();
          for ( k = 0; k < vertex_count; k++ )
          {
            v[remap[k]] += mp.N[k];
          }
          for ( k = 0; k < remap_vertex_count; k++ )
          {
            v[k].Unitize();
          }
        }
        else
        {
          for ( k = 0; k < vertex_count; k++ )
          {
            v[remap[k]] = mp.N[k];
          }
        }
        for ( k = 0; k < remap_vertex_count; k++ )
          mp.N[k] = v[k];
        mesh.m_N.SetCount(remap_vertex_count);
      }
      else
        mesh.m_N.SetCount(0);

      if ( 0 != mp.T && !bIgnoreTextureCoordinates )
      {
        for ( k = 0; k < vertex_count; k++ )
        {
          p[remap[k]] = mp.T[k];
        }
        for ( k = 0; k < remap_vertex_count; k++ )
          mp.T[k] = p[k];
        mesh.m_T.SetCount(remap_vertex_count);
      }
      else
        mesh.m_T.SetCount(0);

      if ( 0 != mp.C && !bIgnoreTextureCoordinates )
      {
        ON_SimpleArray<ON_Color> c_array(remap_vertex_count);
        c_array.SetCount(remap_vertex_count);
        ON_Color* c = c_array.Array();
        for ( k = 0; k < vertex_count; k++ )
        {
          c[remap[k]] = mp.C[k];
        }
        for ( k = 0; k < remap_vertex_count; k++ )
          mp.C[k] = c[k];
        mesh.m_C.SetCount(remap_vertex_count);
      }
      else
        mesh.m_C.SetCount(0);

      if ( 0 != mp.K && !bIgnoreTextureCoordinates )
      {
        ON_SimpleArray<ON_SurfaceCurvature> s_array(remap_vertex_count);
        s_array.SetCount(remap_vertex_count);
        ON_SurfaceCurvature* s = s_array.Array();
        for ( k = 0; k < vertex_count; k++ )
        {
          s[remap[k]] = mp.K[k];
        }
        for ( k = 0; k < remap_vertex_count; k++ )
          mp.K[k] = s[k];
        mesh.m_K.SetCount(remap_vertex_count);
      }
      else
        mesh.m_K.SetCount(0);

      const int face_count = mesh.m_F.Count();
      ON_MeshFace* f = mesh.m_F.Array();
      int* fvi;
      for ( k = 0; k < face_count; k++ )
      {
        fvi = f[k].vi;
        fvi[0] = remap[fvi[0]];
        fvi[1] = remap[fvi[1]];
        fvi[2] = remap[fvi[2]];
        fvi[3] = remap[fvi[3]];
      }

      if (0 != NgonList())
      {
        //This mesh has an ngon list.
        //Modify the vertex indexes in the ngons as 
        //we did the faces above
        ON_MeshNgonList* ngonlist = ModifyNgonList();
        int kk, kkct = ngonlist->NgonCount();
        for (kk = 0; kk < kkct; kk++)
        {
          ON_MeshNgon* ngon = ngonlist->Ngon(kk);
          if (0 == ngon)
            continue;
          for ( k = 0; k < ngon->N; k++ )
            ngon->vi[k] = remap[ngon->vi[k]];
        }
      }

      mesh.DestroyPartition();
      mesh.DestroyTopology();

      if ( mesh.m_V.Capacity() > 4*mesh.m_V.Count() && mesh.m_V.Capacity() > 50 )
      {
        // There is lots of unused memory in the dynamic arrays.
        // Release what we can.
        mesh.Compact();
      }
      rc = true;
    }
  }
  return rc;
}



void ON_Mesh::Append( const ON_Mesh& m )
{ // appends a copy of mesh to this and updates
  const int vcount0 = VertexCount();
  const int vcount1 = m.VertexCount();
  //const int vcount = vcount0 + vcount1;
  const int fcount0 = FaceCount();
  const int fcount1 = m.FaceCount();
  const int fcount = fcount0+fcount1;
  int* vi;
  int j;

  m_top.Destroy();

  // The calls to Has*() must happen before the m_V[] and m_F[] arrays get enlarged
  // Allow the appendage of VertexNormals, TextureCoordinates, PrincipalCurvatures to empty meshes
  // by checking for 0 == vcount0 && 0 == fcount0
  const bool bHasVertexNormals = (0 == vcount0 || HasVertexNormals()) && m.HasVertexNormals();
  const bool bHasFaceNormals   = (0 == fcount0 || HasFaceNormals()) && m.HasFaceNormals();
  const bool bHasTextureCoordinates = (0 == vcount0 || HasTextureCoordinates()) && m.HasTextureCoordinates();
  const bool bHasPrincipalCurvatures = (0 == vcount0 || HasPrincipalCurvatures()) && m.HasPrincipalCurvatures();
  const bool bHasVertexColors = (0 == vcount0 || HasVertexColors()) && m.HasVertexColors();
  const bool bHasSurfaceParameters = (0 == vcount0 || HasSurfaceParameters()) && m.HasSurfaceParameters();


  m_V.Append( vcount1, m.m_V.Array() );
  m_F.Append( fcount1, m.m_F.Array() );
  for ( j = fcount0; j < fcount; j++ ) {
    vi = m_F[j].vi;
    vi[0] += vcount0;
    vi[1] += vcount0;
    vi[2] += vcount0;
    vi[3] += vcount0;
  }
  if ( bHasVertexNormals ) {
    m_N.Append(vcount1, m.m_N.Array());
  }
  else {
    m_N.Destroy();
  }
  if ( bHasFaceNormals ) {
    m_FN.Append( fcount1, m.m_FN.Array() );
  }
  else {
    m_FN.Destroy();
  }

  if ( bHasTextureCoordinates ) {
    m_T.Append( vcount1, m.m_T.Array() );
  }
  else {
    m_T.Destroy();
  }


  if ( bHasSurfaceParameters ) {
    m_S.Append( vcount1, m.m_S.Array() );
  }
  else {
    m_S.Destroy();
  }

  if ( bHasPrincipalCurvatures ) {
    m_K.Append( vcount1, m.m_K.Array() );
  }
  else {
    m_K.Destroy();
  }
  if ( bHasVertexColors ) {
    m_C.Append( vcount1, m.m_C.Array() );
  }
  else {
    m_C.Destroy();
  }


  if ( m_mesh_parameters && m.m_mesh_parameters ) {
    if ( *m_mesh_parameters != *m.m_mesh_parameters ) {
      delete m_mesh_parameters;
      m_mesh_parameters = 0;
    }
  }
  else {
    delete m_mesh_parameters;
    m_mesh_parameters = 0;
  }

  for ( j = 0; j < 4; j++ ) {
    if ( m_kstat[j] ) {
      // will be recomputed if required
      delete m_kstat[j];
      m_kstat[j] = 0;
    }
  }

  //11/06/03 TimH Join should invalidate the m_closed flag
  SetClosed(-1);
  InvalidateBoundingBoxes();
}

double ON_Mesh::Area(double* error_estimate) const
{
  ON_MassProperties mp;
  AreaMassProperties( mp, true, false, false, false );
  if ( !mp.m_bValidMass )
  {
    mp.m_mass = 0.0;
    mp.m_mass_err = 0.0;
  }
  if ( 0 != error_estimate )
    *error_estimate = mp.m_mass_err;
  return mp.m_mass;
}


double ON_Mesh::Volume( ON_3dPoint base_point, double* error_estimate ) const
{
  ON_MassProperties mp;
  VolumeMassProperties( mp, true, false, false, false, base_point );
  if ( !mp.m_bValidMass )
  {
    mp.m_mass = 0.0;
    mp.m_mass_err = 0.0;
  }
  if ( 0 != error_estimate )
    *error_estimate = mp.m_mass_err;
  return mp.m_mass;
}

ON_3dPoint ON_Mesh::AreaCentroid( 
        double* area
        ) const
{
  ON_MassProperties mp;
  AreaMassProperties( mp, true, true, false, false );
  if ( !mp.m_bValidCentroid )
  {
    mp.m_x0 = 0.0;
    mp.m_y0 = 0.0;
    mp.m_z0 = 0.0;
    mp.m_mass = 0.0;
  }
  ON_3dPoint c( mp.m_x0, mp.m_y0, mp.m_z0 );
  if ( 0 != area )
    *area = mp.m_mass;
  return c;
}


ON_3dPoint ON_Mesh::VolumeCentroid( 
        ON_3dPoint base_point,
        double* volume
        ) const
{
  ON_MassProperties mp;
  VolumeMassProperties( mp, true, true, false, false, base_point );
  if ( !mp.m_bValidCentroid )
  {
    mp.m_x0 = 0.0;
    mp.m_y0 = 0.0;
    mp.m_z0 = 0.0;
    mp.m_mass = 0.0;
  }
  ON_3dPoint c( mp.m_x0, mp.m_y0, mp.m_z0 );
  if ( 0 != volume )
    *volume = mp.m_mass;
  return c;
}


bool ON_Mesh::AreaMassProperties(
  ON_MassProperties& mp,
  bool bArea,
  bool bFirstMoments,
  bool bSecondMoments,
  bool bProductMoments
  ) const
{
  bool rc = false;
  // The _MassPropertiesMesh() function is provided by the Rhino SDK.
  if ( 0 != _MassPropertiesMesh )
  {
    int mprc = _MassPropertiesMesh( 
            *this, NULL, 2, ON_UNSET_POINT, mp, 
            bArea, bFirstMoments, bSecondMoments, bProductMoments, 
            ON_UNSET_VALUE, ON_UNSET_VALUE );
    rc = (mprc != 0);
  }
  return rc;
}


bool ON_Mesh::VolumeMassProperties(
  ON_MassProperties& mp, 
  bool bVolume,
  bool bFirstMoments,
  bool bSecondMoments,
  bool bProductMoments,
  ON_3dPoint base_point
  ) const
{
  bool rc = false;
  // The _MassPropertiesMesh() function is provided by the Rhino SDK.
  if ( 0 != _MassPropertiesMesh )
  {
    int mprc = _MassPropertiesMesh( 
            *this, NULL, 3, base_point, mp, 
            bVolume, bFirstMoments, bSecondMoments, bProductMoments, 
            ON_UNSET_VALUE, ON_UNSET_VALUE );
    rc = (mprc != 0);
  }
  return rc;
}


ON_MeshParameters::ON_MeshParameters()
{
  Default();
}

ON_MeshParameters::~ON_MeshParameters()
{
}

double ON_MeshParameters::Tolerance( double density, double actual_size )
{
  // min_rel_tol creates the most facets
  //const double min_rel_tol = 5.0e-5;

  //max rel tol creates the fewest facets
  //const double max_rel_tol = 0.1;
  //m_relative_tolerance = 0.0;
  //if ( density <= 0.0 )
  //  m_relative_tolerance = max_rel_tol;
  //else if ( density >= 1.0 )
  //  m_relative_tolerance = min_rel_tol;
  //else
  //{
  //  ON_Interval e(log10(max_rel_tol),log10(min_rel_tol));
  //  m_relative_tolerance = pow(10.0,e.ParameterAt(density));
  //}

  double tol = 0.0;
  double x, e;
  if ( ON_IsValid(density) && ON_IsValid(actual_size)
       && density > 0.0 && actual_size > 0.0 )
  {
    if ( density > 1.0 )
      density = 1.0;

    //e = (density*(12.0 + density*(2.0*density - 9.0)));
    //e = (1.0 + density*(8.0 - 4.0*density));
    //e = 1.0 + density*4.0;
    //x = 5.0*pow(10.0,-e);

    e = (density < 0.5) 
      ? 1.0 + density*(6.0 - 4.0*density) // 1.0 + 4.0*density
      : 2.0 + 2.0*density;
    x = pow(10.0,-e);

    tol = actual_size*x;
  }
  return tol;
}

void ON_MeshParameters::JaggedAndFasterMeshParameters()
{
  // If you change these, put your name, the date, and the reasons for
  // the change in this comment so we can keep track of what we are
  // trying to accomplish;
  Default();

  bool bRelTolTest = true;

  if ( bRelTolTest )
  {
    // Added 27 April 2006 for the Meshkateers
    //   Attempting to make jagged and faster render meshes a little
    //   more dense.
    //
    // Turn off everything ...
    m_bComputeCurvature = false;
    m_tolerance = 0.0;
    m_bJaggedSeams  = false;
    m_max_edge_length = 0.0;
    m_grid_aspect_ratio = 0.0;
    m_grid_max_count = 0;
    m_grid_angle = 0.0;
    m_grid_amplification = 0.0;
    m_refine_angle = 0.0;

    // ... except ...
    // The m_relative_tolerance setting must be set so that
    // 0.0005 = ON_MeshParameters::Tolerance(m_relative_tolerance,1.0).
    m_relative_tolerance = 0.65;
    //double x = Tolerance(m_relative_tolerance,1.0);

    m_grid_min_count     = 16;
    m_min_edge_length    = 0.0001;
    m_bRefine            = true;
    m_bSimplePlanes      = true;

    m_texture_range = 2; // Don't change this without speaking to Dale Lear
  }
  else
  {
    // 16 July, 2002 - copied V2 hard coded "jagged and faster" render mesh settings
    //
    // Settings used in V2, V3 and early V4 beta
    m_refine_angle       = 20.0*ON_PI/180.0;
    m_grid_angle         = 20.0*ON_PI/180.0;
    m_grid_aspect_ratio  = 0.0;
    m_min_edge_length    = 0.0001;
    m_max_edge_length    = 0.0;
    m_tolerance          = 0.0;
    m_grid_min_count     = 16;
    m_bRefine            = 1;
    m_bJaggedSeams       = 0;
    m_bSimplePlanes      = 0;
  }

  return;
}

void ON_MeshParameters::SmoothAndSlowerMeshParameters()
{
  // If you change these, put your name, the date, and the reasons for
  // the change in this comment so we can keep track of what we are
  // trying to accomplish;
  Default();
  
  bool bRelTolTest = true;

  if ( bRelTolTest )
  {
    // Added 27 April 2006 for the Meshkateers
    //   Attempting to make amooth and slower render meshing a little
    //   faster.
    //
    // Turn off everything ...
    m_bComputeCurvature = false;
    m_tolerance = 0.0;
    m_bJaggedSeams  = false;
    m_max_edge_length = 0.0;
    m_grid_aspect_ratio = 0.0;
    m_grid_max_count = 0;
    m_grid_angle = 0.0;
    m_grid_amplification = 0.0;

    // ... except ...
    // The m_relative_tolerance setting must be set so that
    // 0.00025 = ON_MeshParameters::Tolerance(m_relative_tolerance,1.0).
    m_relative_tolerance = 0.8;
    //double x = Tolerance(m_relative_tolerance,1.0);

    m_grid_min_count     = 16;
    m_min_edge_length    = 0.0001;
    m_bRefine            = true;
    m_bSimplePlanes      = true;
    m_refine_angle       = 20.0*ON_PI/180.0;

    m_texture_range = 2; // Don't change this without speaking to Dale Lear
  }
  else
  {
    // 16 July, 2002 - copied V2 hard coded "smooth and slower" render mesh settings
    //
    // Settings used in V2, V3 and early V4 beta
    m_refine_angle       = 15.0*ON_PI/180.0;
    m_grid_angle         = 15.0*ON_PI/180.0;
    m_grid_aspect_ratio  = 6.0;
    m_min_edge_length    = 0.0001;
    m_max_edge_length    = 0.0;
    m_tolerance          = 0.0;
    m_grid_min_count     = 16;
    m_bRefine            = 1;
    m_bJaggedSeams       = 0;
    m_bSimplePlanes      = 0;
  }

  return;
}

void ON_MeshParameters::DefaultAnalysisMeshParameters()
{
  Default();

  // 7 July 2006 Dale Lear
  //    I added this line for RR 10330
  Set( 0.5, m_min_edge_length );

  m_texture_range = 1; // THIS IS REQUIRED - DO NOT CHANGE IT

  // Meshkateers 
  //   Add your stuff below this line.
  //   Do not change m_texture_range.
}

bool ON_MeshParameters::operator==(const ON_MeshParameters& m2) const
{
  return (Compare(m2)==0) ? true : false;
}

bool ON_MeshParameters::operator!=(const ON_MeshParameters& m2) const
{
  return (Compare(m2)) ? true : false;
}

bool ON_MeshParameters::operator==(const ON_Mesh& mesh) const
{
  const ON_MeshParameters* mp1 = mesh.MeshParameters();
  return mp1 ? (*this == *mp1) : false;
}

bool ON_MeshParameters::operator!=(const ON_Mesh& mesh) const
{
  const ON_MeshParameters* mp1 = mesh.MeshParameters();
  return mp1 ? (*this != *mp1) : true;
}

void ON_MeshParameters::Dump( ON_TextLog& text_log ) const
{
  text_log.Print("Gridding:\n");
  text_log.PushIndent();
  text_log.Print("Min grid count = %d\n",m_grid_min_count);
  text_log.Print("Max grid count = %d\n",m_grid_max_count);
  text_log.Print("Gridding angle = %g radians (%g degrees)\n",m_grid_angle,180.0*m_grid_angle/ON_PI);
  text_log.Print("Aspect ratio = %g\n",m_grid_aspect_ratio);
  text_log.Print("Amplification = %g\n",m_grid_amplification);
  text_log.PopIndent();

  text_log.Print("Refining:\n");
  text_log.PushIndent();
  text_log.Print("Refine = %s\n",m_bRefine?"true":"false");
  text_log.Print("Refine angle = %g radians (%g degrees)\n",m_refine_angle,180.0*m_refine_angle/ON_PI);
  text_log.PopIndent();

  text_log.Print("Metrics:\n");
  text_log.PushIndent();
  text_log.Print("Density = %g (relative tolerance = %g)\n",m_relative_tolerance,ON_MeshParameters::Tolerance(m_relative_tolerance,1.0));
  text_log.Print("Minimum tolerance = %g\n",m_min_tolerance);
  text_log.Print("Tolerance = %g\n",m_tolerance);
  text_log.Print("Min edge length = %g\n",m_min_edge_length);
  text_log.Print("Max edge length = %g\n",m_max_edge_length);
  text_log.PopIndent();

  text_log.Print("Misceleanous:\n");
  text_log.PushIndent();
  text_log.Print("Face type = %d\n",m_face_type );
  text_log.Print("Compute curvature = %s\n",m_bComputeCurvature?"true":"false");
  text_log.Print("Texture range = %d\n",m_texture_range);
  text_log.Print("Simple planes = %s\n",m_bSimplePlanes?"true":"false");
  text_log.Print("Jagged Seams = %s\n",m_bJaggedSeams?"true":"false");
  text_log.Print("Custom settings = %s\n",m_bCustomSettings?"true":"false");
  text_log.PopIndent();
}

void ON_MeshParameters::Default()
{
  memset(this,0,sizeof(*this));

  m_bCustomSettings = false;
  m_bComputeCurvature = false;
  m_bSimplePlanes = false;
  m_bRefine = true;
  m_bJaggedSeams = false;
  m_reserved1 = 0;
  m_reserved2 = 0;
  m_mesher = 0;
  m_texture_range = 2; // packed normalized textures
  m_tolerance = 0.0;
  m_relative_tolerance = 0.0;
  m_min_tolerance = 0.0;
  m_min_edge_length = 0.0001;
  m_max_edge_length = 0.0;
  m_grid_aspect_ratio = 6.0;
  m_grid_min_count = 0; // 16;
  m_grid_max_count = 0;
  m_grid_angle = 20.0*ON_PI/180.0;
  m_grid_amplification = 1.0;
  m_refine_angle = 20.0*ON_PI/180.0;
  m_face_type = 0;
}

void ON_MeshParameters::Set( double density, double min_edge_length )
{
  bool bRelTolTest = true;

  Default();

  // These 4 settings should not be changed
  m_bComputeCurvature = false;
  m_min_edge_length = min_edge_length;
  m_texture_range = 0;
  m_tolerance = 0.0;

  if ( bRelTolTest )
  {
    // Added 27 April 2005
    m_relative_tolerance = density;

    // The other meshing parameters are disabled.
    //
    //   Dale Lear's remark:
    //     I think it is a mistake to leave m_grid_aspect_ratio = 0
    //     at all settings.  This will result in long skinny triangles
    //     in areas where the principal curvatures are dramatically
    //     different.  
    //
    m_bRefine = (density < 0.65);
    m_bSimplePlanes = (density <= 0.0);
    m_bJaggedSeams  = false;
    m_max_edge_length = 0.0;
    m_grid_aspect_ratio = 0.0;
    m_grid_min_count = 0;
    m_grid_max_count = 0;
    m_grid_angle = 0.0;
    m_grid_amplification = 0.0;
    m_refine_angle = 0.0;
  }
  else
  {
    // Used in V3 and prior to 27 April 2005
    double t0, t1;

    m_bRefine = true;
    m_bSimplePlanes = false;
    m_bJaggedSeams  = false;
    m_max_edge_length = 0.0;
    m_grid_aspect_ratio = 0.0;
    m_grid_min_count = 16;
    m_grid_max_count = 0;
    m_grid_angle = 20.0*ON_PI/180.0;
    m_grid_amplification = 1.0;
    m_refine_angle = 20.0*ON_PI/180.0;
 
    if ( density <= 0.0) 
    {
      m_bSimplePlanes = true;
      m_bRefine       = false;

      m_grid_min_count     = 0;
      m_grid_aspect_ratio  = 0.0;
      m_grid_angle         = 0.0;
      m_grid_amplification = 1.0;

      m_refine_angle       = m_grid_angle;
    }
    else if ( density < 0.5)
    {
      t1 = 2.0*density;
      t0 = 1.0-t1;
      
      m_bRefine = density < 0.10 ? false : true;

      m_grid_min_count = 0;
      m_grid_aspect_ratio  = 6.0;
      m_grid_angle         = 20.0*ON_PI/180.0;
      m_grid_amplification = (t1 > 0.0 ) ? t1 : 1.0;

      m_refine_angle = (t0*65.0 + t1*20.0)*ON_PI/180.0;
    }
    else if ( density == 0.5 ) 
    {
      m_bRefine       = true;

      m_grid_min_count     = 0;
      m_grid_aspect_ratio  = 6.0;
      m_grid_angle         = 20.0*ON_PI/180.0;
      m_grid_amplification = 1.0;

      m_refine_angle        = m_grid_angle;
    }
    else
    {
      t1 = 2.0*(density - 0.5);
      if ( t1 > 1.0 )
        t1 = 1.0;
      t0 = 1.0-t1;
      
      m_bRefine = true;

      m_grid_min_count     = 0;
      m_grid_aspect_ratio  = t0*6.0 + t1;
      m_grid_angle         = (t0*20.0 + t1*5.0)*ON_PI/180.0;
      m_grid_amplification = 1.0 + t1;

      m_refine_angle       = m_grid_angle;
    }
  }
}


int ON_MeshParameters::Compare( const ON_MeshParameters& src ) const
{
  // Discuss any changes with Dale Lear.

  if ( !m_bCustomSettings && src.m_bCustomSettings )
    return -1;
  if ( m_bCustomSettings && !src.m_bCustomSettings )
    return 1;
  if ( !m_bSimplePlanes && src.m_bSimplePlanes )
    return -1;
  if ( m_bSimplePlanes && !src.m_bSimplePlanes )
    return 1;
  if ( !m_bRefine && src.m_bRefine )
    return -1;
  if ( m_bRefine && !src.m_bRefine )
    return 1;
  if ( m_bJaggedSeams && !src.m_bJaggedSeams )
    return -1;
  if ( !m_bJaggedSeams && src.m_bJaggedSeams )
    return 1;
  if ( m_mesher < src.m_mesher )
    return -1;
  if ( m_mesher > src.m_mesher )
    return 1;
  if ( m_texture_range < src.m_texture_range )
    return -1;
  if ( m_texture_range > src.m_texture_range )
    return 1;

  if ( m_tolerance <= 0.0 && src.m_tolerance > 0.0 )
    return -1;
  if ( m_tolerance > 0.0 && src.m_tolerance <= 0.0 )
    return 1;
  if ( 0.0 < m_tolerance && m_tolerance < src.m_tolerance )
    return 1;
  if ( 0.0 < src.m_tolerance && src.m_tolerance < m_tolerance )
    return -1;

  if ( m_relative_tolerance <= 0.0 && src.m_relative_tolerance > 0.0 )
    return -1;
  if ( m_relative_tolerance > 0.0 && src.m_relative_tolerance <= 0.0 )
    return 1;
  if ( 0.0 < m_relative_tolerance && m_relative_tolerance < src.m_relative_tolerance )
    return 1;
  if ( 0.0 < src.m_relative_tolerance && src.m_relative_tolerance < m_relative_tolerance )
    return -1;

  // DO NOT COMPARE m_min_tolerance - it is a runtime lower bound clamp.
  //                If it is included here, Rhino will remesh everytime
  //                model tolerance changes.

  if ( m_min_edge_length <= 0.0 && src.m_min_edge_length > 0.0 )
    return 1;
  if ( m_min_edge_length > 0.0 && src.m_min_edge_length <= 0.0 )
    return -1;
  if ( 0.0 < m_min_edge_length && m_min_edge_length < src.m_min_edge_length )
    return 1;
  if ( 0.0 < src.m_min_edge_length && src.m_min_edge_length < m_min_edge_length )
    return 1;

  if ( m_max_edge_length <= 0.0 && src.m_max_edge_length > 0.0 )
    return -1;
  if ( m_max_edge_length > 0.0 && src.m_max_edge_length <= 0.0 )
    return 1;
  if ( 0.0 < m_max_edge_length && m_max_edge_length < src.m_max_edge_length )
    return 1;
  if ( 0.0 < src.m_max_edge_length && src.m_max_edge_length < m_max_edge_length )
    return -1;

  if ( m_grid_aspect_ratio <= 0.0 && src.m_grid_aspect_ratio > 0.0 )
    return -1;
  if ( m_grid_aspect_ratio > 0.0 && src.m_grid_aspect_ratio <= 0.0 )
    return 1;
  if ( 0.0 < m_grid_aspect_ratio && m_grid_aspect_ratio < src.m_grid_aspect_ratio )
    return 1;
  if ( 0.0 < src.m_grid_aspect_ratio && src.m_grid_aspect_ratio < m_grid_aspect_ratio )
    return -1;

  if ( m_grid_min_count <= 0 && src.m_grid_min_count > 0 )
    return -1;
  if ( m_grid_min_count > 0 && src.m_grid_min_count <= 0 )
    return 1;
  if ( 0 < m_grid_min_count && m_grid_min_count < src.m_grid_min_count )
    return 1;
  if ( 0 < src.m_grid_min_count && src.m_grid_min_count < m_grid_min_count )
    return -1;

  if ( m_grid_max_count <= 0 && src.m_grid_max_count > 0 )
    return 1;
  if ( m_grid_max_count > 0 && src.m_grid_max_count <= 0 )
    return -1;
  if ( 0 < m_grid_max_count && m_grid_max_count < src.m_grid_max_count )
    return -1;
  if ( 0 < src.m_grid_max_count && src.m_grid_max_count < m_grid_max_count )
    return 1;

  if ( m_grid_angle <= 0.0 && src.m_grid_angle > 0.0 )
    return -1;
  if ( m_grid_angle > 0.0 && src.m_grid_angle <= 0.0 )
    return 1;
  if ( 0.0 < m_grid_angle && m_grid_angle < src.m_grid_angle )
    return 1;
  if ( 0.0 < src.m_grid_angle && src.m_grid_angle < m_grid_angle )
    return -1;

  if ( m_refine_angle <= 0.0 && src.m_refine_angle > 0.0 )
    return -1;
  if ( m_refine_angle > 0.0 && src.m_refine_angle <= 0.0 )
    return 1;
  if ( 0.0 < m_refine_angle && m_refine_angle < src.m_refine_angle )
    return 1;
  if ( 0.0 < src.m_refine_angle && src.m_refine_angle < m_refine_angle )
    return -1;

  double g0 = ( m_grid_amplification > 0.0 ) ? m_grid_amplification : 1.0;
  double g1 = ( src.m_grid_amplification > 0.0 ) ? src.m_grid_amplification : 1.0;
  if ( g0 < g1 )
    return -1;
  if ( g0 > g1 )
    return 1;

  if ( m_face_type < src.m_face_type )
    return -1;
  if ( m_face_type > src.m_face_type )
    return 1;

  if ( m_relative_tolerance <= 0.0 && src.m_relative_tolerance > 0.0 )
    return -1;
  if ( m_relative_tolerance > 0.0 && src.m_relative_tolerance <= 0.0 )
    return 1;
  if ( 0.0 < m_relative_tolerance && m_relative_tolerance < src.m_relative_tolerance )
    return 1;
  if ( 0.0 < src.m_relative_tolerance && src.m_relative_tolerance < m_relative_tolerance )
    return -1;

  return 0;
}

bool ON_MeshParameters::Write( ON_BinaryArchive& file ) const
{
  bool rc = file.Write3dmChunkVersion(1,3);
  if (rc) {
    if (rc) rc = file.WriteInt(m_bComputeCurvature);
    if (rc) rc = file.WriteInt(m_bSimplePlanes);
    if (rc) rc = file.WriteInt(m_bRefine);
    if (rc) rc = file.WriteInt(m_bJaggedSeams);
    if (rc) rc = file.WriteInt(0); // obsolete m_bWeld field
    if (rc) rc = file.WriteDouble(m_tolerance);
    if (rc) rc = file.WriteDouble(m_min_edge_length);
    if (rc) rc = file.WriteDouble(m_max_edge_length);
    if (rc) rc = file.WriteDouble(m_grid_aspect_ratio);
    if (rc) rc = file.WriteInt(m_grid_min_count);
    if (rc) rc = file.WriteInt(m_grid_max_count);
    if (rc) rc = file.WriteDouble(m_grid_angle);
    if (rc) rc = file.WriteDouble(m_grid_amplification);
    if (rc) rc = file.WriteDouble(m_refine_angle);
    if (rc) rc = file.WriteDouble(5.0*ON_PI/180.0); // obsolete m_combine_angle field
    int mft = m_face_type;
    if ( mft < 0 || mft > 2 ) 
    {
      ON_ERROR("ON_MeshParameters::Read() - m_face_type out of bounds.");
      mft = 0;
    }
    if (rc) rc = file.WriteInt(mft);

    // added for chunk version 1.1
    if (rc) rc = file.WriteInt( m_texture_range );

    // added for chunk version 1.2 - 14 October 2005
    if (rc) rc = file.WriteBool( m_bCustomSettings );
    if (rc) rc = file.WriteDouble( m_relative_tolerance );
    // DO NOT SAVE m_min_tolerance - yet ??

    // added for chunk version 1.3 - 20 February 2006
    if (rc) rc = file.WriteChar(m_mesher);
  }
  return rc;
}

bool ON_MeshParameters::Read( ON_BinaryArchive& file )
{
  Default();
  int major_version = 0;
  int minor_version = 0;
  bool rc = file.Read3dmChunkVersion(&major_version,&minor_version);
  if ( rc && major_version == 1 ) 
  {
    int i;

    i = m_bComputeCurvature;
    if (rc) rc = file.ReadInt(&i);
    m_bComputeCurvature = i?true:false;

    i = m_bSimplePlanes;
    if (rc) rc = file.ReadInt(&i);
    m_bSimplePlanes = i?true:false;

    i = m_bRefine;
    if (rc) rc = file.ReadInt(&i);
    m_bRefine = i?true:false;

    i = m_bJaggedSeams;
    if (rc) rc = file.ReadInt(&i);
    m_bJaggedSeams = i?true:false;

    int obsolete_m_bWeld;
    if (rc) rc = file.ReadInt(&obsolete_m_bWeld);

    if (rc) rc = file.ReadDouble(&m_tolerance);
    if (rc) rc = file.ReadDouble(&m_min_edge_length);
    if (rc) rc = file.ReadDouble(&m_max_edge_length);
    if (rc) rc = file.ReadDouble(&m_grid_aspect_ratio);
    if (rc) rc = file.ReadInt(&m_grid_min_count);
    if (rc) rc = file.ReadInt(&m_grid_max_count);
    if (rc) rc = file.ReadDouble(&m_grid_angle);
    if (rc) rc = file.ReadDouble(&m_grid_amplification);
    if (rc) rc = file.ReadDouble(&m_refine_angle);
    double obsolete_m_combine_angle;
    if (rc) rc = file.ReadDouble(&obsolete_m_combine_angle);
    if (rc) rc = file.ReadInt(&m_face_type);
    if ( m_face_type < 0 || m_face_type > 2 ) 
    {
      ON_ERROR("ON_MeshParameters::Read() - m_face_type out of bounds.");
      m_face_type = 0;
    }

    if ( rc && minor_version >= 1 ) 
    {
      rc = file.ReadInt( &m_texture_range );
      if ( rc && minor_version >= 2 )
      {
        rc = file.ReadBool(&m_bCustomSettings);
        if (rc) rc = file.ReadDouble(&m_relative_tolerance);
        if ( rc && minor_version >= 3 )
        {
          rc = file.ReadChar(&m_mesher);
        }
      }
    }
  }
  return rc;
}



ON_MeshCurvatureStats::ON_MeshCurvatureStats()
{
  Destroy(); // initializes defaults
}

ON_MeshCurvatureStats::~ON_MeshCurvatureStats()
{}

void ON_MeshCurvatureStats::Destroy()
{
  m_style = ON::unknown_curvature_style;
  m_infinity = 0.0;
  m_count_infinite = 0;
  m_count = 0;
  m_mode = 0.0;
  m_average = 0.0;
  m_adev = 0.0;
  m_range.Set(0.0,0.0);
}

void ON_MeshCurvatureStats::EmergencyDestroy()
{
  Destroy(); // - no memory is freed so Destroy() will work
}

ON_MeshCurvatureStats::ON_MeshCurvatureStats(const ON_MeshCurvatureStats& src)
{
  *this = src;
}

ON_MeshCurvatureStats& ON_MeshCurvatureStats::operator=(const ON_MeshCurvatureStats& src)
{
  if ( this != &src ) {
    m_style          = src.m_style;
    m_infinity       = src.m_infinity;
    m_count_infinite = src.m_count_infinite;
    m_count          = src.m_count;
    m_mode           = src.m_mode;
    m_average        = src.m_average;
    m_adev           = src.m_adev;
    m_range          = src.m_range;
  }
  return *this;
}

bool ON_MeshCurvatureStats::Write( ON_BinaryArchive& file ) const
{
  int i;
  bool rc = file.Write3dmChunkVersion(1,1);
  if (rc) {
    i = m_style;
    if (rc) rc = file.WriteInt(i);
    if (rc) rc = file.WriteDouble(m_infinity);
    if (rc) rc = file.WriteInt(m_count_infinite);
    if (rc) rc = file.WriteInt(m_count);
    if (rc) rc = file.WriteDouble(m_mode);
    if (rc) rc = file.WriteDouble(m_average);
    if (rc) rc = file.WriteDouble(m_adev);
    if (rc) rc = file.WriteInterval(m_range);
  }
  return rc;
}


bool ON_MeshCurvatureStats::Read( ON_BinaryArchive& file )
{
  int major_version = 0;
  int minor_version = 0;
  Destroy();
  bool rc = file.Read3dmChunkVersion(&major_version,&minor_version);
  if (rc && major_version == 1) {
    int i=0;
    if (rc) rc = file.ReadInt(&i);
    if (rc) m_style = ON::CurvatureStyle(i);
    if (rc) rc = file.ReadDouble(&m_infinity);
    if (rc) rc = file.ReadInt(&m_count_infinite);
    if (rc) rc = file.ReadInt(&m_count);
    if (rc) rc = file.ReadDouble(&m_mode);
    if (rc) rc = file.ReadDouble(&m_average);
    if (rc) rc = file.ReadDouble(&m_adev);
    if (rc) rc = file.ReadInterval(m_range);
  }
  return rc;
}

bool ON_MeshCurvatureStats::Set( ON::curvature_style kappa_style,
                                 int Kcount,
                                 const ON_SurfaceCurvature* K,
                                 const ON_3fVector* N, // needed for normal sectional curvatures
                                 double infinity
                                 )
{
  bool rc = (Kcount > 0 && K != NULL);

  Destroy();

  if (rc) {
    ON_Workspace ws;
    //ON_3dVector tangent;
    double k;
    double* kappa = ws.GetDoubleMemory(Kcount);
    int i;

    switch( kappa_style ) {
    case ON::gaussian_curvature:
      m_style = kappa_style;
      m_infinity = ( infinity > 0.0 ) ? infinity : 1.0e20;
      break;
    case ON::mean_curvature: // unsigned mean
      m_style = kappa_style;
      m_infinity = ( infinity > 0.0 ) ? infinity : 1.0e10;
      break;
    case ON::min_curvature: // minimum unsigned radius of curvature
      m_style = kappa_style;
      m_infinity = ( infinity > 0.0 ) ? infinity : 1.0e10;
      break;
    case ON::max_curvature: // maximum unsigned radius of curvature
      m_style = kappa_style;
      m_infinity = ( infinity > 0.0 ) ? infinity : 1.0e10;
      break;
    //case ON::section_curvature_x:
    //  if ( !N )
    //    kappa_style = ON::unknown_curvature_style;
    //  m_style = kappa_style;
    //  m_infinity = ( infinity > 0.0 ) ? infinity : 1.0e10;
    //  break;
    //case ON::section_curvature_y:
    //  if ( !N )
    //    kappa_style = ON::unknown_curvature_style;
    //  m_style = kappa_style;
    //  m_infinity = ( infinity > 0.0 ) ? infinity : 1.0e10;
    //  break;
    //case ON::section_curvature_z:
    //  if ( !N )
    //    kappa_style = ON::unknown_curvature_style;
    //  m_style = kappa_style;
    //  m_infinity = ( infinity > 0.0 ) ? infinity : 1.0e10;
    //  break;
    default:
      rc = false;
      break;
    }

    for ( i = 0; i < Kcount; i++ ) {
      switch( kappa_style ) {
      case ON::gaussian_curvature:
        k = K[i].GaussianCurvature();
        break;
      case ON::mean_curvature: // unsigned mean
        k = fabs(K[i].MeanCurvature());
        break;
      case ON::min_curvature: // minimum unsigned radius of curvature
        k = fabs(K[i].MinimumRadius());
        break;
      case ON::max_curvature: // maximum unsigned radius of curvature
        k = fabs(K[i].MaximumRadius());
        break;
      //case ON::section_curvature_x:
      //  tangent.x = 0.0; tangent.y = -N[i].z; tangent.z = N[i].y;
      //  if ( fabs(tangent.y) <= ON_SQRT_EPSILON && fabs(tangent.z) <= ON_SQRT_EPSILON )
      //    tangent.Zero();
      //  else
      //    tangent.Unitize();
      //  k = fabs(K[i].NormalCurvature(tangent));
      //  break;
      //case ON::section_curvature_y:
      //  tangent.x = N[i].z; tangent.y = 0.0; tangent.z = -N[i].x;
      //  if ( fabs(tangent.x) <= ON_SQRT_EPSILON && fabs(tangent.z) <= ON_SQRT_EPSILON )
      //    tangent.Zero();
      //  else
      //    tangent.Unitize();
      //  k = fabs(K[i].NormalCurvature(tangent));
      //  break;
      //case ON::section_curvature_z:
      //  tangent.x = -N[i].y; tangent.y = N[i].x; tangent.z = 0.0;
      //  if ( fabs(tangent.x) <= ON_SQRT_EPSILON && fabs(tangent.y) <= ON_SQRT_EPSILON )
      //    tangent.Zero();
      //  else
      //    tangent.Unitize();
      //  k = fabs(K[i].NormalCurvature(tangent));
      //  break;
      default:
        k=0.0;
        break;
      }
      if ( fabs(k) >= m_infinity ) {
        m_count_infinite++;
        continue;
      }
      if ( m_count ) {
        if ( k < m_range.m_t[0] )
          m_range.m_t[0] = k;
        else if ( k > m_range.m_t[1] )
          m_range.m_t[1] = k;
      }
      else {
        m_range.m_t[0] = m_range.m_t[1] = k;
      }
      kappa[m_count++] = k;
    }


    if ( m_count == 0 )
      rc = false;
    else {
      // sum curvatures
      ON_SortDoubleArray( ON::quick_sort, kappa, m_count );

      // mode
      m_mode = kappa[m_count/2];
      if ( 0 == (m_count % 2) ) {
        m_mode += kappa[(m_count/2)-1];
        m_mode *= 0.5;
      }

      // average
      for ( i = 0; i < m_count; i++ ) {
        m_average += kappa[i];
      }
      m_average = m_average/m_count;
      
      // average deviation
      for ( i = 0; i < m_count; i++ ) {
        m_adev += fabs(kappa[i] - m_average);
      }
      m_adev = m_adev/m_count;
    }  
  }
  
  return rc;  
}

struct EDGEINFO
{
  int fi[2]; // m_F[] triangles on either side of the edge
  int vi[4]; // potential m_V[] quad indices
  int flag; // 0 = available, 
            // 1 = side of quad, 
            // 2 = bndry or nonmanifold edge,
            // 3 = edge crease angle > angle_tol_radians
            // 4 = edge is not longest side of triangle
            // 5 = edge length is zero
            // 6 = edge would be a boundary if mesh were exploded
            // 16 = edge will be removed to make a quad
  double length;
};

bool ON_Mesh::ConvertTrianglesToQuads( 
              double angle_tol_radians,
              double min_diagonal_length_ratio
              )
{
  ON_Workspace ws;

  double d;

  int i, ii, jj;
  int diagonal_count;
  const int* f0vi;
  const int* f1vi;
  const int* fei;

  diagonal_count = 0;

  if ( angle_tol_radians < 0.0 || !ON_IsValid(angle_tol_radians) )
  {
    // 2.5 degrees
    angle_tol_radians = 0.043633231299858239423092269212215;
  }
  else if ( angle_tol_radians < ON_ZERO_TOLERANCE )
  {
    angle_tol_radians = ON_ZERO_TOLERANCE;
  }

  angle_tol_radians = cos(angle_tol_radians);
  if ( angle_tol_radians < 0.5 )
    angle_tol_radians = 0.5;
  else if ( angle_tol_radians > 1.0-ON_SQRT_EPSILON )
    angle_tol_radians = 1.0-ON_SQRT_EPSILON;

  const ON_MeshTopology& top = Topology();

  if ( !HasFaceNormals() )
    ComputeFaceNormals();

  if ( min_diagonal_length_ratio < ON_ZERO_TOLERANCE )
    min_diagonal_length_ratio = ON_ZERO_TOLERANCE;

  double max_diagonal_length_ratio = 1.0/min_diagonal_length_ratio;

  if( min_diagonal_length_ratio > max_diagonal_length_ratio )
  {
    d = min_diagonal_length_ratio;
    min_diagonal_length_ratio = max_diagonal_length_ratio;
    max_diagonal_length_ratio = d;
  }

  //double rel_tol = ON_SQRT_EPSILON;
  double rel_tol = 8.0*ON_SQRT_EPSILON; // to fix RR 51543
  if ( min_diagonal_length_ratio > 1.0-rel_tol )
    min_diagonal_length_ratio = 1.0-rel_tol;
  if ( max_diagonal_length_ratio < 1.0+rel_tol )
    max_diagonal_length_ratio = 1.0+rel_tol;


  const int face_count = m_F.Count();
  int* face_flag = ws.GetIntMemory(face_count);  
  for ( i = 0; i < face_count; i++ )
  {
    f0vi = m_F[i].vi;
    face_flag[i] = ( f0vi[2] == f0vi[3] ) ? 0 : 1;
  }

  const int edge_count = top.m_tope.Count();
  struct EDGEINFO* EI = (struct EDGEINFO*)ws.GetMemory(edge_count*sizeof(*EI));
  for ( i = 0; i < edge_count; i++ )
  {
    struct EDGEINFO& ei = EI[i];
    const ON_MeshTopologyEdge& tope = top.m_tope[i];
    ei.flag = 0;
    ei.vi[0] = top.m_topv[tope.m_topvi[0]].m_vi[0];
    ei.vi[2] = top.m_topv[tope.m_topvi[1]].m_vi[0];
    ei.length = m_V[ei.vi[0]].DistanceTo(m_V[ei.vi[2]]);
    if ( ei.length <= 0.0 || !ON_IsValid(ei.length) )
    {
      ei.flag = 5;
    }
    else if ( tope.m_topf_count != 2 )
    {
      ei.flag = 2;
    }
    else
    {
      ei.fi[0] = tope.m_topfi[0];
      ei.fi[1] = tope.m_topfi[1];
      if (face_flag[ei.fi[0]] || face_flag[ei.fi[1]] )
      {
        ei.flag = 1;
      }
      else if ( m_FN[ei.fi[0]] * m_FN[ei.fi[1]] < angle_tol_radians )
      {
        ei.flag = 3;
      }
      else 
      {
        f0vi = m_F[ei.fi[0]].vi;
        f1vi = m_F[ei.fi[1]].vi;
        ei.flag = 6;
        for ( ii = 0; ii < 3 && ei.flag; ii++ )
        {
          for (jj = 0; jj < 3; jj++)
          {
            if (    f0vi[ii] == f1vi[jj] 
                 && f0vi[(ii+1)%3] == f1vi[(jj+2)%3]
                 && f0vi[(ii+2)%3] != f1vi[(jj+1)%3] )
            {
              if ( ei.fi[0] > ei.fi[1] )
              {
                jj = ei.fi[0]; ei.fi[0] = ei.fi[1]; ei.fi[1] = jj;
              }
              ei.vi[0] = f0vi[ii];
              ei.vi[1] = f1vi[(jj+1)%3];
              ei.vi[2] = f0vi[(ii+1)%3];
              ei.vi[3] = f0vi[(ii+2)%3];
              ei.flag = 0;
              break;
            }
          }
        }
      }
    }
  }

  for ( i = 0; i < edge_count; i++ )
  {
    struct EDGEINFO& ei = EI[i];
    if ( ei.flag )
      continue;

    ei.flag = 16;

    // It is CRITICAL that the length compare use >=.
    // Otherwise tesselations of equailateral triangles
    // will not work right in this fuction.
    fei = top.m_topf[ei.fi[0]].m_topei;
    if (    ( i != fei[0] && EI[fei[0]].length >= ei.length )
         || ( i != fei[1] && EI[fei[1]].length >= ei.length )
         || ( i != fei[2] && EI[fei[2]].length >= ei.length )
         )
    {
      // diagonal is not strictly longest in this triangle
      continue;
    }

    // It is CRITICAL that the length compare use >=.
    // Otherwise tesselations of equailateral triangles
    // will not work right in this fuction.
    fei = top.m_topf[ei.fi[1]].m_topei;
    if (    ( i != fei[0] && EI[fei[0]].length >= ei.length )
         || ( i != fei[1] && EI[fei[1]].length >= ei.length )
         || ( i != fei[2] && EI[fei[2]].length >= ei.length )
         )
    {
      // diagonal is not strictly longest in this triangle
      continue;
    }

    d = m_V[ei.vi[1]].DistanceTo(m_V[ei.vi[3]]);
    d /= ei.length;
    if ( d < min_diagonal_length_ratio || d > max_diagonal_length_ratio )
    {
      // quad wouldn't be square enough
      continue;
    }

    ei.flag = 0;

    diagonal_count++;
  }

  if ( diagonal_count > 0 )
  {
    DestroyTree();
    DestroyPartition();
    m_top.Destroy();
    for ( i = 0; i < edge_count; i++ )
    {
      struct EDGEINFO& ei = EI[i];
      if ( ei.flag )
        continue;
      memcpy( m_F[ei.fi[0]].vi, ei.vi, 4*sizeof(ei.vi[0]) );
      memset( m_F[ei.fi[1]].vi, 0xFF,  4*sizeof(ei.vi[0]) );
      m_triangle_count--;
      m_quad_count++;
    }    
    CullDegenerateFaces();
  }

  return (diagonal_count > 0 );
}

bool ON_Mesh::ConvertQuadsToTriangles()
{
  const bool bHasFaceNormals = HasFaceNormals();
  const int fcount = FaceCount();
  const int vcount = VertexCount();
  int fi, idmin;;
  double d0, d1, dmin, d;
  if ( fcount > 0 && QuadCount() > 0 ) 
  {
    // use SetCapacity() instead of Reserve() because it's unlikely
    // this mesh will have anything else added and we don't want to
    // waste memory.
    if ( m_F.Capacity() < fcount + m_quad_count )
      m_F.SetCapacity( fcount + m_quad_count );
    if ( bHasFaceNormals && m_FN.Capacity() < fcount + m_quad_count )
      m_FN.SetCapacity( fcount + m_quad_count );

    for ( fi = 0; fi < fcount; fi++ ) 
    {
      ON_MeshFace& f0 = m_F[fi];
      if ( f0.IsValid(vcount) && f0.IsQuad() )
      {
        m_quad_count--;
        m_triangle_count++;
        d0 = m_V[f0.vi[0]].DistanceTo(m_V[f0.vi[2]]);
        d1 = m_V[f0.vi[1]].DistanceTo(m_V[f0.vi[3]]);

        // if quad is degenerate, just turn it into a triangle
        idmin = -1;
        dmin = ((d0<=d1)?d0:d1)*1.0e-6;
        if ( dmin > ON_ZERO_TOLERANCE )
          dmin = ON_ZERO_TOLERANCE;
        d = m_V[f0.vi[0]].DistanceTo(m_V[f0.vi[1]]);
        if ( d < dmin )
        {
          idmin = 0;
          dmin = d;
        }
        d = m_V[f0.vi[1]].DistanceTo(m_V[f0.vi[2]]);
        if ( d < dmin )
        {
          idmin = 1;
          dmin = d;
        }
        d = m_V[f0.vi[2]].DistanceTo(m_V[f0.vi[3]]);
        if ( d < dmin )
        {
          idmin = 2;
          dmin = d;
        }
        d = m_V[f0.vi[3]].DistanceTo(m_V[f0.vi[0]]);
        if ( d < dmin )
        {
          idmin = 3;
          dmin = d;
        }

        if ( 0 == idmin ) // m_V[f0.vi[0]] == m_V[f0.vi[1]] (nearly)
        {
          // degenerate quad - remove duplicate vertex
          f0.vi[0] = f0.vi[1];
          f0.vi[1] = f0.vi[2];
          f0.vi[2] = f0.vi[3];
        }
        else if ( 1 == idmin ) // m_V[f0.vi[1]] == m_V[f0.vi[2]] (nearly)
        {
          // degenerate quad - remove duplicate vertex
          int vi0 = f0.vi[0]; 
          f0.vi[0] = f0.vi[2];
          f0.vi[1] = f0.vi[3];
          f0.vi[2] = vi0;
          f0.vi[3] = vi0;
        }
        else if ( 2 == idmin ) // m_V[f0.vi[2]] == m_V[f0.vi[3]] (nearly)
        {
          // degenerate quad - remove duplicate vertex
          f0.vi[2] = f0.vi[1];
          f0.vi[1] = f0.vi[0];
          f0.vi[0] = f0.vi[3];
          f0.vi[3] = f0.vi[2];
        }
        else if ( 3 == idmin ) // m_V[f0.vi[3]] == m_V[f0.vi[0]] (nearly)
        {
          // degenerate quad - remove duplicate vertex
          f0.vi[3] = f0.vi[2];
        }
        else
        {
          // split non-degenerate quad into two triangles
          ON_MeshFace& f1 = m_F.AppendNew();
          if  ( d0 <= d1 ) {
            f1.vi[0] = f0.vi[0];
            f1.vi[1] = f0.vi[2];
            f1.vi[2] = f0.vi[3];
            f1.vi[3] = f1.vi[2];
            f0.vi[3] = f0.vi[2];
          }
          else {
            f1.vi[0] = f0.vi[1];
            f1.vi[1] = f0.vi[2];
            f1.vi[2] = f0.vi[3];
            f1.vi[3] = f1.vi[2];
            f0.vi[2] = f0.vi[3];
          }
          if ( bHasFaceNormals ) 
          {
            m_FN.AppendNew();
            ComputeFaceNormal(fi);
            ComputeFaceNormal(m_F.Count()-1);
          }
        }
      }
    }
    if ( fcount != m_F.Count() )
      DestroyTopology(); // we added some triangles
  }
  return ( QuadCount() == 0 && TriangleCount() == FaceCount() ) ? true : false;
}

bool ON_Mesh::CountQuads()
{
  const int fcount = FaceCount();
  const int vcount = VertexCount();
  int fi;
  m_quad_count = 0;
  m_triangle_count = 0;
  m_invalid_count = 0;
  for ( fi = 0; fi < fcount; fi++ ) {
    const ON_MeshFace& f = m_F[fi];
    if ( f.IsValid(vcount) ) {
      if ( f.IsTriangle() )
        m_triangle_count++;
      else
        m_quad_count++;
    }
    else
      m_invalid_count++;
  }
  return true;
}

bool ON_Mesh::ComputeVertexNormals()
{
  bool rc = false;
  const int fcount = FaceCount();
  const int vcount = VertexCount();
  int vi, fi, j;
  ON_3fVector n;

  if ( fcount > 0 && vcount > 0 ) {
    rc = HasFaceNormals();
    if ( !rc )
      rc = ComputeFaceNormals();
    if ( rc ) {
      ON_Workspace ws;
      //double* face_area = ws.GetDoubleMemory(fcount);
      int* vfcount = ws.GetIntMemory( vcount );
      memset ( vfcount, 0, vcount*sizeof(*vfcount) );

      // count number of faces that use each vertex
      for ( fi = 0; fi < fcount; fi++ ) {
        ON_MeshFace& f = m_F[fi];
        if ( f.IsValid(vcount) ) {
          vfcount[f.vi[0]]++;
          vfcount[f.vi[1]]++;
          vfcount[f.vi[2]]++;
          if ( f.IsQuad() )
            vfcount[f.vi[3]]++;
        }
      }

      // set vfi[vi][] = array of face indices that use vertex vi
      int** vfi = (int**)ws.GetMemory( vcount*sizeof(vfi[0] ) );
      {
        int scratch_sz = 0;
        for ( vi = 0; vi < vcount; vi++ ) {
          scratch_sz += vfcount[vi];
        }
        int* scratch = ws.GetIntMemory(scratch_sz);
        for ( vi = 0; vi < vcount; vi++ ) {
          if ( vfcount[vi] ) {
            vfi[vi] = scratch;
            scratch += vfcount[vi];
          }
          vfcount[vi] = 0;
        }
      }
      for ( fi = 0; fi < fcount; fi++ ) {
        ON_MeshFace& f = m_F[fi];
        if ( f.IsValid(vcount) ) {
          vi = f.vi[0]; vfi[vi][vfcount[vi]++] = fi;
          vi = f.vi[1]; vfi[vi][vfcount[vi]++] = fi;
          vi = f.vi[2]; vfi[vi][vfcount[vi]++] = fi;
          if ( f.IsQuad() ) {
            vi = f.vi[3]; vfi[vi][vfcount[vi]++] = fi;
          }
        }
      }

      // average face normals to get an estimate for a vertex normal
      m_N.SetCapacity(vcount);
      m_N.SetCount(0);
      for ( vi = 0; vi < vcount; vi++ ) {
        n.Zero();
        for ( j = vfcount[vi]-1; j >= 0; j-- ) {
          n += m_FN[vfi[vi][j]];
        }
        if ( !n.Unitize() )
        {
          // this vertex is not used by a face
          // set a unit z normal and press on.
          n.Set(0,0,1);
          rc = false;
        }
        m_N.Append(n);
      }
    }
  }
  return rc;
}

bool ON_Mesh::NormalizeTextureCoordinates()
{
  ON_2fPoint t0;//, t1;
  int ti;
  const int vertex_count = m_V.Count();
  bool rc = HasSurfaceParameters();
  if ( rc )
  {
    const ON_2dPoint* S = m_S.Array();
    ON_Interval udom = m_srf_domain[0];
    ON_Interval vdom = m_srf_domain[1];
    rc = udom.IsIncreasing() && vdom.IsIncreasing();
    if ( !rc )
    {
      udom.Set(S[0].x,S[0].x);
      vdom.Set(S[0].y,S[0].y);
      for ( ti = 1; ti < vertex_count; ti++ )
      {
        if      ( S[ti].x < udom.m_t[0] ) udom.m_t[0] = S[ti].x;
        else if ( S[ti].x > udom.m_t[1] ) udom.m_t[1] = S[ti].x;
        if      ( S[ti].y < vdom.m_t[0] ) vdom.m_t[0] = S[ti].y;
        else if ( S[ti].y > vdom.m_t[1] ) vdom.m_t[1] = S[ti].y;
      }
      rc = udom.IsIncreasing() && vdom.IsIncreasing();
    }

    if (rc)
    {
      m_T.Reserve(vertex_count);
      m_T.SetCount(0);
      for (ti = 0; ti < vertex_count; ti++ )
      {
        t0.x = (float)udom.NormalizedParameterAt(S[ti].x);
        t0.y = (float)vdom.NormalizedParameterAt(S[ti].y);
        m_T.Append(t0);
      }
      m_packed_tex_domain[0].Set(0.0,1.0);
      m_packed_tex_domain[1].Set(0.0,1.0);
      m_packed_tex_rotate = false;
      m_Ttag.SetDefaultSurfaceParameterMappingTag();
      if ( m_mesh_parameters )
        m_mesh_parameters->m_texture_range = 1;
    }
  }

  return rc;
}

bool ON_Mesh::TransposeSurfaceParameters()
{
	// swap m_srf_domain 
	ON_Interval temp = m_srf_domain[0];
	m_srf_domain[0]  = m_srf_domain[1];
	m_srf_domain[1]  = temp;

	double t = m_srf_scale[0];
	m_srf_scale[0] = m_srf_scale[1];
  m_srf_scale[1] = t;

  int S_count = m_S.Count();
  ON_2dPoint* S_array = m_S.Array();
  for (int i = 0; i < S_count; i++ )
  {
    ON_2dPoint& S = S_array[i];
    t = S.x; S.x = S.y; S.y = t;
  }
  return true;
}

bool ON_Mesh::HasPackedTextureRegion() const
{
  return (    ON_IsValid(m_srf_scale[0])
           && m_srf_scale[0] > 0.0
           && ON_IsValid(m_srf_scale[1])
           && m_srf_scale[1] > 0.0
           && m_packed_tex_domain[0].IsInterval()
           && m_packed_tex_domain[1].IsInterval()
           && 0.0 <= m_packed_tex_domain[0].Min()
           && m_packed_tex_domain[0].Max() <= 1.0
           && 0.0 <= m_packed_tex_domain[1].Min()
           && m_packed_tex_domain[1].Max() <= 1.0
           && (   fabs(m_packed_tex_domain[0].Length()) < 1.0
               || fabs(m_packed_tex_domain[1].Length()) < 1.0)
         );
}


bool ON_Mesh::TransposeTextureCoordinates()
{
  if ( !HasTextureCoordinates() )
    return false;

  const int vcnt = m_T.Count();
  int i;

  bool bPackedRegion =  HasPackedTextureRegion();

  bool bSrfParamTag = (!m_Ttag.IsSet() || m_Ttag.IsDefaultSurfaceParameterMapping());

  if ( bPackedRegion && bSrfParamTag )
  {
    // The region of the bitmap the texture uses 
    // cannot change.  The texture coordinates 
    // themselves get reflected in the subrectangle
    // about either the lowerleft to upperright
    // diagonal (llur = true) or the lowerright
    // to upperleft diagonal (llur = false).
    bool bRevU = m_packed_tex_domain[0].IsDecreasing();
    bool bRevV = m_packed_tex_domain[1].IsDecreasing();
    bool llur = (bRevU == bRevV) ? true : false;
    if ( m_packed_tex_rotate )
      llur = !llur;

    ON_Interval TD[2];
  	TD[0] = m_packed_tex_domain[0];
	  TD[1] = m_packed_tex_domain[1];
	  TD[0].MakeIncreasing(); 
	  TD[1].MakeIncreasing(); 
    for( i=0; i<vcnt; i++)
    {
	    ON_2fPoint tc = m_T[i];
	    double x = TD[0].NormalizedParameterAt(tc[0]);
	    double y = TD[1].NormalizedParameterAt(tc[1]);
	    if(!llur)
      {
		    x = 1.0-x;
		    y = 1.0-y;
	    }	
	    double s = TD[0].ParameterAt(y);
	    double t = TD[1].ParameterAt(x);
	    m_T[i].Set((float)s,(float)t);
    }
  }
  else
  {
    float f;
	  for(i=0; i<vcnt; i++)
    {
		  ON_2fPoint& tc = m_T[i];
      f = tc.x; tc.x = tc.y; tc.y = f;
	  }
  }
	return true;
}

bool ON_Mesh::ReverseTextureCoordinates( int dir )
{
  if ( dir < 0 || dir > 1 || !HasTextureCoordinates() )
    return false;

  bool bPackedRegion =  HasPackedTextureRegion();

  bool bSrfParamTag = (!m_Ttag.IsSet() || m_Ttag.IsDefaultSurfaceParameterMapping());

  const int vcount = m_T.Count();
  int i;
  if ( bPackedRegion && bSrfParamTag )
  {
    // The region of the bitmap the texture uses 
    // cannot change.  The texture coordinates 
    // themselves get reflected in the subrectangle
    // about either the lowerleft to upperright
    // diagonal (llur = true) or the lowerright
    // to upperleft diagonal (llur = false).
    if ( m_packed_tex_rotate )
      dir = 1-dir;
    const ON_Interval tex_dom = m_packed_tex_domain[dir];
    double s;
    m_packed_tex_domain[dir].Swap(); // Swap() is correct - Reverse() is wrong.
    for (i = 0; i < vcount; i++ )
    {
      ON_2fPoint& tc = m_T[i];
      s = 1.0 - tex_dom.NormalizedParameterAt(tc[dir]);
      if ( dir )
        tc.y = (float)tex_dom.ParameterAt(s);
      else
        tc.x = (float)tex_dom.ParameterAt(s);
    }
  }
  else
  {
    for (i = 0; i < vcount; i++ )
    {
      ON_2fPoint& tc = m_T[i];
      if ( dir )
        tc.y = 1.0f-tc.y;
      else
        tc.x = 1.0f-tc.x;
    }
  }
	return true;
}

bool ON_Mesh::ReverseSurfaceParameters( int dir )
{
  if ( dir < 0 || dir > 1 || !HasSurfaceParameters() )
    return false;
  if ( m_srf_domain[dir].IsIncreasing() )
    m_srf_domain[dir].Reverse();
  int i, vcount = m_S.Count();
  for (i = 0; i < vcount; i++)
  {
    ON_2dPoint& S = m_S[i];
    if ( dir )
      S.y = -S.y;
    else
      S.x = -S.x;
  }
  return true;
}


bool ON_Mesh::EvaluateMeshGeometry( const ON_Surface& srf )
{
  bool rc = false;
  const int vcount = VertexCount();
  const bool bHasSurfaceParameters = HasSurfaceParameters();
  if ( bHasSurfaceParameters )
  {
    const bool bHasVertexNormals = HasVertexNormals();
    m_N.SetCapacity(vcount);
    int vi, side, hint[2];
    ON_3dPoint point;
    ON_3dVector normal, Ds, Dt, Dss, Dst, Dtt, K1, K2;
    const ON_2dPoint* srf_st;
    double s, t, kgauss, kmean;
    side = 0;
    hint[0] = 0;
    hint[1] = 0;
    const double smax = srf.Domain(0)[1];
    const double tmax = srf.Domain(1)[1];
    if ( HasPrincipalCurvatures() ) 
    {
      for ( vi = 0; vi < vcount; vi++ ) 
      {
        //ON_2fPoint& tex = m_T[vi];
        // convert texture coordinates to normalizied texture coordinates
        srf_st = &m_S[vi];
        s = srf_st->x;
        t = srf_st->y;
        // 19 April 2006 Dale Lear - added side setter so singular normals
        //                           are correctly evaluated RR 12482
        side = ( smax == s ) ? ((tmax == t) ? 3 : 2) : ((tmax == t) ? 4 : 1);
        srf.Ev2Der( s, t, point, Ds, Dt, Dss, Dst, Dtt, side, hint );
        ON_EvNormal( side, Ds, Dt, Dss, Dst, Dtt, normal );
        ON_EvPrincipalCurvatures( Ds, Dt, Dss, Dst, Dtt, normal,
                                  &kgauss, &kmean, 
                                  &m_K[vi].k1, &m_K[vi].k2, 
                                  K1, K2 ); //m_K[vi].e1, m_K[vi].e2 );
        m_V[vi] = &point.x; // use ON_3fPoint double* conversion (quiets gcc)
        if ( bHasVertexNormals )
          m_N[vi] = &normal.x; // use ON_3fVector double* conversion (quiets gcc)
      }
      InvalidateCurvatureStats();
    }
    else if ( bHasVertexNormals ) 
    {
      for ( vi = 0; vi < vcount; vi++ ) 
      {
        //ON_2fPoint& tex = m_T[vi];
        srf_st = &m_S[vi];
        s = srf_st->x;
        t = srf_st->y;
        // 19 April 2006 Dale Lear - added side setter so singular normals
        //                           are correctly evaluated RR 12482
        side = ( smax == s ) ? ((tmax == t) ? 3 : 2) : ((tmax == t) ? 4 : 1);
        srf.EvNormal( s, t, point, normal, side, hint );
        m_V[vi] = &point.x; // use ON_3fPoint double* conversion (quiets gcc)
        m_N[vi] = &normal.x; // use ON_3fVector double* conversion (quiets gcc)
      }
    }
    else 
    {
      for ( vi = 0; vi < vcount; vi++ ) 
      {
        //ON_2fPoint& tex = m_T[vi];
        srf_st = &m_S[vi];
        s = srf_st->x;
        t = srf_st->y;
        srf.EvPoint( s, t, point, side, hint );
        m_V[vi] = &point.x;
      }
    }
    if ( HasFaceNormals() )
    {
      ComputeFaceNormals();
    }
    rc = true;

    m_Ctag.Default();
    InvalidateVertexBoundingBox();
    InvalidateVertexNormalBoundingBox();
    DeleteMeshParameters();
    DestroyTree();
  }
  return rc;
}

void ON_Mesh::SetMeshParameters( const ON_MeshParameters& mp )
{
  DeleteMeshParameters();
  m_mesh_parameters = new ON_MeshParameters(mp);
}

const ON_MeshParameters* ON_Mesh::MeshParameters() const
{
  return m_mesh_parameters;
}

void ON_Mesh::DeleteMeshParameters()
{
  if ( m_mesh_parameters ) {
    delete m_mesh_parameters;
    m_mesh_parameters = 0;
  }
}

void ON_Mesh::DestroyTree( bool bDeleteTree )
{
  if ( m_mtree ) 
  {
#if defined(OPENNURBS_PLUS_INC_)
    if ( bDeleteTree )
    {
      delete m_mtree;
    }
#endif
    m_mtree = 0;
  }
}

static int compare3fPoint( const ON_3fPoint* a, const ON_3fPoint* b )
{
  if ( a->x < b->x ) return -1;
  if ( a->x > b->x ) return  1;
  if ( a->y < b->y ) return -1;
  if ( a->y > b->y ) return  1;
  if ( a->z < b->z ) return -1;
  if ( a->z > b->z ) return  1;
  return 0;
}

typedef int (*ON_COMPAR_LPVOID_LPVOID)(const void*,const void*);

static 
int GetPointMap( int pt_count, const ON_3fPoint* pt, ON_SimpleArray<int>& pt_map )
{
  // Builds a mapping array, pt_map[], such that the length of pt_map[] 
  // is pt_count and pt_map[i] == pt_map[j] if and only if pt[i] == pt[j]
  // as 3d points.  The values in map[] run from 0 to max_pt_index.
  int vt0, vt1;
  ON_3fPoint p0;
  int* map;
  int* index;
  int max_pt_index = 0;
  if ( pt_count > 0 && pt ) {
    index = (int*)onmalloc(pt_count*sizeof(*index));
    ON_Sort( ON::quick_sort, index, pt, pt_count, sizeof(*pt), (ON_COMPAR_LPVOID_LPVOID)compare3fPoint );
    pt_map.SetCapacity( pt_count );
    pt_map.SetCount( pt_count );
    map = pt_map.Array();
    for ( vt0 = 0; vt0 < pt_count; vt0++ )
      map[vt0] = -1;
    for (vt0 = 0; vt0 < pt_count; vt0 = vt1, max_pt_index++) {
      p0 = pt[index[vt0]];
      for ( vt1=vt0+1; vt1<pt_count && 0==compare3fPoint(&p0,pt+index[vt1]); vt1++ ) {
        // empty
      }
      while ( vt0 < vt1 ) {
        map[index[vt0++]] = max_pt_index;
      }
    }
    onfree(index);
  }
  if ( max_pt_index == 0 )
    pt_map.Destroy();
  return max_pt_index;
}

int ON_Mesh::CullDegenerateFaces()
{
  // 30 December 2003 Dale Lear - fixed bug that was not culling
  //     topologically degenerate faces.

  int bad_count = 0;
  int degenerate_count = 0;
  const int fcount = m_F.Count();
  const int vcount = m_V.Count();
  ON_MeshFace f;
  int fi;

  if ( fcount > 0 ) 
  {
    // use GetTopologyVertexMap() instead of calling Topology() because this mesh
    // may have lots of bogus faces that need to be culled.
    ON_SimpleArray<int> topv_map;
    const int topv_count = GetPointMap( m_V.Count(), m_V.Array(), topv_map );
    if ( topv_count > 0 && topv_map.Count() == m_V.Count() ) 
    {
      ON_Workspace ws;
      const int* vtop = topv_map.Array();
      unsigned char* bBadFace = (unsigned char*)ws.GetMemory(fcount*sizeof(*bBadFace));
      memset( bBadFace, 0, fcount*sizeof(*bBadFace) );
      for ( fi = 0; fi < fcount; fi++ ) 
      {
        ON_MeshFace& f0 = m_F[fi];
        // set f.vi[] to values of topoligical indices
        {
          int f0vi = f0.vi[0];
          f.vi[0] = (f0vi < 0 || f0vi >= vcount) ? -1 : vtop[f0vi];

          f0vi = f0.vi[1];
          f.vi[1] = (f0vi < 0 || f0vi >= vcount) ? -1 : vtop[f0vi];

          f0vi = f0.vi[2];
          f.vi[2] = (f0vi < 0 || f0vi >= vcount) ? -1 : vtop[f0vi];

          f0vi = f0.vi[3];
          f.vi[3] = (f0vi < 0 || f0vi >= vcount) ? -1 : vtop[f0vi];
        }

        if ( !f.IsValid(topv_count) ) 
        {
          degenerate_count++;
          //f = m_F[fi];
          if ( f.vi[0] == f.vi[1] || f.vi[0] < 0 || f.vi[0] >= topv_count )
          {
            f0.vi[0] = f0.vi[1];
            f0.vi[1] = f0.vi[2];
            f0.vi[2] = f0.vi[3];
            f.vi[0] = f.vi[1];
            f.vi[1] = f.vi[2];
            f.vi[2] = f.vi[3];
          }

          if ( f.vi[1] == f.vi[2] || f.vi[1] < 0 || f.vi[1] >= topv_count )
          {
            f0.vi[1] = f0.vi[2];
            f0.vi[2] = f0.vi[3];
            f.vi[1] = f.vi[2];
            f.vi[2] = f.vi[3];
          }

          if ( f.vi[2] < 0 || f.vi[2] >= topv_count )
          {
            f0.vi[2] = f0.vi[3];
            f.vi[2] = f.vi[3];
          }

          if ( f.vi[3] < 0 || f.vi[3] >= topv_count )
          {
            f0.vi[3] = f0.vi[2];
            f.vi[3] = f.vi[2];
          }
          else if ( f.vi[0] == f.vi[3] && f.vi[2] != f.vi[3] )
          {
            f0.vi[0] = f0.vi[1];
            f0.vi[1] = f0.vi[2];
            f0.vi[2] = f0.vi[3];
            f.vi[0] = f.vi[1];
            f.vi[1] = f.vi[2];
            f.vi[2] = f.vi[3];
          }

          if ( !f0.IsValid(vcount) || !f.IsValid(topv_count) ) 
          {
            // face cannot be repaired by juggling vertex indices
            bBadFace[fi] = 1;
            bad_count++;
          }
        }
      }

      if ( bad_count > 0 ) 
      {
        //  remove bad faces.
        m_FN.SetCount(0);

        int fcnt = 0;
        for ( fi = fcnt = 0; fi < fcount; fi++ )
        {
          if ( !bBadFace[fi] )
          {
            if ( fcnt < fi )
              m_F[fcnt] = m_F[fi];
            fcnt++;
          }
        }
        m_F.SetCount(fcnt);
      }

      if ( degenerate_count > 0 )
      {
        // mesh was modified - destroy information that was
        // calculated using the old mesh
        DestroyTree();
        DestroyPartition();
        DestroyTopology();
        m_invalid_count = 0;
        m_quad_count = 0;
        m_triangle_count = 0;
      }
    }
  }
  return degenerate_count;
}

int ON_Mesh::CullUnusedVertices()
{
  //int cullcount = 0;
  int vi, fi;
  ON_Workspace ws;
  CullDegenerateFaces();
  int fcnt = m_F.Count();
  int vcnt = m_V.Count();
  int * vmap = ws.GetIntMemory( vcnt );
  memset ( vmap, 0, vcnt*sizeof(vmap[0]) );
  for ( fi = 0; fi < fcnt; fi++ )
  {
    const ON_MeshFace& f = m_F[fi];
    vmap[f.vi[0]] = 1;
    vmap[f.vi[1]] = 1;
    vmap[f.vi[2]] = 1;
    vmap[f.vi[3]] = 1;
  }
  int newvcnt = 0;
  for ( vi = 0; vi < vcnt; vi++ )
  {
    if ( vmap[vi] )
      vmap[vi] = newvcnt++;
    else {
      vmap[vi] = -1;
    }
  }
  if ( newvcnt == 0 )
    Destroy();
  else if ( newvcnt < vcnt )
  {
    DestroyTopology();

    void* buffer = ws.GetMemory(vcnt*9*sizeof(double));

    if ( HasSurfaceParameters() ) {
      ON_2dPoint* s = (ON_2dPoint*)buffer;
      for ( vi = 0; vi < vcnt; vi++ )
      {
        if ( vmap[vi]>=0 )
          s[vmap[vi]] = m_S[vi];
      }
      memcpy( m_S.Array(), s, newvcnt*sizeof(s[0]) );
      m_S.SetCount(newvcnt);
    }

    if ( HasVertexNormals() ) {
      ON_3fVector* v = (ON_3fVector*)buffer;
      for ( vi = 0; vi < vcnt; vi++ )
      {
        if ( vmap[vi]>=0 )
          v[vmap[vi]] = m_N[vi];
      }
      memcpy( m_N.Array(), v, newvcnt*sizeof(v[0]) );
      m_N.SetCount(newvcnt);
    }

    if ( HasTextureCoordinates() ) {
      ON_2fPoint* t = (ON_2fPoint*)buffer;
      for ( vi = 0; vi < vcnt; vi++ )
      {
        if ( vmap[vi]>=0 )
          t[vmap[vi]] = m_T[vi];
      }
      memcpy( m_T.Array(), t, newvcnt*sizeof(t[0]) );
      m_T.SetCount(newvcnt);
    }

    if ( HasPrincipalCurvatures() ) {
      ON_SurfaceCurvature* k = (ON_SurfaceCurvature*)buffer;
      for ( vi = 0; vi < vcnt; vi++ )
      {
        if ( vmap[vi]>=0 )
          k[vmap[vi]] = m_K[vi];
      }
      memcpy( m_K.Array(), k, newvcnt*sizeof(k[0]) );
      m_K.SetCount(newvcnt);
    }

    if ( HasVertexColors() ) {
      ON_Color* c = (ON_Color*)buffer;
      for ( vi = 0; vi < vcnt; vi++ )
      {
        if ( vmap[vi]>=0 )
          c[vmap[vi]] = m_C[vi];
      }
      memcpy( m_C.Array(), c, newvcnt*sizeof(c[0]) );
      m_C.SetCount(newvcnt);
    }

    {
      ON_3fPoint* p = (ON_3fPoint*)buffer;
      for ( vi = 0; vi < vcnt; vi++ )
      {
        if ( vmap[vi]>=0 )
          p[vmap[vi]] = m_V[vi];
      }
      memcpy( m_V.Array(), p, newvcnt*sizeof(p[0]) );
      m_V.SetCount(newvcnt);
    }

    for ( fi = 0; fi < fcnt; fi++ )
    {
      ON_MeshFace& f = m_F[fi];
      f.vi[0] = vmap[f.vi[0]];
      f.vi[1] = vmap[f.vi[1]];
      f.vi[2] = vmap[f.vi[2]];
      f.vi[3] = vmap[f.vi[3]];
    }

  }
  return vcnt - newvcnt;
}

bool ON_Mesh::Compact()
{
  // CullDegenerateFaces(); // CullUnusedVertices() does this
  CullUnusedVertices();
  m_V.Shrink();
  m_F.Shrink();
  m_N.Shrink();
  m_FN.Shrink();
  m_K.Shrink();
  m_C.Shrink();
  m_S.Shrink();
  m_T.Shrink();
  return true;
}

////////////////////////////////////////////////////////////////
//
//   ON_SurfaceCurvature
//
double ON_SurfaceCurvature::GaussianCurvature() const
{
  return k1*k2;
}

double ON_SurfaceCurvature::MeanCurvature() const
{
  return 0.5*(k1+k2);
}

double ON_SurfaceCurvature::MinimumRadius() const
{
  double k;
  k = (fabs(k1)>=fabs(k2)) ? fabs(k1) : fabs(k2); // k = maximum directional curvature
  k = ( k > 1.0e-300 ) ? 1.0/k : 1.0e300;         // 1/k = minimum radius of curvature
  return k;
}

double ON_SurfaceCurvature::MaximumRadius() const
{
  double k;
  if ( k1*k2 <= 0.0 ) {
    // if principal curvatures have opposite signs, there
    // is a direction with zero directional curvature
    k = 0.0;
  }
  else {
    k = (fabs(k1)<=fabs(k2)) ? fabs(k1) : fabs(k2);
  }
  // k = minimum directional curvature
  k = ( k > 1.0e-300 ) ? 1.0/k : 1.0e300; // 1/k = maximum radius of curvature
  return k;
}

//double ON_SurfaceCurvature::NormalCurvature(const ON_3dVector& tangent) const
//{
//  double c = tangent*e1;
//  double s = tangent*e2;
//  return k1*c*c + k2*s*s;
//}

//double ON_SurfaceCurvature::NormalSectionCurvature( const ON_3dVector& section_normal, const ON_3dVector& surface_normal ) const
//{
//  ON_3dVector tangent = ON_CrossProduct( section_normal, surface_normal );
//  if ( fabs(tangent.x) <= ON_SQRT_EPSILON && fabs(tangent.y) <= ON_SQRT_EPSILON && fabs(tangent.z) <= ON_SQRT_EPSILON )
//    tangent.Zero();
//  else
//    tangent.Unitize();
//  return NormalCurvature(tangent);
//}

ON_MeshTopology::ON_MeshTopology() 
: m_mesh(0), m_memchunk(0), m_bIsValid(0)
{
}

ON_MeshTopology::~ON_MeshTopology()
{
  Destroy();
}

void ON_MeshTopology::Destroy()
{
  m_topv_map.Destroy();
  m_topv.Destroy();
  m_tope.Destroy();
  m_topf.Destroy();
  struct memchunk *p, *n;
  n = m_memchunk;
  while(n)
  {
    p = n;
    n = n->next;
    onfree(p);
  }
  m_memchunk = 0;
  m_bIsValid = 0;
}

void ON_MeshTopology::EmergencyDestroy()
{
  m_mesh = 0;
  m_topv_map.EmergencyDestroy();
  m_topv.EmergencyDestroy();
  m_tope.EmergencyDestroy();
  m_topf.EmergencyDestroy();
  m_memchunk = 0;
  m_bIsValid = 0;
}

int ON_MeshTopology::TopVertexCount() const
{
  return m_topv.Count();
}

//////////
// number of topoligical edges
int ON_MeshTopology::TopEdgeCount() const
{
  return m_tope.Count();
}

//////////
// number of topoligical faces (same as m_mesh.FaceCount())
int ON_MeshTopology::TopFaceCount() const
{
  return m_topf.Count();
}

ON_3fPoint ON_MeshTopology::TopVertexPoint( int vtopi ) const
{
  return m_mesh->m_V[m_topv[vtopi].m_vi[0]];
}

ON_Line ON_MeshTopology::TopEdgeLine( int tope_index ) const
{
  ON_Line L(ON_UNSET_POINT,ON_UNSET_POINT);
  if ( m_mesh && tope_index >= 0 && tope_index < m_tope.Count() )
  {
    const int* topvi = m_tope[tope_index].m_topvi;
    if (   topvi[0] >= 0 && topvi[0] < m_topv.Count() 
        && topvi[1] >= 0 && topvi[1] < m_topv.Count() )
    {
      const ON_MeshTopologyVertex& v0 = m_topv[topvi[0]];
      const ON_MeshTopologyVertex& v1 = m_topv[topvi[1]];
      if ( v0.m_v_count > 0 && v0.m_vi && v1.m_v_count > 0 && v1.m_vi )
      {
        int vi0 = v0.m_vi[0];
        int vi1 = v1.m_vi[0];
        int vcount = m_mesh->m_V.Count();
        if ( vi0 >= 0 && vi0 < vcount && vi1 >= 0 && vi1 < vcount )
        {
          L.from = m_mesh->m_V[vi0];
          L.to   = m_mesh->m_V[vi1];
        }
      }
    }
  }
  return L;
}

////////
// returns index of edge that connects topological vertices
// returns -1 if no edge is found.
int ON_MeshTopology::TopEdge( int vtopi0, int vtopi1 ) const
{
  int i0, i1, ei, vi0;
  if ( vtopi0 > vtopi1 ) {vi0 = vtopi0; vtopi0 = vtopi1; vtopi1 = vi0;}
  if ( vtopi0 < vtopi1 ) {
    const int tope_count = TopEdgeCount();
    const ON_MeshTopologyEdge* tope = m_tope.Array(); // to speed array access
    i0 = 0;
    i1 = tope_count;
    while ( i0 < i1 )
    {
      ei = (i0+i1)/2;
      vi0 = tope[ei].m_topvi[0];
      if ( vi0 < vtopi0 ) {
        if ( i0 == ei )
          break;
        i0 = ei;
      }
      else if ( vi0 > vtopi0 ) {
        if ( i1 == ei )
          break;
        i1 = ei;
      }
      else {
        while (ei > 0 && tope[ei-1].m_topvi[0] == vtopi0 )
          ei--;
        while ( ei < tope_count && tope[ei].m_topvi[0] == vtopi0 ) {
          if ( tope[ei].m_topvi[1] == vtopi1 )
            return ei;
          ei++;
        }
        break;
      }
    }
  }
  return -1;
}

bool ON_MeshTopology::GetTopFaceVertices( int fi, int topvi[4] ) const
{
  if ( fi >= 0 && fi < m_mesh->m_F.Count() ) {
    const int* fvi = m_mesh->m_F[fi].vi;
    topvi[0] = m_topv_map[fvi[0]];
    topvi[1] = m_topv_map[fvi[1]];
    topvi[2] = m_topv_map[fvi[2]];
    topvi[3] = m_topv_map[fvi[3]];
  }
  return true;
}

bool ON_MeshTopology::SortVertexEdges() const
{
  bool rc = true;
  int topvi, topv_count = m_topv.Count();
  for ( topvi = 0; topvi < topv_count; topvi++ )
  {
    if ( !SortVertexEdges(topvi) )
      rc = false;
  }
  return rc;
}

// TODO make public and add to header file new ON_SortIntArray
static
void ON_ReverseIntArray(
        int* e,    // array of ints
        size_t  nel   // length of array
        )
{
  int ei;
  size_t i;
  nel--;
  for ( i = 0; i<nel; i++, nel--)
  {
    ei = e[i];
    e[i] = e[nel];
    e[nel] = ei;
  }
}

bool ON_MeshTopology::SortVertexEdges(int topvi) const
{
  if ( topvi < 0 || topvi >= m_topv.Count() )
    return false;

  const ON_MeshTopologyVertex& topv = m_topv[topvi];
  if ( topv.m_tope_count < 2 )
    return true;

  // Divide the edges that use this vertex into two sets:
  //   e1f[] = indices of edges that bound 1 face, 3 or 
  //           more faces, or no faces (in that order).
  //   e2f[] = indices of edges that bound exactly 2 faces.
  int i, j;
  int topei;
  int vei;
  int efcnt;
  int* new_tope = (int*)alloca(5*topv.m_tope_count*sizeof(new_tope[0]));
  int* e2f  = new_tope + topv.m_tope_count;
  int* e1f  = e2f + topv.m_tope_count;
  int e1fcnt = 0;
  int e2fcnt = 0;
  {
    int* e3f  = e1f + topv.m_tope_count; // e3f[] = edges with 3 or more faces
    int* e0f  = e3f + topv.m_tope_count; // e0f[] = edges with no faces
    int e3fcnt = 0;
    int e0fcnt = 0;

    for ( vei = 0; vei < topv.m_tope_count; vei++ )
    {
      topei = topv.m_topei[vei];
      if ( topei >= 0 && topei < m_tope.Count() )
      {
        const ON_MeshTopologyEdge& tope = m_tope[topei];
        if ( tope.m_topvi[0] == topvi || tope.m_topvi[1] == topvi )
        {
          efcnt = m_tope[topei].m_topf_count;
          if ( efcnt < 0 )
          {
            ON_ERROR("ON_MeshTopology::SortVertexEdges(int topvi) - m_tope[topei].m_topf_count < 0"); 
            return false;
          }
          switch(efcnt)
          {
          case 0: // edge has no faces 
            // (never happens if topology is from a valid ON_Mesh.)
            e0f[e0fcnt++] = topei;                                   
            break;

          case 1: // edge has exactly one face
            e1f[e1fcnt++] = topei; 
            break;

          case 2: // edge has exactly two faces
            e2f[e2fcnt++] = topei;
            break;

          default: // edge has 3 or more faces
            // (happens when mesh is non-manifold)
            e3f[e3fcnt++] = topei;
            break;
          }
        }
      }
    }
  
    // append list of non-manifold edges (3 or more faces) to e1f[]
    for ( i = 0; i < e3fcnt; i++ )
    {
      e1f[e1fcnt++] = e3f[i];
    }

    // append list of wire edges (0 faces) to e1f[]
    for ( i = 0; i < e0fcnt; i++ )
    {
      e1f[e1fcnt++] = e0f[i];
    }

    e0fcnt = 0;
    e3fcnt = 0;
    e0f = 0;
    e3f = 0;
  }

  // Put the edge indices in new_tope[] in radial order.
  // NOTE: The code below works for non-oriented meshes and
  //       non-manifold meshes.  If you change the code, you
  //       must make sure that it still works in these cases.
  if ( e1fcnt + e2fcnt != topv.m_tope_count )
  {
    ON_ERROR("ON_MeshTopology::SortVertexEdges() input vertex had bogus m_topei[]");
    return false;
  }
  int fi = -1;
  int next_topei = -1;
  int efi, fecnt, fei, next_fei;
  vei = 0;
  int vei0 = 0;
  int vei1 = 0;
  int elist_dir = 0;
  while(vei < topv.m_tope_count)
  {
    if ( next_topei >= 0 )
    {
      // continue with this group of edges
      topei = next_topei;
      next_topei = -1;
    }
    else if ( e1fcnt > 0 )
    {
      // start a new group of edges
      topei = *e1f++;
      e1fcnt--;
      vei1 = vei;
    }
    else if ( e2fcnt > 0 )
    {
      // start a new group of edges
      // (this only happens in non-manifold cases)
      topei = *e2f++;
      e2fcnt--;
      vei1 = vei;
    }
    else
    {
      ON_ERROR("ON_MeshTopology::SortVertexEdges() input vertex had bogus topology information.");
      return false;
    }

    if ( vei0 < vei1 )
    {
      if ( 1 == elist_dir )
      {
        // 30 December 2003 Dale Lear added this feature
        //   edges new_tope[vei0],...,new_tope[vei1-1] are radially sorted
        //   but the order is not counterclockwise with respect to
        //   the normal of the face attached to the first edge.
        //   So, this group of edges in new_tope[] needs to
        //   be reversed.
        ON_ReverseIntArray( new_tope+vei0, vei1-vei0 );
      }
      elist_dir = 0;
      vei0 = vei1;
    }

    new_tope[vei++] = topei;

    // search faces connected to tope for the next edge
    const ON_MeshTopologyEdge& tope = m_tope[topei];
    for ( efi = 0; next_topei < 0 && efi < tope.m_topf_count; efi++ )
    {
      fi = tope.m_topfi[efi];
      if ( fi < 0 || fi >= m_topf.Count() )
      {
        // bogus face index into m_topf[] array
        continue;
      }

      // find fei so that topf.m_topei[fei] = topei
      const ON_MeshTopologyFace& topf = m_topf[fi];
      fecnt = topf.IsQuad() ? 4 : 3;
      for ( fei = 0; fei < fecnt; fei++ )
      {
        if ( topf.m_topei[fei] != topei )
          continue;

        if ( tope.m_topvi[0] == topvi )
        {
          next_fei = ( topf.m_reve[fei] ) ?  1 : -1;
        }
        else
        {
          next_fei = ( topf.m_reve[fei] ) ? -1 :  1;
        }

        if ( 0 == elist_dir )
          elist_dir = next_fei;

        next_fei = (fei+next_fei+fecnt)%fecnt;
        next_topei = topf.m_topei[next_fei];

        // next_topei = candidate for the next edge
        //  Check to see that it is available by
        //  finding it in the e1f[] or e2f[] arrays.
        j = 0;
        for ( i = 0; i < e1fcnt; i++ )
        {
          if ( next_topei == e1f[i] )
          {
            // found it in the e1f list.
            for ( j = i+1; j < e1fcnt; j++ )
              e1f[j-1] = e1f[j];
            e1fcnt--;
            break;
          }
        }
        if ( 0 == j )
        {
          // search the e2f[] list
          for ( i = 0; i < e2fcnt; i++ )
          {
            if ( next_topei == e2f[i] )
            {
              for ( j = i+1; j < e2fcnt; j++ )
                e2f[j-1] = e2f[j];
              e2fcnt--;
              break;
            }
          }
        }
        if ( 0 == j )
        {
          // the candidate was already used, check the next
          // face attached to this edge.
          next_topei = -1;
        }
        else
        {
          break;
        }
      }
    }
  }

  if ( topv.m_tope_count != vei )
  {
    ON_ERROR("ON_MeshTopology::SortVertexEdges() edge sorting error.");
    return false;
  }

  vei1 = vei;
  if ( vei0 < vei1 )
  {
    if ( 1 == elist_dir )
    {
      // 30 December 2003 Dale Lear added this feature
      //   edges new_tope[vei0],...,new_tope[vei1-1] are radially sorted
      //   but the order is not counterclockwise with respect to
      //   the normal of the face attached to the first edge.
      //   So, this group of edges in new_tope[] needs to
      //   be reversed.
      ON_ReverseIntArray( new_tope+vei0, vei1-vei0 );
    }
    elist_dir = 0;
    vei0 = vei1;
  }

  int* topv_m_topei = const_cast<int*>(topv.m_topei);
  for ( vei = 0; vei < topv.m_tope_count; vei++ )
  {
    topv_m_topei[vei] = new_tope[vei];
  }

  return true;
}

struct TMPEDGE
{
  int vi[2];
  int fi;
};

static int ecomp( const struct TMPEDGE* ea, const struct TMPEDGE* eb )
{
  if ( ea->vi[0] < eb->vi[0] )
    return -1;
  if ( ea->vi[0] > eb->vi[0] )
    return 1;
  if ( ea->vi[1] < eb->vi[1] )
    return -1;
  if ( ea->vi[1] > eb->vi[1] )
    return 1;
  if ( ea->fi < eb->fi )
    return -1;
  if ( ea->fi > eb->fi )
    return 1;
  return 0;
}

int* ON_MeshTopology::GetIntArray(int length)
{
  int* a = 0;
  if ( length > 0 ) {
    struct memchunk* pm;
    pm = (struct memchunk*)onmalloc(length*sizeof(*a) + sizeof(*pm));
    if ( pm ) {
      pm->next = m_memchunk;
      m_memchunk = pm++;
      a = (int*)pm;
    }
  }
  return a;
}

bool ON_MeshTopologyFace::IsTriangle() const
{
  return ( m_topei[2] == m_topei[3] && m_topei[0] != m_topei[1] )
         ? true : false;
}

bool ON_MeshTopologyFace::IsQuad() const
{
  return ( m_topei[2] != m_topei[3] ) ? true : false;
}

bool ON_MeshTopologyFace::IsValid() const
{
  return ( m_topei[0] != m_topei[1] ) ? true : false;
}


bool ON_MeshTopology::IsValid() const
{
  ON_Workspace ws;
  int topvi, topei, topfi, vi, fi, j, jmax, k, tfvi[4];
  ON_3fPoint p;

  // simple checks
  if ( !m_bIsValid )
    return false;
  if ( !m_mesh )
    return false;
  if ( this != &(m_mesh->Topology()) )
    return false;
  const int v_count = m_mesh->VertexCount();
  const int f_count = m_mesh->FaceCount();
  const int topv_count = TopVertexCount();
  const int tope_count = TopEdgeCount();
  const int topf_count = TopFaceCount();
  if ( topv_count > v_count || topv_count < 0 )
    return false;
  if ( topv_count == 0 && v_count > 0 )
    return false;
  if ( topf_count != f_count )
    return false;
  if ( f_count > 0 && tope_count < 3 )
    return false;
  if ( m_topv_map.Count() != v_count )
    return false;

  // check vertex information
  for ( vi = 0; vi < v_count; vi++ ) {
    topvi = m_topv_map[vi];
    if ( topvi < 0 || topvi >= topv_count )
      return false;
  }

  char* vCheck = (char*)ws.GetMemory( v_count*sizeof(*vCheck) );
  memset( vCheck, 0, v_count*sizeof(*vCheck) );
  for ( topvi = 0; topvi < topv_count; topvi++ )
  {
    const ON_MeshTopologyVertex& topv = m_topv[topvi];
    if ( topv.m_v_count <= 0 )
      return false;
    if ( !topv.m_vi )
      return false;
    p = TopVertexPoint(topvi);
    for ( j = 0; j < topv.m_v_count; j++ ) {
      vi = topv.m_vi[j];
      if ( vi < 0 )
        return false;
      if ( vi >= v_count )
        return false;
      if ( vCheck[vi] != 0 )
        return false; // mesh.m_V[vi] is referenced multiple times
      if ( compare3fPoint( &p, &m_mesh->m_V[vi] ) )
        return false; // mesh.m_V[vi] not at same location as topv
      if ( m_topv_map[vi] != topvi )
        return false; 
      vCheck[vi] = 1;
    }

    // check that edges in m_topei[] list have topvi has a start/end
    if ( topv.m_tope_count < 0 )
      return false;
    if ( topv.m_tope_count == 0 && topv.m_topei )
      return false; // array should be NULL
    if ( topv.m_tope_count > 0 && !topv.m_topei )
      return false; // array should not be NULL
    for ( j = 0; j < topv.m_tope_count; j++ ) {
      topei = topv.m_topei[j];
      if ( topei < 0 )
        return false;
      if ( topei >= tope_count )
        return false;
      const ON_MeshTopologyEdge& tope = m_tope[topei];
      if ( tope.m_topvi[0] != topvi && tope.m_topvi[1] != topvi )
        return false; // edge doesn't reference this top vtx
      for ( k = 0; k < j; k++ ) {
        if ( topv.m_topei[k] == topei )
          return false; // edge listed multiple times
      }
    }
  }

  // make sure every mesh.m_V[] maps to a topoligical vertex
  for ( vi = 0; vi < v_count; vi++ ) {
    if ( vCheck[vi] != 1 )
      return false; // mesh.m_V[vi] is not referenced
    topvi = m_topv_map[vi];
    if ( topvi < 0 )
      return false;
    if ( topvi >= topv_count )
      return false;
    const ON_MeshTopologyVertex& topv = m_topv[topvi];
    for ( j = 0; j < topv.m_v_count; j++ ) {
      if ( topv.m_vi[j] == vi )
        break;
    }
    if ( j >= topv.m_v_count )
      return false; // bogus m_topv_map[] array
  }

  // check edges
  for ( topei = 0; topei < tope_count; topei++ ) {
    const ON_MeshTopologyEdge& tope = m_tope[topei];
    if ( tope.m_topvi[0] < 0 || tope.m_topvi[0] >= topv_count )
      return false;
    if ( tope.m_topvi[1] < 0 || tope.m_topvi[1] >= topv_count )
      return false;
    if ( tope.m_topvi[0] == tope.m_topvi[1] )
      return false;

    const ON_MeshTopologyVertex& topv0 = m_topv[tope.m_topvi[0]];
    for ( j = 0; j < topv0.m_tope_count; j++ ) {
      if ( topv0.m_topei[j] == topei )
        break;
    }
    if ( j >= topv0.m_tope_count )
      return false; // start vtx not linked to edge

    const ON_MeshTopologyVertex& topv1 = m_topv[tope.m_topvi[1]];
    for ( j = 0; j < topv1.m_tope_count; j++ ) {
      if ( topv1.m_topei[j] == topei )
        break;
    }
    if ( j >= topv1.m_tope_count )
      return false; // end vtx not linked to edge

    if ( tope.m_topf_count < 0 )
      return false;
    if ( tope.m_topf_count == 0 && tope.m_topfi )
      return false; // array should be NULL
    if ( tope.m_topf_count > 0 && !tope.m_topfi )
      return false; // array should not be NULL
    for ( j = 0; j < tope.m_topf_count; j++ ) {
      fi = tope.m_topfi[j];
      if ( fi < 0 || fi >= f_count )
        return false;
      const ON_MeshTopologyFace& topf = m_topf[fi];
      for ( k = 0; k < 4; k++ ) {
        if ( topf.m_topei[k] == topei )
          break;
      }
      if ( k >= 4 )
        return false; // edge not in face's list
    }
  }

  // check faces
  for ( fi = 0; fi < f_count; fi++ ) {
    topfi = fi;
    const ON_MeshTopologyFace& topf = m_topf[topfi];
    const ON_MeshFace& f = m_mesh->m_F[fi];
    if ( topf.m_topei[0] < 0 || topf.m_topei[0] >= tope_count )
      return false;
    if ( topf.m_topei[1] < 0 || topf.m_topei[1] >= tope_count )
      return false;
    if ( topf.m_topei[2] < 0 || topf.m_topei[2] >= tope_count )
      return false;
    if ( topf.m_topei[3] < 0 || topf.m_topei[3] >= tope_count )
      return false;
    tfvi[0] = m_topv_map[f.vi[0]];
    tfvi[1] = m_topv_map[f.vi[1]];
    tfvi[2] = m_topv_map[f.vi[2]];
    tfvi[3] = m_topv_map[f.vi[3]];
    if (    topf.m_topei[0] != 0 || topf.m_topei[1] != 0 
         || topf.m_topei[2] != 0 || topf.m_topei[3] != 0 ) {
      if ( !f.IsValid(v_count) )
        return false;
      if ( f.IsTriangle() ) {
        if (topf.m_topei[2] != topf.m_topei[3] )
          return false;
        jmax = 3;
      }
      else {
        if (topf.m_topei[2] == topf.m_topei[3] )
          return false;
        jmax = 4;
      }
      for ( j = 0; j < jmax; j++ ) {
        const ON_MeshTopologyEdge& tope = m_tope[topf.m_topei[j]];
        for ( k = 0; k < tope.m_topf_count; k++ ) {
          if ( tope.m_topfi[k] == topfi )
            break;
        }
        if ( k >= tope.m_topf_count )
          return false;

        // topedge m_tope[topf.m_topei[j]] ENDS at topvtx m_topv[tfvi[j]]
        if ( topf.m_reve[j] ) {
          if ( tope.m_topvi[1] != tfvi[(j+3)%4] )
            return false;
          if ( tope.m_topvi[0] != tfvi[j] )
            return false;
        }
        else {
          if ( tope.m_topvi[0] != tfvi[(j+3)%4] )
            return false;
          if ( tope.m_topvi[1] != tfvi[j] )
            return false;
        }
      }      
    }
    else {
      // all topf.m_topei[] must be zeros
      if (    topf.m_topei[0] != 0 || topf.m_topei[1] != 0 
           || topf.m_topei[2] != 0 || topf.m_topei[3] != 0 )
        return false;
    }
  }
  return true;
}

void ON_MeshTopology::Dump( ON_TextLog& dump ) const
{
  ON_3fPoint p;
  int vi, ei, fi, j;
  const int topv_count = m_topv.Count();
  const int tope_count = m_tope.Count();
  const int topf_count = m_topf.Count();

  // topological vertex information
  for ( vi = 0; vi < topv_count; vi++ ) {
    const ON_MeshTopologyVertex& v = m_topv[vi];
    dump.Print("topv %d: ",vi);
    if (m_mesh) {
      // dump geometric location of this vertex
      p = m_mesh->m_V[v.m_vi[0]];
      dump.Print("{%g,%g,%g} ", p.x, p.y, p.z);
    }

    // list all mesh geometry viertices that are coincident with this
    // topological vertex
    dump.Print("(");
    for ( j = 0; j < v.m_v_count; j++ ) {
      if ( j )
        dump.Print(",");
      dump.Print("m_V[%d]",v.m_vi[j]);
    }

    // list all toplogical edges that begin/end at this topological vertex
    dump.Print(") (");
    for ( j = 0; j < v.m_tope_count; j++ ) {
      if ( j )
        dump.Print(",");
      dump.Print("%d",v.m_topei[j]);
    }
    dump.Print(")\n");
  }

  // topological edge information
  for ( ei = 0; ei < tope_count; ei++ ) {
    const ON_MeshTopologyEdge& e = m_tope[ei];
    dump.Print("tope %d: topv%d to topvv%d (", ei, e.m_topvi[0], e.m_topvi[1] );
    // list all mesh topolical faces attached to this topolical edge
    for ( j = 0; j < e.m_topf_count; j++ ) {
      if (j)
        dump.Print(",");
      dump.Print("f%d",e.m_topfi[j]);
    }
    dump.Print(")\n");
  }

  // topological face information
  // mesh m_F[] index = mesh topology m_topf[] index
  for ( fi = 0; fi < topf_count; fi++ ) {
    const ON_MeshTopologyFace& f = m_topf[fi];
    dump.Print("topf %d: (",fi);
    for ( j = 0; j < 4; j++ ) {
      if ( j == 3 && f.m_topei[3] == f.m_topei[2] )
        break; // triangular face
      if (j)
        dump.Print(",");
      dump.Print("%ce%d",f.m_reve[j]?'-':'+',f.m_topei[j]);
    }
    dump.Print(")\n");
  }
}


bool ON_MeshTopology::Create()
{
  if ( !m_bIsValid ) {
    Destroy();

    // build vertex topology information
    const int fcount = m_mesh->FaceCount();
    const int vcount = m_mesh->VertexCount();
    int* vindex = GetIntArray(vcount);
    m_bIsValid = m_mesh->m_V.Sort( ON::quick_sort, vindex, compare3fPoint );
    if ( m_bIsValid ) {
      m_topv_map.SetCapacity( vcount );
      m_topv_map.SetCount( vcount );
      m_topv.SetCapacity( vcount );
      int vt0, vt1, topvi;
      ON_3fPoint p0;
      for (vt0 = 0; vt0 < vcount; vt0 = vt1) {
        topvi = m_topv.Count();
        ON_MeshTopologyVertex& topv = m_topv.AppendNew();
        topv.m_vi = vindex+vt0;
        p0 = m_mesh->m_V[vindex[vt0]];
        for ( vt1=vt0+1; vt1<vcount && 0==compare3fPoint(&p0,&m_mesh->m_V[vindex[vt1]]); vt1++ ) {
          // empty
        }
        topv.m_v_count = vt1-vt0;
        while ( vt0 < vt1 ) {
          m_topv_map[vindex[vt0++]] = topvi;
        }
      }
      m_topv.Shrink();
    }

    // build edge topology information
    const int topv_count = m_topv.Count();
    if ( m_bIsValid && topv_count >= 2 ) {
      bool rc = false;
      int ei, ecnt, fi, fvi, vi0, vi1, efi, topfvi[4];
      const int* vi;
      ON_MeshFace f;

      if ( fcount > 0 && vcount > 0 ) {
        ON_Workspace ws;
        struct TMPEDGE* e = (struct TMPEDGE*)ws.GetMemory( 4*fcount*sizeof(*e) );
        memset(e,0,4*fcount*sizeof(*e));
        ecnt = 0;
        for ( fi = 0; fi < fcount; fi++ ) {
          f = m_mesh->m_F[fi];

          // 5 August 2003 Dale Lear:
          //    I added this check to avoid passing
          //    bogus indices to m_topv_map[].  If faces 
          //    with invalid vi[] values make it past
          //    this point, you will crash on the m_topv_map[]
          //    lines.
          if ( !f.IsValid( vcount) )
            continue;

          f.vi[0] = m_topv_map[f.vi[0]];
          f.vi[1] = m_topv_map[f.vi[1]];
          f.vi[2] = m_topv_map[f.vi[2]];
          f.vi[3] = m_topv_map[f.vi[3]];
//	Greg Arden 29 April 2003.  Fixes TRR#9271.  The below block used to be in an else clause:  
//						          if ( f.IsValid(topv_count) ) 			
//								Since edges of the mesh could have been collapsed
//								it isn't right to expect the face to be valid after the m_topv_map has been applied
//								to the vertex indices.
          {			
            vi = f.vi;
            vi1 = vi[3];
            for ( fvi = 0; fvi < 4; fvi++ ) {
              vi0 = vi1;
              vi1 = vi[fvi];
              if ( vi0 == vi1 )
                continue;

              // 5 August 2003 Dale Lear:
              //   The test removed on 29 April prevented crashes when
              //   vi0 and vi1 were bogus.  I'm adding this test in
              //   to fix a crash reading fsv_r4.3dm.
              if (   vi0 < 0 || vi0 >= topv_count 
                  || vi1 < 0 || vi1 >= topv_count)
                continue;

              e[ecnt].fi = fi;
              if ( vi0 < vi1 ) {
                e[ecnt].vi[0] = vi0;
                e[ecnt++].vi[1] = vi1;
              }
              else if ( vi0 > vi1 ) {
                e[ecnt].vi[0] = vi1;
                e[ecnt++].vi[1] = vi0;
              }
            }
          }
        }
        if ( ecnt > 0 ) {
          rc = true;
          qsort( e, ecnt, sizeof(*e), (ON_COMPAR_LPVOID_LPVOID)ecomp );
          /*
          {
            int ii, kk = 0, jj = 0;
            int badsort = 0;
            for ( ii = 1; ii < ecnt; ii++ ) {
              if ( e[ii].vi[0] == 500 || e[ii-1].vi[0] == 500 )
                jj++;
              if ( e[ii].vi[0] >= e[ii].vi[1] )
                kk++;
              if ( ecomp( &e[ii-1], &e[ii] ) != -1 )
                badsort++;
            }
          }
          */
          int etop_count = 0;
          ei = 0;
          while( ei < ecnt ) {
            etop_count++;
            vi0 = e[ei].vi[0];
            vi1 = e[ei].vi[1];
            ei++;
            while (ei < ecnt && e[ei].vi[0] == vi0 && e[ei].vi[1] == vi1 )
              ei++;
          }
          m_tope.SetCapacity(etop_count);
          int* efindex = GetIntArray(ecnt);
          for ( ei = 0; ei < ecnt; /*empty*/ ) {
            ON_MeshTopologyEdge& tope = m_tope.AppendNew();
            tope.m_topvi[0] = e[ei].vi[0];
            tope.m_topvi[1] = e[ei].vi[1];
            tope.m_topfi = efindex;
            tope.m_topf_count = 0;
            *efindex++ = e[ei].fi;
            tope.m_topf_count++;
            ei++;
            while( ei < ecnt && e[ei].vi[0] == tope.m_topvi[0] && e[ei].vi[1] == tope.m_topvi[1] ) {
              *efindex++ = e[ei].fi;
              tope.m_topf_count++;
              ei++;
            }
          }
          efindex = 0; // memory deallocated by ~ON_MeshTopology()

          // connect vertices to edges;
          ecnt = m_tope.Count();
          int* ve_count = (int*)onmalloc(topv_count*sizeof(*ve_count));
          memset( ve_count, 0, topv_count*sizeof(*ve_count));
          for ( ei = 0; ei < ecnt; ei++ ) {
            ON_MeshTopologyEdge& tope = m_tope[ei];
            ve_count[tope.m_topvi[0]]++;
            ve_count[tope.m_topvi[1]]++;
          }
          int* vei = GetIntArray(2*ecnt);
          for ( vi0 = 0; vi0 < topv_count; vi0++ ) {
            ON_MeshTopologyVertex& topv = m_topv[vi0];
            if ( ve_count[vi0] > 0 ) {
              topv.m_topei = vei;
              vei += ve_count[vi0];
            }
          }
          onfree(ve_count); ve_count = 0;
          vei = 0; // memory deallocated by ~ON_MeshTopology()
          for ( ei = 0; ei < ecnt; ei++ ) {
            ON_MeshTopologyEdge& tope = m_tope[ei];
            ON_MeshTopologyVertex& topv0 = m_topv[tope.m_topvi[0]];
            ON_MeshTopologyVertex& topv1 = m_topv[tope.m_topvi[1]];
            vei = const_cast<int*>(topv0.m_topei);
            vei[topv0.m_tope_count++] = ei;
            vei = const_cast<int*>(topv1.m_topei);
            vei[topv1.m_tope_count++] = ei;
          }


          // build face topology information
          m_topf.SetCapacity(fcount);
          m_topf.SetCount(fcount);
          memset( m_topf.Array(), 0, fcount*sizeof(ON_MeshTopologyFace) );
          for ( fi = 0; fi < fcount; fi++ ) {
            ON_MeshTopologyFace& f = m_topf[fi];
            f.m_topei[0] = -1;
            f.m_topei[1] = -1;
            f.m_topei[2] = -1;
            f.m_topei[3] = -1;
          }
          for ( ei = 0; ei < ecnt; ei++ ) {
            ON_MeshTopologyEdge& tope = m_tope[ei];
            for (efi = 0; efi < tope.m_topf_count; efi++ ) {
              // Because ON_MeshFace.vi[2] == ON_MeshFace.vi[3] for triangles,
              // we have topf.m_topei[j] BEGIN at ON_MeshFace.vi[(j+3)%4] and END at ON_MeshFace.vi[j]
              fi = tope.m_topfi[efi];
              f = m_mesh->m_F[fi];
              topfvi[0] = m_topv_map[f.vi[0]];
              topfvi[1] = m_topv_map[f.vi[1]];
              topfvi[2] = m_topv_map[f.vi[2]];
              topfvi[3] = m_topv_map[f.vi[3]];
              ON_MeshTopologyFace& topf = m_topf[fi];
              vi0 = tope.m_topvi[0];
              vi1 = tope.m_topvi[1];
              // unroll loop for speed
              if      ( vi0 == topfvi[3] && vi1 == topfvi[0] ) {
                topf.m_topei[0] = ei;
                topf.m_reve[0] = 0;
              }
              else if ( vi0 == topfvi[0] && vi1 == topfvi[1] ) {
                topf.m_topei[1] = ei;
                topf.m_reve[1] = 0;
              }
              else if ( vi0 == topfvi[1] && vi1 == topfvi[2] ) {
                topf.m_topei[2] = ei;
                topf.m_reve[2] = 0;
              }
              else if ( vi0 == topfvi[2] && vi1 == topfvi[3] ) {
                topf.m_topei[3] = ei;
                topf.m_reve[3] = 0;
              }
              else if ( vi1 == topfvi[3] && vi0 == topfvi[0] ) {
                topf.m_topei[0] = ei;
                topf.m_reve[0] = 1;
              }
              else if ( vi1 == topfvi[0] && vi0 == topfvi[1] ) {
                topf.m_topei[1] = ei;
                topf.m_reve[1] = 1;
              }
              else if ( vi1 == topfvi[1] && vi0 == topfvi[2] ) {
                topf.m_topei[2] = ei;
                topf.m_reve[2] = 1;
              }
              else if ( vi1 == topfvi[2] && vi0 == topfvi[3] ) {
                topf.m_topei[3] = ei;
                topf.m_reve[3] = 1;
              }
            }
          }
          for ( fi = 0; fi < fcount; fi++ ) {
            ON_MeshTopologyFace& f = m_topf[fi];
            bool bIsGood = false;
            if (    f.m_topei[0] >= 0 && f.m_topei[1] >= 0 && f.m_topei[2] >=0 
                 && f.m_topei[0] != f.m_topei[1] 
                 && f.m_topei[1] != f.m_topei[2] 
                 && f.m_topei[2] != f.m_topei[0] 
                 ) {
              if ( m_mesh->m_F[fi].IsTriangle() ) {
                bIsGood = true;
                f.m_topei[3] = f.m_topei[2];
              }
              else if (   f.m_topei[3] >= 0 
                       && f.m_topei[0] != f.m_topei[3] 
                       && f.m_topei[1] != f.m_topei[3] 
                       && f.m_topei[2] != f.m_topei[3] ) {
                bIsGood = true;
              }
            }
            if ( !bIsGood ) {
              f.m_topei[0] = 0;
              f.m_topei[1] = 0;
              f.m_topei[2] = 0;
              f.m_topei[3] = 0;
              f.m_reve[0] = 0;
              f.m_reve[1] = 0;
              f.m_reve[2] = 0;
              f.m_reve[3] = 0;
            }
          }

        }
      }
    }
  }

  if ( !m_bIsValid )
    Destroy();
  return m_bIsValid;
}

struct tagFPT
{
  int x, y, z;
};

static int compare_fpt( const struct tagFPT* a, const struct tagFPT* b )
{
  if ( a->x < b->x )
    return -1;
  if ( a->x > b->x )
    return 1;
  if ( a->y < b->y )
    return -1;
  if ( a->y > b->y )
    return 1;
  if ( a->z < b->z )
    return -1;
  if ( a->z > b->z )
    return 1;
  return 0;
}

static int compare_pmark( const int* a, const int* b )
{
  if ( *a < *b )
    return -1;
  if ( *a > *b )
    return 1;
  if ( a < b )
    return -1;
  if ( a > b )
    return 1;
  return 0;
}

static int compare_vmap( const void* a, const void* b )
{
  int i = *((int*)a);
  int j = *((int*)b);
  if ( i < j )
    return -1;
  if ( i > j )
    return 1;
  return 0;
}

static int DupVertex( ON_Mesh* mesh, int vi )
{
  int vertex_count = mesh->m_V.Count();
  ON_3fVector n;
  ON_3fPoint v;
  ON_Color c;
  ON_2fPoint t;
  ON_SurfaceCurvature k;
  v = mesh->m_V[vi];
  mesh->m_V.Append(v);
  if (mesh->m_N.Count() == vertex_count) {
    n = mesh->m_N[vi];
    mesh->m_N.Append(n);
  }
  if (mesh->m_T.Count() == vertex_count) {
    t = mesh->m_T[vi];
    mesh->m_T.Append(t);
  }
  if (mesh->m_K.Count() == vertex_count) {
    k = mesh->m_K[vi];
    mesh->m_K.Append(k);
  }
  if (mesh->m_C.Count() == vertex_count) {
    c = mesh->m_C[vi];
    mesh->m_C.Append(c);
  }
  return vertex_count;
}


static int AddToPartition( ON_Mesh* mesh, ON_SimpleArray<int>& pmark, int vi, int partition_mark, int fi0 )
{
  bool b = true;
  int i, fi, new_vi, face_count, *fvi;
  i = pmark[vi];
  if ( !i ) {
    pmark[vi] = partition_mark;
  }
  else if ( i != partition_mark && i != partition_mark-1 ) {
    if ( i == partition_mark-2 )
      pmark[vi] = partition_mark-1; // vertex vi shared between two partitions
    else {
      new_vi = DupVertex(mesh,vi);
      face_count = mesh->m_F.Count();
      for ( fi = fi0; fi < face_count; fi++ ) {
        fvi = mesh->m_F[fi].vi;
        if ( fvi[0] == vi )
          fvi[0] = new_vi;
        if ( fvi[1] == vi )
          fvi[2] = new_vi;
        if ( fvi[2] == vi )
          fvi[2] = new_vi;
        if ( fvi[3] == vi )
          fvi[3] = new_vi;
      }
      pmark.Append(partition_mark);
    }
  }
  else
    b = false; // vertex already in this partition
  return b;
}

bool ON_MeshPartition_IsValid( const ON_MeshPartition& p, const ON_Mesh& mesh )
{
  bool rc = false;
  const int* fvi;
  int j, tcnt, fi, parti, partcount;
  partcount = p.m_part.Count();
  rc = ( partcount > 0 );
  if ( p.m_partition_max_triangle_count < 1 )
    rc = false;
  if ( p.m_partition_max_vertex_count < 3 )
    rc = false;
  for ( parti = 0; parti < partcount && rc; parti++ ) {
    const ON_MeshPart& part = p.m_part[parti];
    if ( part.triangle_count < 1 )
      rc = false;
    if ( part.vertex_count < 1 )
      rc = false;
    if ( part.vertex_count != part.vi[1] - part.vi[0] )
      rc = false;
    tcnt = 0;
    for ( fi = part.fi[0]; fi < part.fi[1]; fi++ ) {
      fvi = mesh.m_F[fi].vi;
      tcnt++;
      if ( fvi[2] != fvi[3] )
        tcnt++;
      for ( j = 0; j < 4; j++ ) {
        if ( fvi[j] < part.vi[0] || fvi[j] >= part.vi[1] )
          rc = false;
      }
    }
    if ( tcnt != part.triangle_count )
      rc = false;
    if ( parti ) {
      if ( part.fi[0] != p.m_part[parti-1].fi[1] )
        rc = false;
      if ( part.vi[0] > p.m_part[parti-1].vi[1] )
        rc = false;
    }
  }
  if ( partcount ) {
    if ( p.m_part[0].fi[0] != 0 || p.m_part[partcount-1].fi[1] != mesh.m_F.Count() )
      rc = false;
  }
  return rc;
}

const ON_MeshPartition* ON_Mesh::CreatePartition( 
              int partition_max_vertex_count, // maximum number of vertices in a partition
              int partition_max_triangle_count // maximum number of triangles in a partition
              )
{
  ON_Workspace ws;
  bool bNeedFaceSort = true;
  if ( m_partition ) 
  {
    bNeedFaceSort = false;
    if (   m_partition->m_partition_max_triangle_count > partition_max_triangle_count
        || m_partition->m_partition_max_vertex_count > partition_max_vertex_count )
        DestroyPartition();
  }
  if ( !m_partition ) 
  {
    // TODO: create one
    struct ON_MeshPart p;
    int vertex_count = VertexCount();
    const int face_count = FaceCount();
    const int triangle_count = TriangleCount() + 2*QuadCount();
    m_partition = new ON_MeshPartition();
    int k = triangle_count/partition_max_triangle_count;
    if ( k < vertex_count/partition_max_vertex_count )
      k = vertex_count/partition_max_vertex_count;
    k++;
    m_partition->m_part.Reserve(k);
    if ( vertex_count   <= partition_max_vertex_count && 
         triangle_count <= partition_max_triangle_count ) 
    {
      m_partition->m_partition_max_vertex_count = vertex_count;
      m_partition->m_partition_max_triangle_count = triangle_count;
      memset(&p,0,sizeof(p));
      p.vi[0] = 0;
      p.vi[1] = vertex_count;
      p.fi[0] = 0;
      p.fi[1] = face_count;
      p.vertex_count = vertex_count;
      p.triangle_count = triangle_count;
      m_partition->m_part.Append(p);
    }
    else 
    {
      int fi;
      int* fvi;
      DestroyTopology();

      // sort faces
      int* fmap = (int*)ws.GetMemory( face_count*sizeof(fmap[0]) );
      ON_SimpleArray<struct tagFPT> fpt(face_count);
      fpt.SetCount(face_count);
      if ( HasTextureCoordinates() )
      {
        ON_2fPoint fcenter;
        ON_BoundingBox bbox = ON_PointListBoundingBox(2,0,m_T.Count(),2,&m_T[0].x);
        const ON_Interval txdom(bbox.m_min.x,bbox.m_max.x);
        const ON_Interval tydom(bbox.m_min.y,bbox.m_max.y);
        for ( fi = 0; fi < face_count; fi++ ) {
          fvi = m_F[fi].vi;
          if ( fvi[2] == fvi[3] ) {
            fcenter = 0.333333333333333333f*(m_T[fvi[0]] + m_T[fvi[1]] + m_T[fvi[2]]);
          }
          else {
            fcenter = 0.25f*(m_T[fvi[0]] + m_T[fvi[1]] + m_T[fvi[2]] + m_T[fvi[3]]);
          }
          fpt[fi].x = (int)floor(txdom.NormalizedParameterAt(fcenter.x)*100);
          fpt[fi].y = (int)floor(tydom.NormalizedParameterAt(fcenter.y)*100);
          fpt[fi].z = 0;
        }
      }
      else
      {
        ON_3dPoint fcenter;
        ON_BoundingBox bbox = BoundingBox();
        const ON_Interval vxdom(bbox.m_min.x,bbox.m_max.x);
        const ON_Interval vydom(bbox.m_min.y,bbox.m_max.y);
        const ON_Interval vzdom(bbox.m_min.z,bbox.m_max.z);
        for ( fi = 0; fi < face_count; fi++ ) {
          fvi = m_F[fi].vi;
          if ( fvi[2] == fvi[3] ) {
            fcenter = 0.333333333333333333f*(m_V[fvi[0]] + m_V[fvi[1]] + m_V[fvi[2]]);
          }
          else {
            fcenter = 0.25f*(m_V[fvi[0]] + m_V[fvi[1]] + m_V[fvi[2]] + m_V[fvi[3]]);
          }
          fpt[fi].x = (int)floor(vxdom.NormalizedParameterAt(fcenter.x)*100);
          fpt[fi].y = (int)floor(vydom.NormalizedParameterAt(fcenter.y)*100);
          fpt[fi].z = (int)floor(vzdom.NormalizedParameterAt(fcenter.z)*100);
        }
      }
      fpt.Sort( ON::heap_sort, fmap, compare_fpt ); 
      m_F.Permute( fmap );
      if ( m_FN.Count() == face_count )
        m_FN.Permute( fmap );

      // sort vertices
      ON_SimpleArray<int>pmark(2*vertex_count);
      pmark.SetCount(vertex_count);
      pmark.Zero();
      int fi0, fi1, partition_mark, partition_vertex_count, partition_triangle_count;
      fi1 = 0;
      fi0 = 0;
      for ( partition_mark = 3, fi0 = 0; fi0 < face_count; fi0 = fi1, partition_mark += 2 ) 
      {
        partition_vertex_count = 0;
        partition_triangle_count = 0;
        for ( fi1 = fi0; 
              fi1 < face_count
              && partition_triangle_count+2 <= partition_max_triangle_count
              && partition_vertex_count+4 <= partition_max_vertex_count;
              fi1++ ) 
        {
          fvi = m_F[fi1].vi;
          partition_triangle_count++;
          if ( AddToPartition( this, pmark, fvi[0], partition_mark, fi ) )
            partition_vertex_count++;
          if ( AddToPartition( this, pmark, fvi[1], partition_mark, fi ) )
            partition_vertex_count++;
          if ( AddToPartition( this, pmark, fvi[2], partition_mark, fi ) )
            partition_vertex_count++;
          if ( fvi[2] != fvi[3] ) {
            partition_triangle_count++; // quads = 2 triangles
            if ( AddToPartition( this, pmark, fvi[3], partition_mark, fi ) )
              partition_vertex_count++;
          }
        }
        if ( fi0 < fi1 ) {
          struct ON_MeshPart p;
          memset(&p,0,sizeof(p));
          p.fi[0] = fi0;
          p.fi[1] = fi1;
          p.vertex_count = partition_vertex_count;
          p.triangle_count = partition_triangle_count;
          m_partition->m_part.Append(p);
        }
        if ( partition_triangle_count > m_partition->m_partition_max_triangle_count )
          m_partition->m_partition_max_triangle_count = partition_triangle_count;
        if ( partition_vertex_count > m_partition->m_partition_max_vertex_count )
          m_partition->m_partition_max_vertex_count = partition_vertex_count;
      }

      // the calls to AddToPartition() may have increased vertex count
      vertex_count = m_V.Count();

      // sort vertices
      int* vmap = (int*)ws.GetMemory( vertex_count*sizeof(vmap[0]) );
      pmark.Sort( ON::heap_sort, vmap, compare_pmark );
      m_V.Permute( vmap );
      if ( m_N.Count() == vertex_count )
        m_N.Permute( vmap );
      if ( m_T.Count() == vertex_count )
        m_T.Permute( vmap );
      if ( m_K.Count() == vertex_count )
        m_K.Permute( vmap );
      if ( m_C.Count() == vertex_count )
        m_C.Permute( vmap );
      pmark.Permute( vmap );
      // pamv[] = inverse of mapv permutation
      int* pamv = (int*)ws.GetMemory( vertex_count*sizeof(pamv[0]) );
      ON_Sort( ON::heap_sort, pamv, vmap, vertex_count, sizeof(vmap[0]), compare_vmap );
      for ( fi = 0; fi < face_count; fi++ ) {
        fvi = m_F[fi].vi;
        fvi[0] = pamv[fvi[0]];
        fvi[1] = pamv[fvi[1]];
        fvi[2] = pamv[fvi[2]];
        fvi[3] = pamv[fvi[3]];
      }

      // fill in m_part.vi[]
      int m, pi, partition_count = m_partition->m_part.Count();
      int vi0, vi1, vi2, vi3;
      for (vi2 = 0; vi2 < vertex_count && pmark[vi2]<2; vi2++)
      {/*empty for body*/}
      vi3=vi2;
      for ( pi = 0; pi < partition_count; pi++ ) {
        vi0 = vi2;
        vi1 = vi3;
        m = 2*pi + 4;
        for ( vi2 = vi3; vi2 < vertex_count && pmark[vi2] <  m; vi2++) 
        {/*empty for body*/}
        for ( vi3 = vi2; vi3 < vertex_count && pmark[vi3] <= m; vi3++) 
        {/*empty for body*/}
        m_partition->m_part[pi].vi[0] = vi0;
        m_partition->m_part[pi].vi[1] = vi3;
      }
    }
    // debugging - test partition
    if ( m_partition && !ON_MeshPartition_IsValid( *m_partition, *this ) ) {
      delete m_partition;
      m_partition = 0;
    }
  }

  return m_partition;
}

const ON_MeshPartition* ON_Mesh::Partition() const
{
  return m_partition;
}

void ON_Mesh::DestroyPartition()
{
  if ( m_partition ) {
    delete m_partition;
    m_partition = 0;
  }
}

ON_MeshPartition::ON_MeshPartition()
{
  m_partition_max_vertex_count = 0;
  m_partition_max_triangle_count = 0;
  m_part = 0;
}

ON_MeshPartition::~ON_MeshPartition()
{
  m_part.Destroy();
}

ON_OBJECT_IMPLEMENT(ON_MeshVertexRef,ON_Geometry,"C547B4BD-BDCD-49b6-A983-0C4A7F02E31A");

ON_OBJECT_IMPLEMENT(ON_MeshEdgeRef,ON_Geometry,"ED727872-463A-4424-851F-9EC02CB0F155");

ON_OBJECT_IMPLEMENT(ON_MeshFaceRef,ON_Geometry,"4F529AA5-EF8D-4c25-BCBB-162D510AA280");

ON_MeshVertexRef::ON_MeshVertexRef()
{
  m_mesh = 0;
  m_mesh_vi = -1;
  m_top_vi = -1;
}

ON_MeshVertexRef::~ON_MeshVertexRef()
{
  m_mesh = 0;
  m_mesh_vi = -1;
  m_top_vi = -1;
}

ON_MeshVertexRef& ON_MeshVertexRef::operator=(const ON_MeshVertexRef& src)
{
  if ( this != &src )
  {
    ON_Geometry::operator=(src);
    m_mesh = src.m_mesh;
    m_mesh_vi = src.m_mesh_vi;
    m_top_vi = src.m_top_vi;
  }
  return *this;
}


ON_BOOL32 ON_MeshVertexRef::IsValid( ON_TextLog* text_log ) const
{
  if ( 0 == m_mesh )
  {
    if ( 0 != text_log )
    {
      text_log->Print("m_mesh = NULL\n");
    }
    return false;
  }

  if ( -1 != m_mesh_vi )
  {
    if ( m_mesh_vi < 0 || m_mesh_vi >= m_mesh->m_V.Count() )
    {
      if ( 0 != text_log )
      {
        text_log->Print("m_mesh_vi = %d (should have 0 <= m_mesh_vi < %d)\n",m_mesh_vi,m_mesh->m_V.Count());
      }
      return false;
    }
  }
  else if ( -1 == m_top_vi )
  {
    if ( 0 != text_log )
    {
      text_log->Print("m_mesh_vi = -1 and m_top_vi = -1\n");
    }
    return false;
  }

  if ( -1 != m_top_vi )
  {
    const ON_MeshTopology* top = MeshTopology();
    if ( 0 == top )
    {
      if ( 0 != text_log )
      {
        text_log->Print("m_top_vi = %d and MeshTopology()=NULL\n",m_top_vi);
      }
      return false;
    }
    if ( m_top_vi < 0 || m_top_vi >= top->m_tope.Count() )
    {
      if ( 0 != text_log )
      {
        text_log->Print("m_top_vi = %d (should have 0 <= m_top_vi < %d)\n",m_top_vi,top->m_topv.Count());
      }
      return false;
    }

    if ( -1 != m_mesh_vi )
    {
      const ON_MeshTopologyVertex& topv = top->m_topv[m_top_vi];
      int i;
      for ( i = 0; i < topv.m_v_count; i++ )
      {
        if ( topv.m_vi[i] == m_mesh_vi )
          break;
      }
      if ( i >= topv.m_v_count )
      {
        if ( 0 != text_log )
        {
          text_log->Print("m_mesh_vi=%d is not in m_top->m_topv[m_top_vi=%d].m_vi[] array.\n",
                          m_mesh_vi,m_top_vi);
          
        }
        return false;
      }
    }
  }

  return true;
}

void ON_MeshVertexRef::Dump( ON_TextLog& text_log ) const
{
  text_log.Print("m_mesh=%08x m_mesh_vi=%d m_top_vi=%d\n",
                  m_mesh,m_mesh_vi,m_top_vi);
  ON_3dPoint v = Point();
  if ( v.IsValid() )
  {
    text_log.PushIndent();
    text_log.Print("Location: ");
    text_log.Print(v);
    text_log.Print("\n");
    text_log.PopIndent();
  }

}
unsigned int ON_MeshVertexRef::SizeOf() const
{
  unsigned int sz = sizeof(*this) - sizeof(ON_Geometry);
  sz += ON_Geometry::SizeOf();
  return sz;
}

ON::object_type ON_MeshVertexRef::ObjectType() const
{
  return ON::meshvertex_object;
}

// overrides of virtual ON_Geometry functions
int ON_MeshVertexRef::Dimension() const
{
  return 3;
}

ON_BOOL32 ON_MeshVertexRef::GetBBox(
       double* boxmin,
       double* boxmax,
       ON_BOOL32 bGrowBox
       ) const
{
  bool rc = false;
  ON_3dPoint v = Point();
  if ( v.IsValid() )
  {
    rc = ON_GetPointListBoundingBox( 3, 0, 1, 3, &v.x, boxmin, boxmax, bGrowBox?true:false );
  }
  return rc;
}

ON_BOOL32 ON_MeshVertexRef::Transform( 
       const ON_Xform& xform
       )
{
  return false;
}

const ON_MeshTopology* ON_MeshVertexRef::MeshTopology() const
{
  return (0 != m_mesh) ? &m_mesh->m_top : 0;
}

ON_3dPoint ON_MeshVertexRef::Point() const
{
  ON_3dPoint v = ON_UNSET_POINT;
  if ( 0 != m_mesh )
  {
    int vi = m_mesh_vi;
    if ( -1 == vi && m_top_vi >= 0 && m_top_vi < m_mesh->m_top.m_topv.Count() )
    {
      const ON_MeshTopologyVertex& topv = m_mesh->m_top.m_topv[m_top_vi];
      if ( topv.m_v_count > 0 )
        vi = topv.m_vi[0];
    }
    if ( vi >= 0 && vi < m_mesh->m_V.Count() )
      v = m_mesh->m_V[vi];
  }
  return v;
}

const ON_MeshTopologyVertex* ON_MeshVertexRef::MeshTopologyVertex() const
{
  const ON_MeshTopologyVertex* topv = 0;
  if ( 0 != m_mesh && m_top_vi >= 0 && m_top_vi < m_mesh->m_top.m_topv.Count() )
  {
    topv = &m_mesh->m_top.m_topv[m_top_vi];
  }
  return topv;
}

ON_MeshEdgeRef::ON_MeshEdgeRef()
{
  m_mesh = 0;
  m_top_ei = -1;
}

ON_MeshEdgeRef::~ON_MeshEdgeRef()
{
  m_mesh = 0;
  m_top_ei = -1;
}

ON_MeshEdgeRef& ON_MeshEdgeRef::operator=(const ON_MeshEdgeRef& src)
{
  if ( this != &src )
  {
    ON_Geometry::operator=(src);
    m_mesh = src.m_mesh;
    m_top_ei = src.m_top_ei;
  }
  return *this;
}


ON_BOOL32 ON_MeshEdgeRef::IsValid( ON_TextLog* text_log ) const
{
  if ( 0 == m_mesh)
  {
    if ( 0 != text_log )
    {
      text_log->Print("m_mesh = NULL\n");
    }
    return false;
  }

  if ( m_top_ei < 0 || m_top_ei >= m_mesh->m_top.m_tope.Count() )
  {
    if ( 0 != text_log )
    {
      text_log->Print("m_top_ei = %d (should have 0 <= m_top_ei < %d)\n",m_top_ei,m_mesh->m_top.m_tope.Count());
    }
    return false;
  }

  return true;
}

void ON_MeshEdgeRef::Dump( ON_TextLog& text_log ) const
{
  text_log.Print("m_mesh=%08x, m_top_ei=%d\n",m_mesh,m_top_ei);
  ON_Line line = Line();
  if ( line.from.IsValid() )
  {
    text_log.PushIndent();
    text_log.Print("Location: ");
    text_log.Print(line.from);
    text_log.Print(" to ");
    text_log.Print(line.to);
    text_log.Print("\n");
    text_log.PopIndent();
  }

}
unsigned int ON_MeshEdgeRef::SizeOf() const
{
  unsigned int sz = sizeof(*this) - sizeof(ON_Geometry);
  sz += ON_Geometry::SizeOf();
  return sz;
}

ON::object_type ON_MeshEdgeRef::ObjectType() const
{
  return ON::meshedge_object;
}

// overrides of virtual ON_Geometry functions
int ON_MeshEdgeRef::Dimension() const
{
  return 3;
}

ON_BOOL32 ON_MeshEdgeRef::GetBBox(
       double* boxmin,
       double* boxmax,
       ON_BOOL32 bGrowBox
       ) const
{
  bool rc = false;
  ON_Line line = Line();
  if ( line.from.IsValid() && line.to.IsValid() )
  {
    rc = ON_GetPointListBoundingBox( 3, 0, 2, 3, &line.from.x, boxmin, boxmax, bGrowBox?true:false );
  }
  return rc;
}

ON_BOOL32 ON_MeshEdgeRef::Transform( 
       const ON_Xform& xform
       )
{
  return false;
}

const ON_MeshTopology* ON_MeshEdgeRef::MeshTopology() const
{
  return (0 != m_mesh) ? &m_mesh->m_top : 0;
}

ON_Line ON_MeshEdgeRef::Line() const
{
  ON_Line line(ON_UNSET_POINT,ON_UNSET_POINT);
  const ON_MeshTopologyEdge* tope = MeshTopologyEdge();
  if ( 0 != tope )
  {
    ON_MeshVertexRef vr;
    vr.m_mesh = m_mesh;
    vr.m_top_vi = tope->m_topvi[0];
    line.from = vr.Point();
    if ( line.from.IsValid() )
    {
      vr.m_top_vi = tope->m_topvi[1];
      line.to = vr.Point();
      if ( !line.to.IsValid() )
        line.from = ON_UNSET_POINT;
    }
  }
  return line;
}

const ON_MeshTopologyEdge* ON_MeshEdgeRef::MeshTopologyEdge() const
{
  const ON_MeshTopologyEdge* tope = 0;
  if ( m_top_ei >= 0 )
  {
    const ON_MeshTopology* top = MeshTopology();
    if ( 0 != top && m_top_ei < top->m_tope.Count() )
      tope = &top->m_tope[m_top_ei];
  }
  return tope;
}

/////////////////////////////////////////////////


ON_MeshFaceRef::ON_MeshFaceRef()
{
  m_mesh = 0;
  m_mesh_fi = -1;
}

ON_MeshFaceRef::~ON_MeshFaceRef()
{
  m_mesh = 0;
  m_mesh_fi = -1;
}

ON_MeshFaceRef& ON_MeshFaceRef::operator=(const ON_MeshFaceRef& src)
{
  if ( this != &src )
  {
    ON_Geometry::operator=(src);
    m_mesh = src.m_mesh;
    m_mesh_fi = src.m_mesh_fi;
  }
  return *this;
}


ON_BOOL32 ON_MeshFaceRef::IsValid( ON_TextLog* text_log ) const
{
  if ( 0 == m_mesh)
  {
    if ( 0 != text_log )
    {
      text_log->Print("m_mesh = NULL\n");
    }
    return false;
  }

  if ( m_mesh_fi < 0 || m_mesh_fi >= m_mesh->m_F.Count() )
  {
    if ( 0 != text_log )
    {
      text_log->Print("m_mesh_fi = %d (should have 0 <= m_mesh_fi < %d)\n",m_mesh_fi,m_mesh->m_F.Count());
    }
    return false;
  }

  return true;
}

void ON_MeshFaceRef::Dump( ON_TextLog& text_log ) const
{
  text_log.Print("m_mesh=%08x, m_mesh_fi=%d\n",m_mesh,m_mesh_fi);
}

unsigned int ON_MeshFaceRef::SizeOf() const
{
  unsigned int sz = sizeof(*this) - sizeof(ON_Geometry);
  sz += ON_Geometry::SizeOf();
  return sz;
}

ON::object_type ON_MeshFaceRef::ObjectType() const
{
  return ON::meshface_object;
}

// overrides of virtual ON_Geometry functions
int ON_MeshFaceRef::Dimension() const
{
  return 3;
}

ON_BOOL32 ON_MeshFaceRef::GetBBox(
       double* boxmin,
       double* boxmax,
       ON_BOOL32 bGrowBox
       ) const
{
  bool rc = false;
  if ( 0 != m_mesh && m_mesh_fi >= 0 && m_mesh_fi < m_mesh->m_F.Count() )
  {
    const int vcnt = m_mesh->m_V.Count();
    const int* Fvi = m_mesh->m_F[m_mesh_fi].vi;
    ON_3dPoint v[4];
    //int count = 0;
    int i;
    for ( i = 0; i < 4; i++ )
    {
      if ( Fvi[i] >= 0 && Fvi[i] < vcnt )
      {
        v[i] = m_mesh->m_V[Fvi[i]];
      }
      else
        return false;
    }
    rc = ON_GetPointListBoundingBox( 3, 0, 4, 3, &v[0].x, boxmin, boxmax, bGrowBox?true:false );
  }
  return rc;
}

ON_BOOL32 ON_MeshFaceRef::Transform( 
       const ON_Xform& xform
       )
{
  return false;
}

const ON_MeshTopology* ON_MeshFaceRef::MeshTopology() const
{
  return (0 != m_mesh) ? &m_mesh->m_top : 0;
}



const ON_MeshFace* ON_MeshFaceRef::MeshFace() const
{
  const ON_MeshFace* f = 0;
  if ( 0 != m_mesh && m_mesh_fi >= 0 && m_mesh_fi < m_mesh->m_F.Count() )
  {
    f = &m_mesh->m_F[m_mesh_fi];
  }
  return f;
}


const ON_MeshTopologyFace* ON_MeshFaceRef::MeshTopologyFace() const
{
  const ON_MeshTopologyFace* topf = 0;
  if ( m_mesh_fi >= 0 )
  {
    const ON_MeshTopology* top = MeshTopology();
    if ( 0 != top && m_mesh_fi < top->m_topf.Count() )
      topf = &top->m_topf[m_mesh_fi];
  }
  return topf;
}

ON_MeshVertexRef ON_Mesh::VertexRef(int mesh_V_index) const
{
  ON_MeshVertexRef vr;
  if ( mesh_V_index >= 0 && mesh_V_index < m_V.Count() )
  {
    vr.m_mesh = this;
    vr.m_mesh_vi = mesh_V_index;
    if ( m_top.m_topv_map.Count() == m_V.Count() )
    {
      vr.m_top_vi = m_top.m_topv_map[mesh_V_index];
    }
  }

  return vr;
}

ON_MeshVertexRef ON_Mesh::VertexRef(ON_COMPONENT_INDEX ci) const
{
  ON_MeshVertexRef vr;

  //int vertex_index = -1;
  switch (ci.m_type)
  {
  case ON_COMPONENT_INDEX::meshtop_vertex:
    vr = m_top.VertexRef(ci);
    break;
  case ON_COMPONENT_INDEX::mesh_vertex:
    if ( ci.m_index >= 0 && ci.m_index < m_V.Count() )
    {
      vr.m_mesh = this;
      vr.m_mesh_vi = ci.m_index;
      if ( m_top.m_topv_map.Count() == m_V.Count() )
      {
        vr.m_top_vi = m_top.m_topv_map[vr.m_mesh_vi];
      }
    }
    break;
  default:
    // intentionally skipping other enum values (also quiets gcc warnings)
    break;
  }

  return vr;
}


ON_MeshEdgeRef ON_Mesh::EdgeRef(int tope_index) const
{
  return m_top.EdgeRef(tope_index);
}

ON_MeshEdgeRef ON_Mesh::EdgeRef(ON_COMPONENT_INDEX ci) const
{
  return m_top.EdgeRef(ci);
}

ON_MeshFaceRef ON_Mesh::FaceRef(int mesh_F_index) const
{
  ON_MeshFaceRef fr;

  if ( mesh_F_index >= 0 && mesh_F_index < m_F.Count() )
  {
    fr.m_mesh = this;
    fr.m_mesh_fi = mesh_F_index;
  }

  return fr;
}

ON_MeshFaceRef ON_Mesh::FaceRef(ON_COMPONENT_INDEX ci) const
{
  ON_MeshFaceRef fr;

  if ( ci.m_type == ON_COMPONENT_INDEX::mesh_face )
  {
    if ( ci.m_index >= 0 && ci.m_index < m_F.Count() )
    {
      fr.m_mesh = this;
      fr.m_mesh_fi = ci.m_index;
    }
  }

  return fr;
}

ON_MeshVertexRef ON_MeshTopology::VertexRef(ON_COMPONENT_INDEX ci) const
{
  ON_MeshVertexRef vr;
  if ( ci.m_index >= 0 )
  {
    switch(ci.m_type)
    {
    case ON_COMPONENT_INDEX::mesh_vertex:
      if (m_mesh)
      {
        vr = m_mesh->VertexRef(ci);
      }
      break;
    case ON_COMPONENT_INDEX::meshtop_vertex:
      if ( ci.m_index < m_topv.Count() )
      {
        vr.m_mesh = m_mesh;
        vr.m_top_vi = ci.m_index;
        if ( m_topv[ci.m_index].m_vi && 1 == m_topv[ci.m_index].m_v_count )
        {
          vr.m_mesh_vi = m_topv[ci.m_index].m_vi[0];
        }
      }
      break;
    default:
      // intentionally skip other ON_COMPONENT_INDEX::TYPE
      // enum values.
      break;
    }
  }
  return vr;  
}

ON_MeshVertexRef ON_MeshTopology::VertexRef(int topv_index) const
{
  ON_MeshVertexRef vr;

  if ( topv_index >= 0 && topv_index < m_topv.Count() )
  {
    vr.m_mesh = m_mesh;
    vr.m_top_vi = topv_index;
    if ( 1 == m_topv[topv_index].m_v_count )
    {
      vr.m_mesh_vi = m_topv[topv_index].m_vi[0];
    }
  }

  return vr;
}

ON_MeshEdgeRef ON_MeshTopology::EdgeRef(ON_COMPONENT_INDEX ci) const
{
  ON_MeshEdgeRef er;
  if ( ON_COMPONENT_INDEX::meshtop_edge == ci.m_type && ci.m_index >= 0 && ci.m_index < m_tope.Count() )
  {
    er.m_mesh = m_mesh;
    er.m_top_ei = ci.m_index;
  }
  return er;
}

ON_MeshEdgeRef ON_MeshTopology::EdgeRef(int tope_index) const
{
  ON_MeshEdgeRef er;

  if ( tope_index >= 0 && tope_index < m_tope.Count() )
  {
    er.m_mesh = m_mesh;
    er.m_top_ei = tope_index;
  }

  return er;
}

ON_MeshFaceRef ON_MeshTopology::FaceRef(ON_COMPONENT_INDEX ci) const
{
  ON_MeshFaceRef fr;
  if ( m_mesh )
  {
    fr = m_mesh->FaceRef(ci.m_index);
  }
  return fr;
}

ON_MeshFaceRef ON_MeshTopology::FaceRef(int topf_index) const
{
  return ( 0 != m_mesh ) ? m_mesh->FaceRef(topf_index) : ON_MeshFaceRef();
}


/*
ON_Mesh::COMPONENT_TYPE ON_Mesh::ComponentIndexType( int component_index )
{
  switch( (mesh_component_mask & component_index) )
  {
  case mesh_vertex:    return mesh_vertex;
  case meshtop_vertex: return meshtop_vertex;
  case meshtop_edge:   return meshtop_edge;
  case mesh_face:      return mesh_face;
  }
  return mesh_component_unset;
}
*/

ON_COMPONENT_INDEX ON_MeshVertexRef::ComponentIndex() const
{
  ON_COMPONENT_INDEX ci(ON_COMPONENT_INDEX::invalid_type,-1);
  if ( m_mesh_vi >= 0 )
  {
    if ( 0 == m_mesh || m_mesh_vi < m_mesh->m_V.Count() )
    {
      ci.Set(ON_COMPONENT_INDEX::mesh_vertex,m_mesh_vi);
    }
  }
  else if ( m_top_vi >= 0 )
  {
    if ( 0 == m_mesh )
    {
      // not enough information to perform validation check
      // assume this is a persistent index to be resolved
      // later and press on
      ci.Set(ON_COMPONENT_INDEX::meshtop_vertex,m_top_vi);
    }
    else if (m_top_vi < m_mesh->m_V.Count())
    {
      if ( 0 == m_mesh->m_top.m_topv.Count() )
      {
        // not enough information to perform validation check
        // assume this is a persistent index to be resolved
        // later and press on
        ci.Set(ON_COMPONENT_INDEX::meshtop_vertex,m_top_vi);
      }
      else if ( m_top_vi < m_mesh->m_top.m_topv.Count() )
      {
        // m_top is constructed and m_top_vi is valid
        ci.Set(ON_COMPONENT_INDEX::meshtop_vertex,m_top_vi);
      }
    }
  }
  return ci;
}

ON_COMPONENT_INDEX ON_MeshEdgeRef::ComponentIndex() const
{
  ON_COMPONENT_INDEX ci(ON_COMPONENT_INDEX::invalid_type,-1);
  if ( m_top_ei >= 0 )
  {
    if ( 0 == m_mesh || 0 == m_mesh->m_top.m_tope.Count() )
    {
      // not enough information to perform validation check
      // assume this is a persistent index to be resolved
      // later and press on
      ci.Set(ON_COMPONENT_INDEX::meshtop_edge,m_top_ei);
    }
    else if ( m_top_ei < m_mesh->m_top.m_tope.Count() )
    {
      // valid index
      ci.Set(ON_COMPONENT_INDEX::meshtop_edge,m_top_ei);
    }
  }
  return ci;
}

ON_COMPONENT_INDEX ON_MeshFaceRef::ComponentIndex() const
{
  ON_COMPONENT_INDEX ci(ON_COMPONENT_INDEX::invalid_type,-1);
  if ( m_mesh_fi >= 0 )
  {
    if ( 0 == m_mesh || m_mesh_fi < m_mesh->m_F.Count() )
    {
      ci.Set(ON_COMPONENT_INDEX::mesh_face,m_mesh_fi);
    }
  }
  return ci;
}

ON_Geometry* ON_Mesh::MeshComponent( 
  ON_COMPONENT_INDEX ci
  ) const
{
  ON_Geometry* component = 0;
  if ( ci.m_index >= 0 )
  {
    switch( ci.m_type )
    {
    case ON_COMPONENT_INDEX::mesh_vertex: 
      {
        ON_MeshVertexRef r = VertexRef(ci);
        component = new ON_MeshVertexRef(r);
      }
      break;

    case ON_COMPONENT_INDEX::meshtop_vertex:
      {
        ON_MeshVertexRef r = Topology().VertexRef(ci);
        component = new ON_MeshVertexRef(r);
      }
      break;

    case ON_COMPONENT_INDEX::meshtop_edge:
      {
        ON_MeshEdgeRef r = EdgeRef(ci);
        component = new ON_MeshEdgeRef(r);
      }
      break;

    case ON_COMPONENT_INDEX::mesh_face:
      {
        ON_MeshFaceRef r = FaceRef(ci);
        component = new ON_MeshFaceRef(r);
      }
      break;

    default:
      // intentionally skip other ON_COMPONENT_INDEX::TYPE
      // enum values.
      break;
    }
  }
  return component;
}

static
bool ON_ClosestPointToTriangleHelper( 
        ON_3dPoint A, ON_3dPoint B, ON_3dPoint C,
        ON_3dPoint P,
        double* a, double* b, double* c
        )
{
  double r,s,t,pr;
  bool rc = false;

  // The arithmetic is written out so this function can be as fast
  // as possible.  It is called often in closest point to mesh.
  ON_3dVector V(B.x-A.x,B.y-A.y,B.z-A.z);
  ON_3dVector W(C.x-A.x,C.y-A.y,C.z-A.z);
  ON_3dVector N(V.y*W.z - W.y*V.z, V.z*W.x - W.z*V.x, V.x*W.y - W.x*V.y );

  if ( N.Unitize() )
  {
    s = (A.x-P.x)*N.x + (A.y-P.y)*N.y + (A.z-P.z)*N.z;
    ON_3dPoint Q(P.x+s*N.x-A.x, P.y+s*N.y-A.y, P.z+s*N.z-A.z);

    if ( 2 == ON_Solve3x2(&V.x,&W.x,
                 Q.x,Q.y,Q.z,
                 &r,&s,&t,&pr))
    {
      *a = 1.0-r-s;
      *b = r;
      *c = s;
      rc = true;
    }
  }

  if ( !rc )
  {
    // degenerate triangle
    *a = 1.0;
    *b = 0.0;
    *c = 0.0;
  }

  return rc;
}



bool ON_ClosestPointToTriangle( 
        ON_3dPoint A, ON_3dPoint B, ON_3dPoint C,
        ON_3dPoint P,
        double* a, double* b, double* c
        )
{
  bool rc;
  int i;
  double x,y,z,d;

  // This function checks distances and calls
  // ON_ClosestPtToTriangleHelper() to insure that it returns exactly
  // the same answer for points all points.

  x = P.x-A.x;
  y = P.y-A.y;
  z = P.z-A.z;
  d = x*x + y*y + z*z;
  i = 0;

  x = P.x-B.x;
  y = P.y-B.y;
  z = P.z-B.z;
  x = x*x + y*y + z*z;
  if ( x < d )
  {
    i = 1;
    d = x;
  }

  x = P.x-C.x;
  y = P.y-C.y;
  z = P.z-C.z;
  x = x*x + y*y + z*z;
 
  if ( x < d )
  {
    // C is closest to P
    rc = ON_ClosestPointToTriangleHelper(C,A,B,P,c,a,b);
  }
  else if ( i )
  {
    // B is closest to P
    rc = ON_ClosestPointToTriangleHelper(B,C,A,P,b,c,a);
  }
  else
  {
    // A is closest to P
    rc = ON_ClosestPointToTriangleHelper(A,B,C,P,a,b,c);
  }
  return rc;
}

bool ON_ClosestPointToTriangleFast( const ON_3dPoint& R, 
                                    const ON_3dPoint& S, 
                                    const ON_3dPoint& T, 
                                    ON_3dPoint Q,
                                    double* r, double* s, double* t
                                    )
{
  // TODO: When I get time, I need to study the stability
  //       of this function and see if I want to use it
  //       instead of the slower one above.
  double a00, a01, a10, a11, b0, b1, ss, tt;

  const ON_3dPoint V0(R.x - T.x ,R.y - T.y, R.z - T.z);
  const ON_3dPoint V1(S.x - T.x ,S.y - T.y, S.z - T.z);

  Q.x -= T.x;
  Q.y -= T.y;
  Q.z -= T.z;

  a00 = V0.x*V0.x + V0.y*V0.y + V0.z*V0.z;
  if ( a00 <= 0.0 )
    return false;
  a00 = 1.0/ a00;

  a11 = V1.x*V1.x + V1.y*V1.y + V1.z*V1.z;
  if ( a11 <= 0.0 )
    return false;
  a11 = 1.0/a11;

  a01 = V0.x*V1.x + V0.y*V1.y + V0.z*V1.z;
  a10 = a01*a11;
  a01 *= a00;

  b0 = (V0.x*Q.x + V0.y*Q.y + V0.z*Q.z)*a00;
  b1 = (V1.x*Q.x + V1.y*Q.y + V1.z*Q.z)*a11;

  if ( a00 <= a11 )
  {
    a11 = 1.0 - a01*a10;
    if ( 0.0 == a11 )
      return false;
    tt = (b1 - a10*b0)/a11;
    ss = (b0 - a01*tt);
  }
  else
  {
    a00 = 1.0 - a01*a10;
    if ( 0.0 == a00 )
      return false;
    ss = (b0 - a01*b1)/a00;
    tt = (b1 - a10*ss);
  }

  *r = ss;
  *s = tt;
  *t = 1.0 - ss - tt;

  return true;
}

ON_3dVector ON_TriangleNormal(
        const ON_3dPoint& A,
        const ON_3dPoint& B,
        const ON_3dPoint& C
        )
{
  // N = normal to triangle's plane
  ON_3dVector N;
  double a, b, c, d;
  N.x = A.y*(B.z-C.z) + B.y*(C.z-A.z) + C.y*(A.z-B.z);
  N.y = A.z*(B.x-C.x) + B.z*(C.x-A.x) + C.z*(A.x-B.x);
  N.z = A.x*(B.y-C.y) + B.x*(C.y-A.y) + C.x*(A.y-B.y);

  a = fabs(N.x);
  b = fabs(N.y);
  c = fabs(N.z);
  if ( b > a )
  {
    if ( c > b )
    {
      // c is largest
      if ( c > ON_DBL_MIN )
      {
        d = 1.0/c; a *= d; b *= d; d = c*sqrt(1.0 + a*a + b*b);
      }
      else
      {
        d = c;
      }
    }
    else
    {
      if ( b > ON_DBL_MIN )
      {
        // b is largest
        d = 1.0/b; a *= d; c *= d; d = b*sqrt(1.0 + c*c + a*a);
      }
      else
      {
        d = b;
      }
    }
  }
  else if ( c > a )
  {
    // c is largest
    if ( c > ON_DBL_MIN )
    {
      d = 1.0/c; a *= d; b *= d; d = c*sqrt(1.0 + a*a + b*b);
    }
    else
    {
      d = c;
    }
  }
  else if ( a > ON_DBL_MIN )
  {
    d = 1.0/a; b *= d; c *= d; d = a*sqrt(1.0 + b*b + c*c);
  }
  else
  {
    d = a;
  }

  if ( d > 0.0 )
  {
    d = 1.0/d;
    N.x *= d; N.y *= d; N.z *= d;
  }

  return N;
}

int ON_LineTriangleIntersect(
        const ON_3dPoint& A,
        const ON_3dPoint& B,
        const ON_3dPoint& C,
        const ON_3dPoint& P,
        const ON_3dPoint& Q,
        double abc[2][3], 
        double t[2],
        double tol
        )
{
  ON_3dPoint L, T;
  ON_3dVector tri; // barycentric coords saved in tri
  double p, q, pdepth, qdepth;
  const ON_3dVector N( ON_TriangleNormal(A,B,C) );
  //const double d ( -(N.x*A.x + N.y*A.y + N.z*A.z) );
  pdepth = N.x*P.x + N.y*P.y + N.z+P.z;
  qdepth = N.x*Q.x + N.y*Q.y + N.z+Q.z;

  if ( tol < ON_ZERO_TOLERANCE )
    tol = ON_ZERO_TOLERANCE;

  if (fabs(pdepth) > tol || fabs(qdepth) > tol )
  {
    q = pdepth-qdepth;
    if ( fabs(q) <= ON_DBL_MIN )
    {
      // line is parallel to triangle's plane 
      return 0;
    }
    // infinte line intersects plane in a single point
    q = pdepth/q;
    if ( q < 0.0 )
    {
      // P is closest point to plane
      if ( fabs(pdepth) > tol )
      {
        // P is too far away
        return 0;
      }
      q = 0.0;
    }
    else if ( q > 1.0 )
    {
      // Q is closest point to plane
      if ( fabs(qdepth) > tol)
      {
        // Q is too far away
        return 0;
      }
      q = 1.0;
    }
    p = 1.0-q;

    L.x = p*P.x + q*Q.x; L.y = p*P.y + q*Q.y; L.z = p*P.z + q*Q.z;
    if ( !ON_ClosestPointToTriangleFast( A, B, C, L, &tri.x, &tri.y, &tri.z ) )
    {
      return 0;
    }
    bool bCheck = false;
    // do not clamp tri.* > 1.0 to 1.0 - let Unitize() call handle it.
    if ( tri.x < 0.0 ) { bCheck = true; tri.x = 0.0; } else if ( tri.x > 1.0 ) { bCheck = true; }
    if ( tri.y < 0.0 ) { bCheck = true; tri.y = 0.0; } else if ( tri.y > 1.0 ) { bCheck = true; }
    if ( tri.z < 0.0 ) { bCheck = true; tri.z = 0.0; } else if ( tri.z > 1.0 ) { bCheck = true; }
    if ( bCheck )
    {
      // point where line intersects plane is not inside triangle
      if ( !tri.Unitize() )
        return false;
      T.x = tri.x*A.x + tri.y*B.x + tri.z*C.x;
      T.y = tri.x*A.y + tri.y*B.y + tri.z*C.y;
      T.z = tri.x*A.z + tri.y*B.z + tri.z*C.z;
      
      qdepth = T.DistanceTo(L);

      // find point on line closest to T;
      ON_Line(P,Q).ClosestPointTo(T,&p);
      if ( p < 0.0 ) p = 0.0; else if ( p > 1.0 ) p = 1.0;
      pdepth = 1.0-p;
      L.x = pdepth*P.x + p*Q.x; L.y = pdepth*P.y + p*Q.y; L.z = pdepth*P.z + p*Q.z;
      pdepth = T.DistanceTo(L);
      if ( pdepth < qdepth )
      {
        q = p;
        qdepth = pdepth;
      }
      if ( qdepth > tol )
        return 0;
    }
    abc[0][0] = tri.x;
    abc[0][1] = tri.y;
    abc[0][2] = tri.z;
    t[0] = q;
    return 1;
  }

  // line lies in triangle's plane
  if ( !ON_ClosestPointToTriangleFast( A, B, C, P, &abc[0][0], &abc[0][1], &abc[0][2] ) )
  {
    return 0;
  }
  if ( !ON_ClosestPointToTriangleFast( A, B, C, Q, &abc[1][0], &abc[1][1], &abc[1][2] ) )
  {
    return 0;
  }

  // TODO - trim to edges of triangle
  t[0] = 0.0;
  t[1] = 1.0;
  return 2;
}

const bool* ON_Mesh::HiddenVertexArray() const
{
  return ( ( m_hidden_count > 0 && m_H.Count() == m_V.Count() ) ? m_H.Array() : 0);
}

void ON_Mesh::DestroyHiddenVertexArray()
{
  m_H.Destroy();
  m_hidden_count = 0;
}

int ON_Mesh::HiddenVertexCount() const
{
  return ((m_H.Count() == m_V.Count()) ? m_hidden_count : 0);
}

void ON_Mesh::SetVertexHiddenFlag( int meshvi, bool bHidden )
{
  const int vcount = m_V.Count();
  if ( meshvi >= 0 && meshvi < vcount )
  {
    if ( bHidden )
    {
      if ( vcount != m_H.Count() )
      {
        m_H.SetCapacity(vcount);
        m_H.SetCount(vcount);
        m_H.Zero();
        m_H[meshvi] = true;
        m_hidden_count = 1;
      }
      else if ( false == m_H[meshvi] )
      {
        m_H[meshvi] = true;
        m_hidden_count++;
      }
    }
    else
    {
      // show this vertex
      if ( m_hidden_count > 0 && vcount == m_H.Count() )
      {
        if  ( m_H[meshvi] )
        {
          m_H[meshvi] = false;
          m_hidden_count--;
          if ( 0 == m_hidden_count )
          {
            DestroyHiddenVertexArray();
          }
        }
      }
      else if ( m_hidden_count > 0 || m_H.Capacity() > 0 )
      {
        // if m_H exists, it is bogus.
        DestroyHiddenVertexArray();
      }
    }
  }
}


bool ON_Mesh::VertexIsHidden(int meshvi) const
{
  const int vcount = m_V.Count();
  return ((m_hidden_count > 0 && meshvi >= 0 && meshvi < vcount && vcount == m_H.Count()) 
          ? m_H[meshvi]
          : false);
}

bool ON_Mesh::FaceIsHidden(int meshfi) const
{
  const bool* bHiddenVertex = HiddenVertexArray();
  if ( bHiddenVertex && meshfi >= 0 && meshfi < m_F.Count() )
  {
    ON_MeshFace f = m_F[meshfi];
    if ( bHiddenVertex[f.vi[0]] || bHiddenVertex[f.vi[1]] || bHiddenVertex[f.vi[2]] || bHiddenVertex[f.vi[3]] )
      return true;
  }
  return false;
}

bool ON_MeshTopology::TopVertexIsHidden( int topvi ) const
{
  const bool* bHiddenVertex = m_mesh ? m_mesh->HiddenVertexArray() : 0;
  if ( bHiddenVertex && topvi >= 0 && topvi < m_topv.Count() )
  {
    const ON_MeshTopologyVertex& topv = m_topv[topvi];
    int i;
    for ( i = 0; i < topv.m_v_count; i++ )
    {
      if ( !bHiddenVertex[topv.m_vi[i]] )
        return false;
    }
    return true;
  }
  return false;
}

bool ON_MeshTopology::TopEdgeIsHidden( int topei ) const
{
  // ugly - but faster than calling TopVertexIsHidden()
  const bool* bHiddenVertex = m_mesh ? m_mesh->HiddenVertexArray() : 0;
  if ( bHiddenVertex && topei >= 0 && topei < m_tope.Count()  )
  {
    const ON_MeshTopologyEdge& tope = m_tope[topei];
    const ON_MeshTopologyVertex& topv0 = m_topv[tope.m_topvi[0]];
    const ON_MeshTopologyVertex& topv1 = m_topv[tope.m_topvi[1]];
    int i;

    for ( i = 0; i < topv0.m_v_count; i++ )
    {
      if ( !bHiddenVertex[topv0.m_vi[i]] )
        break;
    }
    if ( i >= topv0.m_v_count )
      return true;

    for ( i = 0; i < topv1.m_v_count; i++ )
    {
      if ( !bHiddenVertex[topv1.m_vi[i]] )
        return false;
    }

    return true;
  }
  return false;
}

bool ON_MeshTopology::TopFaceIsHidden( int topfi ) const
{
  // topology and mesh face indices are the same
  return m_mesh ? m_mesh->FaceIsHidden(topfi) : false;
}


ON_MappingTag::ON_MappingTag()
{
  Default();
}

void ON_MappingTag::Dump( ON_TextLog& text_log ) const
{
  text_log.Print("Texture/color coordinates tag:\n");
  text_log.PushIndent();
  text_log.Print("mapping id: "); text_log.Print(m_mapping_id); text_log.Print("\n");
  text_log.Print("mapping crc: %08x\n",m_mapping_crc);
  text_log.Print("mesh xform:\n");
  text_log.PushIndent(); text_log.Print(m_mesh_xform); text_log.PopIndent();
  text_log.PushIndent();
  text_log.Print(m_mesh_xform);
  text_log.PopIndent();
  text_log.PopIndent();
}

void ON_MappingTag::Transform( const ON_Xform& xform )
{
  if ( !ON_UuidIsNil(m_mapping_id) )
  {
    // Update mapping transformation.
    // Do NOT change the value of m_mapping_crc.
    m_mesh_xform = xform*m_mesh_xform;
  }
}

void ON_MappingTag::SetDefaultSurfaceParameterMappingTag()
{
  ON_TextureMapping srfp_mapping;
  srfp_mapping.m_type = ON_TextureMapping::srfp_mapping;
  srfp_mapping.m_mapping_id = ON_nil_uuid;
  Set(srfp_mapping); 
}

void ON_MappingTag::Set(const ON_TextureMapping& mapping)
{
  Default();
  m_mapping_id   = mapping.m_mapping_id;
  m_mapping_type = mapping.m_type;
  m_mapping_crc  = mapping.MappingCRC();
}

bool ON_MappingTag::IsSet() const
{
  return (0 != m_mapping_crc || !ON_UuidIsNil(m_mapping_id));
}

bool ON_MappingTag::IsDefaultSurfaceParameterMapping() const
{
  bool rc = (ON_TextureMapping::srfp_mapping == m_mapping_type);
  if ( rc )
  {
    // The crc needs to be checked to insure that m_uvw
    // was the identity.
    ON_TextureMapping tmp;
    tmp.m_type = ON_TextureMapping::srfp_mapping;
    rc = (m_mapping_crc == tmp.MappingCRC());
  }
  return rc;
}


void ON_MappingTag::Default()
{
  memset(this,0,sizeof(*this));
  m_mesh_xform.m_xform[0][0] = 1.0;
  m_mesh_xform.m_xform[1][1] = 1.0;
  m_mesh_xform.m_xform[2][2] = 1.0;
  m_mesh_xform.m_xform[3][3] = 1.0;
}

int ON_MappingTag::Compare( 
             const ON_MappingTag& other,
             bool bCompareId,
             bool bCompareCRC,
             bool bCompareXform
             ) const
{
  int rc = 0;
  if ( !rc && bCompareId )
  {
    rc = ON_UuidCompare(m_mapping_id,other.m_mapping_id);
  }
  if ( !rc && bCompareCRC )
  {
    rc = ((int)m_mapping_crc) - ((int)other.m_mapping_crc);
  }
  if ( !rc && bCompareXform )
  {
    rc = m_mesh_xform.Compare(other.m_mesh_xform);
  }
  return rc;
}



bool ON_MappingTag::Write(ON_BinaryArchive& archive) const
{
  bool rc = archive.BeginWrite3dmChunk(TCODE_ANONYMOUS_CHUNK,1,1);
  if (!rc)
    return false;

  for(;;)
  {
    rc = archive.WriteUuid(m_mapping_id);
    if(!rc) break;
    rc = archive.WriteInt(m_mapping_crc);
    if(!rc) break;
    rc = archive.WriteXform(m_mesh_xform);
    if(!rc) break;

    // 1.1 fields
    rc = archive.WriteInt(m_mapping_type);
    if (!rc) break;

    break;
  }

  if ( !archive.EndWrite3dmChunk() )
    rc = false;
  
  return rc;
}

bool ON_MappingTag::Read(ON_BinaryArchive& archive)
{
  Default();
  int mjv = 0, mnv = 0;
  bool rc = archive.BeginRead3dmChunk(TCODE_ANONYMOUS_CHUNK,&mjv,&mnv);
  if (!rc)
    return false;

  for(;;)
  {
    rc = (1 == mjv);
    if (!rc) break;
    rc = archive.ReadUuid(m_mapping_id);
    if(!rc) break;
    if ( 0 == ON_UuidCompare(&obsolete_default_srfp_mapping_id,&m_mapping_id) )
      m_mapping_id = ON_nil_uuid;
    rc = archive.ReadInt(&m_mapping_crc);
    if(!rc) break;
    rc = archive.ReadXform(m_mesh_xform);
    if(!rc) break;

    if ( mnv >= 1 )
    {
      // 1.1 fields
      int i = m_mapping_type;
      rc = archive.ReadInt(&i);
      if ( rc )
        m_mapping_type = ON_TextureMapping::TypeFromInt(i);
      if (!rc) break;
    }


    break;
  }

  if ( !archive.EndRead3dmChunk() )
    rc = false;
  
  return rc;
}

ON_TextureCoordinates::ON_TextureCoordinates()
{
  m_dim = 0;
}

