#include "opennurbs.h"

bool ON_GetEndCapTransformation(ON_3dPoint P, ON_3dVector T, ON_3dVector U, 
                                const ON_3dVector* Normal, 
                                ON_Xform& xform,
                                ON_Xform* scale2d,
                                ON_Xform* rot3d
                                )
{
  if ( scale2d )
    scale2d->Identity();
  if ( rot3d )
    rot3d->Identity();
  if ( !T.IsUnitVector() && !T.Unitize() )
    return false;
  if ( !U.IsUnitVector() && !U.Unitize() )
    return false;
  ON_3dVector N(0.0,0.0,0.0);
  if ( Normal )
  {
    N = *Normal;
    if ( !N.IsUnitVector() && !N.Unitize() )
      N.Zero();
  }

  ON_Plane p0;
  p0.origin = P;
  p0.zaxis = T;
  p0.yaxis = U;
  p0.xaxis = ON_CrossProduct(U,T);
  if ( !p0.xaxis.IsUnitVector() )
    p0.xaxis.Unitize();
  p0.UpdateEquation();
  xform.Rotation(ON_xy_plane,p0);
  if ( rot3d )
    *rot3d = xform;
  if ( N.z > ON_Beam::m_Nz_min && N.IsUnitVector() )
  {
    //double cosa = T.x*N.x + T.y*N.y + T.z*N.z; // N is relative to T
    double cosa = N.z; // N is relative to xy plane.
    for(;;)
    {
      //ON_3dVector A = ON_CrossProduct(T,N); // N is relative to T
      ON_3dVector A(-N.y,N.x,0.0); // N is relative to xy plane.
      if ( !A.IsValid() )
        break;
      double sina = A.Length();
      if ( !ON_IsValid(sina) )
        break;
      if ( !A.Unitize() )
        break;
      //ON_3dVector B = ON_CrossProduct(N,A);
      ON_3dVector B(-N.z*A.x,-N.z*A.y,N.x*A.y-N.y*A.x);
      if ( !B.IsUnitVector() && !B.Unitize() )
        break;
      
      // A,B,N is a right handed orthonormal frame
      
      // S is a non-uniform scale that maps A to A and perpA to 1/cosa*perpA.
      // The scale distorts the profile so that after it is rotated
      // into the miter plane, the projection of the rotated profile
      // onto the xy-plane matches the original profile.
      ON_Xform S(0.0);
      const double c = 1.0 - 1.0/cosa;
      S.m_xform[0][0] = 1.0 - c*A.y*A.y;
      S.m_xform[0][1] = c*A.x*A.y;

      S.m_xform[1][0] = S.m_xform[0][1];
      S.m_xform[1][1] = 1.0 - c*A.x*A.x;

      S.m_xform[2][2] = 1.0;

      S.m_xform[3][3] = 1.0;
      if (scale2d)
        *scale2d = S;

      // R rotates the profile plane so its normal is equal to N
      ON_Xform R;
      R.Rotation(sina,cosa,A,ON_origin);
      if ( rot3d )
        *rot3d = xform*R;

      xform = xform*R*S;
      break;
    }
  }
  return true;
}

static void ON_BeamInitializeHelper(ON_Beam& beam)
{
  beam.m_path.from.Zero();
  beam.m_path.to.Zero();
  beam.m_t.m_t[0] = 0.0;
  beam.m_t.m_t[1] = 1.0;
  beam.m_up.Zero();
  beam.m_profile = 0;
  beam.m_bHaveN[0] = false;
  beam.m_bHaveN[1] = false;
  beam.m_N[0].Zero();
  beam.m_N[1].Zero();
  beam.m_path_domain.m_t[0] = 0.0;
  beam.m_path_domain.m_t[1] = 1.0;
  beam.m_bTransposed = false;
}

static void ON_BeamCopyHelper(const ON_Beam& src,ON_Beam& dst)
{
  if ( &src != &dst )
  {
    if ( dst.m_profile )
    {
      delete dst.m_profile;
      dst.m_profile = 0;
    }
    dst.m_path = src.m_path;
    dst.m_t = src.m_t;
    dst.m_up = src.m_up;
    dst.m_profile = src.m_profile 
                  ? src.m_profile->DuplicateCurve() 
                  : 0;
    dst.m_bHaveN[0] = src.m_bHaveN[0];
    dst.m_bHaveN[1] = src.m_bHaveN[1];
    dst.m_N[0] = src.m_N[0];
    dst.m_N[1] = src.m_N[1];
    dst.m_path_domain = src.m_path_domain;
    dst.m_bTransposed  = src.m_bTransposed;
  }
}

bool ON_Beam::SetPath(ON_3dPoint A, ON_3dPoint B)
{
  double distAB = 0.0;
  bool rc =    A.IsValid() && B.IsValid() 
            && (distAB = A.DistanceTo(B)) > ON_ZERO_TOLERANCE;
  if (rc)
  {
    m_path.from = A;
    m_path.to = B;
    m_t.Set(0.0,1.0);
    m_path_domain.Set(0.0,distAB);
  }
  return rc;
}

int ON_Beam::PathParameter() const
{
  return m_bTransposed ? 0 : 1;
}

ON_3dPoint ON_Beam::PathStart() const
{
  ON_3dPoint P(ON_UNSET_POINT);
  const double t = m_t.m_t[0];
  if ( 0.0 <= t && t <= 1.0 && m_path.IsValid() )
    P = m_path.PointAt(t);
  return P;
}

ON_3dPoint ON_Beam::PathEnd() const
{
  ON_3dPoint P(ON_UNSET_POINT);
  const double t = m_t.m_t[1];
  if ( 0.0 <= t && t <= 1.0 && m_path.IsValid() )
    P = m_path.PointAt(t);
  return P;
}

ON_3dVector ON_Beam::PathTangent() const
{
  ON_3dVector T(ON_UNSET_VECTOR);
  if ( m_path.IsValid() )
    T = m_path.Tangent();
  return T;
}

void ON_Beam::Destroy()
{
  if ( m_profile)
  {
    delete m_profile;
    m_profile = 0;
  }
  ON_BeamInitializeHelper(*this);
  DestroyRuntimeCache();
  PurgeUserData();
}

bool ON_Beam::SetMiterPlaneNormal(ON_3dVector N, int end)
{
  bool rc = false;
  if ( end >= 0 && end <= 1 )
  {
    if (    N.IsValid() 
         && N.z > ON_Beam::m_Nz_min
         && (N.IsUnitVector() || N.Unitize())
       )
    {
      if (fabs(N.x) <= ON_SQRT_EPSILON && fabs(N.y) <= ON_SQRT_EPSILON)
        N.Set(0.0,0.0,1.0);
      m_N[end] = N;
      m_bHaveN[end] = (N.z != 1.0);
      rc = true;
    }
    else if ( N.IsZero() || ON_UNSET_VECTOR == N )
    {
      m_bHaveN[end] = false;
      rc = true;
    }
  }
  return rc;
}

void ON_Beam::GetMiterPlaneNormal(int end, ON_3dVector& N) const
{
  if ( end >= 0 && end <= 1 && m_bHaveN[end] )
    N = m_N[end];
  else
    N.Set(0.0,0.0,1.0);
}

int ON_Beam::IsMitered() const
{
  int rc = 0;
  if ( m_bHaveN[0] && m_N[0].IsUnitVector() && m_N[0].z > m_Nz_min && (m_N[0].x != 0.0 || m_N[0].y != 0.0) )
    rc += 1;
  if ( m_bHaveN[1] && m_N[1].IsUnitVector() && m_N[1].z > m_Nz_min && (m_N[1].x != 0.0 || m_N[1].y != 0.0) )
    rc += 2;
  return rc;
}


bool ON_Beam::GetProfileTransformation( double s, ON_Xform& xform ) const
{
  //const ON_3dVector* N = (end?(m_bHaveN[1]?&m_N[1]:0):(m_bHaveN[0]?&m_N[0]:0));
  const ON_3dVector T = m_path.Tangent();
  if ( 0.0 == s )
  {
    return ON_GetEndCapTransformation(m_path.PointAt(m_t.m_t[0]),T,m_up,m_bHaveN[0]?&m_N[0]:0,xform,0,0);
  }
  if ( 1.0 == s )
  {
    return ON_GetEndCapTransformation(m_path.PointAt(m_t.m_t[1]),T,m_up,m_bHaveN[1]?&m_N[1]:0,xform,0,0);
  }
  ON_Xform xform0, xform1;
  if ( !ON_GetEndCapTransformation(m_path.PointAt(m_t.m_t[0]),T,m_up,m_bHaveN[0]?&m_N[0]:0,xform0,0,0) )
    return false;
  if ( !ON_GetEndCapTransformation(m_path.PointAt(m_t.m_t[1]),T,m_up,m_bHaveN[1]?&m_N[1]:0,xform1,0,0) )
    return false;

  const double s0 = 1.0-s;
  xform.m_xform[0][0] = s0*xform0.m_xform[0][0] + s*xform1.m_xform[0][0];
  xform.m_xform[0][1] = s0*xform0.m_xform[0][1] + s*xform1.m_xform[0][1];
  xform.m_xform[0][2] = s0*xform0.m_xform[0][2] + s*xform1.m_xform[0][2];
  xform.m_xform[0][3] = s0*xform0.m_xform[0][3] + s*xform1.m_xform[0][3];
  xform.m_xform[1][0] = s0*xform0.m_xform[1][0] + s*xform1.m_xform[1][0];
  xform.m_xform[1][1] = s0*xform0.m_xform[1][1] + s*xform1.m_xform[1][1];
  xform.m_xform[1][2] = s0*xform0.m_xform[1][2] + s*xform1.m_xform[1][2];
  xform.m_xform[1][3] = s0*xform0.m_xform[1][3] + s*xform1.m_xform[1][3];
  xform.m_xform[2][0] = s0*xform0.m_xform[2][0] + s*xform1.m_xform[2][0];
  xform.m_xform[2][1] = s0*xform0.m_xform[2][1] + s*xform1.m_xform[2][1];
  xform.m_xform[2][2] = s0*xform0.m_xform[2][2] + s*xform1.m_xform[2][2];
  xform.m_xform[2][3] = s0*xform0.m_xform[2][3] + s*xform1.m_xform[2][3];
  xform.m_xform[3][0] = s0*xform0.m_xform[3][0] + s*xform1.m_xform[3][0];
  xform.m_xform[3][1] = s0*xform0.m_xform[3][1] + s*xform1.m_xform[3][1];
  xform.m_xform[3][2] = s0*xform0.m_xform[3][2] + s*xform1.m_xform[3][2];
  xform.m_xform[3][3] = s0*xform0.m_xform[3][3] + s*xform1.m_xform[3][3];

  return true;
}

