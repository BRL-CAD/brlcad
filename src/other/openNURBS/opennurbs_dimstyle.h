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

#if !defined(OPENNURBS_DIMSTYLE_INC_)
#define OPENNURBS_DIMSTYLE_INC_

class ON_CLASS ON_DimStyle : public ON_Object
{
  ON_OBJECT_DECLARE(ON_DimStyle);

public:
  enum eArrowType
  {
    solidtriangle = 0,
    dot = 1,
    tick = 2,
    shorttriangle = 3,
    arrow = 4,
    rectangle = 5,
  };

  ON_DimStyle();
  ~ON_DimStyle();
  // C++ default copy construction and operator= work fine.

  ON_DimStyle& operator=( const ON_3dmAnnotationSettings& src);

  //////////////////////////////////////////////////////////////////////
  //
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

  // virtual
  void Dump( ON_TextLog& ) const; // for debugging

  // virtual
  BOOL Write(
         ON_BinaryArchive&  // serialize definition to binary archive
       ) const;

  // virtual
  BOOL Read(
         ON_BinaryArchive&  // restore definition from binary archive
       );

  void EmergencyDestroy();

  // virtual
  ON_UUID ModelObjectId() const;


  //////////////////////////////////////////////////////////////////////
  //
  // Interface

  void SetName( const wchar_t* );
  void SetName( const char* );

  void GetName( ON_wString& ) const;
  const wchar_t* Name() const;

  void SetIndex(int);
  int Index() const;

  void SetDefaults();

  double ExtExtension() const;
  void SetExtExtension( const double);

  double ExtOffset() const;
  void SetExtOffset( const double);

  double ArrowSize() const;
  void SetArrowSize( const double);

  double LeaderArrowSize() const;
  void SetLeaderArrowSize( const double);

  double CenterMark() const;
  void SetCenterMark( const double);

  int TextAlignment() const;
  void SetTextAlignment( ON::eTextDisplayMode);

  int ArrowType() const;
  void SetArrowType( eArrowType);

  int LeaderArrowType() const;
  void SetLeaderArrowType( eArrowType);

  int AngularUnits() const;
  void SetAngularUnits( int);

  int LengthFormat() const;
  void SetLengthFormat( int);

  int AngleFormat() const;
  void SetAngleFormat( int);

  int LengthResolution() const;
  void SetLengthResolution( int);

  int AngleResolution() const;
  void SetAngleResolution( int);

  int FontIndex() const;
  virtual void SetFontIndex( int index);

  double TextGap() const;
  void SetTextGap( double gap);

  double TextHeight() const;
  void SetTextHeight( double height);

  // added at ver 1.3
  double LengthFactor() const;
  void SetLengthactor( double);

  bool Alternate() const;
  void SetAlternate( bool);

  double AlternateLengthFactor() const;
  void SetAlternateLengthactor( double);

  int AlternateLengthFormat() const;
  void SetAlternateLengthFormat( int);

  int AlternateLengthResolution() const;
  void SetAlternateLengthResolution( int);

  int AlternateAngleFormat() const;
  void SetAlternateAngleFormat( int);

  int AlternateAngleResolution() const;
  void SetAlternateAngleResolution( int);

  void GetPrefix( ON_wString& ) const;
  const wchar_t* Prefix() const;
  void SetPrefix( wchar_t*);

  void GetSuffix( ON_wString& ) const;
  const wchar_t* Suffix() const;
  void SetSuffix( wchar_t*);

  void GetAlternatePrefix( ON_wString& ) const;
  const wchar_t* AlternatePrefix() const;
  void SetAlternatePrefix( wchar_t*);

  void GetAlternateSuffix( ON_wString& ) const;
  const wchar_t* AlternateSuffix() const;
  void SetAlternateSuffix( wchar_t*);

  bool SuppressExtension1() const;
  void SetSuppressExtension1( bool);

  bool SuppressExtension2() const;
  void SetSuppressExtension2( bool);

  // replace the values in this with any valid fields in override
  void Composite( const ON_DimStyle& override);

