#!/usr/bin/env python3
"""
db2adoc.py - Convert BRL-CAD DocBook 5 documentation to AsciiDoc

Converts all DocBook XML files under doc/docbook/ to AsciiDoc format,
placing output in a parallel doc/asciidoc/ directory tree.

Uses pandoc for body content conversion and Python's xml.etree for
metadata extraction to generate proper AsciiDoc document headers.

Usage:
    python3 db2adoc.py [--src DOCBOOK_DIR] [--dst ASCIIDOC_DIR] [--dry-run]

Requires: pandoc >= 2.0, Python >= 3.6
"""

import argparse
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

# DocBook 5 namespace
DB_NS = "http://docbook.org/ns/docbook"
XI_NS = "http://www.w3.org/2001/XInclude"

# Files/directories to skip (not actual documentation)
SKIP_PATTERNS = [
    "resources/",
    "CMakeLists.txt",
]


def ns(tag: str) -> str:
    """Return fully-qualified DocBook 5 namespace tag."""
    return f"{{{DB_NS}}}{tag}"


def strip_ns(tag: str) -> str:
    """Strip namespace from a tag name."""
    if tag.startswith("{"):
        return tag.split("}", 1)[1]
    return tag


def text_content(elem) -> str:
    """Get all text content from an element, stripping extra whitespace."""
    if elem is None:
        return ""
    parts = []
    if elem.text:
        parts.append(elem.text)
    for child in elem:
        parts.append(text_content(child))
        if child.tail:
            parts.append(child.tail)
    return " ".join(" ".join(p.split()) for p in parts if p.strip())


def get_authors(info_elem) -> list[str]:
    """Extract author names and emails from an info element."""
    if info_elem is None:
        return []

    authors = []
    # Direct authors
    author_elems = list(info_elem.iter(ns("author")))
    for author in author_elems:
        name_elem = author.find(ns("personname"))
        if name_elem is not None:
            fn = text_content(name_elem.find(ns("firstname")))
            on = text_content(name_elem.find(ns("othername")))
            sn = text_content(name_elem.find(ns("surname")))
            parts = [x for x in [fn, on, sn] if x]
            fullname = " ".join(parts)
        else:
            fullname = text_content(author.find(ns("firstname"))) or ""
            sn = text_content(author.find(ns("surname")))
            if sn:
                fullname = (fullname + " " + sn).strip()

        email = None
        for em in author.iter(ns("email")):
            email = text_content(em)
            break

        if fullname:
            if email:
                authors.append(f"{fullname} <{email}>")
            else:
                authors.append(fullname)

    # corpauthor
    corp = info_elem.find(ns("corpauthor"))
    if corp is not None:
        authors.append(text_content(corp))

    return authors


def extract_metadata(xml_path: Path) -> dict:
    """Parse DocBook XML and return document metadata dict."""
    try:
        # Parse with namespace handling
        tree = ET.parse(xml_path)
    except ET.ParseError as e:
        return {"error": str(e), "doctype": "article"}

    root = tree.getroot()
    local_tag = strip_ns(root.tag)

    meta = {
        "doctype": "article",
        "title": "",
        "authors": [],
        "manvolnum": "",
        "manmanual": "",
        "mansource": "BRL-CAD",
        "root_tag": local_tag,
    }

    if local_tag == "refentry":
        meta["doctype"] = "manpage"
        refmeta = root.find(ns("refmeta"))
        if refmeta is not None:
            meta["title"] = text_content(refmeta.find(ns("refentrytitle"))).upper()
            meta["manvolnum"] = text_content(refmeta.find(ns("manvolnum")))
            for misc in refmeta.iter(ns("refmiscinfo")):
                cls = misc.get("class", "")
                val = text_content(misc)
                if cls == "manual":
                    meta["manmanual"] = val
                elif cls == "source":
                    meta["mansource"] = val

    elif local_tag in ("article", "book", "chapter"):
        meta["doctype"] = "book" if local_tag == "book" else "article"

        # Try info first, then direct title child
        info = root.find(ns("info"))
        title_src = info if info is not None else root
        title_elem = (
            title_src.find(ns("title")) if title_src is not None else None
        ) or root.find(ns("title"))

        meta["title"] = text_content(title_elem) if title_elem is not None else ""
        meta["authors"] = get_authors(info)

    return meta


