#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdlib.h>
#include <strsafe.h>
#include <tlhelp32.h>

#include <string>

#define IDR_EMPF_DLL 101
#define IDR_EMPF_EXTRACT 102

namespace {

constexpr wchar_t kAppName[] = L"eufyMake EMPF Thumbnails";
constexpr wchar_t kVersion[] = L"1.0.0";
constexpr wchar_t kPublisher[] = L"Osterman Designs";
constexpr wchar_t kUninstallKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EmpfThumbs";
constexpr wchar_t kHelpUrl[] =
    L"https://github.com/Osterman-Designs/eufymake-studio-windows-thumbnails";

bool HasFlag(int argc, wchar_t** argv, const wchar_t* flag) {
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], flag) == 0) {
            return true;
        }
    }
    return false;
}

void Notify(bool silent, const wchar_t* text, UINT icon) {
    if (!silent) {
        MessageBoxW(nullptr, text, kAppName, MB_OK | icon);
    }
}

std::wstring InstallDir() {
    wchar_t base[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, base))) {
        return {};
    }
    wchar_t path[MAX_PATH] = {};
    StringCchPrintfW(path, MAX_PATH, L"%s\\EmpfThumbs", base);
    return path;
}

bool FileExists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool WriteResource(int resourceId, const std::wstring& path) {
    HRSRC found = FindResourceW(nullptr, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!found) {
        return false;
    }
    HGLOBAL loaded = LoadResource(nullptr, found);
    const DWORD size = SizeofResource(nullptr, found);
    const void* data = LockResource(loaded);
    if (!data || size == 0) {
        return false;
    }

    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(file, data, size, &written, nullptr);
    CloseHandle(file);
    return ok && written == size;
}

bool CopySelf(const std::wstring& dest) {
    wchar_t self[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, self, MAX_PATH)) {
        return false;
    }
    return CopyFileW(self, dest.c_str(), FALSE) != 0;
}

DWORD FileSizeKb(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA info = {};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info)) {
        return 0;
    }
    ULARGE_INTEGER size = {};
    size.HighPart = info.nFileSizeHigh;
    size.LowPart = info.nFileSizeLow;
    return static_cast<DWORD>((size.QuadPart + 1023) / 1024);
}

using DllRegisterFn = HRESULT(STDAPICALLTYPE*)();

bool CallDllExport(const std::wstring& dllPath, const char* name) {
    HMODULE module = LoadLibraryW(dllPath.c_str());
    if (!module) {
        return false;
    }
    auto fn = reinterpret_cast<DllRegisterFn>(GetProcAddress(module, name));
    const bool ok = fn && SUCCEEDED(fn());
    FreeLibrary(module);
    return ok;
}

void KillByName(const wchar_t* exeName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return;
    }
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, exeName) == 0) {
                HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                if (process) {
                    TerminateProcess(process, 0);
                    CloseHandle(process);
                }
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
}

void ReleaseExplorerHosts() {
    KillByName(L"dllhost.exe");
    KillByName(L"prevhost.exe");
    Sleep(400);
}

bool DeleteFileRetry(const std::wstring& path) {
    for (int i = 0; i < 8; ++i) {
        if (DeleteFileW(path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) {
            return true;
        }
        Sleep(150);
    }
    MoveFileExW(path.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    return false;
}

void WriteUninstallKey(const std::wstring& dir, const std::wstring& setupPath) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kUninstallKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key,
                        nullptr) != ERROR_SUCCESS) {
        return;
    }
    auto setSz = [&](const wchar_t* name, const wchar_t* value) {
        RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value),
                       static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));
    };
    setSz(L"DisplayName", kAppName);
    setSz(L"DisplayVersion", kVersion);
    setSz(L"Publisher", kPublisher);
    setSz(L"InstallLocation", dir.c_str());
    setSz(L"DisplayIcon", setupPath.c_str());
    setSz(L"URLInfoAbout", kHelpUrl);
    wchar_t uninstall[MAX_PATH + 32] = {};
    StringCchPrintfW(uninstall, _countof(uninstall), L"\"%s\" /uninstall", setupPath.c_str());
    setSz(L"UninstallString", uninstall);
    wchar_t quiet[MAX_PATH + 40] = {};
    StringCchPrintfW(quiet, _countof(quiet), L"\"%s\" /uninstall /S", setupPath.c_str());
    setSz(L"QuietUninstallString", quiet);
    const DWORD one = 1;
    RegSetValueExW(key, L"NoModify", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&one), sizeof(one));
    RegSetValueExW(key, L"NoRepair", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&one), sizeof(one));
    const DWORD sizeKb = FileSizeKb(dir + L"\\EmpfThumbs.dll") + FileSizeKb(dir + L"\\empf-extract.exe") +
                         FileSizeKb(setupPath);
    RegSetValueExW(key, L"EstimatedSize", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&sizeKb),
                   sizeof(sizeKb));
    RegCloseKey(key);
}

