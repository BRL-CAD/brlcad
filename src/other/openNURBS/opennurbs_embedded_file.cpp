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

ON_Buffer::ON_Buffer()
: m_buffer_size(0)
, m_current_position(0)
, m_first_segment(0)
, m_last_segment(0)
, m_current_segment(0)
, m_error_handler(0)
, m_last_error(0)
{
  memset(m_reserved,0,sizeof(m_reserved));
}


void ON_Buffer::Destroy()
{
  ChangeSize(0);
}

void ON_Buffer::EmergencyDestroy()
{
  m_buffer_size = 0;
  m_current_position = 0;
  m_first_segment = 0;
  m_last_segment = 0;
  m_current_segment = 0;
  m_error_handler = 0;
  m_last_error = 0;
}

struct ON_BUFFER_SEGMENT
{
  struct ON_BUFFER_SEGMENT* m_prev_segment;
  struct ON_BUFFER_SEGMENT* m_next_segment;
  ON__UINT64 m_segment_position0; // postion of first byte in this segment
  ON__UINT64 m_segment_position1; // position of the first byte in the next segment
                                  // When a segment is the last one in an ON_Buffer,
                                  // is is common for m_segment_position1 > m_buffer_size.
  unsigned char* m_segment_buffer; // null or an array of length (m_segment_position1 - m_segment_position0)
  void* m_reserved;
};

int ON_Buffer::Compare( const ON_Buffer& a, const ON_Buffer& b )
{
  if ( &a == &b )
    return 0;
  if ( a.m_buffer_size < b.m_buffer_size )
    return -1;
  if ( a.m_buffer_size > b.m_buffer_size )
    return 1;

  struct ON_BUFFER_SEGMENT* aseg = a.m_first_segment;
  struct ON_BUFFER_SEGMENT* bseg = b.m_first_segment;
  const ON__UINT64 buffer_size = a.m_buffer_size;
  ON__UINT64 size = 0;
  size_t aoffset = 0;
  size_t boffset = 0;
  size_t asegsize = 0;
  size_t bsegsize = 0;  
  size_t asize = 0;
  size_t bsize = 0;  
  size_t sz;
  int rc = 0;

  while ( 0 != aseg && 0 != bseg && size < buffer_size )
  {
    if ( 0 == asegsize )
    {
      if ( aseg->m_segment_position0 >= aseg->m_segment_position1 )
      {
        aseg = aseg->m_next_segment;
        continue;
      }
      asegsize = (size_t)(aseg->m_segment_position1 - aseg->m_segment_position0);
      aoffset = 0;
    }

    if ( 0 == bsegsize )
    {
      if ( bseg->m_segment_position0 >= bseg->m_segment_position1 )
      {
        bseg = bseg->m_next_segment;
        continue;
      }
      bsegsize = (size_t)(bseg->m_segment_position1 - bseg->m_segment_position0);
      boffset = 0;
    }
    
    if ( aoffset >= asegsize )
    {
      asegsize = 0;
      aseg = aseg->m_next_segment;
      continue;
    }

    if ( boffset >= bsegsize )
    {
      bsegsize = 0;
      bseg = bseg->m_next_segment;
      continue;
    }

    if ( 0 == aseg->m_segment_buffer )
    {
      return (0 == bseg->m_segment_buffer) ? 0 : -1;
    }

    if ( 0 == bseg->m_segment_buffer )
    {
      return 1;
    }

    asize = asegsize - aoffset;
    bsize = bsegsize - boffset;
    sz = (asize <= bsize) ? asize : bsize;
    if ( size + sz > buffer_size )
      sz = (size_t)(buffer_size - size);
    rc = memcmp( aseg->m_segment_buffer + aoffset, bseg->m_segment_buffer + boffset, (size_t)sz );
    if ( 0 != rc )
      return ((rc<0)?-1:1);
    aoffset += sz;
    boffset += sz;
    size += sz;
  }

  return 0;
}


ON_Buffer::~ON_Buffer()
{
  ChangeSize(0); // frees all heap and zeros everything but m_current_position.
  m_current_position = 0;
}

ON_Buffer::ON_Buffer( const ON_Buffer& src )
: m_buffer_size(0)
, m_current_position(0)
, m_first_segment(0)
, m_last_segment(0)
, m_current_segment(0)
, m_error_handler(0)
, m_last_error(0)
{
  memset(m_reserved,0,sizeof(m_reserved));
  Copy(src);
}

ON_Buffer& ON_Buffer::operator=( const ON_Buffer& src )
{
  if ( this != &src )
  {
    ChangeSize(0); // frees all heap and zeros everything but m_current_position.
    m_current_position = 0;
    Copy(src);
  }
  return *this;
}