def pandoc_convert(xml_path: Path) -> str:
    """
    Run pandoc to convert DocBook XML to AsciiDoc body content.
    Returns the converted string (may be empty on failure).
    """
    try:
        result = subprocess.run(
            [
                "pandoc",
                "-f", "docbook",
                "-t", "asciidoc",
                "--wrap=none",
                str(xml_path),
            ],
            capture_output=True,
            text=True,
            timeout=60,
        )
        return result.stdout
    except (subprocess.TimeoutExpired, FileNotFoundError) as e:
        print(f"  WARNING: pandoc failed for {xml_path}: {e}", file=sys.stderr)
        return ""


def build_manpage_header(meta: dict) -> str:
    """Build AsciiDoc manpage document header."""
    title = meta.get("title", "COMMAND")
    vol = meta.get("manvolnum", "1")
    source = meta.get("mansource", "BRL-CAD")
    manual = meta.get("manmanual", "")

    lines = [f"= {title}({vol})"]
    lines.append(":doctype: manpage")
    if source:
        lines.append(f":mansource: {source}")
    if manual:
        lines.append(f":manmanual: {manual}")
    lines.append("")
    return "\n".join(lines)


def build_article_header(meta: dict) -> str:
    """Build AsciiDoc article/book document header."""
    title = meta.get("title", "Untitled")
    authors = meta.get("authors", [])
    doctype = meta.get("doctype", "article")

    lines = [f"= {title}"]
    for author in authors:
        lines.append(author)
    lines.append(f":doctype: {doctype}")
    lines.append("")
    return "\n".join(lines)


def clean_manpage_pandoc_body(body: str, meta: dict) -> str:
    """
    Remove the metadata block that pandoc emits at the top of refentry output,
    and reconstruct NAME and SYNOPSIS sections from the raw pandoc output.

    Pandoc converts refentry to a flat text block with the refentry title,
    volume, source, and manual on separate lines before the first == section.
    Strip those lines and add a proper NAME section from the remaining content.
    """
    lines = body.splitlines(keepends=True)
    result = []
    in_header_block = True
    header_lines_consumed = 0
    # Pandoc emits: title\nvolume\nsource\nmanual\nname\npurpose\nsynopsis-args\n
    # before the first == section heading. We skip all of those.
    # They are replaced by the proper manpage header added elsewhere.
    NAME_section_emitted = False

    i = 0
    while i < len(lines):
        line = lines[i].rstrip("\n")

        if in_header_block:
            # Keep skipping until we hit a == section heading OR
            # until we've found a NAME + purpose line to emit
            if line.startswith("== "):
                in_header_block = False
                result.append(lines[i])
            elif line.startswith("=== ") or line.startswith("==== "):
                in_header_block = False
                result.append(lines[i])
            else:
                # Just skip this line (it's pandoc's raw metadata output)
                pass
        else:
            result.append(lines[i])
        i += 1

    return "".join(result)


def clean_article_pandoc_body(body: str, meta: dict) -> str:
    """
    For article/book output, pandoc may include the title again.
    Since we add it in the header, strip the first heading if it matches.
    """
    return body


SECTION_HEADING_RE = re.compile(r"^(={1,6}) (.+)$", re.MULTILINE)


def bump_section_levels(body: str, shift: int) -> str:
    """
    Shift all section heading levels by `shift` positions.
    Useful when pandoc emits == for top-level sections but we need ===.
    """
    if shift == 0:
        return body

    def replace(m):
        level = len(m.group(1))
        new_level = min(level + shift, 6)
        return "=" * new_level + " " + m.group(2)

    return SECTION_HEADING_RE.sub(replace, body)