void DeleteUninstallKey() {
    RegDeleteTreeW(HKEY_CURRENT_USER, kUninstallKey);
}

bool RunningFromDir(const std::wstring& dir) {
    wchar_t self[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, self, MAX_PATH)) {
        return false;
    }
    return _wcsnicmp(self, dir.c_str(), dir.size()) == 0;
}

int RelaunchUninstallFromTemp(bool silent) {
    wchar_t tempDir[MAX_PATH] = {};
    if (!GetTempPathW(MAX_PATH, tempDir)) {
        return 1;
    }
    wchar_t dest[MAX_PATH] = {};
    StringCchPrintfW(dest, MAX_PATH, L"%sEmpfThumbs-uninstall.exe", tempDir);
    if (!CopySelf(dest)) {
        return 1;
    }

    wchar_t cmd[MAX_PATH + 64] = {};
    StringCchPrintfW(cmd, _countof(cmd), L"\"%s\" /uninstall /from-temp%s", dest,
                     silent ? L" /S" : L"");
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        return 1;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}

int Install(bool silent) {
    const std::wstring dir = InstallDir();
    if (dir.empty()) {
        Notify(silent, L"Could not find the per-user AppData folder.", MB_ICONERROR);
        return 1;
    }
    CreateDirectoryW(dir.c_str(), nullptr);

    const std::wstring dll = dir + L"\\EmpfThumbs.dll";
    const std::wstring extract = dir + L"\\empf-extract.exe";
    const std::wstring setup = dir + L"\\EmpfThumbsSetup.exe";

    if (FileExists(dll)) {
        CallDllExport(dll, "DllUnregisterServer");
        ReleaseExplorerHosts();
    }

    if (!WriteResource(IDR_EMPF_DLL, dll) || !WriteResource(IDR_EMPF_EXTRACT, extract) || !CopySelf(setup)) {
        Notify(silent, L"Failed to copy program files.", MB_ICONERROR);
        return 1;
    }
    if (!CallDllExport(dll, "DllRegisterServer")) {
        Notify(silent, L"Copied files, but Explorer registration failed.", MB_ICONERROR);
        return 1;
    }

    WriteUninstallKey(dir, setup);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    Notify(silent,
           L"Installed. Open a folder of .empf files in large or extra-large icons.\n"
           L"Uninstall from Settings → Apps, or run EmpfThumbsSetup.exe /uninstall.",
           MB_ICONINFORMATION);
    return 0;
}

int Uninstall(bool silent, bool fromTemp) {
    const std::wstring dir = InstallDir();
    if (!fromTemp && RunningFromDir(dir)) {
        return RelaunchUninstallFromTemp(silent);
    }

    const std::wstring dll = dir + L"\\EmpfThumbs.dll";
    if (FileExists(dll)) {
        CallDllExport(dll, "DllUnregisterServer");
    }
    ReleaseExplorerHosts();

    DeleteFileRetry(dll);
    DeleteFileRetry(dir + L"\\empf-extract.exe");
    DeleteFileRetry(dir + L"\\EmpfThumbsSetup.exe");
    DeleteUninstallKey();
    RemoveDirectoryW(dir.c_str());
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    if (fromTemp) {
        wchar_t self[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, self, MAX_PATH)) {
            MoveFileExW(self, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        }
    }

    Notify(silent, L"Uninstalled. Reopen Explorer folders if old thumbnails remain.", MB_ICONINFORMATION);
    return 0;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const bool silent = HasFlag(argc, argv, L"/S") || HasFlag(argc, argv, L"/silent");
    const bool uninstall = HasFlag(argc, argv, L"/uninstall") || HasFlag(argc, argv, L"/u");
    const bool fromTemp = HasFlag(argc, argv, L"/from-temp");
    const int rc = uninstall ? Uninstall(silent, fromTemp) : Install(silent);
    if (argv) {
        LocalFree(argv);
    }
    return rc;
}
