/* $Header$ */
/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2001 Robert McNeel & Associates. All rights reserved.
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

////////////////////////////////////////////////////////////////
//
//   defines ON_Viewport
//
////////////////////////////////////////////////////////////////

#if !defined(OPENNURBS_VIEWPORT_INC_)
#define OPENNURBS_VIEWPORT_INC_

///////////////////////////////////////////////////////////////////////////////
// Class  ON_Viewport
//
//	This object represents a viewing frustum
///////////////////////////////////////////////////////////////////////////////
class ON_CLASS ON_Viewport : public ON_Geometry 
{
	ON_OBJECT_DECLARE( ON_Viewport );
public:

  // Construction
	ON_Viewport();
  ~ON_Viewport();
	ON_Viewport& operator=( const ON_Viewport& );

  bool IsValidCamera() const;
  bool IsValidFrustum() const;

  // ON_Object overrides //////////////////////////////////////////////////////
  //

  /*
  Description:
    Test for a valid camera, frustum, and screen port.
  Parameters:
    text_log - [in] if the object is not valid and text_log
        is not NULL, then a brief englis description of the
        reason the object is not valid is appened to the log.
        The information appended to text_log is suitable for 
        low-level debugging purposes by programmers and is 
        not intended to be useful as a high level user 
        interface tool.
  Returns:
    @untitled table
    TRUE     camera, frustum, and screen port are valid.
    FALSE    camera, frustum, or screen port is invalid.
  Remarks:
    Overrides virtual ON_Object::IsValid
  See Also:
    IsValidCamera, IsValidFrustum
  */
  BOOL IsValid( ON_TextLog* text_log = NULL ) const;

  // Description:
  //   Dumps debugging text description to a text log.
  //
  // Parameters:
  //   dump_target - [in] text log
  //
  // Remarks:
  //   This overrides the virtual ON_Object::Dump() function.
  void Dump( 
    ON_TextLog& // dump_target
    ) const;

  // Description:
  //   Writes ON_Viewport defintion from a binary archive.
  //
  // Parameters:
  //   binary_archive - [in] open binary archive
  //
  // Returns:
  //   TRUE if successful.
  //
  // Remarks:
  //   This overrides the virtual ON_Object::Write() function.
  BOOL Write(
         ON_BinaryArchive&  // binary_archive
       ) const;


  // Description:
  //   Reads ON_Viewport defintion from a binary archive.
  //
  // Parameters:
  //   binary_archive - [in] open binary archive
  //
  // Returns:
  //   TRUE if successful.
  //
  // Remarks:
  //   This overrides the virtual ON_Object::Read() function.
  BOOL Read(
         ON_BinaryArchive&  // binary_archive
       );


  // ON_Geometry overrides //////////////////////////////////////////////////////
  //

  // Description:
  //   The dimension of a camera view frustum is 3.
  //
  // Returns:
  //   3
  //
  // Remarks:
  //   This is virtual ON_Geometry function.
  int Dimension() const;

  // Description:
  //   Gets bounding box of viewing frustum.
  //
  // Parameters:
  //   boxmin - [in/out] array of Dimension() doubles
  //   boxmax - [in/out] array of Dimension() doubles
  //   bGrowBox - [in] (default=FALSE) 
  //     If TRUE, then the union of the input bbox and the 
  //     object's bounding box is returned in bbox.  
  //     If FALSE, the object's bounding box is returned in bbox.
  //
  // Returns:
  //   @untitled table
  //   TRUE     Valid frustum and bounding box returned.
  //   FALSE    Invalid camera or frustum. No bounding box returned.
  //
  // Remarks:
  //   This overrides the virtual ON_Geometry::GetBBox() function.
  BOOL GetBBox( // returns TRUE if successful
         double*, // boxmin
         double*, // boxmax
         BOOL = FALSE // bGrowBox
         ) const;

  // Description:
  //   Transforms the view camera location, direction, and up.
  //
  // Parameters:
  //   xform - [in] transformation to apply to camera.
  //
  // Returns:
  //   @untitled table
  //   TRUE     Valid camera was transformed.
  //   FALSE    Invalid camera, frustum, or transformation.
  //
  // Remarks:
  //   This overrides the virtual ON_Geometry::Transform() function.
  BOOL Transform( 
         const ON_Xform& // xform
         );

  // Interface /////////////////////////////////////////////////////////////////
  //
  void Initialize();

