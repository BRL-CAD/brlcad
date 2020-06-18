The directories embedded and other contain the licenses for components that end
up distributed in the BRL-CAD binary packages.  For 3rd party components that
are to be included in a built BRL-CAD binary package in some form (as opposed
to being simply part of the build infrastructure) we keep copies of the
licenses here so they may be bundled and distributed with the built binaries.

The "embedded" directory holds licenses for code that is incorporated in one of
BRL-CAD's libraries, main repository directories, or sometimes directly in
source files.

The "other" directory holds licenses for src/other components that are bundled
and distributed with BRL-CAD but managed as separate libraries or modules
rather than being directly incorporated into the primary BRL-CAD source tree.

We do not add licenses here for source in the tree that is distributed only as
source code.  If a component that is currently build only changes to being
distributed (say, we bundle libxml as a runtime requirement rather than its
current use of building DocBook documentation at build time) we would add the
license to other (and move libxml from the misc/tools directory to src/other)

