The DocBook files in this directory were converted from original Power
Point presentations.  The intent is to produce from them a text
document in a more article-like form.

The original Power Point presentations are in this directory for
reference by developers and authors.

The transformation process was:

+ use OpenOffice of LibreOffice to convert ppt to xhtml

+ use herold (http://www.dbdoclet.org/projects/herold/index.html) to
  convert xhtml to DocBook

+ use xmllint to fix errors until none remain

+ use 'make doc' and correct DocBook errors until none remain

+ clean up and refine doc until satisfied...

Note that even after DB errors are fixed, there may be tweaking needed
to get an error-free pdf output.