const double ON_Beam::m_Nz_min = 1.0/64.0;

const double ON_Beam::m_path_length_min = ON_ZERO_TOLERANCE;

ON_OBJECT_IMPLEMENT(ON_Beam,ON_Surface,"36F53175-72B8-4d47-BF1F-B4E6FC24F4B9");

ON_Beam::ON_Beam()
{
  ON_BeamInitializeHelper(*this);
}

ON_Beam::ON_Beam(const ON_Beam& src) : ON_Surface(src), m_profile(0)
{
  ON_BeamCopyHelper(src,*this);
}

ON_Beam::~ON_Beam()
{
  if ( m_profile)
  {
    delete m_profile;
  }
}

ON_Beam& ON_Beam::operator=(const ON_Beam& src)
{
  if ( this != &src )
  {
    Destroy();
    ON_Surface::operator=(src);
    ON_BeamCopyHelper(src,*this);
  }
  return *this;
}

ON_BOOL32 ON_Beam::IsValid( ON_TextLog* text_log ) const
{
  // check m_profile
  if ( !m_profile )
  {
    if ( text_log )
      text_log->Print("m_profile is NULL.\n");
    return false;
  }
  if ( !m_profile->IsValid(text_log) )
  {
    if ( text_log )
      text_log->Print("m_profile is not valid.\n");
    return false;
  }

  // check m_path
  if ( !m_path.IsValid() )
  {
    if ( text_log )
      text_log->Print("m_path is not valid.\n");
    return false;
  }
  ON_3dVector D = m_path.to - m_path.from;
  double len = D.Length();
  if ( !ON_IsValid(len) || len <= 0.0 )
  {
    if ( text_log )
      text_log->Print("m_path has zero length.");
    return false;
  }
  if ( !ON_IsValid(len) || len <= ON_Beam::m_path_length_min )
  {
    if ( text_log )
      text_log->Print("m_path has zero length <= ON_Beam::m_path_length_min.\n");
    return false;
  }
  if ( !D.Unitize() || !D.IsUnitVector() )
  {
    if ( text_log )
      text_log->Print("m_path has zero direction.\n");
    return false;
  }
  
  // check m_t
  if ( !(0.0 <= m_t.m_t[0] && m_t.m_t[0] < m_t.m_t[1] && m_t.m_t[1] <= 1.0) )
  {
    if ( text_log )
      text_log->Print("m_t does not satisfy 0<=m_t[0]<m_t[1]<=1\n");
    return false;
  }

  // check m_up
  if ( !m_up.IsUnitVector() )
  {
    if ( text_log )
      text_log->Print("m_up is not a unit vector.\n");
    return false;
  }
  len = m_up*D;
  if ( fabs(len) > ON_SQRT_EPSILON )
  {
    if ( text_log )
      text_log->Print("m_up is not perpindicular to m_path.\n");
    return false;
  }

  // validate ends
  if ( m_bHaveN[0] )
  {
    if ( !m_N[0].IsUnitVector() )
    {
      if ( text_log )
        text_log->Print("m_N[0] is not a unit vector.\n");
      return false;
    }
    if ( !(m_N[0].z > ON_Beam::m_Nz_min) )
    {
      if ( text_log )
        text_log->Print("m_N[0].z is too small (<=ON_Beam::m_Nz_min) or negative\n");
      return false;
    }
  }
  if ( m_bHaveN[1] )
  {
    if ( !m_N[1].IsUnitVector() )
    {
      if ( text_log )
        text_log->Print("m_N[1] is not a unit vector.\n");
      return false;
    }
    if ( !(m_N[1].z > ON_Beam::m_Nz_min) )
    {
      if ( text_log )
        text_log->Print("m_N[1].z is too small (<=ON_Beam::m_Nz_min) or negative\n");
      return false;
    }
  }

  return true;
}

void ON_Beam::Dump( ON_TextLog& text_log ) const
{
  text_log.Print("Path: ");
  text_log.Print(m_path.PointAt(m_t[0]));
  text_log.Print(" ");
  text_log.Print(m_path.PointAt(m_t[1]));
  text_log.Print("\n");
  text_log.Print("Up: ");
  text_log.Print(m_up);
  text_log.Print("\n");
  text_log.Print("Profile:\n");
  text_log.PushIndent();
  if ( !m_profile )
    text_log.Print("NULL");
  else
    m_profile->Dump(text_log);
  text_log.PopIndent();
  return;
}

unsigned int ON_Beam::SizeOf() const
{
  unsigned int sz = sizeof(*this) - sizeof(ON_Surface);
  if ( m_profile )
    sz += m_profile->SizeOf();
  return sz;
}

ON__UINT32 ON_Beam::DataCRC(ON__UINT32 current_remainder) const
{
  if ( m_profile )
    current_remainder = m_profile->DataCRC(current_remainder);
  current_remainder = ON_CRC32(current_remainder,sizeof(m_path),&m_path);
  current_remainder = ON_CRC32(current_remainder,sizeof(m_t),&m_t);
  current_remainder = ON_CRC32(current_remainder,sizeof(m_up),&m_up);
  current_remainder = ON_CRC32(current_remainder,sizeof(m_bHaveN[0]), &m_bHaveN[0]);
  current_remainder = ON_CRC32(current_remainder,sizeof(m_bHaveN[1]), &m_bHaveN[1]);
  current_remainder = ON_CRC32(current_remainder,sizeof(m_N[0]), &m_N[0]);
  current_remainder = ON_CRC32(current_remainder,sizeof(m_N[1]), &m_N[1]);
  current_remainder = ON_CRC32(current_remainder,sizeof(m_path_domain), &m_path_domain);
  current_remainder = ON_CRC32(current_remainder,sizeof(m_bTransposed), &m_bTransposed);
  if ( m_profile )
    current_remainder = m_profile->DataCRC(current_remainder);
  return current_remainder;
}


ON_BOOL32 ON_Beam::Write(
       ON_BinaryArchive& binary_archive
     ) const
{
  bool rc = binary_archive.BeginWrite3dmChunk(TCODE_ANONYMOUS_CHUNK,1,0);
  if (!rc)
    return false;
  for (;;)
  {
    rc = binary_archive.WriteObject(m_profile);
    if (!rc) break;
    rc = binary_archive.WriteLine(m_path);
    if (!rc) break;
    rc = binary_archive.WriteInterval(m_t);
    if (!rc) break;
    rc = binary_archive.WriteVector(m_up);
    if (!rc) break;
    rc = binary_archive.WriteBool(m_bHaveN[0]);
    if (!rc) break;
    rc = binary_archive.WriteBool(m_bHaveN[1]);
    if (!rc) break;
    rc = binary_archive.WriteVector(m_N[0]);
    if (!rc) break;
    rc = binary_archive.WriteVector(m_N[1]);
    if (!rc) break;
    rc = binary_archive.WriteInterval(m_path_domain);
    if (!rc) break;
    rc = binary_archive.WriteBool(m_bTransposed);
    if (!rc) break;
    break;
  }
  if ( !binary_archive.EndWrite3dmChunk() )
    rc = false;
  return rc;
}


ON_BOOL32 ON_Beam::Read(
       ON_BinaryArchive& binary_archive
     )
{
  Destroy();
  int major_version = 0;
  int minor_version = 0;
  bool rc = binary_archive.BeginRead3dmChunk(TCODE_ANONYMOUS_CHUNK,&major_version,&minor_version);
  if (!rc)
    return false;
  for (;;)
  {
    rc = (1 == major_version);
    if (!rc) break;
    ON_Object* obj = 0;
    rc = (1==binary_archive.ReadObject(&obj));
    if (!rc) break;
    if ( obj )
    {
      m_profile = ON_Curve::Cast(obj);
      if ( !m_profile )
      {
        delete obj;
        rc = false;
        break;
      }
    }
    rc = binary_archive.ReadLine(m_path);
    if (!rc) break;
    rc = binary_archive.ReadInterval(m_t);
    if (!rc) break;
    rc = binary_archive.ReadVector(m_up);
    if (!rc) break;
    rc = binary_archive.ReadBool(&m_bHaveN[0]);
    if (!rc) break;
    rc = binary_archive.ReadBool(&m_bHaveN[1]);
    if (!rc) break;
    rc = binary_archive.ReadVector(m_N[0]);
    if (!rc) break;
    rc = binary_archive.ReadVector(m_N[1]);
    if (!rc) break;
    rc = binary_archive.ReadInterval(m_path_domain);
    if (!rc) break;
    rc = binary_archive.ReadBool(&m_bTransposed);
    if (!rc) break;
    break;
  }
  if ( !binary_archive.EndRead3dmChunk() )
    rc = false;
  return rc;
}

ON::object_type ON_Beam::ObjectType() const
{
  return ON::beam_object;
}



int ON_Beam::Dimension() const
{
  return 3;
}

static 
bool GetBoundingBoxHelper(const ON_Beam& beam, 
                          ON_BoundingBox& bbox,
                          const ON_Xform* xform
                          )
{
  // input bbox = profile curve bounding box.
  ON_3dPoint corners[8];
  bbox.m_min.z = 0.0;
  bbox.m_max.z = 0.0;
  corners[0] = corners[1] = bbox.m_min;
  corners[1].x = bbox.m_max.x;
  corners[2] = corners[3] = bbox.m_max;
  corners[3].x = bbox.m_min.x;
  corners[4] = corners[0];
  corners[5] = corners[1];
  corners[6] = corners[2];
  corners[7] = corners[3];

  const ON_3dVector T = beam.m_path.Tangent();
  ON_Xform xform0;
  if ( !beam.GetProfileTransformation(0,xform0) )
    return false;
  ON_Xform xform1;
  if ( !beam.GetProfileTransformation(1,xform1) )
    return false;
  if ( xform && !xform->IsIdentity() )
  {
    xform0 = (*xform)*xform0;
    xform1 = (*xform)*xform1;
  }
  corners[0] = xform0*corners[0];
  corners[1] = xform0*corners[1];
  corners[2] = xform0*corners[2];
  corners[3] = xform0*corners[3];
  corners[4] = xform1*corners[4];
  corners[5] = xform1*corners[5];
  corners[6] = xform1*corners[6];
  corners[7] = xform1*corners[7];
  bbox.Set(3,0,8,3,&corners[0].x,false);
  return true;
}