bool ON_Buffer::Seek( ON__INT64 offset, int origin )
{
  ON__UINT64 pos0, pos1;

  switch(origin)
  {
  case 0: // Seek from beginning of start.
    pos0 = 0;
    break;

  case 1: // Seek from current position.
    pos0 = m_current_position;
    break;

  case 2: // Seek from end.
    pos0 = m_buffer_size;
    break;

  default:
    {
      ON_ERROR("Invalid origin parameter");
      return false;
    }
    break;
  }

  if ( offset < 0 )
  {
    if ( pos0 < (ON__UINT64)(-offset) )
    {
      // current position cannot be negative
      ON_ERROR("Attempt to seek before start of buffer.");
      return false; 
    }    
    pos1 = pos0 - (ON__UINT64)(-offset); // overflow cannot happen in this operation
  }
  else if ( offset > 0 )
  {
    // current position can be >= m_buffer_size
    pos1 = pos0 + (ON__UINT64)(offset); // overflow is possible in this operation
    if ( pos1 <= pos0 )
    {
      // overflow
      ON_ERROR("Attempt to seek to a position that is too large for 64-bit unsigned int storage.");
      return false; 
    }
  }
  else
  {
    pos1 = pos0;
  }

  if ( pos1 != m_current_position )
  {
    m_current_position = pos1;
    m_current_segment = 0;
  }

  return true;
}

bool ON_Buffer::SeekFromStart( ON__INT64 offset )
{
  return Seek(offset,0);
}

bool ON_Buffer::SeekFromCurrentPosition( ON__INT64 offset )
{
  return Seek(offset,1);
}

bool ON_Buffer::SeekFromEnd( ON__INT64 offset )
{
  return Seek(offset,2);
}

bool ON_Buffer::Compact()
{
  bool rc = false;
  if ( 0 == m_buffer_size )
  {
    rc = ChangeSize(0); // frees all heap and zeros everything but m_current_position.
    m_current_segment = 0;
  }
  else if ( 0 != m_last_segment 
            && m_buffer_size > m_last_segment->m_segment_position0
            && m_buffer_size <= m_last_segment->m_segment_position1
            )
  {
    if ( m_buffer_size == m_last_segment->m_segment_position1 )
      rc = true;
    else
    {
      ON__UINT64 sizeof_segment_buffer = m_buffer_size - m_last_segment->m_segment_position0;
      struct ON_BUFFER_SEGMENT* prev_segment = m_last_segment->m_prev_segment;
      void* last_buffer = ( 0 != m_last_segment->m_segment_buffer && m_last_segment->m_segment_buffer != (unsigned char*)(m_last_segment+1) )
                        ? m_last_segment->m_segment_buffer
                        : 0;
      struct ON_BUFFER_SEGMENT* new_last_segment = (struct ON_BUFFER_SEGMENT*)onrealloc(m_last_segment,sizeof(*m_last_segment) + ((size_t)sizeof_segment_buffer)); // sizeof_segment_buffer always < 0xFFFFFFFF
      if ( 0 != new_last_segment )
      {
        if ( new_last_segment != m_last_segment || 0 != last_buffer )
        {
          new_last_segment->m_segment_buffer = (unsigned char*)(new_last_segment+1);
          if ( 0 != last_buffer )
          {
            memcpy(new_last_segment->m_segment_buffer,last_buffer,(size_t)sizeof_segment_buffer);
            onfree(last_buffer);
            last_buffer = 0;
          }
          new_last_segment->m_prev_segment = prev_segment;
          new_last_segment->m_next_segment = 0;
          if ( m_first_segment == m_last_segment )
            m_first_segment = new_last_segment;
          if ( m_current_segment == m_last_segment )
            m_current_segment = new_last_segment;
          m_last_segment = new_last_segment;
          if ( 0 != prev_segment )
          {
            prev_segment->m_next_segment = m_last_segment;
          }
        }
        m_last_segment->m_segment_position1 = m_buffer_size;
        rc = true;
      }
    }
  }
  return true;
}

