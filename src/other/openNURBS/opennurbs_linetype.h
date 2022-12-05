/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2012 Robert McNeel & Associates. All rights reserved.
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

#if !defined(OPENNURBS_LINETYPE_INC_)
#define OPENNURBS_LINETYPE_INC_


// Description:
//   Determine if a line width is deemed to be a "hairline width" in Rhino
//   Any width that is >0 and < 0.001 mm is a hairline width for printing
// Parameters:
//   width_mm: [in] the width to examine in millimeters
// Returns:
//   true if this is a hairline width
ON_DECL bool ON_IsHairlinePrintWidth( double width_mm );

// Description:
//   Return a width in millimeters that is a valid hairline width in rhino
ON_DECL double ON_HairlinePrintWidth();




//////////////////////////////////////////////////////////////////////
// class ON_Linetype

class ON_CLASS ON_Linetype : public ON_ModelComponent
{
  ON_OBJECT_DECLARE(ON_Linetype);

public:
  // no attributes are set.
  static const ON_Linetype Unset;

  // index = -1, id, name and pattern are set.
  static const ON_Linetype Continuous;

  // index = -2, id, name and pattern are set.
  static const ON_Linetype ByLayer;

  // index = -3, id, name and pattern are set.
  static const ON_Linetype ByParent;

  // index = -4, id, name and pattern are set.
  static const ON_Linetype Hidden;

  // index = -5, id, name and pattern are set.
  static const ON_Linetype Dashed;

  // index = -6, id, name and pattern are set.
  static const ON_Linetype DashDot;

  // index = -7, id, name and pattern are set.
  static const ON_Linetype Center;

  // index = -8, id, name and pattern are set.
  static const ON_Linetype Border;

  // index = -9, id, name and pattern are set.
  static const ON_Linetype Dots;

  /*
  Parameters:
    model_component_reference - [in]
    none_return_value - [in]
      value to return if ON_Linetype::Cast(model_component_ref.ModelComponent())
      is nullptr
  Returns:
    If ON_Linetype::Cast(model_component_ref.ModelComponent()) is not nullptr,
    that pointer is returned.  Otherwise, none_return_value is returned. 
  */
  static const ON_Linetype* FromModelComponentRef(
    const class ON_ModelComponentReference& model_component_reference,
    const ON_Linetype* none_return_value
    );

public:

  ON_Linetype() ON_NOEXCEPT;
  ~ON_Linetype() = default;
  ON_Linetype(const ON_Linetype&);
  ON_Linetype& operator=(const ON_Linetype&) = default;

  /*
    Description:
      Tests that name is set and there is at least one non-zero length segment
  */
  bool IsValid( class ON_TextLog* text_log = nullptr ) const override;

  void Dump( ON_TextLog& ) const override; // for debugging

  /*
    Description:
      Write to file
  */
  bool Write(
         ON_BinaryArchive&  // serialize definition to binary archive
       ) const override;

  /*
    Description:
      Read from file
  */
  bool Read(
         ON_BinaryArchive&  // restore definition from binary archive
       ) override;


  //////////////////////////////////////////////////////////////////////
  //
  // Interface

  bool PatternIsSet() const;
  bool ClearPattern();
  bool PatternIsLocked() const;
  void LockPattern();

  /*
    Description:
      Returns the total length of one repeat of the pattern
  */
  double PatternLength() const;


  /*
    Description:
      Returns the number of segments in the pattern
  */
  int SegmentCount() const;

  /*
  Description:
    Adds a segment to the pattern
  Returns:
    Index of the added segment.
  */
  int AppendSegment( const ON_LinetypeSegment& segment);

  /*
  Description:
    Removes a segment in the linetype.
  Parameters:
    index - [in]
      Zero based index of the segment to remove.
  Returns:
    True if the segment index was removed.
  */
  bool RemoveSegment( int index );

  /*
    Description:
      Sets the segment at index to match segment
  */
  bool SetSegment( int index, const ON_LinetypeSegment& segment);

  /*
    Description:
      Sets the length and type of the segment at index
  */
  bool SetSegment( int index, double length, ON_LinetypeSegment::eSegType type);

  /*
  Description:
    Set all segments
  Parameters:
    segments - [in]
  */
  bool SetSegments(const ON_SimpleArray<ON_LinetypeSegment>& segments);


  /*
    Description:
      Returns a copy of the segment at index
  */
  ON_LinetypeSegment Segment( int index) const;

  /*
    Description:
      Expert user function to get access to the segment array
      for rapid calculations.
  */
  // Returns nullptr if the line pattern is locked.
  ON_SimpleArray<ON_LinetypeSegment>* ExpertSegments();

  const ON_SimpleArray<ON_LinetypeSegment>& Segments() const;

private:
  enum : unsigned char
  {
    pattern_bit = 1
  };
  unsigned char m_is_set_bits = 0;
  unsigned char m_is_locked_bits = 0;
  unsigned short m_reserved1 = 0;
  unsigned int m_reserved2 = 0;
  ON_SimpleArray<ON_LinetypeSegment> m_segments;
};

#if defined(ON_DLL_TEMPLATE)
ON_DLL_TEMPLATE template class ON_CLASS ON_SimpleArray<ON_Linetype*>;
ON_DLL_TEMPLATE template class ON_CLASS ON_SimpleArray<const ON_Linetype*>;
ON_DLL_TEMPLATE template class ON_CLASS ON_ObjectArray<ON_Linetype>;
#endif

#endif

