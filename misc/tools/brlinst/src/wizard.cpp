// SPDX-License-Identifier: BSD-2-Clause
#include "installer/windows.hpp"
#include <commctrl.h>
#include <shellapi.h>
#include <array>
#include <sstream>

namespace installer {
namespace {
constexpr int id_back = 100, id_next = 101, id_cancel = 102, id_user = 103, id_machine = 104;
constexpr int id_directory = 105, id_browse = 106, id_start = 107, id_desktop = 108, id_path = 109;
constexpr int id_content = 110, id_progress = 111;
constexpr int classic_width = 508, classic_height = 360, classic_page_bottom = 314;
constexpr int classic_header_width = 150, classic_header_text_left = classic_header_width + 8;
constexpr int classic_license_top = 65, classic_license_bottom = 300;
constexpr int header_bitmap_id = 200, sidebar_bitmap_id = 201;
std::wstring control_text(HWND control) {
    int n = GetWindowTextLengthW(control);
    std::wstring text(static_cast<std::size_t>(n) + 1, L'\0');
    GetWindowTextW(control, text.data(), n + 1);
    text.resize(n);
    return text;
}
} // namespace
struct Wizard::Impl {
    Json product;
    Options &options;
    bool removing = false, cancel = false, ready = false, closed = false, complete = false, running = false;
    int page = 0;
    HWND hwnd = nullptr;
    HFONT font = nullptr, title_font = nullptr;
    HBRUSH background = nullptr;
    HBITMAP header_bitmap = nullptr, sidebar_bitmap = nullptr;
    double scale = 1;
    std::vector<HWND> controls;
    std::wstring finished;
    Impl(const Json &p, Options &o, bool r) : product(p), options(o), removing(r) {
        INITCOMMONCONTROLSEX common{sizeof(common), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES};
        InitCommonControlsEx(&common);
        WNDCLASSW wc{};
        wc.lpfnWndProc = procedure;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"BrlinstWizard";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        background = GetSysColorBrush(COLOR_WINDOW);
        wc.hbrBackground = background;
        wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1));
        if (!wc.hIcon)
            wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        RegisterClassW(&wc);
        auto screen = GetDC(nullptr);
        scale = static_cast<double>(GetDeviceCaps(screen, LOGPIXELSX)) / 96.0;
        ReleaseDC(nullptr, screen);
        font =
            CreateFontW(-px(classic() ? 12 : 14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        title_font = CreateFontW(-px(classic() ? 16 : 23), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                                 L"Segoe UI");
        if (classic()) {
            header_bitmap = static_cast<HBITMAP>(LoadImageW(wc.hInstance, MAKEINTRESOURCEW(header_bitmap_id),
                                                            IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
            sidebar_bitmap = static_cast<HBITMAP>(LoadImageW(
                wc.hInstance, MAKEINTRESOURCEW(sidebar_bitmap_id), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
        }
        RECT rect{0, 0, px(classic() ? classic_width : 700), px(classic() ? classic_height : 500)};
        AdjustWindowRectEx(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0);
        auto title = wide(product.at("name")) + (r ? L" Uninstall" : L" Setup");
        hwnd = CreateWindowExW(0, wc.lpszClassName, title.c_str(),
                               WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT,
                               CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr,
                               nullptr, wc.hInstance, this);
        win_check(hwnd != nullptr, "Cannot create installer window");
        show_page();
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }
    ~Impl() {
        if (hwnd && IsWindow(hwnd))
            DestroyWindow(hwnd);
        if (font)
            DeleteObject(font);
        if (title_font)
            DeleteObject(title_font);
        if (header_bitmap)
            DeleteObject(header_bitmap);
        if (sidebar_bitmap)
            DeleteObject(sidebar_bitmap);
    }
    bool classic() const {
        return product.contains("branding") && product["branding"].value("style", "") == "classic";
    }
    int px(int value) const {
        return static_cast<int>(value * scale);
    }
    HWND add(const wchar_t *cls, const std::wstring &text, int id, int x, int y, int w, int h,
             DWORD style = 0) {
        HWND child = CreateWindowExW(cls == std::wstring(L"EDIT") ? WS_EX_CLIENTEDGE : 0, cls, text.c_str(),
                                     WS_CHILD | WS_VISIBLE | style, px(x), px(y), px(w), px(h), hwnd,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                     GetModuleHandleW(nullptr), nullptr);
        win_check(child != nullptr, "Cannot create wizard control");
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        controls.push_back(child);
        return child;
    }
    void bitmap(HBITMAP image, int x, int y, int width, int height) {
        if (!image)
            return;
        auto control = add(L"STATIC", L"", 0, x, y, width, height, SS_BITMAP);
        SendMessageW(control, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(image));
    }
    void label(const std::wstring &text, int y, int h = 65) {
        add(L"STATIC", text, 0, 28, y, 640, h);
    }
    void show_page() {
        for (auto c : controls)
            DestroyWindow(c);
        controls.clear();
        auto heading = page == 0   ? (removing ? L"Remove this version" : L"Welcome")
                       : page == 1 ? L"License information"
                       : page == 2 ? L"Installation location"
                       : page == 3 ? L"Shortcuts and command line"
                       : page == 4 ? L"Ready to install"
                       : page == 5 ? (removing ? L"Removing files" : L"Installing")
                                   : L"Finished";
        const bool compact = classic();
        const bool side_page = compact && (page == 0 || page == 6);
        if (compact) {
            if (side_page)
                bitmap(sidebar_bitmap, 0, 0, 164, classic_page_bottom);
            else
                bitmap(header_bitmap, 0, 0, classic_header_width, 57);
        }
        const int left = side_page ? 184 : (compact ? 20 : 28);
        const int content_width = side_page ? 304 : (compact ? 468 : 640);
        const int title_left = compact && !side_page ? classic_header_text_left : left;
        auto title = add(L"STATIC", heading, 0, title_left, compact ? 12 : 22,
                         compact ? classic_width - title_left - 20 : 640, compact ? 28 : 38);
        SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font), TRUE);
        if (page == 0) {
            auto text = wide(display_name(product, options.machine));
            text +=
                removing
                    ? L"\r\n\r\nRemove this installation and its shortcuts. User-created and modified files "
                      L"will be preserved."
                    : L"\r\n\r\n" +
                          wide(product.value("welcome", "This wizard will guide you through installation."));
            if (product.contains("website"))
                text += L"\r\n\r\n" + wide(product.at("website"));
            if (removing)
                text += L"\r\n\r\nLocation: " + options.directory.wstring();
            if (compact)
                add(L"STATIC", text, 0, left, 60, content_width, 235);
            else
                label(text, 90, 300);
        } else if (page == 1) {
            auto text = wide(product.at("licenseText"));
            std::wstring normalized;
            for (auto c : text) {
                if (c == L'\n' && (normalized.empty() || normalized.back() != L'\r'))
                    normalized += L'\r';
                normalized += c;
            }
            add(L"EDIT", normalized, id_content, left, compact ? classic_license_top : 80, content_width,
                compact ? classic_license_bottom - classic_license_top : 335,
                ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_TABSTOP);
        } else if (page == 2) {
            add(L"STATIC", L"Install for:", 0, left, compact ? 60 : 82, content_width, 25);
            auto user = add(L"BUTTON", L"Only me (no administrator permission needed)", id_user, left,
                            compact ? 82 : 115, content_width, compact ? 22 : 28,
                            BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP);
            auto machine = add(L"BUTTON", L"All users (administrator permission required)", id_machine,
                               left, compact ? 108 : 150, content_width, compact ? 22 : 28,
                               BS_AUTORADIOBUTTON | WS_TABSTOP);
            SendMessageW(options.machine ? machine : user, BM_SETCHECK, BST_CHECKED, 0);
            add(L"STATIC", L"Installation folder:", 0, left, compact ? 145 : 210, content_width, 22);
            add(L"EDIT", options.directory.wstring(), id_directory, left, compact ? 169 : 243,
                compact ? 370 : 530, compact ? 24 : 30,
                ES_AUTOHSCROLL | WS_TABSTOP);
            add(L"BUTTON", L"Browse...", id_browse, compact ? 400 : 570, compact ? 169 : 243,
                compact ? 88 : 100, compact ? 24 : 30, WS_TABSTOP);
            add(L"STATIC",
                L"Choose a new or empty folder. Different versions can be installed separately. All-users "
                L"installations use a folder inside Program Files.",
                0, left, compact ? 210 : 295, content_width, compact ? 75 : 100);
        } else if (page == 3) {
            auto a = add(L"BUTTON", L"Create Start Menu shortcuts", id_start, left, compact ? 65 : 90,
                         content_width, compact ? 24 : 30,
                         BS_AUTOCHECKBOX | WS_TABSTOP);
            auto b = add(L"BUTTON", L"Create desktop shortcuts", id_desktop, left, compact ? 98 : 135,
                         content_width, compact ? 24 : 30,
                         BS_AUTOCHECKBOX | WS_TABSTOP);
            auto c = add(L"BUTTON", L"Add this version's bin directory to PATH", id_path, left,
                         compact ? 131 : 180, content_width, compact ? 24 : 30,
                         BS_AUTOCHECKBOX | WS_TABSTOP);
            SendMessageW(a, BM_SETCHECK, options.start_menu ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageW(b, BM_SETCHECK, options.desktop ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageW(c, BM_SETCHECK, options.path ? BST_CHECKED : BST_UNCHECKED, 0);
            add(L"STATIC",
                L"Shortcuts identify their version and installation scope.\r\n\r\nPATH changes apply to "
                L"newly started applications. This version is appended; an earlier matching command "
                L"elsewhere in PATH still takes precedence.",
                0, left, compact ? 180 : 245, content_width, compact ? 105 : 160);
        } else if (page == 4) {
            auto text = wide(display_name(product, options.machine)) + L"\r\n\r\nDestination: " +
                        options.directory.wstring();
            text += L"\r\n\r\nStart Menu: " + std::wstring(options.start_menu ? L"yes" : L"no");
            text += L"\r\nDesktop: " + std::wstring(options.desktop ? L"yes" : L"no");
            text += L"\r\nAdd to PATH: " + std::wstring(options.path ? L"yes" : L"no");
            if (options.machine && !elevated())
                text += L"\r\n\r\nWindows will ask for administrator permission.";
            add(L"STATIC", text, 0, left, compact ? 65 : 90, content_width, compact ? 225 : 320);
        } else if (page == 5) {
            add(L"STATIC", L"Please wait. The operation log records detailed progress.", 0, left,
                compact ? 65 : 85, content_width, 45);
            add(L"STATIC", L"Preparing...", id_content, left, compact ? 125 : 155, content_width,
                compact ? 80 : 130);
            add(PROGRESS_CLASSW, L"", id_progress, left, compact ? 225 : 300, content_width,
                compact ? 20 : 25);
            SendMessageW(GetDlgItem(hwnd, id_progress), PBM_SETRANGE32, 0, 1000);
        } else
            add(L"EDIT", finished, id_content, left, compact ? 55 : 85, content_width,
                compact ? 245 : 325,
                ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_TABSTOP);
        if (compact)
            add(L"STATIC", L"", 0, 0, classic_page_bottom, classic_width, 1, SS_ETCHEDHORZ);
        auto back = add(L"BUTTON", L"< Back", id_back, compact ? 265 : 352, compact ? 326 : 450,
                        compact ? 75 : 95, compact ? 23 : 30, WS_TABSTOP);
        auto next =
            add(L"BUTTON",
                page == 6 ? L"Close" : (page == 4 ? L"Install" : (removing ? L"Uninstall" : L"Next >")),
                id_next, compact ? 345 : 457, compact ? 326 : 450, compact ? 75 : 95,
                compact ? 23 : 30, WS_TABSTOP | BS_DEFPUSHBUTTON);
        auto stop = add(L"BUTTON", L"Cancel", id_cancel, compact ? 425 : 570, compact ? 326 : 450,
                        compact ? 75 : 100, compact ? 23 : 30, WS_TABSTOP);
        EnableWindow(back, page > 0 && page < 5 && !removing);
        EnableWindow(next, page != 5);
        EnableWindow(stop, page != 6 && !(page == 5 && removing));
        SendMessageW(hwnd, DM_SETDEFID, id_next, 0);
        if (page != 5)
            SetFocus(next);
    }
    void capture() {
        if (page == 2)
            options.directory = fs::path(control_text(GetDlgItem(hwnd, id_directory)));
        if (page == 3) {
            options.start_menu = SendMessageW(GetDlgItem(hwnd, id_start), BM_GETCHECK, 0, 0) == BST_CHECKED;
            options.desktop = SendMessageW(GetDlgItem(hwnd, id_desktop), BM_GETCHECK, 0, 0) == BST_CHECKED;
            options.path = SendMessageW(GetDlgItem(hwnd, id_path), BM_GETCHECK, 0, 0) == BST_CHECKED;
        }
    }
    void browse() {
        BROWSEINFOW bi{};
        bi.hwndOwner = hwnd;
        bi.lpszTitle = L"Choose an empty installation folder";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        auto item = SHBrowseForFolderW(&bi);
        if (item) {
            std::array<wchar_t, MAX_PATH> path{};
            if (SHGetPathFromIDListW(item, path.data()))
                SetWindowTextW(GetDlgItem(hwnd, id_directory), path.data());
            CoTaskMemFree(item);
        }
    }
    void command(int id) {
        if (id == id_cancel) {
            cancel = true;
            if (!running)
                closed = true;
        } else if (id == id_back) {
            capture();
            --page;
            show_page();
        } else if (id == id_next) {
            if (page == 6) {
                closed = true;
                return;
            }
            capture();
            if (page == 2) {
                validate_destination(options.directory, options.machine);
                if (!fs::is_directory(options.directory.parent_path()))
                    throw Error("Choose a folder whose parent already exists.");
                if (fs::exists(options.directory) && !fs::is_empty(options.directory))
                    throw Error(
                        "Choose a new or empty folder. Uninstall an existing copy before reinstalling.");
                check_registered(product, options.machine);
            }
            if (page == 4 || removing) {
                ready = true;
                running = true;
                page = 5;
                show_page();
            } else {
                ++page;
                show_page();
            }
        } else if (id == id_user || id == id_machine) {
            capture();
            auto old = default_directory(product, options.machine);
            options.machine = id == id_machine;
            if (options.directory == old)
                options.directory = default_directory(product, options.machine);
            SetWindowTextW(GetDlgItem(hwnd, id_directory), options.directory.c_str());
        } else if (id == id_browse)
            browse();
    }
    void pump() {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                cancel = true;
                closed = true;
                continue;
            }
            if (!IsDialogMessageW(hwnd, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }
    static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wp, LPARAM lp) {
        auto self = reinterpret_cast<Impl *>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<Impl *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (self)
            try {
                if (message == WM_COMMAND && HIWORD(wp) == BN_CLICKED) {
                    self->command(LOWORD(wp));
                    return 0;
                }
                if (message == WM_CLOSE) {
                    if (!self->running || !self->removing) {
                        self->cancel = true;
                        if (!self->running)
                            self->closed = true;
                    }
                    return 0;
                }
                if (message == WM_CTLCOLORSTATIC) {
                    SetBkColor(reinterpret_cast<HDC>(wp), GetSysColor(COLOR_WINDOW));
                    return reinterpret_cast<LRESULT>(self->background);
                }
            } catch (const std::exception &e) {
                MessageBoxW(window, wide(e.what()).c_str(), L"brlinst", MB_OK | MB_ICONERROR);
                return 0;
            }
        return DefWindowProcW(window, message, wp, lp);
    }
};
Wizard::Wizard(const Json &p, Options &o, bool r) : impl(std::make_unique<Impl>(p, o, r)) {}
Wizard::~Wizard() = default;
bool Wizard::configure() {
    while (!impl->ready && !impl->closed) {
        impl->pump();
        if (!impl->ready && !impl->closed)
            WaitMessage();
    }
    return impl->ready;
}
void Wizard::progress(const std::string &text, std::uint64_t done, std::uint64_t total) {
    if (impl->page != 5) {
        impl->page = 5;
        impl->running = true;
        impl->show_page();
    }
    SetWindowTextW(GetDlgItem(impl->hwnd, id_content), wide(text).c_str());
    SendMessageW(GetDlgItem(impl->hwnd, id_progress), PBM_SETPOS,
                 total ? static_cast<WPARAM>(done * 1000 / total) : 0, 0);
    impl->pump();
}
bool Wizard::cancelled() {
    impl->pump();
    return impl->cancel;
}
void Wizard::finish(const std::string &text) {
    impl->running = false;
    impl->page = 6;
    impl->finished = wide(text);
    impl->show_page();
    while (!impl->closed) {
        impl->pump();
        if (!impl->closed)
            WaitMessage();
    }
}
HWND Wizard::window() const {
    return impl->hwnd;
}
} // namespace installer
