XSLTPROC_FLAGS=-nonet -xinclude
XSL_XHTML_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/xhtml-1_1/docbook.xsl
XSL_MAN_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/manpages/docbook.xsl
XSL_FO_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/fo/docbook.xsl
XML_CATALOG_FILES=$(top_srcdir)/doc/docbook/catalog.xml

.xml.html: 
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_XHTML_STYLESHEET) $?

.xml.1:
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $?
	mkdir -p system/man1/en/
	mv '$(@F)' system/man1/en/

.fo.pdf: .xml.fo
	$(APACHEFOP) $? -pdf $@

.xml.fo:
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_FO_STYLESHEET) $?

.docpng.png:
	mkdir -p `dirname $@`
	cp $? $@


articlesDOCBOOK_EN = \
	articles/en/build_pattern.xml \
	articles/en/build_region.xml \
	articles/en/ebm_primitive.xml \
	articles/en/mgedrc.xml \
	articles/en/nirt.xml \
	articles/en/oed.xml \
	articles/en/pipes.xml \
	articles/en/projection_shader.xml \
	articles/en/tire.xml 

articlesDOCBOOK_EN_IMAGES = \
	articles/en/images/build_pattern_fig01.docpng \
	articles/en/images/build_pattern_fig02.docpng \
	articles/en/images/build_pattern_fig03.docpng \
	articles/en/images/build_pattern_fig04.docpng \
	articles/en/images/build_pattern_fig05.docpng \
	articles/en/images/build_pattern_fig06.docpng \
	articles/en/images/build_pattern_fig07.docpng \
	articles/en/images/build_region_fig01.docpng \
	articles/en/images/build_region_fig02.docpng \
	articles/en/images/build_region_fig03.docpng \
	articles/en/images/build_region_fig04.docpng \
	articles/en/images/ebm_primitive_fig01.docpng \
	articles/en/images/ebm_primitive_fig02.docpng \
	articles/en/images/ebm_primitive_fig03.docpng \
	articles/en/images/mgedrc_fig02.docpng \
	articles/en/images/mgedrc_fig03.docpng \
	articles/en/images/nirt_fig01.docpng \
	articles/en/images/nirt_fig02.docpng \
	articles/en/images/nirt_fig03.docpng \
	articles/en/images/nirt_fig04.docpng \
	articles/en/images/nirt_fig05.docpng \
	articles/en/images/nirt_fig06.docpng \
	articles/en/images/nirt_fig07.docpng \
	articles/en/images/nirt_fig08.docpng \
	articles/en/images/nirt_fig09.docpng \
	articles/en/images/nirt_fig10.docpng \
	articles/en/images/nirt_fig11.docpng \
	articles/en/images/nirt_fig12.docpng \
	articles/en/images/nirt_fig13.docpng \
	articles/en/images/nirt_fig14.docpng \
	articles/en/images/nirt_fig15.docpng \
	articles/en/images/nirt_fig16.docpng \
	articles/en/images/nirt_fig17.docpng \
	articles/en/images/nirt_fig18.docpng \
	articles/en/images/nirt_fig19.docpng \
	articles/en/images/nirt_fig21.docpng \
	articles/en/images/nirt_fig22.docpng \
	articles/en/images/nirt_fig23.docpng \
	articles/en/images/nirt_fig24.docpng \
	articles/en/images/nirt_fig24.docpng \
	articles/en/images/oed_fig01.docpng \
	articles/en/images/oed_fig02.docpng \
	articles/en/images/oed_fig03.docpng \
	articles/en/images/oed_fig04.docpng \
	articles/en/images/oed_fig05.docpng \
	articles/en/images/oed_fig06.docpng \
	articles/en/images/oed_fig07.docpng \
	articles/en/images/oed_fig08.docpng \
	articles/en/images/oed_fig09.docpng \
	articles/en/images/oed_fig10.docpng \
	articles/en/images/oed_fig11.docpng \
	articles/en/images/oed_fig12.docpng \
	articles/en/images/oed_fig13.docpng \
	articles/en/images/pipes_fig01.docpng \
	articles/en/images/pipes_fig02.docpng \
	articles/en/images/pipes_fig03.docpng \
	articles/en/images/pipes_fig04.docpng \
	articles/en/images/pipes_fig05.docpng \
	articles/en/images/pipes_fig06.docpng \
	articles/en/images/pipes_fig07.docpng \
	articles/en/images/pipes_fig08.docpng \
	articles/en/images/pipes_fig09.docpng \
	articles/en/images/pipes_fig10.docpng \
	articles/en/images/pipes_fig11.docpng \
	articles/en/images/pipes_fig12.docpng \
	articles/en/images/pipes_fig13.docpng \
	articles/en/images/projection_shader_fig01.docpng \
	articles/en/images/projection_shader_fig02.docpng \
	articles/en/images/projection_shader_fig03.docpng \
	articles/en/images/projection_shader_fig04.docpng \
	articles/en/images/projection_shader_fig05.docpng \
	articles/en/images/projection_shader_fig06.docpng \
	articles/en/images/projection_shader_fig07.docpng \
	articles/en/images/tire_fig01.docpng \
	articles/en/images/tire_fig02.docpng \
	articles/en/images/tire_fig03.docpng \
	articles/en/images/tire_fig04.docpng \
	articles/en/images/tire_fig05.docpng \
	articles/en/images/tire_fig06.docpng \
	articles/en/images/tire_fig07.docpng \
	articles/en/images/tire_fig08.docpng \
	articles/en/images/tire_fig09.docpng \
	articles/en/images/tire_fig10.docpng \
	articles/en/images/tire_fig11.docpng \
	articles/en/images/tire_fig12.docpng \
	articles/en/images/tire_fig13.docpng \
	articles/en/images/tire_fig14.docpng

