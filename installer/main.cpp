// CTFontHook 安装器
//
// 面向「完全不懂技术」的玩家：双击运行 → 选择游戏的启动程序(.exe) → 自动识别 32/64 位
// → 备份游戏目录里原有的 winmm.dll / FontHook.ini → 放入对应架构的 winmm.dll。
// 卸载/还原：读取安装时写入的清单，自动恢复或删除。
//
// 设计约束：
//  - 只做「安装 / 卸载」，不做启动游戏、不做导入预判（后续版本再增强）。
//  - 清单需管理员权限（RequireAdministrator），装入 Program Files 也能成功。
//  - 静态链接 CRT，可单文件分发；优先从自身同级目录读取 32bit\ / 64bit\winmm.dll，
//    没有同级目录时回退到自身内嵌的 DLL（单文件安装包场景）。
//
// 注意：本文件保存为 UTF-8，工程使用 /utf-8 编译，因此源码中的中文可安全用于宽字符字面量。

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

#include "resource.h"

namespace {

const wchar_t* kAppTitle = L"时汐字体器 · 安装器";
const wchar_t* kWndClass = L"CTFontHookSetupWnd";
const wchar_t* kBakDirName = L".CTFontHook";
const wchar_t* kManifestName = L"manifest.ini";

HWND g_hPathEdit = nullptr;
HWND g_hArchLabel = nullptr;
HWND g_hLog = nullptr;
HWND g_hHint = nullptr;
HWND g_hList = nullptr;
HWND g_hListLabel = nullptr;

std::wstring g_selectedExe;

// 界面尺寸以 96 DPI 为设计单位，实际像素经 S() 按系统 DPI 放大，
// 避免高分屏上字体放大而固定像素布局导致提示文字被裁剪。
const int kClientW = 780;
const int kClientH = 664;

int g_dpi = 96;
int S(int px) { return MulDiv(px, g_dpi, 96); }
HFONT g_uiFont = nullptr;
HFONT g_titleFont = nullptr;
HBRUSH g_bgBrush = nullptr;      // 窗口底色刷（静态文本背景）
HBRUSH g_whiteBrush = nullptr;   // 卡片 / 列表 / 输入框底色刷
HICON g_appIcon = nullptr;       // 窗口图标（大）
HICON g_appIconSm = nullptr;     // 窗口图标（小）

// ---------------------------------------------------------------------------
// 浅色主题调色板：与「字体选择器」深色主题相反——同一套设计语言，明暗对调。
// ---------------------------------------------------------------------------
const COLORREF COL_WINDOW_BG  = RGB(0xF4, 0xF6, 0xFA); // 窗口底
const COLORREF COL_PANEL_BG   = RGB(0xFF, 0xFF, 0xFF); // 卡片 / 列表 / 输入框底
const COLORREF COL_HEADER_BG  = RGB(0xEE, 0xF3, 0xFA); // 列表表头底
const COLORREF COL_BORDER     = RGB(0xD5, 0xDC, 0xE6); // 描边
const COLORREF COL_ACCENT     = RGB(0x3D, 0x6F, 0xC4); // 主强调（安装）
const COLORREF COL_ACCENT_HOV = RGB(0x33, 0x60, 0xAE);
const COLORREF COL_ACCENT_PSD = RGB(0x2A, 0x50, 0x93);
const COLORREF COL_WARN       = RGB(0xC4, 0x52, 0x3D); // 警示（卸载 / 还原）
const COLORREF COL_WARN_HOV   = RGB(0xAC, 0x44, 0x31);
const COLORREF COL_WARN_PSD   = RGB(0x93, 0x39, 0x29);
const COLORREF COL_TEXT       = RGB(0x1F, 0x27, 0x33); // 主文字
const COLORREF COL_TEXT_DIM   = RGB(0x5F, 0x6B, 0x7D); // 次文字
const COLORREF COL_TEXT_HINT  = RGB(0x8A, 0x93, 0xA3); // 提示
const COLORREF COL_BTN_BG     = RGB(0xEC, 0xF1, 0xF8); // 次级按钮底
const COLORREF COL_BTN_HOV    = RGB(0xDF, 0xE7, 0xF2);
const COLORREF COL_BTN_PSD    = RGB(0xD2, 0xDD, 0xEC);
const COLORREF COL_OK         = RGB(0x2E, 0x8B, 0x62); // 「已安装」状态
const COLORREF COL_LIST_SEL   = RGB(0xDC, 0xE8, 0xFA); // 列表选中行底

// 版式基准（96 DPI 设计单位，实际像素经 S() 放大）
const int LY_ROW1_Y      = 70;   // 「游戏程序」行
const int LY_INSTALL_Y   = 110;  // 「安装到游戏」按钮行
const int LY_LISTLABEL_Y = 154;  // 「已登记的游戏」标签
const int LY_LIST_TOP    = 176;  // 列表
const int LY_LIST_H      = 224;
const int LY_BTN_Y       = 410;  // 列表下方按钮行
const int LY_BTN_H       = 34;
const int LY_LOGLABEL_Y  = 456;  // 「操作日志」标签

// 自绘按钮悬停状态（索引 = 控件 ID；IDC_* 均 < 2048）。
bool g_btnHot[2048] = {};
bool g_btnTrack[2048] = {};

void FillRoundRectDc(HDC dc, const RECT& r, int rad, COLORREF fill, COLORREF border) {
    HBRUSH br = CreateSolidBrush(fill);
    HPEN pn = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, br);
    HGDIOBJ op = SelectObject(dc, pn);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad * 2, rad * 2);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(br);
    DeleteObject(pn);
}

