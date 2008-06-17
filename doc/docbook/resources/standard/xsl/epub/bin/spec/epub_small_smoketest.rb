#!/usr/bin/env ruby
spec = File.expand_path(File.dirname(__FILE__))
$LOAD_PATH.unshift(spec) if File.exist?(spec)
require 'spec/spec_helper'

lib = File.expand_path(File.join(File.dirname(__FILE__), '..', 'lib'))
$LOAD_PATH.unshift(lib) if File.exist?(lib)

require 'tmpdir'
require 'fileutils'

require 'rubygems'
require 'spec'

require 'docbook'

$DEBUG = false

TESTDOCSDIR = File.expand_path(File.join(File.dirname(__FILE__), 'testdocs'))
NUMBER_TO_TEST = 15

describe DocBook::Epub do

  before do
    @tmpdir = File.join(Dir::tmpdir(), "epubspecsmoke"); Dir.mkdir(@tmpdir) rescue Errno::EEXIST
  end

  Dir["#{TESTDOCSDIR}/*.[0-9][0-9][0-9].xml"].sort_by { rand }[0..(NUMBER_TO_TEST-1)].each do |xml_file|
    it "should be able to render a valid .epub for the test document #{xml_file}" do
      epub = DocBook::Epub.new(xml_file, @tmpdir)
      epub_file = File.join(@tmpdir, File.basename(xml_file, ".xml") + ".epub")
      epub.render_to_file(epub_file, $DEBUG)
      FileUtils.copy(epub_file, "." + File.basename(xml_file, ".xml") + ".epub") if $DEBUG
      epub_file.should be_valid_epub  
    end
  end

  after do
    FileUtils.rm_r(@tmpdir, :force => true)
  end  
end
