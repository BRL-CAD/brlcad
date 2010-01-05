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

#if !defined(ON_ARC_INC_)
#define ON_ARC_INC_

/*
Description:
	An ON_Arc is a subcurve of 3d circle. 
Details:
	The curve is parameterized by	an angle expressed in radians.   For an IsValid() arc 
	the total subtended angle 	AngleRadians() = Domain()(1) - Domain()(0) must satisfy
				0< AngleRadians() <2*Pi .
	
	The parameterization of the ON_Arc is inherited from the ON_Circle it is derived from.
	In particular
			 t -> center + cos(t)*radius*xaxis + sin(t)*radius*yaxis	
	where xaxis and yaxis, (part of ON_Circle::m_plane) form an othonormal frame of the plane 
	containing the circle.
*/
class ON_CLASS ON_Arc : public ON_Circle
{
public:
  // Create a radius one arc with angle = 2*pi
  ON_Arc();

  ON_Arc(
    const ON_Circle&,
    double           // angle in radians
    );

  /*
  Parameters:
    circle - [in]
    angle_interval_in_radians - [in] increasing angle interval
       in radians with angle_interval_in_radians.Length() <= 2.0*ON_PI.
  */
  ON_Arc(
    const ON_Circle& circle,
    ON_Interval angle_interval_in_radians
    );

  ON_Arc(
    const ON_Plane&, // circle is in this plane with center at m_origin
    double,          // radius
    double           // angle in radians
    );

  ON_Arc( // arc is parallel to XY plane
    const ON_3dPoint&, // center
    double,            // radius
    double             // angle in radians
    );

  ON_Arc( // arc parallel to a plane
    const ON_Plane&,   // circle will be parallel to this plane
    const ON_3dPoint&, // center
    double,            // radius
    double             // angle in radians
    );

  ON_Arc( // arc through 3 2d points
    const ON_2dPoint&, // point P
    const ON_2dPoint&, // point Q
    const ON_2dPoint&  // point R
    );

  ON_Arc( // arc through 3 3d points
    const ON_3dPoint&, // point P
    const ON_3dPoint&, // point Q
    const ON_3dPoint&  // point R
    );

  //////////
  // Create an arc from a circle and an angle in radians
  bool Create(
    const ON_Circle&, // [IN]
    double            // [IN] angle in radians
    );

  /*
  Description:
    Create an arc from a circle and an increasing angle interval
  Parameters:
    circle - [in]
    angle_interval_in_radians - [in] increasing angle interval in radians
              with angle_interval_in_radians.Length() <= 2.0*ON_PI
  Returns:
    true if input is valid and a valid arc is created.
  */
  bool Create(
    const ON_Circle& circle,
    ON_Interval angle_interval_in_radians
    );

  //////////
  // Create an arc from a plane, radius and an angle in radians.
  // The center of the arc is at the plane's origin.
  bool Create(
    const ON_Plane&, // [IN] circle is in this plane with center at m_origin
    double,          // [IN] radius
    double           // [IN] angle in radians
    );

  //////////
  // Create an arc parallel to the world XY plane from a center point,
  // radius, and angle in radians.  The arc starts at center+(radius,0,0).
  bool Create(
    const ON_3dPoint&, // [IN] center
    double,            // [IN] radius
    double             // [IN] angle in radians
    );

  //////////
  // Create an arc parallel to plane from a center point, radius, 
  // and angle in radians.  The arc starts at center+radius*plane.xaxis.
  bool Create(
    const ON_Plane&,   // [IN] circle will be parallel to this plane
    const ON_3dPoint&, // [IN] center
    double,            // [IN] radius
    double             // [IN] angle in radians
    );

  //////////
  // Create an arc that passes through 3 2d points.
  bool Create( // arc through 3 2d points
    const ON_2dPoint&, // [IN] point P
    const ON_2dPoint&, // [IN] point Q
    const ON_2dPoint&  // [IN] point R
    );

  //////////
  // Create an arc that passes through 3 3d points.
  bool Create( // arc through 3 3d points
    const ON_3dPoint&, // [IN] point P
    const ON_3dPoint&, // [IN] point Q
    const ON_3dPoint&  // [IN] point R
    );

  //////////
  // Create an arc from a 2d start point, 2d start direction, and 2d end point.
  bool Create(
    const ON_2dPoint&,  // [IN] point P
    const ON_2dVector&, // [IN] arc direction at P
    const ON_2dPoint&   // [IN] point R
    );

