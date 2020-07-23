This is a basic C++ program for reading Git fast-import streams and writing
them back out after doing various operations on the data.  It's not as
generally powerful as something like https://github.com/newren/git-filter-repo
- it was put together to handle a few specific desired post-processing
  operations on the BRL-CAD repository after it's main CVS/SVN -> GIT
conversion was completed.  Features:

* Trim spaces and extra line endings from commit messages

* Wrap long single-line commit messages to 72 characters

* Replace committer ids according to a mapping file

* Append git notes to commit messages and remove the notes (essentially
  preserves data while migrating a repository away from the git notes feature.)


There are a variety of known unimplemented features - work was "done" for
BRL-CAD once our conversion could be handled - but it shouldn't be too hard to
expand if there is need/interest.

Unlike most of the repository conversion code, this will stay in BRL-CAD after
the conversion is complete as a guarantor against future needs to change the
repository (say for example, another migration which requires new email
addresses for proper integration with the hosting platform - that was one issue
encountered with the migration to github.com)