// 在子控件（列表 / 日志）外围画一圈圆角卡片；子控件白底与卡片同色，只剩描边可见。
void DrawCardAround(HDC dc, HWND child) {
    if (!child || !IsWindowVisible(child)) return;
    HWND parent = GetParent(child);
    RECT r = {};
    GetWindowRect(child, &r);
    POINT a = { r.left, r.top };
    POINT b = { r.right, r.bottom };
    ScreenToClient(parent, &a);
    ScreenToClient(parent, &b);
    RECT card = { a.x - S(2), a.y - S(2), b.x + S(2), b.y + S(2) };
    FillRoundRectDc(dc, card, S(8), COL_PANEL_BG, COL_BORDER);
}

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
// 中央清单（多游戏管理）：%APPDATA%\CTFontHook\games.ini（UTF-16LE + BOM）
// ---------------------------------------------------------------------------

struct GameRecord {
    std::wstring name;
    std::wstring exe;
    std::wstring dir;
    std::wstring arch;         // "32bit" / "64bit"
    std::wstring installedAt;
};

bool PathEqualNoCase(const std::wstring& a, const std::wstring& b) {
    return lstrcmpiW(a.c_str(), b.c_str()) == 0;
}

std::wstring FileNameOf(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? path : path.substr(pos + 1);
}

std::wstring StripExt(const std::wstring& name) {
    size_t pos = name.find_last_of(L'.');
    return (pos == std::wstring::npos || pos == 0) ? name : name.substr(0, pos);
}

std::wstring AppDataDir() {
    wchar_t buf[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return std::wstring();
    std::wstring d(buf);
    while (!d.empty() && d.back() == L'\\') d.pop_back();
    return d + L"\\CTFontHook";
}

std::wstring GamesIniPath() { return AppDataDir() + L"\\games.ini"; }

bool ReadFileBytes(const std::wstring& path, std::vector<char>* out) {
    out->clear();
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD size = GetFileSize(h, nullptr);
    if (size == 0 || size == INVALID_FILE_SIZE) { CloseHandle(h); return false; }
    out->resize(size);
    DWORD rd = 0;
    BOOL ok = ReadFile(h, out->data(), size, &rd, nullptr);
    CloseHandle(h);
    if (!ok) { out->clear(); return false; }
    out->resize(rd);
    return true;
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (len <= 0) return std::wstring();
    std::vector<wchar_t> buf(len + 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), buf.data(), len);
    return std::wstring(buf.data(), len);
}

// 读取某个游戏目录下 DLL 写出的 FontHook.ini（UTF-8 + BOM）里的 FontNameW。
std::wstring ReadGameFont(const std::wstring& dir) {
    std::vector<char> bytes;
    if (!ReadFileBytes(dir + L"\\FontHook.ini", &bytes)) return std::wstring();
    std::string content(bytes.begin(), bytes.end());
    if (content.size() >= 3 &&
        (unsigned char)content[0] == 0xEF && (unsigned char)content[1] == 0xBB &&
        (unsigned char)content[2] == 0xBF)
        content = content.substr(3);

    const std::string key = "FontNameW=";
    size_t pos = 0;
    while (pos <= content.size()) {
        size_t eol = content.find('\n', pos);
        std::string line = (eol == std::string::npos)
            ? content.substr(pos) : content.substr(pos, eol - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.compare(0, key.size(), key) == 0)
            return Utf8ToWide(line.substr(key.size()));
        if (eol == std::string::npos) break;
        pos = eol + 1;
    }
    return std::wstring();
}

bool IsGameInstalled(const std::wstring& dir) {
    return FileExists(dir + L"\\" + kBakDirName + L"\\" + kManifestName);
}

bool LoadGames(std::vector<GameRecord>* out) {
    out->clear();
    std::vector<char> bytes;
    if (!ReadFileBytes(GamesIniPath(), &bytes)) return false;
    if (bytes.size() < sizeof(wchar_t) * 2) return false;

    const wchar_t* p = reinterpret_cast<const wchar_t*>(bytes.data());
    size_t n = bytes.size() / sizeof(wchar_t);
    if (n && p[0] == 0xFEFF) { ++p; --n; }
    std::wstring text(p, n);

    GameRecord cur;
    bool has = false;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t eol = text.find(L'\n', pos);
        std::wstring line = (eol == std::wstring::npos)
            ? text.substr(pos) : text.substr(pos, eol - pos);
        pos = (eol == std::wstring::npos) ? text.size() + 1 : eol + 1;
        while (!line.empty() && (line.back() == L'\r' || line.back() == L'\n'))
            line.pop_back();
        if (line.empty()) continue;
        if (line[0] == L'[') {
            if (has) out->push_back(cur);
            cur = GameRecord();
            has = true;
            continue;
        }
        if (!has) continue;
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring k = line.substr(0, eq);
        std::wstring v = line.substr(eq + 1);
        if (k == L"Name")             cur.name = v;
        else if (k == L"Exe")         cur.exe = v;
        else if (k == L"Dir")         cur.dir = v;
        else if (k == L"DllArch")     cur.arch = v;
        else if (k == L"InstalledAt") cur.installedAt = v;
    }
    if (has) out->push_back(cur);
    return true;
}

