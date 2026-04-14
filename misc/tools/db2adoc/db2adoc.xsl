<?xml version="1.0" encoding="UTF-8"?>
<!--
  db2adoc.xsl - DocBook 5 to AsciiDoc converter
  BRL-CAD

  Converts BRL-CAD DocBook 5 documentation to AsciiDoc format
  suitable for processing by asciiquack.

  Supported root elements:
    refentry  -> doctype: manpage
    article   -> doctype: article
    book      -> doctype: book

  Usage with xsltproc:
    xsltproc db2adoc.xsl input.xml > output.adoc
-->
<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:db="http://docbook.org/ns/docbook"
  xmlns:xl="http://www.w3.org/1999/xlink"
  exclude-result-prefixes="db xl">

  <xsl:output method="text" encoding="UTF-8" indent="no"/>
  <xsl:strip-space elements="*"/>
  <!-- preserve-space for elements with mixed content (inline text + elements):
       Adding db:para and its common inline containers ensures that inter-element
       whitespace (e.g. the space between <command>foo</command> <filename>bar</filename>)
       is not stripped before our text() template can handle it. -->
  <xsl:preserve-space elements="db:programlisting db:screen db:literallayout db:synopsis db:address
                                 db:para db:simpara db:title db:term db:phrase
                                 db:emphasis db:command db:option db:filename db:varname
                                 db:envar db:systemitem db:literal db:code db:constant
                                 db:type db:markup db:application db:function db:parameter
                                 db:replaceable db:userinput db:computeroutput db:prompt
                                 db:entry db:refpurpose db:citetitle db:link"/>

  <!-- ============================================================
       PARAMETERS
       ============================================================ -->
  <!-- Base section level offset (0 = start at ==) -->
  <xsl:param name="section-depth" select="0"/>

  <!-- ============================================================
       UTILITY TEMPLATES
       ============================================================ -->

  <!-- Emit N equal-signs for a section heading -->
  <xsl:template name="section-mark">
    <xsl:param name="level" select="1"/>
    <xsl:text>=</xsl:text>
    <xsl:if test="$level > 1">
      <xsl:call-template name="section-mark">
        <xsl:with-param name="level" select="$level - 1"/>
      </xsl:call-template>
    </xsl:if>
  </xsl:template>

  <!-- Count ancestor sections to determine heading level -->
  <xsl:template name="heading-level">
    <xsl:param name="node" select="."/>
    <!-- Each section/chapter/part nesting adds one level -->
    <xsl:value-of select="count($node/ancestor::db:section)
      + count($node/ancestor::db:refsection)
      + count($node/ancestor::db:refsect1)
      + count($node/ancestor::db:refsect2)
      + count($node/ancestor::db:chapter)
      + count($node/ancestor::db:appendix)
      + count($node/ancestor::db:preface)
      + count($node/ancestor::db:part)
      + 2"/>
  </xsl:template>

  <!-- Normalize inline whitespace: collapse runs of whitespace to single space -->
  <xsl:template name="normalize-text">
    <xsl:param name="text" select="."/>
    <xsl:value-of select="normalize-space($text)"/>
  </xsl:template>

  <!-- Emit a single space when the current element's text content has a trailing
       whitespace character (e.g. <option>-w </option>or).  normalize-space() inside
       the text() template strips that space; this call restores it after the closing
       inline marker so adjacent text does not fuse with the markup. -->
  <xsl:template name="inline-trailing-space">
    <xsl:variable name="raw" select="string(.)"/>
    <xsl:if test="string-length($raw) > 0 and following-sibling::node()">
      <xsl:variable name="last-char" select="substring($raw, string-length($raw), 1)"/>
      <xsl:choose>
        <!-- Case 1: element content ends with whitespace (space stripped by normalize-space).
             Only add a space when the following sibling text does NOT itself start with
             whitespace – otherwise we would produce a double space (e.g. "_-A_  option"). -->
        <xsl:when test="translate($last-char, ' &#9;&#10;&#13;', '') = ''">
          <xsl:variable name="next-str" select="string(following-sibling::node()[1])"/>
          <xsl:variable name="next-first" select="substring($next-str, 1, 1)"/>
          <xsl:if test="translate($next-first, ' &#9;&#10;&#13;', '') != ''">
            <xsl:text> </xsl:text>
          </xsl:if>
        </xsl:when>
        <!-- Case 2: element content ends with non-whitespace, but the immediately
             following sibling node starts with a WORD character (a-z, A-Z, 0-9, _).
             AsciiDoc constrained inline markers (like _x_ or *y*) require the closing
             marker to be followed by a non-word character; if the next character is a
             word char, AsciiDoc will not close the marker. Insert a space to create a
             word boundary so that the constrained marker is parsed correctly.
             E.g.: <emphasis>g</emphasis>creates → "_g_ creates" (italic g, then creates).
             Note: only triggers on word chars, NOT on punctuation like "(1)", ",", ")" etc. -->
        <xsl:otherwise>
          <xsl:variable name="next-str" select="string(following-sibling::node()[1])"/>
          <xsl:if test="string-length($next-str) > 0">
            <xsl:variable name="next-first" select="substring($next-str, 1, 1)"/>
            <!-- Only add space when next char is alphanumeric or underscore (a word char) -->
            <xsl:if test="contains('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_', $next-first)">
              <xsl:text> </xsl:text>
            </xsl:if>
          </xsl:if>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:if>
  </xsl:template>

  <!-- Emit a single space BEFORE the opening inline marker when the element's
       text content has a leading whitespace character and there is a preceding
       sibling node (e.g. <emphasis> MGED</emphasis> where the space before MGED
       provides the separator between "editor," and the italic word).
       normalize-space() inside the text() template strips that leading space;
       this call restores it before the opening marker. -->
  <xsl:template name="inline-leading-space">
    <xsl:variable name="raw" select="string(.)"/>
    <xsl:if test="string-length($raw) > 0 and preceding-sibling::node()">
      <xsl:variable name="first-char" select="substring($raw, 1, 1)"/>
      <xsl:choose>
        <!-- Case 1: element content starts with whitespace.
             Only add a space when the preceding sibling does NOT already end with
             whitespace – otherwise we produce a double space (e.g. "simple  __x__"). -->
        <xsl:when test="translate($first-char, ' &#9;&#10;&#13;', '') = ''">
          <xsl:variable name="prev-str" select="string(preceding-sibling::node()[1])"/>
          <xsl:variable name="prev-len" select="string-length($prev-str)"/>
          <xsl:variable name="prev-last" select="substring($prev-str, $prev-len, 1)"/>
          <xsl:if test="$prev-len = 0 or translate($prev-last, ' &#9;&#10;&#13;', '') != ''">
            <xsl:text> </xsl:text>
          </xsl:if>
        </xsl:when>
        <!-- Case 2: element content starts with a non-whitespace word char,
             but the immediately preceding sibling is a TEXT NODE that ends without
             whitespace (not alphanumeric boundary-check needed here since AsciiDoc
             constrained markers only need a word boundary).
             Insert a space so that AsciiDoc constrained inline markers
             (like _word_ or *word*) have a proper word boundary.
             E.g.: "the<emphasis>args</emphasis>" → "the _args_"
             Note: we only handle text-node predecessors here to avoid double spaces.
             Adjacent element → element spacing is handled by inline-trailing-space
             of the preceding element (e.g. <emphasis>foo</emphasis><command>bar</command>). -->
        <xsl:otherwise>
          <xsl:if test="preceding-sibling::node()[1][self::text()]">
            <xsl:variable name="prev-str" select="string(preceding-sibling::node()[1])"/>
            <xsl:variable name="prev-len" select="string-length($prev-str)"/>
            <xsl:if test="$prev-len > 0">
              <xsl:variable name="prev-last" select="substring($prev-str, $prev-len, 1)"/>
              <xsl:if test="contains('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_', $prev-last)">
                <xsl:text> </xsl:text>
              </xsl:if>
            </xsl:if>
          </xsl:if>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:if>
  </xsl:template>

  <!-- Escape AsciiDoc special characters in plain text context -->
  <xsl:template name="escape-adoc">
    <xsl:param name="text"/>
    <!-- For now, pass text through - most docbook text is safe in adoc -->
    <xsl:value-of select="$text"/>
  </xsl:template>

  <!-- Repeat a string N times -->
  <xsl:template name="repeat-string">
    <xsl:param name="str"/>
    <xsl:param name="n" select="1"/>
    <xsl:if test="$n > 0">
      <xsl:value-of select="$str"/>
      <xsl:call-template name="repeat-string">
        <xsl:with-param name="str" select="$str"/>
        <xsl:with-param name="n" select="$n - 1"/>
      </xsl:call-template>
    </xsl:if>
  </xsl:template>

  <!-- List item bullet prefix for given depth (0-based) -->
  <xsl:template name="list-bullet">
    <xsl:param name="depth" select="0"/>
    <xsl:call-template name="repeat-string">
      <xsl:with-param name="str" select="'*'"/>
      <xsl:with-param name="n" select="$depth + 1"/>
    </xsl:call-template>
  </xsl:template>

  <!-- List item number prefix for given depth (0-based) -->
  <xsl:template name="list-number">
    <xsl:param name="depth" select="0"/>
    <xsl:call-template name="repeat-string">
      <xsl:with-param name="str" select="'.'"/>
      <xsl:with-param name="n" select="$depth + 1"/>
    </xsl:call-template>
  </xsl:template>

  <!-- ============================================================
       BLOCK-IN-PARA SEPARATOR
       When a block-level element (table, figure, listing, list,
       admonition, etc.) is a direct child of db:para or db:simpara,
       a blank line must be emitted first so that any preceding inline
       text is closed as a proper AsciiDoc paragraph before the block
       attributes start.  Call this at the very start of every template
       that produces a block-level AsciiDoc construct.
       ============================================================ -->

  <xsl:template name="block-sep">
    <!-- If we're inside a para/simpara, ensure a blank line separates us
         from any preceding inline content on the same line. -->
    <xsl:if test="parent::db:para or parent::db:simpara">
      <xsl:text>&#10;&#10;</xsl:text>
    </xsl:if>
  </xsl:template>

  <!-- ============================================================
       ROOT ELEMENT DISPATCH
       ============================================================ -->

  <!-- Man page (refentry) -->
  <xsl:template match="/db:refentry">
    <xsl:call-template name="manpage-header"/>
    <xsl:apply-templates select="db:refnamediv"/>
    <xsl:apply-templates select="db:refsynopsisdiv"/>
    <xsl:apply-templates select="db:refsection | db:refsect1"/>
  </xsl:template>

  <!-- Article -->
  <xsl:template match="/db:article">
    <xsl:call-template name="article-header"/>
    <xsl:apply-templates/>
  </xsl:template>

  <!-- Book -->
  <xsl:template match="/db:book">
    <xsl:call-template name="book-header"/>
    <xsl:apply-templates/>
  </xsl:template>

  <!-- Bare refentry nested inside book/article -->
  <xsl:template match="db:refentry">
    <xsl:call-template name="manpage-header"/>
    <xsl:apply-templates select="db:refnamediv"/>
    <xsl:apply-templates select="db:refsynopsisdiv"/>
    <xsl:apply-templates select="db:refsection | db:refsect1"/>
  </xsl:template>

  <!-- ============================================================
       DOCUMENT HEADERS
       ============================================================ -->

  <xsl:template name="manpage-header">
    <!-- = COMMAND(SECTION)
         :doctype: manpage
         :manmanual: ...
         :mansource: BRL-CAD
    -->
    <xsl:variable name="title">
      <xsl:value-of select="normalize-space(db:refmeta/db:refentrytitle)"/>
    </xsl:variable>
    <xsl:variable name="section">
      <xsl:value-of select="normalize-space(db:refmeta/db:manvolnum)"/>
    </xsl:variable>
    <xsl:variable name="manual">
      <xsl:value-of select="normalize-space(db:refmeta/db:refmiscinfo[@class='manual'])"/>
    </xsl:variable>
    <xsl:variable name="source">
      <xsl:choose>
        <xsl:when test="db:refmeta/db:refmiscinfo[@class='source']">
          <xsl:value-of select="normalize-space(db:refmeta/db:refmiscinfo[@class='source'])"/>
        </xsl:when>
        <xsl:otherwise>BRL-CAD</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <!-- Title line -->
    <xsl:text>= </xsl:text>
    <xsl:value-of select="$title"/>
    <xsl:text>(</xsl:text>
    <xsl:value-of select="$section"/>
    <xsl:text>)</xsl:text>
    <xsl:text>&#10;</xsl:text>
    <!-- doctype -->
    <xsl:text>:doctype: manpage&#10;</xsl:text>
    <!-- mansource -->
    <xsl:if test="$source != ''">
      <xsl:text>:mansource: </xsl:text>
      <xsl:value-of select="$source"/>
      <xsl:text>&#10;</xsl:text>
    </xsl:if>
    <!-- manmanual -->
    <xsl:if test="$manual != ''">
      <xsl:text>:manmanual: </xsl:text>
      <xsl:value-of select="$manual"/>
      <xsl:text>&#10;</xsl:text>
    </xsl:if>
    <xsl:text>&#10;</xsl:text>
  </xsl:template>

  <xsl:template name="article-header">
    <xsl:variable name="title">
      <xsl:choose>
        <xsl:when test="db:info/db:title">
          <xsl:value-of select="normalize-space(db:info/db:title)"/>
        </xsl:when>
        <xsl:when test="db:title">
          <xsl:value-of select="normalize-space(db:title)"/>
        </xsl:when>
        <xsl:otherwise>Untitled</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:text>= </xsl:text>
    <xsl:value-of select="$title"/>
    <xsl:text>&#10;</xsl:text>
    <xsl:call-template name="emit-authors">
      <xsl:with-param name="info" select="db:info"/>
    </xsl:call-template>
    <xsl:text>:doctype: article&#10;</xsl:text>
    <xsl:text>:toc:&#10;</xsl:text>
    <xsl:text>:toclevels: 3&#10;</xsl:text>
    <xsl:text>&#10;</xsl:text>
  </xsl:template>

  <xsl:template name="book-header">
    <xsl:variable name="title">
      <xsl:choose>
        <xsl:when test="db:info/db:title">
          <xsl:value-of select="normalize-space(db:info/db:title)"/>
        </xsl:when>
        <xsl:when test="db:title">
          <xsl:value-of select="normalize-space(db:title)"/>
        </xsl:when>
        <xsl:otherwise>Untitled</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:text>= </xsl:text>
    <xsl:value-of select="$title"/>
    <xsl:text>&#10;</xsl:text>
    <xsl:call-template name="emit-authors">
      <xsl:with-param name="info" select="db:info"/>
    </xsl:call-template>
    <xsl:text>:doctype: book&#10;</xsl:text>
    <xsl:text>:toc:&#10;</xsl:text>
    <xsl:text>:toclevels: 3&#10;</xsl:text>
    <xsl:text>&#10;</xsl:text>
  </xsl:template>

  <!-- Emit author line(s) from info block -->
  <!-- In AsciiDoc, multiple authors must appear on a single line separated
       by "; ".  Emit them that way so the parser can recognise all authors. -->
  <xsl:template name="emit-authors">
    <xsl:param name="info"/>
    <xsl:variable name="authors" select="$info/db:author | $info/db:authorgroup/db:author"/>
    <xsl:for-each select="$authors">
      <xsl:variable name="fn" select="normalize-space(db:personname/db:firstname)"/>
      <xsl:variable name="on" select="normalize-space(db:personname/db:othername)"/>
      <xsl:variable name="sn" select="normalize-space(db:personname/db:surname)"/>
      <xsl:variable name="em" select="normalize-space(db:affiliation/db:address/db:email)"/>
      <xsl:variable name="fullname">
        <xsl:value-of select="$fn"/>
        <xsl:if test="$on != ''">
          <xsl:text> </xsl:text>
          <xsl:value-of select="$on"/>
        </xsl:if>
        <xsl:if test="$sn != ''">
          <xsl:text> </xsl:text>
          <xsl:value-of select="$sn"/>
        </xsl:if>
      </xsl:variable>
      <xsl:if test="normalize-space($fullname) != ''">
        <xsl:if test="position() &gt; 1">
          <xsl:text>; </xsl:text>
        </xsl:if>
        <xsl:value-of select="normalize-space($fullname)"/>
        <xsl:if test="$em != ''">
          <xsl:text> &lt;</xsl:text>
          <xsl:value-of select="$em"/>
          <xsl:text>&gt;</xsl:text>
        </xsl:if>
      </xsl:if>
    </xsl:for-each>
    <xsl:if test="$info/db:corpauthor">
      <xsl:if test="count($authors) &gt; 0">
        <xsl:text>; </xsl:text>
      </xsl:if>
      <xsl:value-of select="normalize-space($info/db:corpauthor)"/>
    </xsl:if>
    <!-- Final newline after all authors -->
    <xsl:if test="count($authors) &gt; 0 or $info/db:corpauthor">
      <xsl:text>&#10;</xsl:text>
    </xsl:if>
  </xsl:template>

  <!-- ============================================================
       INFO BLOCK (skip rendering - handled in header templates)
       ============================================================ -->
  <xsl:template match="db:info[parent::db:article or parent::db:book]"/>
  <xsl:template match="db:refmeta"/>

  <!-- ============================================================
       MAN PAGE STRUCTURE
       ============================================================ -->

  <!-- NAME section -->
  <xsl:template match="db:refnamediv">
    <xsl:text>== NAME&#10;</xsl:text>
    <xsl:for-each select="db:refname">
      <xsl:value-of select="normalize-space(.)"/>
      <xsl:if test="position() != last()">
        <xsl:text>, </xsl:text>
      </xsl:if>
    </xsl:for-each>
    <xsl:if test="db:refpurpose">
      <xsl:text> - </xsl:text>
      <!-- Use apply-templates (not normalize-space/value-of) so that inline
           markup such as <emphasis>, <command>, <option>, <replaceable> inside
           the refpurpose is preserved as AsciiDoc inline markup (_italic_,
           *bold*, etc.).  normalize-space() discards child element markup. -->
      <xsl:apply-templates select="db:refpurpose/node()"/>
    </xsl:if>
    <xsl:text>&#10;&#10;</xsl:text>
  </xsl:template>

  <!-- SYNOPSIS section -->
  <xsl:template match="db:refsynopsisdiv">
    <xsl:text>== SYNOPSIS&#10;</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>&#10;</xsl:text>
  </xsl:template>

  <!-- Command synopsis block -->
  <xsl:template match="db:cmdsynopsis">
    <xsl:call-template name="block-sep"/>
    <xsl:text>&#10;[source]&#10;</xsl:text>
    <xsl:text>----&#10;</xsl:text>
    <xsl:apply-templates mode="synopsis"/>
    <xsl:text>&#10;----&#10;&#10;</xsl:text>
  </xsl:template>

  <!-- Synopsis mode: render command synopsis elements as plain text -->
  <xsl:template match="db:command" mode="synopsis">
    <xsl:value-of select="normalize-space(.)"/>
  </xsl:template>

  <xsl:template match="db:arg" mode="synopsis">
    <xsl:text> </xsl:text>
    <xsl:variable name="choice" select="@choice"/>
    <xsl:variable name="rep" select="@rep"/>
    <xsl:choose>
      <xsl:when test="$choice = 'req'">
        <xsl:text>{</xsl:text>
        <xsl:apply-templates mode="synopsis"/>
        <xsl:text>}</xsl:text>
      </xsl:when>
      <xsl:when test="$choice = 'plain'">
        <xsl:apply-templates mode="synopsis"/>
      </xsl:when>
      <xsl:otherwise>
        <!-- opt is default -->
        <xsl:text>[</xsl:text>
        <xsl:apply-templates mode="synopsis"/>
        <xsl:text>]</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:if test="$rep = 'repeat'">
      <xsl:text>...</xsl:text>
    </xsl:if>
  </xsl:template>

  <xsl:template match="db:replaceable" mode="synopsis">
    <xsl:value-of select="normalize-space(.)"/>
  </xsl:template>

  <xsl:template match="db:option" mode="synopsis">
    <xsl:value-of select="normalize-space(.)"/>
  </xsl:template>

  <xsl:template match="db:group" mode="synopsis">
    <xsl:text> </xsl:text>
    <xsl:variable name="choice" select="@choice"/>
    <xsl:variable name="rep" select="@rep"/>
    <xsl:choose>
      <xsl:when test="$choice = 'req'">
        <xsl:text>{</xsl:text>
        <xsl:for-each select="db:arg | db:group">
          <xsl:if test="position() > 1"><xsl:text> | </xsl:text></xsl:if>
          <!-- First child: suppress the leading space that db:arg normally emits -->
          <xsl:variable name="content">
            <xsl:apply-templates select="." mode="synopsis"/>
          </xsl:variable>
          <xsl:choose>
            <xsl:when test="position() = 1">
              <xsl:value-of select="substring($content, 2)"/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="substring($content, 2)"/>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:for-each>
        <xsl:text>}</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>[</xsl:text>
        <xsl:for-each select="db:arg | db:group">
          <xsl:if test="position() > 1"><xsl:text> | </xsl:text></xsl:if>
          <xsl:variable name="content">
            <xsl:apply-templates select="." mode="synopsis"/>
          </xsl:variable>
          <xsl:value-of select="substring($content, 2)"/>
        </xsl:for-each>
        <xsl:text>]</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:if test="$rep = 'repeat'">
      <xsl:text>...</xsl:text>
    </xsl:if>
  </xsl:template>

  <xsl:template match="db:sbr" mode="synopsis">
    <xsl:text> \&#10;    </xsl:text>
  </xsl:template>

  <xsl:template match="db:synopfragment" mode="synopsis">
    <xsl:text>&#10;</xsl:text>
    <xsl:apply-templates mode="synopsis"/>
  </xsl:template>

  <xsl:template match="db:synopfragmentref" mode="synopsis">
    <xsl:text>&lt;</xsl:text>
    <xsl:value-of select="normalize-space(.)"/>
    <xsl:text>&gt;</xsl:text>
  </xsl:template>

  <!-- text in synopsis mode: normalize whitespace but preserve a trailing space
       when the text ends with whitespace and is followed by another node
       (e.g. the space between "-f " and <replaceable>font</replaceable> in an <arg>). -->
  <xsl:template match="text()" mode="synopsis">
    <xsl:variable name="raw" select="."/>
    <xsl:variable name="norm" select="normalize-space($raw)"/>
    <xsl:if test="string-length($norm) > 0">
      <xsl:value-of select="$norm"/>
      <xsl:variable name="last-char" select="substring($raw, string-length($raw), 1)"/>
      <xsl:if test="translate($last-char, ' &#9;&#10;&#13;', '') = '' and following-sibling::node()">
        <xsl:text> </xsl:text>
      </xsl:if>
    </xsl:if>
  </xsl:template>

  <!-- Function synopsis -->
  <xsl:template match="db:funcsynopsis">
    <xsl:call-template name="block-sep"/>
    <xsl:text>&#10;[source,c]&#10;</xsl:text>
    <xsl:text>----&#10;</xsl:text>
    <xsl:apply-templates mode="funcsynopsis"/>
    <xsl:text>&#10;----&#10;&#10;</xsl:text>
  </xsl:template>

  <!-- funcsynopsisinfo inside funcsynopsis: verbatim preamble (e.g. #include) -->
  <xsl:template match="db:funcsynopsisinfo" mode="funcsynopsis">
    <xsl:value-of select="."/>
  </xsl:template>

  <xsl:template match="db:funcprototype" mode="funcsynopsis">
    <xsl:apply-templates mode="funcsynopsis"/>
    <xsl:text>;&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="db:funcdef" mode="funcsynopsis">
    <xsl:apply-templates mode="funcsynopsis"/>
  </xsl:template>

  <xsl:template match="db:function" mode="funcsynopsis">
    <xsl:value-of select="normalize-space(.)"/>
  </xsl:template>

  <xsl:template match="db:paramdef" mode="funcsynopsis">
    <xsl:if test="not(preceding-sibling::db:paramdef)"><xsl:text>(</xsl:text></xsl:if>
    <xsl:if test="preceding-sibling::db:paramdef"><xsl:text>, </xsl:text></xsl:if>
    <xsl:apply-templates mode="funcsynopsis"/>
    <xsl:if test="not(following-sibling::db:paramdef)"><xsl:text>)</xsl:text></xsl:if>
  </xsl:template>

  <xsl:template match="db:void" mode="funcsynopsis">
    <xsl:text>(void)</xsl:text>
  </xsl:template>

  <xsl:template match="db:parameter" mode="funcsynopsis">
    <xsl:value-of select="normalize-space(.)"/>
  </xsl:template>

  <xsl:template match="text()" mode="funcsynopsis">
    <!-- Preserve meaningful whitespace within funcdef/paramdef contexts.
         normalize-space() would strip the spaces in e.g. "struct rt_i *func".
         Instead: preserve text as-is within elements that need type/name spacing
         (funcdef, paramdef), but collapse pure-whitespace text between sibling
         elements at the funcprototype level to avoid extra blank lines. -->
    <xsl:choose>
      <xsl:when test="parent::db:funcdef or parent::db:paramdef">
        <xsl:value-of select="."/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:if test="normalize-space(.) != ''">
          <xsl:value-of select="normalize-space(.)"/>
        </xsl:if>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- refsect1/refsection: top-level man page sections (NAME, SYNOPSIS, etc.) -->
  <xsl:template match="db:refsection | db:refsect1">
    <xsl:variable name="level">
      <xsl:call-template name="heading-level"/>
    </xsl:variable>
    <xsl:if test="@xml:id">
      <xsl:text>[[</xsl:text>
      <xsl:value-of select="@xml:id"/>
      <xsl:text>]]&#10;</xsl:text>
    </xsl:if>
    <xsl:call-template name="section-mark">
      <xsl:with-param name="level" select="$level"/>
    </xsl:call-template>
    <xsl:text> </xsl:text>
    <xsl:value-of select="normalize-space(db:title | db:info/db:title)"/>
    <xsl:text>&#10;&#10;</xsl:text>
    <xsl:apply-templates select="*[not(self::db:title) and not(self::db:info)]"/>
    <xsl:text>&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="db:refsect2">
    <xsl:variable name="level">
      <xsl:call-template name="heading-level"/>
    </xsl:variable>
    <xsl:if test="@xml:id">
      <xsl:text>[[</xsl:text>
      <xsl:value-of select="@xml:id"/>
      <xsl:text>]]&#10;</xsl:text>
    </xsl:if>
    <xsl:call-template name="section-mark">
      <xsl:with-param name="level" select="$level"/>
    </xsl:call-template>
    <xsl:text> </xsl:text>
    <xsl:value-of select="normalize-space(db:title | db:info/db:title)"/>
    <xsl:text>&#10;&#10;</xsl:text>
    <xsl:apply-templates select="*[not(self::db:title) and not(self::db:info)]"/>
    <xsl:text>&#10;</xsl:text>
  </xsl:template>

  <!-- ============================================================
       STRUCTURAL SECTIONS (article / book)
       ============================================================ -->

  <xsl:template match="db:part">
    <xsl:variable name="level">
      <xsl:call-template name="heading-level"/>
    </xsl:variable>
    <xsl:if test="@xml:id">
      <xsl:text>[[</xsl:text>
      <xsl:value-of select="@xml:id"/>
      <xsl:text>]]&#10;</xsl:text>
    </xsl:if>
    <xsl:call-template name="section-mark">
      <xsl:with-param name="level" select="$level"/>
    </xsl:call-template>
    <xsl:text> </xsl:text>
    <xsl:value-of select="normalize-space(db:title | db:info/db:title)"/>
    <xsl:text>&#10;&#10;</xsl:text>
    <xsl:apply-templates select="*[not(self::db:title) and not(self::db:info)]"/>
  </xsl:template>

  <xsl:template match="db:chapter">
    <xsl:variable name="level">
      <xsl:call-template name="heading-level"/>
    </xsl:variable>
    <xsl:if test="@xml:id">
      <xsl:text>[[</xsl:text>
      <xsl:value-of select="@xml:id"/>
      <xsl:text>]]&#10;</xsl:text>
    </xsl:if>
    <xsl:call-template name="section-mark">
      <xsl:with-param name="level" select="$level"/>
    </xsl:call-template>
    <xsl:text> </xsl:text>
    <xsl:value-of select="normalize-space(db:title | db:info/db:title)"/>
    <xsl:text>&#10;&#10;</xsl:text>
    <xsl:apply-templates select="*[not(self::db:title) and not(self::db:info)]"/>
  </xsl:template>

  <xsl:template match="db:preface">
    <xsl:variable name="level">
      <xsl:call-template name="heading-level"/>
    </xsl:variable>
    <xsl:if test="@xml:id">
      <xsl:text>[[</xsl:text>
      <xsl:value-of select="@xml:id"/>
      <xsl:text>]]&#10;</xsl:text>
    </xsl:if>
    <xsl:text>[preface]&#10;</xsl:text>
    <xsl:call-template name="section-mark">
      <xsl:with-param name="level" select="$level"/>
    </xsl:call-template>
    <xsl:text> </xsl:text>
    <xsl:value-of select="normalize-space(db:title | db:info/db:title)"/>
    <xsl:text>&#10;&#10;</xsl:text>
    <xsl:apply-templates select="*[not(self::db:title) and not(self::db:info)]"/>
  </xsl:template>

  <xsl:template match="db:appendix">
    <xsl:variable name="level">
      <xsl:call-template name="heading-level"/>
    </xsl:variable>
    <xsl:if test="@xml:id">
      <xsl:text>[[</xsl:text>
      <xsl:value-of select="@xml:id"/>
      <xsl:text>]]&#10;</xsl:text>
    </xsl:if>
    <xsl:text>[appendix]&#10;</xsl:text>
    <xsl:call-template name="section-mark">
      <xsl:with-param name="level" select="$level"/>
    </xsl:call-template>
    <xsl:text> </xsl:text>
    <xsl:value-of select="normalize-space(db:title | db:info/db:title)"/>
    <xsl:text>&#10;&#10;</xsl:text>
    <xsl:apply-templates select="*[not(self::db:title) and not(self::db:info)]"/>
  </xsl:template>

  <xsl:template match="db:dedication">
    <xsl:variable name="level">
      <xsl:call-template name="heading-level"/>
    </xsl:variable>
    <xsl:if test="@xml:id">
      <xsl:text>[[</xsl:text>
      <xsl:value-of select="@xml:id"/>
      <xsl:text>]]&#10;</xsl:text>
    </xsl:if>
    <xsl:text>[dedication]&#10;</xsl:text>
    <xsl:call-template name="section-mark">
      <xsl:with-param name="level" select="$level"/>
    </xsl:call-template>
    <xsl:text> Dedication&#10;&#10;</xsl:text>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="db:acknowledgements">
    <xsl:variable name="level">
      <xsl:call-template name="heading-level"/>
    </xsl:variable>
    <xsl:if test="@xml:id">
      <xsl:text>[[</xsl:text>
      <xsl:value-of select="@xml:id"/>
      <xsl:text>]]&#10;</xsl:text>
    </xsl:if>
    <xsl:call-template name="section-mark">
      <xsl:with-param name="level" select="$level"/>
    </xsl:call-template>
    <xsl:text> Acknowledgements&#10;&#10;</xsl:text>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="db:section">
    <xsl:variable name="level">
      <xsl:call-template name="heading-level"/>
    </xsl:variable>
    <xsl:variable name="title-text">
      <xsl:value-of select="normalize-space(db:title | db:info/db:title)"/>
    </xsl:variable>
    <xsl:if test="@xml:id">
      <xsl:text>[[</xsl:text>
      <xsl:value-of select="@xml:id"/>
      <xsl:text>]]&#10;</xsl:text>
    </xsl:if>
    <!-- Only emit a heading if there is a title -->
    <xsl:if test="$title-text != ''">
      <xsl:call-template name="section-mark">
        <xsl:with-param name="level" select="$level"/>
      </xsl:call-template>
      <xsl:text> </xsl:text>
      <xsl:value-of select="$title-text"/>
      <xsl:text>&#10;&#10;</xsl:text>
    </xsl:if>
    <xsl:apply-templates select="*[not(self::db:title) and not(self::db:info)]"/>
    <xsl:text>&#10;</xsl:text>
  </xsl:template>

  <!-- bridgehead (informal heading) -->
  <xsl:template match="db:bridgehead">
    <xsl:variable name="renderas" select="@renderas"/>
    <xsl:text>&#10;[discrete]&#10;</xsl:text>
    <xsl:choose>
      <xsl:when test="$renderas = 'sect1'">==</xsl:when>
      <xsl:when test="$renderas = 'sect2'">===</xsl:when>
      <xsl:when test="$renderas = 'sect3'">====</xsl:when>
      <xsl:when test="$renderas = 'sect4'">=====</xsl:when>
      <xsl:otherwise>===</xsl:otherwise>
    </xsl:choose>
    <xsl:text> </xsl:text>
    <xsl:apply-templates/>
    <xsl:text>&#10;&#10;</xsl:text>
  </xsl:template>

  <!-- ============================================================
       PARAGRAPHS
       ============================================================ -->

  <xsl:template match="db:para | db:simpara">
    <xsl:apply-templates/>
    <xsl:text>&#10;&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="db:formalpara">
    <!-- Title may be a direct db:title child, or nested in db:info/db:title -->
    <xsl:variable name="ftitle">
      <xsl:choose>
        <xsl:when test="db:title">
          <xsl:value-of select="normalize-space(db:title)"/>
        </xsl:when>
        <xsl:when test="db:info/db:title">
          <xsl:value-of select="normalize-space(db:info/db:title)"/>
        </xsl:when>
      </xsl:choose>
    </xsl:variable>
    <xsl:if test="string-length($ftitle) > 0">
      <xsl:text>.</xsl:text>
      <xsl:value-of select="$ftitle"/>
      <xsl:text>&#10;</xsl:text>
    </xsl:if>
    <xsl:apply-templates select="db:para"/>
  </xsl:template>

  <!-- ============================================================
       ADMONITIONS
       ============================================================ -->

  <xsl:template match="db:note">
    <xsl:call-template name="block-sep"/>
    <xsl:text>&#10;[NOTE]&#10;====&#10;</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>====&#10;&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="db:warning">
    <xsl:call-template name="block-sep"/>
    <xsl:text>&#10;[WARNING]&#10;====&#10;</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>====&#10;&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="db:caution">
    <xsl:call-template name="block-sep"/>
    <xsl:text>&#10;[CAUTION]&#10;====&#10;</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>====&#10;&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="db:tip">
    <xsl:call-template name="block-sep"/>
    <xsl:text>&#10;[TIP]&#10;====&#10;</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>====&#10;&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="db:important">
    <xsl:call-template name="block-sep"/>
    <xsl:text>&#10;[IMPORTANT]&#10;====&#10;</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>====&#10;&#10;</xsl:text>
  </xsl:template>

  <!-- ============================================================
       BLOCK QUOTES
       ============================================================ -->

  <xsl:template match="db:blockquote">
    <xsl:call-template name="block-sep"/>
    <xsl:text>&#10;[quote</xsl:text>
    <xsl:if test="db:attribution">
      <xsl:text>, </xsl:text>
      <xsl:value-of select="normalize-space(db:attribution)"/>
    </xsl:if>
    <xsl:text>]&#10;____&#10;</xsl:text>
    <xsl:apply-templates select="*[not(self::db:attribution)]"/>
    <xsl:text>____&#10;&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="db:epigraph">
    <xsl:call-template name="block-sep"/>
    <xsl:text>&#10;[quote</xsl:text>
    <xsl:if test="db:attribution">
      <xsl:text>, </xsl:text>
      <xsl:value-of select="normalize-space(db:attribution)"/>
    </xsl:if>
    <xsl:text>]&#10;____&#10;</xsl:text>
    <xsl:apply-templates select="*[not(self::db:attribution)]"/>
    <xsl:text>____&#10;&#10;</xsl:text>
  </xsl:template>

  <!-- ============================================================
       CODE BLOCKS
       ============================================================ -->

  <xsl:template match="db:programlisting">
    <xsl:call-template name="block-sep"/>
    <xsl:variable name="lang">
      <xsl:choose>
        <xsl:when test="@language"><xsl:value-of select="@language"/></xsl:when>
        <xsl:otherwise></xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:text>&#10;</xsl:text>
    <xsl:if test="$lang != ''">
      <xsl:text>[source,</xsl:text>
      <xsl:value-of select="$lang"/>
      <xsl:text>]&#10;</xsl:text>
    </xsl:if>
    <xsl:text>----&#10;</xsl:text>
    <xsl:value-of select="."/>
    <xsl:if test="substring(., string-length(.), 1) != '&#10;'">
      <xsl:text>&#10;</xsl:text>
    </xsl:if>
    <xsl:text>----&#10;&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="db:screen | db:computeroutput[parent::db:para/parent::db:example or parent::db:para/parent::db:refsection]">
    <xsl:call-template name="block-sep"/>
    <xsl:text>&#10;....&#10;</xsl:text>
    <xsl:value-of select="."/>
    <xsl:if test="substring(., string-length(.), 1) != '&#10;'">
      <xsl:text>&#10;</xsl:text>
    </xsl:if>
    <xsl:text>....&#10;&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="db:literallayout">
    <xsl:call-template name="block-sep"/>
    <xsl:text>&#10;....&#10;</xsl:text>
    <xsl:value-of select="."/>
    <xsl:if test="substring(., string-length(.), 1) != '&#10;'">
      <xsl:text>&#10;</xsl:text>
    </xsl:if>
    <xsl:text>....&#10;&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="db:synopsis">
    <xsl:call-template name="block-sep"/>
    <xsl:text>&#10;----&#10;</xsl:text>
    <xsl:value-of select="."/>
    <xsl:if test="substring(., string-length(.), 1) != '&#10;'">
      <xsl:text>&#10;</xsl:text>
    </xsl:if>
    <xsl:text>----&#10;&#10;</xsl:text>
  </xsl:template>

  <!-- ============================================================
       EXAMPLES / FORMAL PARAGRAPHS
       ============================================================ -->

  <xsl:template match="db:example">
    <xsl:call-template name="block-sep"/>
    <xsl:if test="db:title">
      <xsl:variable name="t" select="normalize-space(db:title)"/>
      <xsl:if test="string-length($t) > 0">
        <xsl:text>.</xsl:text>
        <!-- Apply inline templates to preserve emphasis/_italic_ in the title -->
        <xsl:apply-templates select="db:title/node()"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:if>
    </xsl:if>
    <xsl:text>[example]&#10;====&#10;</xsl:text>
    <xsl:apply-templates select="*[not(self::db:title)]"/>
    <xsl:text>====&#10;&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="db:informalexample">
    <xsl:call-template name="block-sep"/>
    <xsl:text>[example]&#10;====&#10;</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>====&#10;&#10;</xsl:text>
  </xsl:template>

  <!-- ============================================================
       LISTS
       ============================================================ -->

  <xsl:template match="db:itemizedlist">
    <xsl:param name="depth" select="0"/>
    <!-- Only add block separator when NOT nested inside a listitem (directly or via para) -->
    <xsl:if test="not(ancestor::db:listitem)">
      <xsl:call-template name="block-sep"/>
    </xsl:if>
    <xsl:text>&#10;</xsl:text>
    <xsl:apply-templates select="db:listitem">
      <xsl:with-param name="depth" select="$depth"/>
      <xsl:with-param name="type" select="'bullet'"/>
    </xsl:apply-templates>
    <xsl:if test="not(ancestor::db:listitem)">
      <xsl:text>&#10;</xsl:text>
    </xsl:if>
    <!-- When nested inside a listitem and there is following non-whitespace
         content in the same para, emit a blank line followed by a '+' list
         continuation marker.  The blank line terminates the last sub-list item
         so the continuation is captured at the enclosing dlist-item level. -->
    <xsl:if test="ancestor::db:listitem">
      <xsl:variable name="tail">
        <xsl:for-each select="following-sibling::node()">
          <xsl:value-of select="normalize-space(.)"/>
        </xsl:for-each>
      </xsl:variable>
      <xsl:if test="string-length(normalize-space($tail)) > 0">
        <xsl:text>&#10;+&#10;</xsl:text>
      </xsl:if>
    </xsl:if>
  </xsl:template>

  <xsl:template match="db:orderedlist">
    <xsl:param name="depth" select="0"/>
    <!-- Only add block separator when NOT nested inside a listitem (directly or via para) -->
    <xsl:if test="not(ancestor::db:listitem)">
      <xsl:call-template name="block-sep"/>
    </xsl:if>
    <xsl:text>&#10;</xsl:text>
    <xsl:apply-templates select="db:listitem">
      <xsl:with-param name="depth" select="$depth"/>
      <xsl:with-param name="type" select="'number'"/>
    </xsl:apply-templates>
    <xsl:if test="not(ancestor::db:listitem)">
      <xsl:text>&#10;</xsl:text>
    </xsl:if>
    <!-- When nested inside a listitem and there is following non-whitespace
         content in the same para, emit a blank line followed by a '+' list
         continuation marker.  The blank line terminates the last sub-list item
         so the continuation is captured at the enclosing dlist-item level. -->
    <xsl:if test="ancestor::db:listitem">
      <xsl:variable name="tail">
        <xsl:for-each select="following-sibling::node()">
          <xsl:value-of select="normalize-space(.)"/>
        </xsl:for-each>
      </xsl:variable>
      <xsl:if test="string-length(normalize-space($tail)) > 0">
        <xsl:text>&#10;+&#10;</xsl:text>
      </xsl:if>
    </xsl:if>
  </xsl:template>

  <xsl:template match="db:listitem">
    <xsl:param name="depth" select="0"/>
    <xsl:param name="type" select="'bullet'"/>
    <!-- Compute nesting based on ancestor list depth -->
    <xsl:variable name="nest-depth">
      <xsl:choose>
        <xsl:when test="$depth > 0">
          <xsl:value-of select="$depth"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="count(ancestor::db:itemizedlist) + count(ancestor::db:orderedlist) - 1"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:choose>
      <xsl:when test="$type = 'bullet'">
        <xsl:call-template name="list-bullet">
          <xsl:with-param name="depth" select="$nest-depth"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:call-template name="list-number">
          <xsl:with-param name="depth" select="$nest-depth"/>
        </xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text> </xsl:text>
    <!-- First paragraph inline, rest as continuation blocks -->
    <xsl:for-each select="*">
      <xsl:choose>
        <xsl:when test="position() = 1 and (self::db:para or self::db:simpara)">
          <xsl:apply-templates/>
          <xsl:text>&#10;</xsl:text>
        </xsl:when>
        <xsl:when test="self::db:itemizedlist or self::db:orderedlist or self::db:variablelist">
          <xsl:apply-templates select="."/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:text>+&#10;</xsl:text>
          <xsl:apply-templates select="."/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
  </xsl:template>

  <!-- simplelist -->
  <xsl:template match="db:simplelist">
    <xsl:call-template name="block-sep"/>
    <xsl:text>&#10;</xsl:text>
    <xsl:for-each select="db:member">
      <xsl:text>* </xsl:text>
      <xsl:apply-templates/>
      <xsl:text>&#10;</xsl:text>
    </xsl:for-each>
    <xsl:text>&#10;</xsl:text>
  </xsl:template>

  <!-- ============================================================
       VARIABLE / DEFINITION LISTS
       ============================================================ -->

  <xsl:template match="db:variablelist">
    <xsl:call-template name="block-sep"/>
    <xsl:text>&#10;</xsl:text>
    <xsl:apply-templates select="db:varlistentry"/>
    <xsl:text>&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="db:varlistentry">
    <!-- When the entry has an empty term (e.g. in refsynopsisdiv where multiple
         cmdsynopsis variants are listed as a variablelist with empty terms),
         just emit the listitem content directly without any dlist markup.
         Emitting bare '::' produces literal '::\n' in the AsciiDoc output which
         renders as visible '::' text in both asciidoctor and man page backends. -->
    <xsl:variable name="term-text" select="normalize-space(db:term)"/>
    <xsl:choose>
      <xsl:when test="string-length($term-text) = 0">
        <!-- Empty term: emit listitem content directly as top-level blocks -->
        <xsl:apply-templates select="db:listitem/*"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <!-- term(s) -->
        <xsl:for-each select="db:term">
          <xsl:apply-templates/>
          <xsl:if test="position() != last()">
            <xsl:text>&#10;</xsl:text>
          </xsl:if>
        </xsl:for-each>
        <xsl:text>::&#10;</xsl:text>
        <!-- definition content: each block in the listitem is output via its normal
             template.  However, AsciiDoc ends a dlist item body at a blank line, so
             when a listitem contains multiple para/block children we must keep them
             attached with a '+' list-continuation marker (on its own line, without a
             preceding blank line).  We achieve this by emitting '+\n' *instead of*
             the '\n\n' that the db:para template would normally append: we apply
             inline templates of each para directly, then choose the right separator. -->
        <xsl:variable name="children" select="db:listitem/*"/>
        <xsl:for-each select="$children">
          <xsl:choose>
            <!-- Para/simpara that is NOT the last child: emit content then +\n
                 (no blank line – the + attaches the next block to this item). -->
            <xsl:when test="(self::db:para or self::db:simpara) and not(position() = last())">
              <xsl:apply-templates/>
              <xsl:text>&#10;+&#10;</xsl:text>
            </xsl:when>
            <!-- All other elements (including the last child): normal rendering. -->
            <xsl:otherwise>
              <xsl:apply-templates select="."/>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:for-each>
        <xsl:text>&#10;</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- ============================================================
       TABLES
       ============================================================ -->

  <!-- Compute the number of columns an entry spans.
       Checks @namest/@nameend directly, then falls back to @spanname.
       Returns 1 if the entry does not span multiple columns. -->
  <xsl:template name="entry-colspan">
    <xsl:param name="entry" select="."/>
    <xsl:variable name="namest">
      <xsl:choose>
        <xsl:when test="$entry/@namest">
          <xsl:value-of select="$entry/@namest"/>
        </xsl:when>
        <xsl:when test="$entry/@spanname">
          <xsl:variable name="sn" select="$entry/@spanname"/>
          <xsl:value-of select="ancestor::db:tgroup/db:spanspec[@spanname=$sn]/@namest"/>
        </xsl:when>
      </xsl:choose>
    </xsl:variable>
    <xsl:variable name="nameend">
      <xsl:choose>
        <xsl:when test="$entry/@nameend">
          <xsl:value-of select="$entry/@nameend"/>
        </xsl:when>
        <xsl:when test="$entry/@spanname">
          <xsl:variable name="sn" select="$entry/@spanname"/>
          <xsl:value-of select="ancestor::db:tgroup/db:spanspec[@spanname=$sn]/@nameend"/>
        </xsl:when>
      </xsl:choose>
    </xsl:variable>
    <xsl:choose>
      <xsl:when test="$namest != '' and $nameend != '' and $namest != $nameend">
        <!-- Count columns from namest to nameend inclusive -->
        <xsl:variable name="pos-start"
          select="count(ancestor::db:tgroup/db:colspec[@colname=$namest][1]/preceding-sibling::db:colspec) + 1"/>
        <xsl:variable name="pos-end"
          select="count(ancestor::db:tgroup/db:colspec[@colname=$nameend][1]/preceding-sibling::db:colspec) + 1"/>
        <xsl:choose>
          <xsl:when test="$pos-end >= $pos-start">
            <xsl:value-of select="$pos-end - $pos-start + 1"/>
          </xsl:when>
          <xsl:otherwise>1</xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:otherwise>1</xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="db:table | db:informaltable">
    <xsl:call-template name="block-sep"/>
    <xsl:if test="@xml:id">
      <xsl:text>[[</xsl:text>
      <xsl:value-of select="@xml:id"/>
      <xsl:text>]]&#10;</xsl:text>
    </xsl:if>
    <xsl:if test="db:title or db:info/db:title">
      <xsl:text>.</xsl:text>
      <xsl:value-of select="normalize-space(db:title | db:info/db:title)"/>
      <xsl:text>&#10;</xsl:text>
    </xsl:if>
    <!-- Emit a [cols="N*"] attribute when the DocBook tgroup declares the
         column count.  Using the repeat notation "N*" tells the AsciiDoc
         parser how many columns to expect; this is critical for tables
         where each entry is emitted on its own line (multi-line cells)
         so the parser does not prematurely commit a one-cell row. -->
    <xsl:if test=".//db:tgroup[1]/@cols">
      <xsl:text>[cols="</xsl:text>
      <xsl:value-of select=".//db:tgroup[1]/@cols"/>
      <xsl:text>*"]&#10;</xsl:text>
    </xsl:if>
    <!-- Add %noheader option when there is no explicit thead block so
         AsciiDoc does not promote the first body row to a header row. -->
    <xsl:if test="not(.//db:thead)">
      <xsl:text>[%noheader]&#10;</xsl:text>
    </xsl:if>
    <xsl:text>|===&#10;</xsl:text>
    <!-- Process thead rows first, followed by a blank line to mark the
         header/body boundary in AsciiDoc table syntax. -->
    <xsl:if test=".//db:thead">
      <xsl:apply-templates select=".//db:thead/db:row"/>
      <xsl:text>&#10;</xsl:text>
    </xsl:if>
    <!-- Process tbody rows -->
    <xsl:apply-templates select=".//db:tbody/db:row"/>
    <!-- Process tfoot rows -->
    <xsl:apply-templates select=".//db:tfoot/db:row"/>
    <xsl:text>|===&#10;&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="db:row">
    <!-- Emit each entry on its own line so that multi-line cell content
         (e.g. a paragraph followed by an image) is correctly accumulated
         by the AsciiDoc parser into a single cell.  The first entry of
         each row immediately follows the previous row's trailing newline
         (no leading blank line) to avoid creating a spurious header/body
         separator before the first row of a table. -->
    <xsl:for-each select="db:entry">
      <!-- Compute column-span and row-span prefixes. -->
      <xsl:variable name="colspan">
        <xsl:call-template name="entry-colspan"/>
      </xsl:variable>
      <xsl:variable name="rowspan">
        <xsl:choose>
          <xsl:when test="@morerows and @morerows > 0">
            <xsl:value-of select="@morerows + 1"/>
          </xsl:when>
          <xsl:otherwise>0</xsl:otherwise>
        </xsl:choose>
      </xsl:variable>
      <!-- Entries after the first within a row each start on their own line.
           The first entry follows directly on the same line as the row
           context (the trailing newline of the previous row or |===). -->
      <xsl:if test="position() > 1">
        <xsl:text>&#10;</xsl:text>
      </xsl:if>
      <!-- Emit row-span prefix .N+ when the cell spans multiple rows. -->
      <xsl:if test="$rowspan > 1">
        <xsl:text>.</xsl:text>
        <xsl:value-of select="$rowspan"/>
        <xsl:text>+</xsl:text>
      </xsl:if>
      <!-- Emit column-span prefix N+ when the cell spans multiple columns. -->
      <xsl:if test="$colspan > 1">
        <xsl:value-of select="$colspan"/>
        <xsl:text>+</xsl:text>
      </xsl:if>
      <xsl:text>|</xsl:text>
      <xsl:apply-templates mode="table-cell"/>
    </xsl:for-each>
    <xsl:text>&#10;</xsl:text>
  </xsl:template>

  <!-- ============================================================
       TABLE CELL MODE
       Inside a table cell we suppress block-level newlines so that
       each entry's content stays on a single (possibly long) line.
       Block elements are separated by a space instead.
       ============================================================ -->

  <!-- Paragraphs inside cells: inline, separated by a single space. -->
  <xsl:template match="db:para | db:simpara" mode="table-cell">
    <xsl:apply-templates mode="table-cell"/>
    <xsl:text> </xsl:text>
  </xsl:template>

  <!-- Emphasis/bold in cell mode -->
  <xsl:template match="db:emphasis" mode="table-cell">
    <xsl:choose>
      <xsl:when test="@role = 'bold' or @role = 'strong'">
        <xsl:text>*</xsl:text>
        <xsl:apply-templates mode="table-cell"/>
        <xsl:text>*</xsl:text>
      </xsl:when>
      <xsl:when test="@role = 'underline'">
        <xsl:text>[.underline]#</xsl:text>
        <xsl:apply-templates mode="table-cell"/>
        <xsl:text>#</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:text>_</xsl:text>
        <xsl:apply-templates mode="table-cell"/>
        <xsl:text>_</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="db:command | db:option | db:userinput" mode="table-cell">
    <xsl:text>*</xsl:text>
    <xsl:apply-templates mode="table-cell"/>
    <xsl:text>*</xsl:text>
  </xsl:template>

  <xsl:template match="db:literal | db:code | db:constant | db:type | db:markup
                       | db:filename | db:varname | db:envar | db:systemitem
                       | db:computeroutput" mode="table-cell">
    <xsl:text>`</xsl:text>
    <xsl:apply-templates mode="table-cell"/>
    <xsl:text>`</xsl:text>
  </xsl:template>

  <xsl:template match="db:replaceable | db:parameter | db:application" mode="table-cell">
    <xsl:text>_</xsl:text>
    <xsl:apply-templates mode="table-cell"/>
    <xsl:text>_</xsl:text>
  </xsl:template>

  <xsl:template match="db:function" mode="table-cell">
    <xsl:text>`</xsl:text>
    <xsl:apply-templates mode="table-cell"/>
    <xsl:text>()`</xsl:text>
  </xsl:template>

  <xsl:template match="db:superscript" mode="table-cell">
    <xsl:text>^</xsl:text>
    <xsl:apply-templates mode="table-cell"/>
    <xsl:text>^</xsl:text>
  </xsl:template>

  <xsl:template match="db:subscript" mode="table-cell">
    <xsl:text>~</xsl:text>
    <xsl:apply-templates mode="table-cell"/>
    <xsl:text>~</xsl:text>
  </xsl:template>

  <xsl:template match="db:quote" mode="table-cell">
    <xsl:text>&#8220;</xsl:text>
    <xsl:apply-templates mode="table-cell"/>
    <xsl:text>&#8221;</xsl:text>
  </xsl:template>

  <xsl:template match="db:link" mode="table-cell">
    <xsl:choose>
      <xsl:when test="@xl:href">
        <xsl:value-of select="@xl:href"/>
        <xsl:if test="normalize-space(.) != ''">
          <xsl:text>[</xsl:text>
          <xsl:apply-templates mode="table-cell"/>
          <xsl:text>]</xsl:text>
        </xsl:if>
      </xsl:when>
      <xsl:when test="@linkend">
        <xsl:text>&lt;&lt;</xsl:text>
        <xsl:value-of select="@linkend"/>
        <xsl:if test="normalize-space(.) != ''">
          <xsl:text>,</xsl:text>
          <xsl:apply-templates mode="table-cell"/>
        </xsl:if>
        <xsl:text>&gt;&gt;</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates mode="table-cell"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="db:xref" mode="table-cell">
    <xsl:text>&lt;&lt;</xsl:text>
    <xsl:value-of select="@linkend"/>
    <xsl:text>&gt;&gt;</xsl:text>
  </xsl:template>

  <xsl:template match="db:citerefentry" mode="table-cell">
    <xsl:text>*</xsl:text>
    <xsl:value-of select="normalize-space(db:refentrytitle)"/>
    <xsl:text>*(</xsl:text>
    <xsl:value-of select="normalize-space(db:manvolnum)"/>
    <xsl:text>)</xsl:text>
  </xsl:template>

  <!-- Inline images inside cells use the inline image: macro. -->
  <xsl:template match="db:inlinemediaobject" mode="table-cell">
    <xsl:call-template name="preferred-imagedata-inline"/>
  </xsl:template>

  <!-- Block images inside cells are rendered inline (image: not image::). -->
  <xsl:template match="db:mediaobject" mode="table-cell">
    <xsl:call-template name="preferred-imagedata-inline"/>
  </xsl:template>

  <!-- Nested tables inside cells: render as a flat list of values
       separated by commas, since AsciiDoc nested table syntax (!=== )
       is not yet supported by asciiquack. -->
  <xsl:template match="db:table | db:informaltable" mode="table-cell">
    <xsl:for-each select=".//db:entry">
      <xsl:if test="position() > 1">
        <xsl:text>, </xsl:text>
      </xsl:if>
      <xsl:apply-templates mode="table-cell"/>
    </xsl:for-each>
  </xsl:template>

  <!-- Programlisting / screen inside cells: inline monospace. -->
  <xsl:template match="db:programlisting | db:screen | db:literallayout
                       | db:synopsis" mode="table-cell">
    <xsl:text>`</xsl:text>
    <xsl:value-of select="normalize-space(.)"/>
    <xsl:text>`</xsl:text>
  </xsl:template>

  <!-- Admonitions in cells: just render the body text. -->
  <xsl:template match="db:note | db:warning | db:caution | db:tip
                       | db:important" mode="table-cell">
    <xsl:apply-templates mode="table-cell"/>
  </xsl:template>

  <!-- Lists inside cells: comma-separated inline form. -->
  <xsl:template match="db:itemizedlist | db:orderedlist" mode="table-cell">
    <xsl:for-each select="db:listitem">
      <xsl:if test="position() > 1">
        <xsl:text>; </xsl:text>
      </xsl:if>
      <xsl:apply-templates mode="table-cell"/>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="db:listitem" mode="table-cell">
    <xsl:apply-templates mode="table-cell"/>
  </xsl:template>

  <!-- footnotes inside cells -->
  <xsl:template match="db:footnote" mode="table-cell">
    <xsl:text>footnote:[</xsl:text>
    <xsl:apply-templates mode="table-cell"/>
    <xsl:text>]</xsl:text>
  </xsl:template>

  <!-- Generic inline passthrough for table-cell mode: delegate to
       apply-templates so any unmatched element still processes children. -->
  <xsl:template match="db:acronym | db:abbrev | db:prompt
                       | db:uri | db:email" mode="table-cell">
    <xsl:apply-templates mode="table-cell"/>
  </xsl:template>

  <!-- text() in table-cell mode: normalize whitespace (same as default). -->
  <xsl:template match="text()" mode="table-cell">
    <xsl:if test="preceding-sibling::node() and
                  string-length(normalize-space(.)) > 0 and
                  translate(substring(.,1,1),' &#9;&#10;&#13;','') = ''">
      <xsl:text> </xsl:text>
    </xsl:if>
    <xsl:value-of select="normalize-space(.)"/>
    <xsl:if test="following-sibling::node() and
                  string-length(.) > 1 and
                  string-length(normalize-space(.)) > 0 and
                  translate(substring(.,string-length(.),1),' &#9;&#10;&#13;','') = ''">
      <xsl:text> </xsl:text>
    </xsl:if>
  </xsl:template>

  <!-- Default catchall in table-cell mode: process children. -->
  <xsl:template match="*" mode="table-cell">
    <xsl:apply-templates mode="table-cell"/>
  </xsl:template>

  <!-- ============================================================
       IMAGES / FIGURES
       ============================================================ -->

  <!-- Select the single preferred imagedata from a mediaobject context:
       prefer role='html', then no role, then any first imageobject. -->
  <xsl:template name="preferred-imagedata">
    <xsl:choose>
      <xsl:when test="db:imageobject[@role='html']/db:imagedata">
        <xsl:apply-templates select="db:imageobject[@role='html'][1]/db:imagedata"/>
      </xsl:when>
      <xsl:when test="db:imageobject[not(@role)]/db:imagedata">
        <xsl:apply-templates select="db:imageobject[not(@role)][1]/db:imagedata"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="db:imageobject[1]/db:imagedata"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Same selection but in inline (image:) mode -->
  <xsl:template name="preferred-imagedata-inline">
    <xsl:choose>
      <xsl:when test="db:imageobject[@role='html']/db:imagedata">
        <xsl:apply-templates select="db:imageobject[@role='html'][1]/db:imagedata" mode="inline"/>
      </xsl:when>
      <xsl:when test="db:imageobject[not(@role)]/db:imagedata">
        <xsl:apply-templates select="db:imageobject[not(@role)][1]/db:imagedata" mode="inline"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates select="db:imageobject[1]/db:imagedata" mode="inline"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="db:figure | db:informalfigure">
    <xsl:call-template name="block-sep"/>
    <xsl:if test="@xml:id">
      <xsl:text>[[</xsl:text>
      <xsl:value-of select="@xml:id"/>
      <xsl:text>]]&#10;</xsl:text>
    </xsl:if>
    <!-- DocBook 5 allows the title inside an <info> child; check both. -->
    <xsl:if test="db:title or db:info/db:title">
      <xsl:text>.</xsl:text>
      <xsl:value-of select="normalize-space(db:title | db:info/db:title)"/>
      <xsl:text>&#10;</xsl:text>
    </xsl:if>
    <xsl:for-each select=".//db:mediaobject[1]">
      <xsl:call-template name="preferred-imagedata"/>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="db:mediaobject">
    <xsl:call-template name="block-sep"/>
    <!-- In AsciiDoc the block title (caption) must appear BEFORE the image
         macro.  Emit it first, then the image. -->
    <xsl:if test="db:caption">
      <xsl:text>.</xsl:text>
      <xsl:apply-templates select="db:caption/*"/>
      <xsl:text>&#10;</xsl:text>
    </xsl:if>
    <xsl:call-template name="preferred-imagedata"/>
  </xsl:template>

  <xsl:template match="db:inlinemediaobject">
    <xsl:call-template name="preferred-imagedata-inline"/>
  </xsl:template>

  <!-- Extract alt text: prefer @alt attribute, then textobject/phrase,
       then textobject/para. -->
  <xsl:template name="imagedata-alt">
    <!-- Walk up to the containing mediaobject to find the textobject. -->
    <xsl:variable name="mo" select="ancestor::db:mediaobject[1] |
                                     ancestor::db:inlinemediaobject[1]"/>
    <xsl:choose>
      <xsl:when test="@alt">
        <xsl:value-of select="@alt"/>
      </xsl:when>
      <xsl:when test="$mo/db:textobject/db:phrase">
        <xsl:value-of select="normalize-space($mo/db:textobject/db:phrase)"/>
      </xsl:when>
      <xsl:when test="$mo/db:textobject/db:para">
        <xsl:value-of select="normalize-space($mo/db:textobject/db:para)"/>
      </xsl:when>
    </xsl:choose>
  </xsl:template>

  <!-- Block image macro (image::) -->
  <xsl:template match="db:imagedata">
    <xsl:variable name="alt">
      <xsl:call-template name="imagedata-alt"/>
    </xsl:variable>
    <!-- Prefer @width; fall back to @contentwidth if @width is absent -->
    <xsl:variable name="img-width">
      <xsl:choose>
        <xsl:when test="@width"><xsl:value-of select="@width"/></xsl:when>
        <xsl:when test="@contentwidth"><xsl:value-of select="@contentwidth"/></xsl:when>
      </xsl:choose>
    </xsl:variable>
    <xsl:text>image::</xsl:text>
    <xsl:value-of select="@fileref"/>
    <xsl:text>[</xsl:text>
    <xsl:value-of select="$alt"/>
    <xsl:if test="$img-width != ''">
      <xsl:if test="$alt != ''">
        <xsl:text>,</xsl:text>
      </xsl:if>
      <xsl:text>width=</xsl:text>
      <xsl:value-of select="$img-width"/>
    </xsl:if>
    <xsl:text>]&#10;&#10;</xsl:text>
  </xsl:template>

  <!-- Inline image macro (image:) - no trailing blank line -->
  <xsl:template match="db:imagedata" mode="inline">
    <xsl:variable name="alt">
      <xsl:call-template name="imagedata-alt"/>
    </xsl:variable>
    <!-- Prefer @width; fall back to @contentwidth if @width is absent -->
    <xsl:variable name="img-width">
      <xsl:choose>
        <xsl:when test="@width"><xsl:value-of select="@width"/></xsl:when>
        <xsl:when test="@contentwidth"><xsl:value-of select="@contentwidth"/></xsl:when>
      </xsl:choose>
    </xsl:variable>
    <xsl:text>image:</xsl:text>
    <xsl:value-of select="@fileref"/>
    <xsl:text>[</xsl:text>
    <xsl:value-of select="$alt"/>
    <xsl:if test="$img-width != ''">
      <xsl:if test="$alt != ''">
        <xsl:text>,</xsl:text>
      </xsl:if>
      <xsl:text>width=</xsl:text>
      <xsl:value-of select="$img-width"/>
    </xsl:if>
    <xsl:text>]</xsl:text>
  </xsl:template>

  <!-- imageobject is handled via preferred-imagedata* named templates;
       suppress default processing.  textobject is consumed via the
       imagedata-alt named template above; suppress it here so it does not
       produce stray text output. -->
  <xsl:template match="db:imageobject | db:textobject"/>

  <xsl:template match="db:screenshot">
    <xsl:call-template name="block-sep"/>
    <xsl:apply-templates/>
  </xsl:template>

  <!-- ============================================================
       INLINE MARKUP
       ============================================================ -->

  <!-- emphasis: italic by default, bold if role="bold"/"strong"/"B", underline if role="underline" -->
  <!-- remap="B" means "render as bold" (from man-page conversions); remap="I" means italic. -->
  <!-- Emphasis with xlink:href and no text is treated as a hyperlink. -->
  <xsl:template match="db:emphasis">
    <xsl:choose>
      <!-- Linked emphasis: xl:href present → emit as a URL link -->
      <xsl:when test="@xl:href">
        <xsl:variable name="txt" select="normalize-space(.)"/>
        <xsl:choose>
          <xsl:when test="$txt != ''">
            <xsl:value-of select="@xl:href"/>
            <xsl:text>[</xsl:text>
            <xsl:apply-templates/>
            <xsl:text>]</xsl:text>
          </xsl:when>
          <xsl:otherwise>
            <!-- No text: emit the URL bare -->
            <xsl:value-of select="@xl:href"/>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:when>
      <xsl:when test="@role = 'bold' or @role = 'strong' or @role = 'B'
                      or @remap = 'B' or @remap = 'b'">
        <xsl:call-template name="inline-leading-space"/>
        <xsl:text>*</xsl:text>
        <xsl:apply-templates/>
        <xsl:text>*</xsl:text>
        <xsl:call-template name="inline-trailing-space"/>
      </xsl:when>
      <xsl:when test="@role = 'underline'">
        <xsl:call-template name="inline-leading-space"/>
        <xsl:text>[.underline]#</xsl:text>
        <xsl:apply-templates/>
        <xsl:text>#</xsl:text>
        <xsl:call-template name="inline-trailing-space"/>
      </xsl:when>
      <xsl:otherwise>
        <!-- Skip empty emphasis (no text content) to avoid stray __ artifacts -->
        <xsl:if test="normalize-space(.) != ''">
          <xsl:call-template name="inline-leading-space"/>
          <!-- Use unconstrained __text__ when content contains underscores,
               to prevent '_name_with_underscores_' from breaking italic parsing.
               Use constrained _text_ for plain words without underscores. -->
          <xsl:choose>
            <xsl:when test="contains(normalize-space(.), '_')">
              <xsl:text>__</xsl:text>
              <xsl:apply-templates/>
              <xsl:text>__</xsl:text>
            </xsl:when>
            <xsl:otherwise>
              <xsl:text>_</xsl:text>
              <xsl:apply-templates/>
              <xsl:text>_</xsl:text>
            </xsl:otherwise>
          </xsl:choose>
          <xsl:call-template name="inline-trailing-space"/>
        </xsl:if>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- command: bold monospace -->
  <xsl:template match="db:command">
    <xsl:call-template name="inline-leading-space"/>
    <xsl:text>*</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>*</xsl:text>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <!-- option: bold -->
  <xsl:template match="db:option">
    <xsl:call-template name="inline-leading-space"/>
    <xsl:text>*</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>*</xsl:text>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <!-- replaceable: italic -->
  <xsl:template match="db:replaceable">
    <xsl:call-template name="inline-leading-space"/>
    <xsl:text>_</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>_</xsl:text>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <!-- literal, code: monospace -->
  <xsl:template match="db:literal | db:code | db:constant | db:type | db:markup">
    <xsl:call-template name="inline-leading-space"/>
    <xsl:text>`</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>`</xsl:text>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <!-- filename, varname, envar: monospace italic -->
  <xsl:template match="db:filename | db:varname | db:envar | db:systemitem">
    <xsl:call-template name="inline-leading-space"/>
    <xsl:text>`</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>`</xsl:text>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <!-- application: italic -->
  <xsl:template match="db:application">
    <xsl:call-template name="inline-leading-space"/>
    <xsl:text>_</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>_</xsl:text>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <!-- function: monospace -->
  <xsl:template match="db:function">
    <xsl:variable name="txt" select="normalize-space(.)"/>
    <xsl:call-template name="inline-leading-space"/>
    <xsl:text>`</xsl:text>
    <xsl:apply-templates/>
    <!-- Only append () if the text does not already end with () -->
    <xsl:if test="not(substring($txt, string-length($txt) - 1) = '()')">
      <xsl:text>()</xsl:text>
    </xsl:if>
    <xsl:text>`</xsl:text>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <!-- parameter: italic monospace -->
  <xsl:template match="db:parameter">
    <xsl:call-template name="inline-leading-space"/>
    <xsl:text>_</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>_</xsl:text>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <!-- userinput: bold (or passthrough when it contains nested inline elements) -->
  <xsl:template match="db:userinput">
    <xsl:call-template name="inline-leading-space"/>
    <xsl:choose>
      <!-- When userinput contains element children (e.g. <command>, <option>),
           don't wrap with bold markers since nested bold markers would produce
           malformed AsciiDoc like **mged* *-c* text*.
           Instead, emit the content directly — the child elements supply markup. -->
      <xsl:when test="*">
        <xsl:apply-templates/>
      </xsl:when>
      <!-- Use unconstrained bold (**...**) so that the markup is recognised
           regardless of adjacent boundary characters.  In particular, <prompt>
           is often immediately followed by <userinput> with no whitespace
           separator (e.g. "mged>*cmd*"), and the constrained form (*cmd*)
           requires whitespace or an explicit boundary character before the
           opening marker.  The unconstrained form works unconditionally. -->
      <xsl:otherwise>
        <xsl:text>**</xsl:text>
        <xsl:apply-templates/>
        <xsl:text>**</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <!-- computeroutput: monospace -->
  <xsl:template match="db:computeroutput">
    <xsl:call-template name="inline-leading-space"/>
    <xsl:text>`</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>`</xsl:text>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <!-- prompt: just output text.
       When the prompt is inside a <term> (e.g. inside a <varlistentry>), DocBook XSL
       preserves the trailing whitespace, so we call inline-trailing-space.
       In plain <para> context, DocBook strips the trailing space during troff rendering,
       so we skip the trailing-space call to match that behaviour. -->
  <xsl:template match="db:prompt">
    <xsl:apply-templates/>
    <xsl:if test="ancestor::db:term">
      <xsl:call-template name="inline-trailing-space"/>
    </xsl:if>
  </xsl:template>

  <!-- quote -->
  <xsl:template match="db:quote">
    <xsl:text>&#8220;</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>&#8221;</xsl:text>
  </xsl:template>

  <!-- acronym, abbrev -->
  <xsl:template match="db:acronym | db:abbrev">
    <xsl:apply-templates/>
  </xsl:template>

  <!-- classname, interfacename, exceptionname: render as monospace -->
  <xsl:template match="db:classname | db:interfacename | db:exceptionname | db:structname">
    <xsl:call-template name="inline-leading-space"/>
    <xsl:text>`</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>`</xsl:text>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <!-- guilabel, guibutton: render as bold (GUI widget label) -->
  <xsl:template match="db:guilabel | db:guibutton | db:guiicon">
    <xsl:call-template name="inline-leading-space"/>
    <xsl:text>*</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>*</xsl:text>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <!-- guimenu, guisubmenu, guimenuitem: render as bold menu path -->
  <xsl:template match="db:guimenu | db:guisubmenu | db:guimenuitem">
    <xsl:call-template name="inline-leading-space"/>
    <xsl:text>*</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>*</xsl:text>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <!-- optional: inline optional argument shown as [text] -->
  <xsl:template match="db:optional">
    <xsl:call-template name="inline-leading-space"/>
    <xsl:text>[</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>]</xsl:text>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <!-- funcsynopsisinfo: verbatim #include or other synopsis preamble -->
  <xsl:template match="db:funcsynopsisinfo">
    <xsl:value-of select="."/>
  </xsl:template>

  <!-- superscript, subscript -->
  <xsl:template match="db:superscript">
    <xsl:call-template name="inline-leading-space"/>
    <xsl:text>^</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>^</xsl:text>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <xsl:template match="db:subscript">
    <xsl:call-template name="inline-leading-space"/>
    <xsl:text>~</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>~</xsl:text>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <!-- ============================================================
       CROSS-REFERENCES AND LINKS
       ============================================================ -->

  <!-- citerefentry: link to another man page -->
  <xsl:template match="db:citerefentry">
    <xsl:call-template name="inline-leading-space"/>
    <xsl:text>*</xsl:text>
    <xsl:value-of select="normalize-space(db:refentrytitle)"/>
    <xsl:text>*(</xsl:text>
    <xsl:value-of select="normalize-space(db:manvolnum)"/>
    <xsl:text>)</xsl:text>
    <xsl:call-template name="inline-trailing-space"/>
  </xsl:template>

  <!-- link -->
  <xsl:template match="db:link">
    <xsl:choose>
      <xsl:when test="@xl:href">
        <xsl:value-of select="@xl:href"/>
        <xsl:if test="normalize-space(.) != ''">
          <xsl:text>[</xsl:text>
          <xsl:apply-templates/>
          <xsl:text>]</xsl:text>
        </xsl:if>
      </xsl:when>
      <xsl:when test="@linkend">
        <xsl:text>&lt;&lt;</xsl:text>
        <xsl:value-of select="@linkend"/>
        <xsl:if test="normalize-space(.) != ''">
          <xsl:text>,</xsl:text>
          <xsl:apply-templates/>
        </xsl:if>
        <xsl:text>&gt;&gt;</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- ulink (DocBook 4 compat) -->
  <xsl:template match="db:ulink">
    <xsl:value-of select="@url"/>
    <xsl:if test="normalize-space(.) != ''">
      <xsl:text>[</xsl:text>
      <xsl:apply-templates/>
      <xsl:text>]</xsl:text>
    </xsl:if>
  </xsl:template>

  <!-- xref -->
  <xsl:template match="db:xref">
    <xsl:text>&lt;&lt;</xsl:text>
    <xsl:value-of select="@linkend"/>
    <xsl:text>&gt;&gt;</xsl:text>
  </xsl:template>

  <!-- uri -->
  <xsl:template match="db:uri">
    <xsl:apply-templates/>
  </xsl:template>

  <!-- email: emit address text directly without angle-bracket wrapping.
       DocBook renders <email> elements without angle brackets in troff output;
       we match that behaviour so the adoc source matches.  Literal < > already
       in the source XML (e.g. &lt;bugs@brlcad.org&gt;) pass through as-is
       and will render with angle brackets in the man page, which is correct. -->
  <xsl:template match="db:email">
    <xsl:apply-templates/>
  </xsl:template>

  <!-- footnote -->
  <xsl:template match="db:footnote">
    <xsl:text>footnote:[</xsl:text>
    <xsl:for-each select="db:para | db:simpara">
      <xsl:if test="position() > 1"><xsl:text> </xsl:text></xsl:if>
      <xsl:apply-templates/>
    </xsl:for-each>
    <xsl:apply-templates select="node()[not(self::db:para or self::db:simpara)]"/>
    <xsl:text>]</xsl:text>
  </xsl:template>

  <xsl:template match="db:footnoteref">
    <xsl:text>footnote:[</xsl:text>
    <xsl:value-of select="@linkend"/>
    <xsl:text>]</xsl:text>
  </xsl:template>

  <!-- ============================================================
       MATHEMATICAL CONTENT
       ============================================================ -->

  <xsl:template match="db:mathphrase | db:inlineequation | db:informalequation">
    <xsl:text>stem:[</xsl:text>
    <xsl:value-of select="normalize-space(.)"/>
    <xsl:text>]</xsl:text>
  </xsl:template>

  <!-- ============================================================
       PROCEDURES (steps)
       ============================================================ -->

  <xsl:template match="db:procedure">
    <xsl:call-template name="block-sep"/>
    <xsl:if test="db:title">
      <xsl:text>.</xsl:text>
      <xsl:value-of select="normalize-space(db:title)"/>
      <xsl:text>&#10;</xsl:text>
    </xsl:if>
    <xsl:apply-templates select="db:step | db:substeps"/>
  </xsl:template>

  <xsl:template match="db:step">
    <xsl:text>. </xsl:text>
    <xsl:apply-templates/>
    <xsl:text>&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="db:substeps">
    <xsl:apply-templates select="db:step"/>
  </xsl:template>

  <!-- task: render the title as a bold paragraph, then the contents -->
  <xsl:template match="db:task">
    <xsl:call-template name="block-sep"/>
    <xsl:if test="db:title">
      <xsl:text>.</xsl:text>
      <xsl:value-of select="normalize-space(db:title)"/>
      <xsl:text>&#10;</xsl:text>
    </xsl:if>
    <xsl:apply-templates select="*[not(self::db:title)]"/>
  </xsl:template>

  <!-- tasksummary, taskprerequisites: render as a note-like block -->
  <xsl:template match="db:tasksummary">
    <xsl:call-template name="block-sep"/>
    <xsl:text>&#10;.Summary&#10;</xsl:text>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="db:taskprerequisites">
    <xsl:call-template name="block-sep"/>
    <xsl:text>&#10;.Prerequisites&#10;</xsl:text>
    <xsl:apply-templates/>
  </xsl:template>

  <!-- taskrelated: render children as a block -->
  <xsl:template match="db:taskrelated">
    <xsl:call-template name="block-sep"/>
    <xsl:apply-templates/>
  </xsl:template>

  <!-- ============================================================
       GLOSSARY
       ============================================================ -->

  <!-- glossterm: when inside a glossentry, render as a dlist term.
       When used inline (inside a para, title, etc.), render as italic. -->
  <xsl:template match="db:glossterm">
    <xsl:choose>
      <xsl:when test="parent::db:glossentry">
        <xsl:apply-templates/>
        <xsl:text>::&#10;</xsl:text>
      </xsl:when>
      <xsl:otherwise>
        <!-- Inline glossterm: render as italic emphasis -->
        <xsl:call-template name="inline-leading-space"/>
        <xsl:text>_</xsl:text>
        <xsl:apply-templates/>
        <xsl:text>_</xsl:text>
        <xsl:call-template name="inline-trailing-space"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- ============================================================
       BIBLIOGRAPHY
       ============================================================ -->

  <xsl:template match="db:bibliography">
    <xsl:text>== Bibliography&#10;&#10;</xsl:text>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="db:biblioentry">
    <xsl:text>* </xsl:text>
    <xsl:apply-templates/>
    <xsl:text>&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="db:biblioid">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="db:citetitle">
    <xsl:text>_</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>_</xsl:text>
  </xsl:template>

  <!-- ============================================================
       XI:INCLUDE / XInclude
       ============================================================ -->

  <!-- xi:include should be processed by xsltproc -xinclude, so elements
       that remain are either errors or already expanded. Ignore xi: namespace. -->
  <xsl:template match="*[namespace-uri()='http://www.w3.org/2001/XInclude']"/>

  <!-- ============================================================
       SKIPPED ELEMENTS (metadata not useful in AsciiDoc output)
       ============================================================ -->

  <xsl:template match="db:colspec | db:spanspec"/>
  <xsl:template match="db:thead"/>  <!-- handled by table template -->
  <xsl:template match="db:tfoot"/>  <!-- handled by table template -->
  <xsl:template match="db:tgroup"/>  <!-- handled by table template - but we need rows -->
  <xsl:template match="db:caption"/>  <!-- handled inline -->

  <!-- abstract: render as a NOTE admonition block so the content is
       visible in the output rather than silently dropped. -->
  <xsl:template match="db:abstract">
    <xsl:text>&#10;[NOTE]&#10;====&#10;</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>====&#10;&#10;</xsl:text>
  </xsl:template>

  <!-- address block: just emit text -->
  <xsl:template match="db:address">
    <xsl:choose>
      <!-- Inline in a paragraph: comma-separate components on a single line. -->
      <xsl:when test="parent::db:para or parent::db:simpara">
        <xsl:for-each select="*">
          <xsl:if test="position() > 1"><xsl:text>, </xsl:text></xsl:if>
          <xsl:value-of select="normalize-space(.)"/>
        </xsl:for-each>
      </xsl:when>
      <!-- Block address: one component per line. -->
      <xsl:otherwise>
        <xsl:apply-templates/>
        <xsl:text>&#10;</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="db:street | db:city | db:state | db:postcode | db:country | db:otheraddr">
    <xsl:choose>
      <xsl:when test="parent::db:address[parent::db:para or parent::db:simpara]">
        <!-- handled by parent address template in inline mode -->
      </xsl:when>
      <xsl:otherwise>
        <xsl:apply-templates/>
        <xsl:text>&#10;</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- ============================================================
       DEFAULT TEXT NODE
       ============================================================ -->

  <!--
    In code/literal contexts, preserve whitespace exactly.
    In all other contexts, normalize whitespace runs to single spaces
    while preserving leading/trailing space characters for word boundaries
    (only when there is adjacent inline content).
  -->

  <!-- Helper key: is an element a block-level DocBook element (one that produces
       its own paragraph/block in the AsciiDoc output)?  We cannot call a template
       from inside a predicate in XSLT 1.0, so we use an explicit test string. -->
  <xsl:variable name="block-element-names">
    programlisting screen literallayout synopsis figure informalfigure
    table informaltable itemizedlist orderedlist variablelist simplelist
    note warning caution tip important blockquote epigraph example
    informalexample mediaobject procedure cmdsynopsis funcsynopsis
    bridgehead para simpara screenshot
    refsection refsect1 refsect2 section chapter appendix preface part
  </xsl:variable>
  <!-- Normalized (single-space) version for reliable contains() tests. -->
  <xsl:variable name="block-element-names-norm"
                select="normalize-space($block-element-names)"/>

  <xsl:template match="text()">
    <xsl:choose>
      <xsl:when test="ancestor::db:programlisting or ancestor::db:screen or
                      ancestor::db:literallayout or ancestor::db:synopsis or
                      ancestor::db:address or ancestor::db:funcsynopsis">
        <xsl:value-of select="."/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:variable name="norm" select="normalize-space(.)"/>
        <!-- Determine whether the immediately preceding sibling element (if any) is
             a block-level DocBook element.  Block elements already end with \n\n so
             no additional inter-element space should be emitted after them. -->
        <xsl:variable name="prev-is-block">
          <xsl:for-each select="preceding-sibling::*[1]">
            <xsl:variable name="local" select="local-name(.)"/>
            <xsl:if test="contains(concat(' ', $block-element-names-norm, ' '),
                                   concat(' ', $local, ' '))">1</xsl:if>
          </xsl:for-each>
        </xsl:variable>
        <!-- Same check for the immediately following sibling element. -->
        <xsl:variable name="next-is-block">
          <xsl:for-each select="following-sibling::*[1]">
            <xsl:variable name="local" select="local-name(.)"/>
            <xsl:if test="contains(concat(' ', $block-element-names-norm, ' '),
                                   concat(' ', $local, ' '))">1</xsl:if>
          </xsl:for-each>
        </xsl:variable>
        <xsl:choose>
          <!-- Pure-whitespace text node between two INLINE sibling ELEMENTS: emit a
               single space so that adjacent inline elements (e.g. *cmd* `file`) are
               not fused together (which would break AsciiDoc constrained markup).
               
               Rules:
               - Require preceding-sibling::* (an element before us, ignoring comments/PIs)
               - Require following-sibling::* (an element after us, ignoring comments/PIs)
               - Skip when a neighbouring element is a block element
               - Only emit for the LAST whitespace text node before the following element
                 (i.e. the immediately-following sibling NODE must be an element, not
                 another text node or comment) to avoid double spaces when multiple
                 whitespace-only text nodes or comments fall between two inline elements. -->
          <xsl:when test="string-length($norm) = 0 and
                          preceding-sibling::* and
                          following-sibling::* and
                          $prev-is-block != '1' and
                          $next-is-block != '1' and
                          following-sibling::node()[1][self::*]">
            <xsl:text> </xsl:text>
          </xsl:when>
          <xsl:otherwise>
            <!-- Only preserve leading space when there IS a preceding INLINE sibling
                 ELEMENT (i.e., we are in the middle of inline content).  Do not emit a
                 space after a block element such as <screen> or <table> because its
                 output already ends with blank lines and the leading space would produce
                 a literal-paragraph marker in AsciiDoc.  Also skip comments/PIs as
                 preceding siblings by using preceding-sibling::* instead of ::node(). -->
            <xsl:if test="preceding-sibling::* and
                          string-length($norm) > 0 and
                          $prev-is-block != '1' and
                          translate(substring(.,1,1),' &#9;&#10;&#13;','') = ''">
              <xsl:text> </xsl:text>
            </xsl:if>
            <xsl:value-of select="$norm"/>
            <!-- Preserve a single trailing space when the immediately-following sibling
                 is an inline element (following-sibling::*[1] exists and is not a block). -->
            <xsl:if test="following-sibling::node()[1][self::*] and
                          string-length(.) > 1 and
                          string-length($norm) > 0 and
                          $next-is-block != '1' and
                          translate(substring(.,string-length(.),1),' &#9;&#10;&#13;','') = ''">
              <xsl:text> </xsl:text>
            </xsl:if>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- ============================================================
       CATCHALL for unhandled elements: process children
       ============================================================ -->
  <xsl:template match="*">
    <xsl:apply-templates/>
  </xsl:template>

</xsl:stylesheet>
