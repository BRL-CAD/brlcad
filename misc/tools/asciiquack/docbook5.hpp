/// @file docbook5.hpp
/// @brief DocBook 5 XML converter for asciiquack.
///
/// Walks the Document AST and produces a valid DocBook 5 XML document that is
/// compatible with the output of the Ruby Asciidoctor docbook5 backend.
///
/// The converter is a pure function: it takes a const Document& and returns
/// a std::string.  All state is held on the call stack.
///
/// Output format: DocBook 5.0 XML (namespace http://docbook.org/ns/docbook)
///   <article> / <book>  – root element based on doctype
///   <info>              – title, author, date
///   <section>           – section divisions (chapters in book doctype)
///   <para>              – paragraphs
///   <itemizedlist>      – unordered lists
///   <orderedlist>       – ordered lists
///   <variablelist>      – description lists
///   <calloutlist>       – callout lists
///   <programlisting>    – source/listing blocks
///   <literallayout>     – literal/verse blocks
///   <note>, <tip>, etc. – admonition blocks
///   <table>             – tables (CALS model)
///   <emphasis>          – bold/italic inline
///   <literal>           – monospace inline
///   <link>              – hyperlinks

#pragma once

#include "document.hpp"
#include "substitutors.hpp"
#include "outbuf.hpp"

#include <string>

namespace asciiquack {

// ─────────────────────────────────────────────────────────────────────────────
// DocBook5Converter
// ─────────────────────────────────────────────────────────────────────────────

class DocBook5Converter {
public:
    DocBook5Converter() = default;

    /// Convert a parsed Document to a DocBook 5 XML string.
    [[nodiscard]] std::string convert(const Document& doc) const {
        OutputBuffer out;
        convert_document(doc, out);
        return out.release();
    }

private:

    // ── XML helpers ───────────────────────────────────────────────────────────

    /// Escape text for XML content (& < > " ').
    [[nodiscard]] static std::string xml_escape(const std::string& text) {
        std::string out;
        out.reserve(text.size() + 16);
        for (char c : text) {
            switch (c) {
                case '&':  out += "&amp;";  break;
                case '<':  out += "&lt;";   break;
                case '>':  out += "&gt;";   break;
                case '"':  out += "&quot;"; break;
                default:   out += c;        break;
            }
        }
        return out;
    }

    /// Apply inline substitutions and convert inline markup to DocBook XML.
    [[nodiscard]] std::string inline_subs(const std::string& text,
                                          const Document&    doc) const {
        // Apply attribute subs then convert markup
        std::string s = sub_attributes(text, doc.attributes());
        return inline_to_docbook(s);
    }