  //////////
  // Create an arc from a 3d start point, 3d start direction, and 3d end point.
  bool Create(
    const ON_3dPoint&,  // [IN] point P
    const ON_3dVector&, // [IN] arc direction at P
    const ON_3dPoint&   // [IN] point R
    );

  ON_Arc& operator=( const ON_Circle& );


  ~ON_Arc();

  // Description:
  //   Creates a text dump of the arc listing the normal, center
  //   radius, start point, end point, and angle.
  // Remarks:
  //   Dump() is intended for debugging and is not suitable
  //   for creating high quality text descriptions of an
  //   arc.
  void Dump( ON_TextLog& dump ) const;

  // Description:
  //   Checks an arc to make sure it is valid.
	// Detail:
	//	 Radius>0 and 0<AngleRadians()<=2 ON_PI
  // Returns:
  //   true if the arc is valid.
  bool IsValid() const;

  // Description: 
  //   Get arc's 3d axis aligned bounding box.
  // Returns:
  //   3d bounding box.
  ON_BoundingBox BoundingBox() const;

  // Description:
  //   Get arc's 3d axis aligned bounding box or the
  //   union of the input box with the arc's bounding box.
  // Parameters:
  //   bbox - [in/out] 3d axis aligned bounding box
  //   bGrowBox - [in] (default=false) 
  //     If true, then the union of the input bbox and the 
  //     arc's bounding box is returned in bbox.  
  //     If false, the arc's bounding box is returned in bbox.
  // Returns:
  //   true if arc has bounding box and calculation was successful.
  bool GetBoundingBox(
         ON_BoundingBox& bbox,
         int bGrowBox = false
         ) const;

  /*
	Description:
    Get tight bounding box.
	Parameters:
		tight_bbox - [in/out] tight bounding box
		bGrowBox -[in]	(default=false)			
      If true and the input tight_bbox is valid, then returned
      tight_bbox is the union of the input tight_bbox and the 
      arc's tight bounding box.
		xform -[in] (default=NULL)
      If not NULL, the tight bounding box of the transformed
      arc is calculated.  The arc is not modified.
	Returns:
    True if a valid tight_bbox is returned.
  */
	bool GetTightBoundingBox( 
			ON_BoundingBox& tight_bbox, 
      int bGrowBox = false,
			const ON_Xform* xform = 0
      ) const;

  // Returns:
  //   true if the arc is a complete circle; i.e., the arc's
  //   angle is 360 degrees.
  bool IsCircle() const;

  // Returns:
  //   The arc's subtended angle in radians.
  double AngleRadians() const;

  // Returns:
  //   The arc's subtended angle in degrees.
  double AngleDegrees() const;


  /*
  Description:
    Get evaluation domain.
  Returns:
    Evaluation domain (same as DomainRadians()).
  */
  ON_Interval Domain() const;

  // Returns:
  //   The arc's domain in radians.
  ON_Interval DomainRadians() const;

  // Returns:
  //   The arc's domain in degrees.
  ON_Interval DomainDegrees() const;

  // Description:
  //   Set arc's subtended angle in radians.
  // Parameters:
  //   angle_in_radians - [in] 0 <= angle_in_radians <= 2.0*ON_PI
  //
  bool SetAngleRadians(
    double angle_in_radians
    );

  /*
  Description:
    Set arc's angle interval in radians.
  Parameters:
    angle_in_radians - [in] increasing interval with 
                            start and end angle in radians.
                            Length of the interval <= 2.0*ON_PI.
  Returns:
    true if successful. 
  */
  bool SetAngleIntervalRadians(
    ON_Interval angle_in_radians
    );

  // Description:
  //   Set arc's domain as a subdomain of the circle.
  // Parameters:
  //   domain_radian - [in]   0 < domain_radian[1] - domain_radian[0] <= 2.0 * ON*PI
  //
  bool Trim(
    ON_Interval domain_radian
    );

  // Description:
  //   Set arc's subtended angle in degrees.
  // Parameters:
  //   angle_in_degrees - [in] 0 < angle_in_degrees <= 360
  bool SetAngleDegrees(
    double angle_in_degrees
    );

  // Returns:
  //   Point at start of the arc.
  ON_3dPoint StartPoint() const;


  // Returns:
  //   Point at middle of the arc.
  ON_3dPoint MidPoint() const;

  // Returns:
  //   Point at end of the arc.
  ON_3dPoint EndPoint() const;

