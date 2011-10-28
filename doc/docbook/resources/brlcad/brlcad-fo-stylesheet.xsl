<xsl:stylesheet
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:fo="http://www.w3.org/1999/XSL/Format"
  xmlns:d="http://docbook.org/ns/docbook"
  exclude-result-prefixes="d"
  version="1.0"
>

  <!-- the base stylesheet (resolved by this name
       in the xml catalog file) -->
  <xsl:import href="/xsl/fo/docbook.xsl"/>

  <xsl:import href="/brlcad/brlcad-fonts.xsl"/>

  <!-- common param inputs, etc. -->
  <xsl:import href="/brlcad/brlcad-common.xsl"/>

  <!-- these are used in place of stringparam inputs to xsltproc -->
  <!-- pdf bookmarks -->
  <xsl:param name="fop1.extensions">1</xsl:param>

  <xsl:param name="variablelist.as.blocks">1</xsl:param>
  <xsl:param name="insert.xref.page.number">yes</xsl:param>

  <xsl:param name="double.sided">1</xsl:param>
  <xsl:param name="header.column.widths">1 2 1</xsl:param>

  <!-- ======== PAGE LAYOUT ================== -->

  <!--
  <xsl:param name="paper.type">USletter</xsl:param>
  <xsl:param name="page.height.portrait">11.0in</xsl:param>
  <xsl:param name="page.width.portrait">8.5in</xsl:param>

  <xsl:param name="page.margin.inner">0.75in</xsl:param>
  <xsl:param name="page.margin.outer">0.50in</xsl:param>

  <xsl:param name="page.margin.top">0.17in</xsl:param>

  <xsl:param name="region.before.extent">0.40in</xsl:param>
  <xsl:param name="body.margin.top">0.50in</xsl:param>

  <xsl:param name="body.margin.bottom">0.50in</xsl:param>
  <xsl:param name="region.after.extent">0.40in</xsl:param>

  <xsl:param name="page.margin.bottom">0.50in</xsl:param>
  -->

  <!-- other customizations -->
  <!-- line breaks, DB p. 245 -->
  <xsl:template match="processing-instruction('linebreak')">
    <fo:block/>
  </xsl:template>


<xsl:attribute-set name="verbatim.properties">
  <xsl:attribute name="border">0.5pt solid blue</xsl:attribute>
  <xsl:attribute name="background-color">#E0E0E0</xsl:attribute>
  <xsl:attribute name="padding-top">0.25em</xsl:attribute>
  <xsl:attribute name="padding-bottom">0.25em</xsl:attribute>
</xsl:attribute-set>

<xsl:attribute-set name="monospace.properties">
  <xsl:attribute name="font-size">8.0pt</xsl:attribute>
</xsl:attribute-set>

<!-- center table titles, thanks to Dean Nelson <deannelson@aol.com>,
     2010-11-09, from the docbook-apps mailing list> -->
<xsl:attribute-set name="formal.title.properties">
  <xsl:attribute name="text-align">
    <xsl:choose>
      <xsl:when test="self::table[@align] and descendant::title">
        <xsl:value-of select="@align" />
      </xsl:when>
      <xsl:otherwise>center</xsl:otherwise>
    </xsl:choose>
  </xsl:attribute>
</xsl:attribute-set>

<xsl:attribute-set name="informalfigure.properties">
  <xsl:attribute name="text-align">center</xsl:attribute>
</xsl:attribute-set>
<xsl:attribute-set name="figure.properties">
  <xsl:attribute name="text-align">center</xsl:attribute>
</xsl:attribute-set>

  <!-- ==================================================================== -->
  <!-- many itemized list marks are missing -->

