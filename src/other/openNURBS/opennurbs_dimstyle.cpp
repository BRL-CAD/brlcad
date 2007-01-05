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

#include "opennurbs.h"

ON_OBJECT_IMPLEMENT( ON_DimStyle, ON_Object, "81BD83D5-7120-41c4-9A57-C449336FF12C" );

ON_DimStyle::ON_DimStyle()
{
  SetDefaults();
}

ON_DimStyle::~ON_DimStyle()
{
}

void ON_DimStyle::SetDefaults()
{
  m_dimstyle_index = -1;
  memset(&m_dimstyle_id,0,sizeof(m_dimstyle_id));
  m_dimstyle_name = L"Default";

  m_extextension = 0.5;
  m_extoffset = 0.5;
  m_arrowsize = 1.0;
  m_centermark = 0.5;
  m_textgap = 0.25;
  m_textheight = 1.0;
  m_textalign = ON::dtAboveLine;
  m_arrowtype = 0;
  m_angularunits = 0;
  m_lengthformat = 0;
  m_angleformat = 0;
  m_lengthresolution = 2;
  m_angleresolution = 2;
  m_fontindex = -1;

  // Added at 1.3
  m_lengthfactor = 1.0;  
  m_bAlternate = false;
  m_alternate_lengthfactor = 25.4;
  m_alternate_lengthformat = 0;
  m_alternate_lengthresolution = 2;
  m_alternate_angleformat = 0;
  m_alternate_angleresolution = 2;

  m_prefix = L"";
  m_suffix = L"";
  m_alternate_prefix = L" [";
  m_alternate_suffix = L"]";
  m_valid = 0;

  m_dimextension = 0.0;

  m_leaderarrowsize = 1.0;
  m_leaderarrowtype = 0;
  m_bSuppressExtension1 = false;
  m_bSuppressExtension2 = false;

}


// copy from ON_3dmAnnotationSettings and add a couple of fields
ON_DimStyle& ON_DimStyle::operator=( const ON_3dmAnnotationSettings& src)
{
  SetDefaults();

  m_extextension = src.m_dimexe;
  m_extoffset = src.m_dimexo;
  m_arrowsize = src.m_arrowlength;
  m_textalign = src.m_textalign;
  m_centermark = src.m_centermark;
  m_textgap = src.m_dimexo / 2.0;
  m_textheight = src.m_textheight;
  m_arrowtype = src.m_arrowtype;
  m_angularunits = src.m_angularunits;
  m_lengthformat = src.m_lengthformat;
  m_angleformat = src.m_angleformat;
  m_lengthresolution = src.m_resolution;
  m_angleresolution = src.m_resolution;

  return *this;
}

//////////////////////////////////////////////////////////////////////
//
// ON_Object overrides

BOOL ON_DimStyle::IsValid( ON_TextLog* text_log ) const
{
  return ( m_dimstyle_name.Length() > 0 && m_dimstyle_index >= 0);
}

void ON_DimStyle::Dump( ON_TextLog& dump ) const
{
  const wchar_t* name = m_dimstyle_name;
  if ( !name )
    name = L"";
  dump.Print("dimstyle index = %d\n",m_dimstyle_index);
  dump.Print("dimstyle name = \"%S\"\n",name);
}