articlesDOCBOOK_ENhtmldir = $(BRLCAD_DATA)/html/articles/en
articlesDOCBOOK_ENhtml_DATA = \
	$(BUILTARTICLES_ENHTML)

BUILTARTICLES_ENHTML = \
	${articlesDOCBOOK_EN:xml=html}

articlesDOCBOOK_ENhtmlimagesdir = $(BRLCAD_DATA)/html/articles/en/images
articlesDOCBOOK_ENhtmlimages_DATA = \
	$(BUILTARTICLES_ENIMAGES)

BUILTARTICLES_ENIMAGES = \
	${articlesDOCBOOK_EN_IMAGES:docpng=png}


if BUILD_PDF_DOCS

BUILTARTICLES_ENPDF = ${articlesDOCBOOK_EN:xml=pdf}

endif

articlesDOCBOOK_ENpdfdir = $(BRLCAD_DATA)/pdf/articles/en
articlesDOCBOOK_ENpdf_DATA = $(BUILTARTICLES_ENPDF)






lessonsDOCBOOK_EN = \
	lessons/en/mged01_creating_primitive_shapes.xml \
	lessons/en/mged02_learning_viewing_options.xml \
	lessons/en/mged03_using_insert_command.xml \
	lessons/en/mged04_assign_mat_prop_rt.xml \
	lessons/en/mged05_learning_boolean_expressions.xml \
	lessons/en/mged06_creating_a_goblet.xml \
	lessons/en/mged07_goblet_material_properties.xml \
	lessons/en/mged08_goblet_material_properties2.xml \
	lessons/en/mged09_globe_in_display_box.xml \
	lessons/en/mged10_creating_mug.xml \
	lessons/en/mged11_refining_mug.xml \
	lessons/en/mged12_mug_through_gui.xml \
	lessons/en/mged13_placing_shapes_in_3d.xml \
	lessons/en/mged14_placing_shapes_in_3d_2.xml \
	lessons/en/mged15_creating_a_toy_truck.xml \
	lessons/en/mged16_mod_tech_struct.xml 

