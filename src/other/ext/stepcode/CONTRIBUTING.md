# How to contribute

We love contributions!

## Getting started

* Create a github account if you haven't already, and fork the project
* Create a new branch, using a branch name that gives an idea of what the changes are about
* One topic per commit; a number of small commits are better than one big one
  * Do not introduce whitespace changes! (**Windows users:** `git config --global core.autocrlf true`)
  * Encouraged but not enforced: each commit should stand alone, in the sense that the code should compile and run at that point.
* One major topic per pull request. Commits that fix small things (typos, formatting) are perfectly acceptable in a PR fixing a bug or adding a feature.
  * Tests are good. Tests are required unless you're fixing something simple or that was obviously broken.
* Make your changes and push them to your GitHub repo
* Once your branch is pushed, submit a pull request.
* We'll look at the PR and either merge or add feedback. If there isn't any activity within several days, send a message to the mailing list - `scl-dev` AT `groups.google.com`.

## Coding Standards

SC's source has been reformatted with astyle. When making changes, try
to match the current formatting. The main points are:

  - compact (java-style) brackets:
```C
    if( a == 3 ) {
        c = 5;
        function( a, b );
    } else {
        somefunc();
    }
```
  - indents are 4 spaces
  - no tab characters
  - line endings are LF (linux), not CRLF (windows)
  - brackets around single-line conditionals
  - spaces inside parentheses and around operators
  - return type on the same line as the function name, unless that's
    too long
  - doxygen-style comments
    (see http://www.stack.nl/~dimitri/doxygen/docblocks.html)

If in doubt about a large patch, run astyle with the config file
misc/astyle.cfg.
Download astyle from http://sourceforge.net/projects/astyle/files/astyle/

