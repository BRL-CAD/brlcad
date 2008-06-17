#!/usr/bin/env ruby
spec = File.expand_path(File.dirname(__FILE__))
$LOAD_PATH.unshift(spec) if File.exist?(spec)
require 'spec/spec_helper'

lib = File.expand_path(File.join(File.dirname(__FILE__), '..', 'lib'))
$LOAD_PATH.unshift(lib) if File.exist?(lib)

require 'fileutils'
require 'rexml/document'
require 'tmpdir'

require 'rubygems'
require 'spec'

require 'docbook'

$DEBUG = false

describe DocBook::Epub do
  before(:all) do
    @filedir = File.expand_path(File.join(File.dirname(__FILE__), 'files'))
    @testdocsdir = File.expand_path(File.join(File.dirname(__FILE__), 'testdocs'))
    @tmpdir = File.join(Dir::tmpdir(), "epubregressions"); Dir.mkdir(@tmpdir) rescue Errno::EEXIST
  end

  it "should not include two <itemref>s to the contents of <part>s in the OPF file" do
    part_file = File.join(@testdocsdir, "subtitle.001.xml") 
    epub_file = File.join(@tmpdir, File.basename(part_file, ".xml") + ".epub")
    part_epub = DocBook::Epub.new(part_file, @tmpdir)
    part_epub.render_to_file(epub_file, $DEBUG)

    FileUtils.copy(epub_file, "./.t.epub") if $DEBUG

    itemref_tmpdir = File.join(Dir::tmpdir(), "epubitemref"); Dir.mkdir(itemref_tmpdir) rescue Errno::EEXIST
    system("unzip -q -o -d #{itemref_tmpdir} #{epub_file}")
    opf_file = File.join(itemref_tmpdir, "OEBPS", "content.opf")
    opf = REXML::Document.new(File.new(opf_file))

    itemrefs = REXML::XPath.match(opf, "//itemref").map {|e| e.attributes['idref']}
    itemrefs.should == itemrefs.uniq
  end

  after(:all) do
    FileUtils.rm_r(@tmpdir, :force => true)
  end  
end