BOOL ON_DimStyle::Write(
       ON_BinaryArchive& file // serialize definition to binary archive
     ) const
{
  // changed to version 1.4 Dec 28, 05
  // changed to version 1.5 Mar 23, 06
  BOOL rc = file.Write3dmChunkVersion(1,5);

  if (rc) rc = file.WriteInt(m_dimstyle_index);
  if (rc) rc = file.WriteString(m_dimstyle_name);

  if (rc) rc = file.WriteDouble(m_extextension);
  if (rc) rc = file.WriteDouble(m_extoffset);
  if (rc) rc = file.WriteDouble(m_arrowsize);
  if (rc) rc = file.WriteDouble(m_centermark);
  if (rc) rc = file.WriteDouble(m_textgap);
  
  if (rc) rc = file.WriteInt(m_textalign);
  if (rc) rc = file.WriteInt(m_arrowtype);
  if (rc) rc = file.WriteInt(m_angularunits);
  if (rc) rc = file.WriteInt(m_lengthformat);
  if (rc) rc = file.WriteInt(m_angleformat);
  if (rc) rc = file.WriteInt(m_lengthresolution);
  if (rc) rc = file.WriteInt(m_angleresolution);
  if (rc) rc = file.WriteInt(m_fontindex);

  if (rc) rc = file.WriteDouble(m_textheight);

  // added 1/13/05 ver 1.2
  if (rc) rc = file.WriteDouble(m_lengthfactor);
  if (rc) rc = file.WriteString(m_prefix);
  if (rc) rc = file.WriteString(m_suffix);

  if (rc) rc = file.WriteBool(m_bAlternate);
  if (rc) rc = file.WriteDouble(m_alternate_lengthfactor);
  if (rc) rc = file.WriteInt(m_alternate_lengthformat);
  if (rc) rc = file.WriteInt(m_alternate_lengthresolution);
  if (rc) rc = file.WriteInt(m_alternate_angleformat);
  if (rc) rc = file.WriteInt(m_alternate_angleresolution);
  if (rc) rc = file.WriteString(m_alternate_prefix);
  if (rc) rc = file.WriteString(m_alternate_suffix);
  if (rc) rc = file.WriteInt(m_valid);

  // Added 24 October 2005 ver 1.3
  if (rc) rc = file.WriteUuid(m_dimstyle_id);

  // Added Dec 28, 05 ver 1.4
  if (rc) rc = file.WriteDouble(m_dimextension);

  // Added Mar 23 06 ver 1.5
  if (rc) rc = file.WriteDouble(m_leaderarrowsize);
  if (rc) rc = file.WriteInt(m_leaderarrowtype);
  if (rc) rc = file.WriteBool(m_bSuppressExtension1);
  if (rc) rc = file.WriteBool(m_bSuppressExtension2);

  return rc;
}

BOOL ON_DimStyle::Read(
       ON_BinaryArchive& file // restore definition from binary archive
     )
{
  SetDefaults();

  int major_version = 0;
  int minor_version = 0;
  BOOL rc = file.Read3dmChunkVersion(&major_version,&minor_version);


  if ( major_version >= 1 ) 
  {
    if ( rc) rc = file.ReadInt( &m_dimstyle_index);
    if ( rc) rc = file.ReadString( m_dimstyle_name);
    
    if ( rc) rc = file.ReadDouble( &m_extextension);
    if ( rc) rc = file.ReadDouble( &m_extoffset);
    if ( rc) rc = file.ReadDouble( &m_arrowsize);
    if ( rc) rc = file.ReadDouble( &m_centermark);
    if ( rc) rc = file.ReadDouble( &m_textgap);
    
    if ( rc) rc = file.ReadInt( &m_textalign);
    if ( rc) rc = file.ReadInt( &m_arrowtype);
    if ( rc) rc = file.ReadInt( &m_angularunits);
    if ( rc) rc = file.ReadInt( &m_lengthformat);
    if ( rc) rc = file.ReadInt( &m_angleformat);
    if ( rc) rc = file.ReadInt( &m_lengthresolution);
    if ( rc) rc = file.ReadInt( &m_angleresolution);
    if ( rc) rc = file.ReadInt( &m_fontindex);

    if( minor_version >= 1)
      if ( rc) rc = file.ReadDouble( &m_textheight);

    // added 1/13/05
    if( minor_version >= 2)
    {
      if (rc) rc = file.ReadDouble( &m_lengthfactor);
      if (rc) rc = file.ReadString( m_prefix);
      if (rc) rc = file.ReadString( m_suffix);

      if (rc) rc = file.ReadBool( &m_bAlternate);
      if (rc) rc = file.ReadDouble(&m_alternate_lengthfactor);
      if (rc) rc = file.ReadInt( &m_alternate_lengthformat);
      if (rc) rc = file.ReadInt( &m_alternate_lengthresolution);
      if (rc) rc = file.ReadInt( &m_alternate_angleformat);
      if (rc) rc = file.ReadInt( &m_alternate_angleresolution);
      if (rc) rc = file.ReadString( m_alternate_prefix);
      if (rc) rc = file.ReadString( m_alternate_suffix);
      if (rc) rc = file.ReadInt( &m_valid);

      if ( minor_version >= 3 )
      {
        if (rc) rc = file.ReadUuid(m_dimstyle_id);
      }
    }
    // Added Dec 28, 05 ver 1.4
    if( minor_version >= 4)
      if( rc) rc = file.ReadDouble( &m_dimextension);

    // Added Mar 23 06 ver 1.5
    if( minor_version >= 5)
    {
      if (rc) rc = file.ReadDouble( &m_leaderarrowsize);
      if (rc) rc = file.ReadInt( &m_leaderarrowtype);
      if (rc) rc = file.ReadBool( &m_bSuppressExtension1);
      if (rc) rc = file.ReadBool( &m_bSuppressExtension2);
    }
  }
  else
    rc = FALSE;
  return rc;
}

