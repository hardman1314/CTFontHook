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

const wchar_t* kAppTitle = L"CTFontHook 安装器";
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
const int kClientW = 760;
const int kClientH = 626;

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

void LayoutControls(HWND hWnd) {
    RECT rc = {};
    GetClientRect(hWnd, &rc);
    const int logTop = 460;
    const int hintH = 34;
    const int bottomPad = 14;
    const int gap = 8;

    if (g_hList)
        MoveWindow(g_hList, S(16), S(146), rc.right - S(32), S(240), TRUE);

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
        L"第一步：选择游戏的启动程序(.exe)，工具会自动判断 32/64 位并登记到下方列表。",
        WS_CHILD | WS_VISIBLE, S(16), S(16), S(728), S(20), IDC_STATIC_TITLE);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    c = MakeControl(hWnd, L"STATIC", L"游戏程序：",
        WS_CHILD | WS_VISIBLE, S(16), S(48), S(80), S(20), IDC_STATIC_PATHLABEL);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    g_hPathEdit = MakeControl(hWnd, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
        S(100), S(46), S(548), S(26), IDC_EDIT_PATH);
    SendMessageW(g_hPathEdit, WM_SETFONT, (WPARAM)uiFont, TRUE);

    c = MakeControl(hWnd, L"BUTTON", L"浏览…",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        S(654), S(45), S(90), S(28), IDC_BTN_BROWSE);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    c = MakeControl(hWnd, L"BUTTON", L"安装",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        S(16), S(84), S(120), S(32), IDC_BTN_INSTALL);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    g_hArchLabel = MakeControl(hWnd, L"STATIC", L"检测结果：尚未选择",
        WS_CHILD | WS_VISIBLE, S(148), S(90), S(596), S(20), IDC_STATIC_ARCH);
    SendMessageW(g_hArchLabel, WM_SETFONT, (WPARAM)uiFont, TRUE);

    g_hListLabel = MakeControl(hWnd, L"STATIC", L"已登记的游戏：",
        WS_CHILD | WS_VISIBLE, S(16), S(124), S(728), S(18), IDC_STATIC_LISTLABEL);
    SendMessageW(g_hListLabel, WM_SETFONT, (WPARAM)uiFont, TRUE);

    g_hList = MakeControl(hWnd, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP |
        LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        S(16), S(146), S(728), S(240), IDC_LIST_GAMES);
    SendMessageW(g_hList, WM_SETFONT, (WPARAM)uiFont, TRUE);
    ListView_SetExtendedListViewStyle(g_hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
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

    c = MakeControl(hWnd, L"BUTTON", L"卸载 / 还原",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        S(16), S(396), S(110), S(32), IDC_BTN_UNINSTALL);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    c = MakeControl(hWnd, L"BUTTON", L"打开目录",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        S(132), S(396), S(100), S(32), IDC_BTN_OPEN_DIR);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    c = MakeControl(hWnd, L"BUTTON", L"移除记录",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        S(238), S(396), S(100), S(32), IDC_BTN_REMOVE);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    c = MakeControl(hWnd, L"BUTTON", L"手动登记游戏…",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        S(344), S(396), S(130), S(32), IDC_BTN_ADD_GAME);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    c = MakeControl(hWnd, L"BUTTON", L"刷新",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        S(480), S(396), S(80), S(32), IDC_BTN_REFRESH);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    c = MakeControl(hWnd, L"BUTTON", L"退出",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        S(654), S(396), S(90), S(32), IDC_BTN_EXIT);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    c = MakeControl(hWnd, L"STATIC", L"操作日志：",
        WS_CHILD | WS_VISIBLE, S(16), S(440), S(728), S(18), IDC_STATIC_LOGLABEL);
    SendMessageW(c, WM_SETFONT, (WPARAM)uiFont, TRUE);

    g_hLog = MakeControl(hWnd, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        S(16), S(460), S(728), S(110), IDC_EDIT_LOG);
    SendMessageW(g_hLog, WM_SETFONT, (WPARAM)uiFont, TRUE);

    g_hHint = MakeControl(hWnd, L"STATIC",
        L"提示：安装会备份游戏目录原有的 winmm.dll；请先关闭游戏。\n"
        L"列表会记住每个游戏当前应用的字体；联网 / 反作弊游戏请谨慎使用。",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        S(16), S(578), S(728), S(34), IDC_STATIC_HINT);
    SendMessageW(g_hHint, WM_SETFONT, (WPARAM)uiFont, TRUE);

    LayoutControls(hWnd);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        CreateControls(hWnd);
        Log(L"CTFontHook 安装器已就绪。");
        Log(L"请选择游戏的启动程序(.exe)并点击「安装」；已安装的游戏会显示在下方列表。");
        RefreshGameList();
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
