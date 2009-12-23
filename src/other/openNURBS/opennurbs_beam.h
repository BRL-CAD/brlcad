#if !defined(OPENNURBS_BEAM_INC_)
#define OPENNURBS_BEAM_INC_

/*
Description:
  Get the transformation that maps the 2d beam sy profile 
  to 3d world space.
Parameters:
  P - [in] start or end of path
  T - [in] unit tanget to path
  U - [in] unit up vector perpindicular to T
  Normal - [in] optional unit vector with Normal->x > 0 that
     defines the unit normal to the miter plane.
  xform - [out]
    transformation that maps the profile curve to 3d world space
  scale2d - [out]
    If not NULL, this is the scale part of the transformation.
    If there is no mitering, then this is the identity.
  rot2d - [out]
    If not null, this is the part of the transformation
    that rotates the xy plane into its 3d world location.
Returns:
  true if successful.
*/
ON_DECL
bool ON_GetEndCapTransformation(
          ON_3dPoint P, 
          ON_3dVector T, 
          ON_3dVector U, 
          const ON_3dVector* Normal,
          ON_Xform& xform, // = rot3d*scale2d
          ON_Xform* scale2d,
          ON_Xform* rot2d
          );

class ON_CLASS ON_Beam : public ON_Surface
{
  ON_OBJECT_DECLARE(ON_Beam);
public:
  ON_Beam();
  ON_Beam(const ON_Beam& src);
  ~ON_Beam();

  ON_Beam& operator=(const ON_Beam&);

  ////////////////////////////////////////////////////////////
  //
  // overrides of virtual ON_Object functions
  // 
  ON_BOOL32 IsValid( ON_TextLog* text_log = NULL ) const;
  void Dump( ON_TextLog& ) const;
  unsigned int SizeOf() const;
  ON__UINT32 DataCRC( ON__UINT32 current_remainder ) const;
  ON_BOOL32 Write( ON_BinaryArchive& binary_archive) const;
  ON_BOOL32 Read( ON_BinaryArchive& binary_archive );
  ON::object_type ObjectType() const;

  ////////////////////////////////////////////////////////////
  //
  // overrides of virtual ON_Geometry functions
  // 
  int Dimension() const;
  ON_BOOL32 GetBBox(
        double* boxmin,
        double* boxmax,
        int bGrowBox = false
        ) const;
	bool GetTightBoundingBox( 
        ON_BoundingBox& tight_bbox, 
        int bGrowBox = false,
        const ON_Xform* xform = 0
        ) const;
  ON_BOOL32 Transform( 
        const ON_Xform& xform
        );
  ON_Brep* BrepForm(
        ON_Brep* brep = NULL 
        ) const;


  ////////////////////////////////////////////////////////////
  //
  // overrides of virtual ON_Surface functions
  // 
  ON_Mesh* CreateMesh( 
        const ON_MeshParameters& mp,
        ON_Mesh* mesh = NULL
        ) const;
  ON_BOOL32 SetDomain( 
        int dir,
        double t0, 
        double t1
        );
  ON_Interval Domain(
        int dir
        ) const;
  ON_BOOL32 GetSurfaceSize( 
        double* width, 
        double* height 
        ) const;
  int SpanCount(
        int dir
        ) const;
  ON_BOOL32 GetSpanVector(
        int dir,
        double* span_vector
        ) const;
  ON_BOOL32 GetSpanVectorIndex(
        int dir,
        double t,
        int side,
        int* span_vector_index,
        ON_Interval* span_interval
        ) const;
  int Degree(
        int dir
        ) const; 
  ON_BOOL32 GetParameterTolerance(
         int dir,
         double t,
         double* tminus,
         double* tplus
         ) const;
  ISO IsIsoparametric(
        const ON_Curve& curve,
        const ON_Interval* curve_domain = NULL
        ) const;
  ON_BOOL32 IsPlanar(
        ON_Plane* plane = NULL,
        double tolerance = ON_ZERO_TOLERANCE
        ) const;
  ON_BOOL32 IsClosed(
        int
        ) const;
  ON_BOOL32 IsPeriodic(
        int
        ) const;
  bool GetNextDiscontinuity( 
                  int dir,
                  ON::continuity c,
                  double t0,
                  double t1,
                  double* t,
                  int* hint=NULL,
                  int* dtype=NULL,
                  double cos_angle_tolerance=0.99984769515639123915701155881391,
                  double curvature_tolerance=ON_SQRT_EPSILON
                  ) const;
  bool IsContinuous(
    ON::continuity c,
    double s, 
    double t, 
    int* hint = NULL,
    double point_tolerance=ON_ZERO_TOLERANCE,
    double d1_tolerance=ON_ZERO_TOLERANCE,
    double d2_tolerance=ON_ZERO_TOLERANCE,
    double cos_angle_tolerance=0.99984769515639123915701155881391,
    double curvature_tolerance=ON_SQRT_EPSILON
    ) const;
  ISO IsIsoparametric(
        const ON_BoundingBox& bbox
        ) const;
  ON_BOOL32 Reverse( int dir );
  ON_BOOL32 Transpose();
  ON_BOOL32 Evaluate(
         double u, double v,
         int num_der,
         int array_stride,
         double* der_array,
         int quadrant = 0,
         int* hint = 0
         ) const;
  ON_Curve* IsoCurve(
         int dir,
         double c
         ) const;
  ON_BOOL32 Trim(
         int dir,
         const ON_Interval& domain
         );
  bool Extend(
    int dir,
    const ON_Interval& domain
    );
  ON_BOOL32 Split(
         int dir,
         double c,
         ON_Surface*& west_or_south_side,
         ON_Surface*& east_or_north_side
         ) const;
  bool GetClosestPoint( 
          const ON_3dPoint& P,
          double* s,
          double* t,
          double maximum_distance = 0.0,
          const ON_Interval* sdomain = 0,
          const ON_Interval* tdomain = 0
          ) const;

