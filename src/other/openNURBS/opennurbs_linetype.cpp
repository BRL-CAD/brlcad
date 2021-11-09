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

#include "opennurbs.h"

#if !defined(ON_COMPILING_OPENNURBS)
// This check is included in all opennurbs source .c and .cpp files to insure
// ON_COMPILING_OPENNURBS is defined when opennurbs source is compiled.
// When opennurbs source is being compiled, ON_COMPILING_OPENNURBS is defined 
// and the opennurbs .h files alter what is declared and how it is declared.
#error ON_COMPILING_OPENNURBS must be defined when compiling opennurbs
#endif

bool ON_IsHairlinePrintWidth(double width_mm)
{
  if(width_mm > 0.0 && width_mm < 0.001)
    return true;

  return false;
}

double ON_HairlinePrintWidth()
{
  return 0.0001;
}


//////////////////////////////////////////////////////////////////////
// class ON_LinetypeSegment
bool ON_LinetypeSegment::operator==( const ON_LinetypeSegment& src) const
{
  return ( m_length == src.m_length && m_seg_type == src.m_seg_type);
}

bool ON_LinetypeSegment::operator!=( const ON_LinetypeSegment& src) const
{
  return ( m_length != src.m_length || m_seg_type != src.m_seg_type);
}

ON_LinetypeSegment::eSegType ON_LinetypeSegment::SegmentTypeFromUnsigned(
  unsigned int segment_type_as_unsigned
  )
{
  switch (segment_type_as_unsigned)
  {
  ON_ENUM_FROM_UNSIGNED_CASE(ON_LinetypeSegment::eSegType::Unset);
  ON_ENUM_FROM_UNSIGNED_CASE(ON_LinetypeSegment::eSegType::stLine);
  ON_ENUM_FROM_UNSIGNED_CASE(ON_LinetypeSegment::eSegType::stSpace);
  }
  ON_ERROR("Invalid segment_type_as_unsigned value.");
  return ON_LinetypeSegment::eSegType::stLine;
}

ON_LinetypeSegment::ON_LinetypeSegment(
  double segment_length,
  ON_LinetypeSegment::eSegType segment_type
  )
  : m_length(segment_length)
  , m_seg_type(segment_type)
{}

void ON_LinetypeSegment::Dump( ON_TextLog& dump) const
{
  switch( m_seg_type)
  {
  case ON_LinetypeSegment::eSegType::stLine:
    dump.Print( "Segment type = Line: %g\n", m_length);
    break;
  case ON_LinetypeSegment::eSegType::stSpace:
    dump.Print( "Segment type = Space: %g\n", m_length);
    break;
  }
}

//////////////////////////////////////////////////////////////////////
// class ON_Linetype

ON_OBJECT_IMPLEMENT( ON_Linetype, ON_ModelComponent, "26F10A24-7D13-4f05-8FDA-8E364DAF8EA6" );

const ON_Linetype* ON_Linetype::FromModelComponentRef(
  const class ON_ModelComponentReference& model_component_reference,
  const ON_Linetype* none_return_value
  )
{
  const ON_Linetype* p = ON_Linetype::Cast(model_component_reference.ModelComponent());
  return (nullptr != p) ? p : none_return_value;
}


ON_Linetype::ON_Linetype() ON_NOEXCEPT
  : ON_ModelComponent(ON_ModelComponent::Type::LinePattern)
{}

ON_Linetype::ON_Linetype( const ON_Linetype& src )
  : ON_ModelComponent(ON_ModelComponent::Type::LinePattern,src)
  , m_segments(src.m_segments)
{
}

