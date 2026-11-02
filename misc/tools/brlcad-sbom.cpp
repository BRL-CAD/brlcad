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
#include "sha1.h"

#include <algorithm>
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

static bool safe_relative_path(const fs::path &path);

struct installed_file {
    std::string path;
    std::string owner;
    std::string sha1;
};

struct options {
    fs::path component_manifest;
    fs::path dependency_sbom;
    fs::path install_root;
    fs::path project_manifest;
    fs::path output;
    std::string version;
    std::string revision;
    std::string created;
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
        } else if (argument == "--project-manifest") {
            result.project_manifest = required_value(index, argc, argv);
        } else if (argument == "--output") {
            result.output = required_value(index, argc, argv);
        } else if (argument == "--version") {
            result.version = required_value(index, argc, argv);
        } else if (argument == "--revision") {
            result.revision = required_value(index, argc, argv);
        } else if (argument == "--created") {
            result.created = required_value(index, argc, argv);
        } else {
            throw std::runtime_error("unknown argument: " + argument);
        }
    }
    if (result.install_root.empty() || result.project_manifest.empty() || result.output.empty() ||
        result.version.empty() || result.revision.empty() || result.created.empty()) {
        throw std::runtime_error("--install-root, --project-manifest, --output, --version, --revision, and --created are required");
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
hexadecimal(const unsigned char *bytes, std::size_t count)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < count; ++index) {
        stream << std::setw(2) << static_cast<unsigned int>(bytes[index]);
    }
    return stream.str();
}

static std::string
sha1_file(const fs::path &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("unable to hash installed file " + path.string());
    }
    SHA1_CTX context;
    SHA1Init(&context);
    char buffer[64 * 1024];
    while (input) {
        input.read(buffer, sizeof(buffer));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            SHA1Update(&context, reinterpret_cast<const unsigned char *>(buffer), static_cast<uint32_t>(count));
        }
    }
    if (!input.eof()) {
        throw std::runtime_error("failed while hashing installed file " + path.string());
    }
    unsigned char digest[20];
    SHA1Final(digest, &context);
    return hexadecimal(digest, sizeof(digest));
}

static std::string
sha1_text(const std::string &text)
{
    SHA1_CTX context;
    SHA1Init(&context);
    SHA1Update(&context, reinterpret_cast<const unsigned char *>(text.data()), static_cast<uint32_t>(text.size()));
    unsigned char digest[20];
    SHA1Final(digest, &context);
    return hexadecimal(digest, sizeof(digest));
}

static std::string
relative_install_path(const fs::path &root, const fs::path &path)
{
    const fs::path absolute_root = fs::absolute(root).lexically_normal();
    const fs::path absolute_path = fs::absolute(path).lexically_normal();
    const fs::path relative = absolute_path.lexically_relative(absolute_root);
    if (!safe_relative_path(relative)) {
        throw std::runtime_error("installed path is outside the install root: " + path.string());
    }
    return relative.generic_string();
}

static std::set<std::string>
project_files(const options &opts)
{
    std::ifstream input(opts.project_manifest);
    if (!input) {
        throw std::runtime_error("unable to open " + opts.project_manifest.string());
    }
    std::set<std::string> files;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        files.insert(relative_install_path(opts.install_root, fs::path(line)));
    }
    return files;
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

struct installed_components {
    std::map<std::string, std::string> owners;
    std::map<std::string, std::set<std::string>> selected_packages;
};