ON_BOOL32 ON_Beam::GetBBox(double* boxmin,double* boxmax,int bGrowBox) const
{
  bool rc = false;
  if ( m_path.IsValid() && m_profile )
  {
    ON_BoundingBox bbox;
    if ( m_profile->GetTightBoundingBox(bbox) && GetBoundingBoxHelper(*this,bbox,0) )
    {
      rc = true;
      if ( bGrowBox )
      {
        bGrowBox =  ( boxmin[0] <= boxmax[0] 
                      && boxmin[1] <= boxmax[1] 
                      && boxmin[2] <= boxmax[2]
                      && ON_IsValid(boxmax[0]) 
                      && ON_IsValid(boxmax[1]) 
                      && ON_IsValid(boxmax[2]));
      }
      if ( bGrowBox )
      {
        if ( boxmin[0] > bbox.m_min.x ) boxmin[0] = bbox.m_min.x;
        if ( boxmin[1] > bbox.m_min.y ) boxmin[1] = bbox.m_min.y;
        if ( boxmin[2] > bbox.m_min.z ) boxmin[2] = bbox.m_min.z;
        if ( boxmax[0] < bbox.m_max.x ) boxmax[0] = bbox.m_max.x;
        if ( boxmax[1] < bbox.m_max.y ) boxmax[1] = bbox.m_max.y;
        if ( boxmax[2] < bbox.m_max.z ) boxmax[2] = bbox.m_max.z;
      }
      else
      {
        boxmin[0] = bbox.m_min.x;
        boxmin[1] = bbox.m_min.y;
        boxmin[2] = bbox.m_min.z;
        boxmax[0] = bbox.m_max.x;
        boxmax[1] = bbox.m_max.y;
        boxmax[2] = bbox.m_max.z;
      }
    }
  }
  return rc;
}


bool ON_Beam::GetTightBoundingBox(ON_BoundingBox& tight_bbox, int bGrowBox, const ON_Xform* xform ) const
{
  bool rc = false;
  if ( m_path.IsValid() && m_profile )
  {
    ON_BoundingBox bbox;
    if ( m_profile->GetTightBoundingBox(bbox) && GetBoundingBoxHelper(*this,bbox,xform) )
    {
      if ( bGrowBox )
        tight_bbox.Union(bbox);
      else
        tight_bbox = bbox;
      rc = true;
    }
  }
  return rc;
}

ON_BOOL32 ON_Beam::Transform( const ON_Xform& xform )
{
  bool rc = false;
  if ( !m_path.IsValid() )
    return false;

  ON_3dPoint Q0 = xform*m_path.from;
  ON_3dPoint Q1 = xform*m_path.to;
  if ( !Q0.IsValid() )
    return false;
  if ( !Q1.IsValid() )
    return false;
  ON_3dVector T = m_path.Tangent();
  ON_3dVector QT = Q1-Q0;
  if ( !QT.Unitize() )
    return false;
  if ( fabs(QT*T - 1.0) <= ON_SQRT_EPSILON )
    QT = T;
  ON_3dVector X = ON_CrossProduct(m_up,T);
  if ( !X.IsUnitVector() && !X.Unitize() )
    return false;
  
  ON_3dPoint X0 = xform*(m_path.from + X);
  ON_3dPoint Y0 = xform*(m_path.from + m_up);
  ON_3dPoint X1 = xform*(m_path.to + X);
  ON_3dPoint Y1 = xform*(m_path.to + m_up);

  ON_3dVector QU = Y0-Q0;
  if ( !QU.Unitize() )
    return false;
  if ( fabs(QU*m_up - 1.0) <= ON_SQRT_EPSILON )
    QU = m_up;
  if ( !(fabs(QU*QT) <= ON_SQRT_EPSILON) )
    return false;

  double s0 = Q0.DistanceTo(X0);
  double t0 = Q0.DistanceTo(Y0);
  double s1 = Q1.DistanceTo(X1);
  double t1 = Q1.DistanceTo(Y1);
  if ( fabs(s0-s1) > ON_SQRT_EPSILON*(s0+s1) )
    return false;
  if ( fabs(t0-t1) > ON_SQRT_EPSILON*(t0+t1) )
    return false;
  if ( fabs(s0-1.0) <= ON_SQRT_EPSILON )
    s0 = 1.0;
  if ( fabs(t0-1.0) <= ON_SQRT_EPSILON )
    t0 = 1.0;
  if ( fabs(s0-t0) <= ON_SQRT_EPSILON*(s0+t0) )
  {
    if ( 1.0 == s0 )
      t0 = 1.0;
    else
      s0 = t0;
  }
  m_path.from = Q0;
  m_path.to = Q1;
  m_up = QU;
  if ( s0 != 1.0 || t0 != 1.0 )
  {
    // scale profile curve
    ON_Xform profile_scale(1.0);
    profile_scale.m_xform[0][0] = s0;
    profile_scale.m_xform[1][1] = t0;
    if ( s0 != t0 && !m_profile->IsDeformable() )
    {
      ON_NurbsCurve* c = m_profile->NurbsCurve();
      if ( c )
      {
        c->CopyUserData(*m_profile);
        if ( !m_profile->Transform(profile_scale) )
        {
          if( c->Transform(profile_scale) )
          {
            rc = true;
            delete m_profile;
            m_profile = c;
          }
        }
        else
        {
          delete c;
        }
      }
    }
    else
    {
      rc = m_profile->Transform(profile_scale)?true:false;
    }
  }

  return true;
}

class CMyBrepIsSolidSetter : public ON_Brep
{
public:
  void SetIsSolid(int i) {m_is_solid = i;}
};

ON_Brep* ON_Beam::BrepForm( ON_Brep* brep ) const
{
  brep = ON_Surface::BrepForm(brep);
  while (    brep 
       && 1 == brep->m_F.Count() 
       && 4 == brep->m_T.Count() 
       && m_profile->IsClosed() 
       )
  {
    // cap ends
    int capti[2] = {-1,-1};
    int seam_count = 0;
    int cap_count = 0;
    int other_count = 0;
    for ( int ti = 0; ti < brep->m_T.Count(); ti++ )
    {
      switch ( brep->m_T[ti].m_type )
      {
      case ON_BrepTrim::seam:
        seam_count++;
        break;
      case ON_BrepTrim::boundary:
        if ( cap_count < 2 )
          capti[cap_count] = ti;
        cap_count++;
        break;
      default:
        other_count++;
        break;
      }
    }
    if ( 0 != other_count )
      break;
    if ( 2 != seam_count )
      break;
    if ( 2 != cap_count )
      break;
    if ( ON_Surface::S_iso != brep->m_T[capti[0]].m_iso )
      break;
    if ( ON_Surface::N_iso != brep->m_T[capti[1]].m_iso )
      break;
    ON_BrepEdge* edge0 = brep->Edge(brep->m_T[capti[0]].m_ei);
    if ( !edge0 )
      break;
    ON_BrepEdge* edge1 = brep->Edge(brep->m_T[capti[1]].m_ei);
    if ( !edge1 )
      break;

    // Add end caps.
    ON_Xform xform0, xform1, scale0, scale1, rot0, rot1;

    const ON_3dVector T = m_path.Tangent();
    if ( !ON_GetEndCapTransformation(m_path.PointAt(m_t.m_t[0]),T,m_up,m_bHaveN[0]?&m_N[0]:0,xform0,&scale0,&rot0) )
      break;
    if ( !ON_GetEndCapTransformation(m_path.PointAt(m_t.m_t[1]),T,m_up,m_bHaveN[1]?&m_N[1]:0,xform1,&scale1,&rot1) )
      break;

    int bReverseProfile = (-1 == ON_ClosedCurveOrientation(*m_profile,0));
    if ( bReverseProfile )
      brep->Flip();

    brep->m_C2.Reserve(brep->m_C2.Count()+2);
    int c2i[2];
    ON_NurbsCurve* c20 = m_profile->NurbsCurve();
    if ( !c20 )
      break;
    if ( bReverseProfile )
      c20->Reverse();
    c20->ChangeDimension(2);
    ON_NurbsCurve* c21 = c20->Duplicate();
    if ( !c21 )
    {
      delete c20;
      break;
    }
    if ( !scale0.IsIdentity() )
      c20->Transform(scale0);
    if ( !scale1.IsIdentity() )
      c21->Transform(scale1);
    c2i[0] = brep->AddTrimCurve(c20);
    c2i[1] = brep->AddTrimCurve(c21);

    ON_BoundingBox bbox;
    if ( !c20->GetTightBoundingBox(bbox) )
      bbox = c20->BoundingBox();
    ON_Interval u0(bbox.m_min.x,bbox.m_max.x);
    ON_Interval v0(bbox.m_min.y,bbox.m_max.y);
    double d = u0.Length()*0.125; u0.m_t[0] -= d; u0.m_t[1] += d;
    d = v0.Length()*0.125; v0.m_t[0] -= d; v0.m_t[1] += d;

    ON_PlaneSurface* plane0 = new ON_PlaneSurface(ON_xy_plane);
    plane0->SetExtents(0,u0,true);
    plane0->SetExtents(1,v0,true);
    if ( !rot0.IsIdentity() )
      plane0->Transform(rot0);

    if ( !c21->GetTightBoundingBox(bbox) )
      bbox = c21->BoundingBox();
    ON_Interval u1(bbox.m_min.x,bbox.m_max.x);
    ON_Interval v1(bbox.m_min.y,bbox.m_max.y);
    d = u1.Length()*0.125; u1.m_t[0] -= d; u1.m_t[1] += d;
    d = v1.Length()*0.125; v1.m_t[0] -= d; v1.m_t[1] += d;

    ON_PlaneSurface* plane1 = new ON_PlaneSurface(ON_xy_plane);
    plane1->SetExtents(0,u1,true);
    plane1->SetExtents(1,v1,true);
    if ( !rot1.IsIdentity() )
      plane1->Transform(rot1);

    int si[2];
    si[0] = brep->AddSurface(plane0);
    si[1] = brep->AddSurface(plane1);

    brep->m_F.Reserve(brep->m_F.Count()+2);
    brep->m_T.Reserve(brep->m_F.Count()+2);
    brep->m_L.Reserve(brep->m_F.Count()+2);

    ON_BrepFace& face0 = brep->NewFace(si[0]);
    face0.m_bRev = true;
    ON_BrepFace& face1 = brep->NewFace(si[1]);
    ON_BrepLoop& loop0 = brep->NewLoop(ON_BrepLoop::outer,face0);
    ON_BrepLoop& loop1 = brep->NewLoop(ON_BrepLoop::outer,face1);
    ON_BrepTrim& trim0 = brep->NewTrim(*edge0, bReverseProfile,loop0,c2i[0]);
    ON_BrepTrim& trim1 = brep->NewTrim(*edge1,!bReverseProfile,loop1,c2i[1]);
    trim0.m_tolerance[0] = trim0.m_tolerance[1] = 0.0;
    trim1.m_tolerance[0] = trim1.m_tolerance[1] = 0.0;
    ((CMyBrepIsSolidSetter*)brep)->SetIsSolid(1);

    break;
  }
  return brep;
}

