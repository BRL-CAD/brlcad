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

#if !defined(OPENNURBS_COLOR_INC_)
#define OPENNURBS_COLOR_INC_

///////////////////////////////////////////////////////////////////////////////
//
// Class ON_Color
// 
class ON_CLASS ON_Color
{
public:
  ON_Color() = default;
  ~ON_Color() = default;
  ON_Color(const ON_Color&) = default;
  ON_Color& operator=(const ON_Color&) = default;

  static const ON_Color UnsetColor;       // 0xFFFFFFFFu
  static const ON_Color Black;            // 0x00000000u
  static const ON_Color White;            // 0x00FFFFFFu on little endan, 0xFFFFFF00u on big endian
  static const ON_Color SaturatedRed;     // 0x000000FFu on little endan, 0xFF000000u on big endian
  static const ON_Color SaturatedGreen;   // 0x0000FF00u on little endan, 0x00FF0000u on big endian
  static const ON_Color SaturatedBlue;    // 0x00FF0000u on little endan, 0x0000FF00u on big endian
  static const ON_Color SaturatedYellow;  // 0x0000FFFFu on little endan, 0xFFFF0000u on big endian
  static const ON_Color SaturatedCyan;    // 0x00FFFF00u on little endan, 0x00FFFF00u on big endian
  static const ON_Color SaturatedMagenta; // 0x00FF00FFu on little endan, 0xFF00FF00u on big endian
  static const ON_Color SaturatedGold;    // 0x0000BFFFu on little endan, 0xFFBF0000u on big endian
  static const ON_Color Gray105;          // R = G = B = 105 (medium dark)
  static const ON_Color Gray126;          // R = G = B = 128 (medium)
  static const ON_Color Gray160;          // R = G = B = 160 (medium light)
  static const ON_Color Gray230;          // R = G = B = 230 (light)
  static const ON_Color Gray250;          // R = G = B = 250 (lightest)

  // If you need to use byte indexing to convert RGBA components to and from
  // an unsigned int ON_Color value and want your code to work on both little
  // and big endian computers, then use the RGBA_byte_index enum.
  //
  // unsigned int u;
  // unsigned char* rgba = &y;
  // rbga[ON_Color::kRedByteIndex] = red value 0 to 255.
  // rbga[ON_Color::kGreenByteIndex] = green value 0 to 255.
  // rbga[ON_Color::kBlueByteIndex] = blue value 0 to 255.
  // rbga[ON_Color::kAlphaByteIndex] = alpha value 0 to 255.
  // ON_Color color = u;
  enum RGBA_byte_index : unsigned int
  {
    // same for both little and big endian computers.
    kRedByteIndex = 0,
    kGreenByteIndex = 1,
    kBlueByteIndex = 2,
    kAlphaByteIndex = 3
  };

  /*
  Returns:
    A random color.
  */
  static const ON_Color RandomColor();

  /*
  Parameters:
    seed - [in]
    hue_range - [in]
      range of hues. Use ON_Interval::ZeroToTwoPi for all hues.
    saturation_range - [in]
      range of saturations. Use ON_Interval::ZeroToOne for all saturations.
    value_range - [in]
      range of values. Use ON_Interval::ZeroToOne for all values.
  Returns:
     A color generated from seed. The color for a given seed will always be the same.
  */
  static const ON_Color RandomColor(
    ON_Interval hue_range,
    ON_Interval saturation_range,
    ON_Interval value_range
  );

  /*
  Returns:
   A color generated from seed. The color for a given seed will always be the same.
  */
  static const ON_Color RandomColor(
    ON__UINT32 seed
  );

  /*
  Parameters:
    seed - [in]
    hue_range - [in]
      range of hues. Use ON_Interval::ZeroToTwoPi for all hues.
    saturation_range - [in]
      range of saturations. Use ON_Interval::ZeroToOne for all saturations.
    value_range - [in]
      range of values. Use ON_Interval::ZeroToOne for all values.
  Returns:
     A color generated from seed. The color for a given seed will always be the same.
  */
  static const ON_Color RandomColor(
    ON__UINT32 seed,
    ON_Interval hue_range,
    ON_Interval saturation_range,
    ON_Interval value_range
  );

  // If you need to use shifting to convert RGBA components to and from
  // an unsigned int ON_COlor value and you want your code to work 
  // on both little and big endian computers, use the RGBA_shift enum.
  //
  // unsigned int u = 0;
  // u |= ((((unsigned int)red)   & 0xFFU) << ON_Color::RGBA_shift::kRedShift);
  // u |= ((((unsigned int)green) & 0xFFU) << ON_Color::RGBA_shift::kGreenShift);
  // u |= ((((unsigned int)blue)  & 0xFFU) << ON_Color::RGBA_shift::kBlueShift);
  // u |= ((((unsigned int)alpha) & 0xFFU) << ON_Color::RGBA_shift::kAlphaShift);
  // ON_Color color = u;
  enum RGBA_shift : unsigned int
  {
#if defined(ON_LITTLE_ENDIAN)
    kRedShift = 0,
    kGreenShift = 8,
    kBlueShift = 16,
    kAlphaShift = 24
#elif defined(ON_BIG_ENDIAN)
    kRedShift = 24,
    kGreenShift = 16,
    kBlueShift = 8,
    kAlphaShift = 0
#else
#error unknown endian
#endif
  };

