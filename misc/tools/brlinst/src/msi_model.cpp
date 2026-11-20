// SPDX-License-Identifier: BSD-2-Clause
#include "installer/msi.hpp"
#include <algorithm>
#include <climits>
#include <map>
#include <set>
#include <sstream>
namespace installer::msi {
Table &Model::table(const std::string &name) {
    for (auto &t : tables)
        if (t.name == name)
            return t;
    throw Error("Missing MSI table: " + name);
}
std::string formatted_literal(const std::string &s) {
    std::string r;
    for (auto c : s) {
        if (c == '[')
            r += "[\\[]";
        else if (c == ']')
            r += "[\\]]";
        else if (c == '{')
            r += "[\\{]";
        else if (c == '}')
            r += "[\\}]";
        else
            r += c;
    }
    return r;
}
std::string sql_create(const Table &t) {
    std::string s = "CREATE TABLE `" + t.name + "` (";
    for (std::size_t i = 0; i < t.columns.size(); ++i) {
        const auto &c = t.columns[i];
        if (i)
            s += ", ";
        s += "`" + c.name + "` " + c.type;
        if (!c.nullable)
            s += " NOT NULL";
        if (c.localized)
            s += " LOCALIZABLE";
    }
    s += " PRIMARY KEY ";
    bool first = true;
    for (const auto &c : t.columns)
        if (c.key) {
            if (!first)
                s += ", ";
            s += "`" + c.name + "`";
            first = false;
        }
    if (first)
        throw Error("MSI table lacks a primary key");
    return s + ")";
}
namespace {
constexpr char banner_bitmap_key[] = "BrlinstBannerBmp";
constexpr char dialog_bitmap_key[] = "BrlinstDialogBmp";

Column str(const char *n, int length = 72, bool nullable = true, bool key = false,
           const char *category = "Identifier", bool localized = false, const char *foreign = "",
           int foreign_column = 1) {
    return {n,       "CHAR(" + std::to_string(length) + ")", category, nullable, key, localized,
            foreign, *foreign ? foreign_column : 0};
}
Column num(const char *n, bool nullable = true, bool key = false, bool wide = false) {
    return {n, wide ? "LONG" : "SHORT", "", nullable, key, false, "", 0};
}
Column binary(const char *n) {
    return {n, "OBJECT", "Binary", false, false, false, "", 0};
}
void schema(Model &m) {
    auto add = [&](const char *n, std::initializer_list<Column> c) { m.tables.push_back({n, c, {}}); };
    add("Property", {str("Property", 72, false, true), str("Value", 0, false, false, "Text", true)});
    add("Directory", {str("Directory", 72, false, true),
                      str("Directory_Parent", 72, true, false, "Identifier", false, "Directory"),
                      str("DefaultDir", 255, false, false, "DefaultDir", true)});
    add("Component",
        {str("Component", 72, false, true), str("ComponentId", 38, true, false, "Guid"),
         str("Directory_", 72, false, false, "Identifier", false, "Directory"), num("Attributes", false),
         str("Condition", 255, true, false, "Condition"), str("KeyPath")});
    add("Feature",
        {str("Feature", 38, false, true),
         str("Feature_Parent", 38, true, false, "Identifier", false, "Feature"),
         str("Title", 64, true, false, "Text", true), str("Description", 255, true, false, "Text", true),
         num("Display"), num("Level", false),
         str("Directory_", 72, true, false, "Identifier", false, "Directory"), num("Attributes", false)});
    add("FeatureComponents", {str("Feature_", 38, false, true, "Identifier", false, "Feature"),
                              str("Component_", 72, false, true, "Identifier", false, "Component")});
    add("File",
        {str("File", 72, false, true), str("Component_", 72, false, false, "Identifier", false, "Component"),
         str("FileName", 255, false, false, "Filename", true), num("FileSize", false, false, true),
         str("Version", 72, true, false, "Version"), str("Language", 20, true, false, "Language"),
         num("Attributes"), num("Sequence", false, false, true)});
    add("MsiFileHash", {str("File_", 72, false, true, "Identifier", false, "File"), num("Options", false),
                        num("HashPart1", false, false, true), num("HashPart2", false, false, true),
                        num("HashPart3", false, false, true), num("HashPart4", false, false, true)});
    add("Media",
        {num("DiskId", false, true), num("LastSequence", false, false, true),
         str("DiskPrompt", 64, true, false, "Text", true), str("Cabinet", 255, true, false, "Cabinet"),
         str("VolumeLabel", 32, true, false, "Text"), str("Source", 72, true, false, "Property")});
    add("Registry",
        {str("Registry", 72, false, true), num("Root", false), str("Key", 255, false, false, "RegPath", true),
         str("Name", 255, true, false, "Formatted", true), str("Value", 0, true, false, "Formatted", true),
         str("Component_", 72, false, false, "Identifier", false, "Component")});
    add("Shortcut",
        {str("Shortcut", 72, false, true),
         str("Directory_", 72, false, false, "Identifier", false, "Directory"),
         str("Name", 128, false, false, "Filename", true),
         str("Component_", 72, false, false, "Identifier", false, "Component"),
         str("Target", 255, false, false, "Shortcut"), str("Arguments", 255, true, false, "Formatted", true),
         str("Description", 255, true, false, "Text", true), num("Hotkey"),
         str("Icon_", 72, true, false, "Identifier", false, "Icon"), num("IconIndex"), num("ShowCmd"),
         str("WkDir", 72, true, false, "Identifier")});
    add("Icon", {str("Name", 72, false, true), binary("Data")});
    add("Binary", {str("Name", 72, false, true), binary("Data")});
    add("Environment", {str("Environment", 72, false, true), str("Name", 255, false, false, "Text", true),
                        str("Value", 255, true, false, "Formatted", true),
                        str("Component_", 72, false, false, "Identifier", false, "Component")});
    add("RemoveFile", {str("FileKey", 72, false, true),
                       str("Component_", 72, false, false, "Identifier", false, "Component"),
                       str("FileName", 255, true, false, "Filename", true),
                       str("DirProperty", 72, false, false, "Identifier"), num("InstallMode", false)});
    add("LaunchCondition", {str("Condition", 255, false, true, "Condition"),
                            str("Description", 255, false, false, "Formatted", true)});
    add("CustomAction",
        {str("Action", 72, false, true), num("Type", false), str("Source", 72, true, false, "CustomSource"),
         str("Target", 255, true, false, "Formatted")});
    for (auto name :
         {"InstallUISequence", "InstallExecuteSequence", "AdminUISequence", "AdminExecuteSequence"})
        add(name, {str("Action", 72, false, true), str("Condition", 255, true, false, "Condition"),
                   num("Sequence")});
    add("Dialog", {str("Dialog", 72, false, true), num("HCentering", false), num("VCentering", false),
                   num("Width", false), num("Height", false), num("Attributes", true, false, true),
                   str("Title", 128, true, false, "Formatted", true),
                   str("Control_First", 50, false, false, "Identifier"), str("Control_Default", 50),
                   str("Control_Cancel", 50)});
    add("Control", {str("Dialog_", 72, false, true, "Identifier", false, "Dialog"),
                    str("Control", 50, false, true), str("Type", 20, false), num("X", false), num("Y", false),
                    num("Width", false), num("Height", false), num("Attributes", true, false, true),
                    str("Property", 50), str("Text", 0, true, false, "Formatted", true),
                    str("Control_Next", 50), str("Help", 50, true, false, "Text", true)});
    add("ControlEvent",
        {str("Dialog_", 72, false, true, "Identifier", false, "Dialog"),
         str("Control_", 50, false, true, "Identifier", false, "Control", 2),
         str("Event", 50, false, true, "Formatted"), str("Argument", 255, false, true, "Formatted"),
         str("Condition", 255, true, true, "Condition"), num("Ordering")});
    add("EventMapping", {str("Dialog_", 72, false, true, "Identifier", false, "Dialog"),
                         str("Control_", 50, false, true, "Identifier", false, "Control", 2),
                         str("Event", 50, false, true), str("Attribute", 50, false)});
    add("CheckBox", {str("Property", 72, false, true), str("Value", 64, true, false, "Formatted")});
    add("ListBox", {str("Property", 72, false, true), num("Order", false, true),
                    str("Value", 64, false, false, "Text"), str("Text", 64, true, false, "Text", true)});
    add("RadioButton",
        {str("Property", 72, false, true), num("Order", false, true), str("Value", 64, false, false, "Text"),
         num("X", false), num("Y", false), num("Width", false), num("Height", false),
         str("Text", 64, true, false, "Text", true), str("Help", 50, true, false, "Text", true)});
    add("TextStyle", {str("TextStyle", 72, false, true), str("FaceName", 32, false, false, "Text"),
                      num("Size", false), num("Color", true, false, true), num("StyleBits")});
    add("ActionText", {str("Action", 72, false, true), str("Description", 64, true, false, "Text", true),
                       str("Template", 128, true, false, "Template", true)});
    add("_Validation", {str("Table", 32, false, true), str("Column", 32, false, true),
                        str("Nullable", 4, false, false, "Text"), num("MinValue", true, false, true),
                        num("MaxValue", true, false, true), str("KeyTable", 255, true, false, "Text"),
                        num("KeyColumn"), str("Category", 32, true, false, "Text"),
                        str("Set", 255, true, false, "Text"), str("Description", 255, true, false, "Text")});
}
void version(const std::string &s) {
    std::istringstream input(s);
    std::string part, normalized;
    int count = 0;
    while (std::getline(input, part, '.')) {
        if (part.empty() || part.size() > 5 ||
            !std::all_of(part.begin(), part.end(), [](char c) { return c >= '0' && c <= '9'; }))
            throw Error("MSI requires a numeric major.minor.build version", ExitCode::arguments);
        auto n = std::stoul(part);
        if (++count > 3 || n > (count == 3 ? 65535U : 255U))
            throw Error("MSI version exceeds 255.255.65535", ExitCode::arguments);
        normalized += (normalized.empty() ? "" : ".") + std::to_string(n);
    }
    if (count != 3 || normalized != s)
        throw Error("MSI requires major.minor.build without leading zeros", ExitCode::arguments);
}
std::string file_name(const std::string &long_name, unsigned id) {
    // Always supply an explicit, unique 8.3 source alias, independent of NTFS
    // short-name generation. Payload CAB members use File table identifiers.
    if (long_name.size() > 240)
        throw Error("MSI filename exceeds supported length");
    std::ostringstream name;
    name << 'F' << std::hex << id;
    return name.str() + "|" + long_name;
}
void wizard(Model &m, const WindowsPackageInput &input) {
    const auto &product = input.product;
    const bool classic = product.contains("branding") && product["branding"].value("style", "") == "classic";
    constexpr int classic_dialog_width = 370;
    constexpr int classic_dialog_height = 270;
    constexpr int classic_image_height = 234;
    constexpr int classic_banner_height = 44;
    constexpr int classic_banner_title_left = 119;
    constexpr int classic_sidebar_text_left = 138;
    auto row = [&](const char *t, Json r) { m.table(t).rows.push_back(std::move(r)); };
    auto control = [&](const char *d, const char *id, const char *type, int x, int y, int w, int h,
                       Json property, Json text, Json next = nullptr, int attributes = 3) {
        row("Control", {d, id, type, x, y, w, h, attributes, property, text, next, nullptr});
    };
    auto event = [&](const char *d, const char *c, const char *ev, const char *arg,
                     const char *condition = "1",
                     int order = 1) { row("ControlEvent", {d, c, ev, arg, condition, order}); };
    enum class PageStyle { full_image, interior };
    auto dialog = [&](const char *id, const char *first, const char *title, int attributes = 3,
                      PageStyle style = PageStyle::interior) {
        const int width = classic ? classic_dialog_width : 400;
        const int height = classic ? classic_dialog_height : 300;
        row("Dialog", {id, 50, 50, width, height, attributes, "[ProductName] Setup", first,
                       std::string(first) == "Cancel"  ? "Cancel"
                       : std::string(first) == "Retry" ? "Retry"
                       : std::string(first) == "Finish" ? "Finish"
                       : std::string(first) == "Directory" ? "OK"
                                                           : "Next",
                       std::string(first) == "Finish" ? "Finish" : "Cancel"});
        if (!classic) {
            control(id, "Title", "Text", 20, 15, 360, 30, nullptr,
                    std::string("{\\TitleFont}") + title);
            control(id, "Line", "Line", 0, 260, 400, 0, nullptr, nullptr);
            return;
        }
        if (style == PageStyle::full_image) {
            if (!input.branding.msi_dialog.empty())
                control(id, "DialogBitmap", "Bitmap", 0, 0, classic_dialog_width, classic_image_height,
                        nullptr, dialog_bitmap_key, nullptr, 1);
            control(id, "Title", "Text", classic_sidebar_text_left, 9, 217, 24, nullptr,
                    std::string("{\\TitleFont}") + title, nullptr, 65539);
        } else {
            if (!input.branding.msi_banner.empty())
                control(id, "BannerBitmap", "Bitmap", 0, 0, classic_dialog_width, classic_banner_height,
                        nullptr, banner_bitmap_key, nullptr, 1);
            control(id, "Title", "Text", classic_banner_title_left, 6,
                    classic_dialog_width - classic_banner_title_left - 15, 15, nullptr,
                    std::string("{\\TitleFont}") + title, nullptr, 65539);
            control(id, "BannerLine", "Line", 0, classic_banner_height, classic_dialog_width, 0, nullptr,
                    nullptr);
        }
        control(id, "Line", "Line", 0, classic_image_height, classic_dialog_width, 0, nullptr, nullptr);
    };
    auto cancel = [&](const char *d, const char *next) {
        control(d, "Cancel", "PushButton", classic ? 304 : 315, classic ? 243 : 273,
                classic ? 56 : 65, classic ? 17 : 18, nullptr, "Cancel", next);
        event(d, "Cancel", "EndDialog", "Exit");
    };
    auto buttons = [&](const char *d, const char *back, const char *next, const char *label = "Next >") {
        control(d, "Next", "PushButton", classic ? 236 : 240, classic ? 243 : 273,
                classic ? 56 : 65, classic ? 17 : 18, nullptr, label, "Cancel");
        cancel(d, back ? "Back" : "Next");
        if (back) {
            control(d, "Back", "PushButton", classic ? 180 : 165, classic ? 243 : 273,
                    classic ? 56 : 65, classic ? 17 : 18, nullptr, "< Back", "Next");
            event(d, "Back", "NewDialog", back);
        }
        if (next)
            event(d, "Next", "NewDialog", next);
    };
    row("TextStyle", {"BodyFont", "Segoe UI", 9, nullptr, 0});
    row("TextStyle", {"TitleFont", "Segoe UI", 12, nullptr, 1});
    row("Property", {"DefaultUIFont", "BodyFont"});
    dialog("WelcomeDlg", "Next", "Welcome", 3, PageStyle::full_image);
    control("WelcomeDlg", "Body", "Text", classic ? classic_sidebar_text_left : 20,
            classic ? 45 : 60, classic ? 217 : 360, classic ? 176 : 160, nullptr,
            "This wizard installs [ProductName].\r\n\r\n" +
                formatted_literal(product.value("welcome", "Install this version alongside other versions.")),
            nullptr, classic ? 65539 : 3);
    buttons("WelcomeDlg", nullptr, "LicenseDlg");
    dialog("LicenseDlg", "Next", "License information");
    constexpr std::size_t msi_rtf_limit = 60000;
    constexpr unsigned msi_rtf_font_size = 18;
    const auto license = input.branding.msi_license.empty()
                             ? text_rtf(product.at("licenseText").get<std::string>(),
                                        {"Segoe UI", msi_rtf_font_size, msi_rtf_limit})
                             : read_text(input.branding.msi_license, msi_rtf_limit);
    control("LicenseDlg", "License", "ScrollableText", 20, classic ? 58 : 55, classic ? 330 : 360,
            classic ? 166 : 195, nullptr, license, "Next", 7);
    buttons("LicenseDlg", "WelcomeDlg", "ScopeDlg");
    // Scope and optional integrations are selected before costing, so MSI redirects all known folders and
    // registry roots consistently. The next group of pages edits the destination.
    dialog("ScopeDlg", "Scope", "Who should have access?");
    control("ScopeDlg", "Scope", "RadioButtonGroup", 20, 65, classic ? 330 : 360, 70,
            "BRLINSTSCOPE", nullptr, "Back");
    row("RadioButton", {"BRLINSTSCOPE", 1, "user", 0, 0, 350, 20, "Only my account", nullptr});
    row("RadioButton", {"BRLINSTSCOPE", 2, "machine", 0, 35, 350, 20,
                        "Everyone (requires administrator permission)", nullptr});
    control("ScopeDlg", "Body", "Text", 20, classic ? 145 : 155, classic ? 330 : 360,
            classic ? 70 : 75, nullptr,
            "Choose shortcuts and PATH next. To change scope or options after choosing a destination, cancel "
            "and restart setup.");
    buttons("ScopeDlg", "LicenseDlg", nullptr);
    event("ScopeDlg", "Next", "[ALLUSERS]", "2", "BRLINSTSCOPE = \"user\"", 1);
    event("ScopeDlg", "Next", "[MSIINSTALLPERUSER]", "1", "BRLINSTSCOPE = \"user\"", 2);
    event("ScopeDlg", "Next", "[ALLUSERS]", "1", "BRLINSTSCOPE = \"machine\"", 3);
    event("ScopeDlg", "Next", "[MSIINSTALLPERUSER]", "{}", "BRLINSTSCOPE = \"machine\"", 4);
    event("ScopeDlg", "Next", "NewDialog", "OptionsDlg", "1", 5);
    dialog("DestinationDlg", "Path", "Choose the destination");
    control("DestinationDlg", "Body", "Text", 20, 60, classic ? 330 : 360, 60, nullptr,
            "Choose a new or empty folder on a local fixed drive. Each version needs its own directory.");
    control("DestinationDlg", "Path", "PathEdit", 20, classic ? 125 : 135, classic ? 255 : 280, 20,
            "INSTALLDIR", nullptr, "Browse");
    control("DestinationDlg", "Browse", "PushButton", classic ? 285 : 310, classic ? 125 : 135,
            classic ? 75 : 70, 20, nullptr, "Browse...", "Next");
    control("DestinationDlg", "Next", "PushButton", classic ? 236 : 240, classic ? 243 : 273,
            classic ? 56 : 65, classic ? 17 : 18, nullptr, "Next >", "Cancel");
    cancel("DestinationDlg", "Path");
    event("DestinationDlg", "Browse", "[_BrowseProperty]", "INSTALLDIR", "1", 1);
    event("DestinationDlg", "Browse", "SpawnDialog", "BrowseDlg", "1", 2);
    event("DestinationDlg", "Next", "SetTargetPath", "INSTALLDIR", "1", 1);
    event("DestinationDlg", "Next", "NewDialog", "ReadyDlg", "1", 2);
    dialog("BrowseDlg", "Directory", "Browse for a destination");
    control("BrowseDlg", "Directory", "DirectoryCombo", 20, classic ? 55 : 60, classic ? 255 : 280, 18,
            "_BrowseProperty", nullptr, "Up", 11);
    control("BrowseDlg", "Up", "PushButton", classic ? 285 : 310, classic ? 55 : 60,
            classic ? 75 : 70, 18, nullptr, "Up", "DirectoryList");
    event("BrowseDlg", "Up", "DirectoryListUp", "0");
    control("BrowseDlg", "DirectoryList", "DirectoryList", 20, classic ? 82 : 88,
            classic ? 340 : 360, classic ? 100 : 110, "_BrowseProperty", nullptr, "Path", 11);
    control("BrowseDlg", "Path", "PathEdit", 20, classic ? 190 : 208, classic ? 340 : 360, 18,
            "_BrowseProperty", nullptr, "NewFolder", 11);
    control("BrowseDlg", "NewFolder", "PushButton", 20, classic ? 215 : 235, 85, 18, nullptr,
            "New folder", "OK");
    event("BrowseDlg", "NewFolder", "DirectoryListNew", "0");
    control("BrowseDlg", "OK", "PushButton", classic ? 236 : 240, classic ? 243 : 273,
            classic ? 56 : 65, classic ? 17 : 18, nullptr, "OK", "Cancel");
    event("BrowseDlg", "OK", "SetTargetPath", "[_BrowseProperty]", "1", 1);
    event("BrowseDlg", "OK", "EndDialog", "Return", "1", 2);
    control("BrowseDlg", "Cancel", "PushButton", classic ? 304 : 315, classic ? 243 : 273,
            classic ? 56 : 65, classic ? 17 : 18, nullptr, "Cancel", "Directory");
    event("BrowseDlg", "Cancel", "EndDialog", "Return");
    dialog("OptionsDlg", "Menu", "Choose shortcuts and PATH");
    control("OptionsDlg", "Menu", "CheckBox", 20, 65, classic ? 330 : 360, 20, "BRLINST_STARTMENU",
            "Create Start Menu shortcuts", "Desktop");
    control("OptionsDlg", "Desktop", "CheckBox", 20, 100, classic ? 330 : 360, 20, "BRLINST_DESKTOP",
            "Create desktop shortcuts", "Path");
    control("OptionsDlg", "Path", "CheckBox", 20, 135, classic ? 330 : 360, 20, "BRLINST_PATH",
            "Add this version's bin directory to PATH", "Back");
    control("OptionsDlg", "Body", "Text", 20, classic ? 175 : 185, classic ? 330 : 360,
            classic ? 45 : 55, nullptr,
            "Existing PATH entries stay in their current order. Other versions remain installed.");
    for (auto p : {"BRLINST_STARTMENU", "BRLINST_DESKTOP", "BRLINST_PATH"})
        row("CheckBox", {p, "1"});
    buttons("OptionsDlg", "ScopeDlg", nullptr);
    event("OptionsDlg", "Next", "EndDialog", "Return");
    dialog("ReadyDlg", "Next", "Ready to install");
    control("ReadyDlg", "Body", "Text", 20, 60, classic ? 330 : 360, classic ? 155 : 170, nullptr,
            "Product: [ProductName]\r\n\r\nDestination: [INSTALLDIR]\r\n\r\nScope: "
            "[BRLINSTSCOPE]\r\n\r\nClick Install to begin.");
    buttons("ReadyDlg", "DestinationDlg", nullptr, "Install");
    event("ReadyDlg", "Next", "DoAction", "BrlinstCheckUI", "1", 1);
    event("ReadyDlg", "Next", "SpawnDialog", "ValidationDlg", "NOT BRLINSTVALID", 2);
    event("ReadyDlg", "Next", "EndDialog", "Return", "BRLINSTVALID", 3);
    row("Dialog", {"ValidationDlg", 50, 50, 360, 170, 3, "Check installation options", "OK", "OK", "OK"});
    control("ValidationDlg", "Message", "Text", 15, 15, 330, 105, nullptr, "[BRLINSTERROR]");
    control("ValidationDlg", "OK", "PushButton", 275, 135, 65, 18, nullptr, "OK", "OK");
    event("ValidationDlg", "OK", "EndDialog", "Return");
    dialog("RemoveDlg", "Next", "Remove this version?");
    control(
        "RemoveDlg", "Body", "Text", 20, 65, classic ? 330 : 360, classic ? 145 : 150, nullptr,
        "Windows Installer will remove [ProductName] and its shortcuts. Other versions remain installed.");
    buttons("RemoveDlg", nullptr, nullptr, "Remove");
    event("RemoveDlg", "Next", "EndDialog", "Return");
    dialog("ProgressDlg", "Cancel", "Installing or removing files", 1);
    control("ProgressDlg", "Status", "Text", 20, classic ? 70 : 80, classic ? 330 : 360, 40, nullptr,
            "Starting installation...");
    control("ProgressDlg", "Progress", "ProgressBar", 20, classic ? 125 : 135, classic ? 330 : 360, 20,
            nullptr, nullptr);
    control("ProgressDlg", "Cancel", "PushButton", classic ? 304 : 315, classic ? 243 : 273,
            classic ? 56 : 65, classic ? 17 : 18, nullptr, "Cancel", "Cancel");
    event("ProgressDlg", "Cancel", "SpawnDialog", "StopDlg");
    row("Dialog", {"StopDlg", 50, 50, 360, 150, 3, "Cancel setup", "Continue", "Continue", "Continue"});
    control("StopDlg", "Message", "Text", 15, 15, 330, 80, nullptr,
            "Stop this operation and roll back its changes?");
    control("StopDlg", "Stop", "PushButton", 165, 115, 80, 18, nullptr, "Stop setup", "Continue");
    control("StopDlg", "Continue", "PushButton", 255, 115, 90, 18, nullptr, "Continue setup", "Stop");
    event("StopDlg", "Stop", "EndDialog", "Exit");
    event("StopDlg", "Continue", "EndDialog", "Return");
    row("EventMapping", {"ProgressDlg", "Status", "ActionText", "Text"});
    row("EventMapping", {"ProgressDlg", "Progress", "SetProgress", "Progress"});
    for (auto name : {"FinishedDlg", "CancelledDlg", "FailedDlg"}) {
        std::string title = std::string(name) == "FinishedDlg" ? "Installation complete"
                            : std::string(name) == "CancelledDlg" ? "Setup cancelled"
                                                                  : "Setup failed";
        if (classic)
            dialog(name, "Finish", title.c_str(), 3, PageStyle::full_image);
        else
            row("Dialog", {name, 50, 50, 400, 240, 3, "[ProductName] Setup", "Finish", "Finish", "Finish"});
        std::string message = std::string(name) == "FinishedDlg" ? "The operation completed."
                              : std::string(name) == "CancelledDlg"
                                  ? "Setup was cancelled."
                                  : "Setup failed. See the Windows Installer log for details.";
        control(name, "Message", "Text", classic ? classic_sidebar_text_left : 20,
                classic ? 45 : 30, classic ? 217 : 360, classic ? 176 : 150, nullptr, message, nullptr,
                classic ? 65539 : 3);
        control(name, "Finish", "PushButton", classic ? 236 : 315, classic ? 243 : 205,
                classic ? 56 : 65, classic ? 17 : 18, nullptr, "Close", "Finish");
        event(name, "Finish", "EndDialog", "Return");
    }
    // Close the keyboard navigation cycles through editable controls too.
    for (auto &c : m.table("Control").rows) {
        if (c[0] == "ScopeDlg" && c[1] == "Cancel")
            c[10] = "Scope";
        if (c[0] == "DestinationDlg" && c[1] == "Cancel")
            c[10] = "Path";
        if (c[0] == "OptionsDlg" && c[1] == "Cancel")
            c[10] = "Menu";
        if (c[0] == "LicenseDlg" && c[1] == "Back")
            c[10] = "License";
    }
    row("Property", {"ErrorDialog", "ErrorDlg"});
    row("Dialog",
        {"ErrorDlg", 50, 50, 360, 200, 65539, "[ProductName] Setup", "ErrorText", nullptr, nullptr});
    control("ErrorDlg", "ErrorText", "Text", 15, 15, 330, 135, nullptr, nullptr);
    for (auto action : {"Abort", "Cancel", "Ignore", "No", "Ok", "Retry", "Yes"}) {
        const auto button = std::string(1, action[0]);
        control("ErrorDlg", button.c_str(), "PushButton", 140, 165, 65, 18, nullptr, action);
        auto argument = std::string("Error") + action;
        event("ErrorDlg", button.c_str(), "EndDialog", argument.c_str());
    }
    dialog("FilesInUse", "Retry", "Close applications using these files");
    control("FilesInUse", "List", "ListBox", 20, 60, classic ? 330 : 360, classic ? 165 : 185,
            "FileInUseProcess", nullptr, "Ignore", 65539);
    control("FilesInUse", "Retry", "PushButton", classic ? 236 : 240, classic ? 243 : 273,
            classic ? 56 : 65, classic ? 17 : 18, nullptr, "Retry", "Cancel");
    event("FilesInUse", "Retry", "EndDialog", "Retry");
    control("FilesInUse", "Ignore", "PushButton", classic ? 180 : 165, classic ? 243 : 273,
            classic ? 56 : 65, classic ? 17 : 18, nullptr, "Ignore", "Retry");
    event("FilesInUse", "Ignore", "EndDialog", "Ignore");
    cancel("FilesInUse", "List");
}
} // namespace

Model make_model(const WindowsPackageInput &input, const fs::path &notices, const fs::path &actions,
                 const GuidFactory &guid) {
    const auto &p = input.product;
    version(p.at("version").get<std::string>());
    Model m(product_architecture(p));
    schema(m);
    auto row = [&](const char *t, Json r) { m.table(t).rows.push_back(std::move(r)); };
    auto property = [&](const char *key, const std::string &value) { row("Property", {key, value}); };
    const auto identity = "brlinst/msi/v1/" + instance_id(p);
    m.product_code = guid(identity + "/product");
    const auto architecture = architecture_name(m.architecture);
    m.name = p.at("name").get<std::string>() + " " + p.at("version").get<std::string>() + " (" +
             architecture + ", MSI)";
    property("ProductCode", m.product_code);
    property("ProductName", m.name);
    property("UpgradeCode",
             guid("brlinst/msi/v1/family/" + p.at("id").get<std::string>() + "/" + architecture));
    property("ProductVersion", p.at("version"));
    property("Manufacturer", p.at("publisher"));
    property("ProductLanguage", "1033");
    property("ALLUSERS", "2");
    property("MSIINSTALLPERUSER", "1");
    property("BRLINSTSCOPE", "user");
    property("BRLINST_STARTMENU", "1");
    // No default rows for opt-in properties: an unchecked MSI checkbox clears it.
    property("INSTALLLEVEL", "1");
    property("ARPNOMODIFY", "1");
    property("ARPNOREPAIR", "1");
    property("REBOOT", "ReallySuppress");
    property("MSIRESTARTMANAGERCONTROL", "Disable");
    property("BRLINSTPROFILE", "%USERPROFILE%");
    property("SecureCustomProperties",
             "INSTALLDIR;ALLUSERS;MSIINSTALLPERUSER;BRLINST_STARTMENU;BRLINST_DESKTOP;BRLINST_PATH");
    if (p.contains("website"))
        property("ARPURLINFOABOUT", p.at("website"));
    const auto &product_icon = input.branding.msi_icon.empty() ? input.icon : input.branding.msi_icon;
    if (!product_icon.empty()) {
        row("Icon", {"Product.ico", {{"stream", utf8(product_icon)}}});
        property("ARPPRODUCTICON", "Product.ico");
    }
    if (!input.branding.msi_banner.empty())
        row("Binary", {banner_bitmap_key, {{"stream", utf8(input.branding.msi_banner)}}});
    if (!input.branding.msi_dialog.empty())
        row("Binary", {dialog_bitmap_key, {{"stream", utf8(input.branding.msi_dialog)}}});
    row("Directory", {"TARGETDIR", nullptr, "SourceDir"});
    row("Directory", {"ProgramFiles64Folder", "TARGETDIR", "."});
    row("Directory", {"INSTALLDIR", "ProgramFiles64Folder", "BRLINST|" + m.name});
    row("Directory", {"ProgramMenuFolder", "TARGETDIR", "."});
    row("Directory", {"MenuFolder", "ProgramMenuFolder", "BRLINST|" + m.name});
    row("Directory", {"DesktopFolder", "TARGETDIR", "."});
    row("Feature", {"Application", nullptr, "Application", nullptr, 0, 1, "INSTALLDIR", 8});
    std::map<std::string, std::string> dirs{{"", "INSTALLDIR"}, {".", "INSTALLDIR"}};
    unsigned serial = 1;
    std::function<std::string(const std::string &)> directory = [&](const std::string &path) -> std::string {
        auto found = dirs.find(path);
        if (found != dirs.end())
            return found->second;
        auto parent = directory(path_from_utf8(path).parent_path().generic_u8string());
        auto n = serial++;
        auto id = "D" + std::to_string(n);
        row("Directory", {id, parent, file_name(utf8(path_from_utf8(path).filename()), n)});
        dirs[path] = id;
        return id;
    };
    auto component = [&](const std::string &id, const std::string &dir, const std::string &key,
                         Json condition, bool registry, const std::string &purpose) {
        row("Component", {id, guid(identity + "/" + purpose), dir, registry ? 260 : 256, condition, key});
        row("FeatureComponents", {"Application", id});
    };
    auto files = input.files;
    files.emplace_back(std::string(state_directory) + "/" + notices_filename, notices);
    std::sort(files.begin(), files.end());
    if (files.size() > 32767)
        throw Error("MSI backend currently supports at most 32767 files");
    std::uint64_t total = 0;
    std::map<std::string, std::string> file_ids;
    for (const auto &f : files) {
        auto size = fs::file_size(f.second);
        total += size;
        // Conservative single embedded CAB limit; fail before FCI starts writing.
        if (size > INT_MAX || total > 1536ULL * 1024 * 1024)
            throw Error("MSI backend currently supports at most 1.5 GiB of uncompressed files");
        auto sequence = static_cast<int>(m.files.size()) + 1;
        auto id = "F" + std::to_string(sequence);
        auto cid = "C" + std::to_string(sequence);
        auto dir = directory(path_from_utf8(f.first).parent_path().generic_u8string());
        component(cid, dir, id, nullptr, false, "file/" + f.first);
        row("File", {id, cid, file_name(utf8(path_from_utf8(f.first).filename()), serial++), size, nullptr,
                     nullptr, 16384, sequence});
        m.files.push_back({id, f.first, f.second, size});
        file_ids[f.first] = id;
    }
    row("Media", {1, static_cast<int>(files.size()), nullptr, "#payload.cab", nullptr, nullptr});
    const auto registry_key = "Software\\Brlinst\\MSI\\" + m.product_code;
    auto integration = [&](const std::string &id, const std::string &dir, const std::string &condition,
                           int root = -1) {
        auto key = "R" + id;
        component(id, dir, key, condition, true, "integration/" + id);
        row("Registry", {key, root, registry_key, id, "#1", id});
    };
    integration("Menu", "MenuFolder", "BRLINST_STARTMENU = \"1\"");
    integration("Desktop", "DesktopFolder", "BRLINST_DESKTOP = \"1\"");
    row("RemoveFile", {"RemoveMenuFolder", "Menu", nullptr, "MenuFolder", 2});
    if (std::any_of(files.begin(), files.end(), [](const auto &f) { return f.first.rfind("bin/", 0) == 0; }))
        property("BRLINST_HASBIN", "1");
    auto bin = directory("bin");
    integration("UserPath", bin,
                "BRLINST_PATH = \"1\" AND (NOT ALLUSERS OR (ALLUSERS = \"2\" AND MSIINSTALLPERUSER = \"1\"))",
                1);
    integration("MachinePath", bin,
                "BRLINST_PATH = \"1\" AND (ALLUSERS = \"1\" OR (ALLUSERS = \"2\" AND NOT MSIINSTALLPERUSER))",
                2);
    row("Environment", {"UserPath", "=-PATH", "[~];[" + bin + "]", "UserPath"});
    row("Environment", {"MachinePath", "=-*PATH", "[~];[" + bin + "]", "MachinePath"});
    unsigned shortcut_index = 0;
    for (const auto &s : p.at("shortcuts")) {
        auto id = "S" + std::to_string(++shortcut_index);
        auto target = file_ids.at(s.at("target").get<std::string>());
        auto working = s.value("workingDirectory", "@profile");
        auto wkdir = working == "@profile" ? "BRLINSTPROFILE" : directory(working);
        Json icon = nullptr;
        if (s.contains("icon")) {
            const auto icon_path = s.at("icon").get<std::string>();
            auto found =
                std::find_if(files.begin(), files.end(), [&](const auto &f) { return f.first == icon_path; });
            auto ext = utf8(path_from_utf8(icon_path).extension());
            icon = id + ext;
            row("Icon", {icon, {{"stream", utf8(found->second)}}});
        }
        auto args = formatted_literal(s.value("arguments", ""));
        if (args.size() > 255)
            throw Error("MSI shortcut arguments exceed 255 characters");
        auto name = s.at("name").get<std::string>();
        row("Shortcut", {id, "MenuFolder", file_name(name, serial++), "Menu", "[#" + target + "]",
                         args.empty() ? Json(nullptr) : Json(args), nullptr, nullptr, icon,
                         icon.is_null() ? Json(nullptr) : Json(0), 1, wkdir});
        if (s.value("desktop", true)) {
            auto desktop_name = name + " - " + m.name;
            if (desktop_name.size() > 115)
                throw Error("MSI desktop shortcut name exceeds 115 characters");
            row("Shortcut", {id + "Desktop", "DesktopFolder", file_name(desktop_name, serial++), "Desktop",
                             "[#" + target + "]", args.empty() ? Json(nullptr) : Json(args), nullptr, nullptr,
                             icon, icon.is_null() ? Json(nullptr) : Json(0), 1, wkdir});
        }
    }
    // Only validation executes custom code. All installation changes use MSI's
    // standard transactional actions; no EXE installer is nested in the MSI.
    row("Binary", {"BrlinstActions", {{"stream", utf8(actions)}}});
    row("CustomAction", {"BrlinstRejectAdmin", 19, nullptr,
                         "Administrative extraction is not supported by this package. Use the original "
                         "staged distribution."});
    row("AdminExecuteSequence", {"BrlinstRejectAdmin", nullptr, 100});
    row("AdminUISequence", {"BrlinstRejectAdmin", nullptr, 100});
    row("AdminUISequence", {"FinishedDlg", "UILevel = 5", -1});
    row("AdminUISequence", {"CancelledDlg", "UILevel = 5", -2});
    row("AdminUISequence", {"FailedDlg", "UILevel = 5", -3});
    row("CustomAction", {"BrlinstCheck", 1, "BrlinstActions", "BrlinstCheck"});
    row("CustomAction", {"BrlinstCheckUI", 1, "BrlinstActions", "BrlinstCheckUI"});
    int clear_sequence = 210;
    for (auto name : {"BRLINST_STARTMENU", "BRLINST_DESKTOP", "BRLINST_PATH"}) {
        const auto action = std::string("Clear_") + name;
        row("CustomAction", {action, 51, name, nullptr});
        row("InstallUISequence", {action, std::string(name) + " = \"0\"", clear_sequence++});
    }
    row("CustomAction", {"BrlinstMachineUI", 51, "BRLINSTSCOPE", "machine"});
    row("CustomAction", {"BrlinstInstallLocation", 51, "ARPINSTALLLOCATION", "[INSTALLDIR]"});
    row("LaunchCondition", {"VersionNT64", "This package requires 64-bit Windows."});
    row("LaunchCondition", {"NOT Installed OR REMOVE = \"ALL\"",
                            "This version is already installed. Uninstall it before installing again."});
    for (auto name : {"BRLINST_STARTMENU", "BRLINST_DESKTOP", "BRLINST_PATH"})
        row("LaunchCondition",
            {std::string("NOT ") + name + " OR " + name + " = \"0\" OR " + name + " = \"1\"",
             "Shortcut and PATH options must be 0 or 1."});
    const std::vector<std::pair<const char *, int>> execute{{"LaunchConditions", 100},
                                                            {"CostInitialize", 800},
                                                            {"FileCost", 900},
                                                            {"CostFinalize", 1000},
                                                            {"InstallValidate", 1400},
                                                            {"InstallInitialize", 1500},
                                                            {"ProcessComponents", 1600},
                                                            {"UnpublishFeatures", 1800},
                                                            {"RemoveRegistryValues", 2600},
                                                            {"RemoveShortcuts", 3200},
                                                            {"RemoveEnvironmentStrings", 3300},
                                                            {"RemoveFiles", 3500},
                                                            {"RemoveFolders", 3600},
                                                            {"CreateFolders", 3700},
                                                            {"InstallFiles", 4000},
                                                            {"CreateShortcuts", 4500},
                                                            {"WriteRegistryValues", 5000},
                                                            {"WriteEnvironmentStrings", 5200},
                                                            {"RegisterUser", 6000},
                                                            {"RegisterProduct", 6100},
                                                            {"PublishFeatures", 6300},
                                                            {"PublishProduct", 6400},
                                                            {"InstallFinalize", 6600}};
    const std::map<std::string, std::string> messages{
        {"LaunchConditions", "Checking installation requirements"},
        {"CostInitialize", "Calculating required disk space"},
        {"FileCost", "Calculating file costs"},
        {"CostFinalize", "Finalizing the installation plan"},
        {"InstallValidate", "Validating the installation"},
        {"InstallInitialize", "Starting the installation transaction"},
        {"ProcessComponents", "Preparing installation components"},
        {"InstallFiles", "Copying application files"},
        {"RemoveFiles", "Removing application files"},
        {"CreateShortcuts", "Creating shortcuts"},
        {"RemoveShortcuts", "Removing shortcuts"},
        {"WriteEnvironmentStrings", "Updating PATH"},
        {"RemoveEnvironmentStrings", "Removing this version from PATH"},
        {"WriteRegistryValues", "Saving installation settings"},
        {"RemoveRegistryValues", "Removing installation settings"},
        {"RegisterProduct", "Registering this version"},
        {"InstallFinalize", "Finishing the operation"}};
    for (auto a : execute) {
        row("InstallExecuteSequence", {a.first, nullptr, a.second});
        auto message = messages.find(a.first);
        row("ActionText",
            {a.first, message == messages.end() ? "Preparing installation" : message->second, nullptr});
    }
    row("InstallExecuteSequence", {"BrlinstCheck", "NOT Installed", 1101});
    row("InstallExecuteSequence", {"BrlinstInstallLocation", "NOT Installed", 1102});
    row("ActionText", {"BrlinstCheck", "Checking installation options", nullptr});
    row("ActionText", {"BrlinstInstallLocation", "Preparing the installation location", nullptr});
    for (auto a : std::vector<std::pair<const char *, int>>{{"LaunchConditions", 100},
                                                            {"CostInitialize", 800},
                                                            {"FileCost", 900},
                                                            {"CostFinalize", 1000},
                                                            {"ExecuteAction", 1300}})
        row("InstallUISequence", {a.first, nullptr, a.second});
    row("InstallUISequence",
        {"BrlinstMachineUI", "ALLUSERS = \"1\" OR (ALLUSERS = \"2\" AND NOT MSIINSTALLPERUSER)", 201});
    row("InstallUISequence", {"WelcomeDlg", "NOT Installed AND UILevel = 5", 600});
    row("InstallUISequence", {"DestinationDlg", "NOT Installed AND UILevel = 5", 1100});
    row("InstallUISequence", {"RemoveDlg", "Installed AND REMOVE = \"ALL\" AND UILevel = 5", 1200});
    row("InstallUISequence", {"ProgressDlg", nullptr, 1280});
    row("InstallUISequence", {"FinishedDlg", "UILevel = 5", -1});
    row("InstallUISequence", {"CancelledDlg", "UILevel = 5", -2});
    row("InstallUISequence", {"FailedDlg", "UILevel = 5", -3});
    wizard(m, input);
    // Populate validation metadata from the same schema used to create tables.
    // The Windows SDK ICE suite can then check the authored package normally.
    auto &validation = m.table("_Validation");
    for (const auto &t : m.tables)
        for (const auto &c : t.columns)
            validation.rows.push_back({t.name, c.name, c.nullable ? "Y" : "N", nullptr, nullptr,
                                       c.foreign_table.empty() ? Json(nullptr) : Json(c.foreign_table),
                                       c.foreign_column ? Json(c.foreign_column) : Json(nullptr),
                                       c.category.empty() ? Json(nullptr) : Json(c.category), nullptr,
                                       nullptr});
    return m;
}
} // namespace installer::msi
