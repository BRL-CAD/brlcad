```
= asciiquack
:doctype: duck
:quack: true

// parsing...

     __
   <(o )___
    ( ._> /
     `---'

----
quack:: true
duck:: ascii
----
```

**asciiquack** is a fast, self-contained C++17 AsciiDoc processor derived from and
mostly compatible with [Asciidoctor](https://asciidoctor.org/).  It converts `.adoc`
source files to HTML5, PDF, DocBook 5, and troff/groff man pages.  It is self contained
and buildable with just a C++17 compiler and standard libraries - no external packages need to
be installed.

## Usage

```
asciiquack [OPTIONS] FILE...
```

Read one or more AsciiDoc source files (or stdin when no file is given) and
write the converted output to a file beside the input (or to stdout with `-o -`).

### Common options

| Option | Description |
|---|---|
| `-b, --backend FORMAT` | Output format: `html5` (default), `pdf`, `docbook5`, `manpage` |
| `-d, --doctype TYPE` | Document type: `article` (default), `book`, `manpage`, `inline` |
| `-a, --attribute NAME[=VALUE]` | Set or override a document attribute |
| `-o, --out-file PATH` | Output file (use `-` for stdout) |
| `-D, --destination-dir DIR` | Output directory when converting multiple files |
| `-B, --base-dir DIR` | Base directory for the document and relative resources |
| `-e, --embedded` | Suppress document wrapper (`<html>`/`<head>`/`<body>`) |
| `-S, --safe-mode MODE` | Safe mode: `unsafe`, `safe`, `server`, `secure` (default: `secure`) |
| `-q, --quiet` | Suppress informational messages |
| `-v, --verbose` | Increase verbosity |
| `-V, --version` | Print version |
| `-h, --help` | Print help |

### Examples

```bash
# HTML5 (default)
asciiquack doc.adoc

# PDF output
asciiquack -b pdf doc.adoc

# PDF with a custom body font
asciiquack -b pdf -a pdf-font=/path/to/MyFont.ttf doc.adoc

# DocBook 5
asciiquack -b docbook5 doc.adoc

# Man page
asciiquack -b manpage doc.adoc

# Embedded fragment (no <html> wrapper)
asciiquack -e doc.adoc

# Read from stdin, write to stdout
echo "Hello _world_." | asciiquack -o -
```


## Output formats

| Backend | Flag | Output |
|---|---|---|
| HTML5 | `-b html5` | Self-contained HTML with inline CSS |
| PDF | `-b pdf` | Portable Document Format |
| DocBook 5 | `-b docbook5` | DocBook 5 XML |
| Man page | `-b manpage` | troff/groff source (`.1`) |


## PDF output

The PDF backend (`-b pdf`) is more limited than asciidoctor's but does
offer a couple of options.

### Page size

```bash
asciiquack -b pdf -a pdf-page-size=A4 doc.adoc   # A4 (default: Letter)
```

### Fonts

By default the PDF uses the [Noto Sans](https://fonts.google.com/noto) fonts
from the repository's `fonts/` directory (path resolved at build time).
You can override any weight/style individually:

| Attribute | Font role |
|---|---|
| `-a pdf-font=PATH` | Body text (regular) |
| `-a pdf-font-bold=PATH` | Bold body text |
| `-a pdf-font-italic=PATH` | Italic body text |
| `-a pdf-font-bold-italic=PATH` | Bold-italic body text |
| `-a pdf-font-mono=PATH` | Monospace (code blocks) |
| `-a pdf-font-mono-bold=PATH` | Bold monospace |

All values accept an absolute or relative path to a TrueType (`.ttf`) font
file.  Each font is embedded as a `/FontFile2` stream; metrics and the
PostScript name are read directly from the font's OS/2 and name tables.

### PDF features

- Letter (8.5″×11″) and A4 page sizes
- Section headings H1–H4, paragraphs, ordered/unordered lists
- Code/listing blocks, admonition blocks, horizontal rules
- Inline bold, italic, monospace markup
- Multi-page layout with automatic page breaks
- Block and inline images (JPEG and PNG)


## Motivation - How We Got Here

Historically, the BRL-CAD documentation system used Docbook and a bundled XML
processing toolchain to produce HTML and man page outputs from a single source.
That system was fully self contained and needed no system infrastructure to
produce its outputs.  That made it a *very* portable and reliable way to manage
documentation, but the Docbook XML format didn't end up being a good fit for
modern online web interfaces.  Even worse, it proved challenging for contributors
to edit Docbook and that inhibited contributions.

[AsciiDoc](https://docs.asciidoctor.org/asciidoc/latest/) looked like a good
alternative, but its technology stack was orthogonal to BRL-CAD's C/C++ centric
implementation - making AsciiDoc processing self contained wasn't possible with
available tools.

Creating a C++ version of asciidoctor was a possibility, but as a manual effort
it would have consumed more time than could be justified (9-12 weeks for basic
functionality + testing was the initial estimate from Copilot).  However, experimentation
in 2026 proved the translation problem was amenable to agentic AI methods.  Which
is how we ended up with asciiquack - largely in the course of a single weekend.

The feature set we're focusing on is the one needed for BRL-CAD documentation.  We
are NOT aiming to duplicate the entirity of asciidoctor's feature set (even with AI,
that would be a large lift for limited payoff.)  Rather, the idea is to implement
what we need to enable AsciiDoc to function in a way similar to how Docbook has
historically functioned in the BRL-CAD ecosystem for our core needs - while also
making us more compatible with modern online documentation technologies.


## Out of scope

The following Asciidoctor features are intentionally not implemented:

| Feature | Reason |
|---|---|
| Extensions API (`register`, `preprocessor`, etc.) | Requires plugin ABI or embedded scripting; tightly coupled to Ruby's object model |
| Markdown-style headings (`#`, `##`, …) | Conflicts with AsciiDoc `#` line-comment; use `=` headings instead |
| Structured sourcemap logging (`:sourcemap:`) | Complex feature with limited practical value |