static void KinkEvalHelper( const ON_Curve* profile,
                            double kminus, double kink_param, double kplus,
                            ON_3dPoint& P, ON_3dVector& T0, ON_3dVector& T1,
                            int* hint
                            )
{
  ON_3dPoint Q;
  profile->EvTangent(kink_param,Q,T0,-1,hint);
  profile->EvTangent(kink_param,P,T1,+1,hint);
  if ( T0*T1 >= 1.0-ON_SQRT_EPSILON)
  {
    // When the curve is a polycurve or proxycurve,
    // sometimes the reparameterization adds
    // enough fuzz to the kink_param that we have
    // to use the minus/plus approach.
    profile->EvTangent(kminus,Q,T0,-1,hint);
    profile->EvTangent(kplus, Q,T1,+1,hint);
  }
  P.z = 0.0;
  T0.z = 0.0;
  T1.z = 0.0;
}

static bool FindNextKinkHelper( const ON_Curve* profile,
                                double t0, double t1,
                                double* kminus, double* kink_param, double* kplus
                               )
{
  bool rc = profile->GetNextDiscontinuity(ON::G1_continuous,t0,t1,kink_param);
  if (rc && *kink_param > t0 && *kink_param < t1 )
  {
    profile->GetParameterTolerance(*kink_param,kminus,kplus);
  }
  else
  {
    rc = false;
    *kink_param = ON_DBL_MAX;
    *kminus = ON_DBL_MAX;
    *kplus = ON_DBL_MAX;
  }
  return rc;
}

class CVertexInfo
{
  // Helper class used in ON_Beam::CreateMesh()
  // It is declared out here to keep gcc happy.
public:
  ON_3dPoint P;  // profile 2d point
  ON_3dVector T; // profile 3d tangent
  double t;      // profile parameter
  int vi[2];     // mesh->m_V[] indices
};

ON_Mesh* ON_Beam::CreateMesh( 
           const ON_MeshParameters& mp,
           ON_Mesh* mesh
           ) const
{
  ON_Workspace ws;

  bool bCapMesh = true;//true;

  // Make sure beam is well defined
  if ( !m_profile )
    return 0;
  const ON_3dVector pathT = m_path.Tangent();
  ON_Xform xform0, xform1, rot0, rot1, xformT;
  if ( !ON_GetEndCapTransformation(m_path.PointAt(m_t.m_t[0]),pathT,m_up,m_bHaveN[0]?&m_N[0]:0,xform0,0,&rot0) )
    return 0;
  if ( !ON_GetEndCapTransformation(m_path.PointAt(m_t.m_t[1]),pathT,m_up,m_bHaveN[1]?&m_N[1]:0,xform1,0,&rot1) )
    return 0;
  if ( !ON_GetEndCapTransformation(ON_origin,pathT,m_up,0,xformT,0,0) )
    return 0;

  // Get a polyline that approximates the profile;
  ON_PolylineCurve polyline0;
  if ( !m_profile->IsPolyline(&polyline0.m_pline,&polyline0.m_t) )
  {
    ON_MeshCurveParameters mcp;
    mcp.m_max_ang_radians = mp.m_refine_angle;
    mcp.m_max_chr = mp.m_relative_tolerance;
    mcp.m_max_aspect = mp.m_grid_aspect_ratio;
    mcp.m_tolerance = mp.m_tolerance;
    mcp.m_min_edge_length = mp.m_min_edge_length;
    mcp.m_max_edge_length = mp.m_max_edge_length;
    if ( 0 == m_profile->MeshCurve(mcp,&polyline0,false,0) )
      return 0;
  }
  int count = polyline0.m_pline.Count();
  if ( count < 2 )
    return 0;

  bool bFlipMesh = false;
  const bool bIsClosed = count >= 4 && (polyline0.IsClosed() || m_profile->IsClosed());
  if ( bIsClosed )
  {
    *polyline0.m_pline.Last() = polyline0.m_pline[0];

    // see if polyline is going clockwise
    int tcount = polyline0.m_t.Count();
    const double* t = polyline0.m_t.Array();
    ON_3dPoint p0,p1;
    double a = 0.0;
    p1 = m_profile->PointAt(t[tcount-1]);
    for ( int i = 1; i < tcount; i++ )
    {
      p0 = p1;
      p1 = m_profile->PointAt(t[i]);
      a += (p0.x-p1.x)*(p0.y+p1.y);
    }
    if ( a < 0.0 )
    {
      bFlipMesh = true;
    }
  }

  // Convert polyline information into information
  // needed to create mesh vertex, texture, and normal
  // arrays.
  ON_3dVector T1;

  // See if there are kinks in the profile
  double kink_param, kminus, kplus;
  const double kink_max = *polyline0.m_t.Last();
  bool bKinkyProfile = FindNextKinkHelper( m_profile,polyline0.m_t[0],kink_max,&kminus,&kink_param,&kplus);

  ON_SimpleArray<CVertexInfo> Vinfo_list(count+m_profile->SpanCount());
  CVertexInfo Vinfo;
  memset(&Vinfo,0,sizeof(Vinfo));
  int hint = 0;
  int i;
  for ( i = 0; i < count; i++ )
  {
    Vinfo.t = polyline0.m_t[i];
    polyline0.m_pline[i].z = 0.0;
    if (    bKinkyProfile
         && Vinfo_list.Count() > 0 
         && Vinfo_list.Last()->t < kminus 
         && kplus < Vinfo.t 
       )
    {
      while ( bKinkyProfile && kplus < Vinfo.t )
      {
        // add kinks between previous and current polyline0 point
        KinkEvalHelper( m_profile,kminus,kink_param,kplus,Vinfo.P,Vinfo.T,T1,&hint);
        Vinfo.t = kink_param;
        Vinfo_list.Append(Vinfo);
        Vinfo.T = T1;
        Vinfo_list.Append(Vinfo);
        bKinkyProfile = FindNextKinkHelper( m_profile,kplus,kink_max,&kminus,&kink_param,&kplus);
      }
      Vinfo.t = polyline0.m_t[i];
    }

    if (    bKinkyProfile
         && kminus <= Vinfo.t && Vinfo.t <= kplus 
         && i < count-1 && polyline0.m_t[i+1] > kink_param )
    {
      // polyline0.m_t[i] is at a kink
      KinkEvalHelper( m_profile,kminus,kink_param,kplus,Vinfo.P,Vinfo.T,T1,&hint);
      Vinfo.t = kink_param;
      Vinfo_list.Append(Vinfo);
      Vinfo.T = T1;
      Vinfo_list.Append(Vinfo);
      bKinkyProfile = FindNextKinkHelper( m_profile,kplus,kink_max,&kminus,&kink_param,&kplus);
    }
    else
    {
      // no kink at polyline0.m_t[i]
      Vinfo.t = polyline0.m_t[i];
      m_profile->EvTangent(Vinfo.t,Vinfo.P,Vinfo.T,(count-1 == i) ? -1 : 1,&hint);
      Vinfo.P.z = 0;
      Vinfo.T.z = 0;
      Vinfo_list.Append(Vinfo);
    }
  }

  count = Vinfo_list.Count();
  if ( count < 2 )
    return 0;

  if ( bIsClosed )
    Vinfo_list.Last()->P = Vinfo_list[0].P;
  else
    bCapMesh = false;

  const ON_3dex* captri_list = 0;
  ON_2dex* capV_list = 0;
  int captri_count = 0;
  int capV_count = 0;
  ON_3dPoint P;
  if ( bCapMesh )
  {
    polyline0.m_pline.SetCount(0);
    polyline0.m_t.SetCount(0);
    polyline0.m_pline.Reserve(count);
    polyline0.m_t.Reserve(count);
    Vinfo = Vinfo_list[0];
    polyline0.m_pline.Append(Vinfo.P);
    polyline0.m_t.Append(Vinfo.t);
    for (i = 1; i < count; i++)
    {
      const CVertexInfo& VI = Vinfo_list[i];
      if ( VI.t > Vinfo.t )
      {
        Vinfo = VI;
        P = Vinfo.P;
        P.z = i;
        polyline0.m_pline.Append(P);
        polyline0.m_t.Append(Vinfo.t);
      }
    }
    capV_count = polyline0.PointCount()-1;
    captri_count = capV_count-2;
    int* tris = (captri_count > 0) ? ws.GetIntMemory(3*captri_count) : 0;
    if (    captri_count < 1 
         || !ON_Mesh2dPolygon(capV_count,3,&polyline0.m_pline[0].x,3,tris))
    {
      captri_count = 0;
      capV_count = 0;
    }
    else
    {
      ON_2dex* vv = (ON_2dex*)ws.GetIntMemory(2*capV_count);
      for ( i = 0; i < capV_count; i++ )
      {
        // set vv[i].j = Vinfo[] index for point polyline0.m_pline[i]
        vv[i].i = -1;
        vv[i].j = (int)(polyline0.m_pline[i].z); // vv[i].j = Vinfo_list[] index
      }
      capV_list = vv;
      captri_list = (ON_3dex*)tris;
    }
  }

  if ( !mesh )
    mesh = new ON_Mesh();
  else
    mesh->Destroy();

  int path_dir = PathParameter();
  mesh->m_V.Reserve(2*(count+capV_count));
  mesh->m_N.Reserve(2*(count+capV_count));
  mesh->m_S.Reserve(2*(count+capV_count));
  mesh->m_T.Reserve(2*(count+capV_count));
  mesh->m_srf_domain[0] = Domain(0);
  mesh->m_srf_domain[1] = Domain(1);
  mesh->m_srf_scale[path_dir]   = m_path.Length();
  double polyline_length=0.0;
  polyline0.GetLength(&polyline_length);
  mesh->m_srf_scale[1-path_dir] = polyline_length;
  mesh->m_packed_tex_domain[0].Set(0.0,1.0);
  mesh->m_packed_tex_domain[1].Set(0.0,1.0);
  mesh->m_packed_tex_rotate = false;
  ON_2dPoint S;
  ON_3dVector N, T;
  ON_2fPoint tex;
  for ( i = 0; i < count; i++ )
  {
    CVertexInfo& VI = Vinfo_list[i];
    VI.P.z = 0.0;
    VI.T.z = 0.0;
    P = xform0*VI.P;
    T = xformT*VI.T;
    N = ON_CrossProduct(T,pathT);
    if ( m_bTransposed )
      N.Reverse();
    if ( !N.IsUnitVector() || !N.Unitize() )
      N.Zero();
    S[1-path_dir] = VI.t;
    S[path_dir] = m_path_domain[0];
    tex.x = (float)mesh->m_srf_domain[0].NormalizedParameterAt(S.x);
    tex.y = (float)mesh->m_srf_domain[1].NormalizedParameterAt(S.y);
    VI.vi[0] = mesh->m_V.Count();
    mesh->m_V.AppendNew() = P;
    mesh->m_N.AppendNew() = N;
    mesh->m_S.Append(S);
    mesh->m_T.Append(tex);

    P = xform1*VI.P;
    S[path_dir] = m_path_domain[1];
    tex[path_dir] = (float)mesh->m_srf_domain[path_dir].NormalizedParameterAt(S[path_dir]);
    VI.vi[1] = mesh->m_V.Count();
    mesh->m_V.AppendNew() = P;
    mesh->m_N.AppendNew() = N;
    mesh->m_S.Append(S);
    mesh->m_T.Append(tex);
  }

  // add quads
  Vinfo = Vinfo_list[0];
  int fvi1, fvi3;
  if ( m_bTransposed )
  {
    fvi1 = 3;
    fvi3 = 1;
  }
  else
  {
    fvi1 = 1;
    fvi3 = 3;
  }
  mesh->m_F.Reserve(polyline0.PointCount()-1 + 2*captri_count);
  mesh->m_FN.Reserve(polyline0.PointCount()-1 + 2*captri_count);
  for ( i = 1; i < count; i++ )
  {
    const CVertexInfo& Vinfo1 = Vinfo_list[i];
    if ( Vinfo.t < Vinfo1.t )
    {
      ON_MeshFace& f = mesh->m_F.AppendNew();
      f.vi[0] = Vinfo.vi[0];
      f.vi[fvi1] = Vinfo1.vi[0];
      f.vi[2] = Vinfo1.vi[1];
      f.vi[fvi3] = Vinfo.vi[1];      
    }
    Vinfo = Vinfo1;
  }
  mesh->ComputeFaceNormals();

  if ( captri_count > 0 && capV_count == captri_count+2 )
  {
    // add end caps
    ON_3dVector N0, N1;
    if ( m_bHaveN[0] )
    {
      N0 = rot0*ON_zaxis;
      if ( !N0.Unitize() )
        N0 = -pathT;
      else
        N0.Reverse();
    }
    else
    {
      N0 = -pathT;
    }
    if ( m_bHaveN[1] )
    {
      N1 = rot1*ON_zaxis;
      if ( !N1.Unitize() )
        N1 = pathT;
    }
    else
    {
      N1 = pathT;
    }

    if ( bFlipMesh )
    {
      N0.Reverse();
      N1.Reverse();
    }

    mesh->m_V.Reserve(mesh->m_V.Count()+2*capV_count);
    mesh->m_S.Reserve(mesh->m_S.Count()+2*capV_count);
    mesh->m_N.Reserve(mesh->m_N.Count()+2*capV_count);
    mesh->m_T.Reserve(mesh->m_T.Count()+2*capV_count);
    // add vertices for the bottom cap
    for ( i = 0; i < capV_count; i++ )
    {
      // capV_list[i].j = Vinfo_list[] index
      const CVertexInfo& VI = Vinfo_list[capV_list[i].j];
      // set capV_list[i].i = mesh->m_V[] index
      capV_list[i].i = mesh->m_V.Count();
      mesh->m_V.Append(mesh->m_V[VI.vi[0]]);
      mesh->m_S.Append(mesh->m_S[VI.vi[0]]);
      mesh->m_T.Append(mesh->m_T[VI.vi[0]]);
      mesh->m_N.AppendNew() = N0;
    }
    // add vertices for the top cap
    for ( i = 0; i < capV_count; i++ )
    {
      // capV_list[i].j = Vinfo_list[] index
      const CVertexInfo& VI = Vinfo_list[capV_list[i].j];
      // set capV_list[i].j = mesh->m_V[] index
      capV_list[i].j = mesh->m_V.Count();
      mesh->m_V.Append(mesh->m_V[VI.vi[1]]);
      mesh->m_S.Append(mesh->m_S[VI.vi[1]]);
      mesh->m_T.Append(mesh->m_T[VI.vi[1]]);
      mesh->m_N.AppendNew() = N1;
    }
    mesh->m_F.Reserve(mesh->m_F.Count()+2*captri_count);
    mesh->m_FN.Reserve(mesh->m_F.Count()+2*captri_count);
    // add triangles for the bottom cap
    for ( i = 0; i < captri_count; i++ )
    {
      const ON_3dex& tri = captri_list[i];
      ON_MeshFace& f = mesh->m_F.AppendNew();
      f.vi[0] = capV_list[tri.i].i;
      f.vi[1] = capV_list[tri.k].i;
      f.vi[2] = capV_list[tri.j].i;
      f.vi[3] = f.vi[2];
      mesh->m_FN.AppendNew() = N0;
    }
    // add triangles for the top cap
    for ( i = 0; i < captri_count; i++ )
    {
      const ON_3dex& tri = captri_list[i];
      ON_MeshFace& f = mesh->m_F.AppendNew();
      f.vi[0] = capV_list[tri.i].j;
      f.vi[1] = capV_list[tri.j].j;
      f.vi[2] = capV_list[tri.k].j;
      f.vi[3] = f.vi[2];
      mesh->m_FN.AppendNew() = N1;
    }
  }

  if ( 0 != mesh && bFlipMesh )
    mesh->Flip();

  return mesh;
}

