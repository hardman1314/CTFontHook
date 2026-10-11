// CTFontHook 安装器
//
// 面向「完全不懂技术」的玩家：双击运行 → 选择游戏的启动程序(.exe) → 自动识别 32/64 位
// → 备份游戏目录里原有的 winmm.dll / FontHook.ini → 放入对应架构的 winmm.dll。
// 卸载/还原：读取安装时写入的清单，自动恢复或删除。
//
// 设计约束：
//  - 只做「安装 / 卸载」，不做启动游戏、不做导入预判（后续版本再增强）。
//  - 清单需管理员权限（RequireAdministrator），装入 Program Files 也能成功。
//  - 静态链接 CRT，随发布包分发即可运行；从自身同级目录读取 32bit\ / 64bit\winmm.dll。
//
// 注意：本文件保存为 UTF-8，工程使用 /utf-8 编译，因此源码中的中文可安全用于宽字符字面量。

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <vector>

#include "resource.h"

namespace {

const wchar_t* kAppTitle = L"CTFontHook 安装器";
const wchar_t* kWndClass = L"CTFontHookSetupWnd";
const wchar_t* kBakDirName = L".CTFontHook";
const wchar_t* kManifestName = L"manifest.ini";

HWND g_hPathEdit = nullptr;
HWND g_hArchLabel = nullptr;
HWND g_hLog = nullptr;
HWND g_hHint = nullptr;

std::wstring g_selectedExe;

// 界面尺寸以 96 DPI 为设计单位，实际像素经 S() 按系统 DPI 放大，
// 避免高分屏上字体放大而固定像素布局导致提示文字被裁剪。
const int kClientW = 620;
const int kClientH = 450;

int g_dpi = 96;
int S(int px) { return MulDiv(px, g_dpi, 96); }
HFONT g_uiFont = nullptr;

// ---------------------------------------------------------------------------
// 基础工具
// ---------------------------------------------------------------------------

std::wstring DirNameOf(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return std::wstring();
    return path.substr(0, pos);
}

bool FileExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool DirExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

bool EnsureDir(const std::wstring& p) {
    if (DirExists(p)) return true;
    if (CreateDirectoryW(p.c_str(), nullptr)) return true;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

std::wstring ExeDir() {
    wchar_t buf[MAX_PATH] = {};
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return std::wstring();
    return DirNameOf(buf);
}

std::wstring NowString() {
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    wchar_t b[32] = {};
    wsprintfW(b, L"%04d-%02d-%02d %02d:%02d:%02d",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return b;
}

void Log(const std::wstring& s) {
    if (!g_hLog) return;
    std::wstring line = s + L"\r\n";
    int len = GetWindowTextLengthW(g_hLog);
    SendMessageW(g_hLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
}

// ---------------------------------------------------------------------------
// PE 架构识别（不运行目标程序）
// ---------------------------------------------------------------------------

// 返回 IMAGE_FILE_HEADER.Machine；失败返回 0。
WORD ReadPeMachine(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return 0;

    WORD machine = 0;
    DWORD read = 0;
    IMAGE_DOS_HEADER dos = {};
    if (ReadFile(h, &dos, sizeof(dos), &read, nullptr) &&
        read == sizeof(dos) && dos.e_magic == IMAGE_DOS_SIGNATURE) {
        DWORD sig = 0;
        if (SetFilePointer(h, dos.e_lfanew, nullptr, FILE_BEGIN)
                != INVALID_SET_FILE_POINTER &&
            ReadFile(h, &sig, sizeof(sig), &read, nullptr) &&
            read == sizeof(sig) && sig == IMAGE_NT_SIGNATURE) {
            IMAGE_FILE_HEADER fh = {};
            if (ReadFile(h, &fh, sizeof(fh), &read, nullptr) &&
                read == sizeof(fh)) {
                machine = fh.Machine;
            }
        }
    }
    CloseHandle(h);
    return machine;
}

const wchar_t* ArchLabel(WORD m) {
    switch (m) {
    case IMAGE_FILE_MACHINE_I386:  return L"32 位";
    case IMAGE_FILE_MACHINE_AMD64: return L"64 位";
    case IMAGE_FILE_MACHINE_ARM64: return L"ARM64（暂不支持）";
    default:                       return L"无法识别";
    }
}

bool ArchSupported(WORD m) {
    return m == IMAGE_FILE_MACHINE_I386 || m == IMAGE_FILE_MACHINE_AMD64;
}

const wchar_t* ArchSubDir(WORD m) {
    return m == IMAGE_FILE_MACHINE_I386 ? L"32bit" : L"64bit";
}

// ---------------------------------------------------------------------------
// 文件占用检测
// ---------------------------------------------------------------------------

bool IsFileLocked(const std::wstring& p) {
    if (!FileExists(p)) return false;
    HANDLE h = CreateFileW(p.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) return false;
        return true;  // 共享冲突或权限不足，按「不可写」处理
    }
    CloseHandle(h);
    return false;
}

// ---------------------------------------------------------------------------
// 清单读写（UTF-16LE + BOM）
// ---------------------------------------------------------------------------

void WriteManifest(const std::wstring& path, WORD machine,
                   bool hadWinmm, bool hadIni) {
    std::wstring s;
    s += L"[CTFontHook]\r\n";
    s += L"InstalledAt=" + NowString() + L"\r\n";
    s += std::wstring(L"DllArch=") + ArchSubDir(machine) + L"\r\n";
    s += std::wstring(L"HadWinmm=") + (hadWinmm ? L"1" : L"0") + L"\r\n";
    s += std::wstring(L"HadIni=") + (hadIni ? L"1" : L"0") + L"\r\n";

    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    const wchar_t bom = 0xFEFF;
    WriteFile(h, &bom, sizeof(bom), &w, nullptr);
    WriteFile(h, s.c_str(), (DWORD)(s.size() * sizeof(wchar_t)), &w, nullptr);
    CloseHandle(h);
}

bool ReadManifestValue(const std::wstring& path, const std::wstring& key,
                       std::wstring* out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD size = GetFileSize(h, nullptr);
    std::vector<char> buf(size ? size : 1);
    DWORD rd = 0;
    BOOL ok = ReadFile(h, buf.data(), size, &rd, nullptr);
    CloseHandle(h);
    if (!ok || rd < sizeof(wchar_t)) return false;

    const wchar_t* p = reinterpret_cast<const wchar_t*>(buf.data());
    size_t n = rd / sizeof(wchar_t);
    if (n && p[0] == 0xFEFF) { ++p; --n; }
    std::wstring text(p, n);

    std::wstring k = key + L"=";
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t eol = text.find(L'\n', pos);
        std::wstring line = (eol == std::wstring::npos)
            ? text.substr(pos) : text.substr(pos, eol - pos);
        while (!line.empty() && (line.back() == L'\r' || line.back() == L'\n'))
            line.pop_back();
        if (line.compare(0, k.size(), k) == 0) {
            *out = line.substr(k.size());
            return true;
        }
        if (eol == std::wstring::npos) break;
        pos = eol + 1;
    }
    return false;
}

// ---------------------------------------------------------------------------
// 安装 / 卸载
// ---------------------------------------------------------------------------

bool InstallPaths(const std::wstring& exeOrDir, std::wstring* gameDir,
                  std::wstring* bakDir, std::wstring* manifest) {
    *gameDir = DirNameOf(exeOrDir);
    if (gameDir->empty()) return false;
    *bakDir = *gameDir + L"\\" + kBakDirName;
    *manifest = *bakDir + L"\\" + kManifestName;
    return true;
}

bool DoInstall() {
    if (g_selectedExe.empty()) {
        Log(L"[错误] 请先点击「浏览」选择游戏的启动程序(.exe)。");
        return false;
    }

    WORD m = ReadPeMachine(g_selectedExe);
    if (!ArchSupported(m)) {
        Log(std::wstring(L"[错误] 无法识别的架构：") + ArchLabel(m) +
            L"，本工具目前只支持 32/64 位程序。");
        return false;
    }

    std::wstring gameDir, bakDir, manifest;
    if (!InstallPaths(g_selectedExe, &gameDir, &bakDir, &manifest)) {
        Log(L"[错误] 无法确定游戏目录。");
        return false;
    }

    std::wstring ourDll = ExeDir() + L"\\" + ArchSubDir(m) + L"\\winmm.dll";
    if (!FileExists(ourDll)) {
        Log(std::wstring(L"[错误] 发布包中缺少 ") + ArchSubDir(m) +
            L"\\winmm.dll，请在完整解压后的发布包内运行本安装器。");
        return false;
    }

    if (FileExists(manifest)) {
        Log(L"[提示] 该游戏目录已安装过 CTFontHook。请先点「卸载 / 还原」再重新安装。");
        return false;
    }

    std::wstring dstDll = gameDir + L"\\winmm.dll";
    std::wstring dstIni = gameDir + L"\\FontHook.ini";

    if (IsFileLocked(dstDll)) {
        Log(L"[错误] 目标 winmm.dll 正被占用（游戏可能正在运行），请先关闭游戏。");
        return false;
    }

    if (!EnsureDir(bakDir) || !EnsureDir(bakDir + L"\\backup")) {
        Log(std::wstring(L"[错误] 无法创建备份目录：") + bakDir +
            L"（请右键以管理员身份运行）。");
        return false;
    }

    bool hadWinmm = FileExists(dstDll);
    bool hadIni = FileExists(dstIni);

    if (hadWinmm) {
        if (!CopyFileW(dstDll.c_str(), (bakDir + L"\\backup\\winmm.dll").c_str(), FALSE)) {
            Log(L"[错误] 备份原 winmm.dll 失败，已中止安装。");
            return false;
        }
    }
    if (hadIni) {
        CopyFileW(dstIni.c_str(), (bakDir + L"\\backup\\FontHook.ini").c_str(), FALSE);
    }

    if (!CopyFileW(ourDll.c_str(), dstDll.c_str(), FALSE)) {
        Log(L"[错误] 复制 winmm.dll 失败（可能被占用或无写入权限）。");
        return false;
    }

    WriteManifest(manifest, m, hadWinmm, hadIni);

    Log(L"[完成] 安装成功！");
    Log(std::wstring(L"　　游戏目录：") + gameDir);
    Log(std::wstring(L"　　使用架构：") + ArchLabel(m) + L"（" +
        ArchSubDir(m) + L"\\winmm.dll）");
    if (hadWinmm) Log(L"　　原有 winmm.dll 已备份到 " + std::wstring(kBakDirName) + L"\\backup。");
    Log(L"　　启动游戏后按 Insert 呼出字体选择器（被占用时试 Pause / Scroll Lock）。");
    return true;
}

bool DoUninstall() {
    if (g_selectedExe.empty()) {
        Log(L"[错误] 请先点击「浏览」选择该游戏的启动程序(.exe)，用于定位游戏目录。");
        return false;
    }

    std::wstring gameDir, bakDir, manifest;
    if (!InstallPaths(g_selectedExe, &gameDir, &bakDir, &manifest)) {
        Log(L"[错误] 无法确定游戏目录。");
        return false;
    }

    if (!FileExists(manifest)) {
        Log(L"[提示] 该目录没有 CTFontHook 安装记录，无需卸载。");
        return false;
    }

    std::wstring dstDll = gameDir + L"\\winmm.dll";
    std::wstring dstIni = gameDir + L"\\FontHook.ini";
    std::wstring bakDll = bakDir + L"\\backup\\winmm.dll";
    std::wstring bakIni = bakDir + L"\\backup\\FontHook.ini";

    if (IsFileLocked(dstDll)) {
        Log(L"[错误] winmm.dll 正被占用（游戏可能正在运行），请先关闭游戏。");
        return false;
    }

    bool hadWinmm = false, hadIni = false;
    std::wstring v;
    if (ReadManifestValue(manifest, L"HadWinmm", &v)) hadWinmm = (v == L"1");
    if (ReadManifestValue(manifest, L"HadIni", &v)) hadIni = (v == L"1");

    if (hadWinmm && FileExists(bakDll)) {
        if (CopyFileW(bakDll.c_str(), dstDll.c_str(), FALSE))
            Log(L"[完成] 已还原游戏原有的 winmm.dll。");
        else
            Log(L"[错误] 还原 winmm.dll 失败。");
    } else {
        DeleteFileW(dstDll.c_str());
        Log(L"[完成] 已移除 winmm.dll。");
    }

    if (hadIni && FileExists(bakIni)) {
        if (CopyFileW(bakIni.c_str(), dstIni.c_str(), FALSE))
            Log(L"[完成] 已还原游戏原有的 FontHook.ini。");
    }

    // 清理备份目录。FontHook.ini 若是本工具生成的则保留（内含玩家设置），不做删除。
    DeleteFileW(manifest.c_str());
    DeleteFileW(bakDll.c_str());
    DeleteFileW(bakIni.c_str());
    RemoveDirectoryW((bakDir + L"\\backup").c_str());
    RemoveDirectoryW(bakDir.c_str());

    Log(L"[完成] 卸载完成，游戏目录已恢复。");
    return true;
}

// ---------------------------------------------------------------------------
// 界面
// ---------------------------------------------------------------------------

void OnBrowse(HWND hWnd) {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"游戏程序 (*.exe)\0*.exe\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrTitle = L"请选择游戏的启动程序(.exe)";
    if (!GetOpenFileNameW(&ofn)) return;

    g_selectedExe = file;
    SetWindowTextW(g_hPathEdit, file);

    WORD m = ReadPeMachine(file);
    std::wstring label = ArchLabel(m);
    SetWindowTextW(g_hArchLabel, (L"检测结果：" + label).c_str());

    Log(L"已选择：" + std::wstring(file));
    Log(L"架构检测：" + label);
    if (ArchSupported(m))
        Log(std::wstring(L"安装时将使用：") + ArchSubDir(m) + L"\\winmm.dll");
    else
        Log(L"[提示] 该程序架构暂不支持安装。");
}

HWND MakeControl(HWND parent, const wchar_t* cls, const wchar_t* text,
                 DWORD style, int x, int y, int w, int h, int id,
                 DWORD exStyle = 0) {
    return CreateWindowExW(exStyle, cls, text, style, x, y, w, h,
        parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
}

void LayoutControls(HWND hWnd) {
    RECT rc = {};
    GetClientRect(hWnd, &rc);
    const int logTop = 178;
    const int hintH = 50;
    const int bottomPad = 14;
    const int gap = 8;
    int logH = rc.bottom - S(logTop) - S(hintH) - S(bottomPad) - S(gap);
    if (logH < S(80)) logH = S(80);
    if (g_hLog)
        MoveWindow(g_hLog, S(16), S(logTop), rc.right - S(32), logH, TRUE);
    if (g_hHint) {
        int hintTop = S(logTop) + logH + S(gap);
        int h = rc.bottom - hintTop - S(bottomPad);
        if (h < S(30)) h = S(30);
        MoveWindow(g_hHint, S(16), hintTop, rc.right - S(32), h, TRUE);
    }
}

void CreateControls(HWND hWnd) {
    g_uiFont = CreateFontW(-S(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    HFONT uiFont = g_uiFont;

    HWND c = nullptr;

    c = MakeControl(hWnd, L"STATIC",
        L"第一步：选择游戏的启动程序(.exe)，工具会自动判断 32/64 位。",
        WS_CHILD | WS_VISIBLE, S(16), S(16), S(588), S(20), IDC_STATIC_TITLE);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    c = MakeControl(hWnd, L"STATIC", L"游戏程序：",
        WS_CHILD | WS_VISIBLE, S(16), S(48), S(80), S(20), IDC_STATIC_PATHLABEL);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    g_hPathEdit = MakeControl(hWnd, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
        S(100), S(46), S(400), S(26), IDC_EDIT_PATH);
    SendMessageW(g_hPathEdit, WM_SETFONT, (WPARAM)uiFont, TRUE);

    c = MakeControl(hWnd, L"BUTTON", L"浏览…",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        S(510), S(45), S(94), S(28), IDC_BTN_BROWSE);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    g_hArchLabel = MakeControl(hWnd, L"STATIC", L"检测结果：尚未选择",
        WS_CHILD | WS_VISIBLE, S(16), S(80), S(588), S(20), IDC_STATIC_ARCH);
    SendMessageW(g_hArchLabel, WM_SETFONT, (WPARAM)uiFont, TRUE);

    c = MakeControl(hWnd, L"BUTTON", L"安装",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        S(16), S(112), S(150), S(36), IDC_BTN_INSTALL);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    c = MakeControl(hWnd, L"BUTTON", L"卸载 / 还原",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        S(176), S(112), S(150), S(36), IDC_BTN_UNINSTALL);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    c = MakeControl(hWnd, L"BUTTON", L"退出",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        S(510), S(112), S(94), S(36), IDC_BTN_EXIT);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    c = MakeControl(hWnd, L"STATIC", L"日志：",
        WS_CHILD | WS_VISIBLE, S(16), S(158), S(588), S(18), IDC_STATIC_LOGLABEL);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    g_hLog = MakeControl(hWnd, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        S(16), S(178), S(588), S(200), IDC_EDIT_LOG);
    SendMessageW(g_hLog, WM_SETFONT, (WPARAM)uiFont, TRUE);

    g_hHint = MakeControl(hWnd, L"STATIC",
        L"提示：安装会备份游戏目录原有的 winmm.dll；请先关闭游戏。\n"
        L"联网 / 反作弊游戏请谨慎使用；卸载 / 还原可自动恢复游戏目录。",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        S(16), S(386), S(588), S(50), IDC_STATIC_HINT);
    SendMessageW(g_hHint, WM_SETFONT, (WPARAM)uiFont, TRUE);

    LayoutControls(hWnd);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        CreateControls(hWnd);
        Log(L"CTFontHook 安装器已就绪。");
        Log(L"请先选择游戏的启动程序(.exe)。");
        return 0;

    case WM_SIZE:
        if (wp != SIZE_MINIMIZED)
            LayoutControls(hWnd);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        RECT rc = { 0, 0, S(kClientW), S(kClientH) };
        AdjustWindowRectEx(&rc, (DWORD)GetWindowLongW(hWnd, GWL_STYLE), FALSE,
            (DWORD)GetWindowLongW(hWnd, GWL_EXSTYLE));
        mmi->ptMinTrackSize.x = rc.right - rc.left;
        mmi->ptMinTrackSize.y = rc.bottom - rc.top;
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BTN_BROWSE:    OnBrowse(hWnd); return 0;
        case IDC_BTN_INSTALL:   DoInstall(); return 0;
        case IDC_BTN_UNINSTALL: DoUninstall(); return 0;
        case IDC_BTN_EXIT:      DestroyWindow(hWnd); return 0;
        default: return 0;
        }

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        if (g_uiFont) { DeleteObject(g_uiFont); g_uiFont = nullptr; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    // 与系统 DPI 一致即可，避免高分屏下界面模糊；布局尺寸随 DPI 缩放。
    SetProcessDPIAware();
    HDC screenDc = GetDC(nullptr);
    if (screenDc) {
        int dpi = GetDeviceCaps(screenDc, LOGPIXELSX);
        if (dpi > 0) g_dpi = dpi;
        ReleaseDC(nullptr, screenDc);
    }

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kWndClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassExW(&wc);

    const DWORD kStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
        WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
    RECT rc = { 0, 0, S(kClientW), S(kClientH) };
    AdjustWindowRectEx(&rc, kStyle, FALSE, 0);

    HWND hWnd = CreateWindowExW(0, kWndClass, kAppTitle, kStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return 1;

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