  bool SetProjection( ON::view_projection );
  ON::view_projection Projection() const;

  // These return TRUE if the current direction and up are not zero and not
  // parallel so the camera position is well defined.
  bool SetCameraLocation( const ON_3dPoint& );
  bool SetCameraDirection( const ON_3dVector& );
  bool SetCameraUp( const ON_3dVector& );

  ON_3dPoint CameraLocation() const;
  ON_3dVector CameraDirection() const;
  ON_3dVector CameraUp() const;

  // returns TRUE if current camera orientation is valid
  bool GetCameraFrame(
      double*, // CameraLocation[3]
      double*, // CameraX[3]
      double*, // CameraY[3]
      double*  // CameraZ[3]
      ) const;
  // these do not check for a valid camera orientation
  ON_3dVector CameraX() const; // unit to right vector
  ON_3dVector CameraY() const; // unit up vector
  ON_3dVector CameraZ() const; // unit vector in -CameraDirection

  
  bool IsCameraFrameWorldPlan( 
      // Returns true if the camera direction = some world axis.
      // The indices report which axes are used.  For a "twisted"
      // plan view it is possible to have zero x and y indices.
      // This function returns true if and only if the "z" index
      // is non-zero.
      //
      // Indices are +/-1 = world +/-x, +/-2 = world +/-y, +/-3 = world +/-z,
      int*, // if true and plan is axis aligned, view x index, else 0
      int*, // if true and plan is axis aligned, view y index, else 0
      int*  // if true, view z index, else 0
      );

  bool GetCameraExtents( 
      // returns bounding box in camera coordinates - this is useful information
      // for setting view frustrums to include the point list
      int,           // count = number of 3d points
      int,           // stride = number of doubles to skip between points (>=3)
      const double*, // 3d points in world coordinates
      ON_BoundingBox& cambbox, // bounding box in camera coordinates
      int bGrowBox = false   // set to TRUE if you want to enlarge an existing camera coordinate box
      ) const;

  bool GetCameraExtents( 
      // returns bounding box in camera coordinates - this is useful information
      // for setting view frustrums to include the point list
      const ON_BoundingBox&, // world coordinate bounding box
      ON_BoundingBox& cambbox, // bounding box in camera coordinates
      int bGrowBox = false   // set to TRUE if you want to enlarge an existing camera coordinate box
      ) const;

  bool GetCameraExtents( 
      // returns bounding box in camera coordinates - this is useful information
      // for setting view frustrums to include the point list
      ON_3dPoint&,     // world coordinate bounding sphere center
      double,          // world coordinate bounding sphere radius
      ON_BoundingBox& cambox, // bounding box in camera coordinates
      int bGrowBox = false     // set to TRUE if you want to enlarge an existing camera coordinate box
      ) const;

  // Rhino version 1.0 does not support skew frustums; i.e., the
  // viewing frustums used in Rhino have left = -right, bottom = -top.
  // If you specify a skew frustum, all the ON_Viewport functions
  // will work fine.  If you save it a file and read the file into Rhino 1.0,
  // then you will get a symmetric approximation.
  bool SetFrustum(
        double left,   // 
        double right,  //   ( left < right )
        double bottom, // 
        double top,    //   ( bottom < top )
        double near_dist,   // 
        double far_dist     //   ( 0 < near_dist < far_dist ) // ignored by Rhino version 1.0
        );
  bool GetFrustum(
        double* left,        // 
        double* right,       // (left < right)
        double* bottom,      // 
        double* top,         // (bottom < top)
        double* near_dist = NULL, // 
        double* far_dist = NULL   // (0 < near_dist < far_dist)
        ) const;

  // SetFrustumAspect() changes the larger of the frustum's widht/height
  // so that the resulting value of width/height matches the requested
  // aspect.  The camera angle is not changed.  If you change the shape
  // of the view port with a call SetScreenPort(), then you generally 
  // want to call SetFrustumAspect() with the value returned by 
  // GetScreenPortAspect().
  bool SetFrustumAspect( double );

  // Returns frustum's width/height
  bool GetFrustumAspect( double& ) const;

  // Returns world coordinates of frustum's center
  bool GetFrustumCenter( double* ) const;

  // The near clipping plane stored in the Rhino 1.0 file is frequently very
  // small and useless for high quality z-buffer based rendering.  The far
  // clipping value is not stored in the file.  Use these functions to set
  // the frustum's near and far clipping planes to appropriate values.
  double FrustumLeft() const;
  double FrustumRight() const;
  double FrustumBottom() const;
  double FrustumTop() const;
  double FrustumNear() const;
  double FrustumFar() const;

