dom2dox copies a C++ source file, replacing Doc-O-Matic documentation comments with equivalent Doxygen comments.

Usage: dom2dox input output

Known issues:
* BRL-CAD specific.
* Contains memory leaks and missing checks.
* Removes non-documentation comments, sometimes reformatting code.
* Doesn't preserve indentation in example sections.
* Doesn't guard against documentation appearing inside string literals
* Doesn't guard against against documentation which is commented out.