void ON_DimStyle::EmergencyDestroy()
{
  m_prefix.EmergencyDestroy();
  m_suffix.EmergencyDestroy();
  m_alternate_prefix.EmergencyDestroy();
  m_alternate_suffix.EmergencyDestroy();
}

//////////////////////////////////////////////////////////////////////
//
// Interface
void ON_DimStyle::SetName( const wchar_t* s )
{
  m_dimstyle_name = s;
  m_dimstyle_name.TrimLeftAndRight();
  ValidateField( fn_name);
}

void ON_DimStyle::SetName( const char* s )
{
  m_dimstyle_name = s;
  m_dimstyle_name.TrimLeftAndRight();
  ValidateField( fn_name);
}

void ON_DimStyle::GetName( ON_wString& s ) const
{
  s = m_dimstyle_name;
}

const wchar_t* ON_DimStyle::Name() const
{
  const wchar_t* s = m_dimstyle_name;
  return s;
}

void ON_DimStyle::SetIndex(int i )
{
  m_dimstyle_index = i;
  ValidateField( fn_index);
}

int ON_DimStyle::Index() const
{
  return m_dimstyle_index;
}

double ON_DimStyle::ExtExtension() const
{
  return m_extextension;
}

void ON_DimStyle::SetExtExtension( const double e)
{
  // Allow negative?
  m_extextension = e;
  ValidateField( fn_extextension);
}

double ON_DimStyle::ExtOffset() const
{
  return m_extoffset;
}

void ON_DimStyle::SetExtOffset( const double e)
{
  m_extoffset = e;
  ValidateField( fn_extoffset);
}

double ON_DimStyle::ArrowSize() const
{
  return m_arrowsize;
}

void ON_DimStyle::SetArrowSize( const double e)
{
  m_arrowsize = e;
  ValidateField( fn_arrowsize);
}

double ON_DimStyle::LeaderArrowSize() const
{
  return m_leaderarrowsize;
}

void ON_DimStyle::SetLeaderArrowSize( const double e)
{
  m_leaderarrowsize = e;
  ValidateField( fn_leaderarrowsize);
}

double ON_DimStyle::CenterMark() const
{
  return m_centermark;
}

void ON_DimStyle::SetCenterMark( const double e)
{
  m_centermark = e;
  ValidateField( fn_centermark);
}

int ON_DimStyle::TextAlignment() const
{
  return m_textalign;
}

void ON_DimStyle::SetTextAlignment( ON::eTextDisplayMode a)
{
  m_textalign = a;
  ValidateField( fn_textalign);
}

int ON_DimStyle::ArrowType() const
{
  return m_arrowtype;
}

void ON_DimStyle::SetArrowType( eArrowType a)
{
  m_arrowtype = a;
  ValidateField( fn_arrowtype);
}

int ON_DimStyle::LeaderArrowType() const
{
  return m_leaderarrowtype;
}