  bool SetFrustumNearFar(       
         const double* bboxmin,  // 3d bounding box min
         const double* bboxmax   // 3d bounding box max
         );
  bool SetFrustumNearFar( 
         const double* center,  // 3d bounding sphere center
         double radius         // 3d bounding sphere radius
         );
  bool SetFrustumNearFar( 
         double near_dist, // ( > 0 )
         double far_dist   // 
         );

  /*
    Get near and far clipping distances of a point
  Parameters:
    point - [in] 
    near_dist - [out] 
      near distance of the point (can be < 0)
    far_dist - [out] 
      far distance of the point (can be equal to near_dist)
    bGrowNearFar - [in]
      If true and input values of near_dist and far_dist
      are not ON_UNSET_VALUE, the near_dist and far_dist
      are enlarged to include bbox.
  Returns:
    True if the point is ing the view frustum and
    near_dist/far_dist were set.
    False if the bounding box does not intesect the
    view frustum.
  */
  bool GetPointDepth(       
         ON_3dPoint point,
         double* near_dist,
         double* far_dist,
         bool bGrowNearFar=false
         ) const;

  /*
  Description:
    Get near and far clipping distances of a bounding box
  Parameters:
    bbox - [in] 
      bounding box
    near_dist - [out] 
      near distance of the box (can be < 0)
    far_dist - [out] 
      far distance of the box (can be equal to near_dist)
    bGrowNearFar - [in]
      If true and input values of near_dist and far_dist
      are not ON_UNSET_VALUE, the near_dist and far_dist
      are enlarged to include bbox.
  Returns:
    True if the bounding box intersects the view frustum and
    near_dist/far_dist were set.
    False if the bounding box does not intesect the view frustum.
  */
  bool GetBoundingBoxDepth(       
         ON_BoundingBox bbox,
         double* near_dist,
         double* far_dist,
         bool bGrowNearFar=false
         ) const;

  /*
  Description:
    Get near and far clipping distances of a bounding sphere.
  Parameters:
    sphere - [in] 
      bounding sphere
    near_dist - [out] 
      near distance of the sphere (can be < 0)
    far_dist - [out] 
      far distance of the sphere (can be equal to near_dist)
    bGrowNearFar - [in]
      If true and input values of near_dist and far_dist
      are not ON_UNSET_VALUE, the near_dist and far_dist
      are enlarged to include bbox.
  Returns:
    True if the sphere intersects the view frustum and
    near_dist/far_dist were set.
    False if the sphere does not intesect the view frustum.
  */
  bool GetSphereDepth( 
         ON_Sphere sphere,
         double* near_dist,
         double* far_dist,
         bool bGrowNearFar=false
         ) const;

  /*
  Description:
    Set near and far clipping distance subject to constraints.
  Parameters:
    near_dist - [in] (>0) desired near clipping distance
    far_dist - [in] (>near_dist) desired near clipping distance
    min_near_dist - [in] 
      If min_near_dist <= 0.0, it is ignored.
      If min_near_dist > 0 and near_dist < min_near_dist, 
      then the frustum's near_dist will be increased to 
      min_near_dist.
    min_near_over_far - [in] 
      If min_near_over_far <= 0.0, it is ignored.
      If near_dist < far_dist*min_near_over_far, then
      near_dist is increased and/or far_dist is decreased
      so that near_dist = far_dist*min_near_over_far.
      If near_dist < target_dist < far_dist, then near_dist
      near_dist is increased and far_dist is decreased so that
      projection precision will be good at target_dist.
      Otherwise, near_dist is simply set to 
      far_dist*min_near_over_far.
    target_dist - [in]  
      If target_dist <= 0.0, it is ignored.
      If target_dist > 0, it is used as described in the
      description of the min_near_over_far parameter.
  */
  bool SetFrustumNearFar( 
         double near_dist,
         double far_dist,
         double min_near_dist,
         double min_near_over_far,
         double target_dist
         );

  // Description:
  //   Get near clipping plane.
  //
  //  near_plane - [out] near clipping plane if camera and frustum
  //      are valid.  The plane's frame is the same as the camera's
  //      frame.  The origin is located at the intersection of the
  //      camera direction ray and the near clipping plane.
  //
  // Returns:
  //   TRUE if camera and frustum are valid.
  bool GetNearPlane( 
    ON_Plane& near_plane 
    ) const;

