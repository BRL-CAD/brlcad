/// @file html5.hpp
/// @brief HTML 5 converter for asciiquack.
///
/// Walks the Document AST and produces an HTML 5 string that is compatible
/// with the default output of the Ruby Asciidoctor html5 backend.
///
/// The converter is a pure function: it takes a const Document& and returns
/// a std::string.  All state is held on the call stack.

#pragma once

#include "document.hpp"
#include "substitutors.hpp"
#include "outbuf.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

#ifdef ASCIIQUACK_USE_ULIGHT
#include "ulight/ulight.h"
#endif

namespace asciiquack {

#ifdef ASCIIQUACK_USE_ULIGHT
// ─────────────────────────────────────────────────────────────────────────────
// µlight syntax-highlighting helper
// ─────────────────────────────────────────────────────────────────────────────
// highlight_with_ulight() calls the µlight C API to produce HTML for a source
// block.  The returned HTML uses <h- data-h=TYPE>...</h-> spans; see
// ulight_highlight_css() for the matching stylesheet.
//
// Returns an empty string when the language is unknown to µlight, so the
// caller can fall back to plain verbatim output.
//
// The function is compiled as C++17 even though ulight itself was compiled
// as C++23; the two meet only through the plain-C ulight.h ABI.

[[nodiscard]] inline std::string highlight_with_ulight(
    const std::string& source,
    const std::string& language)
{
    // Map the AsciiDoc language name to a ulight language code.
    const ulight_lang lang = ulight_get_lang(language.c_str(), language.size());
    if (lang == ULIGHT_LANG_NONE) {
        return {};  // unknown language → caller uses plain verbatim output
    }

    // Stack-allocated buffers.  ulight flushes them via the callback when full.
    static constexpr std::size_t TOKEN_BUF_SIZE = 4096;
    static constexpr std::size_t TEXT_BUF_SIZE  = 8192;
    ulight_token token_buf[TOKEN_BUF_SIZE];
    char         text_buf[TEXT_BUF_SIZE];

    std::string result;
    result.reserve(source.size() * 3);  // highlighted HTML is typically ~3× raw

    ulight_state state;
    ulight_init(&state);

    state.source        = source.c_str();
    state.source_length = source.size();
    state.lang          = lang;
    // Merge adjacent tokens with the same type for more compact output.
    state.flags         = ULIGHT_COALESCE;

    state.token_buffer        = token_buf;
    state.token_buffer_length = TOKEN_BUF_SIZE;

    state.text_buffer        = text_buf;
    state.text_buffer_length = TEXT_BUF_SIZE;

    // flush_text_data is passed as the first argument to the callback.
    // Cast to const void* here; cast back inside the lambda.
    state.flush_text_data = &result;
    state.flush_text = [](const void* data, char* text, std::size_t length) {
        static_cast<std::string*>(const_cast<void*>(data))->append(text, length);
    };

    const ulight_status status = ulight_source_to_html(&state);
    ulight_destroy(&state);

    if (status != ULIGHT_STATUS_OK) {
        return {};  // highlighting failed → fall back to verbatim
    }

    return result;
}

/// Returns CSS rules that style the <h- data-h=TYPE> spans emitted by µlight.
/// Uses a dark (One-Dark-compatible) palette matching the existing
/// .listingblock pre background (#282c34).
[[nodiscard]] inline std::string ulight_highlight_css()
{
    // The selectors target the non-standard <h-> element by its data-h attribute.
    // Browsers treat unknown elements as inline, so no extra display:inline needed.
    return
        // errors
        "h-[data-h=err]{color:#e06c75}\n"
        // comments
        "h-[data-h=cmt],h-[data-h=cmt_dlim],"
        "h-[data-h=cmt_doc],h-[data-h=cmt_doc_dlim]{color:#7f848e;font-style:italic}\n"
        // strings
        "h-[data-h=str],h-[data-h=str_dlim],"
        "h-[data-h=str_deco]{color:#98c379}\n"
        "h-[data-h=str_esc],h-[data-h=str_intp],"
        "h-[data-h=str_intp_dlim]{color:#56b6c2}\n"
        // numbers
        "h-[data-h=num],h-[data-h=num_dlim],"
        "h-[data-h=num_deco]{color:#d19a66}\n"
        // keywords
        "h-[data-h=kw],h-[data-h=kw_ctrl],"
        "h-[data-h=kw_op],h-[data-h=kw_this]{color:#c678dd}\n"
        "h-[data-h=kw_type]{color:#e5c07b}\n"
        // names / identifiers
        "h-[data-h=name],h-[data-h=name_decl]{color:#abb2bf}\n"
        "h-[data-h=name_fun],h-[data-h=name_fun_decl],"
        "h-[data-h=name_fun_pre]{color:#61afef}\n"
        "h-[data-h=name_type],h-[data-h=name_type_decl],"
        "h-[data-h=name_type_pre]{color:#e5c07b}\n"
        "h-[data-h=name_var],h-[data-h=name_var_decl]{color:#e06c75}\n"
        "h-[data-h=name_cons],h-[data-h=name_cons_decl],"
        "h-[data-h=name_cons_pre]{color:#d19a66}\n"
        "h-[data-h=name_mac],h-[data-h=name_mac_decl],"
        "h-[data-h=name_mac_pre],h-[data-h=name_dirt],"
        "h-[data-h=name_dirt_pre]{color:#56b6c2}\n"
        "h-[data-h=name_life],h-[data-h=name_life_decl]{color:#d19a66}\n"
        "h-[data-h=name_attr],h-[data-h=name_attr_decl]{color:#e5c07b}\n"
        "h-[data-h=name_inst],h-[data-h=name_inst_decl],"
        "h-[data-h=asm_inst_pre]{color:#56b6c2}\n"
        "h-[data-h=name_labl],h-[data-h=name_labl_decl]{color:#e06c75}\n"
        "h-[data-h=name_para],h-[data-h=name_para_decl]{color:#e06c75}\n"
        "h-[data-h=name_cmd],h-[data-h=name_opt]{color:#61afef}\n"
        "h-[data-h=name_nt],h-[data-h=name_nt_decl]{color:#56b6c2}\n"
        // symbols / operators / punctuation
        "h-[data-h=sym_op]{color:#56b6c2}\n"
        "h-[data-h=sym_punc]{color:#abb2bf}\n"
        "h-[data-h=sym_brac],h-[data-h=sym_par],"
        "h-[data-h=sym_sqr],h-[data-h=sym_bket]{color:#abb2bf}\n"
        // diff
        "h-[data-h=diff_del],h-[data-h=diff_del_dlim]{color:#e06c75}\n"
        "h-[data-h=diff_ins],h-[data-h=diff_ins_dlim]{color:#98c379}\n"
        "h-[data-h=diff_head],h-[data-h=diff_head_dlim],"
        "h-[data-h=diff_head_hunk],h-[data-h=diff_head_hunk_dlim]{color:#7f848e}\n"
        "h-[data-h=diff_mod],h-[data-h=diff_mod_dlim]{color:#d19a66}\n"
        // markup (HTML/XML)
        "h-[data-h=mk_tag],h-[data-h=mk_tag_decl],"
        "h-[data-h=mk_tag_dlim]{color:#e06c75}\n"
        "h-[data-h=mk_attr],h-[data-h=mk_attr_decl]{color:#d19a66}\n"
        "h-[data-h=mk_attr_pre]{color:#61afef}\n";
}
#endif  // ASCIIQUACK_USE_ULIGHT

// ─────────────────────────────────────────────────────────────────────────────
// Html5Converter
// ─────────────────────────────────────────────────────────────────────────────

class Html5Converter {
public:
    Html5Converter() = default;

