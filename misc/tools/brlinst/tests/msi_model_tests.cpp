// SPDX-License-Identifier: BSD-2-Clause
#include "installer/msi.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
using namespace installer;
using namespace installer::msi;
namespace {
constexpr char banner_bitmap_key[] = "BrlinstBannerBmp";
constexpr char dialog_bitmap_key[] = "BrlinstDialogBmp";

void require(bool condition, const std::string &message) {
    if (!condition)
        throw Error(message);
}
template <class F> void rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "Invalid MSI input was accepted");
}
void write(const fs::path &p, const std::string &text) {
    std::ofstream(p, std::ios::binary) << text;
}
const Json &by_key(Model &m, const char *table, const std::string &key) {
    for (const auto &row : m.table(table).rows)
        if (row[0] == key)
            return row;
    throw Error("Missing MSI row: " + key);
}
const Json &by_keys(Model &m, const char *table, const std::string &first, const std::string &second) {
    for (const auto &row : m.table(table).rows)
        if (row[0] == first && row[1] == second)
            return row;
    throw Error("Missing MSI row: " + first + "/" + second);
}
bool has_key(Model &m, const char *table, const std::string &key) {
    for (const auto &row : m.table(table).rows)
        if (row[0] == key)
            return true;
    return false;
}
// Check relational integrity independently of the generator. This catches
// broken control navigation, components, file keys and schema changes on Unix.
void relational(Model &m) {
    for (const auto &table : m.tables) {
        std::set<std::string> keys;
        for (const auto &row : table.rows) {
            require(row.size() == table.columns.size(), "Row width: " + table.name);
            Json key = Json::array();
            for (std::size_t i = 0; i < table.columns.size(); ++i) {
                const auto &column = table.columns[i];
                const auto &value = row[i];
                if (column.key)
                    key.push_back(value);
                require(column.nullable || !value.is_null(),
                        "Missing field: " + table.name + "." + column.name);
                if (value.is_null())
                    continue;
                if (column.type == "SHORT" || column.type == "LONG") {
                    require(value.is_number_integer(), "Non-integer MSI field");
                    if (column.type == "SHORT")
                        require(value.get<int>() >= -32767 && value.get<int>() <= 32767, "SHORT overflow");
                } else if (column.type == "OBJECT")
                    require(value.is_object() && value.contains("stream"), "Invalid binary stream");
                else {
                    require(value.is_string(), "Non-string MSI field");
                    auto length = std::stoi(column.type.substr(5));
                    if (length)
                        require(value.get<std::string>().size() <= static_cast<std::size_t>(length),
                                "Oversized field: " + table.name + "." + column.name);
                }
                if (!column.foreign_table.empty()) {
                    const auto &foreign = m.table(column.foreign_table);
                    bool found = false;
                    for (const auto &candidate : foreign.rows) {
                        bool matches = candidate[column.foreign_column - 1] == value;
                        for (int prefix = 0; prefix < column.foreign_column - 1; ++prefix)
                            matches = matches && candidate[prefix] == row[prefix];
                        found = found || matches;
                    }
                    require(found, "Broken foreign key: " + table.name + "." + column.name);
                }
            }
            require(keys.insert(key.dump()).second, "Duplicate primary key: " + table.name);
        }
    }
    for (const auto &row : m.table("Component").rows) {
        const auto table = row[3].get<int>() & 4 ? "Registry" : "File";
        by_key(m, table, row[5]);
    }
    for (const auto &row : m.table("ControlEvent").rows) {
        if (row[2] == "NewDialog" || row[2] == "SpawnDialog")
            by_key(m, "Dialog", row[3]);
        if (row[2] == "DoAction")
            by_key(m, "CustomAction", row[3]);
    }
    for (const auto &d : m.table("Dialog").rows) {
        std::map<std::string, std::string> next;
        for (const auto &c : m.table("Control").rows)
            if (c[0] == d[0] && !c[10].is_null())
                next[c[1]] = c[10];
        for (const auto &c : next) {
            std::set<std::string> visited;
            auto cursor = c.first;
            while (visited.insert(cursor).second) {
                require(next.count(cursor), "Tab navigation leads to an unfocusable control: " +
                                                d[0].get<std::string>() + "/" + c.first);
                cursor = next.at(cursor);
            }
            require(cursor == c.first,
                    "Tab navigation is not a closed cycle: " + d[0].get<std::string>() + "/" + c.first);
        }
        for (int field : {7, 8, 9})
            if (!d[field].is_null()) {
                bool found = false;
                for (const auto &c : m.table("Control").rows)
                    found = found || (c[0] == d[0] && c[1] == d[field]);
                require(found, "Missing dialog focus/default/cancel control: " +
                                   d[0].get<std::string>() + "/" + d[field].get<std::string>());
            }
    }
}
} // namespace
int main() {
    auto root = fs::temp_directory_path() / ("brlinst-msi-model-" + std::to_string(std::random_device{}()));
    fs::create_directories(root / "payload/bin");
    try {
        write(root / "license.txt", "Test license {with} [brackets] and Unicode: 模型\n");
        write(root / "payload/bin/app.exe", "Test executable");
        write(root / "payload/data.txt", "Test data");
        fs::create_directories(root / "payload/share/doc");
        write(root / "payload/share/doc/manual.txt", "Nested documentation");
        write(root / "NOTICE.txt", "Notices");
        write(root / "actions.dll", "Test DLL");
        Json config{{"formatVersion", 1},
                    {"id", "test.msi"},
                    {"name", "MSI Test"},
                    {"version", "1.2.3"},
                    {"architecture", "x64"},
                    {"publisher", "Test"},
                    {"license", "license.txt"},
                    {"shortcuts", Json::array({{{"name", "Application"},
                                                {"target", "bin/app.exe"},
                                                {"arguments", "--title [literal]"}},
                                               {{"name", "Data"},
                                                {"target", "data.txt"},
                                                {"desktop", false},
                                                {"workingDirectory", "."}}})}};
        write(root / "product.json", config.dump());
        auto input = prepare_windows_input(root / "product.json", root / "payload", root / "test.msi");
        // A deterministic stand-in lets tests check the complete identity inputs
        // without requiring Windows CNG. Windows tests check real GUIDs as well.
        std::map<std::string, std::string> ids;
        auto guid = [&](const std::string &key) {
            auto inserted = ids.emplace(key, "");
            if (inserted.second) {
                auto value = std::to_string(ids.size());
                inserted.first->second =
                    "{00000000-0000-8000-8000-" + std::string(12 - value.size(), '0') + value + "}";
            }
            return inserted.first->second;
        };
        auto model = make_model(input, root / "NOTICE.txt", root / "actions.dll", guid);
        relational(model);
        for (const auto &file : model.files)
            require(file.path.find('\\') == std::string::npos,
                    "Windows separators leaked into payload manifest");
        require(model.files.size() == 4, "Missing payload or notices");
        require(model.table("Shortcut").rows.size() == 3, "Desktop eligibility ignored");
        require(by_key(model, "Property", "ALLUSERS")[1] == "2", "Dual-purpose package missing");
        require(by_key(model, "Property", "MSIINSTALLPERUSER")[1] == "1", "Per-user default missing");
        require(!has_key(model, "Property", "BRLINST_PATH"), "MSI PATH option is not opt-in");
        require(by_key(model, "Component", "UserPath")[4] ==
                        "BRLINST_PATH = \"1\" AND (NOT ALLUSERS OR (ALLUSERS = \"2\" AND MSIINSTALLPERUSER = \"1\"))" &&
                    by_key(model, "Component", "MachinePath")[4] ==
                        "BRLINST_PATH = \"1\" AND (ALLUSERS = \"1\" OR (ALLUSERS = \"2\" AND NOT MSIINSTALLPERUSER))",
                "MSI PATH components ignore the PATH option");
        require(has_key(model, "Dialog", "BrowseDlg"), "MSI folder browser dialog missing");
        const auto &directory_control = by_keys(model, "Control", "BrowseDlg", "Directory");
        require(directory_control[2] == "DirectoryCombo" && directory_control[7] == 11 &&
                    directory_control[8] == "_BrowseProperty",
                "MSI folder browser is not bound indirectly to the install directory");
        bool opens_browser = false;
        bool sets_browse_property = false;
        bool selects_user_scope = false;
        bool selects_machine_scope = false;
        for (const auto &event : model.table("ControlEvent").rows) {
            if (event[0] == "DestinationDlg" && event[1] == "Browse") {
                opens_browser = opens_browser || (event[2] == "SpawnDialog" && event[3] == "BrowseDlg");
                sets_browse_property =
                    sets_browse_property || (event[2] == "[_BrowseProperty]" && event[3] == "INSTALLDIR");
            }
            if (event[0] == "ScopeDlg" && event[1] == "Next" && event[2] == "[ALLUSERS]") {
                selects_user_scope = selects_user_scope ||
                                     (event[3] == "2" && event[4] == "BRLINSTSCOPE = \"user\"");
                selects_machine_scope = selects_machine_scope ||
                                        (event[3] == "1" && event[4] == "BRLINSTSCOPE = \"machine\"");
            }
        }
        require(opens_browser && sets_browse_property, "MSI Browse button is not wired to BrowseDlg");
        require(selects_user_scope && selects_machine_scope, "MSI scope dialog does not set ALLUSERS");
        require(by_key(model, "InstallUISequence", "Clear_BRLINST_PATH")[1] == "BRLINST_PATH = \"0\"",
                "Unchecked MSI PATH option is not cleared");
        bool automatic_upgrade = false;
        for (const auto &r : model.table("InstallExecuteSequence").rows)
            automatic_upgrade = automatic_upgrade || r[0] == "RemoveExistingProducts";
        require(!automatic_upgrade, "Automatic upgrading was authored");
        require(by_key(model, "InstallExecuteSequence", "BrlinstCheck")[2].get<int>() <
                    by_key(model, "InstallExecuteSequence", "InstallInitialize")[2].get<int>(),
                "Destination preflight runs after mutation starts");
        require(by_key(model, "ActionText", "CostInitialize")[1] == "Calculating required disk space" &&
                    by_key(model, "ActionText", "BrlinstCheck")[1] == "Checking installation options" &&
                    by_key(model, "ActionText", "InstallInitialize")[1] ==
                        "Starting the installation transaction",
                "MSI early installation phases do not report useful progress");
        require(by_key(model, "LaunchCondition", "VersionNT64")[1] ==
                    "This package requires 64-bit Windows.",
                "64-bit Windows launch condition missing");
        const auto generated_license =
            by_keys(model, "Control", "LicenseDlg", "License")[9].get<std::string>();
        require(generated_license.find("\\'5bbrackets\\'5d") != std::string::npos,
                "Generated MSI license did not escape formatted-field delimiters");
        require(generated_license.find("\\u27169?") != std::string::npos,
                "Generated MSI license did not preserve Unicode");
        require(by_key(model, "Dialog", "WelcomeDlg")[3] == 400 &&
                    by_key(model, "Dialog", "WelcomeDlg")[4] == 300,
                "Unbranded dialog dimensions changed");
        require(!has_key(model, "Binary", banner_bitmap_key) &&
                    !has_key(model, "Binary", dialog_bitmap_key),
                "Unbranded MSI contains branding streams");
        auto arm_input = input;
        arm_input.product["architecture"] = "arm64";
        auto arm_model = make_model(arm_input, root / "NOTICE.txt", root / "actions.dll", guid);
        require(arm_model.architecture == Architecture::arm64, "ARM64 model architecture missing");
        require(arm_model.name == "MSI Test 1.2.3 (arm64, MSI)", "ARM64 product name is incorrect");
        require(arm_model.product_code != model.product_code, "Architectures share a ProductCode");
        require(by_key(arm_model, "Property", "UpgradeCode")[1] !=
                    by_key(model, "Property", "UpgradeCode")[1],
                "Architectures share an UpgradeCode");

        write(root / "generic.ico", "generic icon");
        write(root / "msi.ico", "MSI icon");
        write(root / "banner.bmp", "banner");
        write(root / "dialog.bmp", "dialog");
        write(root / "license.rtf", "{\\rtf1 Branded license}");
        auto classic_input = input;
        classic_input.product["branding"] = {{"style", "classic"}};
        classic_input.icon = root / "generic.ico";
        classic_input.branding.msi_icon = root / "msi.ico";
        classic_input.branding.msi_banner = root / "banner.bmp";
        classic_input.branding.msi_dialog = root / "dialog.bmp";
        classic_input.branding.msi_license = root / "license.rtf";
        auto classic = make_model(classic_input, root / "NOTICE.txt", root / "actions.dll", guid);
        relational(classic);
        require(by_key(classic, "Binary", banner_bitmap_key)[1]["stream"] == utf8(root / "banner.bmp"),
                "Classic MSI banner stream missing");
        require(by_key(classic, "Binary", dialog_bitmap_key)[1]["stream"] == utf8(root / "dialog.bmp"),
                "Classic MSI dialog stream missing");
        require(by_key(classic, "Icon", "Product.ico")[1]["stream"] == utf8(root / "msi.ico"),
                "Classic MSI icon did not override the generic icon");
        require(by_keys(classic, "Control", "LicenseDlg", "License")[9] == "{\\rtf1 Branded license}",
                "Classic MSI license did not override generated RTF");
        for (auto dialog : {"WelcomeDlg", "LicenseDlg", "ScopeDlg", "DestinationDlg", "OptionsDlg",
                            "ReadyDlg", "RemoveDlg", "ProgressDlg", "FinishedDlg", "CancelledDlg",
                            "FailedDlg", "FilesInUse"}) {
            const auto &dialog_row = by_key(classic, "Dialog", dialog);
            require(dialog_row[3] == 370 && dialog_row[4] == 270,
                    std::string("Classic MSI dialog dimensions: ") + dialog);
        }
        for (auto dialog : {"WelcomeDlg", "FinishedDlg", "CancelledDlg", "FailedDlg"}) {
            const auto &bitmap = by_keys(classic, "Control", dialog, "DialogBitmap");
            require(bitmap[2] == "Bitmap" && bitmap[5] == 370 && bitmap[6] == 234 &&
                        bitmap[9] == dialog_bitmap_key,
                    std::string("Classic MSI dialog image: ") + dialog);
        }
        for (auto dialog : {"LicenseDlg", "ScopeDlg", "DestinationDlg", "OptionsDlg", "ReadyDlg",
                            "RemoveDlg", "ProgressDlg", "FilesInUse"}) {
            const auto &bitmap = by_keys(classic, "Control", dialog, "BannerBitmap");
            require(bitmap[2] == "Bitmap" && bitmap[5] == 370 && bitmap[6] == 44 &&
                        bitmap[9] == banner_bitmap_key,
                    std::string("Classic MSI banner image: ") + dialog);
        }
        const auto &classic_next = by_keys(classic, "Control", "WelcomeDlg", "Next");
        require(classic_next[3] == 236 && classic_next[4] == 243 && classic_next[5] == 56 &&
                    classic_next[6] == 17,
                "Classic MSI navigation was not compacted");
        const auto &classic_welcome = by_keys(classic, "Control", "WelcomeDlg", "Body");
        require(classic_welcome[3] == 138 && classic_welcome[4] == 45 &&
                    classic_welcome[5] == 217 && classic_welcome[6] == 176,
                "Classic MSI welcome text does not align with the EXE layout");
        const auto &classic_title = by_keys(classic, "Control", "LicenseDlg", "Title");
        require(classic_title[3] == 119, "Classic MSI title overlaps the shared header artwork");
        classic_input.branding.msi_icon.clear();
        auto fallback = make_model(classic_input, root / "NOTICE.txt", root / "actions.dll", guid);
        require(by_key(fallback, "Icon", "Product.ico")[1]["stream"] == utf8(root / "generic.ico"),
                "MSI icon did not fall back to the generic product icon");

        auto old = model;
        input.product["version"] = "2.0.0";
        auto newer = make_model(input, root / "NOTICE.txt", root / "actions.dll", guid);
        require(newer.product_code != old.product_code, "Versions share a ProductCode");
        std::set<std::string> old_components;
        for (const auto &r : old.table("Component").rows)
            old_components.insert(r[1]);
        for (const auto &r : newer.table("Component").rows)
            require(!old_components.count(r[1]), "Versions share a component GUID");
        input.product["version"] = "1.2.3";
        input.files.insert(input.files.begin(), {"aardvark.txt", root / "payload/data.txt"});
        auto changed = make_model(input, root / "NOTICE.txt", root / "actions.dll", guid);
        require(changed.product_code == old.product_code, "File list changes ProductCode");
        auto component_guid = [](Model &m, const std::string &path) {
            for (const auto &f : m.files)
                if (f.path == path) {
                    auto component = by_key(m, "File", f.id)[1];
                    return by_key(m, "Component", component)[1];
                }
            throw Error("Missing test file");
        };
        require(component_guid(changed, "bin/app.exe") == component_guid(old, "bin/app.exe"),
                "Component identity depends on enumeration order");
        for (auto v : {"1", "1.2", "1.2.3.4", "01.2.3", "256.1.1", "1.256.1", "1.1.65536", "1.2.3-beta"}) {
            input.product["version"] = v;
            rejects([&] { make_model(input, root / "NOTICE.txt", root / "actions.dll", guid); });
        }
        require(formatted_literal("a[b]c") == "a[\\[]b[\\]]c", "Literal MSI property escaping failed");
        fs::remove_all(root);
        std::cout << "MSI model tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        fs::remove_all(root);
        return 1;
    }
}