void ON_DimStyle::SetLeaderArrowType( eArrowType a)
{
  m_leaderarrowtype = a;
  ValidateField( fn_leaderarrowtype);
}

int ON_DimStyle::AngularUnits() const
{
  return m_angularunits;
}

void ON_DimStyle::SetAngularUnits( int u)
{
  m_angularunits = u;
  ValidateField( fn_angularunits);
}

int ON_DimStyle::LengthFormat() const
{
  return m_lengthformat;
}

void ON_DimStyle::SetLengthFormat( int f)
{
  m_lengthformat = f;
  ValidateField( fn_lengthformat);
}

int ON_DimStyle::AngleFormat() const
{
  return m_angleformat;
}

void ON_DimStyle::SetAngleFormat( int f)
{
  m_angleformat = f;
  ValidateField( fn_angleformat);
}

int ON_DimStyle::LengthResolution() const
{
  return m_lengthresolution;
}

void ON_DimStyle::SetLengthResolution( int r)
{
  if( r >= 0 && r < 16)
  {
    m_lengthresolution = r;
    ValidateField( fn_lengthresolution);
  }
}

int ON_DimStyle::AngleResolution() const
{
  return m_angleresolution;
}

void ON_DimStyle::SetAngleResolution( int r)
{
  if( r >= 0 && r < 16)
  {
    m_angleresolution = r;
    ValidateField( fn_angleresolution);
  }
}

int ON_DimStyle::FontIndex() const
{
  return m_fontindex;
}

void ON_DimStyle::SetFontIndex( int index)
{
  m_fontindex = index;
  ValidateField( fn_fontindex);
}

double ON_DimStyle::TextGap() const
{
  return m_textgap;
}

void ON_DimStyle::SetTextGap( double gap)
{
  if( gap >= 0.0)
  {
    m_textgap = gap;
    ValidateField( fn_textgap);
  }
}

double ON_DimStyle::TextHeight() const
{
  return m_textheight;
}

void ON_DimStyle::SetTextHeight( double height)
{
  if( height > ON_SQRT_EPSILON)
  {
    m_textheight = height;
    ValidateField( fn_textheight);
  }
}


double ON_DimStyle::LengthFactor() const
{
  return m_lengthfactor;
}
void ON_DimStyle::SetLengthactor( double factor)
{
  m_lengthfactor = factor;
  ValidateField( fn_lengthfactor);
}

bool ON_DimStyle::Alternate() const
{
  return m_bAlternate;
}
void ON_DimStyle::SetAlternate( bool bAlternate)
{
  m_bAlternate = bAlternate;
  ValidateField( fn_bAlternate);
}

double ON_DimStyle::AlternateLengthFactor() const
{
  return m_alternate_lengthfactor;
}
void ON_DimStyle::SetAlternateLengthactor( double factor)
{
  m_alternate_lengthfactor = factor;
  ValidateField( fn_alternate_lengthfactor);
}

int ON_DimStyle::AlternateLengthFormat() const
{
  return m_alternate_lengthformat;
}
void ON_DimStyle::SetAlternateLengthFormat( int format)
{
  m_alternate_lengthformat = format;
  ValidateField( fn_alternate_lengthformat);
}

int ON_DimStyle::AlternateLengthResolution() const
{
  return m_alternate_lengthresolution;
}
void ON_DimStyle::SetAlternateLengthResolution( int resolution)
{
  m_alternate_lengthresolution = resolution;
  ValidateField( fn_alternate_lengthresolution);
}

int ON_DimStyle::AlternateAngleFormat() const
{
  return m_alternate_angleformat;
}
void ON_DimStyle::SetAlternateAngleFormat( int format)
{
  m_alternate_angleformat = format;
  ValidateField( fn_alternate_angleformat);
}

int ON_DimStyle::AlternateAngleResolution() const
{
  return m_alternate_angleresolution;
}
void ON_DimStyle::SetAlternateAngleResolution( int resolution)
{
  m_alternate_angleresolution = resolution;
  ValidateField( fn_alternate_angleresolution);
}