bool ON_Linetype::IsValid( ON_TextLog* text_log ) const
{
  int i, count = m_segments.Count();

  if (false == ON_ModelComponent::IsValid(text_log))
    return false;

  // An ON_Linetype with an empty name is valid.

  if ( count < 1 )
  {
    if ( text_log )
      text_log->Print("ON_Linetype m_segments.Count() = 0\n");
    return false;
  }

  if ( 1 == count )
  {
    if ( m_segments[0].m_length <= 0.0  )
    {
      if ( text_log )
        text_log->Print("ON_Linetype bogus single segment linetype - length <= 0.0 (it must be > 0)\n");
      return false;
    }

    if ( ON_LinetypeSegment::eSegType::stLine != m_segments[0].m_seg_type )
    {
      if ( text_log )
        text_log->Print("ON_Linetype bogus single segment linetype - type != stLine\n");
      return false;
    }
  }
  else
  {
    for (i = 0; i < count; i++ )
    {
      if ( m_segments[i].m_length < 0.0 )
      {
        if ( text_log )
          text_log->Print("ON_Linetype segment has negative length.\n");
        return false;
      }

      if ( ON_LinetypeSegment::eSegType::stLine != m_segments[i].m_seg_type && ON_LinetypeSegment::eSegType::stSpace != m_segments[i].m_seg_type )
      {
        if ( text_log )
          text_log->Print("ON_Linetype segment has invalid m_seg_type.\n");
        return false;
      }

      if ( i )
      {
        if ( m_segments[i].m_seg_type == m_segments[i-1].m_seg_type )
        {
          if ( text_log )
            text_log->Print("ON_Linetype consecutive segments have same type.\n");
          return false;
        }

        if ( 0.0 == m_segments[i].m_length && 0.0 == m_segments[i-1].m_length )
        {
          if ( text_log )
            text_log->Print("ON_Linetype consecutive segments have length zero.\n");
          return false;
        }
      }
    }
  }

  return true;
}

void ON_Linetype::Dump( ON_TextLog& dump ) const
{
  ON_ModelComponent::Dump(dump);
  dump.Print( "Segment count = %d\n", m_segments.Count());
  dump.Print( "Pattern length = %g\n", PatternLength());
  dump.Print( "Pattern = (" );
  for( int i = 0; i < m_segments.Count(); i++)
  {
    const ON_LinetypeSegment& seg = m_segments[i];
    if ( i )
      dump.Print(",");
    switch( seg.m_seg_type)
    {
    case ON_LinetypeSegment::eSegType::stLine:
      dump.Print( "line");
      break;
    case ON_LinetypeSegment::eSegType::stSpace:
      dump.Print( "space");
      break;
    default:
      dump.Print( "invalid");
      break;
    }
  }
  dump.Print(")\n");
}

bool ON_Linetype::Write( ON_BinaryArchive& file) const
{
  bool rc = false;
  if (file.Archive3dmVersion() < 60)
  {
    if (!file.BeginWrite3dmChunk(TCODE_ANONYMOUS_CHUNK, 1, 1))
      return false;
    for (;;)
    {
      // chunk version 1.0 fields
      if (!file.Write3dmReferencedComponentIndex(*this))
        break;
        
      ON_wString linetype_name;
      GetName(linetype_name);
      if (!file.WriteString(linetype_name))
        break;

      if (!file.WriteArray(m_segments))
        break;

      // chunk version 1.1 fields
      if (!file.WriteUuid(Id()))
        break;

      rc = true;
      break;
    }
  }
  else
  {
    if (!file.BeginWrite3dmChunk(TCODE_ANONYMOUS_CHUNK, 2, 0))
      return false;
    for (;;)
    {
      // chunk version 1.0 fields
      if (!file.WriteModelComponentAttributes(*this,ON_ModelComponent::Attributes::BinaryArchiveAttributes))
        break;

      if (!file.WriteArray(m_segments))
        break;

      rc = true;
      break;
    }

  }
  if (!file.EndWrite3dmChunk())
    rc = false;
  return rc;
}

