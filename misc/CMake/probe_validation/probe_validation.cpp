#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

struct SnapshotData {
    std::map<std::string, std::string> records;
    std::map<std::string, std::string> meta;
};

struct SnapshotDiffResult {
    std::vector<std::tuple<std::string, std::string, std::string>> value_diffs;
    std::vector<std::pair<std::string, std::string>> only_ref;
    std::vector<std::pair<std::string, std::string>> only_cand;
    std::vector<std::pair<std::string, std::string>> matched;
    std::size_t compared = 0;
};

struct FileDiff {
    std::string path;
    std::string reason;
    std::string detail;
};

struct DirCompareResult {
    std::vector<FileDiff> diffs;
    std::vector<std::string> only_ref;
    std::vector<std::string> only_cand;
    std::vector<std::string> matched;
    std::size_t compared = 0;
};

static const std::vector<std::string> kDefaultResultPatterns = {
    "^HAVE_",
    "^SIZEOF_",
    "^WORKING_",
    "_FLAG_FOUND$",
    "_COMPILE$",
    "_FLAG$"
};

static const std::vector<std::string> kDefaultIgnorePatterns = {
    "^CMAKE_GENERATOR",
    "^CMAKE_MAKE_PROGRAM",
    "^CMAKE_COMMAND",
    "^CMAKE_ROOT",
    "^CMAKE_CACHE",
    "^CMAKE_HOME_DIRECTORY$",
    "^CMAKE_BINARY_DIR$",
    "^CMAKE_SOURCE_DIR$",
    "_BINARY_DIR$",
    "_SOURCE_DIR$",
    "_DIR$",
    "_PATH$",
    "_EXECUTABLE$",
    "_LIBRARY$",
    "_LIBRARIES$",
    "_INCLUDE_DIR",
    "^BRLCAD_PROBE_SNAPSHOT_FILE$",
    "^BRLCAD_ENABLE_PARALLEL_CONFIG_PROBES$",
    "^BRLCAD_PROBE_"
};

static const std::vector<std::string> kDefaultPathIgnorePatterns = {
    "^probe_snapshot\\.txt$",
    "^CMakeTmp/.*",
    "^CMakeFiles/CMakeOutput\\.log$",
    "^CMakeFiles/CMakeError\\.log$",
    "^CMakeFiles/CMakeConfigureLog\\.yaml$",
    "^CMakeFiles/cmake\\.check_cache$",
    "^CMakeFiles/CMakeScratch/.*",
    "^CMakeFiles/CheckTypeSize/.*\\.bin$"
};

static std::string trim(const std::string &s)
{
    std::size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        start++;
    }
    std::size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        end--;
    }
    return s.substr(start, end - start);
}

static std::vector<std::regex> compile_patterns(const std::vector<std::string> &patterns)
{
    std::vector<std::regex> compiled;
    compiled.reserve(patterns.size());
    for (const auto &p : patterns) {
        compiled.emplace_back(p, std::regex::ECMAScript);
    }
    return compiled;
}

static bool matches_any(const std::string &value, const std::vector<std::regex> &patterns)
{
    for (const auto &pattern : patterns) {
        if (std::regex_search(value, pattern)) {
            return true;
        }
    }
    return false;
}

static std::string replace_all(std::string input, const std::string &from, const std::string &to)
{
    if (from.empty()) {
        return input;
    }
    std::size_t pos = 0;
    while ((pos = input.find(from, pos)) != std::string::npos) {
        input.replace(pos, from.size(), to);
        pos += to.size();
    }
    return input;
}

static std::string slash_swap(const std::string &input)
{
    std::string out = input;
    for (char &c : out) {
        if (c == '/') {
            c = '\\';
        } else if (c == '\\') {
            c = '/';
        }
    }
    return out;
}