    /// Convert inline AsciiDoc markup to DocBook XML elements.
    [[nodiscard]] static std::string inline_to_docbook(const std::string& text) {
        std::string out;
        out.reserve(text.size() + 32);
        std::size_t i = 0;
        const std::size_t n = text.size();

        while (i < n) {
            // Unconstrained bold: **text**
            if (i + 1 < n && text[i] == '*' && text[i + 1] == '*') {
                std::size_t end = text.find("**", i + 2);
                if (end != std::string::npos) {
                    out += "<emphasis role=\"strong\">";
                    out += xml_escape(text.substr(i + 2, end - (i + 2)));
                    out += "</emphasis>";
                    i = end + 2;
                    continue;
                }
            }
            // Constrained bold: *text*
            if (text[i] == '*' &&
                (i == 0 || !std::isalnum(static_cast<unsigned char>(text[i - 1])))) {
                std::size_t end = text.find('*', i + 1);
                if (end != std::string::npos && end > i + 1 &&
                    (end + 1 >= n || !std::isalnum(static_cast<unsigned char>(text[end + 1])))) {
                    out += "<emphasis role=\"strong\">";
                    out += xml_escape(text.substr(i + 1, end - (i + 1)));
                    out += "</emphasis>";
                    i = end + 1;
                    continue;
                }
            }
            // Unconstrained italic: __text__
            if (i + 1 < n && text[i] == '_' && text[i + 1] == '_') {
                std::size_t end = text.find("__", i + 2);
                if (end != std::string::npos) {
                    out += "<emphasis>";
                    out += xml_escape(text.substr(i + 2, end - (i + 2)));
                    out += "</emphasis>";
                    i = end + 2;
                    continue;
                }
            }
            // Constrained italic: _text_
            if (text[i] == '_' &&
                (i == 0 || !std::isalnum(static_cast<unsigned char>(text[i - 1])))) {
                std::size_t end = text.find('_', i + 1);
                if (end != std::string::npos && end > i + 1 &&
                    (end + 1 >= n || !std::isalnum(static_cast<unsigned char>(text[end + 1])))) {
                    out += "<emphasis>";
                    out += xml_escape(text.substr(i + 1, end - (i + 1)));
                    out += "</emphasis>";
                    i = end + 1;
                    continue;
                }
            }
            // Unconstrained monospace: ``text``
            if (i + 1 < n && text[i] == '`' && text[i + 1] == '`') {
                std::size_t end = text.find("``", i + 2);
                if (end != std::string::npos) {
                    out += "<literal>";
                    out += xml_escape(text.substr(i + 2, end - (i + 2)));
                    out += "</literal>";
                    i = end + 2;
                    continue;
                }
            }
            // Constrained monospace: `text`
            if (text[i] == '`' &&
                (i == 0 || !std::isalnum(static_cast<unsigned char>(text[i - 1])))) {
                std::size_t end = text.find('`', i + 1);
                if (end != std::string::npos && end > i + 1) {
                    out += "<literal>";
                    out += xml_escape(text.substr(i + 1, end - (i + 1)));
                    out += "</literal>";
                    i = end + 1;
                    continue;
                }
            }
            // Superscript: ^text^
            if (text[i] == '^') {
                std::size_t end = text.find('^', i + 1);
                if (end != std::string::npos && end > i + 1) {
                    out += "<superscript>";
                    out += xml_escape(text.substr(i + 1, end - (i + 1)));
                    out += "</superscript>";
                    i = end + 1;
                    continue;
                }
            }
            // Subscript: ~text~
            if (text[i] == '~') {
                std::size_t end = text.find('~', i + 1);
                if (end != std::string::npos && end > i + 1) {
                    out += "<subscript>";
                    out += xml_escape(text.substr(i + 1, end - (i + 1)));
                    out += "</subscript>";
                    i = end + 1;
                    continue;
                }
            }
            // Explicit link macro: link:url[text]
            if (text.compare(i, 5, "link:") == 0) {
                std::size_t bracket = text.find('[', i + 5);
                if (bracket != std::string::npos) {
                    std::size_t end_bracket = text.find(']', bracket + 1);
                    if (end_bracket != std::string::npos) {
                        std::string url   = text.substr(i + 5, bracket - (i + 5));
                        std::string ltext = text.substr(bracket + 1, end_bracket - bracket - 1);
                        out += "<link xlink:href=\"" + xml_escape(url) + "\">";
                        out += xml_escape(ltext.empty() ? url : ltext);
                        out += "</link>";
                        i = end_bracket + 1;
                        continue;
                    }
                }
            }
            // Bare http/https URL
            if (text.compare(i, 7, "http://")  == 0 ||
                text.compare(i, 8, "https://") == 0) {
                std::size_t end = i;
                while (end < n &&
                       !std::isspace(static_cast<unsigned char>(text[end])) &&
                       text[end] != ']') {
                    ++end;
                }
                std::string url = text.substr(i, end - i);
                out += "<link xlink:href=\"" + xml_escape(url) + "\">";
                out += xml_escape(url);
                out += "</link>";
                i = end;
                continue;
            }
            out += xml_escape(std::string(1, text[i]));
            ++i;
        }
        return out;
    }

    // ── DocBook element name for an admonition ────────────────────────────────

    [[nodiscard]] static std::string admonition_elem(const std::string& name) {
        // name is lowercase: "note", "tip", "warning", "important", "caution"
        if (name == "tip")       { return "tip";       }
        if (name == "warning")   { return "warning";   }
        if (name == "important") { return "important"; }
        if (name == "caution")   { return "caution";   }
        return "note";
    }

    // ── Document structure ────────────────────────────────────────────────────