void ON_DimStyle::GetPrefix( ON_wString& prefix) const
{
  prefix = m_prefix;
}
const wchar_t* ON_DimStyle::Prefix() const
{
  return m_prefix;
}
void ON_DimStyle::SetPrefix( wchar_t* prefix)
{
  m_prefix = prefix;
  ValidateField( fn_prefix);
}

void ON_DimStyle::GetSuffix( ON_wString& suffix) const
{
  suffix = m_suffix;
}
const wchar_t* ON_DimStyle::Suffix() const
{
  return m_suffix;
}
void ON_DimStyle::SetSuffix( wchar_t* suffix)
{
  m_suffix = suffix;
  ValidateField( fn_suffix);
}

void ON_DimStyle::GetAlternatePrefix( ON_wString& prefix) const
{
  prefix = m_alternate_prefix;
}
const wchar_t* ON_DimStyle::AlternatePrefix() const
{
  return m_alternate_prefix;
}
void ON_DimStyle::SetAlternatePrefix( wchar_t* prefix)
{
  m_alternate_prefix = prefix;
  ValidateField( fn_alternate_prefix);
}

void ON_DimStyle::GetAlternateSuffix( ON_wString& suffix) const
{
  suffix = m_alternate_suffix;
}
const wchar_t* ON_DimStyle::AlternateSuffix() const
{
  return m_alternate_suffix;
}
void ON_DimStyle::SetAlternateSuffix( wchar_t* suffix)
{
  m_alternate_suffix = suffix;
  ValidateField( fn_alternate_suffix);
}

bool ON_DimStyle::SuppressExtension1() const
{
  return m_bSuppressExtension1;
}
void ON_DimStyle::SetSuppressExtension1( bool suppress)
{
  m_bSuppressExtension1 = suppress;
  ValidateField( fn_suppressextension1);
}
bool ON_DimStyle::SuppressExtension2() const
{
  return m_bSuppressExtension2;
}
void ON_DimStyle::SetSuppressExtension2( bool suppress)
{
  m_bSuppressExtension2 = suppress;
  ValidateField( fn_suppressextension2);
}