<xsl:template name="itemizedlist.label.markup">
  <xsl:param name="itemsymbol">bullet</xsl:param>

  <xsl:choose>
    <xsl:when test="$itemsymbol='none'"></xsl:when>
    <xsl:when test="$itemsymbol='disc'">&#x2022;</xsl:when>
    <xsl:when test="$itemsymbol='bullet'">&#x25CF;</xsl:when>
    <xsl:when test="$itemsymbol='endash'">&#x2013;</xsl:when>
    <xsl:when test="$itemsymbol='emdash'">&#x2014;</xsl:when>
    <!-- Some of these may work in your XSL-FO processor and fonts -->

    <xsl:when test="$itemsymbol='square'">&#x25A0;</xsl:when>
    <xsl:when test="$itemsymbol='box'">&#x25A0;</xsl:when>
    <xsl:when test="$itemsymbol='circle'">&#x25CB;</xsl:when>
    <xsl:when test="$itemsymbol='opencircle'">&#x25CB;</xsl:when>
    <xsl:when test="$itemsymbol='whitesquare'">&#x25A1;</xsl:when>

    <!-- these two are not in Linux Libertine:
    <xsl:when test="$itemsymbol='smallwhitesquare'">&#x25AB;</xsl:when>
    <xsl:when test="$itemsymbol='smallblacksquare'">&#x25AA;</xsl:when>
    -->
    <xsl:when test="$itemsymbol='round'">&#x25CF;</xsl:when>
    <xsl:when test="$itemsymbol='blackcircle'">&#x25CF;</xsl:when>
    <xsl:when test="$itemsymbol='smallcircle'">&#x25E6;</xsl:when>
    <xsl:when test="$itemsymbol='triangle'">&#x2023;</xsl:when>
    <xsl:when test="$itemsymbol='point'">&#x203A;</xsl:when>
    <!--
    <xsl:when test="$itemsymbol='hand'"><fo:inline
                         font-family="Wingdings 2">A</fo:inline></xsl:when>
    -->
    <xsl:otherwise>&#x25CF;</xsl:otherwise><!-- bullet -->
  </xsl:choose>
</xsl:template>

  <!-- order of choice ================================================================== -->
<xsl:template name="next.itemsymbol">
  <xsl:param name="itemsymbol" select="'default'"/>
  <xsl:choose>
    <!-- Change this list if you want to change the order of symbols -->
    <xsl:when test="$itemsymbol = 'bullet'">circle</xsl:when>
    <xsl:when test="$itemsymbol = 'circle'">square</xsl:when>
    <xsl:otherwise>bullet</xsl:otherwise>
  </xsl:choose>
