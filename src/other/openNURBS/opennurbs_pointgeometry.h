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

#if !defined(OPENNURBS_POINT_GEOMETRY_INC_)
#define OPENNURBS_POINT_GEOMETRY_INC_

// NOTE:  ON_3dPoint is much more efficient than ON_Point.
//        Use ON_Point when you need a polymorphic 3d point
//        that is derived from ON_Geometry or ON_Object.

class ON_CLASS ON_Point : public ON_Geometry
{
public:
  ON_3dPoint point;

  ON_Point();
  ON_Point(const ON_Point&);
  ON_Point(const ON_3dPoint&);
  ON_Point(double,double,double);
  ~ON_Point();
  ON_Point& operator=(const ON_Point&);
  ON_Point& operator=(const ON_3dPoint&);
  
  operator double*();
  operator const double*() const;
  operator ON_3dPoint*();
  operator const ON_3dPoint*() const;
  operator ON_3dPoint&();
  operator const ON_3dPoint&() const;

  /////////////////////////////////////////////////////////////////
  //
  // ON_Object overrides
  //

  /*
  Description:
    Tests an object to see if its data members are correctly
    initialized.
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
    TRUE     object is valid
    FALSE    object is invalid, uninitialized, etc.
  Remarks:
    Overrides virtual ON_Object::IsValid
  */
  BOOL IsValid( ON_TextLog* text_log = NULL ) const;

  void Dump( ON_TextLog& ) const; // for debugging

  BOOL Write(
         ON_BinaryArchive&  // serialize definition to binary archive
       ) const;

  BOOL Read(
         ON_BinaryArchive&  // restore definition from binary archive
       );

  ON::object_type ObjectType() const;

  /////////////////////////////////////////////////////////////////
  //
  // ON_Geometry overrides
  //

  int Dimension() const;

  BOOL GetBBox( // returns TRUE if successful
         double*,    // boxmin[dim]
         double*,    // boxmax[dim]
         BOOL = FALSE  // TRUE means grow box
         ) const;

  BOOL Transform( 
         const ON_Xform&
         );

  // virtual ON_Geometry::IsDeformable() override
  bool IsDeformable() const;

  // virtual ON_Geometry::MakeDeformable() override
  bool MakeDeformable();

  BOOL SwapCoordinates(
        int, int        // indices of coords to swap
        );

  // virtual ON_Geometry override
  bool Morph( const ON_SpaceMorph& morph );

  // virtual ON_Geometry override
  bool IsMorphable() const;

private:
  ON_OBJECT_DECLARE(ON_Point);
};

#endif
