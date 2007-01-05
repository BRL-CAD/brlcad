#include "opennurbs.h"
#include "opennurbs_layer.h"

ON_OBJECT_IMPLEMENT(ON_Layer,ON_Object,"95809813-E985-11d3-BFE5-0010830122F0");

ON_Layer::ON_Layer() 
{
  Default();
}

void ON_Layer::Default()
{
  m_layer_id = ON_nil_uuid;
  m_parent_layer_id = ON_nil_uuid;
  m_layer_index = -1; // 10 March 2006 Dale Lear - changed from 0 to -1
  m_iges_level = -1; 
  m_material_index = -1; 
  m_rendering_attributes.Default();
  m_linetype_index = -1;
  m_color.SetRGB(0,0,0);
  m_display_material_id = ON_nil_uuid;
  m_plot_color = ON_UNSET_COLOR;
  m_plot_weight_mm = 0.0;
  m_name.Destroy();
  m_bVisible = true;
  m_bLocked = false;
  m_bExpanded = true;
}

ON_Layer::~ON_Layer()
{
  m_name.Destroy();
}

BOOL ON_Layer::IsValid( ON_TextLog* text_log ) const
{
  if ( m_name.IsEmpty() )
  {
    if ( text_log )
    {
      text_log->Print("Layer name is empty.\n");
    }
    return false;
  }
  return true;
}


void ON_Layer::Dump( ON_TextLog& dump ) const
{
  const wchar_t* sName = LayerName();
  if ( !sName )
    sName = L"";
  dump.Print("index = %d\n",m_layer_index);
  dump.Print("name = \"%S\"\n",sName);
  dump.Print("display = %s\n",m_bVisible?"visible":"hidden");
  dump.Print("picking = %s\n",m_bLocked?"locked":"unlocked");
  dump.Print("display color rgb = "); dump.PrintRGB(m_color); dump.Print("\n");
  dump.Print("plot color rgb = "); dump.PrintRGB(m_plot_color); dump.Print("\n");
  dump.Print("default material index = %d\n",m_material_index);
}

BOOL ON_Layer::Write(
       ON_BinaryArchive& file // serialize definition to binary archive
     ) const
{
  int i;
  bool rc = file.Write3dmChunkVersion(1,8);
  while(rc)
  {
    // Save OBSOLETE mode value so we don't break file format
    if ( m_bVisible )
      i = 0; // "normal" layer mode
    else if ( m_bLocked )
      i = 2; // "locked" layer mode
    else
      i = 1; // "hidden" layer mode

    rc = file.WriteInt( i );
    if (!rc) break;

    rc = file.WriteInt( m_layer_index );
    if (!rc) break;

    rc = file.WriteInt( m_iges_level );
    if (!rc) break;

    rc = file.WriteInt( m_material_index );
    if (!rc) break;

    // Starting with version 200312110, this value is zero.  For files written
    // with earlier versions, the number was a "model index" value that was
    // set to something >= 1, but never used.  We have to continue to 
    // read/write an integer here so that old/new versions of opennurbs can
    // read files written by new/old versions.
    i = 0;
    rc = file.WriteInt( i );
    if (!rc) break;

    rc = file.WriteColor( m_color );
    if (!rc) break;
    
    {
      // OBSOLETE LINE STYLE if ( rc ) rc = file.WriteLineStyle( LineStyle() );
      // Starting with version 200503170, this section is "officially" not used.
      // Prior to that, it was old ON_LineStyle information that has
      // never been used.
      short s = 0;
      if (rc) rc = file.WriteShort(s);    // default pattern
      if (rc) rc = file.WriteShort(s);    // default pattern index
      if (rc) rc = file.WriteDouble(0.0); // default thickness
      if (rc) rc = file.WriteDouble(1.0); // default scale
    }
    if (!rc) break;

    rc = file.WriteString( m_name );
    if (!rc) break;

    // 1.1 fields
    rc = file.WriteBool(m_bVisible);
    if (!rc) break;

    // 1.2 field
    rc = file.WriteInt( m_linetype_index);
    if (!rc) break;

    // 1.3 field - 23 March 2005 Dale Lear
    rc = file.WriteColor( m_plot_color);
    if (!rc) break;
    rc = file.WriteDouble( m_plot_weight_mm);
    if (!rc) break;

    // 1.4 field - 3 May 2005 Dale Lear 
    //           - locked and visible are independent settings
    rc = file.WriteBool( m_bLocked );
    if (!rc) break;

    // 1.5 field
    rc = file.WriteUuid( m_layer_id );
    if (!rc) break;

    // 1.6 field
    rc = file.WriteUuid( m_parent_layer_id );
    if (!rc) break;

    // 1.6 field
    rc = file.WriteBool( m_bExpanded );
    if (!rc) break;

    // 1.7 field - added 6 June 2006
    rc = m_rendering_attributes.Write(file);
    if (!rc) break;

    // 1.8 field - added 19 Sep 2006
    rc = file.WriteUuid(m_display_material_id);

    break;
  }

  return rc;
}