bool SaveGames(const std::vector<GameRecord>& games) {
    std::wstring dir = AppDataDir();
    if (dir.empty() || !EnsureDir(dir)) return false;

    std::wstring s;
    for (size_t i = 0; i < games.size(); ++i) {
        const GameRecord& g = games[i];
        s += L"[Game";
        s += std::to_wstring(i + 1);
        s += L"]\r\n";
        s += L"Name=" + g.name + L"\r\n";
        s += L"Exe=" + g.exe + L"\r\n";
        s += L"Dir=" + g.dir + L"\r\n";
        s += L"DllArch=" + g.arch + L"\r\n";
        s += L"InstalledAt=" + g.installedAt + L"\r\n\r\n";
    }

    HANDLE h = CreateFileW(GamesIniPath().c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD w = 0;
    const wchar_t bom = 0xFEFF;
    WriteFile(h, &bom, sizeof(bom), &w, nullptr);
    WriteFile(h, s.c_str(), (DWORD)(s.size() * sizeof(wchar_t)), &w, nullptr);
    CloseHandle(h);
    return true;
}

// 记录或更新一个游戏（按目录去重）。
bool UpsertGame(const std::wstring& exe, const std::wstring& arch) {
    std::wstring dir = DirNameOf(exe);
    if (dir.empty()) return false;

    std::vector<GameRecord> games;
    LoadGames(&games);

    bool found = false;
    for (GameRecord& g : games) {
        if (!PathEqualNoCase(g.dir, dir)) continue;
        g.name = StripExt(FileNameOf(exe));
        g.exe = exe;
        g.arch = arch;
        g.installedAt = NowString();
        found = true;
        break;
    }
    if (!found) {
        GameRecord g;
        g.name = StripExt(FileNameOf(exe));
        g.exe = exe;
        g.dir = dir;
        g.arch = arch;
        g.installedAt = NowString();
        games.push_back(g);
    }
    return SaveGames(games);
}

bool RemoveGameRecord(const std::wstring& dir) {
    std::vector<GameRecord> games;
    LoadGames(&games);
    std::vector<GameRecord> keep;
    for (const GameRecord& g : games)
        if (!PathEqualNoCase(g.dir, dir)) keep.push_back(g);
    return SaveGames(keep);
}

// ---------------------------------------------------------------------------
// 内嵌 DLL 载荷（单文件安装包）
// ---------------------------------------------------------------------------

// 自身内嵌的对应架构 winmm.dll 资源；不存在返回 nullptr。
HRSRC EmbeddedDllResource(WORD machine) {
    WORD id = (machine == IMAGE_FILE_MACHINE_I386) ? IDR_PAYLOAD_DLL32 : IDR_PAYLOAD_DLL64;
    return FindResourceW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(id), RT_RCDATA);
}