bool ON_Buffer::ChangeSize(ON__UINT64 buffer_size)
{
  if ( buffer_size <= 0 )
  {
    struct ON_BUFFER_SEGMENT* p0 = m_last_segment;
    struct ON_BUFFER_SEGMENT* p1 = 0;
    m_buffer_size = 0;
    m_first_segment = 0;
    m_last_segment = 0;
    m_current_segment = 0;

    // free in reverse order of allocation
    while ( 0 != p0 )
    {
      p1 = p0->m_prev_segment;
      if ( 0 != p0->m_segment_buffer && (void*)(p0->m_segment_buffer) != (void*)(p0+1) )
        onfree(p0->m_segment_buffer);
      onfree(p0);
      p0 = p1;
    }
  }
  else if ( buffer_size < m_buffer_size )
  {
    m_current_segment = 0;

    if ( 0 == m_first_segment || 0 == m_last_segment )
    {
      ON_ERROR("Corrupt ON_Buffer");
      return false;
    }

    while ( 0 != m_last_segment )
    {
      if ( m_last_segment->m_segment_position0 < buffer_size )
      {
        if ( buffer_size > m_last_segment->m_segment_position1 )
        {
          ON_ERROR("Corrupt ON_Buffer.");
          // Set m_buffer_size and m_last_segment to valid values
          // to prevent possible crashes if the return code is
          // ignored.
          if ( m_buffer_size > m_last_segment->m_segment_position1 )
            m_buffer_size = m_last_segment->m_segment_position1;
          m_last_segment->m_next_segment = 0;
          if ( m_current_position > m_buffer_size )
            m_current_position = m_buffer_size;
          return false;
        }
        if ( 0 != m_last_segment->m_segment_buffer && m_last_segment->m_segment_position1 > buffer_size )
        {
          memset(m_last_segment->m_segment_buffer + (buffer_size - m_last_segment->m_segment_position0),
                 0,
                 (size_t)(m_last_segment->m_segment_position1 - buffer_size)
                 );
        }
        m_buffer_size = buffer_size;
        break;
      }
      struct ON_BUFFER_SEGMENT* p = m_last_segment->m_prev_segment;
      if ( 0 != p )
        p->m_next_segment = 0;
      if ( 0 != m_last_segment->m_segment_buffer && (void*)(m_last_segment->m_segment_buffer) != (void*)(m_last_segment+1) )
        onfree(m_last_segment->m_segment_buffer);
      onfree(m_last_segment);
      m_last_segment = p;
    }
  }
  else if ( buffer_size > m_buffer_size )
  {
    // save current position;
    const ON__UINT64 saved_pos = CurrentPosition();
    if ( SeekFromStart(buffer_size-1) )
    {
      // calling Write with the current position at buffer_size-1
      // will pad with zeros from offset m_buffer_size to 
      // offset buffer_size-2, write a zero at offset buffer_size-1,
      // and set m_buffer_size to buffer size.
      const unsigned char zero_byte = 0;
      Write(1,&zero_byte);
    }
    // restore current position.
    SeekFromStart(saved_pos);
  }

  return (buffer_size == m_buffer_size);
}

void ON_Buffer::Copy( const ON_Buffer& src )
{
  const struct ON_BUFFER_SEGMENT* src_seg;
  struct ON_BUFFER_SEGMENT* dst_seg;
  for ( src_seg = src.m_first_segment; 0 != src_seg; src_seg = src_seg->m_next_segment )
  {
    if (    m_buffer_size != src_seg->m_segment_position0 
         || src_seg->m_segment_position0 >= src.m_buffer_size
        )
    {
      ON_ERROR("Attempt to copy corrupt source.");
      break;
    }
    if ( src_seg->m_segment_position0 >= src_seg->m_segment_position1 
        )
    {
      ON_ERROR("Attempt to copy corrupt source.");
      continue;
    }
    ON__UINT64 segment_buffer_size = ( 0 != src_seg->m_segment_buffer)
                               ? src_seg->m_segment_position1 - src_seg->m_segment_position0
                               : 0;
    dst_seg = (struct ON_BUFFER_SEGMENT*)onmalloc(sizeof(*dst_seg) + ((size_t)segment_buffer_size) );
    memset(dst_seg,0,sizeof(*dst_seg));

    if ( segment_buffer_size > 0 )
    {
      dst_seg->m_segment_buffer = (unsigned char*)(dst_seg+1);
      memcpy( dst_seg->m_segment_buffer, src_seg->m_segment_buffer, (size_t)segment_buffer_size ); // segment_buffer_size always < 0xFFFFFFFF
    }

    if ( 0 == m_first_segment )
      m_first_segment = dst_seg;
    dst_seg->m_prev_segment = m_last_segment;
    if ( 0 != m_last_segment )
      m_last_segment->m_next_segment = dst_seg;
    m_last_segment = dst_seg;
    dst_seg->m_segment_position0 = src_seg->m_segment_position0;
    dst_seg->m_segment_position1 = src_seg->m_segment_position1;
    m_buffer_size = (src.m_buffer_size < dst_seg->m_segment_position1)
                  ? src.m_buffer_size
                  : dst_seg->m_segment_position1;
  }
  if ( src.m_current_position <= m_buffer_size )
    m_current_position = src.m_current_position;
  // 27 June, 2001 Dale Lear: Should this copy m_last_error and m_error_handler? Not sure.
}

static bool ON_Buffer_IsNotValid()
{
  return false;
}