static std::string normalize_line_endings(const std::string &input)
{
    std::string out;
    out.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\r') {
            if (i + 1 < input.size() && input[i + 1] == '\n') {
                continue;
            }
        }
        out.push_back(input[i]);
    }
    return out;
}

static std::string normalize_roots(std::string value,
        const std::vector<std::pair<std::string, std::string>> &roots)
{
    for (const auto &root : roots) {
        value = replace_all(value, root.first, root.second);
        value = replace_all(value, slash_swap(root.first), root.second);
    }
    return value;
}

static bool read_file(const fs::path &path, std::string &content)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.good()) {
        return false;
    }
    std::ostringstream buffer;
    buffer << ifs.rdbuf();
    content = buffer.str();
    return true;
}

static bool is_text_content(const std::string &content)
{
    return content.find('\0') == std::string::npos;
}

static SnapshotData parse_snapshot(const fs::path &path)
{
    SnapshotData data;
    std::ifstream ifs(path);
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) {
            continue;
        }
        if (line[0] == '#') {
            std::string body = trim(line.substr(1));
            auto eq = body.find('=');
            if (eq != std::string::npos) {
                data.meta[trim(body.substr(0, eq))] = trim(body.substr(eq + 1));
            }
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        data.records[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return data;
}

static std::set<std::string> select_snapshot_names(const std::set<std::string> &names,
        bool strict,
        const std::vector<std::regex> &result_patterns,
        const std::vector<std::regex> &ignore_patterns)
{
    std::set<std::string> selected;
    for (const auto &name : names) {
        if (strict) {
            if (!matches_any(name, ignore_patterns)) {
                selected.insert(name);
            }
        } else {
            if (matches_any(name, result_patterns)) {
                selected.insert(name);
            }
        }
    }
    return selected;
}

static SnapshotDiffResult diff_snapshots(const SnapshotData &ref,
        const SnapshotData &cand,
        bool strict,
        const std::vector<std::regex> &result_patterns,
        const std::vector<std::regex> &ignore_patterns)
{
    SnapshotDiffResult result;
    std::set<std::string> all_names;
    for (const auto &item : ref.records) {
        all_names.insert(item.first);
    }
    for (const auto &item : cand.records) {
        all_names.insert(item.first);
    }

    auto selected = select_snapshot_names(all_names, strict, result_patterns, ignore_patterns);
    result.compared = selected.size();

    std::vector<std::pair<std::string, std::string>> ref_roots;
    std::vector<std::pair<std::string, std::string>> cand_roots;
    if (auto it = ref.meta.find("binary_dir"); it != ref.meta.end()) {
        ref_roots.push_back({it->second, "<BINDIR>"});
    }
    if (auto it = ref.meta.find("source_dir"); it != ref.meta.end()) {
        ref_roots.push_back({it->second, "<SRCDIR>"});
    }
    if (auto it = cand.meta.find("binary_dir"); it != cand.meta.end()) {
        cand_roots.push_back({it->second, "<BINDIR>"});
    }
    if (auto it = cand.meta.find("source_dir"); it != cand.meta.end()) {
        cand_roots.push_back({it->second, "<SRCDIR>"});
    }

    for (const auto &name : selected) {
        auto rit = ref.records.find(name);
        auto cit = cand.records.find(name);
        if (rit != ref.records.end() && cit != cand.records.end()) {
            std::string rval = normalize_roots(rit->second, ref_roots);
            std::string cval = normalize_roots(cit->second, cand_roots);
            if (rval == cval) {
                result.matched.push_back({name, rit->second});
            } else {
                result.value_diffs.push_back({name, rit->second, cit->second});
            }
            continue;
        }
        if (rit != ref.records.end()) {
            result.only_ref.push_back(*rit);
            continue;
        }
        result.only_cand.push_back(*cit);
    }

    return result;
}

static std::map<std::string, fs::path> collect_files(const fs::path &root,
        const std::vector<std::regex> &ignore_patterns)
{
    std::map<std::string, fs::path> files;
    if (!fs::exists(root)) {
        return files;
    }
    for (const auto &entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string rel = fs::relative(entry.path(), root).generic_string();
        if (rel.rfind("../", 0) == 0) {
            continue;
        }
        if (matches_any(rel, ignore_patterns)) {
            continue;
        }
        files[rel] = entry.path();
    }
    return files;
}

static std::string first_text_difference(const std::string &a, const std::string &b)
{
    std::size_t line = 1;
    std::size_t a_pos = 0;
    std::size_t b_pos = 0;
    while (a_pos < a.size() && b_pos < b.size()) {
        if (a[a_pos] != b[b_pos]) {
            break;
        }
        if (a[a_pos] == '\n') {
            line++;
        }
        a_pos++;
        b_pos++;
    }

    auto line_text = [](const std::string &text, std::size_t pos) {
        std::size_t start = text.rfind('\n', pos);
        start = (start == std::string::npos) ? 0 : start + 1;
        std::size_t end = text.find('\n', pos);
        end = (end == std::string::npos) ? text.size() : end;
        return text.substr(start, end - start);
    };

    std::ostringstream msg;
    msg << "first difference at line " << line
        << "\n      reference = " << std::quoted(line_text(a, a_pos))
        << "\n      candidate = " << std::quoted(line_text(b, b_pos));
    return msg.str();
}

static DirCompareResult compare_directories(const fs::path &ref_root,
        const fs::path &cand_root,
        const std::vector<std::regex> &ignore_patterns,
        const fs::path &ref_source_root = fs::path(),
        const fs::path &cand_source_root = fs::path())
{
    DirCompareResult result;
    auto ref_files = collect_files(ref_root, ignore_patterns);
    auto cand_files = collect_files(cand_root, ignore_patterns);

    std::set<std::string> all_paths;
    for (const auto &item : ref_files) {
        all_paths.insert(item.first);
    }
    for (const auto &item : cand_files) {
        all_paths.insert(item.first);
    }

    std::vector<std::pair<std::string, std::string>> ref_roots = {
        {fs::weakly_canonical(ref_root).generic_string(), "<BINDIR>"},
        {fs::weakly_canonical(ref_root.parent_path()).generic_string(), "<WORKDIR>"}
    };
    std::vector<std::pair<std::string, std::string>> cand_roots = {
        {fs::weakly_canonical(cand_root).generic_string(), "<BINDIR>"},
        {fs::weakly_canonical(cand_root.parent_path()).generic_string(), "<WORKDIR>"}
    };

    auto ref_snap = ref_files.find("probe_snapshot.txt");
    if (ref_snap != ref_files.end()) {
        auto snap = parse_snapshot(ref_snap->second);
        if (auto it = snap.meta.find("source_dir"); it != snap.meta.end()) {
            ref_roots.push_back({it->second, "<SRCDIR>"});
        }
    }
    if (!ref_source_root.empty() && fs::exists(ref_source_root)) {
        ref_roots.push_back({fs::weakly_canonical(ref_source_root).generic_string(), "<SRCDIR>"});
    }
    auto cand_snap = cand_files.find("probe_snapshot.txt");
    if (cand_snap != cand_files.end()) {
        auto snap = parse_snapshot(cand_snap->second);
        if (auto it = snap.meta.find("source_dir"); it != snap.meta.end()) {
            cand_roots.push_back({it->second, "<SRCDIR>"});
        }
    }
    if (!cand_source_root.empty() && fs::exists(cand_source_root)) {
        cand_roots.push_back({fs::weakly_canonical(cand_source_root).generic_string(), "<SRCDIR>"});
    }

    for (const auto &rel : all_paths) {
        auto rit = ref_files.find(rel);
        auto cit = cand_files.find(rel);
        if (rit == ref_files.end()) {
            result.only_cand.push_back(rel);
            continue;
        }
        if (cit == cand_files.end()) {
            result.only_ref.push_back(rel);
            continue;
        }

        result.compared++;
        std::string ref_content;
        std::string cand_content;
        if (!read_file(rit->second, ref_content) || !read_file(cit->second, cand_content)) {
            result.diffs.push_back({rel, "read failure", "could not read one or both files"});
            continue;
        }

        bool ref_text = is_text_content(ref_content);
        bool cand_text = is_text_content(cand_content);
        if (ref_text != cand_text) {
            result.diffs.push_back({rel, "type mismatch", "one file is text, the other is binary"});
            continue;
        }

        if (ref_text) {
            ref_content = normalize_roots(normalize_line_endings(ref_content), ref_roots);
            cand_content = normalize_roots(normalize_line_endings(cand_content), cand_roots);
            if (ref_content == cand_content) {
                result.matched.push_back(rel);
            } else {
                result.diffs.push_back({rel, "text mismatch", first_text_difference(ref_content, cand_content)});
            }
            continue;
        }

        if (ref_content == cand_content) {
            result.matched.push_back(rel);
        } else {
            std::ostringstream detail;
            detail << "binary content differs (" << ref_content.size() << " vs "
                   << cand_content.size() << " bytes)";
            result.diffs.push_back({rel, "binary mismatch", detail.str()});
        }
    }

    return result;
}

static void print_snapshot_report(const fs::path &ref_path,
        const fs::path &cand_path,
        const SnapshotData &ref,
        const SnapshotData &cand,
        const SnapshotDiffResult &result,
        bool strict,
        bool show_matches)
{
    std::cout << "BRL-CAD configure snapshot comparison\n";
    std::cout << "================================================================\n";
    std::cout << "  reference : " << ref_path << "\n";
    for (const auto &key : {"generator", "generator_toolset", "parallel_config_probes", "build_type", "c_compiler"}) {
        auto it = ref.meta.find(key);
        if (it != ref.meta.end()) {
            std::cout << "              " << key << " = " << it->second << "\n";
        }
    }
    std::cout << "  candidate : " << cand_path << "\n";
    for (const auto &key : {"generator", "generator_toolset", "parallel_config_probes", "build_type", "c_compiler"}) {
        auto it = cand.meta.find(key);
        if (it != cand.meta.end()) {
            std::cout << "              " << key << " = " << it->second << "\n";
        }
    }
    std::cout << "  mode      : " << (strict ? "strict" : "result-pattern") << "\n";
    std::cout << "  compared  : " << result.compared << " variables (" << result.matched.size() << " identical)\n";
    std::cout << "================================================================\n";

    if (!result.value_diffs.empty()) {
        std::cout << "\nVALUE MISMATCH (" << result.value_diffs.size() << "):\n";
        for (const auto &diff : result.value_diffs) {
            std::cout << "  " << std::get<0>(diff) << "\n";
            std::cout << "      reference = " << std::quoted(std::get<1>(diff)) << "\n";
            std::cout << "      candidate = " << std::quoted(std::get<2>(diff)) << "\n";
        }
    }
    if (!result.only_ref.empty()) {
        std::cout << "\nONLY IN REFERENCE (" << result.only_ref.size() << "):\n";
        for (const auto &item : result.only_ref) {
            std::cout << "  " << item.first << " = " << std::quoted(item.second) << "\n";
        }
    }
    if (!result.only_cand.empty()) {
        std::cout << "\nONLY IN CANDIDATE (" << result.only_cand.size() << "):\n";
        for (const auto &item : result.only_cand) {
            std::cout << "  " << item.first << " = " << std::quoted(item.second) << "\n";
        }
    }
    if (show_matches && !result.matched.empty()) {
        std::cout << "\nMATCHED (" << result.matched.size() << "):\n";
        for (const auto &item : result.matched) {
            std::cout << "  " << item.first << " = " << std::quoted(item.second) << "\n";
        }
    }

    std::size_t divergences = result.value_diffs.size() + result.only_ref.size() + result.only_cand.size();
    std::cout << "\n================================================================\n";
    if (divergences == 0) {
        std::cout << "RESULT: PASS - compared snapshot variables are identical.\n";
    } else {
        std::cout << "RESULT: FAIL - " << divergences << " divergence(s) found.\n";
    }
}

static void print_directory_report(const fs::path &ref_dir,
        const fs::path &cand_dir,
        const DirCompareResult &result,
        bool show_matches)
{
    std::cout << "\nBRL-CAD configured tree comparison\n";
    std::cout << "================================================================\n";
    std::cout << "  reference : " << ref_dir << "\n";
    std::cout << "  candidate : " << cand_dir << "\n";
    std::cout << "  compared  : " << result.compared << " files (" << result.matched.size() << " identical)\n";
    std::cout << "================================================================\n";

    if (!result.diffs.empty()) {
        std::cout << "\nCONTENT MISMATCH (" << result.diffs.size() << "):\n";
        for (const auto &diff : result.diffs) {
            std::cout << "  " << diff.path << " [" << diff.reason << "]\n";
            std::cout << "      " << diff.detail << "\n";
        }
    }
    if (!result.only_ref.empty()) {
        std::cout << "\nONLY IN REFERENCE (" << result.only_ref.size() << "):\n";
        for (const auto &path : result.only_ref) {
            std::cout << "  " << path << "\n";
        }
    }
    if (!result.only_cand.empty()) {
        std::cout << "\nONLY IN CANDIDATE (" << result.only_cand.size() << "):\n";
        for (const auto &path : result.only_cand) {
            std::cout << "  " << path << "\n";
        }
    }
    if (show_matches && !result.matched.empty()) {
        std::cout << "\nMATCHED (" << result.matched.size() << "):\n";
        for (const auto &path : result.matched) {
            std::cout << "  " << path << "\n";
        }
    }

    std::size_t divergences = result.diffs.size() + result.only_ref.size() + result.only_cand.size();
    std::cout << "\n================================================================\n";
    if (divergences == 0) {
        std::cout << "RESULT: PASS - configured trees are identical after normalization.\n";
    } else {
        std::cout << "RESULT: FAIL - " << divergences << " divergence(s) found.\n";
    }
}

static std::string quote_arg(const std::string &arg)
{
#ifdef _WIN32
    bool need_quotes = arg.find_first_of(" \t\"") != std::string::npos;
    if (!need_quotes) {
        return arg;
    }
    std::string out = "\"";
    unsigned backslashes = 0;
    for (char c : arg) {
        if (c == '\\') {
            backslashes++;
            out.push_back(c);
            continue;
        }
        if (c == '"') {
            out.append(backslashes, '\\');
            out.push_back('\\');
        }
        backslashes = 0;
        out.push_back(c);
    }
    out.push_back('"');
    return out;
#else
    if (arg.find_first_of(" \t'\"\\$`!&()[]{};<>|*?") == std::string::npos) {
        return arg;
    }
    std::string out = "'";
    for (char c : arg) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('\'');
    return out;
#endif
}

static int run_command(const std::vector<std::string> &cmd)
{
    std::ostringstream joined;
    for (std::size_t i = 0; i < cmd.size(); ++i) {
        if (i) {
            joined << ' ';
        }
        joined << quote_arg(cmd[i]);
    }
    std::string command = joined.str();
    std::cout << "  " << command << "\n";
    return std::system(command.c_str());
}

static std::vector<std::string> strip_generator(const std::vector<std::string> &args)
{
    std::vector<std::string> out;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-G") {
            i++;
            continue;
        }
        if (args[i].rfind("-G", 0) == 0) {
            continue;
        }
        out.push_back(args[i]);
    }
    return out;
}