lessonsDOCBOOK_EN_IMAGES = \
	lessons/en/images/mged01_commandwindow.docpng \
	lessons/en/images/mged01_graphicswindow.docpng \
	lessons/en/images/mged01_terminal.docpng \
	lessons/en/images/mged02_coordsys.docpng \
	lessons/en/images/mged02_defaultradio.docpng \
	lessons/en/images/mged02_flower_zooomin.docpng \
	lessons/en/images/mged02_flower_zooomout.docpng \
	lessons/en/images/mged02_multipane.docpng \
	lessons/en/images/mged02_radio_35_25.docpng \
	lessons/en/images/mged02_radio_front.docpng \
	lessons/en/images/mged02_radio_right.docpng \
	lessons/en/images/mged02_radio_top.docpng \
	lessons/en/images/mged02_truck_0_0_14.docpng \
	lessons/en/images/mged02_truck_270_-90.docpng \
	lessons/en/images/mged02_truck_270_90.docpng \
	lessons/en/images/mged02_truck_35_0.docpng \
	lessons/en/images/mged02_truck_35_25.docpng \
	lessons/en/images/mged02_truck_35_60.docpng \
	lessons/en/images/mged02_truck_35_90.docpng \
	lessons/en/images/mged02_truck_front.docpng \
	lessons/en/images/mged02_truck_left.docpng \
	lessons/en/images/mged02_truck_rear.docpng \
	lessons/en/images/mged02_truck_right.docpng \
	lessons/en/images/mged04_commandwindow.docpng \
	lessons/en/images/mged04_fb_overlay_mode.docpng \
	lessons/en/images/mged04_raytracedshapes.docpng \
	lessons/en/images/mged05_combinationeditor.docpng \
	lessons/en/images/mged05_raytraceddome.docpng \
	lessons/en/images/mged05_raytracedpart1.docpng \
	lessons/en/images/mged05_raytracedpart2.docpng \
	lessons/en/images/mged05_raytracedpart3.docpng \
	lessons/en/images/mged05_subtractionspheres.docpng \
	lessons/en/images/mged05_twoprimitivespheres.docpng \
	lessons/en/images/mged05_unionspheres.docpng \
	lessons/en/images/mged06_base_substracted_vol.docpng \
	lessons/en/images/mged06_goblet1stsphere.docpng \
	lessons/en/images/mged06_gobletbase.docpng \
	lessons/en/images/mged06_gobletbasestembasin.docpng \
	lessons/en/images/mged06_gobletstem.docpng \
	lessons/en/images/mged06_rtgoblet.docpng \
	lessons/en/images/mged06_rtgobletaz_35_el_25.docpng \
	lessons/en/images/mged07_goblet_complete_window.docpng \
	lessons/en/images/mged07_goblet_mater_complete.docpng \
	lessons/en/images/mged07_goblet_mirror.docpng \
	lessons/en/images/mged07_goblet_semitransparent.docpng \
	lessons/en/images/mged08_comb_editor_window_mater_prop.docpng \
	lessons/en/images/mged08_goblet_combeditor.docpng \
	lessons/en/images/mged08_goblet_reflectivity_1.docpng \
	lessons/en/images/mged08_goblet_reflectivity_2.docpng \
	lessons/en/images/mged08_refractive_index.docpng \
	lessons/en/images/mged08_spec_vs_diff_reflectivity.docpng \
	lessons/en/images/mged09_globe_base_box_raytraced.docpng \
	lessons/en/images/mged09_globe_base_box_wireframe.docpng \
	lessons/en/images/mged09_globe_inside_outside_box.docpng \
	lessons/en/images/mged09_globe_outside_display_box.docpng \
	lessons/en/images/mged09_globe_raytraced.docpng \
	lessons/en/images/mged09_globe_wireframe.docpng \
	lessons/en/images/mged10_mug_wireframe.docpng \
	lessons/en/images/mged11_mug_raytraced_norim.docpng \
	lessons/en/images/mged11_mug_raytraced_rim.docpng \
	lessons/en/images/mged12_cyls_frontview.docpng \
	lessons/en/images/mged12_mug_gui_finished_raytraced.docpng \
	lessons/en/images/mged12_mug_handle_frontview.docpng \
	lessons/en/images/mged12_mug_handle_topview.docpng \
	lessons/en/images/mged12_mug_rim_frontview.docpng \
	lessons/en/images/mged13_candle_comb_ed_checker.docpng \
	lessons/en/images/mged13_candle_complete_front.docpng \
	lessons/en/images/mged13_candle_design.docpng \
	lessons/en/images/mged13_candle_raytraced_overlay.docpng \
	lessons/en/images/mged13_candle_tabletop_base_candle_front.docpng \
	lessons/en/images/mged13_candle_tabletop_base_front.docpng \
	lessons/en/images/mged13_candle_tabletop_eto1_rcc_wireframe_front.docpng \
	lessons/en/images/mged13_candle_tabletop_eto1_wireframe_front.docpng \
	lessons/en/images/mged13_candle_tabletop_wireframe_front.docpng \
	lessons/en/images/mged14_candle_base_sphere_topview.docpng \
	lessons/en/images/mged14_candle_multipane.docpng \
	lessons/en/images/mged14_candle_spheres_az35el25.docpng \
	lessons/en/images/mged14_candle_spheres_frontview.docpng \
	lessons/en/images/mged14_candle_spheres_raytraced.docpng \
	lessons/en/images/mged14_candle_spheres_topview.docpng \
	lessons/en/images/mged14_primitive_editor.docpng \
	lessons/en/images/mged14_primitive_selection_dropdown.docpng \
	lessons/en/images/mged15_cameo_shader.docpng \
	lessons/en/images/mged15_prim_ed_wheel_well.docpng \
	lessons/en/images/mged15_truck_2nd_wheel_wheel_place.docpng \
	lessons/en/images/mged15_truck_ambient_lighting.docpng \
	lessons/en/images/mged15_truck_cab1s.docpng \
	lessons/en/images/mged15_truck_cab_body.docpng \
	lessons/en/images/mged15_truck_cab_cutoff.docpng \
	lessons/en/images/mged15_truck_default_lighting.docpng \
	lessons/en/images/mged15_truck_design.docpng \
	lessons/en/images/mged15_truck_multipane_1stwheel.docpng \
	lessons/en/images/mged15_truck_wheel_wells_wireframe.docpng \
	lessons/en/images/mged15_truck_wheels.docpng \
	lessons/en/images/mged16_radio_casecut_raytrace.docpng \
	lessons/en/images/mged16_radio_cut_raytrace.docpng \
	lessons/en/images/mged16_radio_cut_wireframe.docpng \
	lessons/en/images/mged16_radio_default_components_props.docpng \
	lessons/en/images/mged16_radio_new_design.docpng \
	lessons/en/images/mged16_radio_transparent.docpng \
	lessons/en/images/mged16_radio_wireframe_circ_board.docpng \
	lessons/en/images/mged16_radio_with_mater_props.docpng