bool ON_Buffer::IsValid( const ON_TextLog* text_log ) const
{
  // This function is primarily used to discover bugs
  // in the ON_Buffer member function code.

  if ( 0 == m_buffer_size )
  {

    if ( 0 != m_first_segment )
      return ON_Buffer_IsNotValid();
    if ( 0 != m_last_segment )
      return ON_Buffer_IsNotValid();
    if ( 0 != m_current_segment )
      return ON_Buffer_IsNotValid();

    return true;
  }


  if ( 0 == m_first_segment )
    return ON_Buffer_IsNotValid();
  if ( 0 != m_first_segment->m_prev_segment )
    return ON_Buffer_IsNotValid();
  if ( 0 == m_last_segment )
    return ON_Buffer_IsNotValid();
  if ( 0 != m_last_segment->m_next_segment )
    return ON_Buffer_IsNotValid();

  bool bCurrentSegInList = (0 == m_current_segment);
  ON__UINT64 pos = 0;
  ON__UINT64 u;
  const struct ON_BUFFER_SEGMENT* prev_seg = 0;
  const struct ON_BUFFER_SEGMENT* seg;
  for ( seg = m_first_segment; seg != 0; seg = seg->m_next_segment )
  {
    if ( prev_seg != seg->m_prev_segment )
      return ON_Buffer_IsNotValid();
    if ( 0 != prev_seg && prev_seg->m_segment_position1 != seg->m_segment_position0 )
      return ON_Buffer_IsNotValid();
    if ( seg->m_segment_position1 <= seg->m_segment_position0 )
      return ON_Buffer_IsNotValid();
    if ( pos != seg->m_segment_position0 )
      return ON_Buffer_IsNotValid();

    if ( m_current_segment == seg )
      bCurrentSegInList = true;

    // pos checks prevent infinite loop when the linked list has a cycle;
    u = pos + (seg->m_segment_position1 - seg->m_segment_position0);
    if ( pos >= u )
      return ON_Buffer_IsNotValid(); // addition wrapped value
    pos = u;
    prev_seg = seg;
  }

  if ( m_last_segment != prev_seg )
    return ON_Buffer_IsNotValid();

  if ( pos < m_buffer_size )
    return ON_Buffer_IsNotValid();

  if (    m_buffer_size <= m_last_segment->m_segment_position0 
       || m_buffer_size > m_last_segment->m_segment_position1 
     )
    return ON_Buffer_IsNotValid();

  return true;
}

bool ON_Buffer::AtEnd() const
{
  return (m_current_position == m_buffer_size);
}

ON__UINT64 ON_Buffer::Size() const
{
  return m_buffer_size;
}

ON__UINT32 ON_Buffer::CRC32( ON__UINT32 current_remainder ) const
{
  ON__UINT64 size, seg_size;
  const struct ON_BUFFER_SEGMENT* prev_seg;
  const struct ON_BUFFER_SEGMENT* seg;
  const struct ON_BUFFER_SEGMENT* seg0 = 0;

  size = 0;
  for ( seg = m_first_segment; 0 != seg; seg = seg->m_next_segment )
  {
    // prev_seg is set this way so that the error handling
    // code can use continue statments for non-fatal errors.
    prev_seg = seg0;
    seg0 = seg;

    if ( seg->m_segment_position0 > seg->m_segment_position1 )
    {
      // This is really bad!  If you can determine how the corruption occurs,
      // plase make a bug report and tell Dale Lear as soon as possible.
      ON_ERROR("corrupt buffer - segment's position values are invalid.");
      continue;
    }

    if ( 0 == prev_seg )
    {
      if ( 0 != seg->m_segment_position0 )
      {
        // The first segment should have seg->m_segment_position0 = 0.
        // We'll keep going after the call to ON_ERROR.
        //
        // If you can determine how the corruption occured, please
        // make a bug report and assign it to Dale Lear.
        ON_ERROR("corrupt buffer - first segment has non-zero value for position0.");
      }
    }
    else if ( prev_seg->m_segment_position1 != seg->m_segment_position0 )
    {
      // Every segment after the first should have 
      // seg->m_segment_position0 = previous_segment->m_segment_position1.
      // We'll keep going after the call to ON_ERROR.
      //
      // If you can determine how the corruption occured, please
      // make a bug report and assign it to Dale Lear.
      ON_ERROR("corrupt buffer - previous segment's position1 !- segment's position0.");
    }

    seg_size = seg->m_segment_position1 - seg->m_segment_position0;

    if ( 0 == seg_size )
    {
      // If you can determine how the corruption occured, please
      // make a bug report and assign it to Dale Lear.
      ON_ERROR("corrupt buffer - empty segment buffer.");
      continue;
    }
    
    if ( seg_size + size > m_buffer_size )
    {
      if ( seg != m_last_segment || seg->m_next_segment )
      {
        // If you can determine how the corruption occured, please
        // make a bug report and assign it to Dale Lear.
        ON_ERROR("corrupt buffer - segments contain more bytes than m_buffer_size.");
      }
      seg_size = m_buffer_size - size;
    }

    current_remainder = ON_CRC32(current_remainder,(size_t)seg_size,seg->m_segment_buffer);
    size += seg_size;
    if ( size >= m_buffer_size )
    {
      if ( seg != m_last_segment || 0 != seg->m_next_segment || size > m_buffer_size )
      {
        // If you can determine how the corruption occured, please
        // make a bug report and assign it to Dale Lear.
        ON_ERROR("corrupt buffer - list of segments is too long.");
      }
      break;
    }
  }

  return current_remainder;
}


