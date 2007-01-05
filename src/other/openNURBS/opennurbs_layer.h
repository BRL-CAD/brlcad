#if !defined(OPENNURBS_LAYER_INC_)
#define OPENNURBS_LAYER_INC_

class ON_CLASS ON_Layer : public ON_Object
{
  ON_OBJECT_DECLARE(ON_Layer);

public:
  ON_Layer();
  ~ON_Layer();
  // C++ default copy construction and operator= work fine.
  // Do not add custom versions.

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

  void Dump( ON_TextLog& ) const; // for debugging

  BOOL Write(
         ON_BinaryArchive&  // serialize definition to binary archive
       ) const;

  BOOL Read(
         ON_BinaryArchive&  // restore definition from binary archive
       );

  ON::object_type ObjectType() const;

  ON_UUID ModelObjectId() const;

  //////////////////////////////////////////////////////////////////////
  //
  // Interface

  void Default();

  bool SetLayerName( const char* );
  bool SetLayerName( const wchar_t* );
	const ON_wString& LayerName() const;

	void SetColor( ON_Color ); // layer display color
	ON_Color Color() const;

	void SetPlotColor( ON_Color ); // plotting color
	ON_Color PlotColor() const;

	bool SetLinetypeIndex( int); // layer linetype
	int LinetypeIndex() const;

  // OBSOLETE - DO NOT USE - instead call IsLocked() and/or IsVisible()
  //__declspec(deprecated) bool SetMode( ON::layer_mode );

  // OBSOLETE - DO NOT USE - instead call IsLocked() and/or IsVisible()
  //__declspec(deprecated) ON::layer_mode Mode() const;

  /*
  Returns:
    Returns true if objects on layer are visible.
  See Also:
    ON_Layer::SetVisible
  */
  bool IsVisible() const;

  /*
  Description:
    Controls layer visibility
  Parameters:
    bVisible - [in] true to make layer visible, 
                    false to make layer invisible
  See Also:
    ON_Layer::IsVisible
  */
  void SetVisible( bool bVisible );

  /*
  Returns:
    Returns true if objects on layer are locked.
  See Also:
    ON_Layer::SetLocked
  */
  bool IsLocked() const;

  /*
  Description:
    Controls layer locked
  Parameters:
    bLocked - [in] True to lock layer
                   False to unlock layer
  See Also:
    ON_Layer::IsLocked
  */
  void SetLocked( bool bLocked );

  /*
  Returns:
    Value of (IsVisible() && !IsLocked()).
  */
  bool IsVisibleAndNotLocked() const;

  /*
  Returns:
    Value of (IsVisible() && IsLocked()).
  */
  bool IsVisibleAndLocked() const;

  // OBSOLETE - DO NOT USE 
  //__declspec(deprecated) bool IsChangeable() const; // TRUE if objects on layer can be changed (normal)


  // OBSOLETE - DO NOT USE 
  //__declspec(deprecated) bool IsSelectable() const; // TRUE if objects on layer are selectable (normal and reference)

  //////////
  // Index of render material for objects on this layer that have
  // MaterialSource() == ON::material_from_layer.
  // A material index of -1 indicates no material has been assigned
  // and the material created by the default ON_Material constructor
  // should be used.
  bool SetRenderMaterialIndex( int ); // index of layer's rendering material
  int RenderMaterialIndex() const;

  bool SetLayerIndex( int ); // index of this layer;
  int LayerIndex() const;

  bool SetIgesLevel( int ); // IGES level for this layer
  int IgesLevel() const;

  /*
  Description:
    Get the weight of the plotting pen.
  Returns:
    Thickness of the plotting pen in millimeters.
    A thickness of 0.0 indicates the "default" pen weight should be used.
  */
  double PlotWeight() const;

  /*
  Description:
    Get the weight of the plotting pen.
  Parameters:
    plot_weight_mm - [in] Set the thickness of the plotting pen in millimeters.
       Any value <= 0.0 is treated as 0.0.
  */
  void SetPlotWeight(double plot_weight_mm);


public:

  int m_layer_index;       // index of this layer
  ON_UUID m_layer_id;
  ON_UUID m_parent_layer_id; // Layers are origanized in a hierarchical 
                             // structure (like file folders).
                             // If a layer is in a parent layer, 
                             // then m_parent_layer_id is the id of 
                             // the parent layer.

  int m_iges_level;        // IGES level number if this layer was made during IGES import



  // Rendering material:
  //   If you want something simple and fast, set 
  //   m_material_index to the index of your rendering material 
  //   and ignore m_rendering_attributes.
  //   If you are developing a fancy plug-in renderer, and a user is
  //   assigning one of your fabulous rendering materials to this
  //   layer, then add rendering material information to the 
  //   m_rendering_attributes.m_materials[] array. 
  //
  // Developers:
  //   As soon as m_rendering_attributes.m_materials[] is not empty,
  //   rendering material queries slow down.  Do not populate
  //   m_rendering_attributes.m_materials[] when setting 
  //   m_material_index will take care of your needs.
  int m_material_index; 
  ON_RenderingAttributes m_rendering_attributes;
  
  int m_linetype_index;    // index of linetype
  
  // Layer display attributes.
  //   If m_display_material_id is nil, then m_color is the layer color
  //   and defaults are used for all other display attributes.
  //   If m_display_material_id is not nil, then some complicated
  //   scheme is used to decide what objects on this layer look like.
  //   In all cases, m_color is a good choice if you don't want to
  //   deal with m_display_material_id.  In Rhino, m_display_material_id
  //   is used to identify a registry entry that contains user specific
  //   display preferences.
  ON_Color m_color;
  ON_UUID m_display_material_id;

  // Layer printing (a.k.a. plotting for you old timers) attributes.
  ON_Color m_plot_color;   // plotting color
  double m_plot_weight_mm; // thickness of plot pen in mm
                           //   =0.0 means use the default width
                           //   <0.0 means don't plot (visible for screen display, but does not show on plot)
  ON_wString m_name;

  bool m_bVisible;  // If true, objects on this layer are visible.
  bool m_bLocked;   // If true, objects on this layer cannot be modified.
  bool m_bExpanded; // If true, when the layer table is displayed in
                    // a tree control then the list of child layers is
                    // shown in the control.
};


#endif