    /// Convert a parsed Document to an HTML 5 string.
    [[nodiscard]] std::string convert(const Document& doc) const {
        // Reset per-conversion state
        counters_.clear();
        footnotes_.clear();
        OutputBuffer out;
        convert_document(doc, out);
        return out.release();
    }

private:
    // ── Per-conversion mutable state ──────────────────────────────────────────
    mutable std::unordered_map<std::string, int> counters_;   ///< counter: macros
    mutable std::vector<FootnoteEntry>           footnotes_;  ///< footnote: macros

    // ── Shorthand helpers ─────────────────────────────────────────────────────

    /// Apply normal substitutions to inline text (with counter/footnote ctx).
    [[nodiscard]] std::string subs(const std::string& text,
                                   const Document&    doc) const {
        InlineContext ctx{doc.attributes(), &counters_, &footnotes_};
        return apply_normal_subs(text, doc.attributes(), &ctx);
    }

    /// Escape HTML characters only (for code / literal content).
    [[nodiscard]] static std::string verbatim(const std::string& text) {
        return apply_verbatim_subs(text);
    }

    // ── Document structure ────────────────────────────────────────────────────

    void convert_document(const Document& doc, OutputBuffer& out) const {
        bool embedded = doc.has_attr("embedded");

        if (!embedded) {
            const std::string& encoding = doc.attr("encoding", "UTF-8");
            const std::string& lang     = doc.attr("lang",     "en");

            out << "<!DOCTYPE html>\n";
            out << "<html lang=\"" << lang << "\">\n";
            out << "<head>\n";
            out << "<meta charset=\"" << encoding << "\">\n";
            out << "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">\n";
            out << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";

            // Generator meta (omit when reproducible is set)
            if (!doc.has_attr("reproducible")) {
                out << "<meta name=\"generator\" content=\"asciiquack "
                    << doc.attr("asciidoctor-version", "0.1.0") << "\">\n";
            }

            // Optional meta tags
            if (doc.has_attr("description")) {
                out << "<meta name=\"description\" content=\""
                    << doc.attr("description") << "\">\n";
            }
            if (doc.has_attr("keywords")) {
                out << "<meta name=\"keywords\" content=\""
                    << doc.attr("keywords") << "\">\n";
            }
            if (doc.has_attr("authors")) {
                out << "<meta name=\"author\" content=\""
                    << doc.attr("authors") << "\">\n";
            }

            // Title
            std::string page_title = doc.doctitle();
            if (page_title.empty()) { page_title = doc.attr("untitled-label", "Untitled"); }
            out << "<title>" << sub_specialchars(page_title) << "</title>\n";

            // Stylesheet: link to external file or inline the default
            if (doc.has_attr("linkcss")) {
                // External stylesheet link
                const std::string& ss = doc.attr("stylesheet");
                if (!ss.empty()) {
                    out << "<link rel=\"stylesheet\" href=\"" << ss << "\">\n";
                } else {
                    // Default: link to asciiquack.css sibling
                    out << "<link rel=\"stylesheet\" href=\"./asciiquack.css\">\n";
                }
            } else {
                out << "<style>\n"
                    << default_css()
                    << "</style>\n";
            }

            // docinfo: head fragment (injected from docinfo.html in unsafe mode)
            inject_docinfo(doc, "head", out);

            // stem/MathJax: include loader script if document has stem content
            if (doc.has_attr("stem")) {
                const std::string& stem_type = doc.attr("stem", "latexmath");
                if (stem_type == "latexmath" || stem_type == "asciimath" || stem_type.empty() || stem_type == "mathjax") {
                    out << "<script>window.MathJax={tex:{inlineMath:[['\\\\(','\\\\)']],"
                        << "displayMath:[['\\\\[','\\\\]']]}};</script>\n"
                        << "<script async src=\"https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js\"></script>\n";
                }
            }

            out << "</head>\n";

            const std::string& doctype = doc.doctype();
            out << "<body class=\"" << doctype << "\">\n";

            // Header div
            out << "<div id=\"header\">\n";
            if (!doc.doctitle().empty()) {
                if (doctype == "manpage") {
                    // Man page: build title as "<manname>(<manvolnum>)".
                    // We use manname + manvolnum rather than doctitle() to avoid
                    // duplication: the document title "RT(1)" already contains
                    // the section number, and manvolnum is also "1", so appending
                    // manvolnum to doctitle() would produce "RT(1)(1)".
                    const std::string& manname   = doc.attr("manname");
                    const std::string& manvolnum = doc.attr("manvolnum");
                    std::string manpage_title;
                    if (!manname.empty()) {
                        manpage_title = sub_specialchars(manname);
                        if (!manvolnum.empty()) {
                            manpage_title += "(" + sub_specialchars(manvolnum) + ")";
                        }
                    } else {
                        // Fallback: doctitle already contains the full "name(vol)" form
                        manpage_title = sub_specialchars(doc.doctitle());
                    }
                    out << "<h1>" << manpage_title << " Manual Page</h1>\n";
                } else {
                    out << "<h1>" << subs(doc.doctitle(), doc) << "</h1>\n";
                }
            }

            const auto& authors = doc.authors();
            if (!authors.empty()) {
                out << "<div class=\"details\">\n";
                for (std::size_t i = 0; i < authors.size(); ++i) {
                    const auto& a    = authors[i];
                    std::string sfx  = (i == 0) ? "" : std::to_string(i + 1);
                    out << "<span id=\"author"  << sfx << "\" class=\"author\">"
                        << sub_specialchars(a.fullname()) << "</span>";
                    if (!a.email.empty()) {
                        out << "<br>\n"
                            << "<span id=\"email" << sfx << "\" class=\"email\">"
                            << "<a href=\"mailto:" << a.email << "\">"
                            << sub_specialchars(a.email) << "</a></span>";
                    }
                    out << "<br>\n";
                }

                const auto& rev = doc.revision();
                if (!rev.number.empty()) {
                    out << "<span id=\"revnumber\">version " << sub_specialchars(rev.number);
                    if (!rev.date.empty()) { out << ","; }
                    out << "</span>\n";
                }
                if (!rev.date.empty()) {
                    out << "<span id=\"revdate\">" << sub_specialchars(rev.date) << "</span>\n";
                }
                if (!rev.remark.empty()) {
                    out << "<br>\n"
                        << "<span id=\"revremark\">" << sub_specialchars(rev.remark) << "</span>\n";
                }
                out << "</div>\n";
            }
            out << "</div>\n";  // #header

            // TOC placement: auto (in header) or preamble (after preamble)
            if (doc.has_attr("toc")) {
                const std::string& placement =
                    doc.attr("toc-placement", "auto");
                if (placement == "auto" || placement.empty()) {
                    convert_toc(doc, out);
                }
            }

            out << "<div id=\"content\">\n";
        }

        // ── Preamble ────────────────────────────────────────────────────────────
        // Blocks that appear before the first section are the preamble.
        // The preamble <div> is only emitted when there are sections following
        // (matching Ruby Asciidoctor behaviour).
        std::vector<const Block*> preamble_blocks;
        std::vector<const Block*> section_blocks;

        for (const auto& child : doc.blocks()) {
            if (child->context() == BlockContext::Section) {
                section_blocks.push_back(child.get());
            } else if (section_blocks.empty()) {
                preamble_blocks.push_back(child.get());
            } else {
                section_blocks.push_back(child.get());
            }
        }

        // Only wrap in preamble div when sections follow.  If there are no
        // sections the content is rendered directly into #content.
        const bool has_sections = !section_blocks.empty();

        if (!preamble_blocks.empty()) {
            if (has_sections) {
                out << "<div id=\"preamble\">\n";
                out << "<div class=\"sectionbody\">\n";
            }
            for (const Block* b : preamble_blocks) {
                convert_block(*b, doc, out);
            }
            if (has_sections) {
                out << "</div>\n";  // sectionbody
                out << "</div>\n";  // #preamble
            }

            // TOC after preamble (toc-placement: preamble)
            if (!embedded && doc.has_attr("toc")) {
                const std::string& placement = doc.attr("toc-placement", "auto");
                if (placement == "preamble") {
                    convert_toc(doc, out);
                }
            }
        }

        for (const Block* b : section_blocks) {
            convert_block(*b, doc, out);
        }

        if (!embedded) {
            out << "</div>\n";  // #content

            // ── Footnotes section ────────────────────────────────────────────
            if (!footnotes_.empty()) {
                convert_footnotes(footnotes_, doc, out);
            }

            out << "<div id=\"footer\">\n";
            out << "<div id=\"footer-text\">\n";
            const auto& rev = doc.revision();
            if (!rev.number.empty()) {
                out << "Version " << sub_specialchars(rev.number) << "<br>\n";
            }
            out << "</div>\n";  // #footer-text
            out << "</div>\n";  // #footer

            // docinfo: footer fragment
            inject_docinfo(doc, "footer", out);

            out << "</body>\n";
            out << "</html>\n";
        }
    }