## Performance

The parser uses a hand-written single-pass block scanner (`block_scanner_hand.c`)
and a hand-written inline scanner (`inline_scanner.hpp`).  No external libraries
or generated code are required on the hot path.

### BRL-CAD corpus (532 files, 50 rounds, GCC 13 `-O3`)

| Processor | µs / file | Files / sec | vs Asciidoctor |
|---|---|---|---|
| Ruby Asciidoctor 2.0.26 | ~2 200 µs | ~450 | 1× (reference) |
| asciiquack | ~357 µs | ~2 800 | **~6.2×** faster |

### Single file (`benchmark/sample-data/mdbasics.adoc`, 334 lines, ~9 KB)

| Processor | Avg / iter | Conv / sec | vs Asciidoctor |
|---|---|---|---|
| Ruby Asciidoctor 2.0.26 | ~2.3 ms | ~440 | 1× (reference) |
| asciiquack | ~0.24 ms | ~4 150 | **~9.5×** faster |


## BRL-CAD corpus compatibility

asciiquack was validated against the full BRL-CAD AsciiDoc corpus (199 files),
comparing output against Asciidoctor 2.0.26.

| Category | Files | Result |
|---|---|---|
| No differences (man pages, most HTML) | 181 / 199 | ✓ identical |
| Block image alt text | 18 / 199 | ✓ improvement — matches Asciidoctor |
| Inline image URL wrapped in `<a href>` | 1 / 199 | ✓ bug fix |
| Malformed asterisks (`*******text*****`) | 3 / 199 | ⚠ benign — all processors diverge on degenerate input |


## Dependencies

All dependencies are vendored; no system packages are required.

| Dependency | Purpose |
|---|---|
| [cxxopts](https://github.com/jarro2783/cxxopts) | Command-line option parsing |
| [minipdf](pdf.hpp) / libharu concepts | Self-contained PDF writer |
| [struetype](https://github.com/starseeker/struetype) | TrueType font parsing (stb_truetype fork) |
| [LodePNG](https://github.com/lvandeve/lodepng) | PNG image decoding |
| [Noto fonts](https://fonts.google.com/noto) | Default PDF fonts (in `fonts/`, path set at build time) |
| [µlight](https://github.com/eisenwave/ulight) | Syntax highlighting (C++23 builds only) |


## References

- AsciiDoc language reference: <https://docs.asciidoctor.org/asciidoc/latest/>