    void convert_document(const Document& doc, OutputBuffer& out) const {
        const std::string& doctype = doc.doctype();
        const std::string  root    = (doctype == "book") ? "book" : "article";

        out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        out << "<" << root
            << " xmlns=\"http://docbook.org/ns/docbook\""
            << " xmlns:xlink=\"http://www.w3.org/1999/xlink\""
            << " version=\"5.0\""
            << " xml:lang=\"" << xml_escape(doc.attr("lang", "en")) << "\"";
        if (!doc.id().empty()) {
            out << " xml:id=\"" << xml_escape(doc.id()) << "\"";
        }
        out << ">\n";

        // <info> block — title, authors, revision date
        out << "<info>\n";
        if (!doc.doctitle().empty()) {
            out << "<title>" << xml_escape(doc.doctitle()) << "</title>\n";
        }
        for (const auto& a : doc.authors()) {
            out << "<author><personname>";
            if (!a.firstname.empty() || !a.lastname.empty()) {
                if (!a.firstname.empty()) {
                    out << "<firstname>" << xml_escape(a.firstname) << "</firstname>";
                }
                if (!a.lastname.empty()) {
                    out << "<surname>" << xml_escape(a.lastname) << "</surname>";
                }
            } else {
                out << xml_escape(a.fullname());
            }
            out << "</personname>";
            if (!a.email.empty()) {
                out << "<email>" << xml_escape(a.email) << "</email>";
            }
            out << "</author>\n";
        }
        const auto& rev = doc.revision();
        if (!rev.date.empty() || !rev.number.empty()) {
            out << "<date>";
            if (!rev.number.empty()) { out << xml_escape(rev.number); }
            if (!rev.number.empty() && !rev.date.empty()) { out << " "; }
            if (!rev.date.empty())   { out << xml_escape(rev.date);   }
            out << "</date>\n";
        }
        out << "</info>\n";

        // Body: blocks and sections
        for (const auto& child : doc.blocks()) {
            convert_block(*child, doc, out);
        }

        out << "</" << root << ">\n";
    }

    void convert_block(const Block& block, const Document& doc,
                       OutputBuffer& out) const {
        switch (block.context()) {
            case BlockContext::Section:
                convert_section(dynamic_cast<const Section&>(block), doc, out);
                break;
            case BlockContext::Preamble:
            case BlockContext::Open:
                for (const auto& child : block.blocks()) {
                    convert_block(*child, doc, out);
                }
                break;
            case BlockContext::Paragraph:
                convert_paragraph(block, doc, out);
                break;
            case BlockContext::Listing:
                convert_listing(block, doc, out);
                break;
            case BlockContext::Literal:
                convert_literal(block, doc, out);
                break;
            case BlockContext::Example:
                convert_example(block, doc, out);
                break;
            case BlockContext::Quote:
                convert_quote(block, doc, out);
                break;
            case BlockContext::Verse:
                convert_verse(block, doc, out);
                break;
            case BlockContext::Sidebar:
                convert_sidebar(block, doc, out);
                break;
            case BlockContext::Admonition:
                convert_admonition(block, doc, out);
                break;
            case BlockContext::Ulist:
                convert_ulist(dynamic_cast<const List&>(block), doc, out);
                break;
            case BlockContext::Olist:
                convert_olist(dynamic_cast<const List&>(block), doc, out);
                break;
            case BlockContext::Dlist:
                convert_dlist(dynamic_cast<const List&>(block), doc, out);
                break;
            case BlockContext::Colist:
                convert_colist(dynamic_cast<const List&>(block), doc, out);
                break;
            case BlockContext::Table:
                convert_table(dynamic_cast<const Table&>(block), doc, out);
                break;
            case BlockContext::Image:
                convert_image(block, doc, out);
                break;
            case BlockContext::Video:
            case BlockContext::Audio:
                convert_media(block, doc, out);
                break;
            case BlockContext::FloatingTitle:
                convert_floating_title(block, doc, out);
                break;
            case BlockContext::Pass:
                // Pass-through: emit raw content verbatim
                out << block.source() << "\n";
                break;
            case BlockContext::ThematicBreak:
                out << "<!-- thematic break -->\n";
                break;
            case BlockContext::PageBreak:
                out << "<?asciidoc-pagebreak?>\n";
                break;
            case BlockContext::Toc:
                // TOC is implicit / auto-generated by DocBook processors
                break;
            default:
                for (const auto& child : block.blocks()) {
                    convert_block(*child, doc, out);
                }
                break;
        }
    }

    void convert_section(const Section& sect, const Document& doc,
                         OutputBuffer& out) const {
        // In book doctype: level-1 sections become <chapter>, others <section>
        const std::string& doctype = doc.doctype();
        std::string elem;
        if (doctype == "book") {
            elem = (sect.level() == 1) ? "chapter" : "section";
        } else {
            elem = "section";
        }

        out << "<" << elem;
        if (!sect.id().empty()) {
            out << " xml:id=\"" << xml_escape(sect.id()) << "\"";
        }
        out << ">\n";
        out << "<title>" << inline_subs(sect.title(), doc) << "</title>\n";

        for (const auto& child : sect.blocks()) {
            convert_block(*child, doc, out);
        }

        out << "</" << elem << ">\n";
    }