static installed_components
read_installed_components(const options &opts)
{
    const json manifest = read_json(opts.component_manifest);
    if (!manifest.is_object() || (manifest.size() != 5 && manifest.size() != 6) ||
        (manifest.size() == 6 && !manifest.contains("selected_packages")) ||
        manifest.value("schema", "") != "https://brlcad.org/schema/bext-component-manifest-v1" ||
        manifest.value("install_root", "") != "install" || manifest.value("generated_by", "") != "bext" ||
        !manifest.contains("platform") || !manifest.at("platform").is_string() ||
        manifest.at("platform").get<std::string>().empty() ||
        !manifest.contains("files") || !manifest.at("files").is_array()) {
        throw std::runtime_error("unsupported or invalid bext component manifest");
    }

    installed_components result;
    for (const auto &entry : manifest.at("files")) {
        if (!entry.is_object() || entry.size() != 2 ||
            !entry.contains("path") || !entry.at("path").is_string() ||
            !entry.contains("component") || !entry.at("component").is_string()) {
            throw std::runtime_error("invalid component ownership entry");
        }
        const std::string relative = entry.at("path").get<std::string>();
        const std::string component = entry.at("component").get<std::string>();
        const fs::path relative_path(relative);
        if (component.empty() || !safe_relative_path(relative_path)) {
            throw std::runtime_error("invalid component ownership entry: " + relative);
        }
        if (!result.owners.emplace(relative_path.generic_string(), component).second) {
            throw std::runtime_error("duplicate component ownership path: " + relative);
        }
    }
    if (manifest.contains("selected_packages")) {
        if (!manifest.at("selected_packages").is_array()) {
            throw std::runtime_error("invalid selected package list");
        }
        static const std::regex spdx_id_pattern(R"(^SPDXRef-[A-Za-z0-9.-]+$)");
        for (const auto &entry : manifest.at("selected_packages")) {
            if (!entry.is_object() || entry.size() != 2 ||
                !entry.contains("component") || !entry.at("component").is_string() ||
                !entry.contains("spdx_id") || !entry.at("spdx_id").is_string()) {
                throw std::runtime_error("invalid selected package entry");
            }
            const std::string component = entry.at("component").get<std::string>();
            const std::string spdx_id = entry.at("spdx_id").get<std::string>();
            if (component.empty() || !std::regex_match(spdx_id, spdx_id_pattern)) {
                throw std::runtime_error("invalid selected package entry: " + component + ": " + spdx_id);
            }
            if (!result.selected_packages[component].insert(spdx_id).second) {
                throw std::runtime_error("duplicate selected package entry: " + component + ": " + spdx_id);
            }
        }
    }
    return result;
}

static std::vector<installed_file>
audit_installed_files(const options &opts, const std::map<std::string, std::string> &component_owners)
{
    const std::set<std::string> brlcad_files = project_files(opts);
    std::vector<installed_file> files;
    std::vector<std::string> unattributed;
    for (const auto &entry : fs::recursive_directory_iterator(opts.install_root)) {
        const fs::file_status status = entry.symlink_status();
        if (!fs::is_regular_file(status) && !fs::is_symlink(status)) {
            continue;
        }
        const std::string relative = relative_install_path(opts.install_root, entry.path());
        if (entry.path() == opts.output) {
            continue;
        }
        const auto component = component_owners.find(relative);
        const bool project_owned = brlcad_files.count(relative) != 0;
        if (component != component_owners.end() && project_owned) {
            throw std::runtime_error("installed file has conflicting BRL-CAD and bext ownership: " + relative);
        }
        std::string owner;
        if (component != component_owners.end()) {
            owner = component->second;
        } else if (project_owned) {
            owner = "BRL-CAD";
        } else {
            unattributed.push_back(relative);
            continue;
        }
        const std::string checksum = fs::is_symlink(status) ?
            sha1_text(fs::read_symlink(entry.path()).generic_string()) : sha1_file(entry.path());
        files.push_back({relative, owner, checksum});
    }
    if (!unattributed.empty()) {
        std::sort(unattributed.begin(), unattributed.end());
        std::ostringstream message;
        message << "refusing to generate an incomplete SBOM; unattributed installed files:";
        for (const auto &path : unattributed) {
            message << "\n  " << path;
        }
        throw std::runtime_error(message.str());
    }
    std::sort(files.begin(), files.end(), [](const installed_file &left, const installed_file &right) {
        return left.path < right.path;
    });
    return files;
}

static std::string
file_spdx_id(const std::string &path)
{
    SHA1_CTX context;
    SHA1Init(&context);
    SHA1Update(&context, reinterpret_cast<const unsigned char *>(path.data()), static_cast<uint32_t>(path.size()));
    unsigned char digest[20];
    SHA1Final(digest, &context);
    return "SPDXRef-File-" + hexadecimal(digest, sizeof(digest));
}