ON_BOOL32 ON_Beam::SetDomain( 
  int dir, // 0 sets first parameter's domain, 1 gets second parameter's domain
  double t0, 
  double t1
  )
{
  bool rc = false;
  if ( ON_IsValid(t0) && ON_IsValid(t1) && t0 < t1 )
  {
    const int path_dir = PathParameter();
    if ( path_dir == dir )
    {
      m_path_domain.Set(t0,t1);
      rc = true;
    }
    else if ( 1-path_dir == dir )
    {
      rc = m_profile->SetDomain(t0,t1)?true:false;
    }
  }
  return rc;
}

ON_Interval ON_Beam::Domain(
  int dir // 0 gets first parameter's domain, 1 gets second parameter's domain
  ) const 
{
  const int path_dir = PathParameter();
  return (path_dir == dir ) 
         ? m_path_domain 
         : ((1-path_dir == dir && m_profile) ? m_profile->Domain() : ON_Interval());
}

ON_BOOL32 ON_Beam::GetSurfaceSize( 
    double* width, 
    double* height 
    ) const
{
  bool rc = true;
  //int path_dir = PathParameter();
  if ( PathParameter() )
  {
    double* p = width;
    width = height;
    height = p;
  }
  if ( width )
  {
    if ( m_path.IsValid() && m_t.IsIncreasing() )
      *width = m_path.Length()*m_t.Length();
    else
    {
      *width = 0.0;
      rc = false;
    }
  }
  if (height)
  {
    if ( m_profile )
    {
      rc = m_profile->GetLength(height)?true:false;
    }
    else 
    {
      rc = false;
      *height = 0.0;
    }
  }
  return rc;
}

int ON_Beam::SpanCount(
  int dir // 0 gets first parameter's domain, 1 gets second parameter's domain
  ) const // number of smooth nonempty spans in the parameter direction
{
  const int path_dir = PathParameter();
  if ( path_dir == dir )
    return 1;
  if ( 1-path_dir == dir && m_profile )
    return m_profile->SpanCount();
  return 0;
}

ON_BOOL32 ON_Beam::GetSpanVector( // span "knots" 
      int dir, // 0 gets first parameter's domain, 1 gets second parameter's domain
      double* span_vector // array of length SpanCount() + 1 
      ) const  // 
{
  if ( span_vector )
  {
    const int path_dir = PathParameter();
    if ( path_dir == dir )
    {
      span_vector[0] = m_path_domain[0];
      span_vector[1] = m_path_domain[1];
      return true;
    }
    if ( 1-path_dir == dir && m_profile )
    {
      return m_profile->GetSpanVector(span_vector);
    }
  }
  return false;
}

ON_BOOL32 ON_Beam::GetSpanVectorIndex(
      int dir , // 0 gets first parameter's domain, 1 gets second parameter's domain
      double t,      // [IN] t = evaluation parameter
      int side,         // [IN] side 0 = default, -1 = from below, +1 = from above
      int* span_vector_index,        // [OUT] span vector index
      ON_Interval* span_interval // [OUT] domain of the span containing "t"
      ) const
{
  const int path_dir = PathParameter();
  if ( path_dir == dir )
  {
    if ( span_vector_index )
      *span_vector_index = 0;
    if ( span_interval )
      *span_interval = m_path_domain;
    return true;
  }
  if ( 1-path_dir == dir && m_profile )
  {
    return m_profile->GetSpanVectorIndex(t,side,span_vector_index,span_interval);
  }
  return false;
}

int ON_Beam::Degree( // returns maximum algebraic degree of any span 
                // ( or a good estimate if curve spans are not algebraic )
  int dir // 0 gets first parameter's domain, 1 gets second parameter's domain
  ) const  
{
  const int path_dir = PathParameter();
  if ( path_dir == dir )
    return 1;
  if ( 1-path_dir == dir && m_profile )
    return m_profile->Degree();
  return 0;
}