BOOL ON_Layer::Read(
       ON_BinaryArchive& file // restore definition from binary archive
     )
{
  int obsolete_value1 = 0; // see ON_Layer::Write
  int major_version=0;
  int minor_version=0;
  int mode = ON::normal_layer;
  Default();
  BOOL rc = file.Read3dmChunkVersion(&major_version,&minor_version);
  if ( rc && major_version == 1 ) 
  {
    // common to all 1.x formats
    if ( rc ) rc = file.ReadInt( &mode );
    if ( rc ) 
    {
      switch(mode)
      {
      case 0: // OBSOLETE ON::normal_layer
        m_bVisible = true;
        m_bLocked = false;
        break;
      case 1: // OBSOLETE ON::hidden_layer
        m_bVisible = false;
        m_bLocked = false;
        break;
      case 2: // OBSOLETE ON::locked_layer
        m_bVisible = true;
        m_bLocked = true;
        break;
      default:
        m_bVisible = true;
        m_bLocked = false;
        break;
      }
    }
    if ( rc ) rc = file.ReadInt( &m_layer_index );
    if ( rc ) rc = file.ReadInt( &m_iges_level );
    if ( rc ) rc = file.ReadInt( &m_material_index );
    if ( rc ) rc = file.ReadInt( &obsolete_value1 );
    if ( rc ) rc = file.ReadColor( m_color );

    {
      // OBSOLETE line style was never used - read and discard the next 20 bytes
      short s;
      double x;
      if (rc) file.ReadShort(&s);
      if (rc) file.ReadShort(&s);
      if (rc) file.ReadDouble(&x);
      if (rc) file.ReadDouble(&x);
    }

    if ( rc ) rc = file.ReadString( m_name );
    if ( rc && minor_version >= 1 )
    {
      rc = file.ReadBool(&m_bVisible);
      if ( rc && minor_version >= 2 )
      {
        rc = file.ReadInt( &m_linetype_index);
        if (rc && minor_version >= 3 )
        {
          // 23 March 2005 Dale Lear
          rc = file.ReadColor( m_plot_color);
          if (rc) rc = file.ReadDouble( &m_plot_weight_mm);

          if (rc && minor_version >= 4 )
          {
            rc = file.ReadBool(&m_bLocked);
            if (rc && minor_version >= 5 )
            {
              rc = file.ReadUuid(m_layer_id);
              if ( rc 
                   && minor_version >= 6 
                   && file.ArchiveOpenNURBSVersion() > 200505110
                 )
              {
                // Some files saved with opennurbs version 200505110 
                // do not contain correctly written m_parent_layer_id
                // and m_bExpanded values.
                // It is ok to default these values.
                rc = file.ReadUuid(m_parent_layer_id);
                if (rc)
                  rc = file.ReadBool(&m_bExpanded);

              }

              if ( rc && minor_version >= 7 )
              {
                // 1.7 field - added 6 June 2006
                rc = m_rendering_attributes.Read(file);

                if ( rc && minor_version >= 8 )
                {
                  // 1.8 field - added 19 Sep 2006
                  rc = file.ReadUuid(m_display_material_id);
                }
              }
            }
          }
        }
      }
    }

    if ( ON_UuidIsNil(m_layer_id) )
    {
      // old files didn't have layer ids and we need unique ones.
      ON_CreateUuid(m_layer_id);
    }
  }
  else {
    ON_ERROR("ON_Layer::Read() encountered a layer written by future code.");
    rc = false;
  }

  return rc;
}

