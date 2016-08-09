<xsl:stylesheet
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:d="http://docbook.org/ns/docbook"
  xmlns:xlink="http://www.w3.org/1999/xlink"
  exclude-result-prefixes="d"
  version='1.0'
  >
<xsl:output method = "html"/>

  <xsl:template match="/">
        <xsl:apply-templates select="//d:article"/>
    </xsl:template>

  <xsl:template match="d:article">
  <html><head>
            <meta charset="utf-8"/>
        <meta name="description" content="A framework for easily creating beautiful presentations using HTML"/>
        <meta name="author" content="Hakim El Hattab"/>

        <meta name="apple-mobile-web-app-capable" content="yes" />
        <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent" />

        <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, minimal-ui"/>

        <link rel="stylesheet" href="../../wp-content/themes/brlcad/css/reveal.css"/>
        <link rel="stylesheet" href="../../wp-content/themes/brlcad/css/black.css" id="theme"/>

        <!-- Code syntax highlighting -->
        <link rel="stylesheet" href="../../wp-content/themes/brlcad/css/zenburn.css"/>

    </head><body>
<a href="../../presentations/en/Introduction-brlcad-app-devel.php" style="color:black">Main Site</a>
            <div class="reveal">

            <!-- Any section element inside of this container is displayed as a slide -->
            <div class="slides">

    <xsl:apply-templates/>
    </div>
</div>
        <script src="../../wp-content/themes/brlcad/js/head.min.js"></script>
        <script src="../../wp-content/themes/brlcad/js/reveal.js"></script>

        <script>

            // Full list of configuration options available at:
            // https://github.com/hakimel/reveal.js#configuration
            Reveal.initialize({
                controls: true,
                progress: true,
                history: true,
                center: true,

                transition: 'slide', // none/fade/slide/convex/concave/zoom

                // Optional reveal.js plugins
                dependencies: [
                    { src: '../../wp-content/themes/brlcad/js/classList.js', condition: function() { return !document.body.classList; } },
                    { src: '../../wp-content/themes/brlcad/js/marked.js', condition: function() { return !!document.querySelector( '[data-markdown]' ); } },
                    { src: '../../wp-content/themes/brlcad/js/markdown.js', condition: function() { return !!document.querySelector( '[data-markdown]' ); } },
                    { src: '../../wp-content/themes/brlcad/js/highlight.js', async: true, condition: function() { return !!document.querySelector( 'pre code' ); }, callback: function() { hljs.initHighlightingOnLoad(); } },
                    { src: '../../wp-content/themes/brlcad/js/zoom.js', async: true },
                    { src: '../../wp-content/themes/brlcad/js/notes.js', async: true }
                ]
            });

        </script>

</body></html>


 </xsl:template>
<xsl:template match="@xmlns">
   <xsl:attribute name="href">
      <xsl:value-of select="."/>
   </xsl:attribute>
 </xsl:template>
<xsl:template match="@version">
   <xsl:attribute name="version">
      <xsl:value-of select="."/>
   </xsl:attribute>
 </xsl:template>

	<xsl:template match="d:para">
 <p>
    <xsl:apply-templates/> </p>
 </xsl:template>
 	<xsl:template match="d:itemizedlist">
 <ul>
    <xsl:apply-templates/> </ul>
 </xsl:template>

 	<xsl:template match="d:listitem">
 <li>
    <xsl:apply-templates/> </li>
 </xsl:template>


  <xsl:template match="d:phrase">
 <p>
     <xsl:apply-templates/>
 </p>
 </xsl:template>

 	<xsl:template match="d:title">
 <h1>
     <xsl:apply-templates/>
 </h1>
 </xsl:template>

 	<xsl:template match="d:table">
 <table>
    <xsl:apply-templates/> </table>
 </xsl:template>

 	<xsl:template match="d:thead">
 <thead>
    <xsl:apply-templates/> </thead>
 </xsl:template>

    <xsl:template match="d:row">
 <tr>
    <xsl:apply-templates/> </tr>
 </xsl:template>

    <xsl:template match="d:entry">
 <td>
    <xsl:apply-templates/> </td>
 </xsl:template>

    <xsl:template match="d:tbody">
 <tbody>
    <xsl:apply-templates/> </tbody>
 </xsl:template>

    <xsl:template match="d:programlisting">
 <pre>
    <xsl:apply-templates/> </pre>
 </xsl:template>

    <xsl:template match="d:filename">
 <code>
    <xsl:apply-templates/> </code>
 </xsl:template>

    <xsl:template match="d:emphasis[@role='bold']">
 <b>
    <xsl:apply-templates/> </b>
 </xsl:template>

     <xsl:template match="d:emphasis">
 <i>
    <xsl:apply-templates/> </i>
 </xsl:template>

    <xsl:template match="d:section[@xml:id]">
 <section>
    <xsl:apply-templates/> </section>
 </xsl:template>

    <xsl:template match="d:info/title/application">
 <h1>
    <xsl:apply-templates/> </h1>
 </xsl:template>


    <xsl:template match="d:section/section/title">
 <h2>
    <xsl:apply-templates/> </h2>
 </xsl:template>


    <xsl:template match="d:section">
 <section>
    <xsl:apply-templates/> </section>
 </xsl:template>

    <xsl:template match="d:info">
 <section>
    <xsl:apply-templates/> </section>
 </xsl:template>

    <xsl:template match="d:personname">
 <p>
    <xsl:apply-templates/> </p>
 </xsl:template>

    <xsl:template match="d:imageobject[@role]">
 <section>
    <xsl:apply-templates/> </section>
 </xsl:template>


    <xsl:template match="d:firstname">
 <h5>
    <xsl:apply-templates/> </h5>
 </xsl:template>

    <xsl:template match="d:surname">
 <h5>
    <xsl:apply-templates/> </h5>
 </xsl:template>

    <xsl:template match="d:legalnotice">
 <p>
    <xsl:apply-templates/> </p>
 </xsl:template>

<xsl:template match="d:imagedata">
   <img><xsl:apply-templates select="@*"/></img>
</xsl:template>

<xsl:template match="@width">
   <xsl:attribute name="width">
      <xsl:value-of select="."/>
   </xsl:attribute>
</xsl:template>

<xsl:template match="@depth">
   <xsl:attribute name="height">
      <xsl:value-of select="."/>
   </xsl:attribute>
</xsl:template>

<xsl:template match="@fileref">
   <xsl:attribute name="src">
      <xsl:value-of select="."/>
   </xsl:attribute>
</xsl:template>
   <xsl:template match="@align">
   <xsl:attribute name="align">
      <xsl:value-of select="."/>
   </xsl:attribute>
  </xsl:template>
<xsl:template match="@scalefit">
   <xsl:attribute name="scalefit">
      <xsl:value-of select="."/>
   </xsl:attribute>

</xsl:template>
    <xsl:template match="d:literallayout">
 <pre>
    <xsl:apply-templates/> </pre>
 </xsl:template>

    <xsl:template match="d:imageobject[@role='fo']">
 <hide>
    <xsl:apply-templates/> </hide>
 </xsl:template>

    <xsl:template match="d:section/figure/title">
 <p>
    <xsl:apply-templates/> </p>
 </xsl:template>


<xsl:template match="d:link">
   <a><xsl:apply-templates select="@*"/></a>
</xsl:template>

<xsl:template match="@linkend">
   <xsl:attribute name="href">
      <xsl:value-of select="."/>
   </xsl:attribute>
 </xsl:template>
 <xsl:template match="*[@xlink:href]">
    <a href="{@xlink:href}"><xsl:apply-templates/></a>
  </xsl:template>
</xsl:stylesheet>