  // Description:
  //   Get far clipping plane.
  //
  //  far_plane - [out] far clipping plane if camera and frustum
  //      are valid.  The plane's frame is the same as the camera's
  //      frame.  The origin is located at the intersection of the
  //      camera direction ray and the far clipping plane.
  //
  // Returns:
  //   TRUE if camera and frustum are valid.
  bool GetFarPlane( 
    ON_Plane& far_plane 
    ) const;

  /*
  Description:
  Get left world frustum clipping plane.
  Parameters:
    left_plane - [out] 
      frustum left side clipping plane.  The normal points
      into the visible region of the frustum.  If the projection
      is perspective, the origin is at the camera location,
      otherwise the origin isthe point on the plane that is
      closest to the camera location.
  Returns:
    True if camera and frustum are valid and plane was set.
  */
  bool GetFrustumLeftPlane( 
    ON_Plane& left_plane 
    ) const;

  /*
  Description:
  Get right world frustum clipping plane.
  Parameters:
    right_plane - [out] 
      frustum right side clipping plane.  The normal points
      out of the visible region of the frustum.  If the projection
      is perspective, the origin is at the camera location,
      otherwise the origin isthe point on the plane that is
      closest to the camera location.
  Returns:
    True if camera and frustum are valid and plane was set.
  */
  bool GetFrustumRightPlane( 
    ON_Plane& right_plane 
    ) const;

  /*
  Description:
  Get right world frustum clipping plane.
  Parameters:
    right_plane - [out] 
      frustum bottom side clipping plane.  The normal points
      into the visible region of the frustum.  If the projection
      is perspective, the origin is at the camera location,
      otherwise the origin isthe point on the plane that is
      closest to the camera location.
  Returns:
    True if camera and frustum are valid and plane was set.
  */
  bool GetFrustumBottomPlane( 
    ON_Plane& bottom_plane 
    ) const;

  /*
  Description:
  Get right world frustum clipping plane.
  Parameters:
    top_plane - [out] 
      frustum top side clipping plane.  The normal points
      into the visible region of the frustum.  If the projection
      is perspective, the origin is at the camera location,
      otherwise the origin isthe point on the plane that is
      closest to the camera location.
  Returns:
    True if camera and frustum are valid and plane was set.
  */
  bool GetFrustumTopPlane( 
    ON_Plane& top_plane 
    ) const;

  // Description:
  //   Get corners of near clipping plane rectangle.
  //
  // Parameters:
  //   left_bottom - [out] 
  //   right_bottom - [out]
  //   left_top - [out]
  //   right_top - [out]
  //
  // Returns:
  //   TRUE if camera and frustum are valid.
  bool GetNearRect( 
          ON_3dPoint& left_bottom,
          ON_3dPoint& right_bottom,
          ON_3dPoint& left_top,
          ON_3dPoint& right_top
          ) const;

  // Description:
  //   Get corners of far clipping plane rectangle.
  //
  // Parameters:
  //   left_bottom - [out] 
  //   right_bottom - [out]
  //   left_top - [out]
  //   right_top - [out]
  //
  // Returns:
  //   TRUE if camera and frustum are valid.
  bool GetFarRect( 
          ON_3dPoint& left_bottom,
          ON_3dPoint& right_bottom,
          ON_3dPoint& left_top,
          ON_3dPoint& right_top
          ) const;


  /*
  Description:
    Location of viewport in pixels.
    These are provided so you can set the port you are using
    and get the appropriate transformations to and from
    screen space.
  Parameters:
    port_left - [in]
    port_right - [in] (port_left != port_right)
    port_bottom - [in]
    port_top - [in] (port_top != port_bottom)
    port_near - [in]
    port_far - [in]
  Example:

          // For a Windows window
          int width = width of window client area in pixels;
          int height = height of window client area in pixels;
          port_left = 0;
          port_right = width;
          port_top = 0;
          port_bottom = height;
          port_near = 0;
          port_far = 1;
          SetScreenPort( port_left, port_right, 
                         port_bottom, port_top, 
                         port_near, port_far );

  Returns:
    true if input is valid.
  See Also:
    ON_Viewport::GetScreenPort
  */
  bool SetScreenPort(
        int port_left,
        int port_right,
        int port_bottom,
        int port_top,
        int port_near = 0,
        int port_far = 0
        );