ON_BOOL32 ON_Beam::GetParameterTolerance( // returns tminus < tplus: parameters tminus <= s <= tplus
       int dir,        // 0 gets first parameter, 1 gets second parameter
       double t,       // t = parameter in domain
       double* tminus, // tminus
       double* tplus   // tplus
       ) const
{
  const int path_dir = PathParameter();
  if ( path_dir == dir )
    return ON_Surface::GetParameterTolerance(dir,t,tminus,tplus);
  if ( 1-path_dir==dir && m_profile)
    return m_profile->GetParameterTolerance(t,tminus,tplus);
  return false;
}

ON_Surface::ISO ON_Beam::IsIsoparametric(
      const ON_Curve& curve,
      const ON_Interval* curve_domain
      ) const
{
  return ON_Surface::IsIsoparametric(curve,curve_domain);
}

ON_BOOL32 ON_Beam::IsPlanar(
      ON_Plane* plane,
      double tolerance
      ) const
{
  if ( m_profile && m_profile->IsLinear(tolerance) )
  {
    if ( plane )
    {
      ON_3dPoint P0 = m_profile->PointAtStart();
      ON_3dPoint P1 = m_profile->PointAtEnd();
      ON_3dVector pathT = m_path.Tangent();
      ON_3dVector Y = m_up;
      ON_3dVector X = ON_CrossProduct(Y,pathT);
      if ( !X.IsUnitVector() )
        X.Unitize();
      ON_3dPoint Q0 = m_path.from + P0.x*X + P0.y*Y;
      ON_3dPoint Q1 = m_path.from + P1.x*X + P1.y*Y;
      ON_3dVector N = ON_CrossProduct(pathT,Q1-Q0);
      N.Unitize();
      plane->origin = P0;
      if ( m_bTransposed )
      {
        plane->yaxis = pathT;
        plane->zaxis = -N;
        plane->xaxis = ON_CrossProduct(plane->yaxis,plane->zaxis);
        plane->xaxis.Unitize();
      }
      else
      {
        plane->xaxis = pathT;
        plane->zaxis = N;
        plane->yaxis = ON_CrossProduct(plane->zaxis,plane->xaxis);
        plane->yaxis.Unitize();
      }
      plane->UpdateEquation();
    }    
    return true;
  }
  return false;
}

ON_BOOL32 ON_Beam::IsClosed(int dir) const
{
  const int path_dir = PathParameter();
  if ( 1-path_dir == dir && m_profile )
    return m_profile->IsClosed();
  return false;
}

ON_BOOL32 ON_Beam::IsPeriodic( int dir ) const
{
  const int path_dir = PathParameter();
  if ( 1-path_dir == dir && m_profile )
    return m_profile->IsPeriodic();
  return false;
}

//ON_BOOL32 ON_Beam::IsSingular( // true if surface side is collapsed to a point
//      int        // side of parameter space to test
//                 // 0 = south, 1 = east, 2 = north, 3 = west
//      ) const
bool ON_Beam::GetNextDiscontinuity( 
                int dir,
                ON::continuity c,
                double t0,
                double t1,
                double* t,
                int* hint,
                int* dtype,
                double cos_angle_tolerance,
                double curvature_tolerance
                ) const
{
  const int path_dir = PathParameter();
  if ( path_dir == dir )
  {
    return ON_Surface::GetNextDiscontinuity(dir,c,t0,t1,t,hint,dtype,cos_angle_tolerance,curvature_tolerance);
  }
  if ( 1-path_dir==dir && m_profile)
  {
    return m_profile->GetNextDiscontinuity(c,t0,t1,t,hint,dtype,cos_angle_tolerance,curvature_tolerance);
  }
  return false;
}

bool ON_Beam::IsContinuous(
  ON::continuity c,
  double s, 
  double t, 
  int* hint,
  double point_tolerance,
  double d1_tolerance,
  double d2_tolerance,
  double cos_angle_tolerance,
  double curvature_tolerance
  ) const
{
  if ( !m_profile )
    return false;
  int* crv_hint = 0;
  double curvet;
  if ( m_bTransposed )
  {
    curvet = s;
    crv_hint = hint;
  }
  else
  {
    curvet = t;
    crv_hint = hint ? hint+1 : 0;
  }
  return m_profile->IsContinuous(c,curvet,crv_hint,point_tolerance,d1_tolerance,d2_tolerance,cos_angle_tolerance,curvature_tolerance);
}

ON_Surface::ISO ON_Beam::IsIsoparametric(
      const ON_BoundingBox& bbox
      ) const
{
  return ON_Surface::IsIsoparametric(bbox);
}

ON_BOOL32 ON_Beam::Reverse( int dir )
{
  const int path_dir = PathParameter();
  if ( path_dir == dir )
  {
    m_path_domain.Reverse();
    return true;
  }
  if ( 1-path_dir == dir && m_profile )
  {
    return m_profile->Reverse();
  }
  return false;
}

ON_BOOL32 ON_Beam::Transpose() // transpose surface parameterization (swap "s" and "t")
{
  m_bTransposed = m_bTransposed?false:true;
  return true;
}

ON_BOOL32 ON_Beam::Evaluate( // returns false if unable to evaluate
       double u, double v,   // evaluation parameters
       int num_der,          // number of derivatives (>=0)
       int array_stride,     // array stride (>=Dimension())
       double* der_array,    // array of length stride*(ndir+1)*(ndir+2)/2
       int quadrant ,     // optional - determines which quadrant to evaluate from
                             //         0 = default
                             //         1 from NE quadrant
                             //         2 from NW quadrant
                             //         3 from SW quadrant
                             //         4 from SE quadrant
       int* hint          // optional - evaluation hint (int[2]) used to speed
                             //            repeated evaluations
       ) const 
{
  if ( !m_profile )
    return false;

  double x,y,dx,dy;
  //int side = 0;
  if ( m_bTransposed ) 
  {
    x = u; u = v; v = x;
    if ( 4 == quadrant )
      quadrant = 2;
    else if ( 2 == quadrant )
      quadrant = 4;
  }

  if ( !m_profile->Evaluate( u, num_der, array_stride, der_array,
                             (1==quadrant||4==quadrant)?1:((2==quadrant||3==quadrant)?-1:0),
                               hint) 
     )
  {
    return false;
  }

  // TODO: After testing, add special case that avoids
  //       two calls to GetProfileTransformation() when 
  //       mitering is trivial.
  const double t1 = m_path_domain.NormalizedParameterAt(v);
  const double t0 = 1.0-t1;
  ON_Xform xform0, xform1;
  const ON_3dVector T = m_path.Tangent();
  if ( 0.0 != t0 || num_der > 0 )
  {
    if ( !ON_GetEndCapTransformation(m_path.PointAt(m_t.m_t[0]),T,m_up,m_bHaveN[0]?&m_N[0]:0,xform0,0,0) )
      return false;
  }
  else
  {
    xform0.Zero();
  }
  if ( 0.0 != t1 || num_der > 0 )
  {
    if ( !ON_GetEndCapTransformation(m_path.PointAt(m_t.m_t[1]),T,m_up,m_bHaveN[1]?&m_N[1]:0,xform1,0,0) )
      return false;
  }
  else
  {
    xform1.Zero();
  }

  double xformP[3][3], xformD[3][3];
  xformP[0][0] = t0*xform0.m_xform[0][0] + t1*xform1.m_xform[0][0];
  xformP[0][1] = t0*xform0.m_xform[0][1] + t1*xform1.m_xform[0][1];
  xformP[0][2] = t0*xform0.m_xform[0][3] + t1*xform1.m_xform[0][3];
  xformP[1][0] = t0*xform0.m_xform[1][0] + t1*xform1.m_xform[1][0];
  xformP[1][1] = t0*xform0.m_xform[1][1] + t1*xform1.m_xform[1][1];
  xformP[1][2] = t0*xform0.m_xform[1][3] + t1*xform1.m_xform[1][3];
  xformP[2][0] = t0*xform0.m_xform[2][0] + t1*xform1.m_xform[2][0];
  xformP[2][1] = t0*xform0.m_xform[2][1] + t1*xform1.m_xform[2][1];
  xformP[2][2] = t0*xform0.m_xform[2][3] + t1*xform1.m_xform[2][3];

  int i,j;
  i = num_der+1;
  double* d1 = der_array + array_stride*(i*(i+1)/2 - 1);
  const double* d0 = der_array + array_stride*(i - 1);
  x = d0[0];
  y = d0[1];
  if ( num_der > 0 )
  {
    double d = m_path_domain.m_t[1] - m_path_domain.m_t[0];
    if ( d > 0.0 )
      d = 1.0/d;
    xformD[0][0] = d*(xform1.m_xform[0][0] - xform0.m_xform[0][0]);
    xformD[0][1] = d*(xform1.m_xform[0][1] - xform0.m_xform[0][1]);
    xformD[0][2] = d*(xform1.m_xform[0][3] - xform0.m_xform[0][3]);
    xformD[1][0] = d*(xform1.m_xform[1][0] - xform0.m_xform[1][0]);
    xformD[1][1] = d*(xform1.m_xform[1][1] - xform0.m_xform[1][1]);
    xformD[1][2] = d*(xform1.m_xform[1][3] - xform0.m_xform[1][3]);
    xformD[2][0] = d*(xform1.m_xform[2][0] - xform0.m_xform[2][0]);
    xformD[2][1] = d*(xform1.m_xform[2][1] - xform0.m_xform[2][1]);
    xformD[2][2] = d*(xform1.m_xform[2][3] - xform0.m_xform[2][3]);

    for ( i = num_der; i > 0; i-- )
    {
      dx = x;
      dy = y;
      d0 -= array_stride;
      x = d0[0];
      y = d0[1];

      // all partials involving two or more derivatives with
      // respect to "v" are zero.
      j = i;
      while ( --j )
      {
        d1[0] = d1[1] = d1[2] = 0.0;
        d1 -= array_stride;
      }    

      // The partial involving a single derivative with respect to "v"
      if ( 1 == i )
      {
        // xformD transform is applied to curve location ((x,y) = point)
        d1[0] = xformD[0][0]*x + xformD[0][1]*y + xformD[0][2];
        d1[1] = xformD[1][0]*x + xformD[1][1]*y + xformD[1][2];
        d1[2] = xformD[2][0]*x + xformD[2][1]*y + xformD[2][2];
      }
      else
      {
        // xformD transform is applied to a curve derivative ((x,y) = vector)
        d1[0] = xformD[0][0]*x + xformD[0][1]*y;
        d1[1] = xformD[1][0]*x + xformD[1][1]*y;
        d1[2] = xformD[2][0]*x + xformD[2][1]*y;
      }
      d1 -= array_stride;

      // The partial involving a all derivatives with respect to "u"
      // xformP transformation is applied to a curve derivative ((x,y) = vector)
      d1[0] = xformP[0][0]*dx + xformP[0][1]*dy;
      d1[1] = xformP[1][0]*dx + xformP[1][1]*dy;
      d1[2] = xformP[2][0]*dx + xformP[2][1]*dy;
      d1 -= array_stride;
    }
  }
  // xformP transformation is applied curve location ((x,y) = point)
  d1[0] = xformP[0][0]*x + xformP[0][1]*y + xformP[0][2];
  d1[1] = xformP[1][0]*x + xformP[1][1]*y + xformP[1][2];
  d1[2] = xformP[2][0]*x + xformP[2][1]*y + xformP[2][2];

  return true;
}

