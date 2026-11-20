// SPDX-License-Identifier: BSD-2-Clause
#include <shlobj.h>
#include "installer/windows.hpp"
#include <sddl.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

namespace installer {
namespace {
constexpr unsigned cleanup_remove_attempts = 50;
constexpr auto cleanup_remove_delay = std::chrono::milliseconds(100);
struct Key {
    HKEY value = nullptr;
    ~Key() {
        if (value)
            RegCloseKey(value);
    }
};
struct Com {
    HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Com() {
        if (FAILED(result))
            throw Error("Cannot initialize Windows shell services");
    }
    ~Com() {
        CoUninitialize();
    }
};
template <class T> struct Interface {
    T *value = nullptr;
    ~Interface() {
        if (value)
            value->Release();
    }
};
struct RegistryValue {
    bool exists = false;
    DWORD type = REG_EXPAND_SZ;
    std::wstring text;
};
const std::wstring uninstall_base = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Brlinst_";
std::wstring registration_key(const Json &p) {
    return uninstall_base + wide(instance_id(p));
}
HKEY hive(bool machine) {
    return machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
}
void reg_check(LSTATUS code, const std::string &operation) {
    if (code != ERROR_SUCCESS)
        throw Error(operation + " (Windows error " + std::to_string(code) + ")");
}
RegistryValue read_value(HKEY root, const std::wstring &subkey, const wchar_t *name) {
    Key key;
    auto status = RegOpenKeyExW(root, subkey.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &key.value);
    if (status == ERROR_FILE_NOT_FOUND)
        return {};
    reg_check(status, "Cannot read registry key");
    RegistryValue v;
    DWORD bytes = 0;
    status = RegQueryValueExW(key.value, name, nullptr, &v.type, nullptr, &bytes);
    if (status == ERROR_FILE_NOT_FOUND)
        return {};
    reg_check(status, "Cannot read registry value");
    if ((v.type != REG_SZ && v.type != REG_EXPAND_SZ) || bytes > 128 * 1024 || bytes % sizeof(wchar_t))
        throw Error("Unexpected registry value type/size");
    v.text.resize(bytes / sizeof(wchar_t));
    reg_check(
        RegQueryValueExW(key.value, name, nullptr, &v.type, reinterpret_cast<BYTE *>(v.text.data()), &bytes),
        "Cannot read registry value");
    while (!v.text.empty() && v.text.back() == L'\0')
        v.text.pop_back();
    if (v.text.find(L'\0') != std::wstring::npos)
        throw Error("Embedded NUL in registry value");
    v.exists = true;
    return v;
}
void set_text(HKEY key, const wchar_t *name, const std::wstring &value, DWORD type = REG_SZ) {
    reg_check(RegSetValueExW(key, name, 0, type, reinterpret_cast<const BYTE *>(value.c_str()),
                             static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))),
              "Cannot write registry value");
}
std::wstring environment_key(bool machine) {
    return machine ? L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment" : L"Environment";
}
std::wstring normalize(std::wstring text) {
    while (!text.empty() && (text.back() == L' ' || text.back() == L'\\' || text.back() == L'/'))
        text.pop_back();
    while (!text.empty() && text.front() == L' ')
        text.erase(text.begin());
    if (text.size() > 1 && text.front() == L'"' && text.back() == L'"')
        text = text.substr(1, text.size() - 2);
    std::wstring expanded(32768, L'\0');
    DWORD n = ExpandEnvironmentStringsW(text.c_str(), expanded.data(), static_cast<DWORD>(expanded.size()));
    if (n && n <= expanded.size()) {
        expanded.resize(n - 1);
        text = expanded;
    }
    std::replace(text.begin(), text.end(), L'/', L'\\');
    while (!text.empty() && text.back() == L'\\')
        text.pop_back();
    CharUpperBuffW(text.data(), static_cast<DWORD>(text.size()));
    return text;
}
std::vector<std::wstring> split_path(const std::wstring &text) {
    std::vector<std::wstring> parts;
    std::size_t start = 0;
    for (;;) {
        auto end = text.find(L';', start);
        parts.push_back(text.substr(start, end == std::wstring::npos ? end : end - start));
        if (end == std::wstring::npos)
            break;
        start = end + 1;
    }
    return parts;
}
void change_path(Json &state, const fs::path &journal, bool removing) {
    bool machine = state.at("machine");
    auto root = path_from_utf8(state.at("root"));
    auto token = (root / L"bin").wstring();
    auto key_name = environment_key(machine);
    auto current = read_value(hive(machine), key_name, L"Path");
    auto parts = split_path(current.text);
    auto same = [&](const std::wstring &x) { return normalize(x) == normalize(token); };
    auto &info = state["integration"];
    if (!removing) {
        if (std::any_of(parts.begin(), parts.end(), same))
            return;
        if (!fs::is_directory(root / L"bin"))
            throw Error("PATH was requested but the package has no bin directory");
        info["path"] = {{"added", true}, {"previouslyExisted", current.exists}};
        durable_json(journal, state);
        if (!current.text.empty() && current.text.back() != L';')
            current.text += L';';
        current.text += token;
    } else {
        if (!info.contains("path") || !info["path"].value("added", false))
            return;
        auto it = std::find_if(parts.begin(), parts.end(), same);
        if (it == parts.end())
            return;
        parts.erase(it);
        current.text.clear();
        for (const auto &part : parts) {
            if (&part != &parts.front())
                current.text += L';';
            current.text += part;
        }
    }
    if (current.text.size() >= 32767)
        throw Error("PATH would exceed the supported Windows length");
    Key key;
    reg_check(RegCreateKeyExW(hive(machine), key_name.c_str(), 0, nullptr, 0, KEY_SET_VALUE | KEY_WOW64_64KEY,
                              nullptr, &key.value, nullptr),
              "Cannot open environment registry key");
    if (removing && current.text.empty() && !info["path"].value("previouslyExisted", true)) {
        auto status = RegDeleteValueW(key.value, L"Path");
        if (status != ERROR_FILE_NOT_FOUND)
            reg_check(status, "Cannot remove PATH value");
    } else
        set_text(key.value, L"Path", current.text, current.type);
    DWORD_PTR unused = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"Environment"),
                        SMTO_ABORTIFHUNG, 2000, &unused);
}
struct Shortcut {
    fs::path path, target, icon;
    std::wstring arguments, working;
};
std::vector<Shortcut> shortcuts(const Json &state) {
    bool machine = state.at("machine");
    auto root = path_from_utf8(state.at("root"));
    const auto &product = state.at("product");
    auto label = wide(display_name(product, machine));
    auto programs = known_directory(machine ? FOLDERID_CommonPrograms : FOLDERID_Programs) / label;
    auto desktop = known_directory(machine ? FOLDERID_PublicDesktop : FOLDERID_Desktop);
    std::vector<Shortcut> result;
    for (const auto &s : product.at("shortcuts")) {
        Shortcut link;
        link.target = root / path_from_utf8(s.at("target"));
        link.icon = s.contains("icon") ? root / path_from_utf8(s["icon"]) : link.target;
        link.arguments = wide(s.value("arguments", ""));
        auto working = s.value("workingDirectory", "@profile");
        link.working = working == "@profile" ? L"%USERPROFILE%"
                       : working == "."      ? root.wstring()
                                             : (root / path_from_utf8(working)).wstring();
        if (state["options"].value("startMenu", true)) {
            link.path = programs / (wide(s.at("name")) + L".lnk");
            result.push_back(link);
        }
        if (state["options"].value("desktop", false) && s.value("desktop", true)) {
            link.path = desktop / (wide(s.at("name")) + L" - " + label + L".lnk");
            result.push_back(link);
        }
    }
    if (state["options"].value("startMenu", true)) {
        auto exe = root / state_directory / uninstall_filename;
        result.push_back({programs / L"Uninstall.lnk", exe, exe, L"", root.wstring()});
    }
    return result;
}
std::wstring link_marker(const Json &state) {
    return L"brlinst:" + wide(instance_id(state.at("product"))) + L":" + wide(state.at("root"));
}
bool owned_link(const fs::path &path, const Json &state) {
    Interface<IShellLinkW> link;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                reinterpret_cast<void **>(&link.value))))
        throw Error("Cannot inspect shortcut");
    Interface<IPersistFile> file;
    if (FAILED(link.value->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&file.value))))
        throw Error("Cannot inspect shortcut file");
    if (FAILED(file.value->Load(path.c_str(), STGM_READ)))
        return false;
    std::array<wchar_t, 32768> description{};
    return SUCCEEDED(link.value->GetDescription(description.data(), static_cast<int>(description.size()))) &&
           description.data() == link_marker(state);
}
void write_link(const Shortcut &s, const Json &state) {
    Interface<IShellLinkW> link;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                reinterpret_cast<void **>(&link.value))))
        throw Error("Cannot create shell shortcut");
    auto check = [](HRESULT h) {
        if (FAILED(h))
            throw Error("Cannot set shell shortcut properties");
    };
    check(link.value->SetPath(s.target.c_str()));
    check(link.value->SetArguments(s.arguments.c_str()));
    check(link.value->SetWorkingDirectory(s.working.c_str()));
    check(link.value->SetIconLocation(s.icon.c_str(), 0));
    check(link.value->SetDescription(link_marker(state).c_str()));
    Interface<IPersistFile> file;
    check(link.value->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&file.value)));
    check(file.value->Save(s.path.c_str(), TRUE));
}
int elevate(const std::wstring &parameters) {
    auto executable = module_path();
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = executable.c_str();
    info.lpParameters = parameters.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info)) {
        if (GetLastError() == ERROR_CANCELLED)
            return static_cast<int>(ExitCode::cancelled);
        throw Error("Could not request administrator privileges");
    }
    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD code = static_cast<DWORD>(ExitCode::failure);
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    return static_cast<int>(code);
}
} // namespace
std::wstring wide(const std::string &text) {
    if (text.empty())
        return {};
    auto n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                                 nullptr, 0);
    if (!n)
        throw Error("Invalid UTF-8 text");
    std::wstring result(n, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                        result.data(), n);
    return result;
}
std::string narrow(const std::wstring &text) {
    return utf8(fs::path(text));
}
void win_check(bool ok, const std::string &operation) {
    if (!ok)
        throw Error(operation + " (Windows error " + std::to_string(GetLastError()) + ")");
}
fs::path module_path() {
    std::wstring value(32768, L'\0');
    DWORD n = GetModuleFileNameW(nullptr, value.data(), static_cast<DWORD>(value.size()));
    win_check(n > 0 && n < value.size(), "Cannot locate executable");
    value.resize(n);
    return fs::path(value);
}
fs::path known_directory(REFKNOWNFOLDERID id) {
    PWSTR value = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &value)))
        throw Error("Cannot find Windows folder");
    fs::path result(value);
    CoTaskMemFree(value);
    return result;
}
bool elevated() {
    HANDLE token = nullptr;
    win_check(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) != 0,
              "Cannot inspect process token");
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    bool ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) != 0;
    CloseHandle(token);
    win_check(ok, "Cannot inspect process elevation");
    return elevation.TokenIsElevated != 0;
}
void require_supported_windows() {
    using VersionFn = LONG(WINAPI *)(OSVERSIONINFOW *);
    auto fn = reinterpret_cast<VersionFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    OSVERSIONINFOW v{};
    v.dwOSVersionInfoSize = sizeof(v);
    if (!fn || fn(&v) != 0 || v.dwMajorVersion < 10)
        throw Error("brlinst requires 64-bit Windows 10 or later");
}
fs::path default_directory(const Json &product, bool machine) {
    auto parent = known_directory(machine ? FOLDERID_ProgramFiles : FOLDERID_UserProgramFiles);
    return parent / (wide(product.at("name")) + L" " + wide(product.at("version")) + L" " +
                     wide(architecture_name(product_architecture(product))));
}
void validate_destination(const fs::path &path, bool machine) {
    if (!path.is_absolute() || path.wstring().rfind(L"\\\\", 0) == 0)
        throw Error("Choose an absolute path on a local drive", ExitCode::arguments);
    check_no_links(path);
    auto drive = path.root_path();
    if (GetDriveTypeW(drive.c_str()) != DRIVE_FIXED)
        throw Error("Choose a directory on a fixed local drive", ExitCode::arguments);
    if (machine) {
        // Machine uninstall records are privileged instructions. Restrict v1 to
        // the protected Program Files hierarchy rather than accepting records
        // placed in an arbitrary user's writable directory.
        auto base = normalize(known_directory(FOLDERID_ProgramFiles).wstring()) + L"\\";
        if (normalize(path.wstring()).rfind(base, 0) != 0)
            throw Error("All-users installations must be inside Program Files", ExitCode::arguments);
        if (normalize(path.parent_path().wstring()) !=
            normalize(known_directory(FOLDERID_ProgramFiles).wstring()))
            throw Error(
                "For all-users installation, choose a new version folder directly inside Program Files",
                ExitCode::arguments);
    }
}
std::wstring quote_argument(const std::wstring &text) {
    std::wstring out = L"\"";
    unsigned slashes = 0;
    for (wchar_t c : text) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        if (c == L'"')
            out.append(slashes * 2 + 1, L'\\');
        else
            out.append(slashes, L'\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, L'\\');
    out += L'"';
    return out;
}
int elevate_install(const Options &o) {
    std::wstring args = L"--apply --scope machine --directory " + quote_argument(o.directory.wstring());
    args += L" --start-menu " + std::wstring(o.start_menu ? L"on" : L"off");
    args += L" --desktop " + std::wstring(o.desktop ? L"on" : L"off");
    args += L" --path " + std::wstring(o.path ? L"on" : L"off");
    if (o.recover)
        args += L" --recover";
    // The elevated process creates its own log instead of writing an arbitrary
    // lower-privilege path supplied by its parent.
    return elevate(args);
}
int elevate_uninstall(const Options &) {
    return elevate(L"");
}
void check_registered(const Json &p, bool machine) {
    Key key;
    auto status =
        RegOpenKeyExW(hive(machine), registration_key(p).c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &key.value);
    if (status == ERROR_SUCCESS)
        throw Error("This version is already installed for the selected scope. Uninstall it first.",
                    ExitCode::existing);
    if (status != ERROR_FILE_NOT_FOUND)
        reg_check(status, "Cannot inspect existing installation");
}
Log::Log(const fs::path &requested) {
    if (requested.empty()) {
        std::array<wchar_t, 32768> temp{};
        DWORD n = GetTempPathW(static_cast<DWORD>(temp.size()), temp.data());
        win_check(n > 0 && n < temp.size(), "Cannot find log directory");
        path = fs::path(temp.data()) / (L"brlinst-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                                        std::to_wstring(GetTickCount64()) + L".log");
    } else
        path = fs::absolute(requested);
    check_no_links(path);
    file.open(path, std::ios::app | std::ios::binary);
    if (!file)
        throw Error("Cannot open log: " + utf8(path));
}
void Log::write(const std::string &message) {
    SYSTEMTIME time{};
    GetSystemTime(&time);
    file << std::setfill('0') << std::setw(4) << time.wYear << '-' << std::setw(2) << time.wMonth << '-'
         << std::setw(2) << time.wDay << ' ' << std::setw(2) << time.wHour << ':' << std::setw(2)
         << time.wMinute << ':' << std::setw(2) << time.wSecond << " UTC " << message << '\n';
    file.flush();
    if (!file)
        throw Error("Cannot write operation log");
}
OperationLock::OperationLock(bool machine) {
    std::wstring name = L"Global\\Brlinst.MachineOperation";
    if (!machine) {
        HANDLE token = nullptr;
        win_check(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) != 0,
                  "Cannot inspect user identity");
        DWORD length = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &length);
        std::vector<BYTE> data(length);
        bool ok = GetTokenInformation(token, TokenUser, data.data(), length, &length) != 0;
        CloseHandle(token);
        win_check(ok, "Cannot read user identity");
        LPWSTR sid = nullptr;
        win_check(ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(data.data())->User.Sid, &sid) != 0,
                  "Cannot format user identity");
        name = L"Global\\Brlinst.UserOperation." + std::wstring(sid);
        LocalFree(sid);
    }
    handle = CreateMutexW(nullptr, FALSE, name.c_str());
    win_check(handle != nullptr, "Cannot create installation lock");
    auto result = WaitForSingleObject(handle, 0);
    if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED) {
        CloseHandle(handle);
        handle = nullptr;
        throw Error("Another brlinst operation is running");
    }
}
OperationLock::~OperationLock() {
    if (handle) {
        ReleaseMutex(handle);
        CloseHandle(handle);
    }
}
void WindowsIntegration::install(Json &state, const fs::path &journal) {
    Com com;
    bool machine = state.at("machine");
    auto &info = state["integration"];
    auto links = shortcuts(state);
    info["shortcuts"] = Json::array();
    for (const auto &link : links) {
        check_no_links(link.path);
        if (fs::exists(link.path))
            throw Error("Shortcut already exists: " + utf8(link.path));
        info["shortcuts"].push_back({{"path", utf8(link.path)}});
        durable_json(journal, state);
        fs::create_directories(link.path.parent_path());
        write_link(link, state);
        auto hash = fingerprint(link.path);
        info["shortcuts"].back()["size"] = hash.size;
        info["shortcuts"].back()["crc"] = hash.crc;
        durable_json(journal, state);
    }
    if (state["options"].value("path", false))
        change_path(state, journal, false);
    check_registered(state.at("product"), machine);
    info["registration"] = true;
    durable_json(journal, state);
    Key key;
    DWORD disposition = 0;
    reg_check(RegCreateKeyExW(hive(machine), registration_key(state.at("product")).c_str(), 0, nullptr, 0,
                              KEY_WRITE | KEY_WOW64_64KEY, nullptr, &key.value, &disposition),
              "Cannot register installation");
    if (disposition != REG_CREATED_NEW_KEY)
        throw Error("Registration appeared during installation");
    auto root = path_from_utf8(state.at("root"));
    auto exe = root / state_directory / uninstall_filename;
    set_text(key.value, L"InstallLocation", root.wstring());
    set_text(key.value, L"DisplayName", wide(display_name(state.at("product"), machine)));
    set_text(key.value, L"DisplayVersion", wide(state["product"]["version"]));
    set_text(key.value, L"Publisher", wide(state["product"]["publisher"]));
    set_text(key.value, L"UninstallString", quote_argument(exe.wstring()));
    set_text(key.value, L"QuietUninstallString", quote_argument(exe.wstring()) + L" --quiet");
    set_text(key.value, L"DisplayIcon", exe.wstring());
    if (state["product"].contains("website"))
        set_text(key.value, L"URLInfoAbout", wide(state["product"]["website"]));
    DWORD one = 1;
    for (auto name : {L"NoModify", L"NoRepair"})
        reg_check(RegSetValueExW(key.value, name, 0, REG_DWORD, reinterpret_cast<BYTE *>(&one), sizeof(one)),
                  "Cannot write registration flags");
    std::uint64_t size = 0;
    for (const auto &file : state["files"])
        size += file["size"].get<std::uint64_t>();
    DWORD kb = static_cast<DWORD>(std::min<std::uint64_t>(size / 1024, UINT32_MAX));
    reg_check(
        RegSetValueExW(key.value, L"EstimatedSize", 0, REG_DWORD, reinterpret_cast<BYTE *>(&kb), sizeof(kb)),
        "Cannot write installed size");
}
void WindowsIntegration::remove(Json &state, const fs::path &journal) {
    Com com;
    bool machine = state.at("machine");
    auto &info = state["integration"];
    auto allowed = shortcuts(state);
    if (info.contains("shortcuts"))
        for (const auto &recorded : info["shortcuts"]) {
            auto path = path_from_utf8(recorded.at("path"));
            if (std::none_of(allowed.begin(), allowed.end(), [&](const Shortcut &s) {
                    return normalize(s.path.wstring()) == normalize(path.wstring());
                }))
                throw Error("Unrecognized shortcut in journal");
            check_no_links(path);
            if (!fs::exists(path))
                continue;
            bool owned = owned_link(path, state);
            if (recorded.contains("crc")) {
                auto hash = fingerprint(path);
                owned = owned && hash.size == recorded.at("size").get<std::uint64_t>() &&
                        hash.crc == recorded.at("crc").get<std::uint32_t>();
            }
            if (owned)
                fs::remove(path);
        }
    for (const auto &link : allowed) {
        auto parent = link.path.parent_path();
        if (parent.filename() == wide(display_name(state.at("product"), machine))) {
            check_no_links(parent);
            if (fs::is_directory(parent) && fs::is_empty(parent))
                fs::remove(parent);
        }
    }
    change_path(state, journal, true);
    if (info.value("registration", false)) {
        auto subkey = registration_key(state.at("product"));
        auto location = read_value(hive(machine), subkey, L"InstallLocation");
        if (location.exists &&
            normalize(location.text) != normalize(path_from_utf8(state.at("root")).wstring()))
            throw Error("Registration belongs to another installation");
        Key key;
        auto status = RegOpenKeyExW(hive(machine), subkey.c_str(), 0,
                                    KEY_READ | KEY_WRITE | DELETE | KEY_WOW64_64KEY, &key.value);
        if (status != ERROR_FILE_NOT_FOUND) {
            reg_check(status, "Cannot open uninstall registration");
            reg_check(RegDeleteTreeW(key.value, nullptr), "Cannot remove uninstall registration values");
            RegCloseKey(key.value);
            key.value = nullptr;
            reg_check(RegDeleteKeyExW(hive(machine), subkey.c_str(), KEY_WOW64_64KEY, 0),
                      "Cannot remove uninstall registration");
        }
    }
    info = Json::object();
    durable_json(journal, state);
}