    // ── Generic block dispatcher ───────────────────────────────────────────────

    void convert_block(const Block& block, const Document& doc,
                       OutputBuffer& out) const {
        switch (block.context()) {
            case BlockContext::Section:
                convert_section(dynamic_cast<const Section&>(block), doc, out);
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
                convert_quote(block, doc, out, false);
                break;
            case BlockContext::Verse:
                convert_quote(block, doc, out, true);
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
                convert_video(block, doc, out);
                break;
            case BlockContext::Audio:
                convert_audio(block, doc, out);
                break;
            case BlockContext::FloatingTitle:
                convert_floating_title(block, doc, out);
                break;
            case BlockContext::Toc:
                convert_toc(doc, out);
                break;
            case BlockContext::Pass: {
                // Check if this is a stem block
                const std::string& s = block.style();
                if (s == "stem" || s == "latexmath") {
                    // Display math: \[...\]
                    out << "<div class=\"stemblock\">\n"
                        << "<div class=\"content\">\n"
                        << "\\[" << block.source() << "\\]\n"
                        << "</div>\n"
                        << "</div>\n";
                } else if (s == "asciimath") {
                    out << "<div class=\"stemblock\">\n"
                        << "<div class=\"content\">\n"
                        << "`" << block.source() << "`\n"
                        << "</div>\n"
                        << "</div>\n";
                } else {
                    // Raw passthrough: emit content without any substitutions
                    out << block.source() << '\n';
                }
                break;
            }
            case BlockContext::ThematicBreak:
                out << "<hr>\n";
                break;
            case BlockContext::PageBreak:
                out << "<div style=\"page-break-after: always;\"></div>\n";
                break;
            default:
                // Recurse into compound children
                for (const auto& child : block.blocks()) {
                    convert_block(*child, doc, out);
                }
                break;
        }
    }

    // ── Section ───────────────────────────────────────────────────────────────

    void convert_section(const Section& sect, const Document& doc,
                         OutputBuffer& out) const {
        int level = sect.level();  // 0 = document title level, 1 = h2, …
        // level 1 (==) → class="sect1", <h2>
        // level 2 (===) → class="sect2", <h3>
        // etc.
        int depth = level;
        std::string tag = "h" + std::to_string(std::min(level + 1, 6));

        // CSS class: use sectname for special sections, otherwise "sectN"
        std::string css;
        if (sect.special() && !sect.sectname().empty()) {
            css = sect.sectname();
        } else {
            css = "sect" + std::to_string(depth);
        }

        out << "<div class=\"" << css << "\">\n";

        const std::string& id = sect.id();

        // Build title text with optional section number prefix
        std::string title_html;
        if (sect.numbered() && !sect.sectnum_string().empty()) {
            title_html = "<span class=\"sectnum\">"
                       + sect.sectnum_string()
                       + "</span> "
                       + subs(sect.title(), doc);
        } else {
            title_html = subs(sect.title(), doc);
        }

        if (!id.empty()) {
            out << "<" << tag << " id=\"" << id << "\">"
                << title_html
                << "</" << tag << ">\n";
        } else {
            out << "<" << tag << ">"
                << title_html
                << "</" << tag << ">\n";
        }

        out << "<div class=\"sectionbody\">\n";
        for (const auto& child : sect.blocks()) {
            convert_block(*child, doc, out);
        }
        out << "</div>\n";  // sectionbody
        out << "</div>\n";  // sect<N>
    }

    // ── Helper: emit an optional id="..." attribute ────────────────────────────

    static void emit_id_attr(const Block& block, OutputBuffer& out) {
        const std::string& id = block.id();
        if (!id.empty()) { out << " id=\"" << id << "\""; }
    }

    // ── Paragraph ─────────────────────────────────────────────────────────────

    void convert_paragraph(const Block& block, const Document& doc,
                           OutputBuffer& out) const {
        std::string role = block.role();
        out << "<div class=\"paragraph";
        if (!role.empty()) { out << " " << role; }
        out << "\"";
        emit_id_attr(block, out);
        out << ">\n";

        if (block.has_title()) {
            out << "<div class=\"title\">" << subs(block.title(), doc) << "</div>\n";
        }

        out << "<p>" << subs(block.source(), doc) << "</p>\n";
        out << "</div>\n";
    }