    void convert_paragraph(const Block& block, const Document& doc,
                            OutputBuffer& out) const {
        if (block.has_title()) {
            out << "<formalpara>\n";
            out << "<title>" << xml_escape(block.title()) << "</title>\n";
        }
        out << "<para>" << inline_subs(block.source(), doc) << "</para>\n";
        if (block.has_title()) {
            out << "</formalpara>\n";
        }
    }

    void convert_listing(const Block& block, const Document& doc,
                         OutputBuffer& out) const {
        // Language: explicit "language" attr takes precedence, then positional attr 2
        const std::string& lang_explicit = block.attr("language", "");
        const std::string& lang_pos2     = block.attr("2", "");
        const std::string& lang          = lang_explicit.empty() ? lang_pos2 : lang_explicit;
        const bool has_title             = block.has_title();

        if (has_title) {
            out << "<example>\n";
            out << "<title>" << xml_escape(block.title()) << "</title>\n";
        }

        if (!lang.empty()) {
            out << "<programlisting language=\"" << xml_escape(lang) << "\">";
        } else {
            out << "<programlisting>";
        }
        out << xml_escape(block.source());
        out << "</programlisting>\n";

        if (has_title) { out << "</example>\n"; }
        (void)doc;
    }

    void convert_literal(const Block& block, const Document&,
                         OutputBuffer& out) const {
        const bool has_title = block.has_title();
        if (has_title) {
            out << "<example>\n";
            out << "<title>" << xml_escape(block.title()) << "</title>\n";
        }
        out << "<literallayout class=\"monospaced\">"
            << xml_escape(block.source())
            << "</literallayout>\n";
        if (has_title) { out << "</example>\n"; }
    }

    void convert_example(const Block& block, const Document& doc,
                         OutputBuffer& out) const {
        out << "<example>\n";
        if (block.has_title()) {
            out << "<title>" << xml_escape(block.title()) << "</title>\n";
        }
        for (const auto& child : block.blocks()) {
            convert_block(*child, doc, out);
        }
        out << "</example>\n";
    }

    void convert_quote(const Block& block, const Document& doc,
                       OutputBuffer& out) const {
        out << "<blockquote>\n";
        if (block.has_title()) {
            out << "<title>" << xml_escape(block.title()) << "</title>\n";
        }
        const std::string attr = !block.attr("attribution").empty()
                                 ? block.attr("attribution") : block.attr("2");
        const std::string cit  = !block.attr("citetitle").empty()
                                 ? block.attr("citetitle") : block.attr("3");
        if (!attr.empty() || !cit.empty()) {
            out << "<attribution>";
            if (!attr.empty()) { out << xml_escape(attr); }
            if (!cit.empty())  { out << "<citetitle>" << xml_escape(cit) << "</citetitle>"; }
            out << "</attribution>\n";
        }
        if (block.content_model() == ContentModel::Simple) {
            out << "<para>" << inline_subs(block.source(), doc) << "</para>\n";
        } else {
            for (const auto& child : block.blocks()) {
                convert_block(*child, doc, out);
            }
        }
        out << "</blockquote>\n";
    }

    void convert_verse(const Block& block, const Document&,
                       OutputBuffer& out) const {
        out << "<blockquote>\n";
        if (block.has_title()) {
            out << "<title>" << xml_escape(block.title()) << "</title>\n";
        }
        const std::string attr = !block.attr("attribution").empty()
                                 ? block.attr("attribution") : block.attr("2");
        const std::string cit  = !block.attr("citetitle").empty()
                                 ? block.attr("citetitle") : block.attr("3");
        if (!attr.empty() || !cit.empty()) {
            out << "<attribution>";
            if (!attr.empty()) { out << xml_escape(attr); }
            if (!cit.empty())  { out << "<citetitle>" << xml_escape(cit) << "</citetitle>"; }
            out << "</attribution>\n";
        }
        out << "<literallayout>" << xml_escape(block.source()) << "</literallayout>\n";
        out << "</blockquote>\n";
    }

    void convert_sidebar(const Block& block, const Document& doc,
                         OutputBuffer& out) const {
        out << "<sidebar>\n";
        if (block.has_title()) {
            out << "<title>" << xml_escape(block.title()) << "</title>\n";
        }
        for (const auto& child : block.blocks()) {
            convert_block(*child, doc, out);
        }
        out << "</sidebar>\n";
    }

