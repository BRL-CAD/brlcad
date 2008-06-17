lib = File.expand_path(File.join(File.dirname(__FILE__), '..', 'lib'))
$LOAD_PATH.unshift(lib) if File.exist?(lib)

require 'docbook'

class BeValidEpub
  
  def initialize
  end
  
  def matches?(epubfile)
    invalidity = DocBook::Epub.invalid?(epubfile)
    @message = invalidity
    # remember backward logic here
    if invalidity
      return false
    else
      return true
    end
  end
  
  def description
    "be valid .epub"
  end
  
  def failure_message
   " expected .epub file to be valid, but validation produced these errors:\n #{@message}"
  end
  
  def negative_failure_message
    " expected to not be valid, but was (missing validation?)"
  end
end

def be_valid_epub
  BeValidEpub.new
end