ON_Curve* ON_Beam::IsoCurve(
       int dir,
       double c
       ) const
{
  // dir 0 first parameter varies and second parameter is constant
  //       e.g., point on IsoCurve(0,c) at t is srf(t,c)
  //     1 first parameter is constant and second parameter varies
  //       e.g., point on IsoCurve(1,c) at t is srf(c,t)

  if ( !m_profile )
    return 0;

  if ( m_bTransposed )
    dir = 1-dir;
  const ON_3dVector T = m_path.Tangent();

  ON_Xform xform0, xform1;
  if ( !ON_GetEndCapTransformation(m_path.PointAt(m_t.m_t[0]),T,m_up,m_bHaveN[0]?&m_N[0]:0,xform0,0,0) )
    return 0;
  if ( !ON_GetEndCapTransformation(m_path.PointAt(m_t.m_t[1]),T,m_up,m_bHaveN[1]?&m_N[1]:0,xform1,0,0) )
    return 0;

  ON_Curve*  isocurve = 0;
  if ( 1 == dir )
  {
    ON_3dPoint P = m_profile->PointAt(c);
    ON_LineCurve* line_curve = new ON_LineCurve();
    line_curve->m_t = m_path_domain;
    line_curve->m_dim = 3;
    line_curve->m_line.from = xform0*P;
    line_curve->m_line.to = xform1*P;
    isocurve = line_curve;
  }
  else if ( 0 == dir )
  {
    double s1 = m_path_domain.NormalizedParameterAt(c);
    const double s0 = 1.0-s1;
    xform1.m_xform[0][0] = s0*xform0.m_xform[0][0] + s1*xform1.m_xform[0][0];
    xform1.m_xform[0][1] = s0*xform0.m_xform[0][1] + s1*xform1.m_xform[0][1];
    xform1.m_xform[0][2] = s0*xform0.m_xform[0][2] + s1*xform1.m_xform[0][2];
    xform1.m_xform[0][3] = s0*xform0.m_xform[0][3] + s1*xform1.m_xform[0][3];

    xform1.m_xform[1][0] = s0*xform0.m_xform[1][0] + s1*xform1.m_xform[1][0];
    xform1.m_xform[1][1] = s0*xform0.m_xform[1][1] + s1*xform1.m_xform[1][1];
    xform1.m_xform[1][2] = s0*xform0.m_xform[1][2] + s1*xform1.m_xform[1][2];
    xform1.m_xform[1][3] = s0*xform0.m_xform[1][3] + s1*xform1.m_xform[1][3];

    xform1.m_xform[2][0] = s0*xform0.m_xform[2][0] + s1*xform1.m_xform[2][0];
    xform1.m_xform[2][1] = s0*xform0.m_xform[2][1] + s1*xform1.m_xform[2][1];
    xform1.m_xform[2][2] = s0*xform0.m_xform[2][2] + s1*xform1.m_xform[2][2];
    xform1.m_xform[2][3] = s0*xform0.m_xform[2][3] + s1*xform1.m_xform[2][3];

    xform1.m_xform[3][0] = s0*xform0.m_xform[3][0] + s1*xform1.m_xform[3][0];
    xform1.m_xform[3][1] = s0*xform0.m_xform[3][1] + s1*xform1.m_xform[3][1];
    xform1.m_xform[3][2] = s0*xform0.m_xform[3][2] + s1*xform1.m_xform[3][2];
    xform1.m_xform[3][3] = s0*xform0.m_xform[3][3] + s1*xform1.m_xform[3][3];

    isocurve = m_profile->DuplicateCurve();
    if ( isocurve )
    {
      isocurve->ChangeDimension(3);
      if ( !isocurve->Transform(xform1) )
      {
        // isocurve is probably a circle
        ON_NurbsCurve* nurbs_curve = isocurve->NurbsCurve();
        delete isocurve;
        isocurve = nurbs_curve;
        nurbs_curve = 0;
        if ( isocurve )
          isocurve->Transform(xform1);
      }
    }
  }

  return isocurve;
}


//ON_Curve* ON_Beam::Pushup( const ON_Curve& curve_2d,
//                  double tolerance,
//                  const ON_Interval* curve_2d_subdomain = NULL
//                  ) const
//ON_Curve* ON_Beam::Pullback( const ON_Curve& curve_3d,
//                  double tolerance,
//                  const ON_Interval* curve_3d_subdomain = NULL,
//                  ON_3dPoint start_uv = ON_UNSET_POINT,
//                  ON_3dPoint end_uv = ON_UNSET_POINT
//                  ) const
ON_BOOL32 ON_Beam::Trim(
       int dir,
       const ON_Interval& domain
       )
{
  bool rc = false;
  if (!domain.IsIncreasing())
    return false;
  if ( m_bTransposed )
    dir = 1-dir;
  if ( 1 == dir )
  {
    rc = m_path_domain.IsIncreasing();
    if ( rc && m_path_domain != domain )
    {
      ON_Interval dom;
      dom.Intersection(domain,m_path_domain);
      rc = dom.IsIncreasing();
      if (rc)
      {
        double s0 = m_path_domain.NormalizedParameterAt(dom[0]);
        double s1 = m_path_domain.NormalizedParameterAt(dom[1]);
        double t0 = (1.0-s0)*m_t[0] + s0*m_t[1];
        double t1 = (1.0-s1)*m_t[0] + s1*m_t[1];
        rc = (s0 < s1 && 0.0 <= t0 && t0 < t1 && t1 <= 1.0);
        if (rc)
        {
          bool bChanged = false;
          if (t0 != m_t[0] && t0 > 0.0 )
          {
            bChanged = true;
            m_t[0] = t0;
            m_bHaveN[0] = false;
          }
          if ( t1 != m_t[1] && t1 < 1.0 )
          {
            bChanged = true;
            m_t[1] = t1;
            m_bHaveN[1] = false;
          }
          if ( bChanged )
          {
            m_path_domain = dom;
            DestroySurfaceTree();
          }
        }
      }
    }
  }
  else if ( 0 == dir )
  {
    if ( m_profile )
    {
      rc = m_profile->Trim(domain)?true:false;
      DestroySurfaceTree();
    }
  }
  return rc;
}

bool ON_Beam::Extend(
  int dir,
  const ON_Interval& domain
  )
{
  bool rc = false;
  if ( 1 == dir )
  {
    rc = domain.IsIncreasing() && m_path_domain.IsIncreasing();
    if ( rc )
    {
      double s0 = m_path_domain.NormalizedParameterAt(domain[0]);
      if ( s0 > 0.0 )
        s0 = 0.0;
      double s1 = m_path_domain.NormalizedParameterAt(domain[1]);
      if ( s1 < 1.0 )
        s1 = 1.0;
      double t0 = (1.0-s0)*m_t[0] + s0*m_t[1];
      double t1 = (1.0-s1)*m_t[0] + s1*m_t[1];
      bool bChanged = false;
      ON_3dPoint P0 = m_path.from;
      ON_3dPoint P1 = m_path.to;
      if ( t0 < m_t[0] )
      {
        bChanged = true;
        m_path_domain.m_t[0] = domain[0];
        if ( t0 < 0.0 )
        {
          P0 = m_path.PointAt(t0);
          m_t[0] = 0.0;
        }
        else
          m_t[0] = t0;
      }
      if ( t1 > m_t[1] )
      {
        bChanged = true;
        m_path_domain.m_t[1] = domain[1];
        if ( t1 > 1.0 )
        {
          P1 = m_path.PointAt(t1);
          m_t[1] = 1.0;
        }
        else
          m_t[1] = t1;
      }
      if ( bChanged )
      {
        m_path.from = P0;
        m_path.to = P1;
        DestroySurfaceTree();
      }
    }
  }
  else if ( 0 == dir )
  {
    if ( m_profile )
    {
      rc = m_profile->Extend(domain);
      if (rc) 
        DestroySurfaceTree();
    }
  }
  return rc;
}