</xsl:template>

  <!-- ==================================================================== -->

      <!--
  <xsl:attribute-set name="list.block.spacing">
    <xsl:attribute name="margin-left">

      <xsl:choose>
        <xsl:when test="self::itemizedlist">1em</xsl:when>
        <xsl:otherwise>1em</xsl:otherwise>
      </xsl:choose>

    </xsl:attribute>
  </xsl:attribute-set>
      -->

  <!-- following are defaults of "1.0em" each for now -->
  <xsl:param name="itemizedlist.label.width">1.0em</xsl:param>
  <xsl:param name="orderedlist.label.width">1.0em</xsl:param>

  <!-- ==================================================================== -->

  <xsl:template name="chapter.title">
    <xsl:param name="node" select="."/>

    <fo:block xsl:use-attribute-sets="chapter.label.properties"
        color='gray' font-size='30pt' >

      <xsl:apply-templates select="$node" mode="label.markup"/>

    </fo:block>

    <fo:block xsl:use-attribute-sets="chapter.title.properties">
      <xsl:apply-templates select="$node" mode="title.markup"/>
    </fo:block>

  </xsl:template>

  <xsl:attribute-set name="chapter.label.properties">
    <xsl:attribute name="keep-with-next.within-column">always</xsl:attribute>
    <xsl:attribute name="space-before.optimum">
      <xsl:value-of select="concat($body.font.master, 'pt')"/>
    </xsl:attribute>
    <xsl:attribute name="space-before.minimum">
      <xsl:value-of select="concat($body.font.master, 'pt * 0.8')"/>
    </xsl:attribute>
    <xsl:attribute name="space-before.maximum">
      <xsl:value-of select="concat($body.font.master, 'pt * 1.2')"/>
    </xsl:attribute>
    <xsl:attribute name="hyphenate">false</xsl:attribute>
    <xsl:attribute name="text-align">start</xsl:attribute>
    <xsl:attribute name="start-indent">
      <xsl:value-of select="$title.margin.left"/>
    </xsl:attribute>
  </xsl:attribute-set>

  <xsl:attribute-set name="chapter.title.properties">
    <xsl:attribute name="keep-with-next.within-column">always</xsl:attribute>
    <xsl:attribute name="space-before.optimum">
      <xsl:value-of select="concat($body.font.master, 'pt')"/>
    </xsl:attribute>
    <xsl:attribute name="space-before.minimum">
      <xsl:value-of select="concat($body.font.master, 'pt * 0.8')"/>
    </xsl:attribute>
    <xsl:attribute name="space-before.maximum">
      <xsl:value-of select="concat($body.font.master, 'pt * 1.2')"/>
    </xsl:attribute>
    <xsl:attribute name="hyphenate">false</xsl:attribute>
    <xsl:attribute name="text-align">start</xsl:attribute>
    <xsl:attribute name="start-indent">
      <xsl:value-of select="$title.margin.left"/>
    </xsl:attribute>
  </xsl:attribute-set>

  <!-- =========== toc indent from fo/autotoc.xsl ============== -->

  <!-- =========== customize 'phrase' with borders (fo only) ============== -->
  <xsl:template match="d:phrase[@role='phrase-border']">
    <fo:block
      background-color='#e0e0e0'
      border='0.5pt solid blue'
    >
      <xsl:apply-templates/>
    </fo:block>
  </xsl:template>

  <xsl:template match="d:phrase[@role='phrase-no-border']">
    <fo:block
      background-color='#e0e0e0'
    >
      <xsl:apply-templates/>
    </fo:block>
  </xsl:template>

  <xsl:template match="d:phrase[@role='smallfont']">
    <fo:inline font-size='5pt'><xsl:apply-templates/></fo:inline>
  </xsl:template>


  <!-- this will place a comment in the fo file: -->
  <!--
  <xsl:comment>
    This is Tom's comment.
  </xsl:comment>
  -->

  <!-- ====================================================== -->
  <!-- center tables in print, from p. 474 in Stayton: -->
  <xsl:include href="center-table-print.xsl"/>

  <!--
<xsl:template name="table.layout">
  <xsl:param name="table.content"/>

  <xsl:variable name="mytablewidth">
    <xsl:call-template name="dbfo-attribute">
      <xsl:with-param name="pis"
        select="ancestor-or-self::d:table/processing-instruction('dbfo')"/>
      <xsl:with-param name="attribute" select="'mytablewidth'"/>
    </xsl:call-template>
  </xsl:variable>

    <xsl:choose>
      <xsl:when test="string-length($mytablewidth) != 0">

  <fo:table width="100%" table-layout="fixed">

    <fo:table-column column-width="proportional-column-width(1)"/>
    <fo:table-column column-width="{$mytablewidth}"/>
    <fo:table-column column-width="proportional-column-width(1)"/>
    <fo:table-body start-indent="0pt">
      <fo:table-row>
        <fo:table-cell><fo:block>&#xFEFF;</fo:block></fo:table-cell>

        <fo:table-cell>

          <fo:table>
            <fo:table-body start-indent="0pt">
              <fo:table-row><fo:table-cell><fo:block>
                <xsl:copy-of select="$table.content"/>
               </fo:block></fo:table-cell></fo:table-row>
            </fo:table-body>
          </fo:table>

        </fo:table-cell>

        <fo:table-cell><fo:block>&#xFEFF;</fo:block></fo:table-cell>

      </fo:table-row>
    </fo:table-body>
  </fo:table>

  </xsl:when>
  <xsl:otherwise>
     <xsl:copy-of select="$table.content"/>
  </xsl:otherwise>
</xsl:choose>


</xsl:template>
-->

</xsl:stylesheet>