struct RunOptions {
    fs::path source;
    fs::path reference_source;
    fs::path candidate_source;
    fs::path work_dir;
    std::string reference_generator;
    std::string candidate_generator;
    std::string reference_parallel = "OFF";
    std::string candidate_parallel = "ON";
    bool skip_configure = false;
    bool show_matches = false;
    bool strict_specified = false;
    bool strict = false;
    std::vector<std::string> passthrough;
    std::vector<std::string> snapshot_patterns;
    std::vector<std::string> snapshot_ignores;
    std::vector<std::string> path_ignores;
};

static int usage()
{
    std::cerr
        << "Usage:\n"
        << "  brlcad_probe_validation diff-snapshots <ref> <cand> [--strict] [--pattern RX] [--ignore RX] [--show-matches]\n"
        << "  brlcad_probe_validation compare-dirs <refdir> <canddir> [--reference-source-root <src>] [--candidate-source-root <src>] [--ignore-path RX] [--show-matches]\n"
        << "  brlcad_probe_validation run --source <src> --work-dir <dir> [--reference-source <src>] [--candidate-source <src>]\n"
        << "                               [--reference-generator <gen>] [--candidate-generator <gen>]\n"
        << "                               [--reference-parallel ON|OFF] [--candidate-parallel ON|OFF]\n"
        << "                               [--strict|--no-strict] [--skip-configure] [--show-matches]\n"
        << "                               [--pattern RX] [--ignore RX] [--ignore-path RX] [-- <cmake args...>]\n";
    return 2;
}