// 把内嵌的对应架构 winmm.dll 释放到临时目录，返回释放出的路径；失败返回空串。
// 单文件安装包没有同级 32bit\ / 64bit\ 目录，就靠这里取出 DLL。
std::wstring ExtractEmbeddedDll(WORD machine) {
    HMODULE mod = GetModuleHandleW(nullptr);
    HRSRC res = EmbeddedDllResource(machine);
    if (!res) return std::wstring();
    DWORD size = SizeofResource(mod, res);
    if (size == 0) return std::wstring();
    HGLOBAL gl = LoadResource(mod, res);
    if (!gl) return std::wstring();
    const void* data = LockResource(gl);
    if (!data) return std::wstring();

    wchar_t tmp[MAX_PATH] = {};
    DWORD n = GetTempPathW(MAX_PATH, tmp);
    if (n == 0 || n >= MAX_PATH) return std::wstring();
    std::wstring dir = std::wstring(tmp) + L"CTFontHook_" +
                       std::to_wstring(GetCurrentProcessId());
    if (!EnsureDir(dir)) return std::wstring();

    std::wstring path = dir + L"\\winmm.dll";
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return std::wstring();
    DWORD written = 0;
    BOOL ok = WriteFile(h, data, size, &written, nullptr);
    CloseHandle(h);
    if (!ok || written != size) {
        DeleteFileW(path.c_str());
        return std::wstring();
    }
    return path;
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

    // 优先用发布包内同级目录的 DLL；单文件安装包没有这些目录，改用内嵌 DLL（稍后释放）。
    std::wstring siblingDll = ExeDir() + L"\\" + ArchSubDir(m) + L"\\winmm.dll";
    bool useEmbedded = !FileExists(siblingDll);
    if (useEmbedded && !EmbeddedDllResource(m)) {
        Log(std::wstring(L"[错误] 发布包中缺少 ") + ArchSubDir(m) +
            L"\\winmm.dll，且安装器内也未内嵌该 DLL。");
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

    std::wstring ourDll = siblingDll;
    if (useEmbedded) {
        ourDll = ExtractEmbeddedDll(m);
        if (ourDll.empty()) {
            Log(L"[错误] 释放内嵌 winmm.dll 失败（临时目录不可写？）。");
            return false;
        }
    }

    if (!CopyFileW(ourDll.c_str(), dstDll.c_str(), FALSE)) {
        Log(L"[错误] 复制 winmm.dll 失败（可能被占用或无写入权限）。");
        if (useEmbedded) DeleteFileW(ourDll.c_str());
        return false;
    }
    if (useEmbedded) {
        DeleteFileW(ourDll.c_str());
        RemoveDirectoryW(DirNameOf(ourDll).c_str());
    }

    WriteManifest(manifest, m, hadWinmm, hadIni);
    UpsertGame(g_selectedExe, ArchSubDir(m));

    Log(L"[完成] 安装成功！");
    Log(std::wstring(L"　　游戏目录：") + gameDir);
    Log(std::wstring(L"　　使用架构：") + ArchLabel(m) + L"（" +
        ArchSubDir(m) + L"\\winmm.dll）");
    if (hadWinmm) Log(L"　　原有 winmm.dll 已备份到 " + std::wstring(kBakDirName) + L"\\backup。");
    Log(L"　　启动游戏后按 Insert 呼出字体选择器（被占用时试 Pause / Scroll Lock）。");
    return true;
}

bool DoUninstallForDir(const std::wstring& gameDir, const std::wstring& displayName) {
    if (gameDir.empty()) {
        Log(L"[错误] 无法确定游戏目录。");
        return false;
    }

    std::wstring bakDir = gameDir + L"\\" + kBakDirName;
    std::wstring manifest = bakDir + L"\\" + kManifestName;

    if (!FileExists(manifest)) {
        Log(L"[提示] 「" + displayName + L"」没有 CTFontHook 安装记录，无法还原。");
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

    Log(L"[完成] 「" + displayName + L"」已卸载，游戏目录已恢复。");
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

// ---------------------------------------------------------------------------
// 多游戏列表
// ---------------------------------------------------------------------------

void RefreshGameList() {
    if (g_hList)
        ListView_DeleteAllItems(g_hList);

    std::vector<GameRecord> games;
    LoadGames(&games);

    for (size_t i = 0; g_hList && i < games.size(); ++i) {
        const GameRecord& g = games[i];
        std::wstring status = IsGameInstalled(g.dir) ? L"已安装" : L"未安装";
        std::wstring font = ReadGameFont(g.dir);
        if (font.empty()) font = L"（默认字体）";

        LVITEMW it = {};
        it.mask = LVIF_TEXT;
        it.iItem = (int)i;
        it.pszText = const_cast<wchar_t*>(g.name.c_str());
        int row = ListView_InsertItem(g_hList, &it);
        if (row < 0) continue;
        ListView_SetItemText(g_hList, row, 1, const_cast<wchar_t*>(g.dir.c_str()));
        ListView_SetItemText(g_hList, row, 2, const_cast<wchar_t*>(g.arch.c_str()));
        ListView_SetItemText(g_hList, row, 3, const_cast<wchar_t*>(font.c_str()));
        ListView_SetItemText(g_hList, row, 4, const_cast<wchar_t*>(status.c_str()));
    }

    if (g_hListLabel) {
        SetWindowTextW(g_hListLabel, games.empty()
            ? L"已登记的游戏：（暂无记录：选好游戏点「安装」后会自动出现在这里）"
            : L"已登记的游戏：");
    }
}

bool GetSelectedGameRecord(GameRecord* out) {
    if (!g_hList) return false;
    int row = ListView_GetNextItem(g_hList, -1, LVNI_SELECTED);
    if (row < 0) return false;
    std::vector<GameRecord> games;
    LoadGames(&games);
    if (row >= (int)games.size()) return false;
    *out = games[row];
    return true;
}

void OnUninstallSelected() {
    GameRecord g;
    if (!GetSelectedGameRecord(&g)) {
        Log(L"[提示] 请先在列表中选中一个游戏。");
        return;
    }
    if (DoUninstallForDir(g.dir, g.name))
        RefreshGameList();
}

void OnOpenSelectedDir() {
    GameRecord g;
    if (!GetSelectedGameRecord(&g)) {
        Log(L"[提示] 请先在列表中选中一个游戏。");
        return;
    }
    if (!DirExists(g.dir)) {
        Log(L"[错误] 目录不存在：" + g.dir);
        return;
    }
    ShellExecuteW(nullptr, L"open", g.dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    Log(L"已打开目录：" + g.dir);
}

void OnRemoveSelectedRecord() {
    GameRecord g;
    if (!GetSelectedGameRecord(&g)) {
        Log(L"[提示] 请先在列表中选中一个游戏。");
        return;
    }
    if (IsGameInstalled(g.dir)) {
        Log(L"[提示] 「" + g.name + L"」仍处于已安装状态，请先「卸载 / 还原」再移除记录。");
        return;
    }
    if (RemoveGameRecord(g.dir)) {
        Log(L"[完成] 已从列表移除记录：" + g.name);
        RefreshGameList();
    } else {
        Log(L"[错误] 写入记录失败。");
    }
}

// 手动登记一个已装好补丁的游戏（没有经过本工具安装记录的老用户）。
void OnAddGameManually(HWND hWnd) {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"游戏程序 (*.exe)\0*.exe\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrTitle = L"请选择要登记的游戏启动程序(.exe)";
    if (!GetOpenFileNameW(&ofn)) return;

    std::wstring exe = file;
    std::wstring dir = DirNameOf(exe);
    if (dir.empty()) {
        Log(L"[错误] 无法确定游戏目录。");
        return;
    }

    if (!FileExists(dir + L"\\winmm.dll") && !IsGameInstalled(dir))
        Log(L"[提示] 该目录下没有 winmm.dll 或 CTFontHook 记录，可能尚未安装字体补丁。");

    WORD m = ReadPeMachine(exe);
    std::wstring arch = ArchSupported(m) ? ArchSubDir(m) : std::wstring();
    if (arch.empty()) {
        // 退一步：用目录里现有 winmm.dll 的架构作参照。
        WORD dm = ReadPeMachine(dir + L"\\winmm.dll");
        arch = ArchSupported(dm) ? ArchSubDir(dm) : L"未知";
    }

    if (UpsertGame(exe, arch)) {
        Log(L"[完成] 已登记游戏：" + StripExt(FileNameOf(exe)) + L"（" + dir + L"）");
        RefreshGameList();
    } else {
        Log(L"[错误] 写入记录失败。");
    }
}

HWND MakeControl(HWND parent, const wchar_t* cls, const wchar_t* text,
                 DWORD style, int x, int y, int w, int h, int id,
                 DWORD exStyle = 0) {
    return CreateWindowExW(exStyle, cls, text, style, x, y, w, h,
        parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
}

// 自绘按钮的子类化过程：只负责维护悬停高亮（按下态由 WM_DRAWITEM 的 ODS_SELECTED 提供）。
LRESULT CALLBACK OwnerButtonProc(HWND h, UINT msg, WPARAM wp, LPARAM lp,
                                 UINT_PTR, DWORD_PTR ref) {
    int id = (int)ref;
    if (id >= 0 && id < 2048) {
        if (msg == WM_MOUSEMOVE) {
            if (!g_btnHot[id]) {
                if (!g_btnTrack[id]) {
                    TRACKMOUSEEVENT t = { sizeof(t), TME_LEAVE, h, 0 };
                    TrackMouseEvent(&t);
                    g_btnTrack[id] = true;
                }
                g_btnHot[id] = true;
                InvalidateRect(h, nullptr, TRUE);
            }
        } else if (msg == WM_MOUSELEAVE) {
            g_btnHot[id] = false;
            g_btnTrack[id] = false;
            InvalidateRect(h, nullptr, TRUE);
        }
    }
    return DefSubclassProc(h, msg, wp, lp);
}

// 自绘按钮：主操作（安装）走主强调色，卸载 / 还原走警示色，其余为浅色次级按钮。
void DrawOwnerButton(const DRAWITEMSTRUCT* od) {
    HDC dc = od->hDC;
    RECT rc = od->rcItem;
    int id = (int)od->CtlID;
    bool pressed = (od->itemState & ODS_SELECTED) != 0;
    bool disabled = (od->itemState & ODS_DISABLED) != 0;
    bool hot = (!pressed && !disabled && id >= 0 && id < 2048) ? g_btnHot[id] : false;

    COLORREF fill = COL_BTN_BG, border = COL_BORDER, textCol = COL_TEXT;
    if (id == IDC_BTN_INSTALL) {
        fill = pressed ? COL_ACCENT_PSD : (hot ? COL_ACCENT_HOV : COL_ACCENT);
        border = fill;
        textCol = RGB(0xFF, 0xFF, 0xFF);
    } else if (id == IDC_BTN_UNINSTALL) {
        fill = pressed ? COL_WARN_PSD : (hot ? COL_WARN_HOV : COL_WARN);
        border = fill;
        textCol = RGB(0xFF, 0xFF, 0xFF);
    } else {
        fill = pressed ? COL_BTN_PSD : (hot ? COL_BTN_HOV : COL_BTN_BG);
    }
    if (disabled) { fill = COL_BTN_BG; border = COL_BORDER; textCol = COL_TEXT_HINT; }

    FillRoundRectDc(dc, rc, S(7), fill, border);

    wchar_t text[128] = {};
    GetWindowTextW(od->hwndItem, text, 128);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, textCol);
    HGDIOBJ of = SelectObject(dc, g_uiFont);
    RECT tr = rc;
    if (pressed) OffsetRect(&tr, 0, S(1));
    DrawTextW(dc, text, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if (od->itemState & ODS_FOCUS) {
        RECT fr = rc;
        InflateRect(&fr, -S(4), -S(4));
        DrawFocusRect(dc, &fr);
    }
    SelectObject(dc, of);
}

void LayoutControls(HWND hWnd) {
    RECT rc = {};
    GetClientRect(hWnd, &rc);
    const int padL = S(16);
    const int padR = S(16);
    const int contentW = rc.right - padL - padR;

    HWND title = GetDlgItem(hWnd, IDC_STATIC_TITLE);
    if (title)
        MoveWindow(title, S(56), S(12), rc.right - S(56) - padR, S(24), TRUE);

    // 「游戏程序」行：输入框随窗口拉伸，浏览按钮贴右。
    if (g_hPathEdit)
        MoveWindow(g_hPathEdit, S(100), S(LY_ROW1_Y),
            rc.right - S(100) - padR - S(98), S(28), TRUE);
    HWND browse = GetDlgItem(hWnd, IDC_BTN_BROWSE);
    if (browse)
        MoveWindow(browse, rc.right - padR - S(90), S(LY_ROW1_Y - 1), S(90), S(30), TRUE);

    if (g_hArchLabel)
        MoveWindow(g_hArchLabel, S(148), S(LY_INSTALL_Y + 6),
            rc.right - S(148) - padR, S(22), TRUE);

    if (g_hListLabel)
        MoveWindow(g_hListLabel, padL, S(LY_LISTLABEL_Y), contentW, S(18), TRUE);
    if (g_hList)
        MoveWindow(g_hList, padL, S(LY_LIST_TOP), contentW, S(LY_LIST_H), TRUE);

    HWND exitBtn = GetDlgItem(hWnd, IDC_BTN_EXIT);
    if (exitBtn)
        MoveWindow(exitBtn, rc.right - padR - S(90), S(LY_BTN_Y), S(90), S(LY_BTN_H), TRUE);
    HWND refresh = GetDlgItem(hWnd, IDC_BTN_REFRESH);
    if (refresh)
        MoveWindow(refresh, rc.right - padR - S(90) - S(10) - S(84), S(LY_BTN_Y),
            S(84), S(LY_BTN_H), TRUE);

    HWND logLabel = GetDlgItem(hWnd, IDC_STATIC_LOGLABEL);
    if (logLabel)
        MoveWindow(logLabel, padL, S(LY_LOGLABEL_Y), contentW, S(18), TRUE);

    const int logTop = S(LY_LOGLABEL_Y + 20);
    const int hintH = S(40);
    const int bottomPad = S(14);
    const int gap = S(8);
    int logH = rc.bottom - logTop - hintH - bottomPad - gap;
    if (logH < S(90)) logH = S(90);
    if (g_hLog)
        MoveWindow(g_hLog, padL, logTop, contentW, logH, TRUE);
    if (g_hHint) {
        int hintTop = logTop + logH + gap;
        int h = rc.bottom - hintTop - bottomPad;
        if (h < S(30)) h = S(30);
        MoveWindow(g_hHint, padL, hintTop, contentW, h, TRUE);
    }
}

void CreateControls(HWND hWnd) {
    g_uiFont = CreateFontW(-S(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    g_titleFont = CreateFontW(-S(17), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    g_bgBrush = CreateSolidBrush(COL_WINDOW_BG);
    g_whiteBrush = CreateSolidBrush(COL_PANEL_BG);
    HFONT uiFont = g_uiFont;

    HWND c = nullptr;

    // 应用名标题；副标题在 WM_PAINT 中绘制，图标在窗口左上角。
    c = MakeControl(hWnd, L"STATIC", L"时汐字体器 · 安装器",
        WS_CHILD | WS_VISIBLE | SS_LEFT, S(56), S(12), S(700), S(24), IDC_STATIC_TITLE);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_titleFont, TRUE);

    c = MakeControl(hWnd, L"STATIC", L"游戏程序：",
        WS_CHILD | WS_VISIBLE, S(16), S(LY_ROW1_Y + 4), S(80), S(20), IDC_STATIC_PATHLABEL);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    g_hPathEdit = MakeControl(hWnd, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_READONLY,
        S(100), S(LY_ROW1_Y), S(540), S(28), IDC_EDIT_PATH);
    SendMessageW(g_hPathEdit, WM_SETFONT, (WPARAM)uiFont, TRUE);

    MakeControl(hWnd, L"BUTTON", L"浏览…",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        S(654), S(LY_ROW1_Y - 1), S(90), S(30), IDC_BTN_BROWSE);

    MakeControl(hWnd, L"BUTTON", L"安装到游戏",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | BS_DEFPUSHBUTTON,
        S(16), S(LY_INSTALL_Y), S(126), S(LY_BTN_H), IDC_BTN_INSTALL);

    g_hArchLabel = MakeControl(hWnd, L"STATIC", L"检测结果：尚未选择",
        WS_CHILD | WS_VISIBLE, S(148), S(LY_INSTALL_Y + 6), S(600), S(22), IDC_STATIC_ARCH);
    SendMessageW(g_hArchLabel, WM_SETFONT, (WPARAM)uiFont, TRUE);

    g_hListLabel = MakeControl(hWnd, L"STATIC", L"已登记的游戏：",
        WS_CHILD | WS_VISIBLE, S(16), S(LY_LISTLABEL_Y), S(700), S(18), IDC_STATIC_LISTLABEL);
    SendMessageW(g_hListLabel, WM_SETFONT, (WPARAM)uiFont, TRUE);

    g_hList = MakeControl(hWnd, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP |
        LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        S(16), S(LY_LIST_TOP), S(740), S(LY_LIST_H), IDC_LIST_GAMES);
    SendMessageW(g_hList, WM_SETFONT, (WPARAM)uiFont, TRUE);
    ListView_SetExtendedListViewStyle(g_hList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);
    ListView_SetBkColor(g_hList, COL_PANEL_BG);
    ListView_SetTextBkColor(g_hList, COL_PANEL_BG);
    ListView_SetTextColor(g_hList, COL_TEXT);
    {
        struct ColDef { const wchar_t* text; int w; };
        const ColDef cols[] = {
            { L"游戏", 150 }, { L"目录", 260 }, { L"架构", 60 },
            { L"当前字体", 170 }, { L"状态", 80 }
        };
        for (int i = 0; i < 5; ++i) {
            LVCOLUMNW col = {};
            col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            col.iSubItem = i;
            col.pszText = const_cast<wchar_t*>(cols[i].text);
            col.cx = S(cols[i].w);
            ListView_InsertColumn(g_hList, i, &col);
        }
    }

    MakeControl(hWnd, L"BUTTON", L"卸载 / 还原",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        S(16), S(LY_BTN_Y), S(112), S(LY_BTN_H), IDC_BTN_UNINSTALL);
    MakeControl(hWnd, L"BUTTON", L"打开目录",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        S(136), S(LY_BTN_Y), S(100), S(LY_BTN_H), IDC_BTN_OPEN_DIR);
    MakeControl(hWnd, L"BUTTON", L"移除记录",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        S(244), S(LY_BTN_Y), S(100), S(LY_BTN_H), IDC_BTN_REMOVE);
    MakeControl(hWnd, L"BUTTON", L"手动登记游戏…",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        S(352), S(LY_BTN_Y), S(132), S(LY_BTN_H), IDC_BTN_ADD_GAME);
    MakeControl(hWnd, L"BUTTON", L"刷新",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        S(492), S(LY_BTN_Y), S(84), S(LY_BTN_H), IDC_BTN_REFRESH);
    MakeControl(hWnd, L"BUTTON", L"退出",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        S(674), S(LY_BTN_Y), S(90), S(LY_BTN_H), IDC_BTN_EXIT);

    // 按钮统一子类化，以支持鼠标悬停高亮。
    const int kBtnIds[] = { IDC_BTN_BROWSE, IDC_BTN_INSTALL, IDC_BTN_UNINSTALL,
                            IDC_BTN_OPEN_DIR, IDC_BTN_REMOVE, IDC_BTN_ADD_GAME,
                            IDC_BTN_REFRESH, IDC_BTN_EXIT };
    for (int id : kBtnIds) {
        HWND b = GetDlgItem(hWnd, id);
        if (b) SetWindowSubclass(b, OwnerButtonProc, 1, (DWORD_PTR)id);
    }

    c = MakeControl(hWnd, L"STATIC", L"操作日志：",
        WS_CHILD | WS_VISIBLE, S(16), S(LY_LOGLABEL_Y), S(700), S(18), IDC_STATIC_LOGLABEL);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    g_hLog = MakeControl(hWnd, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL |
        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        S(16), S(LY_LOGLABEL_Y + 20), S(740), S(110), IDC_EDIT_LOG);
    SendMessageW(g_hLog, WM_SETFONT, (WPARAM)uiFont, TRUE);

    g_hHint = MakeControl(hWnd, L"STATIC",
        L"提示：安装会备份游戏目录原有的 winmm.dll；请先关闭游戏。\n"
        L"列表会记住每个游戏当前应用的字体；联网 / 反作弊游戏请谨慎使用。",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        S(16), S(600), S(740), S(40), IDC_STATIC_HINT);
    SendMessageW(g_hHint, WM_SETFONT, (WPARAM)uiFont, TRUE);

    LayoutControls(hWnd);
}

std::wstring ListSubItemText(int row, int sub) {
    if (!g_hList) return std::wstring();
    wchar_t buf[512] = {};
    ListView_GetItemText(g_hList, row, sub, buf, 512);
    return buf;
}

bool GetGameRecordAt(int row, GameRecord* out) {
    std::vector<GameRecord> games;
    LoadGames(&games);
    if (row < 0 || row >= (int)games.size()) return false;
    *out = games[row];
    return true;
}

// 列表行着色：「已安装」绿色、其余灰色；选中行统一用浅蓝底。
LRESULT HandleListCustomDraw(NMLVCUSTOMDRAW* cd) {
    switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT:
        return CDRF_NOTIFYSUBITEMDRAW;
    case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
        bool sel = (cd->nmcd.uItemState & CDIS_SELECTED) != 0;
        cd->clrTextBk = sel ? COL_LIST_SEL : COL_PANEL_BG;
        if (cd->iSubItem == 4) {
            std::wstring st = ListSubItemText((int)cd->nmcd.dwItemSpec, 4);
            cd->clrText = (st == L"已安装") ? (sel ? COL_TEXT : COL_OK) : COL_TEXT_DIM;
        } else {
            cd->clrText = COL_TEXT;
        }
        return CDRF_DODEFAULT;
    }
    }
    return CDRF_DODEFAULT;
}

// 表头着色：浅色底 + 灰字，与浅色主题统一。
LRESULT HandleHeaderCustomDraw(NMLVCUSTOMDRAW* cd) {
    if (cd->nmcd.dwDrawStage == CDDS_PREPAINT)
        return CDRF_NOTIFYITEMDRAW;
    if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
        cd->clrTextBk = COL_HEADER_BG;
        cd->clrText = COL_TEXT_DIM;
        return CDRF_DODEFAULT;
    }
    return CDRF_DODEFAULT;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        CreateControls(hWnd);
        Log(L"时汐字体器 · 安装器已就绪。");
        Log(L"请选择游戏的启动程序(.exe)并点击「安装到游戏」；已安装的游戏会显示在下方列表。");
        RefreshGameList();
        return 0;

    case WM_SIZE:
        if (wp != SIZE_MINIMIZED)
            LayoutControls(hWnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;  // 背景统一在 WM_PAINT 中绘制，避免闪烁

    case WM_PAINT: {
        PAINTSTRUCT ps = {};
        HDC dc = BeginPaint(hWnd, &ps);
        RECT rc = {};
        GetClientRect(hWnd, &rc);
        if (rc.right <= 0 || rc.bottom <= 0) { EndPaint(hWnd, &ps); return 0; }

        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        HGDIOBJ ob = SelectObject(mem, bmp);

        HBRUSH bg = CreateSolidBrush(COL_WINDOW_BG);
        FillRect(mem, &rc, bg);
        DeleteObject(bg);

        // 顶部主色强调条（设计语言：一条主色顶边）
        RECT bar = { 0, 0, rc.right, S(4) };
        HBRUSH ac = CreateSolidBrush(COL_ACCENT);
        FillRect(mem, &bar, ac);
        DeleteObject(ac);

        if (g_appIcon)
            DrawIconEx(mem, S(16), S(14), g_appIcon, S(30), S(30), 0, nullptr, DI_NORMAL);

        SetBkMode(mem, TRANSPARENT);
        SetTextColor(mem, COL_TEXT_DIM);
        HGDIOBJ of = SelectObject(mem, g_uiFont);
        RECT sub = { S(56), S(36), rc.right - S(16), S(60) };
        DrawTextW(mem, L"为游戏更换字体：选择启动程序(.exe)，自动识别 32/64 位并安装。",
            -1, &sub, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        SelectObject(mem, of);

        DrawCardAround(mem, g_hList);
        DrawCardAround(mem, g_hLog);

        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ob);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        HWND ctl = (HWND)lp;
        SetBkMode(hdc, TRANSPARENT);
        wchar_t cls[16] = {};
        GetClassNameW(ctl, cls, 16);
        if (lstrcmpiW(cls, L"Edit") == 0) {  // 只读 EDIT 也走 STATIC 分支
            SetTextColor(hdc, COL_TEXT);
            SetBkColor(hdc, COL_PANEL_BG);
            return (LRESULT)g_whiteBrush;
        }
        int id = GetDlgCtrlID(ctl);
        COLORREF c = COL_TEXT;
        if (id == IDC_STATIC_HINT) c = COL_TEXT_HINT;
        else if (id == IDC_STATIC_PATHLABEL || id == IDC_STATIC_ARCH ||
                 id == IDC_STATIC_LOGLABEL || id == IDC_STATIC_LISTLABEL) c = COL_TEXT_DIM;
        SetTextColor(hdc, c);
        SetBkColor(hdc, COL_WINDOW_BG);
        return (LRESULT)g_bgBrush;
    }

    case WM_CTLCOLOREDIT:
        SetTextColor((HDC)wp, COL_TEXT);
        SetBkColor((HDC)wp, COL_PANEL_BG);
        return (LRESULT)g_whiteBrush;

    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT* od = (const DRAWITEMSTRUCT*)lp;
        if (od && od->CtlType == ODT_BUTTON) {
            DrawOwnerButton(od);
            return TRUE;
        }
        break;
    }

    case WM_NOTIFY: {
        NMHDR* nh = (NMHDR*)lp;
        if (!nh) break;
        if (g_hList && nh->hwndFrom == ListView_GetHeader(g_hList) &&
            nh->code == NM_CUSTOMDRAW)
            return HandleHeaderCustomDraw((NMLVCUSTOMDRAW*)lp);
        if (g_hList && nh->hwndFrom == g_hList) {
            if (nh->code == NM_DBLCLK) { OnOpenSelectedDir(); return 0; }
            if (nh->code == NM_CUSTOMDRAW) return HandleListCustomDraw((NMLVCUSTOMDRAW*)lp);
            if (nh->code == LVN_GETINFOTIP) {
                NMLVGETINFOTIPW* it = (NMLVGETINFOTIPW*)lp;
                GameRecord g;
                if (GetGameRecordAt(it->iItem, &g))
                    lstrcpynW(it->pszText, g.dir.c_str(), it->cchTextMax);
                return 0;
            }
        }
        break;
    }

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
        case IDC_BTN_INSTALL:   DoInstall(); RefreshGameList(); return 0;
        case IDC_BTN_UNINSTALL: OnUninstallSelected(); return 0;
        case IDC_BTN_OPEN_DIR:  OnOpenSelectedDir(); return 0;
        case IDC_BTN_REMOVE:    OnRemoveSelectedRecord(); return 0;
        case IDC_BTN_REFRESH:   RefreshGameList(); Log(L"已刷新列表。"); return 0;
        case IDC_BTN_ADD_GAME:  OnAddGameManually(hWnd); return 0;
        case IDC_BTN_EXIT:      DestroyWindow(hWnd); return 0;
        default: return 0;
        }

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        if (g_uiFont) { DeleteObject(g_uiFont); g_uiFont = nullptr; }
        if (g_titleFont) { DeleteObject(g_titleFont); g_titleFont = nullptr; }
        if (g_bgBrush) { DeleteObject(g_bgBrush); g_bgBrush = nullptr; }
        if (g_whiteBrush) { DeleteObject(g_whiteBrush); g_whiteBrush = nullptr; }
        if (g_appIcon) { DestroyIcon(g_appIcon); g_appIcon = nullptr; }
        if (g_appIconSm) { DestroyIcon(g_appIconSm); g_appIconSm = nullptr; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    // 与系统 DPI 一致即可，避免高分屏下界面模糊；布局尺寸随 DPI 缩放。
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);
    HDC screenDc = GetDC(nullptr);
    if (screenDc) {
        int dpi = GetDeviceCaps(screenDc, LOGPIXELSX);
        if (dpi > 0) g_dpi = dpi;
        ReleaseDC(nullptr, screenDc);
    }

    // 应用图标：EXE 内嵌的多尺寸 ICON 资源，按系统推荐尺寸加载大小两种。
    g_appIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APPICON),
        IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON),
        LR_DEFAULTCOLOR);
    g_appIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APPICON),
        IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kWndClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = g_appIcon;
    wc.hIconSm = g_appIconSm;
    wc.hbrBackground = nullptr;  // 背景由 WM_PAINT 自绘
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
        // 走对话框式键盘导航：Tab 切换焦点、Enter 触发默认按钮。
        if (!IsDialogMessageW(hWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return 0;
}