lessonsDOCBOOK_ENhtmldir = $(BRLCAD_DATA)/html/lessons/en
lessonsDOCBOOK_ENhtml_DATA = \
	$(BUILTLESSONS_ENHTML)

BUILTLESSONS_ENHTML = \
	${lessonsDOCBOOK_EN:xml=html}

lessonsDOCBOOK_ENhtmlimagesdir = $(BRLCAD_DATA)/html/lessons/en/images
lessonsDOCBOOK_ENhtmlimages_DATA = \
	$(BUILTLESSONS_ENIMAGES)

BUILTLESSONS_ENIMAGES = \
	${lessonsDOCBOOK_EN_IMAGES:docpng=png}


if BUILD_PDF_DOCS

BUILTLESSONS_ENPDF = ${lessonsDOCBOOK_EN:xml=pdf}

endif

lessonsDOCBOOK_ENpdfdir = $(BRLCAD_DATA)/pdf/lessons/en
lessonsDOCBOOK_ENpdf_DATA = $(BUILTLESSONS_ENPDF)







booksDOCBOOK_EN = \
	books/en/BRL-CAD_Tutorial_Series-VolumeII.xml \
	books/en/BRL-CAD_Tutorial_Series-VolumeIII.xml

booksDOCBOOK_EN_IMAGES = \
	books/en/images/tutorial_series_volIII_fig01.docpng \
 	books/en/images/tutorial_series_volIII_fig02.docpng \
 	books/en/images/tutorial_series_volIII_fig03.docpng \
 	books/en/images/tutorial_series_volIII_fig04.docpng \
 	books/en/images/tutorial_series_volIII_fig05.docpng \
 	books/en/images/tutorial_series_volIII_fig06.docpng \
 	books/en/images/tutorial_series_volIII_fig07.docpng \
 	books/en/images/tutorial_series_volIII_fig08.docpng \
 	books/en/images/tutorial_series_volIII_fig09.docpng \
 	books/en/images/tutorial_series_volIII_fig10.docpng \
 	books/en/images/tutorial_series_volIII_fig11.docpng \
 	books/en/images/tutorial_series_volIII_fig12.docpng \
 	books/en/images/tutorial_series_volIII_fig13.docpng \
 	books/en/images/tutorial_series_volIII_fig14.docpng \
 	books/en/images/tutorial_series_volIII_fig15.docpng \
 	books/en/images/tutorial_series_volIII_fig16.docpng \
 	books/en/images/tutorial_series_volIII_fig17.docpng \
 	books/en/images/tutorial_series_volIII_fig18.docpng \
 	books/en/images/tutorial_series_volIII_fig19.docpng \
 	books/en/images/tutorial_series_volIII_fig20.docpng \
 	books/en/images/tutorial_series_volIII_fig21.docpng \
 	books/en/images/tutorial_series_volIII_fig22.docpng \
 	books/en/images/tutorial_series_volIII_fig23.docpng \
 	books/en/images/tutorial_series_volIII_fig24.docpng \
 	books/en/images/tutorial_series_volIII_fig25.docpng \
 	books/en/images/tutorial_series_volIII_fig26.docpng \
 	books/en/images/tutorial_series_volIII_fig27.docpng \
 	books/en/images/tutorial_series_volIII_fig28.docpng \
 	books/en/images/tutorial_series_volIII_fig29.docpng