    // ── Listing / source block ─────────────────────────────────────────────────

    void convert_listing(const Block& block, const Document& doc,
                         OutputBuffer& out) const {
        const std::string& style = block.style();
        // Language: explicit "language" attr takes precedence, then the second
        // positional attribute (set by [source,<lang>] block attribute lines).
        const std::string& lang_explicit = block.attr("language");
        const std::string& lang_pos2     = block.attr("2");
        const std::string& language = lang_explicit.empty() ? lang_pos2 : lang_explicit;
        bool is_source = (style == "source") || !language.empty();

        out << "<div class=\"listingblock\"";
        emit_id_attr(block, out);
        out << ">\n";
        if (block.has_title()) {
            out << "<div class=\"title\">" << subs(block.title(), doc) << "</div>\n";
        }
        out << "<div class=\"content\">\n";

        if (is_source && !language.empty()) {
#ifdef ASCIIQUACK_USE_ULIGHT
            // Attempt syntax highlighting via µlight.  On success the returned
            // HTML already contains &lt;/&gt; escaping, so we run callout
            // substitution directly on it.  On failure (language unknown to
            // µlight) we fall through to the plain-verbatim path below.
            std::string hl = highlight_with_ulight(block.source(), language);
            if (!hl.empty()) {
                std::string src_html = substitute_callouts(hl, block.id());
                out << "<pre class=\"highlight\"><code class=\"language-" << language
                    << "\" data-lang=\"" << language << "\">"
                    << src_html
                    << "</code></pre>\n";
                out << "</div>\n";  // content
                out << "</div>\n";  // listingblock
                return;
            }
#endif  // ASCIIQUACK_USE_ULIGHT
            // Fallback: plain verbatim with HTML escaping (no syntax colours).
            std::string escaped  = verbatim(block.source());
            std::string src_html = substitute_callouts(escaped, block.id());
            out << "<pre class=\"highlight\"><code class=\"language-" << language
                << "\" data-lang=\"" << language << "\">"
                << src_html
                << "</code></pre>\n";
        } else {
            // Apply verbatim escaping, then callout substitution
            std::string escaped  = verbatim(block.source());
            std::string src_html = substitute_callouts(escaped, block.id());
            out << "<pre>" << src_html << "</pre>\n";
        }

        out << "</div>\n";  // content
        out << "</div>\n";  // listingblock
    }

    // ── Literal block ──────────────────────────────────────────────────────────

    void convert_literal(const Block& block, const Document& doc,
                         OutputBuffer& out) const {
        out << "<div class=\"literalblock\"";
        emit_id_attr(block, out);
        out << ">\n";
        if (block.has_title()) {
            out << "<div class=\"title\">" << subs(block.title(), doc) << "</div>\n";
        }
        out << "<div class=\"content\">\n"
            << "<pre>" << verbatim(block.source()) << "</pre>\n"
            << "</div>\n"
            << "</div>\n";
    }

    // ── Example block ──────────────────────────────────────────────────────────

    void convert_example(const Block& block, const Document& doc,
                         OutputBuffer& out) const {
        out << "<div class=\"exampleblock\"";
        emit_id_attr(block, out);
        out << ">\n";
        if (block.has_title()) {
            int n = ++counters_["example"];
            out << "<div class=\"title\">Example " << n << ". "
                << subs(block.title(), doc) << "</div>\n";
        }
        out << "<div class=\"content\">\n";
        for (const auto& child : block.blocks()) {
            convert_block(*child, doc, out);
        }
        out << "</div>\n"
            << "</div>\n";
    }

    // ── Quote / verse ──────────────────────────────────────────────────────────

    void convert_quote(const Block& block, const Document& doc,
                       OutputBuffer& out, bool verse) const {
        // Named attrs take precedence; fall back to positional attrs:
        // [quote, attribution, citetitle] → positional "2" and "3".
        const std::string& attribution = !block.attr("attribution").empty()
                                         ? block.attr("attribution")
                                         : block.attr("2");
        const std::string& citetitle   = !block.attr("citetitle").empty()
                                         ? block.attr("citetitle")
                                         : block.attr("3");

        out << "<div class=\"" << (verse ? "verseblock" : "quoteblock") << "\"";
        emit_id_attr(block, out);
        out << ">\n";
        if (block.has_title()) {
            out << "<div class=\"title\">" << subs(block.title(), doc) << "</div>\n";
        }

        out << "<blockquote>\n";

        if (verse) {
            out << "<pre class=\"content\">" << subs(block.source(), doc) << "</pre>\n";
        } else {
            for (const auto& child : block.blocks()) {
                convert_block(*child, doc, out);
            }
        }

        out << "</blockquote>\n";

        if (!attribution.empty() || !citetitle.empty()) {
            out << "<div class=\"attribution\">\n";
            if (!attribution.empty()) {
                out << "&#8212; " << subs(attribution, doc);
            }
            if (!citetitle.empty()) {
                if (!attribution.empty()) { out << "<br>\n"; }
                out << "<cite>" << subs(citetitle, doc) << "</cite>";
            }
            out << "\n</div>\n";
        }
        out << "</div>\n";
    }

    // ── Sidebar ────────────────────────────────────────────────────────────────

    void convert_sidebar(const Block& block, const Document& doc,
                         OutputBuffer& out) const {
        out << "<div class=\"sidebarblock\"";
        emit_id_attr(block, out);
        out << ">\n"
            << "<div class=\"content\">\n";
        if (block.has_title()) {
            out << "<div class=\"title\">" << subs(block.title(), doc) << "</div>\n";
        }
        for (const auto& child : block.blocks()) {
            convert_block(*child, doc, out);
        }
        out << "</div>\n"
            << "</div>\n";
    }

    // ── Admonition ────────────────────────────────────────────────────────────