static int command_diff_snapshots(const std::vector<std::string> &args)
{
    if (args.size() < 2) {
        return usage();
    }
    fs::path ref = args[0];
    fs::path cand = args[1];
    bool strict = false;
    bool show_matches = false;
    std::vector<std::string> patterns = kDefaultResultPatterns;
    std::vector<std::string> ignores = kDefaultIgnorePatterns;

    for (std::size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--strict") {
            strict = true;
            continue;
        }
        if (args[i] == "--show-matches") {
            show_matches = true;
            continue;
        }
        if (args[i] == "--pattern" && i + 1 < args.size()) {
            patterns.push_back(args[++i]);
            continue;
        }
        if (args[i] == "--ignore" && i + 1 < args.size()) {
            ignores.push_back(args[++i]);
            continue;
        }
        return usage();
    }

    SnapshotData ref_data = parse_snapshot(ref);
    SnapshotData cand_data = parse_snapshot(cand);
    auto result = diff_snapshots(ref_data, cand_data, strict,
        compile_patterns(patterns), compile_patterns(ignores));
    print_snapshot_report(ref, cand, ref_data, cand_data, result, strict, show_matches);
    return (result.value_diffs.empty() && result.only_ref.empty() && result.only_cand.empty()) ? 0 : 1;
}