  bool GetScreenPort(
        int* left,
        int* right,         //( port_left != port_right )
        int* port_bottom,
        int* port_top,      //( port_bottom != port_top)
        int* port_near=NULL,  
        int* port_far=NULL   
        ) const;

  bool GetScreenPortAspect( double& ) const; // port's |width/height|

  bool GetCameraAngle( 
          double* half_diagonal_angle, // 1/2 of diagonal subtended angle
          double* half_vertical_angle, // 1/2 of vertical subtended angle
          double* half_horizontal_angle // 1/2 of horizontal subtended angle
          ) const;
  bool GetCameraAngle( 
          double* half_smallest_angle  // 1/2 of smallest subtended view angle
          ) const;
  bool SetCameraAngle( 
          double half_smallest_angle // 1/2 of smallest subtended view angle
                  // 0 < angle < pi/2
          );

  // These functions assume the camera is horizontal and crop the
  // film rather than the image when the aspect of the frustum
  // is not 36/24.  (35mm film is 36mm wide and 24mm high.)
  //
  // The SetCamera35mmLenseLength() preserves camera location,
  // changes the frustum, but maintains the frsutrum's aspect.
  bool GetCamera35mmLenseLength( 
    double* lense_length 
    ) const;
  bool SetCamera35mmLenseLength( 
    double lense_length 
    );

  bool GetXform( 
         ON::coordinate_system srcCS,
         ON::coordinate_system destCS,
         ON_Xform& matrix      // 4x4 transformation matrix (acts on the left)
         ) const;

  /*
  Description:
    Get the world coordinate line in the view frustum
    that projects to a point on the screen.
  Parameters:
    screenx - [in]
    screeny - [in] (screenx,screeny) = screen location
    world_line - [out] 3d world coordinate line segment
           starting on the near clipping plane and ending 
           on the far clipping plane.
  Returns:
    true if successful. 
    false if view projection or frustum is invalid.
  */
  bool GetFrustumLine( 
            double screenx, 
            double screeny, 
            ON_Line& world_line
            ) const;

  // display tools
  bool GetWorldToScreenScale( 
    const ON_3dPoint& point_in_frustum, // [in]  point in viewing frustum.
    double* pixels_per_unit             // [out] scale = number of pixels per world unit at the 3d point
    ) const;

  bool GetCoordinateSprite(
         int,        // size in pixels of coordinate sprite axes
         int, int,   // screen (x,y) for sprite origin
         int[3],     // returns depth order for axes
         double [3][2]  // screen coords for axes ends
         ) const;

  // Use Extents() as a quick way to set a viewport to so that bounding
  // volume is inside of a viewports frusmtrum.
  // The view angle is used to determine the position of the camera.
  bool Extents( 
         double half_view_angle,        // 1/2 smallest subtended view angle
                        // (0 < angle < pi/2)
         const ON_BoundingBox& world_bbox// 3d world coordinate bounding box
         );
  bool Extents( 
         double half_view_angle,        // 1/2 smallest subtended view angle
                        // (0 < angle < pi/2)
         const ON_3dPoint& center, // 3d world coordinate bounding sphere center
         double radius        // 3d sphere radius
         );

  ////////////////////////////////////////////////////////////////////////
  // View changing from screen input points.  Handy for
  // using a mouse to manipulate a view.
  //

  //////////
  // ZoomToScreenRect() may change camera and frustum settings
  bool ZoomToScreenRect(
         int screen_x0, 
         int screen_y0,  // (x,y) screen coords of a rectangle corner
         int screen_x1, 
         int screen_y1   // (x,y) screen coords of opposite rectangle corner
         );

  //////////
  // DollyCamera() does not update the frustum's clipping planes.
  // To update the frustum's clipping planes call DollyFrustum(d)
  // with d = dollyVector o cameraFrameZ.  To convert screen locations
  // into a dolly vector, use GetDollyCameraVector().
  bool DollyCamera( // Does not update frustum.  To update frustum use 
                    // DollyFrustum(d) with d = dollyVector o cameraFrameZ
          const ON_3dVector& dolly_vector // dolly vector in world coordinates
          );