    void convert_admonition(const Block& block, const Document& doc,
                             OutputBuffer& out) const {
        const std::string  elem = admonition_elem(block.attr("name", "note"));
        out << "<" << elem << ">\n";
        if (block.has_title()) {
            out << "<title>" << xml_escape(block.title()) << "</title>\n";
        }
        if (block.content_model() == ContentModel::Simple) {
            out << "<para>" << inline_subs(block.source(), doc) << "</para>\n";
        } else {
            for (const auto& child : block.blocks()) {
                convert_block(*child, doc, out);
            }
        }
        out << "</" << elem << ">\n";
    }

    void convert_ulist(const List& list, const Document& doc,
                       OutputBuffer& out) const {
        out << "<itemizedlist>\n";
        if (list.has_title()) {
            out << "<title>" << xml_escape(list.title()) << "</title>\n";
        }
        for (const auto& item : list.items()) {
            out << "<listitem>\n";
            if (!item->source().empty()) {
                out << "<para>" << inline_subs(item->source(), doc) << "</para>\n";
            }
            for (const auto& child : item->blocks()) {
                convert_block(*child, doc, out);
            }
            out << "</listitem>\n";
        }
        out << "</itemizedlist>\n";
    }

    void convert_olist(const List& list, const Document& doc,
                       OutputBuffer& out) const {
        std::string numeration;
        switch (list.ordered_style()) {
            case OrderedListStyle::Arabic:     numeration = "arabic";     break;
            case OrderedListStyle::LowerAlpha: numeration = "loweralpha"; break;
            case OrderedListStyle::UpperAlpha: numeration = "upperalpha"; break;
            case OrderedListStyle::LowerRoman: numeration = "lowerroman"; break;
            case OrderedListStyle::UpperRoman: numeration = "upperroman"; break;
        }
        out << "<orderedlist numeration=\"" << numeration << "\"";
        const std::string& start_attr = list.attr("start", "");
        if (!start_attr.empty() && start_attr != "1") {
            out << " startingnumber=\"" << xml_escape(start_attr) << "\"";
        }
        out << ">\n";
        if (list.has_title()) {
            out << "<title>" << xml_escape(list.title()) << "</title>\n";
        }
        for (const auto& item : list.items()) {
            out << "<listitem>\n";
            if (!item->source().empty()) {
                out << "<para>" << inline_subs(item->source(), doc) << "</para>\n";
            }
            for (const auto& child : item->blocks()) {
                convert_block(*child, doc, out);
            }
            out << "</listitem>\n";
        }
        out << "</orderedlist>\n";
    }

    void convert_dlist(const List& list, const Document& doc,
                       OutputBuffer& out) const {
        out << "<variablelist>\n";
        if (list.has_title()) {
            out << "<title>" << xml_escape(list.title()) << "</title>\n";
        }
        for (const auto& item : list.items()) {
            out << "<varlistentry>\n";
            out << "<term>" << inline_subs(item->term(), doc) << "</term>\n";
            out << "<listitem>\n";
            if (!item->source().empty()) {
                out << "<para>" << inline_subs(item->source(), doc) << "</para>\n";
            }
            for (const auto& child : item->blocks()) {
                convert_block(*child, doc, out);
            }
            out << "</listitem>\n";
            out << "</varlistentry>\n";
        }
        out << "</variablelist>\n";
    }

    void convert_colist(const List& list, const Document& doc,
                        OutputBuffer& out) const {
        out << "<calloutlist>\n";
        if (list.has_title()) {
            out << "<title>" << xml_escape(list.title()) << "</title>\n";
        }
        for (const auto& item : list.items()) {
            out << "<callout arearefs=\"\">\n";
            if (!item->source().empty()) {
                out << "<para>" << inline_subs(item->source(), doc) << "</para>\n";
            }
            out << "</callout>\n";
        }
        out << "</calloutlist>\n";
    }

