XSLTPROC_FLAGS=-nonet -xinclude
XSL_SINGLE_XHTML_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/xhtml-1_1/docbook.xsl
XSL_CHUNKED_XHTML_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/xhtml-1_1/chunk.xsl
XSL_FO_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/fo/docbook.xsl
XML_CATALOG_FILES=$(top_srcdir)/doc/docbook/catalog.xml

SUBDIRS = \
	assigning_material_properties_and_raytracing_images \
	creating_a_goblet_images \
	creating_mug_images \
	creating_primitive_shapes_images \
	creating_toy_truck_images \
	globe_in_display_box_images \
	goblet_material_properties_images \
	goblet_material_properties2_images \
	learning_boolean_expressions_images \
	learning_viewing_options_images \
	modeling_techniques_and_structures_images \
	mug_through_gui_images \
	placing_shapes_in_3d_images \
	placing_shapes_in_3d_2_images \
	refining_mug_images



.xml.html: 
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_SINGLE_XHTML_STYLESHEET) $?

.fo.pdf: .xml.fo
	$(APACHEFOP) $? -pdf $@

.xml.fo:
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_FO_STYLESHEET) $?


lessonsDOCBOOK = \
	assigning_material_properties_and_raytracing.xml \
	creating_a_goblet.xml \
	creating_mug.xml \
	creating_primitive_shapes.xml \
	creating_toy_truck.xml \
	globe_in_display_box.xml \
	goblet_material_properties.xml \
	goblet_material_properties2.xml \
	learning_boolean_expressions.xml \
	learning_viewing_options.xml \
	modeling_techniques_and_structures.xml \
	mug_through_gui.xml \
	placing_shapes_in_3d.xml \
	placing_shapes_in_3d_2.xml \
	refining_mug.xml \
	using_insert_command.xml


BUILTHTML = ${lessonsDOCBOOK:xml=html} 


if BUILD_PDF_DOCS

BUILTPDF = ${lessonsDOCBOOK:xml=pdf}

endif

lessonsDOCBOOKhtmldir = $(BRLCAD_DATA)/html/lessons/mged
lessonsDOCBOOKhtml_DATA = $(BUILTHTML)

lessonsDOCBOOKpdfdir = $(BRLCAD_DATA)/pdf/lessons/mged
lessonsDOCBOOKpdf_DATA = $(BUILTPDF)

BUILT_SOURCES = \
	$(BUILTHTML) \
	$(BUILTPDF)

MOSTLYCLEANFILES = \
         $(BUILT_SOURCES)

EXTRA_DIST = \
	${lessonsDOCBOOK}

include $(top_srcdir)/misc/Makefile.defs
