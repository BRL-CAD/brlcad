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

#if !defined(OPENNURBS_POINT_GRID_INC_)
#define OPENNURBS_POINT_GRID_INC_

class ON_CLASS ON_PointGrid : public ON_Geometry
{
public:
  ON_PointGrid();
  ON_PointGrid(const ON_PointGrid&);
  ON_PointGrid(
          int,  // point count0 (>=1)
          int   // point count1 (>=1)
          );

  void Initialize(void);  // zeros all fields

  BOOL Create( 
          int,  // point count0 (>=1)
          int   // point count1 (>=1)
          );

  void Destroy();

  virtual ~ON_PointGrid();
  void EmergencyDestroy(); // call if memory used by point grid becomes invalid

	ON_PointGrid& operator=(const ON_PointGrid&);

  // point_grid[i][j] returns GetPoint(i,j)
  ON_3dPoint* operator[](int);             // 0 <= index < PointCount(0)
  const ON_3dPoint* operator[](int) const; // 0 <= index < PointCount(0)
  
  /////////////////////////////////////////////////////////////////
  // ON_Object overrides

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
         ON_BinaryArchive&  // open binary file
       ) const;

  BOOL Read(
         ON_BinaryArchive&  // open binary file
       );

  ON::object_type ObjectType() const;

  /////////////////////////////////////////////////////////////////
  // ON_Geometry overrides

  int Dimension() const;

  BOOL GetBBox( // returns TRUE if successful
         double*,    // minimum
         double*,    // maximum
         BOOL = FALSE  // TRUE means grow box
         ) const;

  /*
	Description:
    Get tight bounding box of the point grid.
	Parameters:
		tight_bbox - [in/out] tight bounding box
		bGrowBox -[in]	(default=false)			
      If true and the input tight_bbox is valid, then returned
      tight_bbox is the union of the input tight_bbox and the 
      tight bounding box of the point grid.
		xform -[in] (default=NULL)
      If not NULL, the tight bounding box of the transformed
      point grid is calculated.  The point grid is not modified.
	Returns:
    True if the returned tight_bbox is set to a valid 
    bounding box.
  */
	bool GetTightBoundingBox( 
			ON_BoundingBox& tight_bbox, 
      int bGrowBox = false,
			const ON_Xform* xform = 0
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

  /////////////////////////////////////////////////////////////////
  // Interface

  BOOL IsClosed( 
        int // dir
        ) const;

  int PointCount(   // number of points in grid direction
        int         // dir 0 = "s", 1 = "t"
        ) const;

  int PointCount(   // total number of points in grid
        void
        ) const;

  ON_3dPoint& Point(
        int, int // point index ( 0 <= i <= PointCount(0), 0 <= j <= PointCount(1)
        );

  ON_3dPoint Point(
        int, int // point index ( 0 <= i <= PointCount(0), 0 <= j <= PointCount(1)
        ) const;

  double* PointArray();

  const double* PointArray() const;

  int PointArrayStride(  // point stride in grid direction
        int         // dir 0 = "s", 1 = "t"
        ) const;

  BOOL SetPoint(      // set a single point
        int, int, // point index ( 0 <= i <= PointCount(0), 0 <= j <= PointCount(1)
        const ON_3dPoint& // value of point
        );

  BOOL GetPoint(              // get a single control vertex
        int, int,   // CV index ( 0 <= i <= CVCount(0), 0 <= j <= CVCount(1)
        ON_3dPoint&      // gets euclidean cv when NURBS is rational
        ) const;

  BOOL Reverse(  // reverse grid order
    int // dir  0 = "s", 1 = "t"
    );

  BOOL Transpose(); // transpose grid points

  /////////////////////////////////////////////////////////////////
  // Implementation
protected:

  int m_point_count[2];   // number of points (>=1)
  int m_point_stride0;    // >= m_point_count[1]
  ON_3dPointArray m_point;
  // point[i][j] = m_point[i*m_point_stride0+j]

private:
  static ON_3dPoint m_no_point; // prevent crashes when sizes are 0

  ON_OBJECT_DECLARE(ON_PointGrid);
};


#endif
