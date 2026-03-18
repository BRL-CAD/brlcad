/// @file asciiquack.cpp
/// @brief Main entry point for the asciiquack AsciiDoc processor.
///
/// Usage:
///   asciiquack [OPTIONS] FILE...
///   asciiquack --help
///   asciiquack --version
///
/// Reads one or more AsciiDoc files (or stdin when no FILE is given),
/// converts them to the requested output format (default: html5), and
/// writes the result to the output file or stdout.

#include "cxxopts.hpp"
#include "docbook5.hpp"
#include "document.hpp"
#include "html5.hpp"
#include "manpage.hpp"
#include "parser.hpp"
#ifdef ASCIIQUACK_USE_PDF
#include "pdf.hpp"
#endif
#include "reader.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
// Version
// ─────────────────────────────────────────────────────────────────────────────

static constexpr const char* VERSION     = "0.1.0";
static constexpr const char* PROGRAM     = "asciiquack";
static constexpr const char* DESCRIPTION =
    "Convert AsciiDoc source files to HTML 5 and other formats.\n"
    "A C++17 implementation of the Asciidoctor processor.";

// ─────────────────────────────────────────────────────────────────────────────
// I/O helpers
// ─────────────────────────────────────────────────────────────────────────────

/// Read the entire contents of @p path into a string.
/// Throws std::runtime_error on failure.
static std::string read_file(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("cannot open '" + path.string() + "': " +
                                 std::strerror(errno));  // NOLINT
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

/// Write @p content to @p path, creating parent directories as needed.
/// Throws std::runtime_error on failure.
static void write_file(const fs::path& path, const std::string& content) {
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path());
    }
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("cannot open '" + path.string() + "' for writing: " +
                                 std::strerror(errno));  // NOLINT
    }
    file << content;
    if (!file) {
        throw std::runtime_error("write error to '" + path.string() + "'");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Safe-mode helper
// ─────────────────────────────────────────────────────────────────────────────

static asciiquack::SafeMode parse_safe_mode(const std::string& name) {
    if (name == "unsafe") { return asciiquack::SafeMode::Unsafe; }
    if (name == "safe")   { return asciiquack::SafeMode::Safe;   }
    if (name == "server") { return asciiquack::SafeMode::Server; }
    if (name == "secure") { return asciiquack::SafeMode::Secure; }
    throw std::invalid_argument("unknown safe mode: '" + name +
                                "'; expected unsafe, safe, server or secure");
}

// ─────────────────────────────────────────────────────────────────────────────
// Conversion of a single input
// ─────────────────────────────────────────────────────────────────────────────

struct ConvertParams {
    std::string backend;      ///< html5 (only supported backend so far)
    std::string doctype;      ///< article | book | manpage | inline
    std::string out_file;     ///< explicit output path, empty = derive from input
    std::string dest_dir;     ///< output directory
    bool        no_header_footer = false;  ///< suppress document wrapper
    bool        quiet            = false;
    asciiquack::SafeMode safe_mode = asciiquack::SafeMode::Secure;
    std::unordered_map<std::string, std::string> attributes;
};

static int convert_one(const std::string&    source_path,
                       const std::string&    content,
                       const ConvertParams&  params) {
    asciiquack::ParseOptions opts;
    opts.safe_mode  = params.safe_mode;
    opts.doctype    = params.doctype;
    opts.backend    = params.backend;
    opts.attributes = params.attributes;

    if (params.no_header_footer) {
        opts.attributes["embedded"] = "";
    }

    auto doc = asciiquack::Parser::parse_string(content, opts, source_path);

    if (!params.quiet && !source_path.empty() && source_path != "<stdin>") {
        std::cerr << "asciiquack: converting '" << source_path << "'\n";
    }

    // ── Determine output path ─────────────────────────────────────────────────
    std::string out_path = params.out_file;

    if (out_path.empty() && source_path != "<stdin>") {
        fs::path in{source_path};
        fs::path out_dir = params.dest_dir.empty()
                           ? in.parent_path()
                           : fs::path{params.dest_dir};
        std::string suffix = ".html";
        if (params.backend == "docbook5")  { suffix = ".xml"; }
        else if (params.backend == "manpage") { suffix = ".1"; }
#ifdef ASCIIQUACK_USE_PDF
        else if (params.backend == "pdf")  { suffix = ".pdf"; }
#endif
        out_path = (out_dir / in.stem()).string() + suffix;
    }

    // ── Convert ───────────────────────────────────────────────────────────────
    std::string output;

    if (params.backend == "html5" || params.backend == "xhtml5" ||
        params.backend.empty()) {
        output = asciiquack::convert_to_html5(*doc);
    } else if (params.backend == "manpage") {
        output = asciiquack::convert_to_manpage(*doc);
#ifdef ASCIIQUACK_USE_PDF
    } else if (params.backend == "pdf") {
        bool a4 = (params.attributes.count("pdf-page-size") &&
                   params.attributes.at("pdf-page-size") == "A4");

        auto get_attr = [&](const std::string& key) -> std::string {
            auto it = params.attributes.find(key);
            return (it != params.attributes.end()) ? it->second : "";
        };

        asciiquack::FontSet fonts;
        fonts.regular     = get_attr("pdf-font");
        fonts.bold        = get_attr("pdf-font-bold");
        fonts.italic      = get_attr("pdf-font-italic");
        fonts.bold_italic = get_attr("pdf-font-bold-italic");
        fonts.mono        = get_attr("pdf-font-mono");
        fonts.mono_bold   = get_attr("pdf-font-mono-bold");

        // When no body font is explicitly given, fall back to the bundled
        // Noto fonts (defined at build time via ASCIIQUACK_NOTO_FONTS_DIR).
#ifdef ASCIIQUACK_NOTO_FONTS_DIR
        if (fonts.regular.empty()) {
            const std::string noto = ASCIIQUACK_NOTO_FONTS_DIR;
            fonts.regular     = noto + "/Noto_Sans/static/NotoSans-Regular.ttf";
            if (fonts.bold.empty())
                fonts.bold        = noto + "/Noto_Sans/static/NotoSans-Bold.ttf";
            if (fonts.italic.empty())
                fonts.italic      = noto + "/Noto_Sans/static/NotoSans-Italic.ttf";
            if (fonts.bold_italic.empty())
                fonts.bold_italic = noto + "/Noto_Sans/static/NotoSans-BoldItalic.ttf";
            if (fonts.mono.empty())
                fonts.mono        = noto + "/Noto_Sans_Mono/static/NotoSansMono-Regular.ttf";
            if (fonts.mono_bold.empty())
                fonts.mono_bold   = noto + "/Noto_Sans_Mono/static/NotoSansMono-Bold.ttf";
        }
#endif

        output = asciiquack::convert_to_pdf(*doc, a4, fonts,
            source_path != "<stdin>"
                ? fs::path{source_path}.parent_path().string()
                : "");
#else
    } else if (params.backend == "pdf") {
        std::cerr << "asciiquack: PDF output is not available in this build "
                     "(compiled without minipdf/lodepng support)\n";
        return 1;
#endif
    } else if (params.backend == "docbook5" || params.backend == "docbook") {
        output = asciiquack::convert_to_docbook5(*doc);
    } else {
        std::cerr << "asciiquack: backend '" << params.backend
                  << "' is not yet implemented; falling back to html5\n";
        output = asciiquack::convert_to_html5(*doc);
    }

    // ── Write output ──────────────────────────────────────────────────────────
    if (out_path.empty()) {
        // Write to stdout
        std::cout << output;
    } else {
        write_file(fs::path{out_path}, output);
        if (!params.quiet) {
            std::cerr << "asciiquack: output written to '" << out_path << "'\n";
        }
    }

    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // ── Option definitions ────────────────────────────────────────────────────
    cxxopts::Options options(PROGRAM, DESCRIPTION);

    options.add_options("Conversion")
        ("b,backend",
#ifdef ASCIIQUACK_USE_PDF
            "Output format: html5, xhtml5, manpage, docbook5, pdf (default: html5)",
#else
            "Output format: html5, xhtml5, manpage, docbook5 (default: html5)",
#endif
            cxxopts::value<std::string>()->default_value("html5"))
        ("d,doctype",
            "Document type: article, book, manpage, inline (default: article)",
            cxxopts::value<std::string>()->default_value("article"))
        ("e,embedded",
            "Suppress document structure (no <html>/<head>/<body> wrapper)")
        ("s,no-header-footer",
            "Alias for --embedded")
        ("a,attribute",
            "Set or override a document attribute (NAME or NAME=VALUE or NAME!)",
            cxxopts::value<std::vector<std::string>>())
        ("o,out-file",
            "Output file path (use - for stdout)",
            cxxopts::value<std::string>()->default_value(""))
        ("D,destination-dir",
            "Output directory (used when converting multiple files)",
            cxxopts::value<std::string>()->default_value(""))
        ("B,base-dir",
            "Base directory for the document and relative resources",
            cxxopts::value<std::string>()->default_value(""))
        ;

    options.add_options("Safety")
        ("safe",
            "Set safe mode to SAFE")
        ("S,safe-mode",
            "Set safe mode: unsafe, safe, server, secure (default: secure)",
            cxxopts::value<std::string>()->default_value("secure"))
        ;

    options.add_options("Diagnostics")
        ("q,quiet",    "Suppress informational messages to stderr")
        ("v,verbose",  "Increase verbosity (show DEBUG / INFO messages)")
        ("t,timings",  "Print timing report to stderr")
        ;

    options.add_options("General")
        ("h,help",    "Print this help message")
        ("V,version", "Print version information")
        ;

    options.add_options()
        ("input-files", "Input AsciiDoc file(s)",
            cxxopts::value<std::vector<std::string>>())
        ;

    options.parse_positional({"input-files"});
    options.positional_help("[FILE...]");

    // ── Parse command line ────────────────────────────────────────────────────
    cxxopts::ParseResult result;
    try {
        result = options.parse(argc, argv);
    } catch (const cxxopts::exceptions::exception& ex) {
        std::cerr << PROGRAM << ": " << ex.what() << "\n"
                  << "Try '" << PROGRAM << " --help' for usage information.\n";
        return EXIT_FAILURE;
    }

    if (result.count("help")) {
        std::cout << options.help() << "\n";
        return EXIT_SUCCESS;
    }

    if (result.count("version")) {
        std::cout << PROGRAM << " " << VERSION << "\n";
        return EXIT_SUCCESS;
    }

    // ── Build ConvertParams ───────────────────────────────────────────────────
    ConvertParams params;
    params.backend  = result["backend"].as<std::string>();
    params.doctype  = result["doctype"].as<std::string>();

    // When the manpage backend is selected, automatically imply doctype=manpage
    // (matching Asciidoctor behaviour) unless the user explicitly passed -d.
    if (params.backend == "manpage" && !result.count("doctype")) {
        params.doctype = "manpage";
    }
    params.out_file = result["out-file"].as<std::string>();
    params.dest_dir = result["destination-dir"].as<std::string>();
    params.quiet    = result.count("quiet") > 0;

    if (params.out_file == "-") { params.out_file.clear(); }

    params.no_header_footer = (result.count("embedded")         > 0) ||
                              (result.count("no-header-footer")  > 0);

    // Safe mode
    try {
        std::string sm_str = result.count("safe") ? "safe"
                             : result["safe-mode"].as<std::string>();
        params.safe_mode = parse_safe_mode(sm_str);
    } catch (const std::invalid_argument& ex) {
        std::cerr << PROGRAM << ": " << ex.what() << "\n";
        return EXIT_FAILURE;
    }

    // Attribute overrides:  -a foo=bar  or  -a foo  or  -a foo!
    if (result.count("attribute")) {
        for (const auto& attr : result["attribute"].as<std::vector<std::string>>()) {
            auto eq = attr.find('=');
            if (!attr.empty() && attr.back() == '!') {
                // Unset: -a foo!
                params.attributes["!" + attr.substr(0, attr.size() - 1)] = "";
            } else if (eq != std::string::npos) {
                params.attributes[attr.substr(0, eq)] = attr.substr(eq + 1);
            } else {
                params.attributes[attr] = "";
            }
        }
    }

    // Base directory
    if (!result["base-dir"].as<std::string>().empty()) {
        params.attributes["docdir"] = result["base-dir"].as<std::string>();
    }

    // ── Process inputs ────────────────────────────────────────────────────────
    int exit_code = EXIT_SUCCESS;

    if (result.count("input-files")) {
        const auto& files = result["input-files"].as<std::vector<std::string>>();

        // When multiple files are given and no explicit output path is set,
        // --out-file is ignored (each file gets its own output).
        if (files.size() > 1 && !params.out_file.empty()) {
            std::cerr << PROGRAM << ": warning: --out-file is ignored "
                                    "when multiple input files are given\n";
            params.out_file.clear();
        }

        for (const auto& file : files) {
            try {
                std::string content = read_file(fs::path{file});
                int rc = convert_one(file, content, params);
                if (rc != 0) { exit_code = EXIT_FAILURE; }
            } catch (const std::exception& ex) {
                std::cerr << PROGRAM << ": ERROR: " << ex.what() << "\n";
                exit_code = EXIT_FAILURE;
            }
        }
    } else {
        // No files specified → read from stdin
        std::ostringstream buf;
        buf << std::cin.rdbuf();
        try {
            int rc = convert_one("<stdin>", buf.str(), params);
            if (rc != 0) { exit_code = EXIT_FAILURE; }
        } catch (const std::exception& ex) {
            std::cerr << PROGRAM << ": ERROR: " << ex.what() << "\n";
            exit_code = EXIT_FAILURE;
        }
    }

    return exit_code;
}