    void convert_admonition(const Block& block, const Document& doc,
                            OutputBuffer& out) const {
        // attr(name, fallback) returns by value – safe even when the attribute
        // is absent (no dangling reference).
        const std::string name = block.attr("name", "note");

        // Determine caption priority:
        // 1. Document locale attribute (e.g. :note-caption: Nota) – user i18n
        // 2. Block-level caption attribute set by user in block attributes
        // 3. Built-in English default
        std::string caption;
        const std::string locale_key = name + "-caption";
        if (doc.has_attr(locale_key)) {
            caption = doc.attr(locale_key);
        } else if (block.has_attr("caption")) {
            // Block-level caption (set explicitly by user or parser)
            // Only honour if it differs from the default label name
            const std::string& bc = block.attr("caption");
            // Parser sets caption = uppercase label (NOTE, TIP, …)
            // User may override in block attrs: [caption="My Title"]
            // We use the block caption only as-is here; locale takes priority above
            caption = bc;
        }

        // If still empty, use built-in defaults
        if (caption.empty()) {
            if      (name == "tip")       { caption = "Tip"; }
            else if (name == "warning")   { caption = "Warning"; }
            else if (name == "important") { caption = "Important"; }
            else if (name == "caution")   { caption = "Caution"; }
            else                          { caption = "Note"; }
        } else if (caption == "NOTE" || caption == "TIP" ||
                   caption == "WARNING" || caption == "IMPORTANT" ||
                   caption == "CAUTION") {
            // Parser-set raw uppercase label: translate to Title Case default
            if      (caption == "TIP")       { caption = "Tip"; }
            else if (caption == "WARNING")   { caption = "Warning"; }
            else if (caption == "IMPORTANT") { caption = "Important"; }
            else if (caption == "CAUTION")   { caption = "Caution"; }
            else                             { caption = "Note"; }
        }

        out << "<div class=\"admonitionblock " << name << "\">\n"
            << "<table><tr>\n"
            << "<td class=\"icon\">\n"
            << "<div class=\"title\">" << sub_specialchars(caption) << "</div>\n"
            << "</td>\n"
            << "<td class=\"content\">\n";

        if (block.has_title()) {
            out << "<div class=\"title\">" << subs(block.title(), doc) << "</div>\n";
        }

        if (block.content_model() == ContentModel::Simple) {
            out << subs(block.source(), doc) << "\n";
        } else {
            for (const auto& child : block.blocks()) {
                convert_block(*child, doc, out);
            }
        }

        out << "</td>\n"
            << "</tr></table>\n"
            << "</div>\n";
    }

    // ── Unordered list ────────────────────────────────────────────────────────

    void convert_ulist(const List& list, const Document& doc,
                       OutputBuffer& out) const {
        const std::string& role  = list.role();
        const std::string& style = list.style();  // checklist, etc.

        out << "<div class=\"ulist";
        if (!style.empty()) { out << " " << style; }
        if (!role.empty())  { out << " " << role; }
        out << "\">\n";

        if (list.has_title()) {
            out << "<div class=\"title\">" << subs(list.title(), doc) << "</div>\n";
        }

        out << "<ul>\n";
        for (const auto& item : list.items()) {
            out << "<li>\n<p>" << subs(item->source(), doc) << "</p>\n";
            // Sub-list
            for (const auto& child : item->blocks()) {
                convert_block(*child, doc, out);
            }
            out << "</li>\n";
        }
        out << "</ul>\n"
            << "</div>\n";
    }

    // ── Ordered list ──────────────────────────────────────────────────────────

    void convert_olist(const List& list, const Document& doc,
                       OutputBuffer& out) const {
        // Determine CSS class from ordered_style
        std::string style_class;
        switch (list.ordered_style()) {
            case OrderedListStyle::Arabic:     style_class = "arabic";     break;
            case OrderedListStyle::LowerAlpha: style_class = "loweralpha"; break;
            case OrderedListStyle::UpperAlpha: style_class = "upperalpha"; break;
            case OrderedListStyle::LowerRoman: style_class = "lowerroman"; break;
            case OrderedListStyle::UpperRoman: style_class = "upperroman"; break;
        }

        const std::string& role = list.role();
        out << "<div class=\"olist " << style_class;
        if (!role.empty()) { out << " " << role; }
        out << "\">\n";

        if (list.has_title()) {
            out << "<div class=\"title\">" << subs(list.title(), doc) << "</div>\n";
        }

        // start= attribute sets the starting number
        const std::string& start_attr = list.attr("start");
        out << "<ol class=\"" << style_class << "\"";
        if (!start_attr.empty()) { out << " start=\"" << start_attr << "\""; }
        out << ">\n";
        for (const auto& item : list.items()) {
            out << "<li>\n<p>" << subs(item->source(), doc) << "</p>\n";
            for (const auto& child : item->blocks()) {
                convert_block(*child, doc, out);
            }
            out << "</li>\n";
        }
        out << "</ol>\n"
            << "</div>\n";
    }

    // ── Description list ──────────────────────────────────────────────────────

    void convert_dlist(const List& list, const Document& doc,
                       OutputBuffer& out) const {
        // Check whether this DL has any non-empty items to render.  An empty-
        // term item with no body (generated e.g. by db2adoc for multi-variant
        // synopsis blocks) is pure separator noise; suppress the entire <dl>
        // if every item is empty so that callers see no spurious markup.
        bool has_visible = false;
        for (const auto& item : list.items()) {
            if (!item->term().empty() || !item->source().empty() || !item->blocks().empty()) {
                has_visible = true;
                break;
            }
        }
        if (!has_visible) { return; }

        out << "<div class=\"dlist\">\n";
        if (list.has_title()) {
            out << "<div class=\"title\">" << subs(list.title(), doc) << "</div>\n";
        }
        out << "<dl>\n";
        for (const auto& item : list.items()) {
            // Skip completely empty items (empty term + no body)
            if (item->term().empty() && item->source().empty() && item->blocks().empty()) {
                continue;
            }
            out << "<dt class=\"hdlist1\">" << subs(item->term(), doc) << "</dt>\n";
            if (!item->source().empty() || !item->blocks().empty()) {
                out << "<dd>\n";
                if (!item->source().empty()) {
                    out << "<p>" << subs(item->source(), doc) << "</p>\n";
                }
                for (const auto& child : item->blocks()) {
                    convert_block(*child, doc, out);
                }
                out << "</dd>\n";
            } else {
                out << "<dd></dd>\n";
            }
        }
        out << "</dl>\n"
            << "</div>\n";
    }

    // ── Callout list ──────────────────────────────────────────────────────────

    void convert_colist(const List& list, const Document& doc,
                        OutputBuffer& out) const {
        out << "<div class=\"colist arabic\">\n";
        out << "<ol>\n";
        int n = 1;
        for (const auto& item : list.items()) {
            out << "<li>\n";
            // Emit the callout badge number
            out << "<span class=\"conum\">(" << n << ")</span> ";
            out << "<p>" << subs(item->source(), doc) << "</p>\n";
            out << "</li>\n";
            ++n;
        }
        out << "</ol>\n"
            << "</div>\n";
    }

    // ── Table ──────────────────────────────────────────────────────────────────