ON__UINT64 ON_Buffer::CurrentPosition() const
{
  return m_current_position;
}

bool ON_Buffer::SetCurrentSegment( bool bWritePending )
{
  // When ON_Buffer::Write() needs to write at least on byted, it 
  // calls ON_Buffer::SetCurrentSegment(true).
  //   In this case true is returned in all cases unless the information
  //   in the ON_Buffer class is corrupt.
  // When ON_Buffer::Read() needs to read a at least one byte, it
  // calls ON_Buffer::SetCurrentSegment(false).  
  //   In this case, true is returned when m_current_position < m_buffer_size
  //   and false is returned in all other cases.
  //
  // If seeks have occured since the last read or write, m_current_segment
  // and m_current_segment_offset may need to be updated.
  //

  if ( 0 == m_current_segment )
    m_current_segment = (m_current_position <= m_buffer_size/2) ? m_first_segment : m_last_segment;

  if ( !bWritePending && m_current_position >= m_buffer_size )
  {
    m_current_segment = 0;
    return false; // cannot read past end of buffer
  }

  if ( 0 != m_current_segment 
       && m_current_segment->m_segment_position0 <= m_current_position 
       && m_current_position < m_current_segment->m_segment_position1 
     )
  {
    // The current position is inside of m_current_segment.
    // This happens most of the time which is why this code is at the top
    // of this function.
    return true;
  }

  if ( 0 == m_first_segment )
  {
    // m_current_position can be > 0 if we are writing
    m_current_segment = 0;
    return bWritePending;
  }

  if ( 0 == m_last_segment )
  {
    m_current_segment = 0;
    ON_ERROR("Corrupt ON_Buffer");
    return false;
  }

  if ( m_current_position >= m_last_segment->m_segment_position1 )
  {
    m_current_segment = 0;
    return bWritePending;
  }

  while ( m_current_position < m_current_segment->m_segment_position0 )
  {
    m_current_segment = m_current_segment->m_prev_segment;
    if ( 0 == m_current_segment )
    {
      ON_ERROR("Corrupt ON_Buffer");
      return false;
    }
  }

  while ( m_current_position >= m_current_segment->m_segment_position1 )
  {
    m_current_segment = m_current_segment->m_next_segment;
    if ( 0 == m_current_segment )
      return bWritePending;
  }

  return true;
}

ON__UINT64 ON_Buffer::Write( ON__UINT64 size, const void* buffer )
{
  if ( 0 == size )
    return 0; // not an error condition

  if ( 0 == buffer )
  {
    ON_ERROR("size parameter > 0 and buffer parameter is null.");
    return 0;
  }

  if ( !SetCurrentSegment(true) )
  {
    ON_ERROR("Corrupt ON_Buffer");
    return 0;
  }

  // m_current_position >= m_buffer_size is ok - it is not an error condition.

  ON__UINT64 rc = 0;
  while ( size > 0 )
  {
    if ( 0 == m_current_segment )
    {
      // allocate a new segment
      const ON__UINT64 padding_size = 4*sizeof(void*); // room for os heap info
      const ON__UINT64 header_size = sizeof(*m_current_segment);
      ON__UINT64 page_size = ON_MemoryPageSize();
      if ( page_size <= 4096 )
        page_size = 4096;
      const ON__UINT64 max_malloc_size = 16*page_size;  // largest request we want to make

      ON__UINT64 malloc_size = ( 0 != m_last_segment && m_last_segment->m_segment_position1 > m_last_segment->m_segment_position0 )
                          ? padding_size + header_size + (m_last_segment->m_segment_position1 - m_last_segment->m_segment_position0)
                          : 0;
      if ( malloc_size < page_size/2 )
        malloc_size = page_size/2;      
      if ( malloc_size < max_malloc_size )
        malloc_size *= 2;
      while ( malloc_size < max_malloc_size && size > malloc_size - header_size - padding_size )
        malloc_size *= 2;

      malloc_size -= padding_size;
      // (size_t) cast is safe because malloc_size is always <= max_malloc_size = 16*page_size <  0xFFFFFFFF
      m_current_segment = (struct ON_BUFFER_SEGMENT*)onmalloc((size_t)malloc_size); 
      memset(m_current_segment,0,(size_t)malloc_size);
      m_current_segment->m_prev_segment = m_last_segment;
      m_current_segment->m_segment_buffer = (unsigned char*)(m_current_segment + 1);
      if ( 0 != m_last_segment )
      {
        m_last_segment->m_next_segment = m_current_segment;
        m_current_segment->m_segment_position0 = m_last_segment->m_segment_position1;
      }
      else
        m_first_segment = m_current_segment;
      m_last_segment = m_current_segment;
      m_current_segment->m_segment_position1 = m_current_segment->m_segment_position0 + (ON__UINT64)(malloc_size - header_size);
    }

    if (    m_current_position < m_current_segment->m_segment_position0 
         || m_current_segment->m_segment_position1 <= m_current_segment->m_segment_position0 
       )
    {
      ON_ERROR("Corrupt ON_Buffer");
      return 0;
    }

    if ( m_current_position >= m_current_segment->m_segment_position1 )
    {
      // happens when a seek puts the current position beyond the end of the buffer.
      if ( m_current_segment->m_segment_position1 > m_buffer_size )
        m_buffer_size = m_current_segment->m_segment_position1;
      m_current_segment = m_current_segment->m_next_segment;
      continue;
    }
            
    ON__UINT64 offset = m_current_position - m_current_segment->m_segment_position0;
    ON__UINT64 sz = (m_current_segment->m_segment_position1 - m_current_position);

    if ( sz > size )
      sz = size;
    memcpy( m_current_segment->m_segment_buffer + offset, buffer, (size_t)sz );
    m_current_position += sz;
    if ( m_buffer_size < m_current_position )
    {
      // wrote past the old end of the file
      m_buffer_size = m_current_position;
    }
    rc += sz;
    size -= sz;
    buffer = ((const unsigned char*)buffer) + sz;
    if ( size > 0 )
      m_current_segment = m_current_segment->m_next_segment;
  }

  return rc;
}