  //////////
  // Gets a world coordinate dolly vector that can be passed to
  // DollyCamera().
  bool GetDollyCameraVector(
         int screen_x0, 
         int screen_y0,  // (x,y) screen coords of start point
         int screen_x1, 
         int screen_y1,  // (x,y) screen coords of end point
         double proj_plane_dist,      // distance of projection plane from camera.
                      // When in doubt, use 0.5*(frus_near+frus_far).
         ON_3dVector& dolly_vector // world coordinate dolly vector returned here
         ) const;

  //////////
  // Moves frustum's clipping planes
  bool DollyFrustum(
          double dolly_distance // distance to move in camera direction
          );

  /*
  Description:
    Apply scaling factors to parallel projection clipping coordinates
    by setting the m_clip_mod transformation.  
  Parameters:
    x - [in] x > 0
    y - [in] y > 0
  Example:
    If you want to compress the view projection across the viewing
    plane, then set x = 0.5, y = 1.0, and z = 1.0.
  Returns:
    True if successful.
    False if input is invalid or the view is a perspective view.
  */
  bool SetViewScale( double x, double y );
  void GetViewScale( double* x, double* y ) const;
  
  // OBSOLETE - use SetViewScale
  //__declspec(deprecated) bool ScaleView( double x, double y, double z );

  /*
  Description:
    Gets the m_clip_mod transformation;
  Returns:
    value of the m_clip_mod transformation.
  */
  ON_Xform ClipModXform() const;

  /*
  Description:
    Gets the m_clip_mod_inverse transformation;
  Returns:
    value of the m_clip_mod_inverse transformation.
  */
  ON_Xform ClipModInverseXform() const;

  /*
  Returns:
    True if clip mod xform is identity.
  */
  bool ClipModXformIsIdentity() const;

  bool SetTargetPoint( ON_3dPoint target_point );

  ON_3dPoint TargetPoint() const;

  void SetMinNearOverFar(double);
  double MinNearOverFar() const;

  /*
  Description:
    Expert user function to control the minimum
    ratio of near/far when perspective projections
    are begin used.
  Parameters:
    min_near_over_far - [in]
  Remarks:
    This is a runtime setting and is not saved in 3dm files.
  */
  void SetPerspectiveMinNearOverFar(double min_near_over_far);

  /*
  Description:
    Expert user function to get the minimum runtime
    value of near/far when perspective projections
    are begin used.
  Returns:
    The minimum permitted value of near/far when perspective 
    projections are begin used.
  Remarks:
    This is a runtime setting and is not saved in 3dm files.
  */
  double PerspectiveMinNearOverFar() const;

  /*
  Description:
    Expert user function to control the minimum
    value of near when perspective projections
    are begin used.
  Parameters:
    min_near_dist - [in]
  Remarks:
    This is a runtime setting and is not saved in 3dm files.
  */
  void SetPerspectiveMinNearDist(double min_near_dist);

  /*
  Description:
    Expert user function to get the minimum
    value of near when perspective projections
    are begin used.
  Returns:
    The minimum permitted value of near when perspective 
    projections are begin used.
  Remarks:
    This is a runtime setting and is not saved in 3dm files.
  */
  double PerspectiveMinNearDist() const;
  
  /*
  Description:
    Sets the viewport's id to the value used to 
    uniquely identify this viewport.
  Parameters:
    viewport_id - [in]    
  Returns:
    True if the viewport's id was successfully set
    and false otherwise (ie. the viewport uuid has
    already been set).
  Remarks:
    There is no approved way to change the viewport 
    id once it is set in order to maintain consistency
    across multiple viewports and those routines that 
    manage them.
  */
  bool  SetViewportId(const ON_UUID& viewport_id );

  ON_UUID ViewportId(void) const;

  /*
  Description:
    EXPERT USER function to change the viewport's id.
    If you change the id, you risk damaging display
    and visibility relationships in the model.
  Parameters:
    viewport_id - [in]    
  */
  void ChangeViewportId(const ON_UUID& viewport_id);
  
protected:

  // These boolean status flags are set to true when
  // the associated fields contain valid values.
  bool m_bValidCamera;
  bool m_bValidFrustum;
  bool m_bValidPort;

  // Camera Settings: ///////////////////////////////////////////////

  // perspective or parallel projection
  ON::view_projection m_projection;

  //   Camera location, direction and orientation (in world coordinates).
  //   These values are used to set the camera frame vectors CamX, CamY,
  //   CamZ.  If bValidCamera is TRUE, then the CamX, CamY and CamZ
  //   vectors are properly initialized and should be used
  //   instead of CamDir[] and CamUp[].  The frame vectors CamX, CamY, CamZ
  //   are always a right handed orthonormal frame.  The CamDir
  //   and CamUp vectors contain the values passed to SetViewCamera().