  enum eField  // all fields
  {
    fn_name                       = 0,
    fn_index                      = 1,
    fn_extextension               = 2,
    fn_extoffset                  = 3,
    fn_arrowsize                  = 4,
    fn_centermark                 = 5,
    fn_textgap                    = 6,
    fn_textheight                 = 7,
    fn_textalign                  = 8,
    fn_arrowtype                  = 9,
    fn_angularunits               = 10,
    fn_lengthformat               = 11,
    fn_angleformat                = 12,
    fn_angleresolution            = 13,
    fn_lengthresolution           = 14,
    fn_fontindex                  = 15,
    fn_lengthfactor               = 16,
    fn_bAlternate                 = 17,
    fn_alternate_lengthfactor     = 18,
    fn_alternate_lengthformat     = 19, 
    fn_alternate_lengthresolution = 20,
    fn_alternate_angleformat      = 21, 
    fn_alternate_angleresolution  = 22,
    fn_prefix                     = 23,
    fn_suffix                     = 24,
    fn_alternate_prefix           = 25,
    fn_alternate_suffix           = 26,
    fn_dimextension               = 27,
    fn_leaderarrowsize            = 28,
    fn_leaderarrowtype            = 29,
    fn_suppressextension1         = 30,
    fn_suppressextension2         = 31,
    fn_last                       = 32,
  };


  // mark a single field as invalid
  // Setting a field value marks that field as valid
  void InvalidateField( eField field);

  void InvalidateAllFields();

  void ValidateField( eField field);

  bool IsFieldValid( eField) const;

  // added version 1.3
  double DimExtension() const;
  void SetDimExtension( const double);


public:
  ON_wString m_dimstyle_name;   // String name of the style
  int m_dimstyle_index;         // Index in the dimstyle table
  ON_UUID m_dimstyle_id;

  double m_extextension; // extension line extension
  double m_extoffset;    // extension line offset
  double m_arrowsize;  // length of an arrow - may mean different things to different arrows
  double m_centermark; // size of the + at circle centers
  double m_textgap;    // gap around the text for clipping dim line
  double m_textheight; // model unit height of dimension text before applying dimscale
  int m_textalign;     // text alignment relative to the dimension line
  int m_arrowtype;     // 0: filled narrow triangular arrow
  int m_angularunits;  // 0: degrees, 1: radians
  int m_lengthformat;  // 0: decimal, 1: feet, 2: feet & inches
  int m_angleformat;   // 0: decimal degrees, ...
  int m_angleresolution;    // for decimal degrees, digits past decimal
  int m_lengthresolution;   // depends on m_lengthformat
                            // for decimal, digits past the decimal point
  int m_fontindex;     // index of the ON_Font used by this dimstyle

  // added fields version 1.2, Jan 13, 05
  double m_lengthfactor;  // (dimlfac) model units multiplier for length display
  bool m_bAlternate;      // (dimalt) display alternate dimension string (or not)
                          // using m_alternate_xxx values
  double m_alternate_lengthfactor;  // (dimaltf) model units multiplier for alternate length display
  int m_alternate_lengthformat;     // 0: decimal, 1: feet, 2: feet & inches
  int m_alternate_lengthresolution; // depends on m_lengthformat
                                    // for decimal, digits past the decimal point
  int m_alternate_angleformat;      // 0: decimal degrees, ...
  int m_alternate_angleresolution;  // for decimal degrees, digits past decimal
  ON_wString m_prefix;              // string preceding dimension value string
  ON_wString m_suffix;              // string following dimension value string
  ON_wString m_alternate_prefix;    // string preceding alternate value string
  ON_wString m_alternate_suffix;    // string following alternate value string

  unsigned int m_valid;        // flags of what fields are being used

  // field added version 1.4, Dec 28, 05
  double m_dimextension;  // (dimdle) dimension line extension past the "tip" location

  // fields added version 1.5 Mar 23 06
  double m_leaderarrowsize;
  int    m_leaderarrowtype;
  bool   m_bSuppressExtension1;
  bool   m_bSuppressExtension2;
};

#endif