ON__UINT64 ON_Buffer::Read( ON__UINT64 size, void* buffer )
{
  if ( 0 == size )
  {
    // not an error condition
    return 0;
  }

  if ( 0 == buffer )
  {
    // ON_Buffer error
    ON_ERROR("size parameter > 0 and buffer parameter is null.");
    return 0;
  }

  if ( m_current_position >= m_buffer_size )
  {
    // m_current_position == m_buffer_size is a common situation
    // and is not an error condition.
    // For example, it occurs when a previous Read() read up to the
    // end of the buffer and the caller is testing the number of 
    // bytes read to detect the end of buffer condition.
    if ( m_current_position > m_buffer_size )
    {
      ON_ERROR("Read attempted when current position > buffer size.");
    }
    return 0;
  }

  if ( !SetCurrentSegment(false) )
  {
    ON_ERROR("Corrupt ON_Buffer");
    return 0;
  }

  ON__UINT64 rc = 0;
  while ( size > 0 )
  {
    if( 0 == m_current_segment || 0 == m_current_segment->m_segment_buffer )
    {
      ON_ERROR("Corrupt ON_Buffer");
      return 0;
    }

    // set pos1 to the maximum position to be read from m_current_segment.
    ON__UINT64 pos1 = (m_buffer_size < m_current_segment->m_segment_position1)
                    ? m_buffer_size
                    : m_current_segment->m_segment_position1;
    if ( m_current_position < m_current_segment->m_segment_position0 || m_current_position >= pos1 )
    {
      ON_ERROR("Corrupt ON_Buffer");
      return 0;
    }

    ON__UINT64 offset = m_current_position - m_current_segment->m_segment_position0;           
    ON__UINT64 sz = pos1 - m_current_position;

    if ( sz > size )
      sz = size;
    memcpy( buffer, m_current_segment->m_segment_buffer + offset, (size_t)sz );
    m_current_position += sz;
    rc += sz;
    size -= sz;
    buffer = ((unsigned char*)buffer) + sz;
    if ( size > 0 )
    {
      if ( m_current_position == m_buffer_size && m_current_segment == m_last_segment )
      {
        // This is a common situation that occures when the read request is for a 
        // size larger than the remaining number of bytes in the buffer. For example,
        // when repeatedly reading into a fixed size buffer until reasing the end
        // of the file. This is not an error condition.
        break;
      }
      m_current_segment = m_current_segment->m_next_segment;
    }
  }

  return rc;
}

ON__UINT32 ON_Buffer::LastError() const
{
  return m_last_error;
}

  
void ON_Buffer::ClearLastError()
{
  m_last_error = 0;
}


ON_Buffer_ErrorHandler ON_Buffer::ErrorHandler() const
{
  return m_error_handler;
}
  
void ON_Buffer::SetErrorHandler(ON_Buffer_ErrorHandler error_handler)
{
  m_error_handler = error_handler;
}

