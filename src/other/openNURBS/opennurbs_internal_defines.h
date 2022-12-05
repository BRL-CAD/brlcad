/*
// Copyright (c) 1993-2017 Robert McNeel & Associates. All rights reserved.
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

#if !defined(OPENNURBS_INTERNAL_DEFINES_INC_)
#define OPENNURBS_INTERNAL_DEFINES_INC_

#if defined(ON_COMPILING_OPENNURBS)

class ON_INTERNAL_OBSOLETE
{
public:

  //// OBSOLETE V5 Dimension Types ///////////////////////////////////////////////////////////
  enum class V5_eAnnotationType : unsigned char
  {
    dtNothing,
    dtDimLinear,
    dtDimAligned,
    dtDimAngular,
    dtDimDiameter,
    dtDimRadius,
    dtLeader,
    dtTextBlock,
    dtDimOrdinate,
  };

  // convert integer to eAnnotationType enum
  static ON_INTERNAL_OBSOLETE::V5_eAnnotationType V5AnnotationTypeFromUnsigned(
    unsigned int v5_annotation_type_as_unsigned
  );

  //// dim text locations ///////////////////////////////////////////////////////////
  enum class V5_TextDisplayMode : unsigned char
  {
    kNormal = 0, // antique name - triggers use of current default
    kHorizontalToScreen = 1,         // Horizontal to the screen
    kAboveLine = 2,
    kInLine = 3,
    kHorizontalInCplane = 4   // horizontal in the dimension's plane 
  };

  static ON_INTERNAL_OBSOLETE::V5_TextDisplayMode V5TextDisplayModeFromUnsigned(
    unsigned int text_display_mode_as_unsigned
  );

  static ON_INTERNAL_OBSOLETE::V5_TextDisplayMode V5TextDisplayModeFromV6DimStyle(
    const ON_DimStyle& V6_dim_style
  );
  
  /// <summary>
  /// Attachment of content
  /// </summary>
  enum class V5_vertical_alignment : unsigned char
  {
    /// <summary>
    /// Text centered on dimension line (does not apply to leaders or text)
    /// </summary>
    Centered = 0,
    /// <summary>
    /// Text above dimension line (does not apply to leaders or text)
    /// </summary>
    Above = 1,
    /// <summary>
    /// Text below dimension line (does not apply to leaders or text)
    /// </summary>
    Below = 2,
    /// <summary>
    /// Leader tail at top of text  (does not apply to text or dimensions)
    /// </summary>
    Top = 3, // = TextVerticalAlignment::Top
    /// <summary>
    /// Leader tail at middle of first text line (does not apply to text or dimensions)
    /// </summary>
    FirstLine = 4, // = MiddleOfTop
    /// <summary>
    /// Leader tail at middle of text or content (does not apply to text or dimensions)
    /// </summary>
    Middle = 5, // = Middle
    /// <summary>
    /// Leader tail at middle of last text line (does not apply to text or dimensions)
    /// </summary>
    LastLine = 6, // = MiddleOfBottom
    /// <summary>
    /// Leader tail at bottom of text (does not apply to text or dimensions)
    /// </summary>
    Bottom = 7, // = Bottom
    /// <summary>
    /// Leader tail at bottom of text, text underlined (does not apply to text or dimensions)
    /// </summary>
    Underlined = 8 // Underlined

    // nothing matched BottomOfTop
  };

  static ON_INTERNAL_OBSOLETE::V5_vertical_alignment V5VerticalAlignmentFromUnsigned(
    unsigned int vertical_alignment_as_unsigned
  );

  static ON_INTERNAL_OBSOLETE::V5_vertical_alignment V5VerticalAlignmentFromV5Justification(
    unsigned int v5_justification_bits
  );

  static ON_INTERNAL_OBSOLETE::V5_vertical_alignment V5VerticalAlignmentFromV6VerticalAlignment(
    const ON::TextVerticalAlignment text_vertical_alignment
  );

  static ON::TextVerticalAlignment V6VerticalAlignmentFromV5VerticalAlignment(
    ON_INTERNAL_OBSOLETE::V5_vertical_alignment V5_vertical_alignment
  );


  enum class V5_horizontal_alignment : unsigned char
  {
    /// <summary>
    /// Left aligned
    /// </summary>
    Left = 0, // Left
    /// <summary>
    /// Centered
    /// </summary>
    Center = 1,
    /// <summary>
    /// Right aligned
    /// </summary>
    Right = 2,
    /// <summary>
    /// Determined by orientation
    /// Primarily for leaders to make
    /// text right align when tail is to the left 
    /// and left align when tail is to the right
    /// </summary>
    Auto = 3,
  };

  static ON_INTERNAL_OBSOLETE::V5_horizontal_alignment V5HorizontalAlignmentFromUnsigned(
    unsigned int horizontal_alignment_as_unsigned
  );

  static ON_INTERNAL_OBSOLETE::V5_horizontal_alignment V5HorizontalAlignmentFromV5Justification(
    unsigned int v5_justification_bits
  );

  static ON_INTERNAL_OBSOLETE::V5_horizontal_alignment V5HorizontalAlignmentFromV6HorizontalAlignment(
    const ON::TextHorizontalAlignment text_horizontal_alignment
  );

  static ON::TextHorizontalAlignment V6HorizontalAlignmentFromV5HorizontalAlignment(
    ON_INTERNAL_OBSOLETE::V5_horizontal_alignment V5_vertical_alignment
  );
};

#endif

#endif