  // Sets A = 0
	ON_Color(
    int red,   // ( 0 to 255 )
    int green, // ( 0 to 255 )
    int blue   // ( 0 to 255 )
    );

	ON_Color(
    int red,   // ( 0 to 255 )
    int green, // ( 0 to 255 )
    int blue,  // ( 0 to 255 )
    int alpha  // ( 0 to 255 )  (0 = opaque, 255 = transparent)
    );

  /*
  Parameters:
    colorref - [in]
      Windows COLORREF in little endian RGBA order.
  */
	ON_Color(
    unsigned int colorref
    );

	// Conversion to Windows COLORREF in little endian RGBA order.
  operator unsigned int() const;	

  /*
  Description:
    Call this function when the color is needed in a 
    Windows COLORREF format with alpha = 0;
  Returns
    A Windows COLOREF with alpha = 0.
  */
  unsigned int WindowsRGB() const;

  // < 0 if this < arg, 0 ir this==arg, > 0 if this > arg
  int Compare( const ON_Color& ) const; 

	int Red()   const; // ( 0 to 255 )
	int Green() const; // ( 0 to 255 )
	int Blue()  const; // ( 0 to 255 )
  int Alpha() const; // ( 0 to 255 ) (0 = opaque, 255 = transparent)

	double FractionRed()   const; // ( 0.0 to 1.0 )
	double FractionGreen() const; // ( 0.0 to 1.0 )
	double FractionBlue()  const; // ( 0.0 to 1.0 )
	double FractionAlpha() const; // ( 0.0 to 1.0 ) (0.0 = opaque, 1.0 = transparent)

  void SetRGB(
    int red,   // red in range 0 to 255
    int green, // green in range 0 to 255
    int blue   // blue in range 0 to 255
    );

  void SetFractionalRGB(
    double red,   // red in range 0.0 to 1.0
    double green, // green in range 0.0 to 1.0
    double blue   // blue in range 0.0 to 1.0
    );

  void SetAlpha(
    int alpha // alpha in range 0 to 255 (0 = opaque, 255 = transparent)
    );

  void SetFractionalAlpha(
    double alpha // alpha in range 0.0 to 1.0 (0.0 = opaque, 1.0 = transparent)
    );

  void SetRGBA(
    int red,   // red in range 0 to 255
    int green, // green in range 0 to 255
    int blue,  // blue in range 0 to 255
    int alpha  // alpha in range 0 to 255 (0 = opaque, 255 = transparent)
    );

  // input args
  void SetFractionalRGBA(
    double red,   // red in range 0.0 to 1.0
    double green, // green in range 0.0 to 1.0
    double blue,  // blue in range 0.0 to 1.0
    double alpha  // alpha in range 0.0 to 1.0 (0.0 = opaque, 1.0 = transparent)
    );

  // Hue() returns an angle in the range 0 to 2*pi 
  //
  //           0 = red, pi/3 = yellow, 2*pi/3 = green, 
  //           pi = cyan, 4*pi/3 = blue,5*pi/3 = magenta,
  //           2*pi = red
  double Hue() const;

  // Returns 0.0 (gray) to 1.0 (saturated)
  double Saturation() const;

  // Returns 0.0 (black) to 1.0 (white)
  double Value() const;

  void SetHSV( 
         double h, // hue in radians 0 to 2*pi
         double s, // satuation 0.0 = gray, 1.0 = saturated
         double v // value     
         );

  ///<summary>
  /// Formats used by ON_Color::ToText() and ON_Color::ToString().
  ///</summary>
  enum class TextFormat: unsigned char
  {
    ///<summary>
    /// Indicates no format has been selected. Empty text is created.
    ///</summary>
    Unset = 0,

    ///<summary>
    /// red,green,blue as floating point values from  0.0 to 1.0.
    ///</summary>
    FractionalRGB = 1,

    ///<summary>
    /// red,green,blue as floating point values from 0.0 to 1.0. alpha is appended if it is not zero.
    ///</summary>
    FractionalRGBa = 2,

    ///<summary>
    /// red,green,blue,alpha as floating point values from 0.0 to 1.0.
    ///</summary>
    FractionalRGBA = 3,

    ///<summary>
    /// red,green,blue as decimal integers from 0 to 255.
    ///</summary>
    DecimalRGB = 4,

    ///<summary>
    /// red,green,blue as decimal integers from 0 to 255. alpha is appended if it is not zero.
    ///</summary>
    DecimalRGBa = 5,

    ///<summary>
    /// red,green,blue,alpha as decimal integers from 0 to 255.
    ///</summary>
    DecimalRGBA = 6,

    ///<summary>
    /// red,green,blue as hexadecimal integers from 0 to 255.
    ///</summary>
    HexadecimalRGB = 7,