    void convert_table(const Table& table, const Document& doc,
                       OutputBuffer& out) const {
        const std::string& role = table.role();

        out << "<table class=\"tableblock frame-all grid-all stretch";
        if (!role.empty()) { out << " " << role; }
        out << "\">\n";

        if (table.has_title()) {
            int n = ++counters_["table"];
            out << "<caption class=\"title\">Table " << n << ". "
                << subs(table.title(), doc) << "</caption>\n";
        }

        // Colgroup
        const auto& col_specs = table.column_specs();

        // Compute total proportional width so we can convert to CSS percentages.
        // If all widths are 0 (auto-width), skip colgroup entirely.
        int total_width = 0;
        bool all_auto = true;
        for (const auto& cs : col_specs) {
            if (cs.width > 0) { total_width += cs.width; all_auto = false; }
        }
        if (total_width == 0) { total_width = 1; }  // guard against divide-by-zero

        // Helper: convert proportional width to CSS percentage string.
        auto colgroup_style = [&](const ColumnSpec& cs) -> std::string {
            if (cs.width == 0 || all_auto) { return ""; }
            // Round to 2 decimal places (Asciidoctor compatibility).
            double pct = 100.0 * cs.width / total_width;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.2f%%", pct);
            return std::string("width: ") + buf;
        };

        // Build a map from column index → CSS halign class using column specs.
        auto col_halign = [&](std::size_t col_idx) -> std::string {
            if (col_idx < col_specs.size() && !col_specs[col_idx].halign.empty()) {
                return "halign-" + col_specs[col_idx].halign;
            }
            return "halign-left";
        };

        if (!col_specs.empty()) {
            out << "<colgroup>\n";
            for (const auto& cs : col_specs) {
                std::string style = colgroup_style(cs);
                if (!style.empty()) {
                    out << "<col style=\"" << style << "\">\n";
                } else {
                    out << "<col>\n";
                }
            }
            out << "</colgroup>\n";
        }

        // thead
        if (table.has_header()) {
            out << "<thead>\n";
            for (const auto& row : table.head_rows()) {
                out << "<tr>\n";
                std::size_t col_pos = 0;
                for (const auto& cell : row.cells()) {
                    const int cs = cell->colspan();
                    const int rs = cell->rowspan();
                    out << "<th class=\"tableblock " << col_halign(col_pos) << " valign-top\"";
                    if (cs > 1) { out << " colspan=\"" << cs << "\""; }
                    if (rs > 1) { out << " rowspan=\"" << rs << "\""; }
                    out << ">" << subs(cell->source(), doc) << "</th>\n";
                    col_pos += static_cast<std::size_t>(cs);
                }
                out << "</tr>\n";
            }
            out << "</thead>\n";
        }

        // tbody
        if (!table.body_rows().empty()) {
            out << "<tbody>\n";
            for (const auto& row : table.body_rows()) {
                out << "<tr>\n";
                std::size_t col_pos = 0;
                for (const auto& cell : row.cells()) {
                    const int cs = cell->colspan();
                    const int rs = cell->rowspan();
                    // Determine alignment: cell-level overrides column spec
                    std::string halign = cell->halign().empty()
                                         ? col_halign(col_pos)
                                         : "halign-" + cell->halign();
                    // Column style 'h' makes the cell a header cell
                    bool is_header_col = (col_pos < col_specs.size() &&
                                          col_specs[col_pos].style == "h");
                    std::string span_attrs;
                    if (cs > 1) { span_attrs += " colspan=\"" + std::to_string(cs) + "\""; }
                    if (rs > 1) { span_attrs += " rowspan=\"" + std::to_string(rs) + "\""; }
                    if (is_header_col) {
                        out << "<th class=\"tableblock " << halign << " valign-top\""
                            << span_attrs << ">"
                            << "<p class=\"tableblock\">"
                            << subs(cell->source(), doc)
                            << "</p></th>\n";
                    } else {
                        out << "<td class=\"tableblock " << halign << " valign-top\""
                            << span_attrs << ">"
                            << "<p class=\"tableblock\">"
                            << subs(cell->source(), doc)
                            << "</p></td>\n";
                    }
                    col_pos += static_cast<std::size_t>(cs);
                }
                out << "</tr>\n";
            }
            out << "</tbody>\n";
        }

        // tfoot
        if (table.has_footer()) {
            out << "<tfoot>\n";
            for (const auto& row : table.foot_rows()) {
                out << "<tr>\n";
                std::size_t col_pos = 0;
                for (const auto& cell : row.cells()) {
                    const int cs = cell->colspan();
                    const int rs = cell->rowspan();
                    out << "<td class=\"tableblock " << col_halign(col_pos) << " valign-top\"";
                    if (cs > 1) { out << " colspan=\"" << cs << "\""; }
                    if (rs > 1) { out << " rowspan=\"" << rs << "\""; }
                    out << "><p class=\"tableblock\">"
                        << subs(cell->source(), doc)
                        << "</p></td>\n";
                    col_pos += static_cast<std::size_t>(cs);
                }
                out << "</tr>\n";
            }
            out << "</tfoot>\n";
        }

        out << "</table>\n";
    }

    // ── Image block ────────────────────────────────────────────────────────────

    void convert_image(const Block& block, const Document& doc,
                       OutputBuffer& out) const {
        const std::string& target = block.attr("target");
        const std::string& alt    = block.attr("alt");
        const std::string& width  = block.attr("width");
        const std::string& height = block.attr("height");
        const std::string& role   = block.role();

        out << "<div class=\"imageblock";
        if (!role.empty()) { out << " " << role; }
        out << "\">\n"
            << "<div class=\"content\">\n"
            << "<img src=\"" << target << "\" alt=\"" << sub_specialchars(alt) << "\"";

        if (!width.empty())  { out << " width=\""  << width  << "\""; }
        if (!height.empty()) { out << " height=\"" << height << "\""; }

        out << ">\n"
            << "</div>\n";

        if (block.has_title()) {
            out << "<div class=\"title\">" << subs(block.title(), doc) << "</div>\n";
        }

        out << "</div>\n";
    }

    // ── Video block ────────────────────────────────────────────────────────────

    void convert_video(const Block& block, const Document& doc,
                       OutputBuffer& out) const {
        const std::string& target = block.attr("target");
        const std::string& width  = block.attr("width");
        const std::string& height = block.attr("height");
        const std::string& role   = block.role();

        out << "<div class=\"videoblock";
        if (!role.empty()) { out << " " << role; }
        out << "\">\n";
        if (block.has_title()) {
            out << "<div class=\"title\">" << subs(block.title(), doc) << "</div>\n";
        }
        out << "<div class=\"content\">\n"
            << "<video src=\"" << sub_specialchars(target) << "\"";
        if (!width.empty())  { out << " width=\""  << width  << "\""; }
        if (!height.empty()) { out << " height=\"" << height << "\""; }
        if (block.has_attr("autoplay")) { out << " autoplay"; }
        if (block.has_attr("loop"))     { out << " loop"; }
        if (block.has_attr("nocontrols")) {
            // nocontrols: suppress default controls
        } else {
            out << " controls";
        }
        out << ">Your browser does not support the video tag.</video>\n"
            << "</div>\n"
            << "</div>\n";
    }

    // ── Audio block ────────────────────────────────────────────────────────────