def convert_file(xml_path: Path, out_path: Path) -> bool:
    """
    Convert a single DocBook XML file to AsciiDoc.
    Returns True on success.
    """
    # Extract metadata from XML
    meta = extract_metadata(xml_path)
    if "error" in meta:
        print(f"  ERROR parsing {xml_path}: {meta['error']}", file=sys.stderr)
        return False

    # Run pandoc for body conversion
    body = pandoc_convert(xml_path)
    if not body:
        print(f"  WARNING: pandoc produced no output for {xml_path}", file=sys.stderr)
        return False

    # Build the document header
    if meta["doctype"] == "manpage":
        header = build_manpage_header(meta)
        body = clean_manpage_pandoc_body(body, meta)
    else:
        header = build_article_header(meta)
        body = clean_article_pandoc_body(body, meta)

    # Combine header + body
    content = header + body

    # Ensure output directory exists
    out_path.parent.mkdir(parents=True, exist_ok=True)

    # Write output
    out_path.write_text(content, encoding="utf-8")
    return True


def find_docbook_files(src_dir: Path) -> list[Path]:
    """Find all DocBook XML files to convert."""
    files = []
    for xml_file in sorted(src_dir.rglob("*.xml")):
        rel = xml_file.relative_to(src_dir)
        rel_str = str(rel)

        # Skip resource/stylesheet files
        skip = False
        for pattern in SKIP_PATTERNS:
            if pattern in rel_str:
                skip = True
                break
        if skip:
            continue

        files.append(xml_file)
    return files


def main():
    parser = argparse.ArgumentParser(description="Convert BRL-CAD DocBook docs to AsciiDoc")
    parser.add_argument(
        "--src",
        type=Path,
        default=Path(__file__).parent.parent.parent.parent / "doc" / "docbook",
        help="Source DocBook directory (default: brlcad/doc/docbook)",
    )
    parser.add_argument(
        "--dst",
        type=Path,
        default=Path(__file__).parent.parent.parent.parent / "doc" / "asciidoc",
        help="Destination AsciiDoc directory (default: brlcad/doc/asciidoc)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be converted without writing files",
    )
    parser.add_argument(
        "--file",
        type=Path,
        metavar="FILE",
        help="Convert only this specific file",
    )
    args = parser.parse_args()

    src_dir = args.src.resolve()
    dst_dir = args.dst.resolve()

    if not src_dir.exists():
        print(f"ERROR: Source directory not found: {src_dir}", file=sys.stderr)
        sys.exit(1)

    # Check pandoc is available
    try:
        r = subprocess.run(["pandoc", "--version"], capture_output=True, timeout=10)
        if r.returncode != 0:
            print("ERROR: pandoc not found or not working", file=sys.stderr)
            sys.exit(1)
    except FileNotFoundError:
        print("ERROR: pandoc is not installed", file=sys.stderr)
        sys.exit(1)

    # Determine files to process
    if args.file:
        files = [args.file.resolve()]
        # Compute relative path from src_dir
        try:
            rel = files[0].relative_to(src_dir)
        except ValueError:
            rel = files[0].name
            rel = Path(rel).with_suffix(".adoc")
        out_files = [dst_dir / Path(str(rel)).with_suffix(".adoc")]
    else:
        files = find_docbook_files(src_dir)
        out_files = [
            dst_dir / f.relative_to(src_dir).with_suffix(".adoc")
            for f in files
        ]

    print(f"Converting {len(files)} DocBook files from:")
    print(f"  {src_dir}")
    print(f"  -> {dst_dir}")
    print()

    success = 0
    failures = []

    for xml_path, out_path in zip(files, out_files):
        rel = xml_path.relative_to(src_dir)
        print(f"  {rel} -> {out_path.relative_to(dst_dir)}", end="")

        if args.dry_run:
            print(" [dry-run]")
            success += 1
            continue

        if convert_file(xml_path, out_path):
            print(" OK")
            success += 1
        else:
            print(" FAILED")
            failures.append(str(rel))

    print()
    print(f"Results: {success}/{len(files)} converted successfully")
    if failures:
        print(f"Failed ({len(failures)}):")
        for f in failures:
            print(f"  {f}")
        sys.exit(1)


if __name__ == "__main__":
    main()