static int command_compare_dirs(const std::vector<std::string> &args)
{
    if (args.size() < 2) {
        return usage();
    }
    fs::path ref = args[0];
    fs::path cand = args[1];
    fs::path ref_source_root;
    fs::path cand_source_root;
    bool show_matches = false;
    std::vector<std::string> ignore_paths = kDefaultPathIgnorePatterns;

    for (std::size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--reference-source-root" && i + 1 < args.size()) {
            ref_source_root = args[++i];
            continue;
        }
        if (args[i] == "--candidate-source-root" && i + 1 < args.size()) {
            cand_source_root = args[++i];
            continue;
        }
        if (args[i] == "--show-matches") {
            show_matches = true;
            continue;
        }
        if (args[i] == "--ignore-path" && i + 1 < args.size()) {
            ignore_paths.push_back(args[++i]);
            continue;
        }
        return usage();
    }

    auto result = compare_directories(ref, cand, compile_patterns(ignore_paths), ref_source_root, cand_source_root);
    print_directory_report(ref, cand, result, show_matches);
    return (result.diffs.empty() && result.only_ref.empty() && result.only_cand.empty()) ? 0 : 1;
}

static int command_run(const std::vector<std::string> &argv)
{
    RunOptions opts;
    std::size_t sep = argv.size();
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (argv[i] == "--") {
            sep = i;
            break;
        }
    }

    for (std::size_t i = 0; i < sep; ++i) {
        const std::string &arg = argv[i];
        if ((arg == "--source" || arg == "--reference-source" || arg == "--candidate-source" ||
             arg == "--work-dir" || arg == "--reference-generator" || arg == "--candidate-generator" ||
             arg == "--reference-parallel" || arg == "--candidate-parallel" ||
             arg == "--pattern" || arg == "--ignore" || arg == "--ignore-path") && i + 1 >= sep) {
            return usage();
        }
        if (arg == "--source") {
            opts.source = argv[++i];
            continue;
        }
        if (arg == "--reference-source") {
            opts.reference_source = argv[++i];
            continue;
        }
        if (arg == "--candidate-source") {
            opts.candidate_source = argv[++i];
            continue;
        }
        if (arg == "--work-dir") {
            opts.work_dir = argv[++i];
            continue;
        }
        if (arg == "--reference-generator") {
            opts.reference_generator = argv[++i];
            continue;
        }
        if (arg == "--candidate-generator") {
            opts.candidate_generator = argv[++i];
            continue;
        }
        if (arg == "--reference-parallel") {
            opts.reference_parallel = argv[++i];
            continue;
        }
        if (arg == "--candidate-parallel") {
            opts.candidate_parallel = argv[++i];
            continue;
        }
        if (arg == "--pattern") {
            opts.snapshot_patterns.push_back(argv[++i]);
            continue;
        }
        if (arg == "--ignore") {
            opts.snapshot_ignores.push_back(argv[++i]);
            continue;
        }
        if (arg == "--ignore-path") {
            opts.path_ignores.push_back(argv[++i]);
            continue;
        }
        if (arg == "--strict") {
            opts.strict_specified = true;
            opts.strict = true;
            continue;
        }
        if (arg == "--no-strict") {
            opts.strict_specified = true;
            opts.strict = false;
            continue;
        }
        if (arg == "--skip-configure") {
            opts.skip_configure = true;
            continue;
        }
        if (arg == "--show-matches") {
            opts.show_matches = true;
            continue;
        }
        return usage();
    }

    if (sep < argv.size()) {
        opts.passthrough.assign(argv.begin() + static_cast<std::ptrdiff_t>(sep + 1), argv.end());
    }

    if (opts.source.empty() && (opts.reference_source.empty() || opts.candidate_source.empty())) {
        return usage();
    }
    if (opts.work_dir.empty()) {
        return usage();
    }

    if (opts.reference_source.empty()) {
        opts.reference_source = opts.source;
    }
    if (opts.candidate_source.empty()) {
        opts.candidate_source = opts.source;
    }
    if (opts.candidate_generator.empty()) {
        opts.candidate_generator = opts.reference_generator;
    }

    fs::path ref_build = opts.work_dir / "reference";
    fs::path cand_build = opts.work_dir / "candidate";
    fs::path ref_snap = ref_build / "probe_snapshot.txt";
    fs::path cand_snap = cand_build / "probe_snapshot.txt";

    if (!opts.skip_configure) {
        fs::create_directories(ref_build);
        fs::create_directories(cand_build);

        auto ref_passthrough = opts.reference_generator.empty() ? opts.passthrough : strip_generator(opts.passthrough);
        auto cand_passthrough = opts.candidate_generator.empty() ? opts.passthrough : strip_generator(opts.passthrough);

        std::vector<std::string> ref_cmd = {"cmake", "-S", fs::absolute(opts.reference_source).string(),
            "-B", fs::absolute(ref_build).string()};
        if (!opts.reference_generator.empty()) {
            ref_cmd.push_back("-G");
            ref_cmd.push_back(opts.reference_generator);
        }
        ref_cmd.insert(ref_cmd.end(), ref_passthrough.begin(), ref_passthrough.end());
        ref_cmd.push_back("-DBRLCAD_ENABLE_PARALLEL_CONFIG_PROBES=" + opts.reference_parallel);
        ref_cmd.push_back("-DBRLCAD_PROBE_SNAPSHOT_FILE=" + fs::absolute(ref_snap).string());

        std::cout << "REFERENCE configure\n";
        if (run_command(ref_cmd) != 0) {
            std::cerr << "Reference configure failed.\n";
            return 2;
        }

        std::vector<std::string> cand_cmd = {"cmake", "-S", fs::absolute(opts.candidate_source).string(),
            "-B", fs::absolute(cand_build).string()};
        if (!opts.candidate_generator.empty()) {
            cand_cmd.push_back("-G");
            cand_cmd.push_back(opts.candidate_generator);
        }
        cand_cmd.insert(cand_cmd.end(), cand_passthrough.begin(), cand_passthrough.end());
        cand_cmd.push_back("-DBRLCAD_ENABLE_PARALLEL_CONFIG_PROBES=" + opts.candidate_parallel);
        cand_cmd.push_back("-DBRLCAD_PROBE_SNAPSHOT_FILE=" + fs::absolute(cand_snap).string());

        std::cout << "\nCANDIDATE configure\n";
        if (run_command(cand_cmd) != 0) {
            std::cerr << "Candidate configure failed.\n";
            return 2;
        }
    }

    bool strict = opts.strict_specified
        ? opts.strict
        : (fs::equivalent(fs::absolute(opts.reference_source), fs::absolute(opts.candidate_source))
            && opts.reference_generator == opts.candidate_generator);

    int overall = 0;

    std::vector<std::string> path_ignores = kDefaultPathIgnorePatterns;
    path_ignores.insert(path_ignores.end(), opts.path_ignores.begin(), opts.path_ignores.end());
    auto dir_result = compare_directories(ref_build, cand_build, compile_patterns(path_ignores),
        opts.reference_source, opts.candidate_source);
    print_directory_report(ref_build, cand_build, dir_result, opts.show_matches);
    if (!dir_result.diffs.empty() || !dir_result.only_ref.empty() || !dir_result.only_cand.empty()) {
        overall = 1;
    }

    if (fs::exists(ref_snap) && fs::exists(cand_snap)) {
        std::vector<std::string> patterns = kDefaultResultPatterns;
        patterns.insert(patterns.end(), opts.snapshot_patterns.begin(), opts.snapshot_patterns.end());
        std::vector<std::string> ignores = kDefaultIgnorePatterns;
        ignores.insert(ignores.end(), opts.snapshot_ignores.begin(), opts.snapshot_ignores.end());
        SnapshotData ref_data = parse_snapshot(ref_snap);
        SnapshotData cand_data = parse_snapshot(cand_snap);
        auto snap_result = diff_snapshots(ref_data, cand_data, strict,
            compile_patterns(patterns), compile_patterns(ignores));
        std::cout << "\n";
        print_snapshot_report(ref_snap, cand_snap, ref_data, cand_data, snap_result, strict, opts.show_matches);
        if (!snap_result.value_diffs.empty() || !snap_result.only_ref.empty() || !snap_result.only_cand.empty()) {
            overall = 1;
        }
    } else {
        std::cout << "\nSnapshot comparison skipped: one or both source trees did not emit probe snapshots.\n";
    }

    return overall;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        return usage();
    }

    std::string command = argv[1];
    std::vector<std::string> args(argv + 2, argv + argc);

    try {
        if (command == "diff-snapshots") {
            return command_diff_snapshots(args);
        }
        if (command == "compare-dirs") {
            return command_compare_dirs(args);
        }
        if (command == "run") {
            return command_run(args);
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 2;
    }

    return usage();
}
