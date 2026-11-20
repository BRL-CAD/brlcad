// SPDX-License-Identifier: BSD-2-Clause
// Read-only MSI preflight. Files, registry, shortcuts and PATH are changed only
// by standard Windows Installer actions, never by this DLL.
#include "installer/core.hpp"
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <shlobj.h>
#include <algorithm>
namespace {
using namespace installer;
std::wstring property(MSIHANDLE install, const wchar_t *name) {
    DWORD size = 0;
    auto status = MsiGetPropertyW(install, name, nullptr, &size);
    if (status != ERROR_MORE_DATA && status != ERROR_SUCCESS)
        throw Error("Cannot read installation property");
    DWORD capacity = size + 1;
    std::wstring value(capacity, L'\0');
    if (MsiGetPropertyW(install, name, value.data(), &capacity) != ERROR_SUCCESS)
        throw Error("Cannot read installation property");
    value.resize(capacity);
    return value;
}
fs::path known(REFKNOWNFOLDERID id) {
    PWSTR value = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &value)))
        throw Error("Cannot resolve Windows installation folder");
    fs::path result(value);
    CoTaskMemFree(value);
    return result;
}
void validate(MSIHANDLE install) {
    const auto scope = property(install, L"ALLUSERS");
    const auto per_user = property(install, L"MSIINSTALLPERUSER");
    // ALLUSERS=2 is the valid dual-purpose authoring value and may survive
    // costing, in which case MSIINSTALLPERUSER disambiguates the chosen scope.
    const bool machine = scope == L"1" || (scope == L"2" && per_user.empty());
    const bool user = scope.empty() || (scope == L"2" && per_user == L"1");
    if (!machine && !user)
        throw Error("Invalid installation scope after costing.");
    for (auto name : {L"BRLINST_STARTMENU", L"BRLINST_DESKTOP", L"BRLINST_PATH"}) {
        auto value = property(install, name);
        if (!value.empty() && value != L"0" && value != L"1")
            throw Error("Shortcut and PATH options must be 0 or 1.");
    }
    if (property(install, L"BRLINST_PATH") == L"1" && property(install, L"BRLINST_HASBIN") != L"1")
        throw Error("This package has no bin directory to add to PATH.");
    auto code = property(install, L"ProductCode");
    for (auto context : {MSIINSTALLCONTEXT_USERMANAGED, MSIINSTALLCONTEXT_USERUNMANAGED,
                         MSIINSTALLCONTEXT_MACHINE}) {
        wchar_t state[32]{};
        DWORD count = 32;
        auto status =
            MsiGetProductInfoExW(code.c_str(), nullptr, context, INSTALLPROPERTY_PRODUCTSTATE, state, &count);
        if (status == ERROR_SUCCESS)
            throw Error(
                "This version is already registered for your account or for all users. Uninstall it first.");
        if (status != ERROR_UNKNOWN_PRODUCT && status != ERROR_UNKNOWN_PROPERTY)
            throw Error("Cannot check existing Windows Installer registration.");
    }
    auto text = property(install, L"INSTALLDIR");
    fs::path root(text);
    while (root.has_relative_path() && root.filename().empty())
        root = root.parent_path();
    auto drive = root.root_name().wstring();
    if (!root.is_absolute() || drive.size() != 2 || drive[1] != L':' ||
        !((drive[0] >= L'A' && drive[0] <= L'Z') || (drive[0] >= L'a' && drive[0] <= L'z')) ||
        !root.has_relative_path())
        throw Error("Choose a folder beneath a local drive root, using an absolute drive-letter path.");
    validate_relative(root.relative_path().generic_u8string());
    if (GetDriveTypeW(root.root_path().c_str()) != DRIVE_FIXED)
        throw Error("Choose a local fixed drive for installation.");
    check_no_links(root);
    if (machine) {
        auto parent = root.parent_path().wstring();
        auto program_files = known(FOLDERID_ProgramFilesX64).wstring();
        if (CompareStringOrdinal(parent.c_str(), -1, program_files.c_str(), -1, TRUE) != CSTR_EQUAL)
            throw Error("For all-users installation, choose a folder directly beneath Program Files.");
    }
    if (fs::exists(root) && (!fs::is_directory(root) || !fs::is_empty(root)))
        throw Error("The destination must be new or empty. Choose another folder or uninstall the existing "
                    "version first.");
}
UINT run(MSIHANDLE install, bool ui) noexcept {
    try {
        MsiSetPropertyW(install, L"BRLINSTVALID", L"");
        validate(install);
        if (MsiSetPropertyW(install, L"BRLINSTVALID", L"1") != ERROR_SUCCESS)
            return ERROR_INSTALL_FAILURE;
        return ERROR_SUCCESS;
    } catch (const std::exception &e) {
        try {
            auto text = path_from_utf8(e.what()).wstring();
            MsiSetPropertyW(install, L"BRLINSTERROR", text.c_str());
            auto record = MsiCreateRecord(1);
            if (record) {
                MsiRecordSetStringW(record, 0, L"[1]");
                MsiRecordSetStringW(record, 1, text.c_str());
                MsiProcessMessage(install, ui ? INSTALLMESSAGE_INFO : INSTALLMESSAGE_ERROR, record);
                MsiCloseHandle(record);
            }
        } catch (...) {
            return ERROR_INSTALL_FAILURE;
        }
        return ui ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    } catch (...) {
        return ERROR_INSTALL_FAILURE;
    }
}
} // namespace
extern "C" __declspec(dllexport) UINT __stdcall BrlinstCheck(MSIHANDLE install) {
    return run(install, false);
}
extern "C" __declspec(dllexport) UINT __stdcall BrlinstCheckUI(MSIHANDLE install) {
    return run(install, true);
}
