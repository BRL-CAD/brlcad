<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

  <!-- Include other stylesheets -->
  <xsl:import href="../other/standard/xsl/xhtml/docbook.xsl"/>

   <xsl:template match="*" mode="process.root">
    <xsl:variable name="doc" select="self::*"/>
    <xsl:processing-instruction name="php">
      require('../../wp-blog-header.php');
      class MyPost { var $post_title = "<xsl:apply-templates select="$doc" mode="object.title.markup.textonly"/>"; }
      $wp_query->is_home = false;
      $wp_query->is_single = true;
      $wp_query->queried_object = new MyPost();
      get_header();
    </xsl:processing-instruction>
<div id="#content-side3"><div class="row4">
    <xsl:processing-instruction name="php">
  search_document();    main_menu();
</xsl:processing-instruction>
</div></div>

  <div id="content-side">
  <div id="primary" class="content-area">
    <main id="main" class="site-main" role="main">
      <div class="row3">
      <a name="top"></a>
      <xsl:apply-templates select="."/>
      </div>
    </main>
      </div>
  </div>
<div id='content-side4'><div class='row5'>
  <div id='secondary' class='widget-area' role='complementary'>
    <xsl:processing-instruction name="php">
      if(is_user_logged_in()){
        edit();
      }
      else
      {
        without_login_edit();
      }

    brlcad_language();
    </xsl:processing-instruction>
        <div id="topp"><a href="#top">

    <xsl:processing-instruction name="php">
            up_scroll();
          </xsl:processing-instruction>
        </a></div>
    <xsl:processing-instruction name="php">
    google_languages();

    </xsl:processing-instruction>

  </div>
</div>
</div>
    <xsl:processing-instruction name="php">
      get_footer();
    </xsl:processing-instruction>
  </xsl:template>

</xsl:stylesheet>

