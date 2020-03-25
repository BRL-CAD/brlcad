## DESCRIPTION

`hexdump.c` is a single-file C library implementation of the arcane BSD
command-line utility, `hexdump(1)`. It allows specifying simple formatting
programs for display and analysis of binary blobs. For example, the format
specification

```
	"%08.8_ax  " 8/1 "%02x " "  " 8/1 "%02x "
	"  |" 16/1 "%_p" "|\n"
```

produces the more familiar output

```
	00000000  54 68 65 20 71 75 69 63  6b 20 62 72 6f 77 6e 20  |The quick brown |
	00000010  66 6f 78 20 6a 75 6d 70  73 20 6f 76 65 72 20 74  |fox jumps over t|
	00000020  68 65 20 6c 61 7a 79 20  64 6f 67                 |he lazy dog|
```

`hexdump.c` can be built as a simple library, a Lua module, or a
command-line utility; or just dropped into your project without fuss. In
addition to a few bugs (see below), unlike the traditional utility
`hexdump.c` allows specifying the byte order of the converted words. It also
allows processing of odd sized words (e.g. 3 bytes), and in the future will
allow processing of words larger than 4 bytes.

I wrote `hexdump.c` because I kept rewriting fixed format ouput generators
over-and-over, in both C and various scripting languages; for simple
hexadecimal conversion of checksums, analysis of I/O buffers, etc. One bored
day I asked myself, "why not solve this once and for all." `hexdump(1)` is
what I use on the command-line, so I decided to copy its semantics.

Instead of refactoring or otherwise copying the BSD implementation, I took
the opportunity to flex some creative muscle and implement hexdump by
translating the formatting specification to instructions for a simple
virtual machine. I mean... why not, right?

`hexdump.c` is fairly conformant to the manual page description of
`hexdump(1)`. Known bugs include the lack of floating point support (i.e. no
%E, %e, %f, %G, or %g conversions), the inability to handle %_A address
conversions, and in a multiline format string no implicit looping of a
trailing formatting unit to consume the remainder of a block. Because
`hexdump.c` doesn't generate a parse tree of the formatting string, these
latter two are more difficult to support and will have to wait until I have
the patience to add the necessary black magic (i.e. splicing instructions
into the generated code after analyzing more context).

Note that the original BSD implementation contains a typo, printing the
ASCII label "dcl" instead of "dc1" for the %_u conversion of octet 021
(0x11). This typo also manifests in the POSIX `od(1)` utility, which on BSD
systems is implemented with hexdump. I've filed bug reports with FreeBSD,
OpenBSD, NetBSD, Debian, and Dragonfly BSD. Apple appears to have quietly
fixed their copy of BSD hexdump in OS X. (UPDATE: OpenBSD, NetBSD, FreeBSD,
and Debian have fixed this in their respective trunks.)

I'm unaware of any other independent implementations of hexdump. Some Linux
distributions repackage BSD hexdump, although not od, which comes from GNU
core-utils. RedHat's "hexdump" from util-linux appears to be a simple
wrapper to GNU core-utils od, which cannot handle arbitrary formats, yet
which is ironically much larger than BSD `hexdump(1)` or `hexdump.c`, which
belies POSIX's stated reason for excluding hexdump. Solaris doesn't provide
anything named hexdump; just od.


## DEPENDENCIES

None, except that the Lua module requires headers from Lua 5.1 (or LuaJIT),
Lua 5.2, or Lua 5.3.

The source is mostly C99 compliant. The library optionally uses POSIX
`_setjmp`/`_longjmp` instead of C99 `setjmp`/`longjmp`, and the command-line
utility uses POSIX `getopt(3)`. The source should compile cleanly with GCC,
clang, Sun Studio, and Visual Studio (2013 and later).

The Makefile requires GNU Make, however, so it can introspect the
compilation environment.


## BUILDING

There's little reason not to just plop `hexdump.c` and `hexdump.h` into your
own project, and that's how I use the library. Neither of the two source
files will ever be split up. But build rules are included for convenience of
development.

### Build Targets

#### hexdump

Command-line utility substantially like BSD `hexdump(1)`.

#### libhexdump.so

Dynamic library.

#### libhexdump.dylib

Mach-O dynamic library.

#### 5.1/hexdump.so, 5.2/hexdump.so, 5.3/hexdump.so

Lua 5.1, 5.2, and 5.3 modules, respectively.

#### all

Builds hexdump, dynamically selects libhexdump.so or libhexdump.dylib, and
builds Lua modules according to `$(LUA_APIS)`, which can contain a
space-separated list of Lua APIs (e.g. `make LUA_APIS="5.1 5.2 5.3"`). If
empty then `LUA_APIS` is dynamically set by searching for available Lua
headers.


## USAGE

See `hexdump.h` for some documentation. The most concise example of usage of
the C API can be found in `hxdL_apply()` in `hexdump.c`. Just ignore the
parts where the context object is cached. Also see `main()`.


## EXTENSIONS

o Explicit byte order support. By default the native byte order is used when
  loading multibyte words. Currently only big-endian and little-endian is
  supported, however.

o 3-octet-word conversions. BSD `hexdump(1)` only supports loading of 1-, 2-,
  and 4-octet words. In the future I'd also like to support up to 16-octet
  words, which would help debug more complex protocols, like some Microsoft
  protocols which are fond of such objects as 16-byte, little-endian UUIDs.
  Maybe binary words, too.


## BUGS

* No floating point support. In particular, no %E, %e, %f, %G, or %g
  conversions.

* No %_A[dox] conversions. %_a[dox] works fine, however.

* No implicit looping of trailing units to consume the remainder of
  a block. Instead of

  <pre>
  "%08.8_ax  " 8/1 "%02x " "  " 8/1 "%02x "
  "  |" "%_p" "|\n"
  </pre>

  you must write

  <pre>
  "%08.8_ax  " 8/1 "%02x " "  " 8/1 "%02x "
  "  |" 16 "%_p" "|\n"
  </pre>

  Note the additional loop specification, "16", in the second format block.

* Unlike hexdump(1), runs of identical blocks are not skipped by default.
  There's no configuration option to change this, yet, either.

* No locale support for %_c and %_p, although I consider this a feature.
  For equivalent hexdump(1) output you need to do `env LANG=C hexdump ...`.

## LICENSE

Copyright (c) 2013, 2016 William Ahern

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.


<!-- Markdeep: --><style class="fallback">body{visibility:hidden;white-space:pre;font-family:monospace}</style><script src="markdeep.min.js"></script><script src="https://casual-effects.com/markdeep/latest/markdeep.min.js"></script><script>window.alreadyProcessedMarkdeep||(document.body.style.visibility="visible")</script>