    ///<summary>
    /// red,green,blue as hexadecimal integers from 0 to 255. alpha is appended if it is not zero.
    ///</summary>
    HexadecimalRGBa = 8,

    ///<summary>
    /// red,green,blue,alpha as hexadecimal integers from 0 to 255.
    ///</summary>
    HexadecimalRGBA = 9,

    ///<summary>
    /// hue (0 to 2pi), saturation (0 to 1), value (0 to 1) as floating point values.
    ///</summary>
    HSV = 10,

    ///<summary>
    /// hue (0 to 2pi), saturation (0 to 1), value (0 to 1) as floating point values. alpha (0 to 1) is appended if it is not zero.
    ///</summary>
    HSVa = 11,

    ///<summary>
    /// hue (0 to 2pi), saturation (0 to 1), value (0 to 1), alpha (0 to 1) as floating point values.
    ///</summary>
    HSVA = 12,
  };

  /*
  Parameters:
    format - [in]
    separator - [in]
    character to sepearate numbers (unicode code point - UTF-16 surrogate pairs not supported)
      pass 0 for default.
    bFormatUnsetColor - [in]
      If true, ON_Color::UnsetColor will return "UnsetColor". Otherwise ON_Color::UnsetColor will return the empty string.
    text_log - [in]
      destination of the text.
  */
  const ON_wString ToString(
    ON_Color::TextFormat format,
    wchar_t separator,
    bool bFormatUnsetColor,
    class ON_TextLog& text_log
  ) const;

  /*
  Parameters:
    format - [in]
      If format is ON_Color::TextFormat::Unset, then text_log.ColorFormat is used.
    separator - [in]
    character to sepearate numbers (unicode code point - UTF-16 surrogate pairs not supported)
      pass 0 for default.
    bFormatUnsetColor - [in]
      If true, ON_Color::UnsetColor will return "UnsetColor". Otherwise ON_Color::UnsetColor will return the empty string.
    text_log - [in]
      destination of the text.
  */
  void ToText(
    ON_Color::TextFormat format,
    wchar_t separator,
    bool bFormatUnsetColor,
    class ON_TextLog& text_log
  ) const;


private:
  union {
    // On little endian (Intel) computers, m_color has the same byte order
    // as Windows COLORREF values.
    // On little endian computers, m_color = 0xaabbggrr as an unsigned int value.
    // On big endian computers, m_color = 0xrrggbbaa as an unsigned int value
    //  rr = red component 0-255
    //  gg = grean component 0-255
    //  bb = blue component 0-255
    //  aa = alpha 0-255. 0 means opaque, 255 means transparent.
    unsigned int m_color = 0;

    // m_colorComponent is a 4 unsigned byte array in RGBA order
    // red component = m_RGBA[ON_Color::RGBA_byte::kRed]
    // grean component = m_RGBA[ON_Color::RGBA_byte::kGreen]
    // blue component = m_RGBA[ON_Color::RGBA_byte::kBlue]
    // alpha component = m_RGBA[ON_Color::RGBA_byte::kAlpha]
    unsigned char m_RGBA[4];
  };
};

///////////////////////////////////////////////////////////////////////////////
//
// Class ON_ColorStop
// 
// Combination of a color and a single value. Typically used for defining
// gradient fills over a series of colors.
class ON_CLASS ON_ColorStop
{
public:
  ON_ColorStop() = default;
  ON_ColorStop(const ON_Color& color, double position);

  bool Write(class ON_BinaryArchive& archive) const;
  bool Read(class ON_BinaryArchive& archive);

  ON_Color m_color = ON_Color::UnsetColor;
  double m_position = 0;
};

#if defined(ON_DLL_TEMPLATE)
ON_DLL_TEMPLATE template class ON_CLASS ON_SimpleArray<ON_ColorStop>;
#endif


class ON_CLASS ON_4fColor
{
public:
  ON_4fColor();
  ~ON_4fColor() = default;
  ON_4fColor(const ON_4fColor&) = default;
  ON_4fColor& operator=(const ON_4fColor&) = default;

  static const ON_4fColor Unset;

  //Note that these function will set the alpha correctly from ON_Colors "inverted" alpha.
  ON_4fColor(const ON_Color&);
  ON_4fColor& operator=(const ON_Color&);

  //Will invert the opacity alpha to transparency.
  operator ON_Color(void) const;

  float Red(void) const;
  void SetRed(float);

  float Green(void) const;
  void SetGreen(float);

  float Blue(void) const;
  void SetBlue(float);

  //Alpha in ON_4fColor is OPACITY - not transparency as in ON_Color.
  float Alpha(void) const;
  void SetAlpha(float);

  void SetRGBA(float r, float g, float b, float a);

  bool IsValid(class ON_TextLog* text_log = nullptr) const;

  // < 0 if this < arg, 0 ir this==arg, > 0 if this > arg
  int Compare(const ON_4fColor&) const;

private:
  float m_color[4];
};

#if defined(ON_DLL_TEMPLATE)
ON_DLL_TEMPLATE template class ON_CLASS ON_SimpleArray<ON_4fColor>;
#endif


#endif