ON::object_type ON_Layer::ObjectType() const
{
  return ON::layer_object;
}

//////////////////////////////////////////////////////////////////////
//
// Interface

bool ON_Layer::SetLayerName( const char* s )
{
  m_name = s;
  return IsValid()?true:false;
}

bool ON_Layer::SetLayerName( const wchar_t* s )
{
  m_name = s;
  return IsValid()?true:false;
}

const ON_wString& ON_Layer::LayerName() const
{
  return m_name;
}

void ON_Layer::SetColor( ON_Color c)
{
  m_color = c;
}

void ON_Layer::SetPlotColor( ON_Color c)
{
  m_plot_color = c;
}

ON_Color ON_Layer::Color() const
{
  return m_color;
}

ON_Color ON_Layer::PlotColor() const
{
  return ((m_plot_color == ON_UNSET_COLOR) ? m_color : m_plot_color);
}

bool ON_Layer::SetLinetypeIndex( int index)
{
  if( index >= -1)
  {
    m_linetype_index = index;
    return true;
  }
  return false;
}

int ON_Layer::LinetypeIndex() const
{
  return m_linetype_index;
}

//bool ON_Layer::SetMode( ON::layer_mode mode )
//{
//  // OBSOLETE - use SetVisible() and SetLocked() instead
//  m_bVisible = (mode != ON::hidden_layer);
//  m_bLocked  = (mode == ON::locked_layer);
//  return true;
//}

//ON::layer_mode ON_Layer::Mode() const
//{
//  // OBSOLETE - use IsVisible() and IsLocked() instead
//
//  // If you are looking at this code in the debugger, you need to 
//  // change your code to use ON_Layer::IsLocked() and/or
//  // ON_Layer::IsVisible().  This function is OBSOLETE
//  // and will be removed soon!
//
//  //ON_ERROR("ON_Layer::Mode() IS OBSOLETE - FIX CALLING CODE.");
//
//  if ( m_bLocked )
//  {
//    // NOTE - even if the layer is not visible, return locked.
//    return ON::locked_layer;
//  }
//
//  if ( m_bVisible )
//  {
//    return ON::normal_layer;
//  }
//
//  return ON::hidden_layer;
//}

bool ON_Layer::IsVisible() const
{
  return m_bVisible;
}

void ON_Layer::SetVisible( bool bVisible )
{
  m_bVisible = ( bVisible ? true : false );
}

void ON_Layer::SetLocked( bool bLocked )
{
  m_bLocked = ( bLocked ? true : false );
}

//bool ON_Layer::IsChangeable() const
//{
//  // OBSOLETE
//  return IsVisibleAndNotLocked();
//}

bool ON_Layer::IsLocked() const
{
  return m_bLocked;
}

bool ON_Layer::IsVisibleAndNotLocked() const
{
  return (m_bVisible && !m_bLocked);
}

bool ON_Layer::IsVisibleAndLocked() const
{
  return (m_bVisible && m_bLocked);
}


//bool ON_Layer::IsSelectable() const
//{
//  // OBSOLETE
//  return IsVisibleAndNotLocked();
//}

bool ON_Layer::SetRenderMaterialIndex( int i )
{
  m_material_index = i;
  return true;
}

int ON_Layer::RenderMaterialIndex() const
{
  return m_material_index;
}

bool ON_Layer::SetLayerIndex( int i )
{
  m_layer_index = i;
  return true;
}

int ON_Layer::LayerIndex() const
{
  return m_layer_index;
}


bool ON_Layer::SetIgesLevel( int level )
{
  m_iges_level = level;
  return true;
}

int ON_Layer::IgesLevel() const
{
  return m_iges_level;
}

double ON_Layer::PlotWeight() const
{
  return m_plot_weight_mm;
}

void ON_Layer::SetPlotWeight(double plot_weight_mm)
{
  m_plot_weight_mm = (ON_IsValid(plot_weight_mm)) 
                   ? plot_weight_mm 
                   : 0.0;
}