  ON_3dPoint m_CamLoc;  // camera location
  ON_3dVector m_CamDir; // from camera towards view (nonzero and not parallel to m_CamUp)
  ON_3dVector m_CamUp;  // (nonzero and not parallel to m_CamDir)

  //double m_target_distance; // if > 0, CamLoc + m_target_distance*CamDir 
  //                          // is the position of the view's "target" point.

  // The camera frame vectors are properly initialized by SetCamera()
  ON_3dVector m_CamX;
  ON_3dVector m_CamY;
  ON_3dVector m_CamZ;

  // View Frustum Settings: ///////////////////////////////////////
  //   left, right are camera X coords on near clipping plane
  //   bottom, top are camera Y coords on near clipping plane
  //   near = distance from camera to near clipping plane
  //   far = distance from camera to far clipping plane
  double m_frus_left,   m_frus_right; // frus_left < frus_right 
  double m_frus_bottom, m_frus_top;   // frus_bottom < frus_top 
  double m_frus_near,   m_frus_far;   // frus_near < frus_far 
                                      // in perspective, 0 < frus_near
  

  // Device View Port Box Settings: ( in display device coordinates ) ////
  //   The point (left,bottom,-near), in camera coordinates, of the view
  //   frustum is mapped to pixel coordinate (port_left,port_bottom,port_near).
  //   The point (right,top,-far), in camera coordinates, of the view frustum 
  //   is mapped to pixel coordinate (port_right,port_top,port_far).
  int m_port_left,   m_port_right; // port_left != port_right
  int m_port_bottom, m_port_top;   // port_bottom != port_top  
                                   // In many situations including Windows,
                                   // port_left = 0,
                                   // port_right = viewport width-1,
                                   // port_top = 0,
                                   // port_bottom = viewport height-1.
  int m_port_near,   m_port_far;   // (If you want an 8 bit z-buffer with 
                                   // z=255 being "in front of" z=0, then
                                   // set port_near = 255 and port_far = 0.)


  // The location of this point has no impact on the 
  // view projection. It is simply a suggestion for a 
  // fixed point when views are rotated or the isometric 
  // depth when perpsective views are dollied.  The default
  // is ON_UNSET_POINT.
  ON_3dPoint m_target_point;

private:
  // When this id matches the viewport id saved in an ON_DisplayMaterialRef
  // list in ON_3dmObjectAttributes, then the the display material is used
  // for that object in this view.
  ON_UUID m_viewport_id;

  bool SetCameraFrame(); // used to set m_CamX, m_CamY, m_CamZ

  // This transform is used to tweak the clipping 
  // coordinates.  The default is the identity.  
  // Modify this transformation when you need to do
  // things like z-buffer bias, non-uniform viewplane
  // scaling, and so on.

  /*
  Description:
    Sets the m_clip_mod transformation;
  Parameters:
    clip_mod_xform - [in] invertable transformation
  */
  bool SetClipModXform( ON_Xform clip_mod_xform );
  ON_Xform m_clip_mods;
  ON_Xform m_clip_mods_inverse;

  // Runtime values that depend on the graphics hardware being used.
  // These values are not saved in 3dm files.
  double m__MIN_NEAR_DIST;
  double m__MIN_NEAR_OVER_FAR;
};

ON_DECL
bool 
ON_GetViewportRotationAngles( 
    const ON_3dVector&, // X, // X,Y,Z must be a right handed orthonormal basis
    const ON_3dVector&, // Y, 
    const ON_3dVector&, // Z,
    double*, // angle1, // returns rotation about world Z
    double*, // angle2, // returns rotation about world X ( 0 <= a2 <= pi )
    double*  // angle3  // returns rotation about world Z
    );

ON_DECL
bool
ON_ViewportFromRhinoView( // create ON_Viewport from legacy Rhino projection info
        ON::view_projection, // projection,
        const ON_3dPoint&, // rhvp_target, // 3d point
        double, // rhvp_angle1 in radians
        double, // rhvp_angle2 in radians
        double, // rhvp_angle3 in radians
        double, // rhvp_viewsize,     // > 0
        double, // rhvp_cameradist,   // > 0
        int, // screen_width, 
        int, // screen_height,
        ON_Viewport&
        );
#endif