    void convert_table(const Table& table, const Document& doc,
                       OutputBuffer& out) const {
        const bool has_title = table.has_title();
        const auto& specs    = table.column_specs();

        // Compute ncols from specs or first row
        std::size_t ncols = specs.size();
        if (ncols == 0) {
            if (!table.head_rows().empty()) {
                ncols = table.head_rows()[0].cells().size();
            } else if (!table.body_rows().empty()) {
                ncols = table.body_rows()[0].cells().size();
            }
        }

        out << (has_title ? "<table" : "<informaltable");
        if (!table.id().empty()) {
            out << " xml:id=\"" << xml_escape(table.id()) << "\"";
        }
        out << ">\n";
        if (has_title) {
            out << "<title>" << xml_escape(table.title()) << "</title>\n";
        }

        out << "<tgroup cols=\"" << ncols << "\">\n";

        for (std::size_t ci = 0; ci < ncols; ++ci) {
            out << "<colspec colname=\"col" << (ci + 1) << "\"";
            if (ci < specs.size() && specs[ci].width > 0) {
                out << " colwidth=\"" << specs[ci].width << "*\"";
            }
            out << "/>\n";
        }

        if (!table.head_rows().empty()) {
            out << "<thead>\n";
            for (const auto& row : table.head_rows()) {
                convert_table_row(row, doc, out);
            }
            out << "</thead>\n";
        }

        out << "<tbody>\n";
        if (!table.body_rows().empty()) {
            for (const auto& row : table.body_rows()) {
                convert_table_row(row, doc, out);
            }
        }
        out << "</tbody>\n";

        if (!table.foot_rows().empty()) {
            out << "<tfoot>\n";
            for (const auto& row : table.foot_rows()) {
                convert_table_row(row, doc, out);
            }
            out << "</tfoot>\n";
        }

        out << "</tgroup>\n";
        out << (has_title ? "</table>\n" : "</informaltable>\n");
    }

    void convert_table_row(const TableRow& row, const Document& doc,
                            OutputBuffer& out) const {
        out << "<row>\n";
        for (const auto& cell : row.cells()) {
            out << "<entry>" << inline_subs(cell->source(), doc) << "</entry>\n";
        }
        out << "</row>\n";
    }

    void convert_image(const Block& block, const Document& doc,
                       OutputBuffer& out) const {
        const std::string& target = block.attr("target", "");
        const std::string& alt    = block.attr("alt",    "");
        const bool has_title      = block.has_title();

        out << (has_title ? "<figure>\n" : "<informalfigure>\n");
        if (has_title) {
            out << "<title>" << xml_escape(block.title()) << "</title>\n";
        }
        out << "<mediaobject>\n"
            << "<imageobject>\n"
            << "<imagedata fileref=\"" << xml_escape(target) << "\"";
        const std::string& w = block.attr("width",  "");
        const std::string& h = block.attr("height", "");
        if (!w.empty()) { out << " width=\""  << xml_escape(w) << "\""; }
        if (!h.empty()) { out << " depth=\""  << xml_escape(h) << "\""; }
        out << "/>\n"
            << "</imageobject>\n";
        if (!alt.empty()) {
            out << "<textobject><phrase>"
                << xml_escape(alt)
                << "</phrase></textobject>\n";
        }
        out << "</mediaobject>\n"
            << (has_title ? "</figure>\n" : "</informalfigure>\n");
        (void)doc;
    }

    void convert_media(const Block& block, const Document& doc,
                       OutputBuffer& out) const {
        // Video/audio have no standard DocBook counterpart; emit a note
        const std::string& target = block.attr("target", "");
        out << "<para role=\"media\">"
            << "<!-- media: " << xml_escape(target) << " -->"
            << "</para>\n";
        (void)doc;
    }

    void convert_floating_title(const Block& block, const Document& doc,
                                 OutputBuffer& out) const {
        // DocBook uses <bridgehead> for floating titles
        const int level = [&]() {
            const std::string& lv = block.attr("level", "");
            if (!lv.empty()) { try { return std::stoi(lv); } catch (...) {} }
            return 2;
        }();
        // Map AsciiDoc section level to DocBook bridgehead renderas attribute
        // (level 1 → sect1, level 2 → sect2, …, level 5 → sect5)
        std::string re = "sect2";  // default
        if (level >= 1 && level <= 5) {
            re = "sect" + std::to_string(level);
        }
        out << "<bridgehead renderas=\"" << re << "\">"
            << inline_subs(block.source(), doc)
            << "</bridgehead>\n";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Free function
// ─────────────────────────────────────────────────────────────────────────────

/// Convert a parsed Document to a DocBook 5 XML string.
[[nodiscard]] inline std::string convert_to_docbook5(const Document& doc) {
    return DocBook5Converter{}.convert(doc);
}

} // namespace asciiquack