  //ON_BOOL32 GetLocalClosestPoint( const ON_3dPoint&, // test_point
  //        double,double,     // seed_parameters
  //        double*,double*,   // parameters of local closest point returned here
  //        const ON_Interval* = NULL, // first parameter sub_domain
  //        const ON_Interval* = NULL  // second parameter sub_domain
  //        ) const;
  //ON_Surface* Offset(
  //      double offset_distance, 
  //      double tolerance, 
  //      double* max_deviation = NULL
  //      ) const;

  int GetNurbForm(
        ON_NurbsSurface& nurbs_surface,
        double tolerance = 0.0
        ) const;
  int HasNurbForm() const;
  bool GetSurfaceParameterFromNurbFormParameter(
        double nurbs_s, double nurbs_t,
        double* surface_s, double* surface_t
        ) const;
  bool GetNurbFormParameterFromSurfaceParameter(
        double surface_s, double surface_t,
        double* nurbs_s,  double* nurbs_t
        ) const;

  ////////////////////////////////////////////////////////////
  //
  // ON_Beam interface
  // 
  void Destroy();

  /*
  Description:
    Sets m_path to (A,B), m_path_domain to [0,Length(AB)],
    and m_t to [0,1].
  Parameters:
    A - [in] path start
    B - [in] path end
  Returns:
    true  A and B are valid, the distance from A to B is larger
          than ON_ZERO_TOLERANCE, and the path was set.
    false A or B is not valid or the distance from A to B is
          at most ON_ZERO_TOLERANCE. In this case nothing is set.
  */
  bool SetPath(ON_3dPoint A, ON_3dPoint B);

  /*
  Description:
    Get the surface parameter for the path.
  Returns:
    0: The first surface parameter corresponds to the path direction.
       (m_bTransposed = true)
    1: The second surface parameter corresponds to the path direction.
       (m_bTransposed = false)
  Remarks:
    The default beam constructor sets m_bTransposed = false which
    corresponds to the 1 = PathDir().
  */
  int PathParameter() const;

  ON_3dPoint PathStart() const;
  ON_3dPoint PathEnd() const;
  ON_3dVector PathTangent() const;

  /*
  Description:
    Set miter plane normal.
  Parameters:
    N - [in] If ON_UNSET_VECTOR or N is parallel to the z-axis,
             then the miter plane is the default plane 
             perpindicular to the path.
             If N is valid and the z coordinate of a unitized
             N is greater than m_Nz_tol, then the miter plane 
             normal is set.
    end - [in] 0 = set miter plane at the start of the path.
               1 = set miter plane at the end of the path.
  */
  bool SetMiterPlaneNormal(ON_3dVector N, int end);

  void GetMiterPlaneNormal(int end, ON_3dVector& N) const;

  /*
  Returns:
    0: not mitered.
    1: start of path is mitered.
    2: end of path is mitered.
    3: start and end are mitered.
  */
  int IsMitered() const;

  /*
  Description:
    Get the transformation that maps the xy profile curve
    to its 3d location.
  Parameters:
    s - [in] 0.0 = starting profile
             1.0 = ending profile
  */
  bool GetProfileTransformation( double s, ON_Xform& xform ) const;

  // path definition:
  //   The line m_path must have length > m_path_length_min.
  //   The interval m_t must statisfy 0 <= m_t[0] < m_t[1] <= 1.
  //   The extrusion starts at m_path.PointAt(m_t[0]) and ends
  //   at m_path.PointAt(m_t[1]).
  //   The "up" direction m_up is a unit vector that must
  //   be perpindicular to m_path.Tangent().
  ON_Line m_path;
  ON_Interval m_t;
  ON_3dVector m_up;

  // profile information:
  //   The profile curve must be in the x-y plane.
  //   The profile's "y" axis corresponds to m_up.
  //   The point (0,0) is extruded along the m_path line.
  ON_Curve* m_profile; 

  // mitered end information:
  //   The normals m_N[] are with respect to the xy plane.
  //   A normal parallel to the z axis has no mitering.
  //   If m_bHaveN[i] is true, then m_N[i] must be a 3d unit
  //   vector with m_N[i].z > m_Nz_tol;  If m_bHaveN[i]
  //   is false, then m_N[i] is ignored.  The normal m_N[0]
  //   defines the start miter plane and m_N[1] defines the
  //   end miter plane.
  bool m_bHaveN[2];
  ON_3dVector m_N[2];

  // Surface parameterization information
  ON_Interval m_path_domain;
  bool m_bTransposed; // false: (s,t) = (profile,path)

  // The z coordinates of miter plane normals must be
  // greater than m_Nz_tol
  static const double m_Nz_min; // 1/64;

  // The length of the m_path line must be greater than
  // m_path_length_min
  static const double m_path_length_min; // ON_ZERO_TOLERANCE;
};

#endif