booksDOCBOOK_ENhtmldir = $(BRLCAD_DATA)/html/books/en
booksDOCBOOK_ENhtml_DATA = \
	$(BUILTBOOKS_ENHTML)

BUILTBOOKS_ENHTML = \
	${booksDOCBOOK_EN:xml=html}

booksDOCBOOK_ENhtmlimagesdir = $(BRLCAD_DATA)/html/books/en/images
booksDOCBOOK_ENhtmlimages_DATA = \
	$(BUILTBOOKS_ENIMAGES)

BUILTBOOKS_ENIMAGES = \
	${booksDOCBOOK_EN_IMAGES:docpng=png}


if BUILD_PDF_DOCS

BUILTBOOKS_ENPDF = ${booksDOCBOOK_EN:xml=pdf}

endif

booksDOCBOOK_ENpdfdir = $(BRLCAD_DATA)/pdf/books/en
booksDOCBOOK_ENpdf_DATA = $(BUILTBOOKS_ENPDF)




specificationsDOCBOOK_EN = \
	specifications/en/BRL_CAD_g_format_V5.xml

specificationsDOCBOOK_EN_IMAGES = 

specificationsDOCBOOK_ENhtmldir = $(BRLCAD_DATA)/html/specifications/en
specificationsDOCBOOK_ENhtml_DATA = \
	$(BUILTSPECS_ENHTML)

BUILTSPECS_ENHTML = \
	${specificationsDOCBOOK_EN:xml=html}

specificationsDOCBOOK_ENhtmlimagesdir = $(BRLCAD_DATA)/html/specifications/en/images
specificationsDOCBOOK_ENhtmlimages_DATA = \
	$(BUILTSPECS_ENIMAGES)

BUILTSPECS_ENIMAGES = \
	${specificationsDOCBOOK_EN_IMAGES:docpng=png}


if BUILD_PDF_DOCS

BUILTSPECS_ENPDF = ${specificationsDOCBOOK_EN:xml=pdf}

endif

specificationsDOCBOOK_ENpdfdir = $(BRLCAD_DATA)/pdf/specifications/en
specificationsDOCBOOK_ENpdf_DATA = $(BUILTSPECS_ENPDF)