void schedule_cleanup(const fs::path &root, const fs::path &log, bool machine, bool remove_root) {
    // A short-lived copy removes the original running image after its process
    // exits. No shell commands or reboot deletion are used.
    std::array<wchar_t, 32768> temp{};
    win_check(GetTempPathW(static_cast<DWORD>(temp.size()), temp.data()) > 0,
              "Cannot locate cleanup directory");
    auto dir = fs::path(temp.data()) / (L"brlinst-cleanup-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                                        std::to_wstring(GetTickCount64()));
    check_no_links(dir);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    win_check(ConvertStringSecurityDescriptorToSecurityDescriptorW(
                  L"D:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;OW)", SDDL_REVISION_1, &descriptor, nullptr) != 0,
              "Cannot secure cleanup directory");
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor, FALSE};
    bool created = CreateDirectoryW(dir.c_str(), &attributes) != 0;
    LocalFree(descriptor);
    win_check(created, "Cannot create cleanup directory");
    auto helper = dir / L"cleanup.exe";
    fs::copy_file(module_path(), helper);
    std::wstring cmd = quote_argument(helper.wstring()) + L" --cleanup " + quote_argument(root.wstring()) +
                       L" --parent " + std::to_wstring(GetCurrentProcessId()) + L" --log " +
                        quote_argument(log.wstring()) + L" --scope " + (machine ? L"machine" : L"user") +
                        L" --remove-root " + (remove_root ? L"on" : L"off");
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(helper.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        dir.c_str(), &si, &pi)) {
        fs::remove(helper);
        fs::remove(dir);
        throw Error("Could not launch uninstaller cleanup; rerun uninstall to retry");
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}
int cleanup_process(const fs::path &root, DWORD parent_pid, const fs::path &logpath, bool machine,
                    bool remove_root) {
    HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parent_pid);
    if (parent) {
        WaitForSingleObject(parent, INFINITE);
        CloseHandle(parent);
    }
    Log log(logpath);
    try {
        validate_destination(root, machine);
        if (machine && !elevated())
            throw Error("Cleanup requires elevation");
        auto journal = root / state_directory / state_filename;
        check_no_links(journal);
        if (fs::exists(journal))
            throw Error("Uninstall did not finalize its recovery journal");
        OperationLock lock(machine);
        auto uninstaller = root / state_directory / uninstall_filename;
        check_no_links(uninstaller);
        if (fs::exists(uninstaller)) {
            if (!fs::is_regular_file(uninstaller))
                throw Error("Installed uninstaller is not a regular file");
            std::error_code error;
            for (unsigned attempt = 0; attempt != cleanup_remove_attempts; ++attempt) {
                if (fs::remove(uninstaller, error))
                    break;
                if (!fs::exists(uninstaller))
                    break;
                std::this_thread::sleep_for(cleanup_remove_delay);
            }
            if (fs::exists(uninstaller))
                throw Error("Cannot remove installed uninstaller");
        }
        auto state_dir = root / state_directory;
        check_no_links(state_dir);
        if (fs::is_directory(state_dir) && fs::is_empty(state_dir))
            fs::remove(state_dir);
        if (remove_root && fs::is_directory(root) && fs::is_empty(root))
            fs::remove(root);
        log.write("Uninstaller self-removal complete");
        // A Windows process cannot reliably delete its mapped image. Keep only
        // this small helper in TEMP; never schedule deletion of user data.
        return 0;
    } catch (const std::exception &e) {
        log.write(std::string("Cleanup incomplete: ") + e.what());
        return static_cast<int>(ExitCode::failure);
    }
}
} // namespace installer
