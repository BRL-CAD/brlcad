#!/usr/bin/env ruby
spec = File.expand_path(File.dirname(__FILE__))
$LOAD_PATH.unshift(spec) if File.exist?(spec)
require 'spec/spec_helper'

lib = File.expand_path(File.join(File.dirname(__FILE__), '..', 'lib'))
$LOAD_PATH.unshift(lib) if File.exist?(lib)

require 'fileutils'
require 'tmpdir'

require 'rubygems'
require 'spec'

require 'docbook'

$DEBUG = false

TESTDOCSDIR = File.expand_path(File.join(File.dirname(__FILE__), 'files'))

TMPDIR = File.join(Dir::tmpdir(), "epubspecreal"); Dir.mkdir(TMPDIR) rescue Errno::EEXIST

describe DocBook::Epub do
  Dir["#{TESTDOCSDIR}/orm*.[0-9][0-9][0-9].xml"].sort_by { rand }.each do |xml_file|
    epub = DocBook::Epub.new(xml_file, TMPDIR)
    epub_file = File.join(TMPDIR, File.basename(xml_file, ".xml") + ".epub")
    epub.render_to_file(epub_file, $DEBUG)

    FileUtils.copy(epub_file, "." + File.basename(xml_file, ".xml") + ".epub") if $DEBUG

    it "should be able to render a valid .epub for the 'Real Book' test document #{xml_file}" do
      epub_file.should be_valid_epub  
    end

    it "should include cover images in each rendered epub of a 'Real Book' test document like #{xml_file}" do
      cvr_tmpdir = File.join(Dir::tmpdir(), "epubcovers"); Dir.mkdir(cvr_tmpdir) rescue Errno::EEXIST
      system("unzip -q -o -d #{cvr_tmpdir} #{epub_file}")
      cover_grep_lines = `grep --no-filename -c cvr_ #{cvr_tmpdir}/OEBPS/*.html`
      num_covers = cover_grep_lines.split("\n").inject(0) {|sum,n| sum + n.to_i}
      num_covers.should > 0
      FileUtils.rm_r(cvr_tmpdir, :force => true)
    end
  end

  after(:all) do
    FileUtils.rm_r(TMPDIR, :force => true)
  end  
end