static std::string
package_verification_code(const std::vector<installed_file> &files, const std::string &owner)
{
    std::vector<std::string> owner_hashes;
    for (const auto &file : files) {
        if (file.owner == owner) {
            owner_hashes.push_back(file.sha1);
        }
    }
    std::sort(owner_hashes.begin(), owner_hashes.end());
    std::string hashes;
    for (const auto &hash : owner_hashes) {
        hashes += hash;
    }
    return sha1_text(hashes);
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

static json
brlcad_package(const options &opts, const std::vector<installed_file> &files)
{
    const bool dirty = opts.revision.find("-dirty-") != std::string::npos;
    return {
        {"name", "BRL-CAD"},
        {"SPDXID", "SPDXRef-Package-BRL-CAD"},
        {"versionInfo", opts.version},
        {"downloadLocation", dirty ? "NOASSERTION" : "https://github.com/BRL-CAD/brlcad/archive/" + opts.revision + ".tar.gz"},
        {"packageSourceInfo", "BRL-CAD source revision " + opts.revision},
        {"filesAnalyzed", true},
        {"packageVerificationCode", {{"packageVerificationCodeValue", package_verification_code(files, "BRL-CAD")}}},
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
    std::map<std::string, std::string> component_owners;
    std::map<std::string, std::set<std::string>> selected_package_ids;
    if (!opts.component_manifest.empty()) {
        installed_components components = read_installed_components(opts);
        component_owners = std::move(components.owners);
        selected_package_ids = std::move(components.selected_packages);
    }
    const std::vector<installed_file> installed_files = audit_installed_files(opts, component_owners);
    std::map<std::string, std::size_t> component_counts;
    for (const auto &file : installed_files) {
        if (file.owner != "BRL-CAD") {
            ++component_counts[file.owner];
        }
    }
    std::map<std::string, std::string> project_package_ids;

    if (!opts.component_manifest.empty()) {
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
            const std::string package_id = package.at("SPDXID").get<std::string>();
            const auto selected = selected_package_ids.find(project);
            if (selected != selected_package_ids.end() && selected->second.count(package_id) == 0) {
                continue;
            }
            package["comment"] = package.at("comment").get<std::string>() +
                "; installed files attributed to bext project " + project + ": " + std::to_string(count->second);
            selected_ids.insert(package_id);
            project_package_ids.emplace(project, package_id);
            selected_packages.push_back(std::move(package));
            resolved_projects.insert(project);
        }
        for (const auto &selection : selected_package_ids) {
            if (component_counts.count(selection.first) == 0) {
                continue;
            }
            for (const auto &package_id : selection.second) {
                if (selected_ids.count(package_id) == 0) {
                    throw std::runtime_error("selected bext package is absent from dependency SBOM: " + package_id);
                }
            }
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

    std::string identity = opts.revision;
    for (const auto &file : installed_files) {
        identity += "\n" + file.path + " " + file.sha1;
    }
    SHA1_CTX identity_context;
    SHA1Init(&identity_context);
    SHA1Update(&identity_context, reinterpret_cast<const unsigned char *>(identity.data()), static_cast<uint32_t>(identity.size()));
    unsigned char identity_digest[20];
    SHA1Final(identity_digest, &identity_context);
    json result = {
        {"spdxVersion", "SPDX-2.3"},
        {"dataLicense", "CC0-1.0"},
        {"SPDXID", "SPDXRef-DOCUMENT"},
        {"name", "BRL-CAD-" + opts.version + "-installed"},
        {"documentNamespace", "https://github.com/BRL-CAD/brlcad/sbom/" + opts.revision + "/" + hexadecimal(identity_digest, sizeof(identity_digest))},
        {"creationInfo", {{"created", opts.created}, {"creators", json::array({"Tool: brlcad-sbom"})}}},
        {"packages", json::array()},
        {"files", json::array()},
        {"relationships", json::array()}
    };
    if (!extracted_licenses.empty()) {
        result["hasExtractedLicensingInfos"] = std::move(extracted_licenses);
    }

    result["packages"].push_back(brlcad_package(opts, installed_files));
    for (auto &package : selected_packages) {
        const std::string project = project_from_comment(package);
        const auto primary = project_package_ids.find(project);
        if (primary != project_package_ids.end() && primary->second == package.at("SPDXID").get<std::string>()) {
            package["filesAnalyzed"] = true;
            package["packageVerificationCode"] = {{"packageVerificationCodeValue", package_verification_code(installed_files, project)}};
        }
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
    for (const auto &file : installed_files) {
        const std::string id = file_spdx_id(file.path);
        result["files"].push_back({
            {"fileName", "./" + file.path},
            {"SPDXID", id},
            {"checksums", json::array({{{"algorithm", "SHA1"}, {"checksumValue", file.sha1}}})},
            {"licenseConcluded", "NOASSERTION"},
            {"copyrightText", "NOASSERTION"}
        });
        std::string owner_id = "SPDXRef-Package-BRL-CAD";
        if (file.owner != "BRL-CAD") {
            const auto owner = project_package_ids.find(file.owner);
            if (owner == project_package_ids.end()) {
                throw std::runtime_error("no package owns installed file: " + file.path);
            }
            owner_id = owner->second;
        }
        result["relationships"].push_back({
            {"spdxElementId", owner_id},
            {"relationshipType", "CONTAINS"},
            {"relatedSpdxElement", id}
        });
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
        std::error_code remove_error;
        fs::remove(opts.output, remove_error);
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
