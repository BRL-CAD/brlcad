/*                 B R L C A D - S B O M . C P P
 * BRL-CAD
 *
 * Published in 2026 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file brlcad-sbom.cpp
 *
 */

#include "json.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

struct options {
    fs::path component_manifest;
    fs::path dependency_sbom;
    fs::path install_root;
    fs::path output;
    std::string version;
    std::string revision;
};

static std::string
required_value(int &index, int argc, char **argv)
{
    if (++index >= argc) {
        throw std::runtime_error(std::string("missing value for ") + argv[index - 1]);
    }
    return argv[index];
}

static options
parse_options(int argc, char **argv)
{
    options result;
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--component-manifest") {
            result.component_manifest = required_value(index, argc, argv);
        } else if (argument == "--dependency-sbom") {
            result.dependency_sbom = required_value(index, argc, argv);
        } else if (argument == "--install-root") {
            result.install_root = required_value(index, argc, argv);
        } else if (argument == "--output") {
            result.output = required_value(index, argc, argv);
        } else if (argument == "--version") {
            result.version = required_value(index, argc, argv);
        } else if (argument == "--revision") {
            result.revision = required_value(index, argc, argv);
        } else {
            throw std::runtime_error("unknown argument: " + argument);
        }
    }
    if (result.install_root.empty() || result.output.empty() || result.version.empty() || result.revision.empty()) {
        throw std::runtime_error("--install-root, --output, --version, and --revision are required");
    }
    if (result.component_manifest.empty() != result.dependency_sbom.empty()) {
        throw std::runtime_error("--component-manifest and --dependency-sbom must be specified together");
    }
    return result;
}

static json
read_json(const fs::path &path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("unable to open " + path.string());
    }
    json value;
    input >> value;
    return value;
}

static std::string
project_from_comment(const json &package)
{
    const std::string comment = package.at("comment").get<std::string>();
    static const std::regex project_pattern(R"((?:^|;\s*)bext project:\s*([^;]+))");
    std::smatch match;
    if (!std::regex_search(comment, match, project_pattern)) {
        throw std::runtime_error("bext package has no project association: " + package.at("SPDXID").get<std::string>());
    }
    return match[1].str();
}

static bool
safe_relative_path(const fs::path &path)
{
    if (path.empty() || path.is_absolute() || path.has_root_path()) {
        return false;
    }
    for (const auto &part : path) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

static std::map<std::string, std::size_t>
installed_component_counts(const options &opts)
{
    const json manifest = read_json(opts.component_manifest);
    if (manifest.value("schema", 0) != 1 || !manifest.contains("files") || !manifest.at("files").is_array()) {
        throw std::runtime_error("unsupported or invalid bext component manifest");
    }

    std::map<std::string, std::size_t> counts;
    std::set<std::string> paths;
    for (const auto &entry : manifest.at("files")) {
        const std::string relative = entry.at("path").get<std::string>();
        const std::string component = entry.at("component").get<std::string>();
        const fs::path relative_path(relative);
        if (component.empty() || !safe_relative_path(relative_path)) {
            throw std::runtime_error("invalid component ownership entry: " + relative);
        }
        if (!paths.insert(relative).second) {
            throw std::runtime_error("duplicate component ownership path: " + relative);
        }
        if (fs::is_regular_file(opts.install_root / relative_path)) {
            ++counts[component];
        }
    }
    return counts;
}

static std::set<std::string>
license_references(const std::vector<json> &packages)
{
    static const std::regex license_pattern(R"(LicenseRef-[A-Za-z0-9.-]+)");
    std::set<std::string> references;
    for (const auto &package : packages) {
        for (const char *field : {"licenseDeclared", "licenseConcluded"}) {
            const std::string expression = package.value(field, "");
            for (std::sregex_iterator match(expression.begin(), expression.end(), license_pattern), end; match != end; ++match) {
                references.insert(match->str());
            }
        }
    }
    return references;
}

static std::string
utc_timestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t value = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &value);
#else
    gmtime_r(&value, &utc);
#endif
    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

static json
brlcad_package(const options &opts)
{
    return {
        {"name", "BRL-CAD"},
        {"SPDXID", "SPDXRef-Package-BRL-CAD"},
        {"versionInfo", opts.version},
        {"downloadLocation", "https://github.com/BRL-CAD/brlcad/archive/" + opts.revision + ".tar.gz"},
        {"packageSourceInfo", "BRL-CAD source revision " + opts.revision},
        {"filesAnalyzed", false},
        {"primaryPackagePurpose", "APPLICATION"},
        {"licenseConcluded", "LGPL-2.1-or-later"},
        {"licenseDeclared", "LGPL-2.1-or-later"},
        {"copyrightText", "Copyright U.S. Government and BRL-CAD contributors"},
        {"externalRefs", json::array({{{"referenceCategory", "PACKAGE-MANAGER"}, {"referenceType", "purl"}, {"referenceLocator", "pkg:github/BRL-CAD/brlcad@" + opts.version}}})},
        {"comment", "The unified BRL-CAD work is distributed under LGPL-2.1-or-later; separately identified files retain the file-level terms documented in the installed legal materials."}
    };
}