bool ON_Buffer::WriteToBinaryArchive( ON_BinaryArchive& archive ) const
{
  // The ON_Buffer::CRC32() calculation will call ON_ERROR if the segment list
  // is not perfect.  The code below that goes through the segments
  // checks for errors so that crashes are avoided, but does not make
  // additional calls to ON_ERROR.
  ON__UINT32 buffer_crc = CRC32(0);

  if ( !archive.BeginWrite3dmChunk(TCODE_OPENNURBS_BUFFER,1,0) )
    return false;

  bool rc = false;
  for(;;)
  {
    if ( !archive.WriteBigInt(m_buffer_size) )
      break;
    if ( !archive.WriteInt(buffer_crc) )
      break;

    bool buffer_rc = true;
    ON__UINT64 size = 0;
    for ( struct ON_BUFFER_SEGMENT* seg = m_first_segment; 
          0 != seg && size < m_buffer_size; 
          seg = seg->m_next_segment 
        )
    {
      if ( 0 == seg->m_segment_buffer )
        continue;
      if ( seg->m_segment_position1 <= seg->m_segment_position0 )
        continue;
      ON__UINT64 seg_size = (seg->m_segment_position1 - seg->m_segment_position0);
      if ( seg_size + size > m_buffer_size )
        seg_size = m_buffer_size - size;
      if ( !archive.WriteByte( (size_t)seg_size, seg->m_segment_buffer ) )
      {
        buffer_rc = false;
        break;
      }
      size += seg_size;
    }
    
    rc = true;
    break;
  }

  if ( !archive.EndWrite3dmChunk() )
    rc = false;

  return rc;
}


bool ON_Buffer::ReadFromBinaryArchive( ON_BinaryArchive& archive )
{
  Destroy();
  
  int major_version = 0;
  int minor_version = 0;
  if ( !archive.BeginRead3dmChunk(TCODE_OPENNURBS_BUFFER,&major_version,&minor_version) )
    return false;

  ON_3DM_BIG_CHUNK c0;
  memset(&c0,0,sizeof(c0));
  archive.GetCurrentChunk(c0);

  ON__UINT64 saved_buffer_size = 0;
  ON__UINT32 saved_buffer_crc = 0;
  bool rc = false;
  void* a = 0;
  for(;;)
  {
    if ( 1 != major_version )
      break;
    
    if ( !archive.ReadBigInt(&saved_buffer_size) )
      break;

    if ( !archive.ReadInt(&saved_buffer_crc) )
      break;

    const ON__UINT64 extra_size = 24; // =
                                      //  4  ( major version number )
                                      // +4  ( minor version number )
                                      // +8  ( 64-bit buffer_size )
                                      // +4  ( 32-bit buffer_crc )
                                      // +4  ( 32-bit chunk crc )
    if ( 0 == minor_version )
    {
      if ( c0.Length() != extra_size + saved_buffer_size )
      {
        ON_ERROR("corrupt archive");
        break;
      }
    }
    else if ( c0.Length() < extra_size + saved_buffer_size )
    {
      // later versions may add more information
      // but there still needs to be enough room
      // to store the buffer.
      ON_ERROR("corrupt archive");
      break;
    }

    if ( saved_buffer_size > 0 )
    {
      ON__UINT64 a_capacity = saved_buffer_size;
      if ( a_capacity > 16*4096 )
        a_capacity = 16*4096;
      a = onmalloc((size_t)a_capacity);
      if ( 0 == a )
        break;
      ON__UINT64 size = 0;
      bool buffer_rc = true;
      while( size < saved_buffer_size )
      {
        ON__UINT64 read_size = a_capacity;
        if ( read_size > saved_buffer_size - size )
          read_size = saved_buffer_size - size;
        if ( !archive.ReadByte((size_t)read_size,a) )
        {
          buffer_rc = false;
          break;
        }
        // add to buffer
        Write(read_size,a);
        size += read_size;
      }
    
      if ( !buffer_rc )
        break;
    }

    rc = true;
    break;
  }

  if ( 0 != a )
    onfree(a);

  if ( !archive.EndRead3dmChunk() )
    rc = false;

  if ( rc )
  {
    Compact();
    const ON__UINT32 buffer_crc = CRC32(0);
    if ( buffer_crc != saved_buffer_crc || m_buffer_size != saved_buffer_size)
    {
      // The buffer's contents have been damaged.
      ON_ERROR("The buffer contents were corrupted during, writing, storage or reading.");
    }
  }
  else
  {
    Destroy();
  }

  return rc;
}

static bool ON_Buffer_StreamCallback( void* context, ON__UINT32 size, const void* buffer )
{
  return ( size == ((ON_Buffer*)context)->Write(size,buffer) );
}

