<xsl:stylesheet
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:fo="http://www.w3.org/1999/XSL/Format"
  xmlns:d="http://docbook.org/ns/docbook"
  exclude-result-prefixes="d"
  version='1.0'
>

  <!-- a stylesheet with common items for all products -->
  <!-- these are used in place of stringparam inputs to xsltproc -->
  <xsl:param name="part.autolabel">I</xsl:param>
  <xsl:param name="generate.section.toc.level">0</xsl:param>
  <xsl:param name="toc.max.depth">2</xsl:param>
  <xsl:param name="chapter.autolabel">1</xsl:param>
  <xsl:param name="appendix.autolabel">A</xsl:param>
  <xsl:param name="section.autolabel">1</xsl:param>
  <xsl:param name="section.label.includes.component.label">1</xsl:param>
  <xsl:param name="section.autolabel.maxdepth">2</xsl:param>

  <xsl:param name="draft.mode">no</xsl:param>
  <xsl:param name="draft.watermark.image"></xsl:param>

  <!-- do not use a unit, it is assumed to be "pt": -->
  <xsl:param name="toc.indent.width">10</xsl:param>
  <xsl:param name="part.autolabel">0</xsl:param>

  <xsl:param name="default.table.width">100%</xsl:param>

  <!-- these may not work: -->
  <!--
  <xsl:param name="use.extensions">1</xsl:param>
  <xsl:param name="tablecolumns.extension">1</xsl:param>
  -->

  <!-- control what parts of a book have a TOC: we want main book only -->
  <xsl:param name="generate.toc">appendix nop
                                 book     toc,title,figure,table,example,equation
  </xsl:param>
  <!--                               book     toc,title,figure,table,example,equation -->

  <!-- control placing of float titles only -->
  <xsl:param name="formal.title.placement">
    figure after
    equation after

    example before
    table before
    procedure before
    listing before
  </xsl:param>

  <!--
  <xsl:param name="admon.graphics">1</xsl:param>
  -->

  <!-- =========== customize 'phrase' ============== -->
  <xsl:template match="d:phrase[@role='tag']">
    <fo:inline font-weight='bold'>&lt;<xsl:apply-templates/>&gt;</fo:inline>
  </xsl:template>

  <xsl:template match="d:phrase[@role='class']">
    <fo:inline font-weight='bold'><xsl:apply-templates/></fo:inline>
  </xsl:template>

  <xsl:template match="d:phrase[@role='smallfont']">
    <fo:inline font-size='5pt'><xsl:apply-templates/></fo:inline>
  </xsl:template>

  <xsl:template match="d:phrase[@role='ko']">
    <fo:inline font-family='ko-Bandal'><xsl:apply-templates/></fo:inline>
  </xsl:template>

  <!-- =========== customize qandaset ============== -->
  <xsl:param name="qandadiv.autolabel">1</xsl:param>
  <xsl:template match="question" mode="label.markup">
    <xsl:text>Q </xsl:text>
    <xsl:number level="any" count="qandaentry" format="1"/>
  </xsl:template>

  <!-- =========== customize 'qandaentry' ============== -->
  <!-- from common/common.xsl -->
  <!-- <xsl:template match="qandaentry" mode="number"> -->
  <xsl:template match="qandaentry" mode="label.markup">
    <xsl:choose>
      <xsl:when test="ancestor::qandadiv">
        <xsl:number level="single" from="qandadiv" format="1.1"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:number level="single" from="qandaset" format="1"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

</xsl:stylesheet>