bool ON_Linetype::Read( ON_BinaryArchive& file)
{
  *this = ON_Linetype::Unset;

  int major_version=0;
  int minor_version=0;
  if (!file.BeginRead3dmChunk( TCODE_ANONYMOUS_CHUNK, &major_version, &minor_version ))
    return false;

  bool rc = false;


  if( 1 == major_version ) 
  {
    for (;;)
    {
      // chunk version 1.0 fields
      int linetype_index = Index();
      if (!file.ReadInt(&linetype_index))
        break;
      SetIndex(linetype_index);

      ON_wString linetype_name;
      GetName(linetype_name);
      if (!file.ReadString(linetype_name))
        break;
      SetName(linetype_name);

      if (!file.ReadArray(m_segments))
        break;

      if (minor_version >= 1)
      {
        ON_UUID linetype_id = Id();
        if (!file.ReadUuid(linetype_id))
          break;
        SetId(linetype_id);
      }

      rc = true;
      break;
    }
  }
  else if ( 2 == major_version )
  {
    for (;;)
    {
      // chunk version 2.0 fields
      unsigned int model_component_attributes_filter = 0;
      if (!file.ReadModelComponentAttributes(*this,&model_component_attributes_filter))
        break;

      if (!file.ReadArray(m_segments))
        break;

      rc = true;
      break;
    }
  }

  if ( !file.EndRead3dmChunk() )
    rc = false;

  return rc;
}

bool ON_Linetype::PatternIsSet() const
{
  return (0 != (m_is_set_bits & ON_Linetype::pattern_bit));
}


bool ON_Linetype::ClearPattern()
{
  if (false == PatternIsLocked())
  {
    m_is_set_bits &= ~ON_Linetype::pattern_bit;
    m_segments.Destroy();
  }
  return (false == PatternIsSet());
}

bool ON_Linetype::PatternIsLocked() const
{
  return (0 != (m_is_locked_bits & ON_Linetype::pattern_bit));
}

void ON_Linetype::LockPattern()
{
  m_is_locked_bits |= ON_Linetype::pattern_bit;
}

double ON_Linetype::PatternLength() const
{
  double length = 0.0;
  int seg_count = m_segments.Count();
  for( int i = 0; i < seg_count; i++)
  {
    length += m_segments[i].m_length;
  }
  return length;
}

ON_SimpleArray<ON_LinetypeSegment>* ON_Linetype::ExpertSegments()
{
  if (PatternIsLocked())
    return nullptr;
  return &m_segments;
}

const ON_SimpleArray<ON_LinetypeSegment>& ON_Linetype::Segments() const
{
  return m_segments;
}

int ON_Linetype::SegmentCount() const
{
  return m_segments.Count();
}

int ON_Linetype::AppendSegment( const ON_LinetypeSegment& segment)
{
  if (PatternIsLocked())
    return -1;

  m_segments.Append( segment);
  return( m_segments.Count()-1);
}

bool ON_Linetype::RemoveSegment( int index )
{
  if (PatternIsLocked())
    return false;
  bool rc = ( index >= 0 && index < m_segments.Count());
  if (rc)
    m_segments.Remove(index);
  return rc;
}

bool ON_Linetype::SetSegment( int index, const ON_LinetypeSegment& segment)
{
  if (PatternIsLocked())
    return false;

  if( index >= 0 && index < m_segments.Count())
  {
    m_segments[index] = segment;
    return true;
  }
  else
    return false;
}

bool ON_Linetype::SetSegment( int index, double length, ON_LinetypeSegment::eSegType type)
{
  if (PatternIsLocked())
    return false;

  if( index >= 0 && index < m_segments.Count())
  {
    m_segments[index].m_length = length;
    m_segments[index].m_seg_type = type;
    return true;
  }
  else
    return false;
}

bool ON_Linetype::SetSegments(const ON_SimpleArray<ON_LinetypeSegment>& segments)
{
  if (PatternIsLocked())
    return false;

  m_segments = segments;
  return true;
}


ON_LinetypeSegment ON_Linetype::Segment( int index) const
{
  if( index >= 0 && index < m_segments.Count())
    return m_segments[index];
  else
    return ON_LinetypeSegment::OneMillimeterLine;
}