ON_BOOL32 ON_Beam::Split(
       int dir,
       double c,
       ON_Surface*& west_or_south_side,
       ON_Surface*& east_or_north_side
       ) const
{
  if ( dir < 0 || dir > 1 || !ON_IsValid(c) )
    return false;
  if ( 0 != west_or_south_side && west_or_south_side == east_or_north_side )
    return false;

  ON_Interval domain = Domain(dir);
  double s = domain.NormalizedParameterAt(c);
  if ( s <= 0.0 || s >= 1.0 )
    return false;
  if (c <= domain[0] || c >= domain[1] )
    return false;

  ON_Beam* left = 0;
  ON_Beam* right = 0;
  if ( west_or_south_side )
  {
    left = ON_Beam::Cast(west_or_south_side);
    if ( !left )
      return false;
  }
  if ( east_or_north_side )
  {
    right = ON_Beam::Cast(east_or_north_side);
    if ( !right )
      return false;
  }

  const int path_dir = PathParameter();
  bool rc = false;
  if ( dir == path_dir )
  {
    // split path
    ON_Line left_path, right_path;
    ON_Interval left_domain, right_domain;
    ON_Interval left_t, right_t;

    const double t0 = m_t[0];
    const double t1 = m_t[1];
    const double t = (1.0-s)*t0 + s*t1;
    if ( !ON_IsValid(t) || t <= t0 || t >= t1 )
      return false;

    ON_3dPoint P = m_path.PointAt(s);
    left_path.from = m_path.from;
    left_path.to = P;
    right_path.from = P;
    right_path.to = m_path.to;
    left_domain.Set(domain[0],c);
    right_domain.Set(c,domain[1]);
    left_t.Set(t0,t);
    right_t.Set(t,t1);
    if ( !left_path.IsValid() || left_path.Length() <= m_path_length_min )
      return false;
    if ( !right_path.IsValid() || right_path.Length() <= m_path_length_min )
      return false;

    // return result
    if ( !left )
      left = new ON_Beam(*this);
    else if ( left != this )
      left->operator =(*this);
    else
      left->DestroyRuntimeCache();
    if ( !right )
      right = new ON_Beam(*this);
    else if ( right != this )
      right->operator =(*this);
    else
      right->DestroyRuntimeCache();
    left->m_path = left_path;
    left->m_path_domain = left_domain;
    left->m_t = left_t;
    right->m_path = right_path;
    right->m_path_domain = right_domain;
    right->m_t = right_t;

    west_or_south_side = left;
    east_or_north_side = right;
    rc = true;
  }
  else
  {
    if ( 0 == m_profile )
      return false;
    ON_Curve* left_profile = 0;
    ON_Curve* right_profile = 0;

    if ( left == this )
    {
      left_profile = left->m_profile;
      left->DestroyRuntimeCache();
    }
    else if ( 0 != left && 0 != left->m_profile )
    {
      delete left->m_profile;
      left->m_profile = 0;
    }

    if ( right == this )
    {
      right_profile = right->m_profile;
      right->DestroyRuntimeCache();
    }
    else if ( 0 != right && 0 != right->m_profile )
    {
      delete right->m_profile;
      right->m_profile = 0;
    }

    if ( !m_profile->Split(c,left_profile,right_profile) )
      return false;
    if ( 0 == left_profile || 0 == right_profile )
    {
      if ( 0 != left_profile && m_profile != left_profile )
        delete left_profile;
      if ( 0 != right_profile && m_profile != right_profile )
        delete right_profile;
      return false;
    }

    ON_Curve* this_profile = 0;
    if ( left_profile != m_profile && right_profile != m_profile )
    {
      if ( left == this || right == this )
      {
        delete m_profile;
      }
      else
      {
        this_profile = m_profile;
      }
    }

    // Prevent this m_profile from being copied
    const_cast<ON_Beam*>(this)->m_profile = 0;

    // Create new left and right sides with NULL profiles
    if ( !left )
      left = new ON_Beam(*this);
    else if ( left != this )
      left->operator =(*this);
    if ( !right )
      right = new ON_Beam(*this);
    else if ( right != this )
      right->operator =(*this);

    // Restore this m_profile
    const_cast<ON_Beam*>(this)->m_profile = this_profile;

    // Set left and right profiles
    left->m_profile = left_profile;
    right->m_profile = right_profile;

    west_or_south_side = left;
    east_or_north_side = right;
    rc = true;
  }

  return rc;
}

bool ON_Beam::GetClosestPoint( 
        const ON_3dPoint& P,
        double* s,
        double* t,
        double maximum_distance,
        const ON_Interval* sdomain,
        const ON_Interval* tdomain
        ) const
{
  if ( 0 == m_profile )
    return false;

  if ( !P.IsValid() )
    return false;

  const ON_Line path(PathStart(),PathEnd());
  if ( !path.IsValid() )
    return false;

  double path_parameter=ON_UNSET_VALUE;
  if ( !path.ClosestPointTo(P,&path_parameter) || !ON_IsValid(path_parameter) )
    return false;

  const ON_3dPoint path_P = path.PointAt(path_parameter);
  const ON_3dVector zaxis = m_path.Tangent();
  const ON_3dVector yaxis = m_up;
  ON_3dVector xaxis = ON_CrossProduct(yaxis,zaxis);
  if ( !xaxis.IsUnitVector() )
    xaxis.Unitize();

  const ON_3dPoint profile_2dP(xaxis*(P - path_P),yaxis*(P - path_P),0.0);
  if ( !profile_2dP.IsValid() )
    return false;
  
  if ( m_bTransposed ) 
  {
    double* p1 = s; s=t; t = p1;
    const ON_Interval* p2=sdomain;sdomain=tdomain;tdomain=p2;
  }

  if ( 0 != tdomain )
  {
    if ( !tdomain->IsValid() || tdomain->IsDecreasing() )
      return false;
    if ( tdomain->m_t[0] > m_path_domain.m_t[1] )
      return false;
    if ( tdomain->m_t[1] < m_path_domain.m_t[0] )
      return false;
  }

  double profile_parameter=ON_UNSET_VALUE;
  if (    !m_profile->GetClosestPoint(profile_2dP,&profile_parameter,maximum_distance,tdomain) 
       || !ON_IsValid(profile_parameter)
     )
  {
    return false;
  }
  const ON_3dPoint Q2d = m_profile->PointAt(profile_parameter);

  ON_Line L;
  if ( IsMitered() )
  {
    ON_Xform xform0, xform1;
    if ( !ON_GetEndCapTransformation(path.from,zaxis,m_up,m_bHaveN[0]?&m_N[0]:0,xform0,0,0) )
      return false;
    if ( !ON_GetEndCapTransformation(path.to,zaxis,m_up,m_bHaveN[1]?&m_N[1]:0,xform1,0,0) )
      return false;
    L.from = xform0*Q2d;
    L.to = xform1*Q2d;
    if ( !L.ClosestPointTo(P,&path_parameter) || !ON_IsValid(path_parameter) )
      return false;
  }
  else
  {
    const ON_3dVector D = Q2d.x*xaxis + Q2d.y*yaxis;
    L.from = path.from + D;
    L.to   = path.to + D;
  }
  if ( path_parameter < 0.0 )
    path_parameter = 0.0;
  else if (path_parameter > 1.0)
    path_parameter = 1.0;
  
  double line_parameter = m_path_domain.ParameterAt(path_parameter);
  if ( 0 != tdomain )
  {
    if ( line_parameter < tdomain->m_t[0] )
    {
      line_parameter = tdomain->m_t[0];
      path_parameter = m_path_domain.NormalizedParameterAt(line_parameter);
    }
    else if ( line_parameter > tdomain->m_t[1] )
    {
      line_parameter = tdomain->m_t[1];
      path_parameter = m_path_domain.NormalizedParameterAt(line_parameter);
    }
  }
  if ( maximum_distance > 0.0 )
  {
    ON_3dPoint Q = L.PointAt(path_parameter);
    double d = Q.DistanceTo(P);
    if ( d > maximum_distance )
      return false;
  }
  if ( s )
    *s = profile_parameter;
  if ( t )
    *t = line_parameter;

  return true;
}


//ON_BOOL32 ON_Beam::GetLocalClosestPoint( const ON_3dPoint&, // test_point
//        double,double,     // seed_parameters
//        double*,double*,   // parameters of local closest point returned here
//        const ON_Interval* = NULL, // first parameter sub_domain
//        const ON_Interval* = NULL  // second parameter sub_domain
//        ) const
//ON_Surface* ON_Beam::Offset(
//      double offset_distance, 
//      double tolerance, 
//      double* max_deviation = NULL
//      ) const
int ON_Beam::GetNurbForm(
      ON_NurbsSurface& nurbs_surface,
      double tolerance
      ) const
{
  if ( !m_profile )
    return 0;

  ON_Xform xform0,xform1;
  const ON_3dVector pathT = m_path.Tangent();
  if ( !GetProfileTransformation(0,xform0) )
    return false;
  if ( !GetProfileTransformation(1,xform1) )
    return false;

  ON_NurbsCurve nc0;
  int rc = m_profile->GetNurbForm(nc0,tolerance);
  if ( rc <= 0 )
    return rc;
  if ( 3 != nc0.m_dim )
    nc0.ChangeDimension(3);
  ON_NurbsCurve nc1 = nc0;
  nc0.Transform(xform0);
  nc1.Transform(xform1);

  nurbs_surface.Create(3,nc0.m_is_rat,nc0.m_order,2,nc0.m_cv_count,2);
  memcpy(nurbs_surface.m_knot[0],nc0.m_knot,nurbs_surface.KnotCount(0)*sizeof(nurbs_surface.m_knot[0][0]));
  nurbs_surface.m_knot[1][0] = m_path_domain[0];
  nurbs_surface.m_knot[1][1] = m_path_domain[1];
  for ( int i = 0; i < nurbs_surface.m_cv_count[0]; i++ )
  {
    nurbs_surface.SetCV(i,0,ON::intrinsic_point_style,nc0.CV(i));
    nurbs_surface.SetCV(i,1,ON::intrinsic_point_style,nc1.CV(i));
  }

  return rc;
}

int ON_Beam::HasNurbForm() const
{
  return m_profile ? m_profile->HasNurbForm() : 0;
}

bool ON_Beam::GetSurfaceParameterFromNurbFormParameter(
      double nurbs_s, double nurbs_t,
      double* surface_s, double* surface_t
      ) const
{
  bool rc = true;
  if ( m_bTransposed )
  {
    double* p = surface_s; 
    surface_s = surface_t; 
    surface_t = p;
    double t = nurbs_s;
    nurbs_s = nurbs_t;
    nurbs_t = t;
  }
  if ( surface_s )
  {
    rc = m_profile 
       ? (m_profile->GetCurveParameterFromNurbFormParameter(nurbs_s,surface_s)?true:false) 
       : false;
  }
  if ( surface_t )
    *surface_t = nurbs_t;
  return rc;
}

bool ON_Beam::GetNurbFormParameterFromSurfaceParameter(
      double surface_s, double surface_t,
      double* nurbs_s,  double* nurbs_t
      ) const
{
  bool rc = true;
  if ( m_bTransposed )
  {
    double p = surface_s; 
    surface_s = surface_t; 
    surface_t = p;
    double* t = nurbs_s;
    nurbs_s = nurbs_t;
    nurbs_t = t;
  }
  if ( nurbs_s )
  {
    rc = m_profile 
      ? (m_profile->GetNurbFormParameterFromCurveParameter(surface_s,nurbs_s)?true:false)
      : false;
  }
  if ( nurbs_t )
    *nurbs_t = surface_t;
  return rc;
}

