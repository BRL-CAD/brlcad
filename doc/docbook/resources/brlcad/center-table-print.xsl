<xsl:stylesheet
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  version="1.0"

  xmlns:fo="http://www.w3.org/1999/XSL/Format"
  xmlns:d="http://docbook.org/ns/docbook"
  exclude-result-prefixes="d"
>

  <!-- ====================================================== -->
  <!--
       Center tables in print, based on the example on p. 474 in
       "DocBool XSL: The Complete Guide,", 4th Ed., by Bob Stayton.

       Version 1.0

       With changes by Tom Browder <tom.browder@gmail.com> and Dean
       Nelson <deannelson@aol.com>.

       This was tested on Linux, Ubuntu 10.04 LTS, 64-bit, with the
       following tool chain:

         xsltproc: Using libxml 20706, libxslt 10126 and libexslt 815
         Apache Fop version 1.0
         DocBook 5 style sheets, version docbook-xsl-ns-1.76.1

       Notes:

       1.  This is designed to center tables, in print only (fo),
           for tables whose width is less than the available print
           width.

       2.  This style sheet uses DocBook namespaces.

       3.  This does not seem to work with proportional columns widths.

       Known improvements needed:

         Make the choice also based on a test against the available
         print width versus the PI width.  Do not use the table wrap
         method if the PI width is greater than or equal to the width
         available.

         Could be made fancier I think by calculating the column widths
         from table itself.

       Use:  Put a processing instruction (PI) following the table
       element for a table to be centered.  The PI will define the
       nominal fixed width of the table (which I calculate as the
       sum of the column fixed widths I define in the table).  For
       example:

       <table>
         <?dbfo centered-table-width="3in" ?>
         <title>A Sample Centered Table</title>
         <tgroup cols="2">
           <colspec colwidth="1in"/>
           <colspec colwidth="2in"/>
           <thead>
             <row>
               <entry align="center"><emphasis>Key</emphasis></entry>
               <entry align="center"><emphasis>Value</emphasis></entry>
             </row>
           </thead>
           <tbody>
             <row>
               <entry>city</entry>
               <entry>Niceville</entry>
             </row>
           </tbody>
          </tgroup>
       </table>

  -->

  <xsl:template name="table.layout">
    <xsl:param name="table.content"/>

    <!--
         make our own PI, from p. 116 in Stayton:
    -->
    <xsl:variable name="centered-table-width">
      <xsl:call-template name="dbfo-attribute">
        <!-- note the DocBook namespace prefix below in the select
             attribute ("d:table"), remove the "d:" if not using
             namespaces -->
        <xsl:with-param name="pis"
          select="ancestor-or-self::d:table/processing-instruction('dbfo')"/>
        <xsl:with-param name="attribute" select="'centered-table-width'"/>
      </xsl:call-template>
    </xsl:variable>

    <!-- select table wrapping only if we meet two tests: (1) a full page width
         table is not desired and (2) a table width has been specified with the
         new PI -->
    <xsl:choose>

      <xsl:when test="(string-length(@pgwide) = 0
                      and string-length($centered-table-width) != 0)
                      or string-length(@debug) != 0
                      ">

        <!-- we wrap our table in the center of another table -->
        <!--
             calculate a new variable based on input columns widths
             (this is a place holder for improvements)
         -->
        <xsl:variable name="calc-width" select="$centered-table-width"/>

        <fo:table width="100%" table-layout="fixed">
          <fo:table-column column-width="proportional-column-width(1)"/>
          <fo:table-column column-width="{$calc-width}"/>
          <fo:table-column column-width="proportional-column-width(1)"/>
          <fo:table-body start-indent="0pt">
            <fo:table-row>

              <!-- fop complains about missing table-cell children, I
                   added Bob's zero space char from p. 261 -->
              <fo:table-cell><fo:block>&#xFEFF;</fo:block></fo:table-cell>

              <!-- we wrap our table to be centered in the center table cell -->
              <fo:table-cell>
                <fo:table>
                  <fo:table-body start-indent="0pt">
                    <fo:table-row>
                      <fo:table-cell><fo:block>
                        <xsl:copy-of select="$table.content"/>
                      </fo:block></fo:table-cell>
                    </fo:table-row>
                  </fo:table-body>
                </fo:table>
              </fo:table-cell>

              <!-- ditto space char -->
              <fo:table-cell><fo:block>&#xFEFF;</fo:block></fo:table-cell>

            </fo:table-row>
          </fo:table-body>
        </fo:table>
      </xsl:when>

      <xsl:otherwise>
        <!-- we use our table as is with no wrapping -->
        <xsl:copy-of select="$table.content"/>
      </xsl:otherwise>

    </xsl:choose>
</xsl:template>

<xsl:template name="col-width-sum">
  <!-- input parameters -->
  <xsl:param name="total-width" select="0"/>
  <xsl:param name="col-path" select="//colspec"/>

  <!-- other instructions -->
  <xsl:comment>
    comment in template "col-width-sum"
  </xsl:comment>
</xsl:template>


</xsl:stylesheet>
