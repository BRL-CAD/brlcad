#!/usr/bin/env ruby
spec = File.expand_path(File.dirname(__FILE__))
$LOAD_PATH.unshift(spec) if File.exist?(spec)
require 'spec/spec_helper'

lib = File.expand_path(File.join(File.dirname(__FILE__), '..', 'lib'))
$LOAD_PATH.unshift(lib) if File.exist?(lib)

require 'tmpdir'
require 'fileutils'
include Enumerable

require 'rubygems'
require 'spec'

require 'docbook'

$DEBUG = false

TESTDOCSDIR = File.expand_path(File.join(File.dirname(__FILE__), 'testdocs'))

describe DocBook::Epub do

  before do
    @tmpdir = File.join(Dir::tmpdir(), "epubspecsmoke"); Dir.mkdir(@tmpdir) rescue Errno::EEXIST
  end

  # TODO 
  # Known failures on all of:
  #  calloutlist.003.xml
  #  extensions.00[24].xml
  #  programlisting.00[26].xml 
  #  olink.*.xml
  #  cmdsynopsis.002.xml
  #  refentry.007.xml
  #  programlistingco.002.xml
  #  textobject.*.xml
  #
  # The causes of the failures are typically missing extensions in xsltproc
  # (specifically insertfile, for textdata, imagedata, graphic, or inlinegraphic
  # text/XML @filerefs, invalid XHTML 1.1 (block elements inside inlines that 
  # I don't feel like # fixing because I think they're edge cases), callouts 
  # (which are hard in .epub), or test docs I really don't think are cromulent.
  
  # Current passage rate:
  #   224 examples, 12 failures (94.6%)

  Dir["#{TESTDOCSDIR}/[a-z]*.[0-9][0-9][0-9].xml"].each_with_index do |xml_file, ix|
    it "should be able to render a valid .epub for the test document #{xml_file} [#{ix}]" do
      epub = DocBook::Epub.new(xml_file, @tmpdir)
      epub_file = File.join(@tmpdir, File.basename(xml_file, ".xml") + ".epub")
      epub.render_to_file(epub_file, $DEBUG)
      FileUtils.copy(epub_file, ".smt.epub") if $DEBUG
      epub_file.should be_valid_epub  
    end
  end

  after do
    FileUtils.rm_r(@tmpdir, :force => true)
  end  
end