  // Description:
  //   Get the point on the arc that is closest to test_point.
  // Parameters:
  //   test_point - [in]
  //   t - [out] parameter (in radians) of the point on the arc that
  //       is closest to test_point.  If test_point is the center
  //       of the arc, then the starting point of the arc is
  //       (arc.Domain()[0]) returned.
  bool ClosestPointTo( 
         const ON_3dPoint& test_point, 
         double* t
         ) const;

  // Description:
  //   Get the point on the arc that is closest to test_point.
  // Parameters:
  //   test_point - [in]
  // Returns:
  //   The point on the arc that is closest to test_point.
  //   If test_point is the center of the arc, then the 
  //   starting point of the arc is returned.
  ON_3dPoint ClosestPointTo( 
         const ON_3dPoint& test_point
         ) const;

  // Returns:
  //   Length of the arc = radius*(subtended angle in radians).
  double Length() const;

  // Description:
  //   Reverse the orientation of the arc.  Changes the domain
  //   from [a,b] to [-b.-a].
  bool Reverse();

  // Description:
  //   Get a rational degree 2 NURBS curve representation
  //   of the arc.  Note that the parameterization of NURBS curve
  //   does not match  arc's transcendental paramaterization.  
  //   Use GetRadianFromNurbFormParameter() and
  //   GetParameterFromRadian() to convert between the NURBS curve 
  //   parameter and the transcendental parameter
  // Parameters:
  //   nurbs_curve - [out] nurbs_curve returned here.
  // Returns:
  //   0 for failure and 2 for success.
  int GetNurbForm(
        ON_NurbsCurve& nurbs_curve
        ) const; 

  /*
  Description:
    Convert a NURBS curve arc parameter to a arc radians parameter.
  Parameters:
    nurbs_parameter - [in]
    arc_radians_parameter - [out]
  Example:

          ON_Arc arc = ...;
          double nurbs_t = 1.2345; // some number in interval (0,2.0*ON_PI).
          double arc_t;
          arc.GetRadianFromNurbFormParameter( nurbs_t, &arc_t );

          ON_NurbsCurve nurbs_curve;
          arc.GetNurbsForm( nurbs_curve );
          arc_pt = arc.PointAt(arc_t);
          nurbs_pt = nurbs_curve.PointAt(nurbs_t);
          // arc_pt and nurbs_pt will be the same

  Remarks:
    The NURBS curve parameter is with respect to the NURBS curve
    created by ON_Arc::GetNurbForm.  At nurbs parameter values of 
    0.0, 0.5*ON_PI, ON_PI, 1.5*ON_PI, and 2.0*ON_PI, the nurbs
    parameter and radian parameter are the same.  At all other
    values the nurbs and radian parameter values are different.
  See Also:
    ON_Arc::GetNurbFormParameterFromRadian
  */
  bool GetRadianFromNurbFormParameter(
        double nurbs_parameter,
        double* arc_radians_parameter
        ) const;

  /*
  Description:
    Convert a arc radians parameter to a NURBS curve arc parameter.
  Parameters:
    arc_radians_parameter - [in] 0.0 to 2.0*ON_PI
    nurbs_parameter - [out]
  Example:

          ON_Arc arc = ...;
          double arc_t = 1.2345; // some number in interval (0,2.0*ON_PI).
          double nurbs_t;
          arc.GetNurbFormParameterFromRadian( arc_t, &nurbs_t );

          ON_NurbsCurve nurbs_curve;
          arc.GetNurbsForm( nurbs_curve );
          arc_pt = arc.PointAt(arc_t);
          nurbs_pt = nurbs_curve.PointAt(nurbs_t);
          // arc_pt and nurbs_pt will be the same

  Remarks:
    The NURBS curve parameter is with respect to the NURBS curve
    created by ON_Arc::GetNurbForm.  At radian values of 
    0.0, 0.5*ON_PI, ON_PI, 1.5*ON_PI, and 2.0*ON_PI, the nurbs
    parameter and radian parameter are the same.  At all other
    values the nurbs and radian parameter values are different.
  See Also:
    ON_Arc::GetNurbFormParameterFromRadian
  */
  bool GetNurbFormParameterFromRadian(
        double arc_radians_parameter,
        double* nurbs_parameter
        ) const;

private:
  friend bool ON_BinaryArchive::ReadArc( ON_Arc& );
  friend bool ON_BinaryArchive::WriteArc( const ON_Arc& );

  // increasing interval with start and end angle in radians
  ON_Interval m_angle;
};

#endif

