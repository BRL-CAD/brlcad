XSLTPROC_FLAGS=-nonet -xinclude
XSL_XHTML_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/xhtml-1_1/docbook.xsl
XSL_MAN_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/manpages/docbook.xsl
XML_CATALOG_FILES=$(top_srcdir)/doc/docbook/catalog.xml



.xml.html: 
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_XHTML_STYLESHEET) $?


.xml.1:
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $?


man1DOCBOOK = \
	alias-pix.xml \
	all_sf.xml 

if BUILD_DOCBOOK
BUILTMAN_SOURCES = \
         ${man1DOCBOOK:xml=1}  
 
BUILT_SOURCES = \
         ${man1DOCBOOK:xml=html}
 
man1DOCBOOKdir = $(BRLCAD_DATA)/html/man1
 
man1DOCBOOK_DATA = \
         $(BUILT_SOURCES)
 
man_MANS = \
         $(BUILTMAN_SOURCES)
 
 
MOSTLYCLEANFILES = \
         $(BUILT_SOURCES) \
         $(BUILTMAN_SOURCES)
 
endif

EXTRA_DIST = \
	${man1DOCBOOK}

 include $(top_srcdir)/misc/Makefile.defs