    void convert_audio(const Block& block, const Document& doc,
                       OutputBuffer& out) const {
        const std::string& target = block.attr("target");
        const std::string& role   = block.role();

        out << "<div class=\"audioblock";
        if (!role.empty()) { out << " " << role; }
        out << "\">\n";
        if (block.has_title()) {
            out << "<div class=\"title\">" << subs(block.title(), doc) << "</div>\n";
        }
        out << "<div class=\"content\">\n"
            << "<audio src=\"" << sub_specialchars(target) << "\"";
        if (block.has_attr("autoplay")) { out << " autoplay"; }
        if (block.has_attr("loop"))     { out << " loop"; }
        if (!block.has_attr("nocontrols")) { out << " controls"; }
        out << ">Your browser does not support the audio tag.</audio>\n"
            << "</div>\n"
            << "</div>\n";
    }

    // ── Footnotes section ─────────────────────────────────────────────────────

    /// Render the footnote section at the end of the document.
    /// Each entry in @p footnotes becomes a numbered definition that back-links
    /// to its inline reference marker.
    void convert_footnotes(const std::vector<FootnoteEntry>& footnotes,
                           const Document& doc,
                           OutputBuffer& out) const {
        if (footnotes.empty()) { return; }
        out << "<div id=\"footnotes\">\n"
            << "<hr>\n";
        for (const auto& fn : footnotes) {
            out << "<div class=\"footnote\" id=\"_footnotedef_"
                << fn.number << "\">\n"
                << "<a href=\"#_footnoteref_" << fn.number << "\">"
                << fn.number << "</a>. "
                << subs(fn.text, doc)
                << "\n</div>\n";
        }
        out << "</div>\n";
    }

    // ── Source callout substitution ────────────────────────────────────────────

    /// Replace HTML-escaped callout markers (``&lt;N&gt;``) in verbatim source
    /// text with styled callout badges.  Each callout gets an anchor so the
    /// Scan @p escaped_source for callout markers &lt;N&gt; and replace each
    /// with an HTML anchor badge.  No regex needed.
    static std::string substitute_callouts(const std::string& escaped_source,
                                           const std::string& block_id)
    {
        std::string result;
        result.reserve(escaped_source.size());
        const std::size_t n = escaped_source.size();

        for (std::size_t i = 0; i < n; ) {
            // Look for "&lt;" (4 bytes)
            if (i + 4 <= n &&
                escaped_source[i]   == '&' && escaped_source[i+1] == 'l' &&
                escaped_source[i+2] == 't' && escaped_source[i+3] == ';') {
                std::size_t j = i + 4;
                const std::size_t num_start = j;
                // Consume digits
                while (j < n && escaped_source[j] >= '0' && escaped_source[j] <= '9') { ++j; }
                const std::size_t num_len = j - num_start;
                // Check for "&gt;" (4 bytes) immediately after digits
                if (num_len > 0 && j + 4 <= n &&
                    escaped_source[j]   == '&' && escaped_source[j+1] == 'g' &&
                    escaped_source[j+2] == 't' && escaped_source[j+3] == ';') {
                    std::string num(escaped_source, num_start, num_len);
                    const std::string& mid = block_id.empty() ? num : block_id + "-" + num;
                    result.reserve(result.size() + 28 + mid.size() + num.size());
                    result += "<b id=\"CO";
                    result += mid;
                    result += "\" class=\"conum\">(";
                    result += num;
                    result += ")</b>";
                    i = j + 4;
                    continue;
                }
            }
            result += escaped_source[i++];
        }
        return result;
    }

    // ── docinfo file injection ─────────────────────────────────────────────────