// replace the values in this with any valid fields in override
// mark the fields that came from this dimstyle invalid
// and the ones from the override dimstyle valid
void ON_DimStyle::Composite( const ON_DimStyle& OverRide)
{
  InvalidateAllFields();

  if( OverRide.IsFieldValid( fn_name))                          { ValidateField( fn_name); m_dimstyle_name = OverRide.m_dimstyle_name; }
  if( OverRide.IsFieldValid( fn_index))                         { ValidateField( fn_index); m_dimstyle_index = OverRide.m_dimstyle_index; }
  if( OverRide.IsFieldValid( fn_extextension))                  { ValidateField( fn_extextension); m_extextension = OverRide.m_extextension; }
  if( OverRide.IsFieldValid( fn_extoffset))                     { ValidateField( fn_extoffset); m_extoffset = OverRide.m_extoffset; }
  if( OverRide.IsFieldValid( fn_arrowsize))                     { ValidateField( fn_arrowsize); m_arrowsize = OverRide.m_arrowsize; }
  if( OverRide.IsFieldValid( fn_leaderarrowsize))               { ValidateField( fn_leaderarrowsize); m_leaderarrowsize = OverRide.m_leaderarrowsize; }
  if( OverRide.IsFieldValid( fn_centermark))                    { ValidateField( fn_centermark); m_centermark = OverRide.m_centermark; }
  if( OverRide.IsFieldValid( fn_textgap))                       { ValidateField( fn_textgap); m_textgap = OverRide.m_textgap; }
  if( OverRide.IsFieldValid( fn_textheight))                    { ValidateField( fn_textheight); m_textheight = OverRide.m_textheight; }
  if( OverRide.IsFieldValid( fn_textalign))                     { ValidateField( fn_textalign); m_textalign = OverRide.m_textalign; }
  if( OverRide.IsFieldValid( fn_arrowtype))                     { ValidateField( fn_arrowtype); m_arrowtype = OverRide.m_arrowtype; }
  if( OverRide.IsFieldValid( fn_leaderarrowtype))               { ValidateField( fn_leaderarrowtype); m_leaderarrowtype = OverRide.m_leaderarrowtype; }
  if( OverRide.IsFieldValid( fn_angularunits))                  { ValidateField( fn_angularunits); m_angularunits = OverRide.m_angularunits; }
  if( OverRide.IsFieldValid( fn_lengthformat))                  { ValidateField( fn_lengthformat); m_lengthformat = OverRide.m_lengthformat; }
  if( OverRide.IsFieldValid( fn_angleformat))                   { ValidateField( fn_angleformat); m_angleformat = OverRide.m_angleformat; }
  if( OverRide.IsFieldValid( fn_angleresolution))               { ValidateField( fn_angleresolution); m_angleresolution = OverRide.m_angleresolution; }
  if( OverRide.IsFieldValid( fn_lengthresolution))              { ValidateField( fn_lengthresolution); m_lengthresolution = OverRide.m_lengthresolution; }
  if( OverRide.IsFieldValid( fn_fontindex))                     { ValidateField( fn_fontindex); m_fontindex = OverRide.m_fontindex; }
  if( OverRide.IsFieldValid( fn_lengthfactor))                  { ValidateField( fn_lengthfactor); m_lengthfactor = OverRide.m_lengthfactor; }
  if( OverRide.IsFieldValid( fn_bAlternate))                    { ValidateField( fn_bAlternate); m_bAlternate = OverRide.m_bAlternate; }
  if( OverRide.IsFieldValid( fn_alternate_lengthfactor))        { ValidateField( fn_alternate_lengthfactor); m_alternate_lengthfactor = OverRide.m_alternate_lengthfactor; }
  if( OverRide.IsFieldValid( fn_alternate_lengthformat))        { ValidateField( fn_alternate_lengthformat); m_alternate_lengthformat = OverRide.m_alternate_lengthformat; }
  if( OverRide.IsFieldValid( fn_alternate_lengthresolution))    { ValidateField( fn_alternate_lengthresolution); m_alternate_lengthresolution = OverRide.m_alternate_lengthresolution; }
  if( OverRide.IsFieldValid( fn_prefix))                        { ValidateField( fn_prefix); m_prefix = OverRide.m_prefix; }
  if( OverRide.IsFieldValid( fn_suffix))                        { ValidateField( fn_suffix); m_suffix = OverRide.m_suffix; }
  if( OverRide.IsFieldValid( fn_alternate_prefix))              { ValidateField( fn_alternate_prefix); m_alternate_prefix = OverRide.m_alternate_prefix; }
  if( OverRide.IsFieldValid( fn_alternate_suffix))              { ValidateField( fn_alternate_suffix); m_alternate_suffix = OverRide.m_alternate_suffix; }
  if( OverRide.IsFieldValid( fn_dimextension))                  { ValidateField( fn_dimextension); m_dimextension = OverRide.m_dimextension; }
  if( OverRide.IsFieldValid( fn_suppressextension1))            { ValidateField( fn_suppressextension1); m_bSuppressExtension1 = OverRide.m_bSuppressExtension1; }
  if( OverRide.IsFieldValid( fn_suppressextension2))            { ValidateField( fn_suppressextension2); m_bSuppressExtension2 = OverRide.m_bSuppressExtension2; }
}

void ON_DimStyle::InvalidateField( eField field)
{
  m_valid &= ~( 1 << field);
}

void ON_DimStyle::InvalidateAllFields()
{
  m_valid = 0;
}

void ON_DimStyle::ValidateField( eField field)
{
  m_valid |= ( 1 << field);
}

bool ON_DimStyle::IsFieldValid( eField field) const
{
  return ( m_valid & ( 1 << field)) ? true : false;
}


// ver 1.4, Dec 28, 05
double ON_DimStyle::DimExtension() const
{
  return m_dimextension;
}

void ON_DimStyle::SetDimExtension( const double e)
{
  // Allow negative?
  m_dimextension = e;
  ValidateField( fn_dimextension);
}