static json
generate_sbom(const options &opts)
{
    std::vector<json> selected_packages;
    std::set<std::string> selected_ids;
    std::set<std::string> dependency_ids;
    json retained_relationships = json::array();
    json extracted_licenses = json::array();

    if (!opts.component_manifest.empty()) {
        const auto component_counts = installed_component_counts(opts);
        const json dependency_sbom = read_json(opts.dependency_sbom);
        if (dependency_sbom.value("spdxVersion", "") != "SPDX-2.3" ||
            !dependency_sbom.contains("packages") || !dependency_sbom.at("packages").is_array()) {
            throw std::runtime_error("unsupported or invalid bext SPDX document");
        }

        std::set<std::string> resolved_projects;
        for (auto package : dependency_sbom.at("packages")) {
            const std::string project = project_from_comment(package);
            const auto count = component_counts.find(project);
            if (count == component_counts.end()) {
                continue;
            }
            package["comment"] = package.at("comment").get<std::string>() +
                "; installed files attributed to bext project " + project + ": " + std::to_string(count->second);
            selected_ids.insert(package.at("SPDXID").get<std::string>());
            selected_packages.push_back(std::move(package));
            resolved_projects.insert(project);
        }
        for (const auto &component : component_counts) {
            if (resolved_projects.count(component.first) == 0) {
                throw std::runtime_error("no bext SPDX package describes installed component: " + component.first);
            }
        }

        if (!dependency_sbom.contains("relationships") || !dependency_sbom.at("relationships").is_array()) {
            throw std::runtime_error("bext SPDX document has no relationship list");
        }
        for (const auto &relationship : dependency_sbom.at("relationships")) {
            const std::string source = relationship.at("spdxElementId").get<std::string>();
            const std::string type = relationship.at("relationshipType").get<std::string>();
            const std::string target = relationship.at("relatedSpdxElement").get<std::string>();
            if (source == "SPDXRef-DOCUMENT" && type == "DESCRIBES" && selected_ids.count(target) != 0) {
                dependency_ids.insert(target);
            } else if (selected_ids.count(source) != 0 && selected_ids.count(target) != 0) {
                retained_relationships.push_back(relationship);
            }
        }

        const auto required_licenses = license_references(selected_packages);
        if (!required_licenses.empty()) {
            if (!dependency_sbom.contains("hasExtractedLicensingInfos") ||
                !dependency_sbom.at("hasExtractedLicensingInfos").is_array()) {
                throw std::runtime_error("bext SPDX document omits required extracted license definitions");
            }
            std::set<std::string> found_licenses;
            for (const auto &license : dependency_sbom.at("hasExtractedLicensingInfos")) {
                const std::string id = license.at("licenseId").get<std::string>();
                if (required_licenses.count(id) != 0) {
                    extracted_licenses.push_back(license);
                    found_licenses.insert(id);
                }
            }
            if (found_licenses != required_licenses) {
                throw std::runtime_error("bext SPDX document does not define every referenced LicenseRef");
            }
        }
    }

    const std::string created = utc_timestamp();
    const auto nonce = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    json result = {
        {"spdxVersion", "SPDX-2.3"},
        {"dataLicense", "CC0-1.0"},
        {"SPDXID", "SPDXRef-DOCUMENT"},
        {"name", "BRL-CAD-" + opts.version + "-installed"},
        {"documentNamespace", "https://github.com/BRL-CAD/brlcad/sbom/" + opts.revision + "/" + std::to_string(nonce)},
        {"creationInfo", {{"created", created}, {"creators", json::array({"Tool: brlcad-sbom"})}}},
        {"packages", json::array()},
        {"relationships", json::array()}
    };
    if (!extracted_licenses.empty()) {
        result["hasExtractedLicensingInfos"] = std::move(extracted_licenses);
    }

    result["packages"].push_back(brlcad_package(opts));
    for (auto &package : selected_packages) {
        result["packages"].push_back(std::move(package));
    }
    result["relationships"].push_back({
        {"spdxElementId", "SPDXRef-DOCUMENT"},
        {"relationshipType", "DESCRIBES"},
        {"relatedSpdxElement", "SPDXRef-Package-BRL-CAD"}
    });
    for (const auto &dependency_id : dependency_ids) {
        result["relationships"].push_back({
            {"spdxElementId", "SPDXRef-DOCUMENT"},
            {"relationshipType", "DESCRIBES"},
            {"relatedSpdxElement", dependency_id}
        });
        result["relationships"].push_back({
            {"spdxElementId", "SPDXRef-Package-BRL-CAD"},
            {"relationshipType", "DEPENDS_ON"},
            {"relatedSpdxElement", dependency_id}
        });
    }
    for (auto &relationship : retained_relationships) {
        result["relationships"].push_back(std::move(relationship));
    }
    return result;
}

static void
write_json(const fs::path &path, const json &value)
{
    fs::create_directories(path.parent_path());
    const fs::path temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            throw std::runtime_error("unable to write " + temporary.string());
        }
        output << std::setw(2) << value << '\n';
        if (!output) {
            throw std::runtime_error("failed while writing " + temporary.string());
        }
    }
    std::error_code error;
    fs::remove(path, error);
    error.clear();
    fs::rename(temporary, path, error);
    if (error) {
        fs::remove(temporary);
        throw std::runtime_error("unable to publish " + path.string() + ": " + error.message());
    }
}

int
main(int argc, char **argv)
{
    try {
        const options opts = parse_options(argc, argv);
        write_json(opts.output, generate_sbom(opts));
        std::cout << "Generated installed BRL-CAD SPDX 2.3 SBOM: " << opts.output.string() << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "brlcad-sbom: " << error.what() << '\n';
        return 1;
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