man1DOCBOOK_EN = \
	system/man1/en/3ptarb.xml \
	system/man1/en/B.xml \
	system/man1/en/E.xml \
	system/man1/en/M.xml \
	system/man1/en/Z.xml \
	system/man1/en/adc.xml \
	system/man1/en/ae.xml \
	system/man1/en/alias-pix.xml \
	system/man1/en/all_sf.xml \
	system/man1/en/analyze.xml \
	system/man1/en/anim_cascade.xml \
	system/man1/en/anim_fly.xml \
	system/man1/en/anim_hardtrack.xml \
	system/man1/en/anim_keyread.xml \
	system/man1/en/anim_lookat.xml \
	system/man1/en/anim_offset.xml \
	system/man1/en/anim_orient.xml \
	system/man1/en/animmate.xml \
	system/man1/en/apropos.xml \
	system/man1/en/aproposdevel.xml \
	system/man1/en/aproposlib.xml \
	system/man1/en/arb.xml \
	system/man1/en/arced.xml \
	system/man1/en/area.xml \
	system/man1/en/arot.xml \
	system/man1/en/attach.xml \
	system/man1/en/attr.xml \
	system/man1/en/autoview.xml \
	system/man1/en/bb.xml \
	system/man1/en/bev.xml \
	system/man1/en/bo.xml \
	system/man1/en/bolt.xml \
	system/man1/en/bot_condense.xml \
	system/man1/en/bot_decimate.xml \
	system/man1/en/bot_face_fuse.xml \
	system/man1/en/bot_face_sort.xml \
	system/man1/en/bot_vertex_fuse.xml \
	system/man1/en/build_region.xml \
	system/man1/en/bwscale.xml \
	system/man1/en/c.xml \
	system/man1/en/cat.xml \
	system/man1/en/center.xml \
	system/man1/en/coil.xml \
	system/man1/en/color.xml \
	system/man1/en/comb_color.xml \
	system/man1/en/copyeval.xml \
	system/man1/en/copymat.xml \
	system/man1/en/cp.xml \
	system/man1/en/cpi.xml \
	system/man1/en/d.xml \
	system/man1/en/dall.xml \
	system/man1/en/db.xml \
	system/man1/en/db_glob.xml \
	system/man1/en/dbconcat.xml \
	system/man1/en/debugbu.xml \
	system/man1/en/debugdir.xml \
	system/man1/en/debuglib.xml \
	system/man1/en/debugmem.xml \
	system/man1/en/debugnmg.xml \
	system/man1/en/decompose.xml \
	system/man1/en/delay.xml \
	system/man1/en/dm.xml \
	system/man1/en/gastank.xml \
	system/man1/en/gqa.xml \
	system/man1/en/handle.xml \
	system/man1/en/mged.xml \
	system/man1/en/nirt.xml \
	system/man1/en/picket_fence.xml \
	system/man1/en/rt.xml \
	system/man1/en/rtarea.xml \
	system/man1/en/rtcheck.xml \
	system/man1/en/rtedge.xml \
	system/man1/en/search.xml \
	system/man1/en/tire.xml \
	system/man1/en/window.xml \
	system/man1/en/window_frame.xml \
	system/man1/en/wire.xml

man1INTRO_EN = \
	system/man1/en/Introduction.xml

man1DOCBOOK_ENhtmldir = $(BRLCAD_DATA)/html/man1/en
man1DOCBOOK_ENhtml_DATA = $(BUILTMAN_ENHTML)

BUILTMAN_ENHTML = \
	${man1DOCBOOK_EN:xml=html} \
	${man1INTRO_EN:xml=html} 

if BUILD_PDF_DOCS

BUILTMAN_ENPDF = ${man1DOCBOOK_EN:xml=pdf}

endif

man1DOCBOOK_ENpdfdir = $(BRLCAD_DATA)/pdf/man1/en
man1DOCBOOK_ENpdf_DATA = $(BUILTMAN_ENPDF)





BUILT_SOURCES = \
	$(BUILTARTICLES_ENIMAGES) \
	$(BUILTARTICLES_ENHTML) \
	$(BUILTARTICLES_ENPDF) \
	$(BUILTLESSONS_ENIMAGES) \
	$(BUILTLESSONS_ENHTML) \
	$(BUILTLESSONS_ENPDF) \
	$(BUILTBOOKS_ENIMAGES) \
	$(BUILTBOOKS_ENHTML) \
	$(BUILTBOOKS_ENPDF) \
	$(BUILTSPECS_ENIMAGES) \
	$(BUILTSPECS_ENHTML) \
	$(BUILTSPECS_ENPDF) \
	$(BUILTMAN_ENHTML) \
	$(BUILTMAN_ENPDF)

BUILTMAN_EN_SOURCES = \
	${man1DOCBOOK_EN:xml=1}  

man_MANS = \
         $(BUILTMAN_EN_SOURCES)

MOSTLYCLEANFILES = \
         $(BUILT_SOURCES) \
         $(BUILTMAN_EN_SOURCES)

clean-generic: mostlyclean-generic
	-rmdir -p articles/en/images
	-rmdir -p books/en/images
	-rmdir -p lessons/en/images
	-rmdir -p system/man1/en

EXTRA_DIST = \
	${articlesDOCBOOK_EN} \
	${articlesDOCBOOK_EN_IMAGES} \
	${man1DOCBOOK_EN} \
	${man1INTRO_EN} \
	README \
	books/README \
	catalog.xml \
	resources \
	system/man1/mged_cmd_template.xml \
	articles/en/images/tire_fig12.svg \
	articles/en/images/tire_fig13.svg

include $(top_srcdir)/misc/Makefile.defs