bool ON_Buffer::Compress( ON_Buffer& compressed_buffer ) const
{
  bool rc = false;
  ON_CompressStream compressor;
  ON_Buffer* out = ( this == &compressed_buffer ) ? new ON_Buffer() : &compressed_buffer;

  out->Destroy();

  for (;;)
  {
    ON__UINT64 uncompressed_size = Size();
    if ( uncompressed_size <= 0 )
      break;
    if ( !compressor.SetCallback(ON_Buffer_StreamCallback,out) )
      break;
    if ( !compressor.Begin() )
      break;

    struct ON_BUFFER_SEGMENT* prev_seg = 0;
    struct ON_BUFFER_SEGMENT* seg = 0;
    for ( seg = m_first_segment; 0 != seg; seg = seg->m_next_segment )
    {
      const ON__UINT64 pos1 = (uncompressed_size < seg->m_segment_position1)
                            ? uncompressed_size 
                            : seg->m_segment_position1;
      if ( pos1 < seg->m_segment_position0 )
        break;
      if ( prev_seg != seg->m_prev_segment )
        break;
      if ( 0 == prev_seg )
      {
        if ( 0 != seg->m_segment_position0 )
          break;
      }
      else
      {
        if ( prev_seg->m_segment_position1 != seg->m_segment_position0 )
          break;
      }
      if ( !compressor.In(pos1 - seg->m_segment_position0,seg->m_segment_buffer) )
        break;
      prev_seg = seg;
    }
    if ( 0 != seg )
      break;

    if ( !compressor.End() )
      break;

    if ( compressor.InSize() != uncompressed_size )
      break;
    if ( compressor.InCRC() != CRC32(0) )
      break;
    if ( compressor.OutSize() != out->Size() )
      break;
    if ( compressor.OutCRC() != out->CRC32(0) )
      break;

    rc = true;
    break;
  }

  if ( !rc )
  {
    out->Destroy();
    if ( this == &compressed_buffer )
      delete out;
  }
  else
  {
    out->Compact();
    out->m_current_position = 0;
    out->m_current_segment = 0;
    if ( this == &compressed_buffer )
    {
      // transfer "out" to "this"
      compressed_buffer.Destroy();
      compressed_buffer.m_buffer_size = out->m_buffer_size;
      compressed_buffer.m_current_position = out->m_current_position;
      compressed_buffer.m_first_segment = out->m_first_segment;
      compressed_buffer.m_last_segment = out->m_last_segment;
      compressed_buffer.m_current_segment = out->m_current_segment;
      compressed_buffer.m_error_handler = out->m_error_handler;
      compressed_buffer.m_last_error = out->m_last_error;
      
      out->m_first_segment = 0;
      out->m_last_segment = 0;
      out->m_current_segment = 0;
      out->m_buffer_size = 0;
      delete out;
    }
  }

  return rc;
}

bool ON_Buffer::Uncompress( ON_Buffer& uncompressed_buffer ) const
{
  bool rc = false;
  ON_UncompressStream uncompressor;
  ON_Buffer* out = ( this == &uncompressed_buffer ) ? new ON_Buffer() : &uncompressed_buffer;

  out->Destroy();

  for (;;)
  {
    ON__UINT64 compressed_size = Size();
    if ( compressed_size <= 0 )
      break;
    if ( !uncompressor.SetCallback(ON_Buffer_StreamCallback,out) )
      break;
    if ( !uncompressor.Begin() )
      break;

    struct ON_BUFFER_SEGMENT* prev_seg = 0;
    struct ON_BUFFER_SEGMENT* seg = 0;
    for ( seg = m_first_segment; 0 != seg; seg = seg->m_next_segment )
    {
      const ON__UINT64 pos1 = (compressed_size < seg->m_segment_position1)
                            ? compressed_size 
                            : seg->m_segment_position1;
      if ( pos1 < seg->m_segment_position0 )
        break;
      if ( prev_seg != seg->m_prev_segment )
        break;
      if ( 0 == prev_seg )
      {
        if ( 0 != seg->m_segment_position0 )
          break;
      }
      else
      {
        if ( prev_seg->m_segment_position1 != seg->m_segment_position0 )
          break;
      }
      if ( !uncompressor.In(pos1 - seg->m_segment_position0,seg->m_segment_buffer) )
        break;
      prev_seg = seg;
    }
    if ( 0 != seg )
      break;

    if ( !uncompressor.End() )
      break;

    if ( uncompressor.InSize() != compressed_size )
      break;
    if ( uncompressor.InCRC() != CRC32(0) )
      break;
    if ( uncompressor.OutSize() != out->Size() )
      break;
    if ( uncompressor.OutCRC() != out->CRC32(0) )
      break;

    rc = true;
    break;
  }

  if ( !rc )
  {
    out->Destroy();
    if ( this == &uncompressed_buffer )
      delete out;
  }
  else
  {
    out->Compact();
    out->m_current_position = 0;
    out->m_current_segment = 0;
    if ( this == &uncompressed_buffer )
    {
      // transfer "out" to "this"
      uncompressed_buffer.Destroy();
      uncompressed_buffer.m_buffer_size = out->m_buffer_size;
      uncompressed_buffer.m_current_position = out->m_current_position;
      uncompressed_buffer.m_first_segment = out->m_first_segment;
      uncompressed_buffer.m_last_segment = out->m_last_segment;
      uncompressed_buffer.m_current_segment = out->m_current_segment;
      uncompressed_buffer.m_error_handler = out->m_error_handler;
      uncompressed_buffer.m_last_error = out->m_last_error;
      
      out->m_first_segment = 0;
      out->m_last_segment = 0;
      out->m_current_segment = 0;
      out->m_buffer_size = 0;
      delete out;
    }
  }

  return rc;
}