    /// Inject docinfo fragment content.
    ///
    /// @p location is "head" or "footer".
    /// In head location, content is injected just before `</head>`.
    /// In footer location, content is injected just before `</body>`.
    ///
    /// Files are only read in Unsafe or Server safe-mode.
    /// Looks for: `docinfo.html` (head) and `docinfo-footer.html` (footer),
    /// and also `<docname>-docinfo.html` / `<docname>-docinfo-footer.html`.
    void inject_docinfo(const Document& doc,
                        const std::string& location,
                        OutputBuffer& out) const {
        // Only inject in unsafe mode
        if (doc.safe_mode() != SafeMode::Unsafe &&
            doc.safe_mode() != SafeMode::Server) {
            return;
        }

        namespace fs = std::filesystem;
        const std::string& base = doc.base_dir().empty() ? "." : doc.base_dir();

        // Build candidate file names
        std::string suffix = (location == "head") ? "docinfo.html" : "docinfo-footer.html";
        std::vector<fs::path> candidates = {
            fs::path(base) / suffix
        };
        // Per-document docinfo: <docname>-docinfo.html
        const std::string& src_file = doc.source_file();
        if (!src_file.empty()) {
            fs::path stem = fs::path(src_file).stem();
            std::string per_doc_suffix = (location == "head")
                ? (stem.string() + "-docinfo.html")
                : (stem.string() + "-docinfo-footer.html");
            candidates.push_back(fs::path(base) / per_doc_suffix);
        }

        for (const auto& p : candidates) {
            std::error_code ec;
            if (!fs::exists(p, ec)) { continue; }
            std::ifstream f(p);
            if (!f.is_open()) { continue; }
            std::string content((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
            out << content << '\n';
        }
    }

    // ── Floating title ─────────────────────────────────────────────────────────

    void convert_floating_title(const Block& block, const Document& doc,
                                OutputBuffer& out) const {
        int level = 1;
        const std::string& lv_str = block.attr("level");
        if (!lv_str.empty()) {
            try { level = std::stoi(lv_str); } catch (...) {}
        }
        std::string tag = "h" + std::to_string(std::min(level + 1, 6));
        const std::string& id = block.id();

        out << "<" << tag << " class=\"discrete\"";
        if (!id.empty()) { out << " id=\"" << id << "\""; }
        out << ">" << subs(block.source(), doc) << "</" << tag << ">\n";
    }

    // ── Table of Contents ──────────────────────────────────────────────────────

    void convert_toc(const Document& doc, OutputBuffer& out) const {
        const auto& entries = doc.toc_entries();
        if (entries.empty()) { return; }

        int toclevels = 2;
        if (doc.has_attr("toclevels")) {
            try { toclevels = std::stoi(doc.attr("toclevels")); } catch (...) {}
        }

        std::string toc_title = doc.attr("toc-title", "Table of Contents");

        out << "<div id=\"toc\" class=\"toc\">\n"
            << "<div id=\"toctitle\">" << sub_specialchars(toc_title) << "</div>\n";

        // We maintain a stack of open list levels.
        // open_li[lv] = true means a <li> at that level has been opened but not yet
        // closed with </li>.  This lets us insert a nested <ul> inside it.
        static constexpr std::size_t MAX_LEVEL = 7;
        std::array<bool, MAX_LEVEL> open_li{};
        std::size_t list_depth = 0;  // deepest open <ul> level

        for (const auto& entry : entries) {
            if (entry.level < 1 || entry.level > toclevels) { continue; }
            auto lv = static_cast<std::size_t>(entry.level);

            // ── Close deeper levels first ────────────────────────────────────
            while (list_depth > lv) {
                // Close the innermost open <li>, then the <ul>
                if (list_depth < MAX_LEVEL && open_li[list_depth]) {
                    out << "</li>\n";
                    open_li[list_depth] = false;
                }
                out << "</ul>\n";
                --list_depth;
            }

            // ── Open lists up to the required level ──────────────────────────
            while (list_depth < lv) {
                ++list_depth;
                out << "<ul class=\"sectlevel" << list_depth << "\">\n";
            }

            // ── Close the previous item at this level (if any) ───────────────
            if (lv < MAX_LEVEL && open_li[lv]) {
                out << "</li>\n";
                open_li[lv] = false;
            }

            // ── Emit the entry ────────────────────────────────────────────────
            std::string title_text = entry.title;
            if (!entry.sectnum.empty()) {
                title_text = entry.sectnum + " " + title_text;
            }
            out << "<li><a href=\"#" << entry.id << "\">"
                << sub_specialchars(title_text)
                << "</a>";
            if (lv < MAX_LEVEL) { open_li[lv] = true; }
        }

        // ── Close everything that's still open ────────────────────────────────
        while (list_depth >= 1) {
            if (list_depth < MAX_LEVEL && open_li[list_depth]) {
                out << "</li>\n";
                open_li[list_depth] = false;
            }
            out << "</ul>\n";
            --list_depth;
        }

        out << "</div>\n";
    }

    // ── Minimal default CSS ────────────────────────────────────────────────────

    /// Returns a minimal stylesheet sufficient for basic readability.
    /// A production build would use the full Asciidoctor stylesheet or link
    /// to an external file.
    [[nodiscard]] static std::string default_css() {
        return
            "/* asciiquack default stylesheet (minimal) */\n"
            "body{font-family:sans-serif;max-width:900px;margin:0 auto;padding:1em 2em;"
              "line-height:1.6;color:#333}\n"
            "h1,h2,h3,h4,h5,h6{color:#2c5282;line-height:1.2;margin-top:1.4em}\n"
            "h1{font-size:2em;border-bottom:2px solid #2c5282;padding-bottom:.25em}\n"
            "h2{font-size:1.6em;border-bottom:1px solid #ccc}\n"
            "code,pre{font-family:monospace;background:#f5f5f5;border-radius:3px}\n"
            "pre{padding:1em;overflow:auto;border-left:4px solid #ccc}\n"
            "code{padding:.1em .3em}\n"
            ".listingblock pre{background:#282c34;color:#abb2bf;border:none}\n"
            "table{border-collapse:collapse;width:100%}\n"
            "td,th{border:1px solid #ddd;padding:.5em .75em;text-align:left}\n"
            "th{background:#f0f0f0;font-weight:bold}\n"
            ".admonitionblock table{border:none;width:100%}\n"
            ".admonitionblock td.icon{width:80px;font-weight:bold;text-transform:uppercase}\n"
            ".admonitionblock td.content{padding:.5em 1em}\n"
            ".admonitionblock.note td.icon{color:#31708f}\n"
            ".admonitionblock.tip td.icon{color:#3c763d}\n"
            ".admonitionblock.warning td.icon{color:#8a6d3b}\n"
            ".admonitionblock.important td.icon{color:#a94442}\n"
            ".admonitionblock.caution td.icon{color:#a94442}\n"
            "blockquote{border-left:4px solid #ccc;padding:.5em 1em;margin:1em 0;"
              "color:#555;font-style:italic}\n"
            ".sidebarblock{background:#f5f5f5;border:1px solid #ddd;border-radius:4px;"
              "padding:1em;margin:1em 0}\n"
            "#header,#content,#footer{margin-bottom:1em}\n"
            "#footer{border-top:1px solid #ccc;padding-top:.5em;color:#666;font-size:.9em}\n"
            "#header .details{font-size:.9em;color:#666}\n"
            "mark{background:#ff9}\n"
            ".exampleblock{border:1px solid #ddd;border-radius:4px;padding:1em;margin:1em 0}\n"
            "img{max-width:100%;height:auto}\n"
            ".imageblock{text-align:center;margin:1em 0}\n"
            ".imageblock .title{font-style:italic;color:#666;margin-top:.5em}\n"
            "video,audio{max-width:100%}\n"
            ".videoblock,.audioblock{margin:1em 0}\n"
            ".sectnum{margin-right:.25em}\n"
            "#toc{background:#f8f8f8;border:1px solid #ddd;border-radius:4px;"
              "padding:1em 1.5em;margin:1em 0;display:inline-block;min-width:200px}\n"
            "#toctitle{font-weight:bold;margin-bottom:.5em}\n"
            "#toc ul{list-style:none;padding-left:0;margin:0}\n"
            "#toc ul ul{padding-left:1.5em}\n"
            "#toc a{text-decoration:none;color:#2c5282}\n"
            "#toc a:hover{text-decoration:underline}\n"
            "#toc li{margin:.25em 0}\n"
            // footnotes
            "#footnotes{border-top:1px solid #ccc;margin-top:1em;padding-top:.5em}\n"
            ".footnote{font-size:.9em;color:#666;margin:.25em 0}\n"
            "sup.footnote{font-size:.75em;vertical-align:super;line-height:0}\n"
            "sup.footnote a{color:#2c5282;text-decoration:none}\n"
            // UI macros
            "kbd{font-family:monospace;border:1px solid #ccc;border-radius:3px;"
              "padding:.1em .35em;font-size:.9em;background:#f5f5f5}\n"
            ".keyseq{font-family:monospace;font-size:.9em}\n"
            "b.button{border:1px solid #aaa;border-radius:3px;padding:.1em .35em;"
              "font-size:.9em;background:#f0f0f0;font-weight:normal}\n"
            ".menuseq,.menu,.menuitem{font-style:italic}\n"
            ".caret{font-style:normal;font-weight:bold}\n"
            // source callouts
            "b.conum,span.conum{display:inline-block;background:#555;color:#fff;"
              "border-radius:50%;width:1.2em;height:1.2em;text-align:center;"
              "line-height:1.2em;font-size:.8em;font-weight:bold;font-style:normal;"
              "margin:0 .1em}\n"
            ".colist{margin:.5em 0}\n"
            ".colist ol{list-style:none;padding-left:0}\n"
            // stem / math
            ".stemblock{margin:1em 0}\n"
#ifdef ASCIIQUACK_USE_ULIGHT
            // µlight syntax-highlighting token colours (One Dark palette)
            + ulight_highlight_css()
#endif
            ;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Free function convenience wrapper
// ─────────────────────────────────────────────────────────────────────────────

/// Convert a parsed Document to HTML 5.
[[nodiscard]] inline std::string convert_to_html5(const Document& doc) {
    return Html5Converter{}.convert(doc);
}

} // namespace asciiquack
